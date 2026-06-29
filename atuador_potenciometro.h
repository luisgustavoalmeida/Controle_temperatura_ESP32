/**
 * atuador_potenciometro.h — Atuador 1× ou 2× TPL0501
 *
 * Traduz OUT 0..1 do PID em passos via potenciometro_map e aplica via SPI.
 * Modo duplo: escada intercalada chip A ↔ chip B (511 níveis).
 */

#ifndef ATUADOR_POTENCIOMETRO_H
#define ATUADOR_POTENCIOMETRO_H

#include <Arduino.h>
#include "config.h"
#include "tpl0501.h"

class AtuadorPotenciometro {
public:
  AtuadorPotenciometro();
  void iniciar();

  /** OUT 0..1 → passos (salto SPI imediato). */
  void definirSaidaNormalizadaRapida(float potencia01);
  /** Alias de definirSaidaNormalizadaRapida (teste_tpl0501). */
  void definirSaidaNormalizada(float potencia01);

  /** Passos absolutos A e B (salto SPI). */
  void definirPassosAlvo(uint8_t alvoA, uint8_t alvoB);
  void definirPassosAlvoRapido(uint8_t alvoA, uint8_t alvoB);

  /** Move só um chip — o outro permanece (aferição no teste_tpl0501). */
  void definirPassoIsolado(char chip, uint8_t passo);

  /** Passo 0 em ambos os chips. */
  void reiniciarParaMinimo();

  float saidaNormalizadaAlvo() const { return _alvoNormalizado; }
  uint8_t passoAtualA() const;
  uint8_t passoAtualB() const;
  uint8_t passoAtual() const { return passoAtualA(); }
  float potenciaAtualPercentual() const;
  float potenciaAlvoPercentual() const;
  uint8_t passoAlvoA() const;
  uint8_t passoAlvoB() const;

private:
  TPL0501 _chipA;
#if POT_USA_DOIS_CHIPS
  TPL0501 _chipB;
#endif
  float _alvoNormalizado;
  uint8_t _alvoPassoA;
  uint8_t _alvoPassoB;
};

#endif // ATUADOR_POTENCIOMETRO_H
