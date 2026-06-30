/**
 * lcd_i2c_compat.h — LCD I2C (PCF8574) para NewLiquidCrystal ou marcoschwartz.
 */

#ifndef LCD_I2C_COMPAT_H
#define LCD_I2C_COMPAT_H

#include "config.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#if LCD_USA_NEW_LIQUIDCRYSTAL && LCD_LAYOUT_YWROBOT
#define LCD_I2C_CTOR_ARGS  LCD_ENDERECO_I2C, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE
#elif LCD_USA_NEW_LIQUIDCRYSTAL
#define LCD_I2C_CTOR_ARGS  LCD_ENDERECO_I2C
#else
#define LCD_I2C_CTOR_ARGS  LCD_ENDERECO_I2C, LCD_COLUNAS, LCD_LINHAS
#endif

inline void lcdI2cWireBegin() {
  Wire.begin(PINO_I2C_SDA, PINO_I2C_SCL);
  delay(50);
}

inline bool lcdI2cEnderecoResponde(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

inline void lcdI2cScanSerial() {
  Serial.println("[INFO] Varredura I2C (SDA/SCL):");
  uint8_t encontrados = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    if (lcdI2cEnderecoResponde(addr)) {
      Serial.printf("       dispositivo em 0x%02X", addr);
      if (addr == LCD_ENDERECO_I2C) {
        Serial.print("  <- LCD_ENDERECO_I2C em config.h");
      }
      Serial.println();
      encontrados++;
    }
  }
  if (encontrados == 0) {
    Serial.println("[ERRO] Nenhum dispositivo I2C. Verifique SDA, SCL, VCC, GND.");
  } else if (!lcdI2cEnderecoResponde(LCD_ENDERECO_I2C)) {
    Serial.printf("[ERRO] Nada em 0x%02X — altere LCD_ENDERECO_I2C (comum: 0x27 ou 0x3F).\n",
                   LCD_ENDERECO_I2C);
  } else {
    Serial.println("[OK] Endereco LCD_ENDERECO_I2C responde.");
  }
}

#if LCD_USA_NEW_LIQUIDCRYSTAL

inline void lcdI2cIniciar(LiquidCrystal_I2C& lcd) {
  lcd.begin(LCD_COLUNAS, LCD_LINHAS);
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

inline void lcdI2cBacklightOn(LiquidCrystal_I2C& lcd) {
#if LCD_USA_NEW_LIQUIDCRYSTAL
  lcd.setBacklight(255);
#else
  lcd.backlight();
#endif
}

inline void lcdI2cBacklightOff(LiquidCrystal_I2C& lcd) {
#if LCD_USA_NEW_LIQUIDCRYSTAL
  lcd.setBacklight(0);
#else
  lcd.noBacklight();
#endif
}

#endif // LCD_I2C_COMPAT_H
