/**
 * atuador_dimmer.h — Atuador RobotDyn AC Light Dimmer (rbdimmerESP32)
 *
 * Traduz OUT 0..1 do PID em nível 0..100 % (corte de fase TRIAC).
 * Pinos: PINO_DIMMER_PSM (disparo) e PINO_DIMMER_ZC (zero-cross) em config.h.
 */

#ifndef ATUADOR_DIMMER_H
#define ATUADOR_DIMMER_H

#include <Arduino.h>
#include "config.h"

class AtuadorDimmer {
public:
  AtuadorDimmer();

  void iniciar();

  /** OUT 0..1 = potencia desejada (linear se calibracao ativa) ou nivel direto. */
  void definirSaidaNormalizadaRapida(float potencia01);

  /** Potência máxima (100 %) — boot com malha já ativa. */
  void definirPotenciaMaxima();

  uint8_t nivelAtual() const;
  /** Potencia estimada na carga [%]; com calibracao = tabela, senao = nivel. */
  float potenciaAtualPercentual() const;
  /** Potencia desejada (alvo) [%]. */
  float potenciaAlvoPercentual() const;
  /** Comando enviado ao dimmer antes do corte DIMMER_NIVEL_MAX [%]. */
  float comandoDimmerPercentual() const;
  bool usaCalibracaoPotenciaLinear() const;

  /** 0=LINEAR, 1=RMS, 2=LOG (macro DIMMER_CURVA_TIPO em config.h / build). */
  int tipoCurvaConfig() const;
  /** Valor de rbdimmer_get_curve() no canal ativo. */
  int tipoCurvaBiblioteca() const;
  /** Atraso de disparo TRIAC [us] calculado pela biblioteca para o nivel atual. */
  uint32_t atrasoDisparoUs() const;

  /** true se houve zero-cross recente (AC presente no módulo dimmer). */
  bool redeComZeroCross() const;

private:
  float _alvoNormalizado;
  uint8_t _nivelAlvo;
  uint8_t _nivelAtual;
  bool _iniciado;

  void aplicarNivel(uint8_t nivelPct);
};

#endif // ATUADOR_DIMMER_H
