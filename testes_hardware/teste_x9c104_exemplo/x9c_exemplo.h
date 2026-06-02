/**
 * x9c_exemplo.h — Driver de exemplo para X9C104S
 *
 * Baseado na afericao em bancada (VL-VW pinos 6-5):
 *   - Pulso so na borda de SUBIDA (LOW->HIGH).
 *   - Passo 99 + 1 pulso extra SUBIDA -> ~91,8 kΩ (maximo fisico).
 *   - Passo 0 + 1 pulso extra DESCIDA -> minimo fisico (~0 kΩ).
 *   - Saindo do minimo: 1o + no passo 0 nao avanca contador (desancora).
 *   - Saindo do maximo: 1o - no passo 99 nao baixa contador (desancora).
 */

#ifndef X9C_EXEMPLO_H
#define X9C_EXEMPLO_H

#include <Arduino.h>
#include "../../config.h"

class X9CExemplo {
public:
  void iniciar();
  void reiniciarMinimo();
  void reiniciarParaMaximo();
  void irParaPasso(uint8_t alvo);
  void incrementarPassos(int delta);
  void ajustarKohm(float kohm);

  uint8_t passoAtual() const;
  bool incEmNivelAlto() const;
  bool ancoradoNoMinimo() const;
  bool ancoradoNoMaximo() const;
  float resistenciaEstimadaKohm() const;

private:
  uint8_t _passo;
  bool _incAlto;
  bool _ancoradoMin;
  bool _ancoradoMax;

  void definirIncIdle(bool alto);
  void prepararTapSubida();
  void enviarTap(bool subir);

  void aplicarPulsosExtraMaximo();
  void aplicarPulsosExtraMinimo();
  void ancorarMinimo();
  void ancorarMaximo();
  void finalizarPosicao(uint8_t alvo);

  /** Tap subida; retorna true se incrementou passo firmware. */
  bool tapSubirContado();
  /** Tap descida; retorna true se decrementou passo firmware. */
  bool tapDescerContado();

  uint8_t passoDeKohm(float kohm) const;
};

#endif // X9C_EXEMPLO_H
