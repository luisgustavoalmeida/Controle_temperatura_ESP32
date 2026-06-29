/**
 * Teste DS18B20 — Arduino IDE
 *
 * Monitor Serial: 115200 baud — acompanhe todas as leituras aqui.
 * Bibliotecas: OneWire 2.3.8 (pasta libraries/ deste sketch ou do projeto)
 *              + DallasTemperature (Gerenciador de bibliotecas)
 * Pinos: config.h (GPIO 4 + pull-up 4,7 kOhm)
 *
 * Erro GPIO / OneWire_direct_gpio.h: remova ou renomeie
 * Documents/Arduino/libraries/arduino_514513 (versao antiga incompativel com ESP32 3.x).
 */

#include "../../config.h"
#include <OneWire.h>
#include <DallasTemperature.h>

OneWire oneWire(PINO_SENSOR_TEMP);
DallasTemperature sensores(&oneWire);

void setup() {
  Serial.begin(SERIAL_VELOCIDADE);
  delay(800);
  Serial.println();
  Serial.println("========================================");
  Serial.println("  TESTE DS18B20");
  Serial.println("  Monitor Serial: 115200 baud");
  Serial.println("========================================");
  Serial.printf("[INFO] Pino DQ: GPIO %d\n", PINO_SENSOR_TEMP);
  Serial.println("[INFO] Ligacao: VDD->3,3V GND->GND DQ->GPIO4 + 4,7k para 3,3V");

  sensores.begin();
  sensores.setResolution(12);

  uint8_t n = sensores.getDeviceCount();
  Serial.printf("[INFO] Sensores na busca 1-Wire: %u\n", n);

  if (n == 0) {
    Serial.println("[ERRO] Nenhum DS18B20 detectado.");
    Serial.println("[ERRO] Verifique fios, GND, 3,3 V e resistor pull-up.");
  } else {
    Serial.println("[OK] Sensor encontrado. Iniciando leituras...");
  }
  Serial.println("----------------------------------------");
}

void loop() {
  static unsigned long ultimaLeitura = 0;
  if (millis() - ultimaLeitura < 1000) {
    return;
  }
  ultimaLeitura = millis();

  sensores.requestTemperatures();
  delay(750);
  float c = sensores.getTempCByIndex(0);

  if (c == DEVICE_DISCONNECTED_C) {
    Serial.println("[ERRO] Leitura invalida — sensor desconectado ou barramento ruim.");
  } else {
    Serial.printf("[OK] Temperatura: %.2f C\n", c);
  }
}
