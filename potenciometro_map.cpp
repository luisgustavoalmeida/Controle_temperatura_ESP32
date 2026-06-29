/**
 * potenciometro_map.cpp — Mapa OUT 0..1 ↔ passos TPL0501
 *
 * Modo duplo — escada intercalada (n = 0..POT_PASSOS_INTERCALADOS_MAX):
 *   n=0 → A0 B0 | n=1 → A1 B0 | n=2 → A1 B1 | n=3 → A2 B1 …
 * Busca binária em R_série; desempata pelo n mais próximo da posição atual.
 *
 * Com paralelo: Req = R_serie || RESISTOR_PARALELO_KOHM.
 */

#include "potenciometro_map.h"
#include "config.h"
#include <math.h>

// ---------------------------------------------------------------------------
// Helpers internos — modo de rede
// ---------------------------------------------------------------------------

bool potenciometroUsaDoisChips() {
#if POT_USA_DOIS_CHIPS
  return true;
#else
  return false;
#endif
}

bool reqParaleloEstaAtivo() {
#if REDE_COM_RESISTOR_PARALELO
  return true;
#else
  return false;
#endif
}

const char* potenciometroModoRedeNome() {
#if MODO_POT_REDE == MODO_POT_UNICO
  return "1x TPL0501";
#elif MODO_POT_REDE == MODO_POT_UNICO_PARALELO
  return "1x TPL0501 + paralelo";
#elif MODO_POT_REDE == MODO_POT_DUPLO_SERIE
  return "2x TPL0501 serie";
#elif MODO_POT_REDE == MODO_POT_DUPLO_SERIE_PARALELO
  return "2x TPL0501 serie + paralelo";
#else
  return "desconhecido";
#endif
}

static float limitarZeroUm(float u) {
  if (u < 0.0f) return 0.0f;
  if (u > 1.0f) return 1.0f;
  return u;
}

// ---------------------------------------------------------------------------
// Resistência por chip (kΩ VL–VW)
// ---------------------------------------------------------------------------

static float kohmPorPassoChipA() {
  return POT_KOHM_POR_PASSO_A;
}

static float kohmPorPassoChipB() {
  return POT_KOHM_POR_PASSO_B;
}

static float rpotMaxChipA() {
  return POT_AFERIDO_KOHM_MAX_A;
}

static float rpotMaxChipB() {
  return POT_AFERIDO_KOHM_MAX_B;
}

static float rpotKohmDePassoChip(uint8_t passo, float kohmPorPasso, float rpotMax) {
  if (passo > POT_PASSOS_MAX) {
    passo = POT_PASSOS_MAX;
  }
  float r = (float)passo * kohmPorPasso;
  if (r > rpotMax) {
    return rpotMax;
  }
  return r;
}

static uint8_t passoDeRpotChip(float rpotKohm, float kohmPorPasso, float rpotMax) {
  if (rpotKohm <= 0.0f) {
    return 0;
  }
  if (rpotKohm > rpotMax) {
    rpotKohm = rpotMax;
  }
  int passo = (int)roundf(rpotKohm / kohmPorPasso);
  if (passo < 0) passo = 0;
  if (passo > POT_PASSOS_MAX) passo = POT_PASSOS_MAX;
  return (uint8_t)passo;
}

float rpotEstimadaKohm(uint8_t passo) {
  return rpotKohmDePassoChip(passo, kohmPorPassoChipA(), rpotMaxChipA());
}

float rpotEstimadaKohmChipB(uint8_t passo) {
  return rpotKohmDePassoChip(passo, kohmPorPassoChipB(), rpotMaxChipB());
}

float rpotSerieEstimadaKohm(uint8_t passoA, uint8_t passoB) {
  float rSerie = rpotEstimadaKohm(passoA);
  if (potenciometroUsaDoisChips()) {
    rSerie += rpotEstimadaKohmChipB(passoB);
  }
  return rSerie;
}

float rpotSerieMaximaKohm() {
  float maxSerie = rpotMaxChipA();
  if (potenciometroUsaDoisChips()) {
    maxSerie += rpotMaxChipB();
  }
  return maxSerie;
}

// ---------------------------------------------------------------------------
// Req equivalente (com ou sem paralelo)
// ---------------------------------------------------------------------------

static float reqEquivDeRserie(float rSerieKohm) {
  if (rSerieKohm <= 0.0f) {
    return 0.0f;
  }
  if (!reqParaleloEstaAtivo()) {
    return rSerieKohm;
  }
  return (RESISTOR_PARALELO_KOHM * rSerieKohm) / (RESISTOR_PARALELO_KOHM + rSerieKohm);
}

static float rserieDeReqEquiv(float reqKohm) {
  if (reqKohm <= 0.0f) {
    return 0.0f;
  }
  if (!reqParaleloEstaAtivo()) {
    float maxSerie = rpotSerieMaximaKohm();
    if (reqKohm > maxSerie) {
      return maxSerie;
    }
    return reqKohm;
  }
  if (reqKohm >= RESISTOR_PARALELO_KOHM - 0.001f) {
    return rpotSerieMaximaKohm();
  }
  return (RESISTOR_PARALELO_KOHM * reqKohm) / (RESISTOR_PARALELO_KOHM - reqKohm);
}

float reqEquivKohmDePasso(uint8_t passo) {
  return reqEquivDeRserie(rpotSerieEstimadaKohm(passo, 0));
}

float reqEquivKohmDePassos(uint8_t passoA, uint8_t passoB) {
  return reqEquivDeRserie(rpotSerieEstimadaKohm(passoA, passoB));
}

float reqMaxFisicoKohm() {
  return reqEquivDeRserie(rpotSerieMaximaKohm());
}

/** Req de referência ideal (0 % potência) — valor lido no multímetro na saída. */
float reqIdealMinPotenciaKohm() {
  float reqIdeal = REQ_IDEAL_POTENCIA_MIN_KOHM;

  if (reqParaleloEstaAtivo()) {
    if (reqIdeal >= RESISTOR_PARALELO_KOHM - 0.001f) {
      reqIdeal = RESISTOR_PARALELO_KOHM - 0.001f;
    }
  } else {
    // Sem paralelo: Req = R_série; se alguém passou R_série bruta, converte (idempotente se já for Req).
    reqIdeal = reqEquivDeRserie(reqIdeal);
  }

  float reqMaxFis = reqMaxFisicoKohm();
  if (reqIdeal > reqMaxFis) {
    return reqMaxFis;
  }
  return reqIdeal;
}

/** Req com passos no mínimo — referência de 100 % potência. */
float reqFisicoMaxPotenciaKohm() {
  return reqEquivDeRserie(0.0f);
}

/** Req no máximo físico alcançável — referência de 0 % (modo FISICA). */
float reqFisicoMinPotenciaKohm() {
  return reqMaxFisicoKohm();
}

static float reqReferenciaZeroPotenciaKohm() {
#if REF_POTENCIA_MINIMA == REF_POTENCIA_MIN_FISICA
  return reqFisicoMinPotenciaKohm();
#else
  return reqIdealMinPotenciaKohm();
#endif
}

static float limitarReqAlvoMinPotencia(float reqKohm) {
  if (REQ_MAXIMA_SAIDA_KOHM > 0.001f && reqKohm > REQ_MAXIMA_SAIDA_KOHM) {
    return REQ_MAXIMA_SAIDA_KOHM;
  }
  float reqRef0 = reqReferenciaZeroPotenciaKohm();
  if (reqKohm > reqRef0) {
    return reqRef0;
  }
  return reqKohm;
}

/** Potência coerente 0..1: u ∝ (Req_ref0 − Req) / span. */
static float potenciaDeReqKohm(float reqKohm) {
  float reqRef0 = reqReferenciaZeroPotenciaKohm();
  float req100pct = reqFisicoMaxPotenciaKohm();
  float denominador = reqRef0 - req100pct;
  if (denominador <= 0.001f) {
    return 1.0f;
  }
  return limitarZeroUm((reqRef0 - reqKohm) / denominador);
}

float reqAlvoKohmDePotencia(float sinalControle) {
  float u = limitarZeroUm(sinalControle);
  float reqRef0 = reqReferenciaZeroPotenciaKohm();
  float reqAlvo = reqRef0 * (1.0f - u);
  return limitarReqAlvoMinPotencia(reqAlvo);
}

// ---------------------------------------------------------------------------
// Potência coerente
// ---------------------------------------------------------------------------

float passoParaPotenciaCombinada(uint8_t passoA, uint8_t passoB) {
  return potenciaDeReqKohm(reqEquivKohmDePassos(passoA, passoB));
}

float passoParaPotencia(uint8_t passo) {
  return passoParaPotenciaCombinada(passo, 0);
}

float passoParaPotenciaPercentual(uint8_t passo) {
  return passoParaPotencia(passo) * 100.0f;
}

float passoParaPotenciaCombinadaPercentual(uint8_t passoA, uint8_t passoB) {
  return passoParaPotenciaCombinada(passoA, passoB) * 100.0f;
}

float potenciaMinimaAlcancavelPercentual() {
  if (potenciometroUsaDoisChips()) {
    return passoParaPotenciaCombinadaPercentual(POT_PASSOS_MAX, POT_PASSOS_MAX);
  }
  return passoParaPotenciaPercentual(POT_PASSOS_MAX);
}

// ---------------------------------------------------------------------------
// Modo duplo — escada intercalada (incremento virtual n = 0..POT_PASSOS_INTERCALADOS_MAX)
// ---------------------------------------------------------------------------

/** n → passos na escada A↔B (A sobe primeiro em cada par). */
static void passosDeIncrementoVirtual(int n, uint8_t* passoA, uint8_t* passoB) {
  if (n < 0) {
    n = 0;
  }
  if (n > POT_PASSOS_INTERCALADOS_MAX) {
    n = POT_PASSOS_INTERCALADOS_MAX;
  }
  *passoA = (uint8_t)((n + 1) / 2);
  *passoB = (uint8_t)(n / 2);
}

/** Projeta par (A,B) na escada e retorna n correspondente. */
static int incrementoVirtualDePassos(uint8_t passoA, uint8_t passoB) {
  if (passoA > passoB + 1) {
    passoA = (uint8_t)(passoB + 1);
  }
  if (passoB > passoA) {
    passoB = passoA;
  }
  if (passoA == passoB) {
    return 2 * (int)passoB;
  }
  return 2 * (int)passoB + 1;
}

static float rserieDeIncrementoVirtual(int n) {
  uint8_t a = 0;
  uint8_t b = 0;
  passosDeIncrementoVirtual(n, &a, &b);
  return rpotSerieEstimadaKohm(a, b);
}

/** R_série alvo → n (busca binária; escada é monotônica em n). */
static int incrementoVirtualDeRserie(float rSerieKohm) {
  const float maxSerie = rpotSerieMaximaKohm();
  if (rSerieKohm <= 0.0f) {
    return 0;
  }
  if (rSerieKohm >= maxSerie - 0.001f) {
    return POT_PASSOS_INTERCALADOS_MAX;
  }

  int lo = 0;
  int hi = POT_PASSOS_INTERCALADOS_MAX;
  while (lo < hi) {
    const int mid = (lo + hi) / 2;
    if (rserieDeIncrementoVirtual(mid) < rSerieKohm) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }

  if (lo > 0) {
    const float errLo = fabsf(rserieDeIncrementoVirtual(lo) - rSerieKohm);
    const float errPrev = fabsf(rserieDeIncrementoVirtual(lo - 1) - rSerieKohm);
    if (errPrev < errLo) {
      return lo - 1;
    }
  }
  return lo;
}

/** Escolhe n com menor |R_série(n) − alvo|; desempata pelo n mais próximo de nAtual. */
static int incrementoVirtualDeRserieDesde(float rSerieAlvo, int nAtual) {
  int n = incrementoVirtualDeRserie(rSerieAlvo);

  if (n > 0) {
    const float errN = fabsf(rserieDeIncrementoVirtual(n) - rSerieAlvo);
    const float errPrev = fabsf(rserieDeIncrementoVirtual(n - 1) - rSerieAlvo);
    if (fabsf(errN - errPrev) < 0.001f
        && abs(n - 1 - nAtual) < abs(n - nAtual)) {
      n = n - 1;
    }
  }
  if (n < POT_PASSOS_INTERCALADOS_MAX) {
    const float errN = fabsf(rserieDeIncrementoVirtual(n) - rSerieAlvo);
    const float errNext = fabsf(rserieDeIncrementoVirtual(n + 1) - rSerieAlvo);
    if (fabsf(errN - errNext) < 0.001f
        && abs(n + 1 - nAtual) < abs(n - nAtual)) {
      n = n + 1;
    }
  }
  return n;
}

static void passosDeRserieIntercalado(float rSerieKohm, int nAtual,
                                      uint8_t* passoA, uint8_t* passoB) {
  float maxSerie = rpotSerieMaximaKohm();
  if (rSerieKohm < 0.0f) {
    rSerieKohm = 0.0f;
  }
  if (rSerieKohm > maxSerie) {
    rSerieKohm = maxSerie;
  }
  const int n = incrementoVirtualDeRserieDesde(rSerieKohm, nAtual);
  passosDeIncrementoVirtual(n, passoA, passoB);
}

static void passosDeRserieKohm(float rSerieKohm, uint8_t passoAtualA, uint8_t passoAtualB,
                               uint8_t* passoA, uint8_t* passoB) {
  if (!potenciometroUsaDoisChips()) {
    *passoA = passoDeRpotChip(rSerieKohm, kohmPorPassoChipA(), rpotMaxChipA());
    *passoB = 0;
    return;
  }
  const int nAtual = incrementoVirtualDePassos(passoAtualA, passoAtualB);
  passosDeRserieIntercalado(rSerieKohm, nAtual, passoA, passoB);
}

void passosDeReqEquivKohm(float reqKohm, uint8_t* passoA, uint8_t* passoB) {
  if (reqKohm <= 0.0f) {
    *passoA = 0;
    *passoB = 0;
    return;
  }
  reqKohm = limitarReqAlvoMinPotencia(reqKohm);
  passosDeRserieKohm(rserieDeReqEquiv(reqKohm), 0, 0, passoA, passoB);
}

uint8_t passoDeReqEquivKohm(float reqKohm) {
  uint8_t passoA = 0;
  uint8_t passoB = 0;
  passosDeReqEquivKohm(reqKohm, &passoA, &passoB);
  return passoA;
}

void potenciaParaPassos(float sinalControle, uint8_t* passoA, uint8_t* passoB) {
  potenciaParaPassosDesde(sinalControle, 0, 0, passoA, passoB);
}

void potenciaParaPassosDesde(float sinalControle, uint8_t passoAtualA, uint8_t passoAtualB,
                             uint8_t* passoA, uint8_t* passoB) {
  float u = limitarZeroUm(sinalControle);

  if (!potenciometroUsaDoisChips()) {
    passosDeReqEquivKohm(reqAlvoKohmDePotencia(u), passoA, passoB);
    return;
  }

  if (u <= 0.0f) {
    *passoA = POT_PASSOS_MAX;
    *passoB = POT_PASSOS_MAX;
    return;
  }
  if (u >= 1.0f) {
    *passoA = 0;
    *passoB = 0;
    return;
  }

  const float reqAlvo = reqAlvoKohmDePotencia(u);
  const float rSerieAlvo = rserieDeReqEquiv(reqAlvo);
  const int nAtual = incrementoVirtualDePassos(passoAtualA, passoAtualB);
  passosDeRserieIntercalado(rSerieAlvo, nAtual, passoA, passoB);
}

uint8_t potenciaParaPasso(float sinalControle) {
  uint8_t passoA = 0;
  uint8_t passoB = 0;
  potenciaParaPassos(sinalControle, &passoA, &passoB);
  return passoA;
}
