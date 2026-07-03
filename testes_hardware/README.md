# Testes de hardware

Sketches para validar cada periférico **antes** do firmware principal. Pinos e constantes em [`../config.h`](../config.h).

**Monitor Serial:** 115200 baud.

## Configuração

1. Suporte **ESP32** no Arduino IDE ou **PlatformIO**.
2. Bibliotecas: **DallasTemperature**, **OneWire** (`libraries/OneWire`), **LiquidCrystal I2C** ou **NewLiquidCrystal** (`LCD_USA_NEW_LIQUIDCRYSTAL` em `config.h`).
3. Abra **somente** o `.ino` da pasta do teste.
4. Grave **um teste por vez**.

## Ordem sugerida

| # | Pasta | O que valida |
|---|-------|----------------|
| 1 | `teste_buzzer/` | Buzzer em `PINO_BUZZER` |
| 2 | `teste_lcd/` | LCD I2C — endereço `LCD_ENDERECO_I2C` |
| 3 | `teste_ds18b20/` | Sensor em `PINO_SENSOR_TEMP` |
| 4 | `teste_encoder/` | Encoder em `PINO_ENCODER_*` |

## O que esperar no Serial

### teste_buzzer
Bipes 1/3 … 3/3 a cada 2 s.

### teste_lcd
Varredura I2C; contador nas 4 linhas. Se piscar sem texto: `LCD_LAYOUT_YWROBOT = 1` e endereço `0x27` ou `0x3F`.

### teste_ds18b20
`[OK] Temperatura: XX.X C` ou `[ERRO]` se sensor ausente.

### teste_encoder
Rotação ±1, clique, duplo clique.

---

O atuador de potência usa **dimmer TRIAC** no firmware principal — não há sketch de teste dedicado na pasta `testes_hardware/`.

Depois de todos os testes OK, grave o firmware principal: `Controle_temperatura_ESP32.ino`.
