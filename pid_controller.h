/**
 * pid_controller.h — Controlador PID para temperatura
 *
 * Entrada: setpoint e temperatura medida [°C]
 * Saída: potência normalizada entre PID_SAIDA_MIN e PID_SAIDA_MAX (0..1)
 *
 * Portado de: Malha_PID_temperatura/src/pid_controller.py
 * Inclui limite na integral e anti-windup por back-calculation.
 */

#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include "config.h"

class ControladorPID {
public:
  ControladorPID();

  /** Zera integral e histórico (use após falha de sensor ou botão encoder) */
  void reiniciar();

  /**
   * Um passo do controlador.
   * @param valorDesejado  setpoint [°C]
   * @param valorMedido    temperatura filtrada [°C]
   * @param tempoAtualS    tempo monotônico em segundos (ex.: millis()/1000)
   * @return saída limitada 0..1
   */
  float passo(float valorDesejado, float valorMedido, float tempoAtualS);

  float ultimoErro() const { return _ultimoErro; }
  float ultimoTermoP() const { return _ultimoTermoP; }
  float ultimoTermoI() const { return _ultimoTermoI; }
  float ultimoTermoD() const { return _ultimoTermoD; }

  void definirLimites(float saidaMinima, float saidaMaxima);

private:
  float _kp, _ki, _kd;
  float _saidaMin, _saidaMax;
  float _integral;
  float _integralMax;   // teto da integral (anti-windup)
  float _ultimoErro;
  float _ultimoTermoP;
  float _ultimoTermoI;
  float _ultimoTermoD;
  float _ultimoTempo;
  bool _primeiroPasso;

  void recalcularIntegralMax();
  static float limitar(float valor, float minimo, float maximo);
};

#endif // PID_CONTROLLER_H
