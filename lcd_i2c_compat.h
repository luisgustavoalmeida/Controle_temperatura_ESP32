/**
 * lcd_i2c_compat.h — LCD I2C (PCF8574) para NewLiquidCrystal ou marcoschwartz.
 *
 * Modulos baratos usam layout YWROBOT (LCD_PCF8574_LAYOUT_YWROBOT = 1).
 * Mapeamento errado faz o backlight piscar e o texto nao aparece.
 */

#ifndef LCD_I2C_COMPAT_H
#define LCD_I2C_COMPAT_H

#include "config.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#if LCD_USE_NEW_LIQUIDCRYSTAL && LCD_PCF8574_LAYOUT_YWROBOT
// Rs=0, Rw=1, En=2, D4..D7=4..7, backlight=3 (igual LiquidCrystal I2C comum)
#define LCD_I2C_CTOR_ARGS  LCD_I2C_ADDR, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE
#elif LCD_USE_NEW_LIQUIDCRYSTAL
#define LCD_I2C_CTOR_ARGS  LCD_I2C_ADDR
#else
#define LCD_I2C_CTOR_ARGS  LCD_I2C_ADDR, LCD_COLS, LCD_ROWS
#endif

inline void lcdI2cWireBegin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  delay(50);
}

inline bool lcdI2cEnderecoResponde(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

/** Varredura I2C — use no teste_lcd com Monitor Serial 115200 */
inline void lcdI2cScanSerial() {
  Serial.println("[INFO] Varredura I2C (SDA/SCL):");
  uint8_t encontrados = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    if (lcdI2cEnderecoResponde(addr)) {
      Serial.printf("       dispositivo em 0x%02X", addr);
      if (addr == LCD_I2C_ADDR) {
        Serial.print("  <- LCD_I2C_ADDR em config.h");
      }
      Serial.println();
      encontrados++;
    }
  }
  if (encontrados == 0) {
    Serial.println("[ERRO] Nenhum dispositivo I2C. Verifique SDA, SCL, VCC, GND.");
  } else if (!lcdI2cEnderecoResponde(LCD_I2C_ADDR)) {
    Serial.printf("[ERRO] Nada em 0x%02X — altere LCD_I2C_ADDR em config.h (comum: 0x27 ou 0x3F).\n",
                   LCD_I2C_ADDR);
  } else {
    Serial.println("[OK] Endereco LCD_I2C_ADDR responde.");
  }
}

#if LCD_USE_NEW_LIQUIDCRYSTAL

inline void lcdI2cIniciar(LiquidCrystal_I2C& lcd) {
  lcd.begin(LCD_COLS, LCD_ROWS);
  lcd.setBacklight(255);
  delay(10);
  lcd.clear();
}

#else

inline void lcdI2cIniciar(LiquidCrystal_I2C& lcd) {
  lcd.init();
  lcd.backlight();
  delay(10);
  lcd.clear();
}

#endif

#endif // LCD_I2C_COMPAT_H
