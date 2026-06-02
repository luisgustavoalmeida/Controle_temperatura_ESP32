/**
 * Teste LCD 20x4 I2C — Arduino IDE
 *
 * Monitor Serial: 115200 baud — varredura I2C + status (confira o LCD).
 * Biblioteca: ver LCD_USE_NEW_LIQUIDCRYSTAL e LCD_PCF8574_LAYOUT_YWROBOT em config.h
 */

#include "../../config.h"
#include "../../lcd_i2c_compat.h"

LiquidCrystal_I2C lcd(LCD_I2C_CTOR_ARGS);

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(800);
  Serial.println();
  Serial.println("========================================");
  Serial.println("  TESTE LCD I2C 20x4");
  Serial.println("  Monitor Serial: 115200 baud");
  Serial.println("========================================");
  Serial.printf("[INFO] SDA=GPIO%d SCL=GPIO%d addr config=0x%02X\n",
                 PIN_I2C_SDA, PIN_I2C_SCL, LCD_I2C_ADDR);
#if LCD_USE_NEW_LIQUIDCRYSTAL && LCD_PCF8574_LAYOUT_YWROBOT
  Serial.println("[INFO] NewLiquidCrystal + layout YWROBOT (modulo comum)");
#elif LCD_USE_NEW_LIQUIDCRYSTAL
  Serial.println("[INFO] NewLiquidCrystal layout padrao (raro)");
#else
  Serial.println("[INFO] Biblioteca LiquidCrystal I2C (marcoschwartz)");
#endif
  Serial.println("[INFO] Ajuste o trimpot de CONTRASTE no modulo se nao ver texto.");
  Serial.println();

  lcdI2cWireBegin();
  lcdI2cScanSerial();

  if (!lcdI2cEnderecoResponde(LCD_I2C_ADDR)) {
    Serial.println("[ERRO] Pare aqui: corrija endereco I2C ou fiacao antes do LCD.");
    return;
  }

  Serial.println("[INFO] Inicializando LCD...");
  lcdI2cIniciar(lcd);

  lcd.setCursor(0, 0);
  lcd.print("Teste LCD OK");
  lcd.setCursor(0, 1);
  lcd.print("Monitor 115200");
  lcd.setCursor(0, 2);
  lcd.print("GPIO21/22");
  lcd.setCursor(0, 3);
  lcd.print("Contador: 0");

  Serial.println("[OK] Texto enviado ao LCD.");
  Serial.println("[OK] Deve ver 4 linhas estaveis (sem piscar).");
  Serial.println("[OK] Contador na linha 4 a cada 1 s no Serial e no LCD.");
  Serial.println("----------------------------------------");
}

void loop() {
  static unsigned contador = 0;
  static unsigned long ultimo = 0;

  if (millis() - ultimo < 1000) {
    return;
  }
  ultimo = millis();
  contador++;

  lcd.setCursor(11, 3);
  lcd.print(contador);
  if (contador < 10) {
    lcd.print(' ');
  }

  Serial.printf("[OK] Contador LCD: %u\n", contador);
}
