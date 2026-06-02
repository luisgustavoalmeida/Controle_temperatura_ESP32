/**
 * Teste encoder rotativo — Arduino IDE
 *
 * Monitor Serial: 115200 baud
 * Pinos: config.h (CLK 25, DT 26, SW 27)
 */

#include "../../config.h"

static volatile int delta = 0;
static int ultimoClk = HIGH;

static long totalSemBotao = 0;
static long totalComBotao = 0;
static unsigned long totalCliques = 0;

static bool swAnterior = HIGH;
static unsigned long swPressionadoMs = 0;
static bool rotouComSwPressionado = false;

void IRAM_ATTR isrEncoder() {
  int clk = digitalRead(PIN_ENC_CLK);
  int dt = digitalRead(PIN_ENC_DT);
  if (clk != ultimoClk) {
    if (dt != clk) {
      delta++;
    } else {
      delta--;
    }
    ultimoClk = clk;
  }
}

bool botaoPressionado() {
  return digitalRead(PIN_ENC_SW) == LOW;
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(800);
  Serial.println();
  Serial.println("========================================");
  Serial.println("  TESTE ENCODER ROTATIVO");
  Serial.println("  Monitor Serial: 115200 baud");
  Serial.println("========================================");
  Serial.printf("[INFO] CLK=GPIO%d DT=GPIO%d SW=GPIO%d\n",
                 PIN_ENC_CLK, PIN_ENC_DT, PIN_ENC_SW);

  pinMode(PIN_ENC_CLK, INPUT_PULLUP);
  pinMode(PIN_ENC_DT, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);

  ultimoClk = digitalRead(PIN_ENC_CLK);
  swAnterior = digitalRead(PIN_ENC_SW);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_CLK), isrEncoder, CHANGE);

  Serial.println("[OK] Encoder pronto.");
  Serial.println("----------------------------------------");
}

void loop() {
  int d;
  bool swAgora = digitalRead(PIN_ENC_SW);

  noInterrupts();
  d = delta;
  delta = 0;
  interrupts();

  if (swAgora == LOW && swAnterior == HIGH) {
    swPressionadoMs = millis();
    rotouComSwPressionado = false;
  }

  if (d != 0) {
    if (botaoPressionado()) {
      rotouComSwPressionado = true;
      totalComBotao += d;
      Serial.printf("[OK] Rotacao COM botao: %+d | total: %ld\n", d, totalComBotao);
    } else {
      totalSemBotao += d;
      Serial.printf("[OK] Rotacao sem botao: %+d | total: %ld\n", d, totalSemBotao);
    }
  }

  if (swAgora == HIGH && swAnterior == LOW) {
    if (!rotouComSwPressionado && (millis() - swPressionadoMs) < 600) {
      totalCliques++;
      Serial.printf("[OK] Clique #%lu\n", totalCliques);
    }
  }

  swAnterior = swAgora;
  delay(10);
}
