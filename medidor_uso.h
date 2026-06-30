/**
 * medidor_uso.h — Cronômetro de uso e energia integrada
 *
 * Integra P(t) com 100 % = POTENCIA_MAX_WATTS (config.h).
 * Reinicia ao ligar o controle; mantém valores exibidos ao desligar.
 */

#ifndef MEDIDOR_USO_H
#define MEDIDOR_USO_H

#include <Arduino.h>

class MedidorUso {
public:
  void iniciar();
  void reiniciar(unsigned long agoraMs);
  void atualizar(float potencia01, bool contagemAtiva, unsigned long agoraMs);

  uint32_t tempoSegundos() const;
  float energiaWh() const;

private:
  float _energiaWh;
  uint32_t _tempoAcumuladoMs;
  unsigned long _ultimoTickMs;
  float _potenciaAnterior;
  bool _contando;

  void integrarIntervalo(float potencia01, unsigned long agoraMs);
};

#endif // MEDIDOR_USO_H
