/**
 * encoder_rotativo.cpp — Quadratura e botão em ISR + debounce no loop
 *
 * Setpoint em ALVO_TEMP_* ou ALVO_POT_* (config.h).
 * Segurar 3 s sem girar troca modo; clique longo (~800 ms) reinicia PID no firmware principal.
 */

#include "encoder_rotativo.h"
#include "config.h"
#include <math.h>

static const float POT_PCT_EPS = 0.05f;

static float arredondarPotenciaDecimo(float pct) {
  return roundf(pct * 10.0f) / 10.0f;
}

/** Giro normal: ±1 na parte inteira, preservando os décimos (75,3 → 76,3). */
static float ajustarPotenciaGrosso(float atual, int dir) {
  atual = arredondarPotenciaDecimo(atual);
  float inteiro = floorf(atual + POT_PCT_EPS);
  float frac = atual - inteiro;
  if (frac < 0.0f) {
    frac = 0.0f;
  }
  return arredondarPotenciaDecimo(inteiro + (float)dir + frac);
}

/** Botão + giro: altera décimos (75,1 → 75,2). */
static float ajustarPotenciaFino(float atual, int dir) {
  return arredondarPotenciaDecimo(atual + (float)dir * ALVO_POT_PASSO_FINO_PCT);
}

static volatile int8_t ultimoClk = 1;
static EncoderRotativo* instancia = nullptr;

void IRAM_ATTR EncoderRotativo::isrEncoder() {
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

void IRAM_ATTR EncoderRotativo::isrBotao() {
  if (!instancia) {
    return;
  }
  instancia->_botaoLeituraIsr = digitalRead(PINO_ENCODER_BOTAO);
  instancia->_botaoBordaMs = millis();
  instancia->_bordaBotaoPendente = true;
}

void EncoderRotativo::iniciar() {
  pinMode(PINO_ENCODER_CLK, INPUT_PULLUP);
  pinMode(PINO_ENCODER_DT, INPUT_PULLUP);
  pinMode(PINO_ENCODER_BOTAO, INPUT_PULLUP);

  _modoAjuste = ENCODER_AJUSTE_TEMPERATURA;
  _setpoint = ALVO_TEMP_PADRAO_C;
  _alvoPotenciaPct = ALVO_POT_PADRAO_PCT;
  _delta = 0;
  _cliquePendente = false;
  _duploCliquePendente = false;
  _cliqueLongoPendente = false;
  _trocaModoPendente = false;
  _passosGiroPendentes = 0;
  _aguardandoPossivelDuplo = false;
  _ultimoRotacaoMs = 0;
  _ultimoCliqueMs = 0;
  _millisUltimoSoltarBotao = 0;
  _botaoEstavaPressionado = false;
  _millisBotaoPressionado = 0;
  _giroComBotaoPressionado = false;
  _trocaModoJaDisparou = false;
  _acumuladorDetente = 0;
  _botaoEstavelPressionado = false;
  _bordaBotaoPendente = false;
  _botaoLeituraIsr = HIGH;
  _botaoBordaMs = 0;

  ultimoClk = digitalRead(PINO_ENCODER_CLK);
  _botaoEstavelPressionado = (digitalRead(PINO_ENCODER_BOTAO) == LOW);
  _botaoEstavaPressionado = _botaoEstavelPressionado;
  instancia = this;
  attachInterrupt(digitalPinToInterrupt(PINO_ENCODER_CLK), isrEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PINO_ENCODER_BOTAO), isrBotao, CHANGE);
}

void EncoderRotativo::definirModoAjuste(ModoAjusteEncoder modo) {
  _modoAjuste = modo;
}

ModoAjusteEncoder EncoderRotativo::modoAjuste() const {
  return _modoAjuste;
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

void EncoderRotativo::definirAlvoPotenciaPercent(float pct) {
  pct = arredondarPotenciaDecimo(pct);
  if (pct < ALVO_POT_MIN_PCT) {
    pct = ALVO_POT_MIN_PCT;
  }
  if (pct > ALVO_POT_MAX_PCT) {
    pct = ALVO_POT_MAX_PCT;
  }
  _alvoPotenciaPct = pct;
}

float EncoderRotativo::alvoPotenciaPercent() const {
  return _alvoPotenciaPct;
}

void EncoderRotativo::aplicarPassoGiro(int passos, bool botaoPressionado) {
  int passosAplicados = 0;
  int dir = (passos > 0) ? 1 : -1;
  int quantidade = (passos > 0) ? passos : -passos;
  for (int i = 0; i < quantidade; i++) {
    if (_modoAjuste == ENCODER_AJUSTE_POTENCIA) {
      float antes = _alvoPotenciaPct;
      float novo = botaoPressionado
                       ? ajustarPotenciaFino(_alvoPotenciaPct, dir)
                       : ajustarPotenciaGrosso(_alvoPotenciaPct, dir);
      definirAlvoPotenciaPercent(novo);
      if (fabsf(_alvoPotenciaPct - antes) < 0.0001f) {
        break;
      }
    } else {
      float passo = botaoPressionado ? ALVO_TEMP_PASSO_FINO_C : ALVO_TEMP_PASSO_C;
      float antes = _setpoint;
      definirSetpoint(_setpoint + (float)dir * passo);
      if (fabsf(_setpoint - antes) < 0.0001f) {
        break;
      }
    }
    passosAplicados++;
  }
  if (passosAplicados > 0) {
    _passosGiroPendentes += passosAplicados;
  }
}

void EncoderRotativo::registrarSoltarBotao(unsigned long agora, unsigned long duracao) {
  if (_giroComBotaoPressionado || _trocaModoJaDisparou) {
    _aguardandoPossivelDuplo = false;
    return;
  }
  if (duracao < ENCODER_CLIQUE_MIN_MS) {
    return;
  }

  if (duracao >= ENCODER_CLIQUE_LONGO_MS && duracao < ENCODER_TROCA_MODO_MS) {
    _cliqueLongoPendente = true;
    _aguardandoPossivelDuplo = false;
    _cliquePendente = false;
    return;
  }

  if (duracao > ENCODER_CLIQUE_MAX_MS) {
    _aguardandoPossivelDuplo = false;
    return;
  }

  if (_aguardandoPossivelDuplo &&
      (agora - _millisUltimoSoltarBotao) <= ENCODER_DUPLO_CLIQUE_MS) {
    _duploCliquePendente = true;
    _aguardandoPossivelDuplo = false;
    _cliquePendente = false;
    return;
  }

  _aguardandoPossivelDuplo = true;
  _millisUltimoSoltarBotao = agora;
}

void EncoderRotativo::processarBotao(unsigned long agora) {
  if (_bordaBotaoPendente &&
      (agora - _botaoBordaMs) >= ENCODER_BOTAO_DEBOUNCE_MS) {
    _bordaBotaoPendente = false;
    bool pressionado = (_botaoLeituraIsr == LOW);
    if (pressionado != _botaoEstavelPressionado) {
      _botaoEstavelPressionado = pressionado;
    }
  } else if (!_bordaBotaoPendente) {
    bool leituraDireta = (digitalRead(PINO_ENCODER_BOTAO) == LOW);
    if (leituraDireta != _botaoEstavelPressionado) {
      _botaoEstavelPressionado = leituraDireta;
    }
  }

  bool botao = _botaoEstavelPressionado;

  if (botao && !_botaoEstavaPressionado) {
    _millisBotaoPressionado = agora;
    _giroComBotaoPressionado = false;
    _trocaModoJaDisparou = false;
  }

  if (botao && !_giroComBotaoPressionado && !_trocaModoJaDisparou) {
    unsigned long duracaoPressao = agora - _millisBotaoPressionado;
    if (duracaoPressao >= ENCODER_TROCA_MODO_MS) {
      _trocaModoPendente = true;
      _trocaModoJaDisparou = true;
      _aguardandoPossivelDuplo = false;
      _cliquePendente = false;
    }
  }

  if (!botao && _botaoEstavaPressionado) {
    registrarSoltarBotao(agora, agora - _millisBotaoPressionado);
    _giroComBotaoPressionado = false;
  }
  _botaoEstavaPressionado = botao;

  if (_aguardandoPossivelDuplo &&
      (agora - _millisUltimoSoltarBotao) > ENCODER_DUPLO_CLIQUE_MS) {
    _cliquePendente = true;
    _aguardandoPossivelDuplo = false;
  }
}

void EncoderRotativo::atualizar() {
  unsigned long agora = millis();
  processarBotao(agora);
  bool botao = _botaoEstavelPressionado;

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

  aplicarPassoGiro(passos, botao);
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

bool EncoderRotativo::consumirEventoTrocaModo() {
  if (!_trocaModoPendente) {
    return false;
  }
  _trocaModoPendente = false;
  _ultimoCliqueMs = millis();
  return true;
}
