# Controle de temperatura — ESP32 + PID + chuveiro

Firmware para **Arduino IDE** ou **PlatformIO**, em placa **ESP32** (referência de fiação: **NodeMCU-32S** / ESP-WROOM-32; na IDE use **ESP32 Dev Module**). Malha fechada PID com atuador **TPL0501-100** (100 kΩ, 256 taps, SPI), sensor **DS18B20**, display **LCD 20×4 I2C**, **encoder rotativo** e **buzzer**.

A regulagem **não pausa** quando a temperatura entra na faixa do alvo: o PID continua atuando. A histerese (`BUZZER_HISTERESE_C`) vale só para o **buzzer** e para o texto **Temp OK** no LCD.

Os ganhos PID foram calibrados no projeto irmão [`Malha_PID_temperatura`](../Malha_PID_temperatura) (portados de `pid_controller.py`):

| Parâmetro | Valor |
|-----------|-------|
| Kp | 0,025 |
| Ki | 0,0013 |
| Kd | 0,01 |

Saída do PID: **0,0** a **1,0** = potência coerente (1 = máx., 0 = ideal). Escala **`REQ_IDEAL_POTENCIA_MIN_KOHM`** (ex. 150 kΩ) vs TPL0501 ~100 kΩ aferido; LCD mostra % real do passo.

## Pinagem (GPIO)

Valores definidos em `config.h` (diagrama detalhado no cabeçalho do arquivo e em `imagens/Imagem pinos ESP32.png`, se existir):

| Função | GPIO | Observação |
|--------|------|------------|
| DS18B20 (DQ) | 4 | Pull-up **4,7 kΩ** entre DQ e 3,3 V |
| TPL0501 CS (chip A) | 5 | Chip select A (ativo baixo) |
| TPL0501 CS (chip B) | 16 | Modo duplo — chip select B |
| TPL0501 SCLK | 18 | SPI clock (compartilhado) |
| TPL0501 DIN (MOSI) | 23 | SPI dados (compartilhado) |
| I2C SDA (LCD) | 21 | |
| I2C SCL (LCD) | 22 | |
| Encoder CLK (A) | 25 | `INPUT_PULLUP` |
| Encoder DT (B) | 26 | |
| Encoder SW | 27 | Botão para GND ao pressionar |
| Buzzer | 32 | Ativo recomendado; (−) em GND |

**Alimentação (resumo):** DS18B20, encoder, buzzer e TPL0501 em **3,3 V**; LCD em **5 V** ou **3,3 V** conforme o módulo; pinos **H/L/W** do TPL0501 no circuito do potenciômetro do chuveiro (não são GPIO).

**LCD I2C:** endereço padrão **0x27** (`LCD_ENDERECO_I2C`; comum também **0x3F** — use a varredura em `teste_lcd`). Layout PCF8574 **YWROBOT** (`LCD_LAYOUT_YWROBOT = 1`).

**GPIO a evitar:** 0, 2, 15 (boot), 6–11 (flash), 34–39 (somente entrada).

## Arduino IDE — configuração

### 1. Suporte ESP32

- **Ferramentas** → **Placa** → **Gerenciador de placas**
- Instale **esp32** (Espressif Systems)

### 2. Bibliotecas

**Sketch** → **Incluir Biblioteca** → **Gerenciar Bibliotecas**:

1. **OneWire 2.3.8** — use a pasta [`libraries/OneWire`](libraries/OneWire) deste repositório (compatível com ESP32 core 3.x). Se existir `arduino_514513` em `Documentos/Arduino/libraries`, **apague** (versão incompatível). Detalhes em [`libraries/README.md`](libraries/README.md).
2. **DallasTemperature** — Miles Burton (Gerenciador de bibliotecas).
3. **LCD I2C** — conforme `config.h`:
   - `LCD_USA_NEW_LIQUIDCRYSTAL = 1` → **NewLiquidCrystal** (pasta `NewLiquidCrystal_lib` no Arduino IDE).
   - `LCD_USA_NEW_LIQUIDCRYSTAL = 0` → **LiquidCrystal I2C** (marcoschwartz / Frank de Brabander).

O arquivo [`lcd_i2c_compat.h`](lcd_i2c_compat.h) unifica inicialização, varredura I2C e o mapeamento **YWROBOT** (evita backlight piscando sem texto).

### 3. Abrir o sketch

Abra `Controle_temperatura_ESP32.ino` (a pasta inteira vira o projeto).

### 4. Placa e upload

| Opção | Valor |
|-------|-------|
| Placa | **ESP32 Dev Module** |
| Porta | COM do ESP32 (ex.: COM4) |
| Upload speed | 921600 ou **115200** se falhar |

Monitor serial: **115200** baud.

> Você pode editar no **Cursor** e compilar/gravar na **Arduino IDE** — os dois usam a mesma pasta.

## PlatformIO (opcional)

Arquivo [`platformio.ini`](platformio.ini): ambiente `esp32dev`, `monitor_speed = 115200`.

Dependências declaradas: **OneWire**, **DallasTemperature**, **LiquidCrystal I2C** (marcoschwartz). Para compilar com o mesmo LCD da Arduino IDE (**NewLiquidCrystal** + layout YWROBOT), instale a biblioteca correspondente e mantenha `LCD_USA_NEW_LIQUIDCRYSTAL = 1` em `config.h`; ou defina `LCD_USA_NEW_LIQUIDCRYSTAL = 0` e use apenas a lib do `platformio.ini`.

```bash
pio run -t upload
pio device monitor -b 115200
```

**Teste TPL0501 (aferição 2× em série + paralelo):**

```bash
cd testes_hardware/teste_tpl0501
pio run -t upload
pio device monitor -b 115200
```

Comandos principais na Serial: `v` (validação rápida), `aa`/`ab`/`ae` (aferição), `l` (copiar `#define`), `p128`, `u50`, `?`. Ver [`testes_hardware/teste_tpl0501/README.md`](testes_hardware/teste_tpl0501/README.md).

## Testes de hardware (antes do firmware principal)

Na pasta [`testes_hardware/`](testes_hardware/) há um sketch por periférico (mesmos pinos de `config.h`). Grave na Arduino IDE e acompanhe o **Monitor Serial (115200 baud)**. Detalhes em [`testes_hardware/README.md`](testes_hardware/README.md).

| Ordem | Pasta | Objetivo |
|-------|-------|----------|
| 1 | `teste_buzzer/` | Bipes |
| 2 | `teste_lcd/` | I2C + texto (layout/endereço) |
| 3 | `teste_ds18b20/` | Temperatura |
| 4 | `teste_encoder/` | Giro, botão, duplo clique |
| 5 | `teste_tpl0501/` | **TPL0501** — validação, aferição, escada A↔B, paralelo |

## Estrutura do código

Todos os módulos têm comentários em português no cabeçalho e nas funções públicas.

| Arquivo | Função |
|---------|--------|
| `config.h` | Pinos, PID, setpoint, períodos, TPL0501, DS18B20, LCD, serial debug |
| `Controle_temperatura_ESP32.ino` | Programa principal — `setup()` / `loop()` e tarefas periódicas |
| `pid_controller.*` | PID com limite na integral e anti-windup (back-calculation) |
| `potenciometro_map.*` | PID 0..1 → Req linear → passos (1 ou 2 chips, paralelo opcional) |
| `tpl0501.*` | Driver SPI por chip (CS individual; SCLK/MOSI compartilhados) |
| `atuador_potenciometro.*` | Atuador da malha — intercalação A/B no modo duplo |
| `sensor_ds18b20.*` | DS18B20 assíncrono (12 bits, ~750 ms) |
| `lcd_i2c_compat.h` | Compatibilidade e scan I2C (NewLiquidCrystal / marcoschwartz) |
| `display_lcd.*` | Layout das 4 linhas e estados |
| `encoder_rotativo.*` | Setpoint e eventos (giro, clique, duplo, clique longo) |
| `buzzer.*` | Feedback sonoro |

### Fluxo do `loop()`

O `loop()` **não usa `delay()`** (exceto splash curto no `setup()`). Quatro tarefas, cada uma no intervalo de `config.h`:

| Tarefa | Período | Conteúdo |
|--------|---------|----------|
| `tarefaInterfaceUsuario()` | 10 ms (`PERIODO_LOOP_MS`) | Encoder + buzzer |
| `tarefaLeituraSensor()` | 800 ms (`PERIODO_SENSOR_MS`) | DS18B20 — conversão em paralelo (~750 ms) |
| `tarefaMalhaPid()` | 100 ms (`PERIODO_PID_MS`) | PID + atuador (1 ou 2× TPL0501) |
| `tarefaAtualizarDisplay()` | 100 ms (`PERIODO_LCD_MS`) | LCD (cache; só redesenha se mudou) |

**Auxiliares no `.ino`:** filtro de temperatura (`FILTRO_TEMP_AMOSTRAS` em `config.h`), modo seguro se o sensor falhar, detecção de meta para buzzer, ligar/desligar malha, mensagens de transição no LCD.

### Estados no LCD

| Estado | Linha 2 / 3 (resumo) |
|--------|------------------------|
| `ESTADO_AGUARDE_SENSOR` | Aguardando primeira leitura válida |
| `ESTADO_PID_ATIVO` | Regulação; animação `PID.` na linha 4 até meta |
| `ESTADO_CONTROLE_DESLIGADO` | Malha off — potência 0 % |
| `ESTADO_SENSOR_ERRO` | Falha — potência mínima forçada |

Linha 0: **Controle PID ON/OFF** ou mensagens *Ligando/Desligando Malha PID...* (~2,5 s). Linha 1: alvo; linha 2: temperatura atual (2 casas decimais); linha 3: potência % e status (`Temp OK`, `OFF`, `FALHA SENS`, etc.).

## Uso (interface)

### Setpoint (encoder)

| Ação | Comportamento |
|------|----------------|
| Girar encoder | Passo **0,25 °C** (`ALVO_TEMP_PASSO_C`); LCD atualiza na hora |
| Parar de girar | Após **1,5 s** (`ALVO_TEMP_PAUSA_MS`) o alvo entra no PID |
| Alvo pendente no LCD | Linha `Alvo: XX.XX >` (sem `C`) até aplicar na malha |
| Segurar botão + girar | Passo fino **0,01 °C** (`ALVO_TEMP_PASSO_FINO_C`) |
| Faixa | **10,0** a **45,0 °C** |
| Valor inicial | **38,0 °C** (`ALVO_TEMP_PADRAO_C`) |

### Malha PID

| Ação | Comportamento |
|------|----------------|
| **Duplo clique** (malha ligada) | Standby: potenciômetro 0 %, malha **pausada** (OUT/integral preservados) |
| **Clique simples** (malha desligada) | Religa e aplica a OUT memorizada |
| **Clique simples** (desligada e temp. muito abaixo do alvo) | Religa com `pid.reiniciar()` se `SP − PV > STANDBY_RELIGA_REINICIA_DELTA_C` (default 3 °C) |
| **Clique longo** (~800 ms, sem girar) | Reinicia o PID (`ENCODER_CLIQUE_LONGO_MS`) |
| Boot | Malha **desligada** por padrão (`MALHA_INICIA_ATIVA = false`) — **clique simples** para ligar |

Em standby o potenciômetro fica em 0 %; a saída da malha (`saidaPid`) permanece na RAM até religar.

### Buzzer

| Evento | Som |
|--------|-----|
| Entrou na faixa do alvo (± `BUZZER_HISTERESE_C`, default 0,2 °C) | 3 tons ascendentes |
| Saiu da faixa (tinha atingido e afastou) | 3 tons descendentes |
| Rotação do encoder | Clique curto |
| Duplo clique / clique (religar) / clique longo | Confirmação |

### Atuador TPL0501 — modos de rede

Escolha **uma** topologia em `MODO_POT_REDE` (`config.h`):

| Valor | Modo | Req na saída |
|-------|------|----------------|
| `MODO_POT_UNICO` | 1× TPL0501 | R_pot |
| `MODO_POT_UNICO_PARALELO` | 1× TPL0501 + resistor fixo | R_pot ∥ R_par |
| `MODO_POT_DUPLO_SERIE` | 2× TPL0501 em série | R_A + R_B |
| `MODO_POT_DUPLO_SERIE_PARALELO` | 2× série + paralelo | (R_A + R_B) ∥ R_par |

**Modo duplo — fiação elétrica:**

- Chip A: L → GND; W → H do chip B  
- Chip B: L ← W do A; W → saída do circuito  
- GPIO: **SCLK** (GPIO 18) e **MOSI** (GPIO 23) compartilhados; **CS_A** (GPIO 5) e **CS_B** (GPIO 16) separados  

O firmware usa **escada intercalada** (nível virtual 0..510): alterna passos A↔B e busca binária em R_série monotônica — sem grid 256×256.

**Parâmetros principais (`config.h`):**

| Constante | Uso |
|-----------|-----|
| `MODO_POT_REDE` | Topologia (tabela acima) |
| `RESISTOR_PARALELO_KOHM` | Resistor em paralelo [kΩ] (modos `*_PARALELO`) |
| `POT_AFERIDO_KOHM_MAX_A` / `_B` | Aferição L–W de cada chip (passo 255) |
| `REQ_IDEAL_POTENCIA_MIN_KOHM` | Referência de **0 %** potência (ex. 150 kΩ) |
| `REF_POTENCIA_MINIMA` | `REF_POTENCIA_MIN_IDEAL` ou `REF_POTENCIA_MIN_FISICA` |
| `REQ_MAXIMA_SAIDA_KOHM` | Limite opcional de Req (`0` = sem limite extra) |
| `POT_INVERTE_SENTIDO` | `1` se passo 0/255 variar Req no sentido errado |

Com **2× TPL0501** em série, R_série máxima ≈ **192 kΩ** (2 × ~96 kΩ aferido), aproximando a escala ideal de 150 kΩ.

Teste dedicado: [`testes_hardware/teste_tpl0501/`](testes_hardware/teste_tpl0501/) — ver [`testes_hardware/README.md`](testes_hardware/README.md).

### Ajustes em `config.h` (demais)

| Constante | Uso |
|-----------|-----|
| `POT_INVERTE_SENTIDO` | `1` se a potência variar no sentido errado |
| `SERIAL_DEPURAR_MALHA` | `true`: linha `[MALHA]` a cada passo PID (SP, PV, P, I, D, OUT, PCT, POT A/B, META, ACT) |
| `LCD_ENDERECO_I2C` / `LCD_LAYOUT_YWROBOT` | Endereço e mapeamento do módulo I2C |
| `MALHA_INICIA_ATIVA` | `true` para iniciar já regulando |
| `FILTRO_TEMP_AMOSTRAS` | Amostras na média móvel da temperatura antes do PID |

### TPL0501 (calibração no firmware)

Constantes de aferição em `config.h`: passo **255** ≈ **96–97 kΩ** por chip (estimativa datasheet; recalibre com multímetro L–W via `teste_tpl0501`). O registrador WR é escrito diretamente via SPI.

## Segurança

Controle na **linha de potência** do chuveiro exige projeto elétrico adequado (DR, terra, isolamento). Este firmware **não substitui** proteções de segurança.

Em **falha do sensor** ou leitura inválida: wiper no **mínimo**, PID reiniciado, mensagem no LCD e serial.

## Testes em bancada (firmware principal)

1. LCD — splash *Controle Temperatura* / *ESP32 + PID*; depois linhas de operação.
2. DS18B20 — temperatura coerente na linha *Atual*.
3. Encoder — limites 10–45 °C, passos fino/grosso, beeps.
4. Duplo clique — standby (pot. 0 %); clique simples religa com OUT preservada.
5. TPL0501 — `%` na linha 4 acompanha o PID (e `[MALHA]` na serial se debug ativo).
6. Malha fechada — carga térmica controlada (água); buzzer na entrada/saída da faixa do alvo.
