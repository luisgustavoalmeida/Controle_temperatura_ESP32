/**
 * tpl0501.h — Driver SPI do TPL0501-100 (100 kΩ, 256 taps)
 *
 * Vários chips compartilham SCLK e MOSI; cada instância usa CS próprio.
 * A posição do wiper é escrita no registrador WR — não há deriva por pulsos.
 */

#ifndef TPL0501_H
#define TPL0501_H

#include <Arduino.h>
#include "config.h"

class TPL0501 {
public:
  explicit TPL0501(uint8_t pinoCs = PINO_POT_CS_A);

  /** Inicializa SPI e pinos CS (idempotente; compartilhado entre chips). */
  static void iniciarBarramentoCompartilhado();

  /** Zera wiper em RAM e no chip (passo 0). */
  void iniciar();

  /** Escreve passo 0..255 no registrador WR via SPI. */
  void definirPassoAlvo(uint8_t alvo);

  /** OUT 0..1 → passo via potenciometro_map (modo 1 chip). */
  void definirSaidaNormalizada(float potencia01);

  float saidaNormalizadaAlvo() const { return _alvoNormalizado; }

  /** Passo 0 — potência máxima na escala do chuveiro. */
  void reiniciarParaMinimo();

  /** Passo 255 — potência mínima na escala do chuveiro. */
  void reiniciarParaMaximo();

  uint8_t passoAtual() const;

private:
  uint8_t _pinoCs;
  uint8_t _passoAtual;
  float _alvoNormalizado;
  static bool _barramentoIniciado;

  uint8_t passoParaRegistro(uint8_t passo) const;
  void escreverPasso(uint8_t passo);
};

#endif // TPL0501_H
