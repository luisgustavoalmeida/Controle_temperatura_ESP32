/**
 * tpl0501.cpp — Protocolo SPI do TPL0501-100
 *
 * 8 bits MSB-first no DIN; CS alto latcha o valor no registrador WR.
 * Registro 0x00 = wiper em L (R mínima), 0xFF = wiper em H (R máxima).
 */

#include "tpl0501.h"
#include "config.h"
#include "potenciometro_map.h"
#include <SPI.h>

bool TPL0501::_barramentoIniciado = false;

static SPISettings spiPot(POT_SPI_FREQUENCIA_HZ, MSBFIRST, SPI_MODE0);

TPL0501::TPL0501(uint8_t pinoCs)
    : _pinoCs(pinoCs), _passoAtual(0), _alvoNormalizado(0.0f) {}

void TPL0501::iniciarBarramentoCompartilhado() {
  if (_barramentoIniciado) {
    return;
  }
  pinMode(PINO_POT_CS_A, OUTPUT);
  digitalWrite(PINO_POT_CS_A, HIGH);
#if POT_USA_DOIS_CHIPS
  pinMode(PINO_POT_CS_B, OUTPUT);
  digitalWrite(PINO_POT_CS_B, HIGH);
#endif
  SPI.begin(PINO_POT_SCLK, -1, PINO_POT_MOSI);
  _barramentoIniciado = true;
}

uint8_t TPL0501::passoParaRegistro(uint8_t passo) const {
#if POT_INVERTE_SENTIDO
  return static_cast<uint8_t>(POT_PASSOS_MAX - passo);
#else
  return passo;
#endif
}

void TPL0501::escreverPasso(uint8_t passo) {
  if (passo > POT_PASSOS_MAX) {
    passo = POT_PASSOS_MAX;
  }
  uint8_t reg = passoParaRegistro(passo);
  SPI.beginTransaction(spiPot);
  digitalWrite(_pinoCs, LOW);
  SPI.transfer(reg);
  digitalWrite(_pinoCs, HIGH);
  SPI.endTransaction();
}

void TPL0501::iniciar() {
  iniciarBarramentoCompartilhado();
  pinMode(_pinoCs, OUTPUT);
  digitalWrite(_pinoCs, HIGH);
  _passoAtual = 0;
  _alvoNormalizado = 0.0f;
  escreverPasso(0);
}

void TPL0501::definirSaidaNormalizada(float potencia01) {
  if (potencia01 < 0.0f) {
    potencia01 = 0.0f;
  } else if (potencia01 > 1.0f) {
    potencia01 = 1.0f;
  }
  _alvoNormalizado = potencia01;
  definirPassoAlvo(potenciaParaPasso(potencia01));
}

void TPL0501::definirPassoAlvo(uint8_t alvo) {
  if (alvo > POT_PASSOS_MAX) {
    alvo = POT_PASSOS_MAX;
  }
  if (alvo == _passoAtual) {
    return;
  }
  _passoAtual = alvo;
  escreverPasso(alvo);
}

void TPL0501::reiniciarParaMinimo() {
  _passoAtual = 0;
  escreverPasso(0);
}

void TPL0501::reiniciarParaMaximo() {
  definirPassoAlvo(POT_PASSOS_MAX);
}

uint8_t TPL0501::passoAtual() const {
  return _passoAtual;
}
