/**
 * x9c_exemplo.cpp — Implementacao do driver de exemplo X9C104S
 */

#include "x9c_exemplo.h"

void X9CExemplo::definirIncIdle(bool alto) {
  digitalWrite(PIN_X9C_CS, HIGH);
  digitalWrite(PIN_X9C_INC, alto ? HIGH : LOW);
  _incAlto = alto;
  delayMicroseconds(X9C_PULSO_US);
}

void X9CExemplo::prepararTapSubida() {
  if (_incAlto) {
    definirIncIdle(false);
  }
}

void X9CExemplo::enviarTap(bool subir) {
  prepararTapSubida();
  digitalWrite(PIN_X9C_CS, LOW);
#if X9C_INVERTE_DIRECAO
  digitalWrite(PIN_X9C_UD, subir ? LOW : HIGH);
#else
  digitalWrite(PIN_X9C_UD, subir ? HIGH : LOW);
#endif

#if X9C_PULSO_SO_SUBIDA
  digitalWrite(PIN_X9C_INC, HIGH);
  _incAlto = true;
  delayMicroseconds(X9C_PULSO_US);
#else
  digitalWrite(PIN_X9C_INC, HIGH);
  _incAlto = true;
  delayMicroseconds(X9C_PULSO_US);
  digitalWrite(PIN_X9C_INC, LOW);
  _incAlto = false;
  delayMicroseconds(X9C_PULSO_US);
#endif

  digitalWrite(PIN_X9C_CS, HIGH);
}

void X9CExemplo::aplicarPulsosExtraMaximo() {
#if X9C_PULSOS_EXTRA_MAX > 0
  for (int i = 0; i < X9C_PULSOS_EXTRA_MAX; i++) {
    enviarTap(true);
  }
#endif
}

void X9CExemplo::aplicarPulsosExtraMinimo() {
#if X9C_PULSOS_EXTRA_MIN > 0
  for (int i = 0; i < X9C_PULSOS_EXTRA_MIN; i++) {
    enviarTap(false);
  }
#endif
}

void X9CExemplo::ancorarMinimo() {
  aplicarPulsosExtraMinimo();
  _ancoradoMin = true;
  _ancoradoMax = false;
}

void X9CExemplo::ancorarMaximo() {
  aplicarPulsosExtraMaximo();
  _ancoradoMax = true;
  _ancoradoMin = false;
}

void X9CExemplo::finalizarPosicao(uint8_t alvo) {
  if (alvo == 0) {
    ancorarMinimo();
  } else if (alvo == X9C_PASSOS_MAX) {
    ancorarMaximo();
  } else {
    _ancoradoMin = false;
    _ancoradoMax = false;
  }
}

/**
 * Tap para cima que pode consumir a saida do minimo (1o + no zero nao avanca passo).
 * Retorna true se o passo firmware incrementou.
 */
bool X9CExemplo::tapSubirContado() {
  if (_ancoradoMin) {
    enviarTap(true);
    _ancoradoMin = false;
    return false;
  }

  enviarTap(true);
  if (_passo < X9C_PASSOS_MAX) {
    _passo++;
  }
  _ancoradoMax = false;
  return true;
}

/**
 * Tap para baixo que pode consumir a saida do maximo (1o - no 99 nao baixa passo).
 * Retorna true se o passo firmware decrementou.
 */
bool X9CExemplo::tapDescerContado() {
  if (_ancoradoMax) {
    enviarTap(false);
    _ancoradoMax = false;
    return false;
  }

  enviarTap(false);
  if (_passo > 0) {
    _passo--;
  }
  _ancoradoMin = false;
  return true;
}

void X9CExemplo::iniciar() {
  pinMode(PIN_X9C_CS, OUTPUT);
  pinMode(PIN_X9C_INC, OUTPUT);
  pinMode(PIN_X9C_UD, OUTPUT);
  digitalWrite(PIN_X9C_CS, HIGH);
  digitalWrite(PIN_X9C_UD, LOW);
  _passo = 0;
  _incAlto = false;
  _ancoradoMin = false;
  _ancoradoMax = false;
  definirIncIdle(false);
}

void X9CExemplo::reiniciarMinimo() {
  for (int i = 0; i <= X9C_PASSOS_MAX; i++) {
    enviarTap(false);
  }
  _passo = 0;
  definirIncIdle(false);
  ancorarMinimo();
}

void X9CExemplo::reiniciarParaMaximo() {
  irParaPasso(X9C_PASSOS_MAX);
}

float X9CExemplo::resistenciaEstimadaKohm() const {
  return (float)_passo * X9C_KOHM_POR_PASSO;
}

uint8_t X9CExemplo::passoDeKohm(float kohm) const {
  if (kohm < 0.0f) {
    kohm = 0.0f;
  }
  if (kohm > X9C_MAX_KOHM_VL_VW) {
    kohm = X9C_MAX_KOHM_VL_VW;
  }
  uint8_t p = (uint8_t)(kohm / X9C_KOHM_POR_PASSO + 0.5f);
  if (p > X9C_PASSOS_MAX) {
    p = X9C_PASSOS_MAX;
  }
  return p;
}

void X9CExemplo::irParaPasso(uint8_t alvo) {
  if (alvo > X9C_PASSOS_MAX) {
    alvo = X9C_PASSOS_MAX;
  }

  if (alvo == _passo) {
    if (alvo == 0 && !_ancoradoMin) {
      finalizarPosicao(0);
    } else if (alvo == X9C_PASSOS_MAX && !_ancoradoMax) {
      finalizarPosicao(X9C_PASSOS_MAX);
    }
    return;
  }

  while (_passo < alvo) {
    tapSubirContado();
  }
  while (_passo > alvo) {
    tapDescerContado();
  }

  finalizarPosicao(alvo);
}

void X9CExemplo::incrementarPassos(int delta) {
  if (delta == 0) {
    return;
  }

  if (delta > 0) {
    for (int i = 0; i < delta; i++) {
      tapSubirContado();
    }
  } else {
    for (int i = 0; i < -delta; i++) {
      tapDescerContado();
    }
  }
}

void X9CExemplo::ajustarKohm(float kohm) {
  irParaPasso(passoDeKohm(kohm));
}

uint8_t X9CExemplo::passoAtual() const {
  return _passo;
}

bool X9CExemplo::incEmNivelAlto() const {
  return _incAlto;
}

bool X9CExemplo::ancoradoNoMinimo() const {
  return _ancoradoMin;
}

bool X9CExemplo::ancoradoNoMaximo() const {
  return _ancoradoMax;
}
