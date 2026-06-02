/**
 * sensor_ds18b20.h — DS18B20 em modo assíncrono (não bloqueante)
 *
 * Padrão correto (evita delay/travamento no loop):
 *   1. iniciarConversao()     → requestTemperatures() e retorna na hora
 *   2. (outras tarefas do firmware rodam livremente)
 *   3. conversaoConcluida()   → true só após ~750 ms (12 bits) ou chip pronto
 *   4. lerGrausCSePronto()    → getTempCByIndex() apenas se o passo 3 for true
 *
 * Atalho no loop principal: atualizar(&temp) faz os passos 3–4 e já dispara o próximo 1.
 *
 * Requer setWaitForConversion(false) — configurado em iniciar().
 */

#ifndef SENSOR_DS18B20_H
#define SENSOR_DS18B20_H

#include <Arduino.h>

class SensorDS18B20 {
public:
  void iniciar();

  /** Dispara conversão no barramento 1-Wire (não espera o resultado). */
  void iniciarConversao();

  /**
   * true quando já passou o tempo mínimo da resolução atual ou o chip sinalizou fim.
   * Enquanto false, NÃO chame lerGrausCSePronto().
   */
  bool conversaoConcluida();

  /** true se há conversão em andamento aguardando tempo/bus. */
  bool conversaoEmAndamento() const;

  /**
   * Lê °C somente se conversaoConcluida(); caso contrário retorna NAN.
   * Não bloqueia o processador.
   */
  float lerGrausCSePronto();

  /**
   * Ciclo completo para o loop principal:
   *   - Se a conversão anterior terminou → preenche *temperaturaC e retorna true.
   *   - Sempre agenda a próxima conversão após uma leitura bem-sucedida.
   *   - Se ainda convertendo → retorna false (continue outras tarefas).
   */
  bool atualizar(float* temperaturaC);

  bool sensorOk() const;
  bool jaObteveLeituraValida() const;

private:
  bool _ok;
  bool _conversaoEmAndamento;
  bool _leituraValidaJaOcorreu;
  unsigned long _millisInicioConversao;
  uint16_t _tempoMinimoConversaoMs;
  uint8_t _resolucaoBits;
  bool _checarConversaoNoBarramento;

  float lerValorConvertido();
};

#endif // SENSOR_DS18B20_H
