/**
 * x9c104s.cpp — Driver do X9C104S
 *
 * Sequência de um passo:
 *   1. prepararTap() — garante INC em LOW antes do pulso (modo aferido)
 *   2. CS = LOW, U/D = direção, pulso em INC
 *   3. CS = HIGH
 *
 * Nos extremos 0 e 99 aplicamos pulsos extras (ancoragem) para atingir
 * ~0 kΩ e ~91,8 kΩ medidos na bancada.
 */

#include "x9c104s.h"
#include "config.h"
#include "potenciometro_map.h"

// --- Nível idle do pino INC (entre movimentos) ---

void X9C104S::definirIncIdle(bool alto) {
  digitalWrite(PIN_X9C_CS, HIGH);
  digitalWrite(PIN_X9C_INC, alto ? HIGH : LOW);
  _incAlto = alto;
  delayMicroseconds(X9C_PULSO_US);
}

void X9C104S::prepararTap() {
  if (_incAlto) {
    definirIncIdle(false);
  }
}

void X9C104S::pulsoInc() {
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
}

// --- Um passo físico no chip (não altera _wiperAtual sozinho) ---

void X9C104S::moverUmPasso(bool subir) {
  prepararTap();
  digitalWrite(PIN_X9C_CS, LOW);
#if X9C_INVERTE_DIRECAO
  digitalWrite(PIN_X9C_UD, subir ? LOW : HIGH);
#else
  digitalWrite(PIN_X9C_UD, subir ? HIGH : LOW);
#endif
  pulsoInc();
  digitalWrite(PIN_X9C_CS, HIGH);
}

void X9C104S::aplicarPulsosExtrasMax() {
#if X9C_PULSOS_EXTRA_MAX > 0
  for (int i = 0; i < X9C_PULSOS_EXTRA_MAX; i++) {
    moverUmPasso(true);
  }
#endif
}

void X9C104S::aplicarPulsosExtrasMin() {
#if X9C_PULSOS_EXTRA_MIN > 0
  for (int i = 0; i < X9C_PULSOS_EXTRA_MIN; i++) {
    moverUmPasso(false);
  }
#endif
}

void X9C104S::ancorarMinimo() {
  aplicarPulsosExtrasMin();
  _ancoradoMin = true;
  _ancoradoMax = false;
}

void X9C104S::ancorarMaximo() {
  aplicarPulsosExtrasMax();
  _ancoradoMax = true;
  _ancoradoMin = false;
}

void X9C104S::finalizarPosicao(uint8_t alvo) {
  if (alvo == 0) {
    ancorarMinimo();
  } else if (alvo == X9C_PASSOS_MAX) {
    ancorarMaximo();
  } else {
    _ancoradoMin = false;
    _ancoradoMax = false;
  }
}

// --- Movimento com contagem de passos 0..99 na RAM ---

bool X9C104S::tapSubirContado() {
  if (_ancoradoMin) {
    moverUmPasso(true);
    _ancoradoMin = false;
    return false;
  }

  moverUmPasso(true);
  if (_wiperAtual < X9C_PASSOS_MAX) {
    _wiperAtual++;
  }
  _ancoradoMax = false;
  return true;
}

bool X9C104S::tapDescerContado() {
  if (_ancoradoMax) {
    moverUmPasso(false);
    _ancoradoMax = false;
    return false;
  }

  moverUmPasso(false);
  if (_wiperAtual > 0) {
    _wiperAtual--;
  }
  _ancoradoMin = false;
  return true;
}

// --- API pública ---

void X9C104S::iniciar() {
  pinMode(PIN_X9C_CS, OUTPUT);
  pinMode(PIN_X9C_INC, OUTPUT);
  pinMode(PIN_X9C_UD, OUTPUT);
  digitalWrite(PIN_X9C_CS, HIGH);
  digitalWrite(PIN_X9C_UD, LOW);
  _incAlto = false;
  _ancoradoMin = false;
  _ancoradoMax = false;
  definirIncIdle(false);
  _wiperAtual = 0;
  _alvoNormalizado = 0.0f;
}

void X9C104S::definirSaidaNormalizada(float potencia01) {
  if (potencia01 < 0.0f) {
    potencia01 = 0.0f;
  } else if (potencia01 > 1.0f) {
    potencia01 = 1.0f;
  }
  _alvoNormalizado = potencia01;
  definirPassoAlvo(potenciaParaPasso(potencia01));
}

void X9C104S::definirPassoAlvo(uint8_t alvo) {
  if (alvo > X9C_PASSOS_MAX) {
    alvo = X9C_PASSOS_MAX;
  }

  if (alvo == _wiperAtual) {
    if (alvo == 0 && !_ancoradoMin) {
      finalizarPosicao(0);
    } else if (alvo == X9C_PASSOS_MAX && !_ancoradoMax) {
      finalizarPosicao(X9C_PASSOS_MAX);
    }
    return;
  }

  while (_wiperAtual < alvo) {
    tapSubirContado();
  }
  while (_wiperAtual > alvo) {
    tapDescerContado();
  }

  finalizarPosicao(alvo);
}

void X9C104S::reiniciarParaMinimo() {
  for (int i = 0; i <= X9C_PASSOS_MAX; i++) {
    moverUmPasso(false);
  }
  _wiperAtual = 0;
  definirIncIdle(false);
  ancorarMinimo();
}

void X9C104S::reiniciarParaMaximo() {
  definirPassoAlvo(X9C_PASSOS_MAX);
}

uint8_t X9C104S::passoAtual() const {
  return _wiperAtual;
}

bool X9C104S::incEmNivelAlto() const {
  return _incAlto;
}
