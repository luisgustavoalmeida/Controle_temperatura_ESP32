/**
 * pid_controller.cpp — Implementação do PID
 *
 * Erro = setpoint - medida (queremos aquecer se medida < setpoint).
 * A saída alta aumenta potência via X9C104S.
 */

#include "pid_controller.h"
#include <math.h>

ControladorPID::ControladorPID()
    : _kp(PID_KP),
      _ki(PID_KI),
      _kd(PID_KD),
      _saidaMin(PID_SAIDA_MIN),
      _saidaMax(PID_SAIDA_MAX),
      _integral(0.0f),
      _integralMax(0.0f),
      _ultimoErro(0.0f),
      _ultimoTermoP(0.0f),
      _ultimoTermoI(0.0f),
      _ultimoTermoD(0.0f),
      _ultimoTempo(0.0f),
      _primeiroPasso(true) {
  recalcularIntegralMax();
}

void ControladorPID::reiniciar() {
  _integral = 0.0f;
  _ultimoErro = 0.0f;
  _ultimoTempo = 0.0f;
  _primeiroPasso = true;
}

void ControladorPID::definirLimites(float saidaMinima, float saidaMaxima) {
  _saidaMin = saidaMinima;
  _saidaMax = saidaMaxima;
  recalcularIntegralMax();
}

void ControladorPID::recalcularIntegralMax() {
  float ki = (_ki > 1e-9f) ? _ki : 1e-9f;
  _integralMax = (_saidaMax - _saidaMin) / ki;
}

float ControladorPID::limitar(float valor, float minimo, float maximo) {
  if (valor < minimo) return minimo;
  if (valor > maximo) return maximo;
  return valor;
}

float ControladorPID::passo(float valorDesejado, float valorMedido, float tempoAtualS) {
  float erro = valorDesejado - valorMedido;

  // Intervalo entre passos (segundos) — no primeiro passo usa valor mínimo
  float deltaT = 1e-6f;
  if (!_primeiroPasso) {
    deltaT = tempoAtualS - _ultimoTempo;
    if (deltaT <= 0.0f) {
      deltaT = 1e-6f;
    }
  }

  // --- Termos P, I, D ---
  float termoP = _kp * erro;

  _integral += erro * deltaT;
  _integral = limitar(_integral, -_integralMax, _integralMax);
  float termoI = _ki * _integral;

  float termoD = 0.0f;
  if (!_primeiroPasso) {
    float derivadaErro = (erro - _ultimoErro) / deltaT;
    termoD = _kd * derivadaErro;
  }

  _ultimoErro = erro;
  _ultimoTermoP = termoP;
  _ultimoTermoI = termoI;
  _ultimoTermoD = termoD;
  _ultimoTempo = tempoAtualS;
  _primeiroPasso = false;

  float acaoBruta = termoP + termoI + termoD;
  float acaoLimitada = limitar(acaoBruta, _saidaMin, _saidaMax);

  // Se saturou, corrige a integral (anti-windup back-calculation)
  if (fabsf(acaoBruta - acaoLimitada) > 1e-9f) {
    float ki = (_ki > 1e-9f) ? _ki : 1e-9f;
    _integral -= (acaoBruta - acaoLimitada) / ki;
    _integral = limitar(_integral, -_integralMax, _integralMax);
  }

  return acaoLimitada;
}
