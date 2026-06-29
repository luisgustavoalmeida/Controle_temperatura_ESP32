/**
 * Teste buzzer — Arduino IDE
 *
 * Monitor Serial: 115200 baud — confirme cada ciclo de bipes.
 * Pino: config.h (GPIO 32); (-) do buzzer -> GND
 */

#include "../../config.h"

void setup() {
  Serial.begin(SERIAL_VELOCIDADE);
  delay(800);
  Serial.println();
  Serial.println("========================================");
  Serial.println("  TESTE BUZZER");
  Serial.println("  Monitor Serial: 115200 baud");
  Serial.println("========================================");
  Serial.printf("[INFO] Pino sinal: GPIO %d | (-) -> GND\n", PINO_BUZZER);
  Serial.println("[INFO] 3 bipes curtos a cada 2 s — escute e leia o serial.");

  pinMode(PINO_BUZZER, OUTPUT);
  digitalWrite(PINO_BUZZER, LOW);
  Serial.println("[OK] Buzzer configurado. Iniciando ciclos...");
  Serial.println("----------------------------------------");
}

void tresBipes() {
  const int freq = 2000;
  const int ms = 120;
  for (int i = 0; i < 3; i++) {
    Serial.printf("[OK] Bipe %d/3 — frequencia %d Hz\n", i + 1, freq);
    tone(PINO_BUZZER, freq);
    delay(ms);
    noTone(PINO_BUZZER);
    delay(80);
  }
}

void loop() {
  Serial.println("[INFO] --- Novo ciclo de bipes ---");
  tresBipes();
  Serial.println("[OK] Ciclo completo. Proximo em 2 s.");
  Serial.println();
  delay(2000);
}
