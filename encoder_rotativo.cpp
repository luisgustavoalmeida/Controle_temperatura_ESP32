/**
 * encoder_rotativo.cpp — Quadratura em ISR + debounce do botão
 *
 * Setpoint em ALVO_TEMP_* (config.h). Clique longo reinicia PID no firmware principal.
 */

#include "encoder_rotativo.h"
#include "config.h"

static volatile int8_t ultimoClk = 1;
static EncoderRotativo* instancia = nullptr;

void EncoderRotativo::isrEncoder() {
  if (!instancia) {
    return;
  }

  int8_t clk = digitalRead(PINO_ENCODER_CLK);
  int8_t dt = digitalRead(PINO_ENCODER_DT);

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
  pinMode(PINO_ENCODER_CLK, INPUT_PULLUP);
  pinMode(PINO_ENCODER_DT, INPUT_PULLUP);
  pinMode(PINO_ENCODER_BOTAO, INPUT_PULLUP);

  _setpoint = ALVO_TEMP_PADRAO_C;
  _delta = 0;
  _cliquePendente = false;
  _duploCliquePendente = false;
  _cliqueLongoPendente = false;
  _passosGiroPendentes = 0;
  _aguardandoPossivelDuplo = false;
  _ultimoRotacaoMs = 0;
  _ultimoCliqueMs = 0;
  _millisUltimoSoltarBotao = 0;
  _botaoEstavaPressionado = false;
  _millisBotaoPressionado = 0;
  _giroComBotaoPressionado = false;
  _acumuladorDetente = 0;

  ultimoClk = digitalRead(PINO_ENCODER_CLK);
  instancia = this;
  attachInterrupt(digitalPinToInterrupt(PINO_ENCODER_CLK), isrEncoder, CHANGE);
}

void EncoderRotativo::definirSetpoint(float c) {
  if (c < ALVO_TEMP_MIN_C) {
    c = ALVO_TEMP_MIN_C;
  }
  if (c > ALVO_TEMP_MAX_C) {
    c = ALVO_TEMP_MAX_C;
  }
  _setpoint = c;
}

float EncoderRotativo::setpointC() const {
  return _setpoint;
}

void EncoderRotativo::atualizar() {
  unsigned long agora = millis();
  bool botao = (digitalRead(PINO_ENCODER_BOTAO) == LOW);

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

  noInterrupts();
  int d = _delta;
  _delta = 0;
  interrupts();

  if (d == 0) {
    return;
  }

  _acumuladorDetente += d;
  int passos = 0;
  while (_acumuladorDetente >= ENCODER_CONTAGENS_POR_DETENTE) {
    passos++;
    _acumuladorDetente -= ENCODER_CONTAGENS_POR_DETENTE;
  }
  while (_acumuladorDetente <= -ENCODER_CONTAGENS_POR_DETENTE) {
    passos--;
    _acumuladorDetente += ENCODER_CONTAGENS_POR_DETENTE;
  }
  if (passos == 0) {
    return;
  }

  if (botao) {
    _giroComBotaoPressionado = true;
    _aguardandoPossivelDuplo = false;
  }

  float passo = botao ? ALVO_TEMP_PASSO_FINO_C : ALVO_TEMP_PASSO_C;
  int passosAplicados = 0;
  int dir = (passos > 0) ? 1 : -1;
  int quantidade = (passos > 0) ? passos : -passos;
  for (int i = 0; i < quantidade; i++) {
    float antes = _setpoint;
    definirSetpoint(_setpoint + (float)dir * passo);
    if (fabsf(_setpoint - antes) < 0.0001f) {
      break;
    }
    passosAplicados++;
  }
  if (passosAplicados == 0) {
    return;
  }
  _passosGiroPendentes += passosAplicados;
}

bool EncoderRotativo::consumirEventoRotacao() {
  if (_passosGiroPendentes <= 0) {
    return false;
  }
  _passosGiroPendentes--;
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
