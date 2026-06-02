/**
 * encoder_rotativo.cpp — Quadratura em ISR + clique e duplo clique no botao
 */

#include "encoder_rotativo.h"
#include "config.h"

static volatile int8_t ultimoClk = 1;
static EncoderRotativo* instancia = nullptr;

void EncoderRotativo::isrEncoder() {
  if (!instancia) {
    return;
  }

  int8_t clk = digitalRead(PIN_ENC_CLK);
  int8_t dt = digitalRead(PIN_ENC_DT);

  if (clk != ultimoClk) {
    if (dt != clk) {
      instancia->_delta++;
    } else {
      instancia->_delta--;
    }
    ultimoClk = clk;
  }
}

void EncoderRotativo::iniciar() {
  pinMode(PIN_ENC_CLK, INPUT_PULLUP);
  pinMode(PIN_ENC_DT, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);

  _setpoint = SETPOINT_PADRAO_C;
  _delta = 0;
  _cliquePendente = false;
  _duploCliquePendente = false;
  _cliqueLongoPendente = false;
  _rotacaoPendente = false;
  _aguardandoPossivelDuplo = false;
  _ultimoRotacaoMs = 0;
  _ultimoCliqueMs = 0;
  _millisUltimoSoltarBotao = 0;
  _botaoEstavaPressionado = false;
  _millisBotaoPressionado = 0;
  _giroComBotaoPressionado = false;

  ultimoClk = digitalRead(PIN_ENC_CLK);
  instancia = this;
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_CLK), isrEncoder, CHANGE);
}

void EncoderRotativo::definirSetpoint(float c) {
  if (c < SETPOINT_MIN_C) {
    c = SETPOINT_MIN_C;
  }
  if (c > SETPOINT_MAX_C) {
    c = SETPOINT_MAX_C;
  }
  _setpoint = c;
}

float EncoderRotativo::setpointC() const {
  return _setpoint;
}

void EncoderRotativo::atualizar() {
  unsigned long agora = millis();
  bool botao = (digitalRead(PIN_ENC_SW) == LOW);

  if (botao && !_botaoEstavaPressionado) {
    _millisBotaoPressionado = agora;
    _giroComBotaoPressionado = false;
  }

  if (!botao && _botaoEstavaPressionado) {
    unsigned long duracao = agora - _millisBotaoPressionado;
    if (!_giroComBotaoPressionado && duracao >= ENCODER_CLIQUE_MIN_MS) {
      if (duracao >= ENCODER_CLIQUE_LONGO_MS) {
        _cliqueLongoPendente = true;
        _aguardandoPossivelDuplo = false;
        _cliquePendente = false;
      } else if (duracao <= ENCODER_CLIQUE_MAX_MS) {
        if (_aguardandoPossivelDuplo &&
            (agora - _millisUltimoSoltarBotao) < ENCODER_DUPLO_CLIQUE_MS) {
          _duploCliquePendente = true;
          _aguardandoPossivelDuplo = false;
          _cliquePendente = false;
        } else {
          _aguardandoPossivelDuplo = true;
          _millisUltimoSoltarBotao = agora;
        }
      } else {
        _aguardandoPossivelDuplo = false;
      }
    } else {
      _aguardandoPossivelDuplo = false;
    }
    _giroComBotaoPressionado = false;
  }
  _botaoEstavaPressionado = botao;

  if (_aguardandoPossivelDuplo &&
      (agora - _millisUltimoSoltarBotao) >= ENCODER_DUPLO_CLIQUE_MS) {
    _cliquePendente = true;
    _aguardandoPossivelDuplo = false;
  }

  if (agora - _ultimoRotacaoMs < 5) {
    return;
  }

  noInterrupts();
  int d = _delta;
  _delta = 0;
  interrupts();

  if (d == 0) {
    return;
  }
  _ultimoRotacaoMs = agora;

  if (botao) {
    _giroComBotaoPressionado = true;
    _aguardandoPossivelDuplo = false;
  }

  float passo = botao ? SETPOINT_PASSO_FINO_C : SETPOINT_PASSO_C;
  definirSetpoint(_setpoint + (float)d * passo);
  _rotacaoPendente = true;
}

bool EncoderRotativo::consumirEventoRotacao() {
  if (!_rotacaoPendente) {
    return false;
  }
  _rotacaoPendente = false;
  return true;
}

bool EncoderRotativo::consumirEventoClique() {
  if (!_cliquePendente) {
    return false;
  }
  if (millis() - _ultimoCliqueMs < 200) {
    return false;
  }
  _cliquePendente = false;
  _ultimoCliqueMs = millis();
  return true;
}

bool EncoderRotativo::consumirEventoDuploClique() {
  if (!_duploCliquePendente) {
    return false;
  }
  _duploCliquePendente = false;
  _aguardandoPossivelDuplo = false;
  _ultimoCliqueMs = millis();
  return true;
}

bool EncoderRotativo::consumirEventoCliqueLongo() {
  if (!_cliqueLongoPendente) {
    return false;
  }
  _cliqueLongoPendente = false;
  _aguardandoPossivelDuplo = false;
  _ultimoCliqueMs = millis();
  return true;
}
