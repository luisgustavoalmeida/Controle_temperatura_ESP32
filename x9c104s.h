/**
 * x9c104s.h — Potenciômetro digital X9C104S (100 kΩ, 100 taps)
 *
 * Pinos do CI controlados pelo ESP32:
 *   CS  — LOW habilita o chip durante pulsos
 *   U/D — direção do cursor (subir = mais resistência VL-VW na SUBIDA aferida)
 *   INC — cada pulso move um passo (modo confirmado: só borda de subida)
 *
 * O firmware mantém _wiperAtual (0..99) em RAM; pulsos extras nos extremos
 * compensam o máximo físico (~91,8 kΩ) — ver config.h X9C_PULSOS_EXTRA_*.
 */

#ifndef X9C104S_H
#define X9C104S_H

#include <Arduino.h>

class X9C104S {
public:
  void iniciar();
  /** Move o cursor até o passo desejado (0 = mín potência, 99 = máx) */
  void definirPassoAlvo(uint8_t alvo);
  /** Comando 0..1 do PID — mapeado linearmente para passo 0..99 */
  void definirSaidaNormalizada(float potencia01);
  float saidaNormalizadaAlvo() const { return _alvoNormalizado; }
  void reiniciarParaMinimo();
  void reiniciarParaMaximo();
  uint8_t passoAtual() const;
  bool incEmNivelAlto() const;

private:
  uint8_t _wiperAtual;
  float _alvoNormalizado;
  bool _incAlto;
  bool _ancoradoMin;   // já aplicou pulsos extras no zero
  bool _ancoradoMax;   // já aplicou pulsos extras no máximo

  void definirIncIdle(bool alto);
  void prepararTap();
  void pulsoInc();
  void moverUmPasso(bool subir);
  void aplicarPulsosExtrasMax();
  void aplicarPulsosExtrasMin();
  void ancorarMinimo();
  void ancorarMaximo();
  void finalizarPosicao(uint8_t alvo);
  bool tapSubirContado();
  bool tapDescerContado();
};

#endif // X9C104S_H
