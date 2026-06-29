/**
 * potenciometro_map.h — Potência coerente 0..1 ↔ passos TPL0501 [kΩ]
 *
 * Suporta (via MODO_POT_REDE em config.h):
 *   • 1 chip — Req = R_pot
 *   • 1 chip + paralelo — Req = R_pot || R_par
 *   • 2 chips em série — Req = R_potA + R_potB (escada intercalada A↔B)
 *   • 2 chips em série + paralelo — Req = (R_A + R_B) || R_par
 *
 * Escala de potência (chuveiro em série):
 *   100 % = Req mínima (passos 0)
 *   0 %   = Req de referência (REQ_IDEAL_POTENCIA_MIN_KOHM ou física máx.)
 */

#ifndef POTENCIOMETRO_MAP_H
#define POTENCIOMETRO_MAP_H

#include <Arduino.h>

/** true se MODO_POT_REDE usa dois TPL0501 em série */
bool potenciometroUsaDoisChips();

/** true se há resistor fixo em paralelo na saída */
bool reqParaleloEstaAtivo();

/** Nome legível do modo configurado (Serial / debug) */
const char* potenciometroModoRedeNome();

/** PID OUT 1=máx potência, 0=mín → passo do chip A (modo único: único passo) */
uint8_t potenciaParaPasso(float sinalControle);

/** PID OUT → passos A e B (modo duplo: escada intercalada A↔B, 511 níveis) */
void potenciaParaPassos(float sinalControle, uint8_t* passoA, uint8_t* passoB);

/** Como potenciaParaPassos; desempata pelo incremento virtual mais próximo da posição atual */
void potenciaParaPassosDesde(float sinalControle, uint8_t passoAtualA, uint8_t passoAtualB,
                             uint8_t* passoA, uint8_t* passoB);

/** Potência coerente 0..1 a partir do passo (modo único) */
float passoParaPotencia(uint8_t passo);

/** Potência coerente 0..1 a partir dos passos A e B (modo duplo ou único com passoB=0) */
float passoParaPotenciaCombinada(uint8_t passoA, uint8_t passoB);

float passoParaPotenciaPercentual(uint8_t passo);
float passoParaPotenciaCombinadaPercentual(uint8_t passoA, uint8_t passoB);
float potenciaMinimaAlcancavelPercentual();

float rpotEstimadaKohm(uint8_t passo);
float rpotEstimadaKohmChipB(uint8_t passo);
float rpotSerieEstimadaKohm(uint8_t passoA, uint8_t passoB);
float rpotSerieMaximaKohm();

float reqEquivKohmDePasso(uint8_t passo);
float reqEquivKohmDePassos(uint8_t passoA, uint8_t passoB);
uint8_t passoDeReqEquivKohm(float reqKohm);
void passosDeReqEquivKohm(float reqKohm, uint8_t* passoA, uint8_t* passoB);

float reqIdealMinPotenciaKohm();
float reqFisicoMaxPotenciaKohm();
float reqFisicoMinPotenciaKohm();
float reqMaxFisicoKohm();

/** Req alvo [kΩ] na saída para OUT 0..1 (métrica do slew Req-constante) */
float reqAlvoKohmDePotencia(float sinalControle);

#endif // POTENCIOMETRO_MAP_H
