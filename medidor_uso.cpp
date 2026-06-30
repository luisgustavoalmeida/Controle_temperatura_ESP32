/**
 * medidor_uso.cpp — ∫ P dt; P [W] = potência01 × POTENCIA_MAX_WATTS
 */

#include "medidor_uso.h"
#include "config.h"

void MedidorUso::iniciar() {
  _energiaWh = 0.0f;
  _tempoAcumuladoMs = 0;
  _ultimoTickMs = 0;
  _potenciaAnterior = 0.0f;
  _contando = false;
}

void MedidorUso::reiniciar(unsigned long agoraMs) {
  _energiaWh = 0.0f;
  _tempoAcumuladoMs = 0;
  _ultimoTickMs = agoraMs;
  _potenciaAnterior = 0.0f;
  _contando = false;
}

void MedidorUso::integrarIntervalo(float potencia01, unsigned long agoraMs) {
  if (_ultimoTickMs == 0) {
    _ultimoTickMs = agoraMs;
    return;
  }
  unsigned long dtMs = agoraMs - _ultimoTickMs;
  if (dtMs == 0) {
    return;
  }
  _ultimoTickMs = agoraMs;
  _tempoAcumuladoMs += dtMs;

  if (potencia01 < 0.0f) {
    potencia01 = 0.0f;
  }
  if (potencia01 > 1.0f) {
    potencia01 = 1.0f;
  }
  float watts = potencia01 * POTENCIA_MAX_WATTS;
  _energiaWh += watts * ((float)dtMs / 3600000.0f);
}

void MedidorUso::atualizar(float potencia01, bool contagemAtiva, unsigned long agoraMs) {
  if (!contagemAtiva) {
    if (_contando) {
      integrarIntervalo(_potenciaAnterior, agoraMs);
      _contando = false;
    }
    return;
  }

  if (!_contando) {
    _ultimoTickMs = agoraMs;
    _potenciaAnterior = potencia01;
    _contando = true;
    return;
  }

  integrarIntervalo(_potenciaAnterior, agoraMs);
  _potenciaAnterior = potencia01;
}

uint32_t MedidorUso::tempoSegundos() const {
  return _tempoAcumuladoMs / 1000UL;
}

float MedidorUso::energiaWh() const {
  return _energiaWh;
}
