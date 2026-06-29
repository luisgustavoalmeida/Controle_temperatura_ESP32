/**
 * display_lcd.h — LCD 20×4 I2C (PCF8574)
 *
 * Layout típico:
 *   Linha 0 — alvo °C
 *   Linha 1 — temperatura medida
 *   Linha 2 — potência % (comando PID)
 *   Linha 3 — estado / animação PID
 */

#ifndef DISPLAY_LCD_H
#define DISPLAY_LCD_H

#include <Arduino.h>

enum EstadoSistema {
  ESTADO_INICIALIZANDO,
  ESTADO_PID_ATIVO,
  ESTADO_SENSOR_ERRO,
  ESTADO_AGUARDE_SENSOR,
  ESTADO_CONTROLE_DESLIGADO
};

enum MensagemTransicao {
  MSG_NENHUMA,
  MSG_ATIVANDO_MALHA,
  MSG_DESATIVANDO_MALHA
};

class DisplayLCD {
public:
  bool iniciar();
  void splashInicializacao();

  /** potenciaCmd01 = OUT do PID 0..1 (exibido como % na linha 3) */
  void atualizar(float setpointC, float atualC, float potenciaCmd01,
                 uint8_t passoPotA, uint8_t passoPotB, EstadoSistema estado,
                 bool metaAtingida, bool controleAtivo, MensagemTransicao msgTransicao,
                 bool setpointPendenteNaMalha);

  void invalidarCache();

private:
  bool _ok;
  float _ultimoSetpoint;
  float _ultimoAtual;
  float _ultimaPotCmd;
  uint8_t _ultimoPassoPotA;
  uint8_t _ultimoPassoPotB;
  EstadoSistema _ultimoEstado;
  bool _ultimaMeta;
  bool _ultimoControleAtivo;
  MensagemTransicao _ultimaMsgTransicao;
  uint8_t _ultimoFrameAnimPid;
  bool _ultimoSetpointPendente;

  void escreverLinha(uint8_t linha, const char* texto);
  static void montarLinhaBuscandoTemp(char* buffer, size_t tam, float percentual,
                                      uint8_t frameAnim);
};

#endif // DISPLAY_LCD_H
