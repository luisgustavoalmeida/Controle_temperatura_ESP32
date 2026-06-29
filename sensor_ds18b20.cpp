/**
 * sensor_ds18b20.cpp — Leitura assíncrona DallasTemperature
 *
 * O erro mais comum é:
 *   requestTemperatures();
 *   getTempCByIndex(0);   // ← ainda dentro do tempo de conversão ou com wait=true
 *
 * Aqui waitForConversion permanece FALSE; o tempo é controlado por este módulo.
 */

#include "sensor_ds18b20.h"
#include "config.h"
#include <OneWire.h>
#include <DallasTemperature.h>

static OneWire barramentoOneWire(PINO_SENSOR_TEMP);
static DallasTemperature sensores(&barramentoOneWire);

void SensorDS18B20::iniciar() {
  sensores.begin();
  sensores.setResolution(SENSOR_RESOLUCAO_BITS);

  // Essencial: requestTemperatures() retorna imediatamente
  sensores.setWaitForConversion(false);

  // Opcional: consulta o barramento (read_bit) em vez de chutar só o delay
  sensores.setCheckForConversion(true);
  _checarConversaoNoBarramento = true;

  _resolucaoBits = SENSOR_RESOLUCAO_BITS;
  _tempoMinimoConversaoMs = sensores.millisToWaitForConversion(_resolucaoBits);

  _ok = (sensores.getDeviceCount() > 0);
  _conversaoEmAndamento = false;
  _leituraValidaJaOcorreu = false;
  _millisInicioConversao = 0;

  if (_ok) {
    iniciarConversao();
  }
}

void SensorDS18B20::iniciarConversao() {
  if (!_ok) {
    return;
  }

  // Envia comando Convert T em todo o barramento — retorno imediato (modo async)
  sensores.requestTemperatures();
  _millisInicioConversao = millis();
  _conversaoEmAndamento = true;
}

bool SensorDS18B20::conversaoConcluida() {
  if (!_ok || !_conversaoEmAndamento) {
    return false;
  }

  unsigned long decorrido = millis() - _millisInicioConversao;

  if (decorrido >= _tempoMinimoConversaoMs) {
    return true;
  }

  // Antes do tempo mínimo: só aceita se o chip sinalizar no barramento (sem delay)
  if (_checarConversaoNoBarramento && sensores.isConversionComplete()) {
    return true;
  }

  return false;
}

bool SensorDS18B20::conversaoEmAndamento() const {
  return _conversaoEmAndamento;
}

float SensorDS18B20::lerValorConvertido() {
  // Só lê o scratchpad — não dispara nova conversão nem chama delay()
  float temperatura = sensores.getTempCByIndex(0);

  if (temperatura == DEVICE_DISCONNECTED_C || temperatura < -55.0f ||
      temperatura > 125.0f) {
    _ok = false;
    return NAN;
  }
  return temperatura;
}

float SensorDS18B20::lerGrausCSePronto() {
  if (!conversaoConcluida()) {
    return NAN;
  }

  float t = lerValorConvertido();
  _conversaoEmAndamento = false;
  if (!isnan(t)) {
    _leituraValidaJaOcorreu = true;
  }
  return t;
}

bool SensorDS18B20::atualizar(float* temperaturaC) {
  if (temperaturaC == nullptr) {
    return false;
  }

  if (!_ok) {
    *temperaturaC = NAN;
    return false;
  }

  // Conversão anterior terminou → ler agora e já pedir a próxima
  if (_conversaoEmAndamento && conversaoConcluida()) {
    *temperaturaC = lerGrausCSePronto();

    if (isnan(*temperaturaC)) {
      return true;  // leitura falhou, mas o ciclo "terminou"
    }

    iniciarConversao();
    return true;
  }

  // Primeira vez ou estado inconsistente: garante conversão em andamento
  if (!_conversaoEmAndamento) {
    iniciarConversao();
  }

  return false;
}

bool SensorDS18B20::sensorOk() const {
  return _ok;
}

bool SensorDS18B20::jaObteveLeituraValida() const {
  return _leituraValidaJaOcorreu;
}
