/**
 * display_lcd.h — LCD 20x4 (status da malha, temperaturas e mensagens)
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

  void atualizar(float setpointC, float atualC, float saidaPid, EstadoSistema estado,
                 bool metaAtingida, bool controleAtivo, MensagemTransicao msgTransicao);

  void invalidarCache();

private:
  bool _ok;
  float _ultimoSetpoint;
  float _ultimoAtual;
  float _ultimaSaida;
  EstadoSistema _ultimoEstado;
  bool _ultimaMeta;
  bool _ultimoControleAtivo;
  MensagemTransicao _ultimaMsgTransicao;
  uint8_t _ultimoFrameAnimPid;

  void escreverLinha(uint8_t linha, const char* texto);
  static void montarLinhaBuscandoTemp(char* buffer, size_t tam, int percentual,
                                      uint8_t frameAnim);
};

#endif // DISPLAY_LCD_H
