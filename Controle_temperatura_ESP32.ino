/**
 * =============================================================================
 * Controle_temperatura_ESP32.ino — Programa principal
 * =============================================================================
 *
 * O que este firmware faz:
 *   1. Lê a temperatura da água (sensor DS18B20).
 *   2. O usuário escolhe a temperatura desejada no encoder rotativo.
 *   3. PID calcula OUT (0..1); esse valor comanda o X9C104S a cada passo (~100 ms).
 *   4. A malha nao pausa na meta: regulacao continua (histerese so no buzzer).
 *   5. LCD e buzzer; duplo clique no encoder liga/desliga a malha (standby = pot. 0).
 *
 * Ganhos PID (projeto Malha_PID_temperatura): Kp=0,032  Ki=0,002  Kd=0,015
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
#include "x9c104s.h"
#include "sensor_ds18b20.h"
#include "display_lcd.h"
#include "encoder_rotativo.h"
#include "buzzer.h"

// =============================================================================
// Módulos de hardware (um objeto por periférico)
// =============================================================================
ControladorPID pid;           // Malha de controle
X9C104S potenciometro;        // Atuador (potência do chuveiro)
SensorDS18B20 sensorTemp;     // Entrada (temperatura)
DisplayLCD display;           // Saída visual
EncoderRotativo encoder;      // Entrada do usuário (setpoint)
Buzzer buzzer;                // Saída sonora

// =============================================================================
// Variáveis da malha de controle
// =============================================================================
float temperaturaBruta = NAN;      // Última leitura direta do DS18B20 [°C]
float temperaturaFiltrada = NAN;   // Média móvel (menos ruído) [°C]
float saidaPid = 0.0f;             // Saída do PID: 0,0 = mín potência, 1,0 = máx
float setpointAtual = SETPOINT_PADRAO_C;  // Temperatura desejada [°C]

bool metaAtingida = false;
bool estavaNaMeta = false;

bool controleMalhaAtivo = CONTROLE_INICIA_LIGADO;
MensagemTransicao msgTransicao = MSG_NENHUMA;
unsigned long msgTransicaoAteMs = 0;

// Filtro de temperatura: guarda as últimas N leituras para calcular média
#define FILTRO_TEMP_N  3
float historicoTemp[FILTRO_TEMP_N];
uint8_t indiceHistorico = 0;

// =============================================================================
// Controle de tempo (cada tarefa só roda quando o intervalo expira)
// =============================================================================
unsigned long momentoUltimoLoopRapido = 0;
unsigned long momentoUltimoSensor = 0;
unsigned long momentoUltimoPid = 0;
unsigned long momentoUltimoLcd = 0;

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
  indiceHistorico = (indiceHistorico + 1) % FILTRO_TEMP_N;

  float soma = 0.0f;
  int quantidadeValida = 0;
  for (int i = 0; i < FILTRO_TEMP_N; i++) {
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

/** true quando a temperatura filtrada está perto o suficiente do alvo */
bool temperaturaAtingiuMeta() {
  if (isnan(temperaturaFiltrada)) {
    return false;
  }
  float diferenca = fabsf(temperaturaFiltrada - setpointAtual);
  return (diferenca <= HISTERESE_BUZZER_C);
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
/** OUT (0..1) e o unico comando da malha para o X9C104S. */
void aplicarSaidaPidNoPotenciometro() {
  potenciometro.definirSaidaNormalizada(saidaPid);
}

void entrarModoSeguroPorFalhaSensor() {
  saidaPid = PID_SAIDA_MIN;
  aplicarSaidaPidNoPotenciometro();
  pid.reiniciar();
}

/** Forca potencia minima e pausa o PID (standby do usuario). */
void desligarMalhaControle() {
  controleMalhaAtivo = false;
  saidaPid = PID_SAIDA_MIN;
  aplicarSaidaPidNoPotenciometro();
  pid.reiniciar();
  estavaNaMeta = false;
  metaAtingida = false;
  msgTransicao = MSG_DESATIVANDO_MALHA;
  msgTransicaoAteMs = millis() + DISPLAY_MSG_TRANSICAO_MS;
  display.invalidarCache();
  Serial.println(F("[CTRL] Malha DESLIGADA — pot. minimo"));
}

/** Reativa regulacao de temperatura (PID). */
void ligarMalhaControle() {
  controleMalhaAtivo = true;
  pid.reiniciar();
  estavaNaMeta = false;
  metaAtingida = false;
  msgTransicao = MSG_ATIVANDO_MALHA;
  msgTransicaoAteMs = millis() + DISPLAY_MSG_TRANSICAO_MS;
  display.invalidarCache();
  Serial.println(F("[CTRL] Malha LIGADA — PID ativo"));
}

void atualizarMensagemTransicao() {
  if (msgTransicao != MSG_NENHUMA && millis() >= msgTransicaoAteMs) {
    msgTransicao = MSG_NENHUMA;
    display.invalidarCache();
  }
}

#if SERIAL_DEBUG_MALHA
/** Uma linha por passo do PID (~PERIODO_PID_MS) — somente malha de controle. */
void imprimirSerialMalha() {
  Serial.print(F("[MALHA]"));
  Serial.print(F(" ACT="));
  Serial.print(controleMalhaAtivo ? 1 : 0);
  Serial.print(F(" SP="));
  Serial.print(setpointAtual, 2);
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
  Serial.print(saidaPid, 3);
  Serial.print(F(" POT="));
  Serial.print(potenciometro.passoAtual());
  Serial.print(F(" META="));
  Serial.println(metaAtingida ? 1 : 0);
}
#endif

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
  setpointAtual = encoder.setpointC();
  atualizarMensagemTransicao();

  if (encoder.consumirEventoDuploClique()) {
    buzzer.tocarConfirmacao();
    if (controleMalhaAtivo) {
      desligarMalhaControle();
    } else {
      ligarMalhaControle();
    }
  }

  if (encoder.consumirEventoRotacao()) {
    buzzer.tocarClique();
    estavaNaMeta = false;
  }

  if (encoder.consumirEventoClique()) {
    if (controleMalhaAtivo) {
      buzzer.tocarConfirmacao();
      pid.reiniciar();
      estavaNaMeta = false;
      Serial.println(F("[UI] Clique: PID reiniciado"));
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
 * Tarefa do PID (~100 ms): calcula potência e comanda o X9C104S.
 */
void tarefaMalhaPid(unsigned long instanteAtualMs) {
  if (!controleMalhaAtivo) {
    saidaPid = PID_SAIDA_MIN;
    aplicarSaidaPidNoPotenciometro();
#if SERIAL_DEBUG_MALHA
    imprimirSerialMalha();
#endif
    return;
  }

  if (!sensorTemp.sensorOk() || isnan(temperaturaFiltrada)) {
    entrarModoSeguroPorFalhaSensor();
#if SERIAL_DEBUG_MALHA
    imprimirSerialMalha();
#endif
    return;
  }

  float tempoSegundos = instanteAtualMs / 1000.0f;
  saidaPid = pid.passo(setpointAtual, temperaturaFiltrada, tempoSegundos);
  aplicarSaidaPidNoPotenciometro();

  metaAtingida = temperaturaAtingiuMeta();
  avisarMudancaDeMeta(metaAtingida);

#if SERIAL_DEBUG_MALHA
  imprimirSerialMalha();
#endif
}

/** Tarefa do LCD (~300 ms): atualiza as 4 linhas se algo mudou */
void tarefaAtualizarDisplay() {
  display.atualizar(setpointAtual, temperaturaFiltrada, saidaPid,
                    obterEstadoParaDisplay(), metaAtingida, controleMalhaAtivo,
                    msgTransicao);
}

// -----------------------------------------------------------------------------
// setup() e loop()
// -----------------------------------------------------------------------------

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);
  Serial.println(F("=== Inicio: controle chuveiro ESP32 ==="));
#if SERIAL_DEBUG_MALHA
  Serial.println(F("[MALHA] Serial: SP PV ERR P I D OUT POT META ACT (a cada PID)"));
#endif

  // Inicialização na ordem: feedback visual → entradas → atuador → sensor → PID
  display.iniciar();
  display.splashInicializacao();

  buzzer.iniciar();
  encoder.iniciar();
  setpointAtual = encoder.setpointC();

  potenciometro.iniciar();
  potenciometro.reiniciarParaMinimo();
  Serial.println(F("[X9C] Wiper no minimo (potencia minima)"));

  for (int i = 0; i < FILTRO_TEMP_N; i++) {
    historicoTemp[i] = NAN;
  }

  sensorTemp.iniciar();  // já dispara a 1ª conversão assíncrona (sem esperar aqui)

  pid.reiniciar();

  if (!CONTROLE_INICIA_LIGADO) {
    desligarMalhaControle();
    msgTransicao = MSG_NENHUMA;
  }

  unsigned long agora = millis();
  momentoUltimoLoopRapido = agora;
  momentoUltimoSensor = agora;
  momentoUltimoPid = agora;
  momentoUltimoLcd = agora;

  buzzer.tocarConfirmacao();
  delay(800);  // único delay longo: tempo para o usuário ver o splash no LCD
  display.atualizar(setpointAtual, NAN, 0.0f, obterEstadoParaDisplay(), false,
                    controleMalhaAtivo, msgTransicao);
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
