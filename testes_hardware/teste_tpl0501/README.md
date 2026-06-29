# Teste TPL0501 — 2 chips + resistor em paralelo

Sketch para **validar** e **aferir** o hardware antes de gravar a malha principal.

## O que você vai montar

```
[GND]─── L(A) ─── TPL0501 A ─── W(A) ─── H(B) ─── TPL0501 B ─── W(B) ─── saída ───┬── resistor fixo R_par ── GND
                                                                                  └── (rede do chuveiro)
```

- **Chip A:** terminal L → GND; cursor W → terminal H do chip B  
- **Chip B:** terminal L ← W do A; cursor W → saída do circuito  
- **Resistor em paralelo:** entre a saída (W do B) e GND  
- **SPI:** CS_A=GPIO5, CS_B=GPIO16, SCLK=18, MOSI=23  

## Gravar o teste

```bash
cd testes_hardware/teste_tpl0501
pio run -t upload
pio device monitor -b 115200
```

**Arduino IDE:** abra `teste_tpl0501.ino`. Em `config.h` confirme  
`MODO_POT_REDE = MODO_POT_DUPLO_SERIE_PARALELO`.

Monitor Serial: **115200**, fim de linha **NL**.

---

## Passo a passo — aferição completa

| Passo | Comando | O que fazer |
|-------|---------|-------------|
| **1** | `v` | Teste rápido — Req na rede deve subir de 0 → 128 → 255 |
| **2** | `aa` | L–W chip A (3 pontos) |
| **3** | `ab` | L–W chip B |
| **4** | `ae` | Req na rede (A=B) |
| **5** | `l` | Copiar `#define` para `config.h` |

**Antes da aferição:** meça o resistor paralelo e confira `RESISTOR_PARALELO_KOHM` em `config.h`.

---

## Comandos úteis

| Comando | Função |
|---------|--------|
| `?` | Ajuda |
| `s` | Status passos, R_série, Req, potência % |
| `h` | Passo 0 (Req mínima) |
| `m` | Passo 255 (Req máxima) |
| `p128` | Passo igual A=B |
| `u50` | Potência 50 % (como o PID) |

---

## O que colar em `config.h`

Após `l`:

```c
#define POT_AFERIDO_KOHM_MAX_A             97.5f
#define POT_AFERIDO_KOHM_MAX_B             98.2f
#define REQ_IDEAL_POTENCIA_MIN_KOHM       150.6f
#define RESISTOR_PARALELO_KOHM            730.0f
```

Depois grave o firmware principal: `Controle_temperatura_ESP32.ino`.
