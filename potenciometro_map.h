/**
 * potenciometro_map.h — Converte saída do PID em passo do X9C104S
 *
 * PID entrega 0,0 (mínima potência) a 1,0 (máxima).
 * O chip tem 100 posições numeradas de 0 a 99.
 */

#ifndef POTENCIOMETRO_MAP_H
#define POTENCIOMETRO_MAP_H

#include <Arduino.h>

/** Saída PID 0..1 → passo 0..99 */
uint8_t potenciaParaPasso(float sinalControle);

/** Passo 0..99 → saída 0..1 (útil para debug) */
float passoParaPotencia(uint8_t passo);

#endif // POTENCIOMETRO_MAP_H
