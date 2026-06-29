/**
 * atuador_potenciometro.cpp — Camada PID → SPI para TPL0501
 */

#include "atuador_potenciometro.h"
#include "potenciometro_map.h"

AtuadorPotenciometro::AtuadorPotenciometro()
    : _chipA(PINO_POT_CS_A),
#if POT_USA_DOIS_CHIPS
      _chipB(PINO_POT_CS_B),
#endif
      _alvoNormalizado(0.0f),
      _alvoPassoA(0),
      _alvoPassoB(0) {}

void AtuadorPotenciometro::iniciar() {
  TPL0501::iniciarBarramentoCompartilhado();
  _chipA.iniciar();
#if POT_USA_DOIS_CHIPS
  _chipB.iniciar();
#endif
  _alvoNormalizado = 0.0f;
  _alvoPassoA = 0;
  _alvoPassoB = 0;
}

void AtuadorPotenciometro::reiniciarParaMinimo() {
  _chipA.reiniciarParaMinimo();
#if POT_USA_DOIS_CHIPS
  _chipB.reiniciarParaMinimo();
#endif
}

uint8_t AtuadorPotenciometro::passoAtualA() const {
  return _chipA.passoAtual();
}

uint8_t AtuadorPotenciometro::passoAtualB() const {
#if POT_USA_DOIS_CHIPS
  return _chipB.passoAtual();
#else
  return 0;
#endif
}

float AtuadorPotenciometro::potenciaAtualPercentual() const {
  return passoParaPotenciaCombinadaPercentual(passoAtualA(), passoAtualB());
}

float AtuadorPotenciometro::potenciaAlvoPercentual() const {
  return _alvoNormalizado * 100.0f;
}

uint8_t AtuadorPotenciometro::passoAlvoA() const {
  return _alvoPassoA;
}

uint8_t AtuadorPotenciometro::passoAlvoB() const {
  return _alvoPassoB;
}

void AtuadorPotenciometro::definirPassosAlvoRapido(uint8_t alvoA, uint8_t alvoB) {
  if (alvoA > POT_PASSOS_MAX) {
    alvoA = POT_PASSOS_MAX;
  }
  if (alvoB > POT_PASSOS_MAX) {
    alvoB = POT_PASSOS_MAX;
  }
  _chipA.definirPassoAlvo(alvoA);
#if POT_USA_DOIS_CHIPS
  _chipB.definirPassoAlvo(alvoB);
#endif
  _alvoPassoA = alvoA;
  _alvoPassoB = alvoB;
}

void AtuadorPotenciometro::definirPassosAlvo(uint8_t alvoA, uint8_t alvoB) {
  definirPassosAlvoRapido(alvoA, alvoB);
}

void AtuadorPotenciometro::definirSaidaNormalizadaRapida(float potencia01) {
  if (potencia01 < 0.0f) {
    potencia01 = 0.0f;
  } else if (potencia01 > 1.0f) {
    potencia01 = 1.0f;
  }
  _alvoNormalizado = potencia01;
  uint8_t alvoA = 0;
  uint8_t alvoB = 0;
  potenciaParaPassosDesde(potencia01, passoAtualA(), passoAtualB(), &alvoA, &alvoB);
  definirPassosAlvoRapido(alvoA, alvoB);
}

void AtuadorPotenciometro::definirSaidaNormalizada(float potencia01) {
  definirSaidaNormalizadaRapida(potencia01);
}

void AtuadorPotenciometro::definirPassoIsolado(char chip, uint8_t passo) {
  if (passo > POT_PASSOS_MAX) {
    passo = POT_PASSOS_MAX;
  }
#if !POT_USA_DOIS_CHIPS
  (void)chip;
  _chipA.definirPassoAlvo(passo);
#else
  if (chip == 'A' || chip == 'a') {
    _chipA.definirPassoAlvo(passo);
  } else {
    _chipB.definirPassoAlvo(passo);
  }
#endif
}
