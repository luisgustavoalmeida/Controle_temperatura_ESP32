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
| 5 | `teste_tpl0501/` | TPL0501 — aferição e validação |

## teste_tpl0501 (TPL0501)

**Guia completo:** [`teste_tpl0501/README.md`](teste_tpl0501/README.md)

```bash
cd testes_hardware/teste_tpl0501
pio run -t upload
pio device monitor -b 115200
```

**Roteiro de aferição:**

| # | Comando | Multímetro |
|---|---------|------------|
| 1 | `v` | Req na rede de saída |
| 2 | `aa` | L–W chip A (B=0) |
| 3 | `ab` | L–W chip B (A=0) |
| 4 | `ae` | Req com A=B |
| 5 | `l` | Copia `#define` para `config.h` |

Atalho: `a` = passos 2+3+4. Ajuda: `?`.

**Modos PlatformIO:** `-e esp32dev_paralelo` (padrão) | `esp32dev` (1 chip) | `esp32dev_duplo` (2 série).

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

Testes legados **X9C104S** foram removidos — o atuador atual usa **TPL0501** (SPI).

Depois de todos os testes OK, grave o firmware principal: `Controle_temperatura_ESP32.ino`.
