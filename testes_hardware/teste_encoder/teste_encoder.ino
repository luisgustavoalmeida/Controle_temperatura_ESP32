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
  int clk = digitalRead(PINO_ENCODER_CLK);
  int dt = digitalRead(PINO_ENCODER_DT);
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
  return digitalRead(PINO_ENCODER_BOTAO) == LOW;
}

void setup() {
  Serial.begin(SERIAL_VELOCIDADE);
  delay(800);
  Serial.println();
  Serial.println("========================================");
  Serial.println("  TESTE ENCODER ROTATIVO");
  Serial.println("  Monitor Serial: 115200 baud");
  Serial.println("========================================");
  Serial.printf("[INFO] CLK=GPIO%d DT=GPIO%d SW=GPIO%d\n",
                 PINO_ENCODER_CLK, PINO_ENCODER_DT, PINO_ENCODER_BOTAO);

  pinMode(PINO_ENCODER_CLK, INPUT_PULLUP);
  pinMode(PINO_ENCODER_DT, INPUT_PULLUP);
  pinMode(PINO_ENCODER_BOTAO, INPUT_PULLUP);

  ultimoClk = digitalRead(PINO_ENCODER_CLK);
  swAnterior = digitalRead(PINO_ENCODER_BOTAO);
  attachInterrupt(digitalPinToInterrupt(PINO_ENCODER_CLK), isrEncoder, CHANGE);

  Serial.println("[OK] Encoder pronto.");
  Serial.println("----------------------------------------");
}

void loop() {
  int d;
  bool swAgora = digitalRead(PINO_ENCODER_BOTAO);

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
