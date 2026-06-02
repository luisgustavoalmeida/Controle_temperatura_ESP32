/**
 * display_lcd.cpp — Layout do LCD com mensagens de operacao
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
  for (int i = len; i < LCD_COLS; i++) {
    lcd.print(' ');
  }
}

void DisplayLCD::invalidarCache() {
  _ultimoSetpoint = -999.0f;
  _ultimoAtual = -999.0f;
  _ultimaSaida = -1.0f;
  _ultimoEstado = ESTADO_INICIALIZANDO;
  _ultimaMsgTransicao = MSG_NENHUMA;
  _ultimoFrameAnimPid = 255;
  _ultimoSetpointPendente = false;
}

void DisplayLCD::montarLinhaBuscandoTemp(char* buffer, size_t tam, int percentual,
                                         uint8_t frameAnim) {
  static const char* sufixos[] = {"", ".", "..", "..."};
  frameAnim %= 4;
  snprintf(buffer, tam, "Pot:%3d%% | PID%s", percentual, sufixos[frameAnim]);
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

void DisplayLCD::atualizar(float setpointC, float atualC, float saidaPid,
                           EstadoSistema estado, bool metaAtingida,
                           bool controleAtivo, MensagemTransicao msgTransicao,
                           bool setpointPendenteNaMalha) {
  if (!_ok) {
    return;
  }

  bool buscandoTemperatura =
      controleAtivo && (estado == ESTADO_PID_ATIVO) && !metaAtingida &&
      (msgTransicao == MSG_NENHUMA);
  uint8_t frameAnimPid =
      buscandoTemperatura
          ? (uint8_t)((millis() / DISPLAY_ANIM_PID_MS) % 4)
          : 0;

  bool precisaAtualizar =
      (fabsf(setpointC - _ultimoSetpoint) > 0.01f) ||
      (fabsf(atualC - _ultimoAtual) > 0.01f) ||
      (fabsf(saidaPid - _ultimaSaida) > 0.01f) ||
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
  _ultimaSaida = saidaPid;
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

  int percentual = (int)(saidaPid * 100.0f + 0.5f);
  if (percentual < 0) percentual = 0;
  if (percentual > 100) percentual = 100;

  if (estado == ESTADO_SENSOR_ERRO) {
    escreverLinha(3, "Pot: MIN | FALHA SENS");
  } else if (!controleAtivo || estado == ESTADO_CONTROLE_DESLIGADO) {
    escreverLinha(3, "Pot:   0% | OFF");
  } else if (msgTransicao == MSG_ATIVANDO_MALHA) {
    escreverLinha(3, "PID: iniciando...");
  } else if (msgTransicao == MSG_DESATIVANDO_MALHA) {
    escreverLinha(3, "PID: encerrando...");
  } else if (metaAtingida) {
    snprintf(buffer, sizeof(buffer), "Pot:%3d%% | Temp OK", percentual);
    escreverLinha(3, buffer);
  } else {
    montarLinhaBuscandoTemp(buffer, sizeof(buffer), percentual, frameAnimPid);
    escreverLinha(3, buffer);
  }
}
