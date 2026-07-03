/**
 * atuador_dimmer.cpp — Driver RobotDyn via rbdimmerESP32
 *
 * ESP32 core 3.x + ESP-IDF 5+. Níveis 0..100 % (corte de fase TRIAC).
 */

#include "atuador_dimmer.h"
#include "rbdimmerESP32.h"
#include <esp_timer.h>

static rbdimmer_channel_t* s_canalDimmer = nullptr;
static volatile uint32_t s_ultimoZeroCrossUs = 0;

static void IRAM_ATTR aoZeroCross(void* /*userData*/) {
  s_ultimoZeroCrossUs = (uint32_t)esp_timer_get_time();
}

static rbdimmer_curve_t curvaDimmerDeConfig() {
#if DIMMER_CURVA_TIPO == DIMMER_CURVA_RMS
  return RBDIMMER_CURVE_RMS;
#elif DIMMER_CURVA_TIPO == DIMMER_CURVA_LOGARITMICA
  return RBDIMMER_CURVE_LOGARITHMIC;
#else
  return RBDIMMER_CURVE_LINEAR;
#endif
}

static_assert(DIMMER_CURVA_LINEAR == (int)RBDIMMER_CURVE_LINEAR,
              "DIMMER_CURVA_LINEAR deve coincidir com RBDIMMER_CURVE_LINEAR");
static_assert(DIMMER_CURVA_RMS == (int)RBDIMMER_CURVE_RMS,
              "DIMMER_CURVA_RMS deve coincidir com RBDIMMER_CURVE_RMS");
static_assert(DIMMER_CURVA_LOGARITMICA == (int)RBDIMMER_CURVE_LOGARITHMIC,
              "DIMMER_CURVA_LOGARITMICA deve coincidir com RBDIMMER_CURVE_LOGARITHMIC");

/** Recalcula current_delay mesmo se o nivel nao mudou (ex.: apos set_active). */
static void forcarRecalculoAtraso(rbdimmer_channel_t* canal) {
  if (canal == nullptr) {
    return;
  }
  uint8_t nivel = rbdimmer_get_level(canal);
  if (nivel == 0) {
    return;
  }
  if (nivel > 1) {
    rbdimmer_set_level(canal, (uint8_t)(nivel - 1));
  }
  rbdimmer_set_level(canal, nivel);
  rbdimmer_update_all();
}

/** Evita nivel 100 (LINEAR: delay 0) e extremo instavel no timer minimo. */
static uint8_t limitarNivelParaBiblioteca(uint8_t nivelPct) {
  if (nivelPct > DIMMER_NIVEL_PLENO_ESTAVEL) {
    return DIMMER_NIVEL_PLENO_ESTAVEL;
  }
  return nivelPct;
}

static void logarErroDimmer(const __FlashStringHelper* etapa, rbdimmer_err_t err) {
  if (err == RBDIMMER_OK) {
    return;
  }
  Serial.print(F("[DIM] "));
  Serial.print(etapa);
  Serial.print(F(" falhou: "));
  Serial.println((int)err);
}

#if DIMMER_USA_CALIBRACAO_POTENCIA_LINEAR

static const float s_calComandoPct[] = { DIMMER_CAL_COMANDO_PCT_TABLE };
static const float s_calPotenciaPct[] = { DIMMER_CAL_POTENCIA_PCT_TABLE };

static_assert(sizeof(s_calComandoPct) / sizeof(s_calComandoPct[0]) ==
                  DIMMER_CAL_POT_NUM_PONTOS,
              "DIMMER_CAL_COMANDO_PCT_TABLE: ajuste DIMMER_CAL_POT_NUM_PONTOS");
static_assert(sizeof(s_calPotenciaPct) / sizeof(s_calPotenciaPct[0]) ==
                  DIMMER_CAL_POT_NUM_PONTOS,
              "DIMMER_CAL_POTENCIA_PCT_TABLE: ajuste DIMMER_CAL_POT_NUM_PONTOS");

static float interpolarTabela(const float* eixoX, const float* eixoY, size_t n,
                              float x) {
  if (n == 0) {
    return 0.0f;
  }
  if (x <= eixoX[0]) {
    return eixoY[0];
  }
  if (x >= eixoX[n - 1]) {
    return eixoY[n - 1];
  }
  for (size_t i = 0; i < n - 1; i++) {
    if (x <= eixoX[i + 1]) {
      float dx = eixoX[i + 1] - eixoX[i];
      if (dx <= 0.0f) {
        return eixoY[i + 1];
      }
      float t = (x - eixoX[i]) / dx;
      return eixoY[i] + t * (eixoY[i + 1] - eixoY[i]);
    }
  }
  return eixoY[n - 1];
}

/** Potencia real estimada [%] para um comando [%] (tabela direta). */
static float potenciaPctDeComando(float comandoPct) {
  return interpolarTabela(s_calComandoPct, s_calPotenciaPct, DIMMER_CAL_POT_NUM_PONTOS,
                          comandoPct);
}

/** Comando [%] para atingir potencia alvo [%] (tabela inversa). */
static float comandoPctDePotenciaAlvo(float potenciaAlvoPct) {
  if (potenciaAlvoPct <= 0.0f) {
    return 0.0f;
  }
  if (potenciaAlvoPct >= 100.0f) {
    return 100.0f;
  }
  return interpolarTabela(s_calPotenciaPct, s_calComandoPct, DIMMER_CAL_POT_NUM_PONTOS,
                          potenciaAlvoPct);
}

#endif // DIMMER_USA_CALIBRACAO_POTENCIA_LINEAR

static uint8_t nivelDeComandoPct(float comandoPct) {
  if (comandoPct <= 0.0f) {
    return 0;
  }
  int nivel = (int)(comandoPct * (float)DIMMER_NIVEL_MAX / 100.0f + 0.5f);
  if (nivel < (int)DIMMER_NIVEL_MIN_EFETIVO) {
    nivel = DIMMER_NIVEL_MIN_EFETIVO;
  }
  if (nivel > (int)DIMMER_NIVEL_MAX) {
    nivel = DIMMER_NIVEL_MAX;
  }
  return limitarNivelParaBiblioteca((uint8_t)nivel);
}

AtuadorDimmer::AtuadorDimmer()
    : _alvoNormalizado(0.0f), _nivelAlvo(0), _nivelAtual(0), _iniciado(false) {}

void AtuadorDimmer::iniciar() {
  if (_iniciado) {
    return;
  }

  logarErroDimmer(F("init"), rbdimmer_init());
  logarErroDimmer(F("zero_cross"),
                  rbdimmer_register_zero_cross(PINO_DIMMER_ZC, DIMMER_FASE_REDE,
                                               DIMMER_FREQUENCIA_REDE_HZ));
  logarErroDimmer(F("zc_callback"),
                  rbdimmer_set_callback(DIMMER_FASE_REDE, aoZeroCross, nullptr));

  rbdimmer_config_t config = {
      .gpio_pin = PINO_DIMMER_PSM,
      .phase = DIMMER_FASE_REDE,
      .initial_level = 0,
      .curve_type = curvaDimmerDeConfig(),
  };

  logarErroDimmer(F("create_channel"), rbdimmer_create_channel(&config, &s_canalDimmer));

  if (s_canalDimmer != nullptr) {
    rbdimmer_curve_t curva = curvaDimmerDeConfig();
    rbdimmer_set_curve(s_canalDimmer, curva);
    // Canal permanece ativo (padrao da biblioteca); nivel 0 = sem disparo.
    rbdimmer_set_level(s_canalDimmer, 0);
    rbdimmer_update_all();
  }

  _alvoNormalizado = 0.0f;
  _nivelAlvo = 0;
  _nivelAtual = 0;
  _iniciado = true;

  Serial.print(F("[DIM] rbdimmerESP32 ZC="));
  Serial.print(PINO_DIMMER_ZC);
  Serial.print(F(" PSM="));
  Serial.print(PINO_DIMMER_PSM);
  Serial.print(F(" curva_cfg="));
  Serial.print(DIMMER_CURVA_TIPO);
  Serial.print(F(" ("));
  Serial.print(F(DIMMER_CURVA_NOME));
  Serial.print(F(") curva_lib="));
  Serial.print(s_canalDimmer != nullptr ? (int)rbdimmer_get_curve(s_canalDimmer) : -1);
  Serial.print(F(" pleno="));
  Serial.print(DIMMER_NIVEL_PLENO_ESTAVEL);
  Serial.print(F(" min="));
  Serial.print(DIMMER_NIVEL_MIN);
  Serial.print(F(" minEff="));
  Serial.print(DIMMER_NIVEL_MIN_EFETIVO);
  Serial.print(F(" cal_pot="));
  Serial.println(DIMMER_USA_CALIBRACAO_POTENCIA_LINEAR ? 1 : 0);
}

void AtuadorDimmer::aplicarNivel(uint8_t nivelPct) {
  if (!_iniciado || s_canalDimmer == nullptr) {
    return;
  }
  if (nivelPct > DIMMER_NIVEL_MAX) {
    nivelPct = DIMMER_NIVEL_MAX;
  }
  nivelPct = limitarNivelParaBiblioteca(nivelPct);

  _nivelAlvo = nivelPct;

  if (nivelPct == _nivelAtual) {
    return;
  }

  _nivelAtual = nivelPct;

  if (nivelPct == 0) {
    rbdimmer_set_level(s_canalDimmer, 0);
    rbdimmer_update_all();
    return;
  }

  if (!rbdimmer_is_active(s_canalDimmer)) {
    rbdimmer_set_active(s_canalDimmer, true);
  }
  rbdimmer_set_level(s_canalDimmer, nivelPct);
  forcarRecalculoAtraso(s_canalDimmer);
}

/** OUT 0..1 -> nivel dimmer; com calibracao, OUT = potencia linear desejada. */
static uint8_t nivelDePotencia01(float potencia01) {
  if (potencia01 <= 0.0f) {
    return 0;
  }
#if DIMMER_USA_CALIBRACAO_POTENCIA_LINEAR
  float comandoPct = comandoPctDePotenciaAlvo(potencia01 * 100.0f);
  return nivelDeComandoPct(comandoPct);
#else
  int nivel = (int)(potencia01 * (float)DIMMER_NIVEL_MAX + 0.5f);
  if (nivel < (int)DIMMER_NIVEL_MIN_EFETIVO) {
    nivel = DIMMER_NIVEL_MIN_EFETIVO;
  }
  if (nivel > (int)DIMMER_NIVEL_MAX) {
    nivel = DIMMER_NIVEL_MAX;
  }
  return limitarNivelParaBiblioteca((uint8_t)nivel);
#endif
}

void AtuadorDimmer::definirSaidaNormalizadaRapida(float potencia01) {
  if (potencia01 < 0.0f) {
    potencia01 = 0.0f;
  } else if (potencia01 > 1.0f) {
    potencia01 = 1.0f;
  }
  _alvoNormalizado = potencia01;
  aplicarNivel(nivelDePotencia01(potencia01));
}

void AtuadorDimmer::definirPotenciaMaxima() {
  definirSaidaNormalizadaRapida(1.0f);
}

uint8_t AtuadorDimmer::nivelAtual() const {
  if (s_canalDimmer != nullptr && _iniciado) {
    return rbdimmer_get_level(s_canalDimmer);
  }
  return _nivelAtual;
}

float AtuadorDimmer::potenciaAtualPercentual() const {
#if DIMMER_USA_CALIBRACAO_POTENCIA_LINEAR
  return potenciaPctDeComando(comandoDimmerPercentual());
#else
  return (float)nivelAtual();
#endif
}

float AtuadorDimmer::potenciaAlvoPercentual() const {
  return _alvoNormalizado * 100.0f;
}

float AtuadorDimmer::comandoDimmerPercentual() const {
  return (float)nivelAtual() * 100.0f / (float)DIMMER_NIVEL_MAX;
}

bool AtuadorDimmer::usaCalibracaoPotenciaLinear() const {
#if DIMMER_USA_CALIBRACAO_POTENCIA_LINEAR
  return true;
#else
  return false;
#endif
}

int AtuadorDimmer::tipoCurvaConfig() const {
  return (int)DIMMER_CURVA_TIPO;
}

int AtuadorDimmer::tipoCurvaBiblioteca() const {
  if (s_canalDimmer != nullptr && _iniciado) {
    return (int)rbdimmer_get_curve(s_canalDimmer);
  }
  return -1;
}

uint32_t AtuadorDimmer::atrasoDisparoUs() const {
  if (s_canalDimmer != nullptr && _iniciado) {
    return rbdimmer_get_delay(s_canalDimmer);
  }
  return 0;
}

bool AtuadorDimmer::redeComZeroCross() const {
  if (!_iniciado) {
    return false;
  }
  uint32_t ultimoUs = s_ultimoZeroCrossUs;
  if (ultimoUs == 0) {
    return false;
  }
  uint32_t agoraUs = (uint32_t)esp_timer_get_time();
  uint32_t limiteUs = (uint32_t)MEDIDOR_ZC_TIMEOUT_MS * 1000UL;
  return (agoraUs - ultimoUs) <= limiteUs;
}
