/**
 * =============================================================================
 * Controle_temperatura_ESP32.ino — Programa principal
 * =============================================================================
 *
 * O que este firmware faz:
 *   1. Lê a temperatura da água (sensor DS18B20).
 *   2. O usuário escolhe a temperatura desejada no encoder rotativo.
 *   3. PID OUT 0..1 → passos TPL0501 via SPI; atuador a cada ciclo PID.
 *   4. A malha nao pausa na meta: regulacao continua (histerese so no buzzer).
 *   5. Duplo clique = standby (pot. 0, malha preservada); clique = religar; longo = reiniciar PID.
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

// =============================================================================
// Módulos de hardware (um objeto por periférico)
// =============================================================================
ControladorPID pid;           // Malha de controle
AtuadorPotenciometro potenciometro;  // Atuador (1× ou 2× TPL0501)
SensorDS18B20 sensorTemp;     // Entrada (temperatura)
DisplayLCD display;           // Saída visual
EncoderRotativo encoder;      // Entrada do usuário (setpoint)
Buzzer buzzer;                // Saída sonora

// =============================================================================
// Variáveis da malha de controle
// =============================================================================
float temperaturaBruta = NAN;      // Última leitura direta do DS18B20 [°C]
float temperaturaFiltrada = NAN;   // Média móvel (menos ruído) [°C]
float saidaPid = 0.0f;             // PID OUT 0..1 → potência (1=máx); passo via escala ideal 150 kΩ
float setpointEncoder = ALVO_TEMP_PADRAO_C;
float setpointMalha = ALVO_TEMP_PADRAO_C;
bool setpointPendenteNaMalha = false;
unsigned long momentoUltimoGiroEncoder = 0;

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
    Serial.println(F("[BUZZER] Entrou na meta"));
  } else if (!naMetaAgora && estavaNaMeta) {
    buzzer.tocarForaDaMeta();
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

/** OUT (0..1) → TPL0501: salto SPI direto a cada atualização do PID. */
void aplicarSaidaPidNoPotenciometro() {
  potenciometro.definirSaidaNormalizadaRapida(saidaPid);
  registrarAplicacaoPot(saidaPid);
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
  aplicarSaidaPidNoPotenciometro();
  pid.reiniciar();
}

/** Standby: pot. 0 %; preserva saidaPid e estado do PID. */
void desligarMalhaControle() {
  controleMalhaAtivo = false;
  aplicarPotenciometroStandby();
  estavaNaMeta = false;
  metaAtingida = false;
  msgTransicao = MSG_DESATIVANDO_MALHA;
  msgTransicaoAteMs = millis() + LCD_MSG_TRANSICAO_MS;
  display.invalidarCache();
  Serial.print(F("[CTRL] Standby — pot. 0 | OUT mem="));
  Serial.println(saidaPid, 3);
}

/** Reativa malha; opcionalmente reinicia PID (integral zerada). */
void ligarMalhaControle(bool reiniciarPid) {
  controleMalhaAtivo = true;
  if (reiniciarPid) {
    pid.reiniciar();
    saidaPid = PID_SAIDA_MIN;
  }
  aplicarSaidaPidNoPotenciometro();
  estavaNaMeta = false;
  metaAtingida = false;
  msgTransicao = MSG_ATIVANDO_MALHA;
  msgTransicaoAteMs = millis() + LCD_MSG_TRANSICAO_MS;
  display.invalidarCache();
  Serial.print(F("[CTRL] Malha ON | reinicia="));
  Serial.print(reiniciarPid ? '1' : '0');
  Serial.print(F(" OUT="));
  Serial.println(saidaPid, 3);
}

void reiniciarMalhaPid() {
  pid.reiniciar();
  estavaNaMeta = false;
  metaAtingida = false;
  if (controleMalhaAtivo) {
    saidaPid = PID_SAIDA_MIN;
    aplicarSaidaPidNoPotenciometro();
  }
  Serial.println(F("[CTRL] PID reiniciado (clique longo)"));
}

void atualizarMensagemTransicao() {
  if (msgTransicao != MSG_NENHUMA && millis() >= msgTransicaoAteMs) {
    msgTransicao = MSG_NENHUMA;
    display.invalidarCache();
  }
}

#if SERIAL_DEPURAR_MALHA
/** Uma linha por passo do PID (~PERIODO_PID_MS) — somente malha de controle. */
void imprimirSerialMalha() {
  Serial.print(F("[MALHA]"));
  Serial.print(F(" ACT="));
  Serial.print(controleMalhaAtivo ? 1 : 0);
  Serial.print(F(" SP="));
  Serial.print(setpointMalha, 2);
  Serial.print(F(" PV="));
  if (isnan(temperaturaFiltrada)) {
    Serial.print(F("---"));
  } else {
    Serial.print(temperaturaFiltrada, 2);
  }
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
  Serial.print(F(" PCT_CMD="));
  Serial.print(potenciometro.potenciaAlvoPercentual(), 3);
  Serial.print(F(" PCT="));
  Serial.print(potenciometro.potenciaAtualPercentual(), 3);
  if (fabsf(potenciometro.potenciaAtualPercentual() / 100.0f - saidaPid)
      > SERIAL_TOLERANCIA_ERRO_POT) {
    Serial.print(F(" dPOT="));
    Serial.print(potenciometro.potenciaAtualPercentual() - saidaPid * 100.0f, 2);
  }
  Serial.print(F(" POT="));
  Serial.print(potenciometro.passoAtualA());
#if POT_USA_DOIS_CHIPS
  Serial.print(F("/"));
  Serial.print(potenciometro.passoAtualB());
  if (potenciometro.passoAtualA() != potenciometro.passoAlvoA()
      || potenciometro.passoAtualB() != potenciometro.passoAlvoB()) {
    Serial.print(F(" ALVO="));
    Serial.print(potenciometro.passoAlvoA());
    Serial.print(F("/"));
    Serial.print(potenciometro.passoAlvoB());
  }
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
  return ESTADO_PID_ATIVO;
}

// -----------------------------------------------------------------------------
// Tarefas periódicas (chamadas pelo loop)
// -----------------------------------------------------------------------------

/**
 * Tarefa rápida (~10 ms): encoder + buzzer.
 * O usuário sente resposta imediata ao girar ou pressionar o botão.
 */
void tarefaInterfaceUsuario() {
  encoder.atualizar();
  float spEncoder = encoder.setpointC();
  bool girou = false;
  while (encoder.consumirEventoRotacao()) {
    buzzer.tocarClique();
    girou = true;
  }

  if (girou || fabsf(spEncoder - setpointEncoder) > 0.001f) {
    setpointEncoder = spEncoder;
    momentoUltimoGiroEncoder = millis();
    setpointPendenteNaMalha = true;
  }

  atualizarSetpointMalhaSeEstavel();
  atualizarMensagemTransicao();

  if (encoder.consumirEventoDuploClique()) {
    buzzer.tocarConfirmacao();
    if (controleMalhaAtivo) {
      desligarMalhaControle();
    }
  }

  if (encoder.consumirEventoCliqueLongo()) {
    buzzer.tocarConfirmacao();
    reiniciarMalhaPid();
  }

  if (encoder.consumirEventoClique()) {
    if (!controleMalhaAtivo) {
      buzzer.tocarConfirmacao();
      setpointMalha = setpointEncoder;
      setpointPendenteNaMalha = false;
      ligarMalhaControle(temperaturaMuitoAbaixoDoAlvo());
    }
  }

  buzzer.atualizar();
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
 * Tarefa do PID (~PERIODO_PID_MS): calcula OUT; atuador ajusta a cada
 * PERIODO_ATUADOR_POT_MS quando o par (A,B) alvo muda (sem histerese de OUT).
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

/** Tarefa do LCD (~100 ms): linha 3 = OUT do PID em % (comando da malha). */
void tarefaAtualizarDisplay() {
  float potCmd = controleMalhaAtivo ? saidaPid : 0.0f;
  display.atualizar(setpointEncoder, temperaturaFiltrada, potCmd,
                    potenciometro.passoAtualA(), potenciometro.passoAtualB(),
                    obterEstadoParaDisplay(), metaAtingida, controleMalhaAtivo,
                    msgTransicao, setpointPendenteNaMalha);
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
  setpointEncoder = encoder.setpointC();
  setpointMalha = setpointEncoder;
  momentoUltimoGiroEncoder = millis();
  setpointPendenteNaMalha = false;

  potenciometro.iniciar();

  if (!MALHA_INICIA_ATIVA) {
    controleMalhaAtivo = false;
    Serial.println(F("[POT] Calculando posicao standby..."));
    potenciometro.definirSaidaNormalizadaRapida(PID_SAIDA_MIN);
    registrarAplicacaoPot(PID_SAIDA_MIN);
    Serial.println(F("[POT] Standby — potencia 0 % (inicializacao rapida)"));
  } else {
    potenciometro.reiniciarParaMinimo();
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
  display.atualizar(setpointEncoder, NAN, 0.0f, 0, 0, obterEstadoParaDisplay(), false,
                    controleMalhaAtivo, msgTransicao, setpointPendenteNaMalha);
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
