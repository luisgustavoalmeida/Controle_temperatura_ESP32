/**
 * =============================================================================
 * Controle_temperatura_ESP32.ino — Programa principal
 * =============================================================================
 *
 * O que este firmware faz:
 *   1. Lê a temperatura da água (sensor DS18B20).
 *   2. O usuário escolhe a temperatura desejada no encoder rotativo.
 *   3. PID OUT 0..1 → passos TPL0501 via SPI; atuador a cada ciclo PID.
 *   4. A malha nao pausa na meta: regulacao continua (histerese so no buzzer/backlight).
 *   5. Duplo clique = standby (pot. 0); clique = religar; longo = reiniciar PID (modo temp).
 *   6. Segurar encoder 3 s sem girar = alternar PID ↔ potência manual.
 *
 * Ganhos PID: ver PID_GANHO_* em config.h
 *
 * O loop() NÃO usa delay() — cada tarefa roda em seu próprio intervalo (ms).
 *
 * ATENÇÃO: controle na linha de potência exige projeto elétrico seguro (DR,
 * terra, isolamento). Este código não substitui proteções de segurança.
 * =============================================================================
 */

#include "config.h"
#include "pid_controller.h"
#include "potenciometro_map.h"
#include "atuador_potenciometro.h"
#include "sensor_ds18b20.h"
#include "display_lcd.h"
#include "encoder_rotativo.h"
#include "buzzer.h"
#include "medidor_uso.h"

// =============================================================================
// Módulos de hardware (um objeto por periférico)
// =============================================================================
ControladorPID pid;           // Malha de controle
AtuadorPotenciometro potenciometro;  // Atuador (1× ou 2× TPL0501)
SensorDS18B20 sensorTemp;     // Entrada (temperatura)
DisplayLCD display;           // Saída visual
EncoderRotativo encoder;      // Entrada do usuário (setpoint)
Buzzer buzzer;                // Saída sonora
MedidorUso medidorUso;        // Cronômetro + energia ∫P dt

// =============================================================================
// Variáveis da malha de controle
// =============================================================================
float temperaturaBruta = NAN;      // Última leitura direta do DS18B20 [°C]
float temperaturaFiltrada = NAN;   // Média móvel (menos ruído) [°C]
float saidaPid = 0.0f;             // PID OUT 0..1 → potência (1=máx); passo via escala ideal 150 kΩ
float setpointEncoder = ALVO_TEMP_PADRAO_C;
float setpointMalha = ALVO_TEMP_PADRAO_C;
float alvoPotenciaEncoder = ALVO_POT_PADRAO_PCT;
float saidaPotenciaManual = 0.0f;
bool setpointPendenteNaMalha = false;
unsigned long momentoUltimoGiroEncoder = 0;
unsigned long momentoUltimaInteracaoEncoder = 0;

ModoControle modoControle = MODO_CONTROLE_PID;

bool metaAtingida = false;
bool estavaNaMeta = false;

bool controleMalhaAtivo = MALHA_INICIA_ATIVA;
MensagemTransicao msgTransicao = MSG_NENHUMA;
unsigned long msgTransicaoAteMs = 0;

// Filtro de temperatura — tamanho em FILTRO_TEMP_AMOSTRAS (config.h)
float historicoTemp[FILTRO_TEMP_AMOSTRAS];
uint8_t indiceHistorico = 0;

// =============================================================================
// Controle de tempo (cada tarefa só roda quando o intervalo expira)
// =============================================================================
unsigned long momentoUltimoLoopRapido = 0;
unsigned long momentoUltimoSensor = 0;
unsigned long momentoUltimoPid = 0;
unsigned long momentoUltimoLcd = 0;

/** Histerese do atuador: último OUT aplicado no TPL0501 (−1 = nunca aplicou). */
float saidaPidUltimoAplicado = -1.0f;
unsigned long momentoUltimoAjustePot = 0;

// (Conversão DS18B20: estado interno em sensor_ds18b20.cpp — não bloqueia o loop)

// -----------------------------------------------------------------------------
// Filtro de temperatura
// -----------------------------------------------------------------------------

/**
 * Guarda a nova leitura no histórico e calcula a média das amostras válidas.
 * Isso reduz oscilações no display e na entrada do PID.
 */
void aplicarFiltroTemperatura(float novaLeituraC) {
  if (isnan(novaLeituraC)) {
    temperaturaFiltrada = NAN;
    return;
  }

  historicoTemp[indiceHistorico] = novaLeituraC;
  indiceHistorico = (indiceHistorico + 1) % FILTRO_TEMP_AMOSTRAS;

  float soma = 0.0f;
  int quantidadeValida = 0;
  for (int i = 0; i < FILTRO_TEMP_AMOSTRAS; i++) {
    if (!isnan(historicoTemp[i])) {
      soma += historicoTemp[i];
      quantidadeValida++;
    }
  }
  temperaturaFiltrada = (quantidadeValida > 0)
                            ? (soma / (float)quantidadeValida)
                            : novaLeituraC;
}

// -----------------------------------------------------------------------------
// Segurança e estado do sistema
// -----------------------------------------------------------------------------

/** Alvo usado no buzzer de meta: encoder enquanto ajusta, malha quando estavel. */
float alvoMetaBuzzer() {
  return setpointPendenteNaMalha ? setpointEncoder : setpointMalha;
}

/** true quando a temperatura filtrada está perto o suficiente do alvo */
bool temperaturaAtingiuMeta() {
  if (isnan(temperaturaFiltrada)) {
    return false;
  }
  float diferenca = fabsf(temperaturaFiltrada - alvoMetaBuzzer());
  return (diferenca <= BUZZER_HISTERESE_C);
}

/**
 * Toca buzzer apenas na transicao (evita repetir enquanto permanece na mesma faixa).
 *   Entrou na meta  -> 3 tons ascendentes
 *   Saiu da meta   -> 3 tons descendentes
 */
void avisarMudancaDeMeta(bool naMetaAgora) {
  if (naMetaAgora && !estavaNaMeta) {
    buzzer.tocarMetaAtingida();
    display.piscarMetaAtingida();
    Serial.println(F("[BUZZER] Entrou na meta"));
  } else if (!naMetaAgora && estavaNaMeta) {
    buzzer.tocarForaDaMeta();
    display.piscarForaDaMeta();
    Serial.println(F("[BUZZER] Saiu da meta"));
  }
  estavaNaMeta = naMetaAgora;
}

/**
 * Modo seguro: sensor falhou ou leitura inválida.
 * Coloca potência mínima no chuveiro e zera o estado interno do PID.
 */
/** Registra instante e OUT do último comando enviado ao TPL0501. */
void registrarAplicacaoPot(float outAplicado) {
  saidaPidUltimoAplicado = outAplicado;
  momentoUltimoAjustePot = millis();
}

/** OUT (0..1) → TPL0501: salto SPI direto. */
void aplicarSaidaPidNoPotenciometro() {
  potenciometro.definirSaidaNormalizadaRapida(saidaPid);
  registrarAplicacaoPot(saidaPid);
}

void aplicarSaidaPotenciaManualNoPotenciometro() {
  potenciometro.definirSaidaNormalizadaRapida(saidaPotenciaManual);
  registrarAplicacaoPot(saidaPotenciaManual);
}

/** Standby: potencia 0 % sem alterar saidaPid (memoria da malha). */
void aplicarPotenciometroStandby() {
  potenciometro.definirSaidaNormalizadaRapida(PID_SAIDA_MIN);
  registrarAplicacaoPot(PID_SAIDA_MIN);
}

/** true se passou PERIODO_ATUADOR_POT_MS e o OUT do PID mudou. */
bool deveAplicarSaidaPidNoPotenciometro(unsigned long instanteAtualMs) {
  if (saidaPidUltimoAplicado < -0.5f) {
    return true;
  }
  if ((instanteAtualMs - momentoUltimoAjustePot) < PERIODO_ATUADOR_POT_MS) {
    return false;
  }
#if POT_HISTERESIS_SAIDA_ATIVA
  return (fabsf(saidaPid - saidaPidUltimoAplicado) > POT_HISTERESIS_SAIDA_LIMIAR);
#else
  return (fabsf(saidaPid - saidaPidUltimoAplicado) > 0.0001f);
#endif
}

/** true se a temperatura esta bem abaixo do alvo (religar deve reiniciar PID). */
bool temperaturaMuitoAbaixoDoAlvo() {
  if (isnan(temperaturaFiltrada)) {
    return false;
  }
  float deficit = setpointMalha - temperaturaFiltrada;
  return (deficit > STANDBY_RELIGA_REINICIA_DELTA_C);
}

void entrarModoSeguroPorFalhaSensor() {
  saidaPid = PID_SAIDA_MIN;
  saidaPotenciaManual = PID_SAIDA_MIN;
  alvoPotenciaEncoder = ALVO_POT_MIN_PCT;
  encoder.definirAlvoPotenciaPercent(alvoPotenciaEncoder);
  aplicarSaidaPidNoPotenciometro();
  pid.reiniciar();
}

/** Potência normalizada 0..1 em uso (PID ou manual). */
float potenciaComandoAtual() {
  if (!controleMalhaAtivo) {
    return 0.0f;
  }
  return (modoControle == MODO_CONTROLE_POTENCIA) ? saidaPotenciaManual : saidaPid;
}

void desligarMalhaControle(bool porInatividade = false);

void registrarInteracaoEncoder(bool atualizarBacklight = true) {
  momentoUltimaInteracaoEncoder = millis();
  if (atualizarBacklight) {
    display.notificarAtividadeEncoder();
  }
}

void verificarDesligamentoAutomatico() {
  if (!controleMalhaAtivo) {
    return;
  }
  if ((millis() - momentoUltimaInteracaoEncoder) < AUTO_DESLIGA_INATIVIDADE_MS) {
    return;
  }
  buzzer.tocarConfirmacao();
  desligarMalhaControle(true);
}

/** Standby: pot. 0 %; preserva saidaPid e estado do PID. */
void desligarMalhaControle(bool porInatividade) {
  unsigned long agora = millis();
  medidorUso.atualizar(potenciaComandoAtual(), true, agora);
  medidorUso.atualizar(0.0f, false, agora);

  controleMalhaAtivo = false;
  aplicarPotenciometroStandby();
  estavaNaMeta = false;
  metaAtingida = false;
  if (porInatividade) {
    msgTransicao = MSG_DESLIGA_INATIVIDADE;
    Serial.println(F("[CTRL] Desligamento automatico — 40 min sem interacao"));
  } else {
    msgTransicao = MSG_DESATIVANDO_MALHA;
    Serial.print(F("[CTRL] Standby — pot. 0 | OUT mem="));
    Serial.println(saidaPid, 3);
  }
  msgTransicaoAteMs = millis() + LCD_MSG_TRANSICAO_MS;
  display.invalidarCache();
  registrarInteracaoEncoder(false);
}

/** Reativa controle; no modo PID pode reiniciar integral. */
void ligarMalhaControle(bool reiniciarPid) {
  medidorUso.reiniciar(millis());
  controleMalhaAtivo = true;
  if (modoControle == MODO_CONTROLE_PID && reiniciarPid) {
    pid.reiniciar();
    saidaPid = PID_SAIDA_MIN;
    aplicarSaidaPidNoPotenciometro();
  } else if (modoControle == MODO_CONTROLE_POTENCIA) {
    saidaPotenciaManual = alvoPotenciaEncoder / 100.0f;
    aplicarSaidaPotenciaManualNoPotenciometro();
  } else {
    aplicarSaidaPidNoPotenciometro();
  }
  estavaNaMeta = false;
  metaAtingida = false;
  msgTransicao = MSG_ATIVANDO_MALHA;
  msgTransicaoAteMs = millis() + LCD_MSG_TRANSICAO_MS;
  display.invalidarCache();
  registrarInteracaoEncoder(false);
  Serial.print(F("[CTRL] Malha ON | modo="));
  Serial.print(nomeModoControle());
  Serial.print(F(" reinicia="));
  Serial.print(reiniciarPid ? '1' : '0');
  Serial.print(F(" OUT="));
  Serial.println((modoControle == MODO_CONTROLE_POTENCIA) ? saidaPotenciaManual
                                                          : saidaPid,
                 3);
}

void reiniciarMalhaPid() {
  if (modoControle != MODO_CONTROLE_PID) {
    return;
  }
  pid.reiniciar();
  estavaNaMeta = false;
  metaAtingida = false;
  if (controleMalhaAtivo) {
    saidaPid = PID_SAIDA_MIN;
    aplicarSaidaPidNoPotenciometro();
  }
  Serial.println(F("[CTRL] PID reiniciado (clique longo)"));
}

const char* nomeModoControle() {
  return (modoControle == MODO_CONTROLE_POTENCIA) ? "POT" : "PID";
}

void alternarModoControle() {
  ModoControle novoModo = (modoControle == MODO_CONTROLE_PID)
                              ? MODO_CONTROLE_POTENCIA
                              : MODO_CONTROLE_PID;

  if (controleMalhaAtivo) {
    if (novoModo == MODO_CONTROLE_POTENCIA) {
      alvoPotenciaEncoder = saidaPid * 100.0f;
      encoder.definirAlvoPotenciaPercent(alvoPotenciaEncoder);
      saidaPotenciaManual = saidaPid;
    } else {
      setpointEncoder = setpointMalha;
      encoder.definirSetpoint(setpointEncoder);
      setpointPendenteNaMalha = false;
      if (controleMalhaAtivo) {
        pid.reiniciar();
        saidaPid = PID_SAIDA_MIN;
      }
    }
  }

  modoControle = novoModo;
  encoder.definirModoAjuste(
      (modoControle == MODO_CONTROLE_POTENCIA) ? ENCODER_AJUSTE_POTENCIA
                                               : ENCODER_AJUSTE_TEMPERATURA);

  estavaNaMeta = false;
  metaAtingida = false;
  msgTransicao = MSG_TROCANDO_MODO;
  msgTransicaoAteMs = millis() + LCD_MSG_TRANSICAO_MS;
  display.invalidarCache();

  Serial.print(F("[CTRL] Modo "));
  Serial.println(nomeModoControle());

  if (controleMalhaAtivo) {
    if (modoControle == MODO_CONTROLE_POTENCIA) {
      aplicarSaidaPotenciaManualNoPotenciometro();
    } else {
      aplicarSaidaPidNoPotenciometro();
    }
  }
}

void atualizarMensagemTransicao() {
  if (msgTransicao != MSG_NENHUMA && millis() >= msgTransicaoAteMs) {
    msgTransicao = MSG_NENHUMA;
    display.invalidarCache();
  }
}

#if SERIAL_DEPURAR_MALHA
/** Uma linha por passo do controle (~PERIODO_PID_MS). */
void imprimirSerialMalha() {
  Serial.print(F("[MALHA]"));
  Serial.print(F(" MOD="));
  Serial.print(nomeModoControle());
  Serial.print(F(" ACT="));
  Serial.print(controleMalhaAtivo ? 1 : 0);
  if (modoControle == MODO_CONTROLE_POTENCIA) {
    Serial.print(F(" ALVO_POT="));
    Serial.print(alvoPotenciaEncoder, 1);
    Serial.print(F(" OUT="));
    Serial.print(saidaPotenciaManual, 4);
  } else {
    Serial.print(F(" SP="));
    Serial.print(setpointMalha, 2);
    Serial.print(F(" ERR="));
    Serial.print(pid.ultimoErro(), 3);
    Serial.print(F(" P="));
    Serial.print(pid.ultimoTermoP(), 4);
    Serial.print(F(" I="));
    Serial.print(pid.ultimoTermoI(), 4);
    Serial.print(F(" D="));
    Serial.print(pid.ultimoTermoD(), 4);
    Serial.print(F(" OUT="));
    Serial.print(saidaPid, 4);
  }
  Serial.print(F(" PV="));
  if (isnan(temperaturaFiltrada)) {
    Serial.print(F("---"));
  } else {
    Serial.print(temperaturaFiltrada, 2);
  }
  Serial.print(F(" PCT_CMD="));
  Serial.print(potenciometro.potenciaAlvoPercentual(), 3);
  Serial.print(F(" PCT="));
  Serial.print(potenciometro.potenciaAtualPercentual(), 3);
  float outRef = (modoControle == MODO_CONTROLE_POTENCIA) ? saidaPotenciaManual
                                                          : saidaPid;
  if (fabsf(potenciometro.potenciaAtualPercentual() / 100.0f - outRef)
      > SERIAL_TOLERANCIA_ERRO_POT) {
    Serial.print(F(" dPOT="));
    Serial.print(potenciometro.potenciaAtualPercentual() - outRef * 100.0f, 2);
  }
  Serial.print(F(" POT="));
  Serial.print(potenciometro.passoAtualA());
#if POT_USA_DOIS_CHIPS
  Serial.print(F("/"));
  Serial.print(potenciometro.passoAtualB());
#endif
  Serial.print(F(" META="));
  Serial.println(metaAtingida ? 1 : 0);
}
#endif

/** Copia setpointEncoder -> setpointMalha apos pausa sem giro no encoder. */
void atualizarSetpointMalhaSeEstavel() {
  if (!setpointPendenteNaMalha) {
    return;
  }
  if (millis() - momentoUltimoGiroEncoder < ALVO_TEMP_PAUSA_MS) {
    return;
  }
  if (fabsf(setpointMalha - setpointEncoder) < 0.001f) {
    setpointPendenteNaMalha = false;
    return;
  }
  setpointMalha = setpointEncoder;
  setpointPendenteNaMalha = false;
  estavaNaMeta = temperaturaAtingiuMeta();
  display.invalidarCache();
  Serial.print(F("[SP] Alvo aplicado na malha: "));
  Serial.println(setpointMalha, 2);
}

EstadoSistema obterEstadoParaDisplay() {
  if (!sensorTemp.sensorOk()) {
    return ESTADO_SENSOR_ERRO;
  }
  if (!sensorTemp.jaObteveLeituraValida()) {
    return ESTADO_AGUARDE_SENSOR;
  }
  if (!controleMalhaAtivo) {
    return ESTADO_CONTROLE_DESLIGADO;
  }
  if (modoControle == MODO_CONTROLE_POTENCIA) {
    return ESTADO_POTENCIA_ATIVO;
  }
  return ESTADO_PID_ATIVO;
}

// -----------------------------------------------------------------------------
// Tarefas periódicas (chamadas pelo loop)
// -----------------------------------------------------------------------------

/**
 * Tarefa rápida (~5 ms): encoder + buzzer.
 * O usuário sente resposta imediata ao girar ou pressionar o botão.
 */
void tarefaInterfaceUsuario() {
  buzzer.atualizar();
  encoder.atualizar();

  if (controleMalhaAtivo && digitalRead(PINO_ENCODER_BOTAO) == LOW) {
    registrarInteracaoEncoder();
  }

  if (encoder.consumirEventoTrocaModo()) {
    buzzer.tocarConfirmacao();
    registrarInteracaoEncoder();
    alternarModoControle();
  }

  float spEncoder = encoder.setpointC();
  float potEncoder = encoder.alvoPotenciaPercent();
  bool girou = false;
  while (encoder.consumirEventoRotacao()) {
    buzzer.tocarClique();
    girou = true;
  }

  if (modoControle == MODO_CONTROLE_POTENCIA) {
    if (girou || fabsf(potEncoder - alvoPotenciaEncoder) > 0.001f) {
      alvoPotenciaEncoder = potEncoder;
      saidaPotenciaManual = alvoPotenciaEncoder / 100.0f;
      momentoUltimoGiroEncoder = millis();
      if (girou) {
        registrarInteracaoEncoder();
      }
      if (controleMalhaAtivo) {
        aplicarSaidaPotenciaManualNoPotenciometro();
        display.invalidarCache();
      }
    }
  } else if (girou || fabsf(spEncoder - setpointEncoder) > 0.001f) {
    setpointEncoder = spEncoder;
    momentoUltimoGiroEncoder = millis();
    setpointPendenteNaMalha = true;
    if (girou) {
      registrarInteracaoEncoder();
    }
  }

  if (modoControle == MODO_CONTROLE_PID) {
    atualizarSetpointMalhaSeEstavel();
  }
  atualizarMensagemTransicao();

  if (encoder.consumirEventoDuploClique()) {
    buzzer.tocarConfirmacao();
    registrarInteracaoEncoder();
    if (controleMalhaAtivo) {
      desligarMalhaControle();
    }
  }

  if (encoder.consumirEventoCliqueLongo()) {
    buzzer.tocarConfirmacao();
    registrarInteracaoEncoder();
    reiniciarMalhaPid();
  }

  if (encoder.consumirEventoClique()) {
    registrarInteracaoEncoder();
    if (!controleMalhaAtivo) {
      buzzer.tocarConfirmacao();
      if (modoControle == MODO_CONTROLE_PID) {
        setpointMalha = setpointEncoder;
        setpointPendenteNaMalha = false;
        ligarMalhaControle(temperaturaMuitoAbaixoDoAlvo());
      } else {
        saidaPotenciaManual = alvoPotenciaEncoder / 100.0f;
        ligarMalhaControle(false);
        aplicarSaidaPotenciaManualNoPotenciometro();
      }
    }
  }

  verificarDesligamentoAutomatico();

  buzzer.atualizar();
  display.atualizarIluminacao(controleMalhaAtivo, momentoUltimaInteracaoEncoder);
}

/**
 * Tarefa do sensor (~800 ms): tenta ler só se a conversão assíncrona terminou.
 *
 * Entre uma chamada e outra o loop roda PID, LCD e encoder — sem delay(750).
 * Fluxo: requestTemperatures (instantâneo) → outras tarefas → getTempC quando pronto.
 */
void tarefaLeituraSensor() {
  float novaLeitura = NAN;

  if (!sensorTemp.atualizar(&novaLeitura)) {
    return;  // conversão ainda em andamento — sair sem bloquear
  }

  temperaturaBruta = novaLeitura;

  if (isnan(temperaturaBruta)) {
    Serial.println(F("[SENSOR] Falha DS18B20 — modo seguro"));
    entrarModoSeguroPorFalhaSensor();
  } else {
    aplicarFiltroTemperatura(temperaturaBruta);
  }
}

/**
 * Tarefa do controle (~PERIODO_PID_MS): PID ou potência manual conforme o modo.
 */
void tarefaMalhaPid(unsigned long instanteAtualMs) {
  if (!controleMalhaAtivo) {
    if (fabsf(saidaPidUltimoAplicado - PID_SAIDA_MIN) > 0.0001f
        || potenciometro.passoAtualA() != POT_PASSOS_MAX
#if POT_USA_DOIS_CHIPS
        || potenciometro.passoAtualB() != POT_PASSOS_MAX
#endif
    ) {
      aplicarPotenciometroStandby();
    }
#if SERIAL_DEPURAR_MALHA
    imprimirSerialMalha();
#endif
    return;
  }

  if (!sensorTemp.sensorOk() || isnan(temperaturaFiltrada)) {
    entrarModoSeguroPorFalhaSensor();
#if SERIAL_DEPURAR_MALHA
    imprimirSerialMalha();
#endif
    return;
  }

  if (modoControle == MODO_CONTROLE_POTENCIA) {
    saidaPotenciaManual = alvoPotenciaEncoder / 100.0f;
    if (deveAplicarSaidaPidNoPotenciometro(instanteAtualMs)) {
      aplicarSaidaPotenciaManualNoPotenciometro();
      display.invalidarCache();
    }
#if SERIAL_DEPURAR_MALHA
    imprimirSerialMalha();
#endif
    return;
  }

  float tempoSegundos = instanteAtualMs / 1000.0f;
  atualizarSetpointMalhaSeEstavel();
  saidaPid = pid.passo(setpointMalha, temperaturaFiltrada, tempoSegundos);
  if (deveAplicarSaidaPidNoPotenciometro(instanteAtualMs)) {
    aplicarSaidaPidNoPotenciometro();
    display.invalidarCache();
  }

  metaAtingida = temperaturaAtingiuMeta();
  avisarMudancaDeMeta(metaAtingida);

#if SERIAL_DEPURAR_MALHA
  imprimirSerialMalha();
#endif
}

/** Tarefa do LCD (~100 ms): linha 0 = uso/energia; linha 3 = potência. */
void tarefaAtualizarDisplay() {
  unsigned long agora = millis();
  float potCmd = potenciaComandoAtual();
  medidorUso.atualizar(potCmd, controleMalhaAtivo, agora);

  display.atualizar(setpointEncoder, alvoPotenciaEncoder, temperaturaFiltrada,
                    potCmd, potenciometro.passoAtualA(), potenciometro.passoAtualB(),
                    obterEstadoParaDisplay(), modoControle, metaAtingida,
                    controleMalhaAtivo, msgTransicao, setpointPendenteNaMalha,
                    medidorUso.tempoSegundos(), medidorUso.energiaWh());
}

// -----------------------------------------------------------------------------
// setup() e loop()
// -----------------------------------------------------------------------------

void setup() {
  Serial.begin(SERIAL_VELOCIDADE);
  delay(500);
  Serial.println(F("=== Inicio: controle chuveiro ESP32 ==="));
  Serial.print(F("[MAP] Modo "));
  Serial.print(potenciometroModoRedeNome());
  if (reqParaleloEstaAtivo()) {
    Serial.print(F(" | R_par "));
    Serial.print(RESISTOR_PARALELO_KOHM, 0);
    Serial.print(F(" kΩ"));
  }
  Serial.print(F(" | R_serie max "));
  Serial.print(rpotSerieMaximaKohm(), 1);
  Serial.print(F(" kΩ | ideal "));
  Serial.print(REQ_IDEAL_POTENCIA_MIN_KOHM, 0);
  Serial.print(F(" kΩ (0% ref) | min alcanç. ~ "));
  Serial.print(potenciaMinimaAlcancavelPercentual(), 1);
  Serial.println(F(" %"));
#if POT_USA_DOIS_CHIPS
  Serial.println(F("[MAP] 2 chips: SPI compartilhado, CS_A/CS_B separados"));
#endif
  Serial.println(F("[MAP] Escala potencia: Req linear"));
#if SERIAL_DEPURAR_MALHA
  Serial.print(F("[MALHA] PID "));
  Serial.print(PERIODO_PID_MS);
  Serial.print(F("ms | pot "));
  Serial.print(PERIODO_ATUADOR_POT_MS);
  Serial.print(F("ms dOUT>"));
#if POT_HISTERESIS_SAIDA_ATIVA
  Serial.print(POT_HISTERESIS_SAIDA_LIMIAR, 3);
#else
  Serial.print(F("passo"));
#endif
  Serial.println(F(" | escada A/B intercalada"));
#endif

  // Inicialização na ordem: feedback visual → entradas → atuador → sensor → PID
  display.iniciar();
  display.splashInicializacao();

  buzzer.iniciar();
  encoder.iniciar();
  encoder.definirModoAjuste(ENCODER_AJUSTE_TEMPERATURA);
  setpointEncoder = encoder.setpointC();
  alvoPotenciaEncoder = encoder.alvoPotenciaPercent();
  setpointMalha = setpointEncoder;
  saidaPotenciaManual = alvoPotenciaEncoder / 100.0f;
  momentoUltimoGiroEncoder = millis();
  momentoUltimaInteracaoEncoder = millis();
  setpointPendenteNaMalha = false;

  potenciometro.iniciar();
  medidorUso.iniciar();

  if (!MALHA_INICIA_ATIVA) {
    controleMalhaAtivo = false;
    Serial.println(F("[POT] Calculando posicao standby..."));
    potenciometro.definirSaidaNormalizadaRapida(PID_SAIDA_MIN);
    registrarAplicacaoPot(PID_SAIDA_MIN);
    Serial.println(F("[POT] Standby — potencia 0 % (inicializacao rapida)"));
  } else {
    potenciometro.reiniciarParaMinimo();
    medidorUso.reiniciar(millis());
    Serial.println(F("[POT] Wipers no minimo (potencia maxima)"));
  }

  for (int i = 0; i < FILTRO_TEMP_AMOSTRAS; i++) {
    historicoTemp[i] = NAN;
  }

  sensorTemp.iniciar();  // já dispara a 1ª conversão assíncrona (sem esperar aqui)

  pid.reiniciar();

  if (!MALHA_INICIA_ATIVA) {
    msgTransicao = MSG_NENHUMA;
  }

  unsigned long agora = millis();
  momentoUltimoLoopRapido = agora;
  momentoUltimoSensor = agora;
  momentoUltimoPid = agora;
  momentoUltimoLcd = agora;

  buzzer.tocarConfirmacao();
  delay(800);  // único delay longo: tempo para o usuário ver o splash no LCD
  display.atualizar(setpointEncoder, alvoPotenciaEncoder, NAN, 0.0f, 0, 0,
                    obterEstadoParaDisplay(), modoControle, false,
                    controleMalhaAtivo, msgTransicao, setpointPendenteNaMalha,
                    medidorUso.tempoSegundos(), medidorUso.energiaWh());
}

void loop() {
  unsigned long agora = millis();

  // --- Interface (encoder + buzzer) ---
  if (agora - momentoUltimoLoopRapido >= PERIODO_LOOP_MS) {
    momentoUltimoLoopRapido = agora;
    tarefaInterfaceUsuario();
  }

  // --- Sensor de temperatura ---
  if (agora - momentoUltimoSensor >= PERIODO_SENSOR_MS) {
    momentoUltimoSensor = agora;
    tarefaLeituraSensor();
  }

  // --- Malha PID + atuador ---
  if (agora - momentoUltimoPid >= PERIODO_PID_MS) {
    momentoUltimoPid = agora;
    tarefaMalhaPid(agora);
  }

  // --- Display ---
  if (agora - momentoUltimoLcd >= PERIODO_LCD_MS) {
    momentoUltimoLcd = agora;
    tarefaAtualizarDisplay();
  }
}
