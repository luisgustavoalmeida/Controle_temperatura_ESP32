/**
 * potenciometro_map.cpp — Relação linear potência ↔ passo
 * Equivalente ao mapeamento em Malha_PID_temperatura/potenciometro.py
 */

#include "potenciometro_map.h"
#include "config.h"

static float limitarZeroUm(float u) {
  if (u < 0.0f) return 0.0f;
  if (u > 1.0f) return 1.0f;
  return u;
}

uint8_t potenciaParaPasso(float sinalControle) {
  float u = limitarZeroUm(sinalControle);
  // Linear: OUT 0..1 -> passo 0..99 (sem zona morta na malha)
  int passo = (int)roundf(u * (float)X9C_PASSOS_MAX);
  if (passo < 0) passo = 0;
  if (passo > X9C_PASSOS_MAX) passo = X9C_PASSOS_MAX;
  return (uint8_t)passo;
}

float passoParaPotencia(uint8_t passo) {
  if (passo > X9C_PASSOS_MAX) {
    passo = X9C_PASSOS_MAX;
  }
  return (float)passo / (float)X9C_PASSOS_MAX;
}
