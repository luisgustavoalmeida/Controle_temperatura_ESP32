/**
 * encoder_rotativo.h — Encoder KY-040 (CLK, DT, SW)
 *
 * Giro normal / fino (botao+giro), clique simples, duplo clique no botao.
 */

#ifndef ENCODER_ROTATIVO_H
#define ENCODER_ROTATIVO_H

#include <Arduino.h>

class EncoderRotativo {
public:
  void iniciar();
  void atualizar();

  float setpointC() const;
  void definirSetpoint(float c);

  bool consumirEventoRotacao();
  bool consumirEventoClique();
  bool consumirEventoDuploClique();

private:
  volatile int _delta;
  volatile bool _cliquePendente;
  volatile bool _duploCliquePendente;
  bool _rotacaoPendente;
  bool _aguardandoPossivelDuplo;
  float _setpoint;
  unsigned long _ultimoRotacaoMs;
  unsigned long _ultimoCliqueMs;
  unsigned long _millisUltimoSoltarBotao;
  bool _botaoEstavaPressionado;
  unsigned long _millisBotaoPressionado;
  bool _giroComBotaoPressionado;

  static void isrEncoder();
};

#endif // ENCODER_ROTATIVO_H
