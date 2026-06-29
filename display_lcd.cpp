/**
 * display_lcd.cpp — Renderização do LCD com cache (evita flicker)
 *
 * Só redesenha linhas quando setpoint, PV, OUT ou estado mudam.
 * Animação "PID." na linha 4 usa LCD_ANIM_PID_MS (config.h).
 */

#include "display_lcd.h"
#include "config.h"
#include "lcd_i2c_compat.h"
#include <string.h>
#include <LiquidCrystal_I2C.h>

static LiquidCrystal_I2C lcd(LCD_I2C_CTOR_ARGS);

void DisplayLCD::escreverLinha(uint8_t linha, const char* texto) {
  lcd.setCursor(0, linha);
  lcd.print(texto);
  int len = strlen(texto);
  for (int i = len; i < LCD_COLUNAS; i++) {
    lcd.print(' ');
  }
}

void DisplayLCD::invalidarCache() {
  _ultimoSetpoint = -999.0f;
  _ultimoAtual = -999.0f;
  _ultimaPotCmd = -1.0f;
  _ultimoPassoPotA = 0xFF;
  _ultimoPassoPotB = 0xFF;
  _ultimoEstado = ESTADO_INICIALIZANDO;
  _ultimaMsgTransicao = MSG_NENHUMA;
  _ultimoFrameAnimPid = 255;
  _ultimoSetpointPendente = false;
}

void DisplayLCD::montarLinhaBuscandoTemp(char* buffer, size_t tam, float percentual,
                                         uint8_t frameAnim) {
  static const char* sufixos[] = {"", ".", "..", "..."};
  frameAnim %= 4;
  snprintf(buffer, tam, "Pot:%5.1f%% | PID%s", percentual, sufixos[frameAnim]);
}

bool DisplayLCD::iniciar() {
  lcdI2cWireBegin();
  lcdI2cIniciar(lcd);
  _ok = true;
  invalidarCache();
  _ultimaMeta = false;
  _ultimoControleAtivo = true;
  _ultimoFrameAnimPid = 255;
  return _ok;
}

void DisplayLCD::splashInicializacao() {
  lcd.clear();
  escreverLinha(0, "Controle Temperatura");
  escreverLinha(1, "ESP32 + PID");
  escreverLinha(2, "Inicializando...");
  escreverLinha(3, "I2C 0x27 OK");
}

void DisplayLCD::atualizar(float setpointC, float atualC, float potenciaCmd01,
                           uint8_t passoPotA, uint8_t passoPotB, EstadoSistema estado,
                           bool metaAtingida, bool controleAtivo,
                           MensagemTransicao msgTransicao, bool setpointPendenteNaMalha) {
  if (!_ok) {
    return;
  }

  bool buscandoTemperatura =
      controleAtivo && (estado == ESTADO_PID_ATIVO) && !metaAtingida &&
      (msgTransicao == MSG_NENHUMA);
  uint8_t frameAnimPid =
      buscandoTemperatura
          ? (uint8_t)((millis() / LCD_ANIM_PID_MS) % 4)
          : 0;

  bool precisaAtualizar =
      (fabsf(setpointC - _ultimoSetpoint) > 0.001f) ||
      (fabsf(atualC - _ultimoAtual) > 0.01f) ||
      (fabsf(potenciaCmd01 - _ultimaPotCmd) > 0.001f) ||
      (passoPotA != _ultimoPassoPotA) || (passoPotB != _ultimoPassoPotB) ||
      (estado != _ultimoEstado) || (metaAtingida != _ultimaMeta) ||
      (controleAtivo != _ultimoControleAtivo) ||
      (msgTransicao != _ultimaMsgTransicao) ||
      (setpointPendenteNaMalha != _ultimoSetpointPendente) ||
      (buscandoTemperatura && frameAnimPid != _ultimoFrameAnimPid);

  if (!precisaAtualizar) {
    return;
  }

  _ultimoSetpoint = setpointC;
  _ultimoAtual = atualC;
  _ultimaPotCmd = potenciaCmd01;
  _ultimoPassoPotA = passoPotA;
  _ultimoPassoPotB = passoPotB;
  _ultimoEstado = estado;
  _ultimaMeta = metaAtingida;
  _ultimoControleAtivo = controleAtivo;
  _ultimaMsgTransicao = msgTransicao;
  _ultimoFrameAnimPid = frameAnimPid;
  _ultimoSetpointPendente = setpointPendenteNaMalha;

  char buffer[21];

  if (msgTransicao == MSG_ATIVANDO_MALHA) {
    escreverLinha(0, "Ligando Malha PID...");
  } else if (msgTransicao == MSG_DESATIVANDO_MALHA) {
    escreverLinha(0, "Desligando Malha PID...");
  } else if (!controleAtivo) {
    escreverLinha(0, "Controle PID OFF");
  } else {
    escreverLinha(0, "Controle PID ON");
  }

  if (setpointPendenteNaMalha) {
    snprintf(buffer, sizeof(buffer), "Alvo: %6.2f >", setpointC);
  } else {
    snprintf(buffer, sizeof(buffer), "Alvo: %6.2f C", setpointC);
  }
  escreverLinha(1, buffer);

  if (estado == ESTADO_SENSOR_ERRO) {
    escreverLinha(2, "Atual: sensor falhou");
  } else if (estado == ESTADO_AGUARDE_SENSOR) {
    escreverLinha(2, "Atual: aguarde leit.");
  } else if (isnan(atualC)) {
    escreverLinha(2, "Atual:   --- C");
  } else {
    snprintf(buffer, sizeof(buffer), "Atual: %6.2f C", atualC);
    escreverLinha(2, buffer);
  }

  float pctCmd = potenciaCmd01 * 100.0f;
  if (pctCmd < 0.0f) {
    pctCmd = 0.0f;
  }
  if (pctCmd > 100.0f) {
    pctCmd = 100.0f;
  }

  if (estado == ESTADO_SENSOR_ERRO) {
    escreverLinha(3, "Pot: MIN | FALHA SENS");
  } else if (!controleAtivo || estado == ESTADO_CONTROLE_DESLIGADO) {
    escreverLinha(3, "Pot:   0% | OFF");
  } else if (msgTransicao == MSG_ATIVANDO_MALHA) {
    escreverLinha(3, "PID: iniciando...");
  } else if (msgTransicao == MSG_DESATIVANDO_MALHA) {
    escreverLinha(3, "PID: encerrando...");
  } else if (metaAtingida) {
    snprintf(buffer, sizeof(buffer), "Pot:%5.1f%% | Temp OK", pctCmd);
    escreverLinha(3, buffer);
  } else {
    montarLinhaBuscandoTemp(buffer, sizeof(buffer), pctCmd, frameAnimPid);
    escreverLinha(3, buffer);
  }
}
