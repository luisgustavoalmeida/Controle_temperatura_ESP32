/**
 * =============================================================================
 * Controle_temperatura_ESP32.ino — Programa principal
 * =============================================================================
 *
 * O que este firmware faz:
 *   1. Lê a temperatura da água (sensor DS18B20).
 *   2. O usuário escolhe a temperatura desejada no encoder rotativo.
 *   3. PID OUT 0..1 → dimmer RobotDyn (0..100 %); atuador a cada ciclo PID.
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
#include "atuador_dimmer.h"
#include "sensor_ds18b20.h"
#include "display_lcd.h"
#include "encoder_rotativo.h"
#include "buzzer.h"
#include "medidor_uso.h"

// =============================================================================
// Módulos de hardware (um objeto por periférico)
// =============================================================================
ControladorPID pid;           // Malha de controle
AtuadorDimmer dimmer;  // RobotDyn AC Light Dimmer (TRIAC)
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
float saidaPid = 0.0f;             // PID OUT 0..1 → dimmer 0..100 %
float setpointEncoder = ALVO_TEMP_PADRAO_C;
float setpointMalha = ALVO_TEMP_PADRAO_C;
float alvoPotenciaEncoder = ALVO_POT_PADRAO_PCT;
float saidaPotenciaManual = 0.0f;
bool setpointPendenteNaMalha = false;
/** -1 = diminuindo, 0 = estavel, 1 = aumentando (enquanto alvo pendente). */
int8_t direcaoAjusteAlvoTemp = 0;
unsigned long momentoUltimoGiroEncoder = 0;
unsigned long momentoUltimaInteracaoEncoder = 0;

ModoControle modoControle = MODO_CONTROLE_PID;

bool metaAtingida = false;
bool estavaNaMeta = false;

bool controleMalhaAtivo = MALHA_INICIA_ATIVA;
MensagemTransicao msgTransicao = MSG_NENHUMA;
unsigned long msgTransicaoAteMs = 0;
bool ultimoZcPresente = false;

// Filtro de temperatura — tamanho em FILTRO_TEMP_AMOSTRAS (config.h)
float historicoTemp[FILTRO_TEMP_AMOSTRAS];
uint8_t indiceHistorico = 0;

/** Falhas consecutivas antes de modo seguro; recuperacao apos leituras boas. */
uint8_t falhasConsecutivasSensor = 0;
uint8_t sucessosConsecutivosSensor = 0;
bool sensorEmErroPersistente = false;

// =============================================================================
// Controle de tempo (cada tarefa só roda quando o intervalo expira)
// =============================================================================
unsigned long momentoUltimoLoopRapido = 0;
unsigned long momentoUltimoSensor = 0;
unsigned long momentoUltimoPid = 0;
unsigned long momentoUltimoLcd = 0;

/** Histerese do atuador: último OUT aplicado no dimmer (−1 = nunca aplicou). */
float saidaPidUltimoAplicado = -1.0f;
unsigned long momentoUltimoAjustePot = 0;

// (Conversão DS18B20: estado interno em sensor_ds18b20.cpp — não bloqueia o loop)

void entrarModoSeguroPorFalhaSensor();

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

/** true se a leitura esta na faixa e sem salto absurdo em relacao ao filtro. */
bool leituraSensorPlausivel(float novaLeituraC) {
  if (isnan(novaLeituraC)) {
    return false;
  }
  if (novaLeituraC < SENSOR_LEITURA_MIN_C || novaLeituraC > SENSOR_LEITURA_MAX_C) {
    return false;
  }
  if (!isnan(temperaturaFiltrada) &&
      fabsf(novaLeituraC - temperaturaFiltrada) > SENSOR_SALTO_MAXIMO_C) {
    return false;
  }
  return true;
}

void registrarFalhaLeituraSensor() {
  falhasConsecutivasSensor++;
  sucessosConsecutivosSensor = 0;

  if (!sensorEmErroPersistente &&
      falhasConsecutivasSensor >= SENSOR_FALHAS_ANTES_ERRO) {
    sensorEmErroPersistente = true;
    Serial.println(F("[SENSOR] Falha persistente — modo seguro"));
    entrarModoSeguroPorFalhaSensor();
    display.invalidarCache();
  } else if (!sensorEmErroPersistente) {
    Serial.print(F("[SENSOR] Leitura invalida ("));
    Serial.print(falhasConsecutivasSensor);
    Serial.print(F("/"));
    Serial.print(SENSOR_FALHAS_ANTES_ERRO);
    Serial.println(F(") — nova tentativa"));
  }

  if (falhasConsecutivasSensor > 0 &&
      (falhasConsecutivasSensor % SENSOR_REENUM_INTERVALO) == 0) {
    sensorTemp.tentarReenumerar();
  }
}

void recuperarSensorAposErro() {
  if (controleMalhaAtivo && modoControle == MODO_CONTROLE_PID &&
      !isnan(temperaturaFiltrada)) {
    pid.sincronizarIntegralParaSaida(saidaPid, setpointMalha, temperaturaFiltrada);
    estavaNaMeta = temperaturaAtingiuMeta();
  }
  display.invalidarCache();
}

void registrarLeituraSensorOk(float novaLeituraC) {
  falhasConsecutivasSensor = 0;
  temperaturaBruta = novaLeituraC;
  aplicarFiltroTemperatura(novaLeituraC);

  if (!sensorEmErroPersistente) {
    sucessosConsecutivosSensor = 0;
    return;
  }

  sucessosConsecutivosSensor++;
  if (sucessosConsecutivosSensor < SENSOR_SUCESSOS_RECUPERACAO) {
    return;
  }

  sucessosConsecutivosSensor = 0;
  sensorEmErroPersistente = false;
  Serial.println(F("[SENSOR] Recuperado — retomando controle"));
  recuperarSensorAposErro();
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
/** Registra instante e OUT do último comando enviado ao dimmer. */
void registrarAplicacaoDimmer(float outAplicado) {
  saidaPidUltimoAplicado = outAplicado;
  momentoUltimoAjustePot = millis();
}

/** OUT (0..1) → dimmer 0..100 %. */
void aplicarSaidaPidNoDimmer() {
  dimmer.definirSaidaNormalizadaRapida(saidaPid);
  registrarAplicacaoDimmer(saidaPid);
}

void aplicarSaidaPotenciaManualNoDimmer() {
  dimmer.definirSaidaNormalizadaRapida(saidaPotenciaManual);
  registrarAplicacaoDimmer(saidaPotenciaManual);
}

/** Standby: potência 0 % sem alterar saidaPid (memória da malha). */
void aplicarDimmerStandby() {
  dimmer.definirSaidaNormalizadaRapida(PID_SAIDA_MIN);
  registrarAplicacaoDimmer(PID_SAIDA_MIN);
}

/** true se ha zero-cross recente no dimmer (rede/chuveiro energizado), mesmo com malha off. */
bool chuveiroEnergizadoNaRede() {
  return dimmer.redeComZeroCross();
}

/** Cronômetro/energia: malha ligada e AC presente (zero-cross recente). */
bool medidorDeveContar() {
  return controleMalhaAtivo && dimmer.redeComZeroCross();
}

/** Evita beep espúrio ao ligar/desligar a malha com ZC já presente. */
void sincronizarEstadoZeroCrossBuzzer() {
  ultimoZcPresente = dimmer.redeComZeroCross();
}

/** Aviso sonoro/visual quando a rede/chuveiro entra ou sai (independe da malha). */
void verificarTransicaoZeroCross() {
  bool zc = dimmer.redeComZeroCross();
  if (zc == ultimoZcPresente) {
    return;
  }
  ultimoZcPresente = zc;
  if (zc) {
    buzzer.tocarRedePresente();
    display.piscarRedePresente();
    Serial.println(F("[DIM] Zero-cross detectado — chuveiro energizado"));
  } else {
    buzzer.tocarRedeAusente();
    display.piscarRedeAusente();
    Serial.println(F("[DIM] Zero-cross perdido — chuveiro desenergizado"));
  }
}

/** true se passou PERIODO_ATUADOR_DIMMER_MS e o OUT desejado mudou. */
bool deveAplicarSaidaNoDimmer(unsigned long instanteAtualMs, float outDesejado) {
  if (saidaPidUltimoAplicado < -0.5f) {
    return true;
  }
  if ((instanteAtualMs - momentoUltimoAjustePot) < PERIODO_ATUADOR_DIMMER_MS) {
    return false;
  }
#if DIMMER_HISTERESIS_SAIDA_ATIVA
  return (fabsf(outDesejado - saidaPidUltimoAplicado) > DIMMER_HISTERESIS_SAIDA_LIMIAR);
#else
  return (fabsf(outDesejado - saidaPidUltimoAplicado) > 0.0001f);
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
  aplicarSaidaPidNoDimmer();
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
  medidorUso.atualizar(potenciaComandoAtual(), medidorDeveContar(), agora);
  medidorUso.atualizar(0.0f, false, agora);

  controleMalhaAtivo = false;
  aplicarDimmerStandby();
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
    aplicarSaidaPidNoDimmer();
  } else if (modoControle == MODO_CONTROLE_POTENCIA) {
    saidaPotenciaManual = alvoPotenciaEncoder / 100.0f;
    aplicarSaidaPotenciaManualNoDimmer();
  } else {
    aplicarSaidaPidNoDimmer();
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
    aplicarSaidaPidNoDimmer();
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
      direcaoAjusteAlvoTemp = 0;
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
      aplicarSaidaPotenciaManualNoDimmer();
    } else {
      aplicarSaidaPidNoDimmer();
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
  Serial.print(dimmer.potenciaAlvoPercentual(), 3);
  Serial.print(F(" PCT="));
  Serial.print(dimmer.potenciaAtualPercentual(), 3);
  float outRef = (modoControle == MODO_CONTROLE_POTENCIA) ? saidaPotenciaManual
                                                          : saidaPid;
  if (fabsf(dimmer.potenciaAtualPercentual() / 100.0f - outRef)
      > SERIAL_TOLERANCIA_ERRO_DIM) {
    Serial.print(F(" dPOT="));
    Serial.print(dimmer.potenciaAtualPercentual() - outRef * 100.0f, 2);
  }
  Serial.print(F(" CMD="));
  Serial.print(dimmer.comandoDimmerPercentual(), 1);
  Serial.print(F(" DIM="));
  Serial.print(dimmer.nivelAtual());
  Serial.print(F(" CUR="));
  Serial.print(dimmer.tipoCurvaBiblioteca());
  Serial.print(F(" DLY_US="));
  Serial.print(dimmer.atrasoDisparoUs());
  Serial.print(F(" META="));
  Serial.println(metaAtingida ? 1 : 0);
}
#endif

/** Copia setpointEncoder -> setpointMalha apos pausa sem giro (ALVO_TEMP_PAUSA_MS).
 *  Enquanto pendente, o PID continua no setpointMalha antigo; so o LCD/encoder mudam.
 */
void atualizarSetpointMalhaSeEstavel() {
  if (!setpointPendenteNaMalha) {
    return;
  }
  if (millis() - momentoUltimoGiroEncoder < ALVO_TEMP_PAUSA_MS) {
    return;
  }
  if (fabsf(setpointMalha - setpointEncoder) < 0.001f) {
    setpointPendenteNaMalha = false;
    direcaoAjusteAlvoTemp = 0;
    return;
  }
  setpointMalha = setpointEncoder;
  setpointPendenteNaMalha = false;
  direcaoAjusteAlvoTemp = 0;
  pid.sincronizarIntegralParaSaida(saidaPid, setpointMalha, temperaturaFiltrada);
  estavaNaMeta = temperaturaAtingiuMeta();
  display.invalidarCache();
  Serial.print(F("[SP] Alvo aplicado na malha: "));
  Serial.println(setpointMalha, 2);
}

EstadoSistema obterEstadoParaDisplay() {
  if (sensorEmErroPersistente) {
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
  verificarTransicaoZeroCross();
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
        aplicarSaidaPotenciaManualNoDimmer();
        display.invalidarCache();
      }
    }
  } else if (girou || fabsf(spEncoder - setpointEncoder) > 0.001f) {
    if (spEncoder > setpointEncoder + 0.0001f) {
      direcaoAjusteAlvoTemp = 1;
    } else if (spEncoder < setpointEncoder - 0.0001f) {
      direcaoAjusteAlvoTemp = -1;
    }
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
        direcaoAjusteAlvoTemp = 0;
        ligarMalhaControle(temperaturaMuitoAbaixoDoAlvo());
      } else {
        saidaPotenciaManual = alvoPotenciaEncoder / 100.0f;
        ligarMalhaControle(false);
        aplicarSaidaPotenciaManualNoDimmer();
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

  if (!sensorTemp.sensorOk()) {
    sensorTemp.tentarReenumerar();
  }

  if (!sensorTemp.atualizar(&novaLeitura)) {
    return;  // conversão ainda em andamento — sair sem bloquear
  }

  if (!leituraSensorPlausivel(novaLeitura)) {
    registrarFalhaLeituraSensor();
    return;
  }

  registrarLeituraSensorOk(novaLeitura);
}

/**
 * Tarefa do controle (~PERIODO_PID_MS): PID ou potência manual conforme o modo.
 */
void tarefaMalhaPid(unsigned long instanteAtualMs) {
  if (!controleMalhaAtivo) {
    if (fabsf(saidaPidUltimoAplicado - PID_SAIDA_MIN) > 0.0001f
        || dimmer.nivelAtual() != 0) {
      aplicarDimmerStandby();
    }
#if SERIAL_DEPURAR_MALHA
    imprimirSerialMalha();
#endif
    return;
  }

  if (sensorEmErroPersistente) {
#if SERIAL_DEPURAR_MALHA
    imprimirSerialMalha();
#endif
    return;
  }

  if (isnan(temperaturaFiltrada)) {
#if SERIAL_DEPURAR_MALHA
    imprimirSerialMalha();
#endif
    return;
  }

  if (modoControle == MODO_CONTROLE_POTENCIA) {
    saidaPotenciaManual = alvoPotenciaEncoder / 100.0f;
    if (deveAplicarSaidaNoDimmer(instanteAtualMs, saidaPotenciaManual)) {
      aplicarSaidaPotenciaManualNoDimmer();
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
  if (deveAplicarSaidaNoDimmer(instanteAtualMs, saidaPid)) {
    aplicarSaidaPidNoDimmer();
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
  medidorUso.atualizar(potCmd, medidorDeveContar(), agora);

  display.atualizar(setpointEncoder, alvoPotenciaEncoder, temperaturaFiltrada,
                    potCmd, obterEstadoParaDisplay(), modoControle, metaAtingida,
                    controleMalhaAtivo, chuveiroEnergizadoNaRede(), msgTransicao,
                    setpointPendenteNaMalha,
                    direcaoAjusteAlvoTemp, medidorUso.tempoSegundos(),
                    medidorUso.energiaWh());
}

// -----------------------------------------------------------------------------
// setup() e loop()
// -----------------------------------------------------------------------------

void setup() {
  Serial.begin(SERIAL_VELOCIDADE);
  delay(500);
  Serial.println(F("=== Inicio: controle chuveiro ESP32 ==="));
  Serial.println(F("[DIM] rbdimmerESP32 + Arduino-ESP32 3.x (pioarduino)"));
  Serial.print(F("[DIM] ZC="));
  Serial.print(PINO_DIMMER_ZC);
  Serial.print(F(" PSM="));
  Serial.println(PINO_DIMMER_PSM);
#if SERIAL_DEPURAR_MALHA
  Serial.print(F("[MALHA] PID "));
  Serial.print(PERIODO_PID_MS);
  Serial.print(F("ms | dimmer "));
  Serial.print(PERIODO_ATUADOR_DIMMER_MS);
  Serial.print(F("ms dOUT>"));
#if DIMMER_HISTERESIS_SAIDA_ATIVA
  Serial.print(DIMMER_HISTERESIS_SAIDA_LIMIAR, 3);
#else
  Serial.print(F("passo"));
#endif
  Serial.println(F(" | corte de fase"));
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
  direcaoAjusteAlvoTemp = 0;

  dimmer.iniciar();
  medidorUso.iniciar();
  sincronizarEstadoZeroCrossBuzzer();

  if (!MALHA_INICIA_ATIVA) {
    controleMalhaAtivo = false;
    Serial.println(F("[DIM] Posicao standby..."));
    dimmer.definirSaidaNormalizadaRapida(PID_SAIDA_MIN);
    registrarAplicacaoDimmer(PID_SAIDA_MIN);
    Serial.println(F("[DIM] Standby — potencia 0 %"));
  } else {
    dimmer.definirPotenciaMaxima();
    medidorUso.reiniciar(millis());
    Serial.println(F("[DIM] Nivel maximo (potencia maxima)"));
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
  display.atualizar(setpointEncoder, alvoPotenciaEncoder, NAN, 0.0f,
                    obterEstadoParaDisplay(), modoControle, false,
                    controleMalhaAtivo, chuveiroEnergizadoNaRede(), msgTransicao,
                    setpointPendenteNaMalha,
                    direcaoAjusteAlvoTemp, medidorUso.tempoSegundos(),
                    medidorUso.energiaWh());
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
