# Controle de temperatura — ESP32 + PID + chuveiro

Firmware para **Arduino IDE** ou **PlatformIO**, em placa **ESP32** (referência de fiação: **NodeMCU-32S** / ESP-WROOM-32; na IDE use **ESP32 Dev Module**). Malha fechada PID com atuador **X9C104S** (100 kΩ), sensor **DS18B20**, display **LCD 20×4 I2C**, **encoder rotativo** e **buzzer**.

A regulagem **não pausa** quando a temperatura entra na faixa do alvo: o PID continua atuando. A histerese (`HISTERESE_BUZZER_C`) vale só para o **buzzer** e para o texto **Temp OK** no LCD.

Os ganhos PID foram calibrados no projeto irmão [`Malha_PID_temperatura`](../Malha_PID_temperatura) (portados de `pid_controller.py`):

| Parâmetro | Valor |
|-----------|-------|
| Kp | 0,032 |
| Ki | 0,002 |
| Kd | 0,015 |

Saída do PID: **0,0** (potência mínima) a **1,0** (máxima), mapeada para o passo **0..99** do X9C104S.

## Pinagem (GPIO)

Valores definidos em `config.h` (diagrama detalhado no cabeçalho do arquivo e em `imagens/Imagem pinos ESP32.png`, se existir):

| Função | GPIO | Observação |
|--------|------|------------|
| DS18B20 (DQ) | 4 | Pull-up **4,7 kΩ** entre DQ e 3,3 V |
| X9C104 CS | 5 | Pino 7 do CI |
| X9C104 INC | 16 | Pino 1 |
| X9C104 U/D | 17 | Pino 2 |
| I2C SDA (LCD) | 21 | |
| I2C SCL (LCD) | 22 | |
| Encoder CLK (A) | 25 | `INPUT_PULLUP` |
| Encoder DT (B) | 26 | |
| Encoder SW | 27 | Botão para GND ao pressionar |
| Buzzer | 32 | Ativo recomendado; (−) em GND |

**Alimentação (resumo):** DS18B20, encoder e buzzer em **3,3 V**; LCD em **5 V** ou **3,3 V** conforme o módulo; X9C104 pino 8 em **5 V**, VSS em **GND**; pinos 3/5/6 do X9C no circuito do potenciômetro do chuveiro (não são GPIO).

**LCD I2C:** endereço padrão **0x27** (`LCD_I2C_ADDR`; comum também **0x3F** — use a varredura em `teste_lcd`). Layout PCF8574 **YWROBOT** (`LCD_PCF8574_LAYOUT_YWROBOT = 1`).

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
   - `LCD_USE_NEW_LIQUIDCRYSTAL = 1` → **NewLiquidCrystal** (pasta `NewLiquidCrystal_lib` no Arduino IDE).
   - `LCD_USE_NEW_LIQUIDCRYSTAL = 0` → **LiquidCrystal I2C** (marcoschwartz / Frank de Brabander).

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

Dependências declaradas: **OneWire**, **DallasTemperature**, **LiquidCrystal I2C** (marcoschwartz). Para compilar com o mesmo LCD da Arduino IDE (**NewLiquidCrystal** + layout YWROBOT), instale a biblioteca correspondente e mantenha `LCD_USE_NEW_LIQUIDCRYSTAL = 1` em `config.h`; ou defina `LCD_USE_NEW_LIQUIDCRYSTAL = 0` e use apenas a lib do `platformio.ini`.

```bash
pio run -t upload
pio device monitor -b 115200
```

## Testes de hardware (antes do firmware principal)

Na pasta [`testes_hardware/`](testes_hardware/) há um sketch por periférico (mesmos pinos de `config.h`). Grave na Arduino IDE e acompanhe o **Monitor Serial (115200 baud)**. Detalhes em [`testes_hardware/README.md`](testes_hardware/README.md).

| Ordem | Pasta | Objetivo |
|-------|-------|----------|
| 1 | `teste_buzzer/` | Bipes |
| 2 | `teste_lcd/` | I2C + texto (layout/endereço) |
| 3 | `teste_ds18b20/` | Temperatura |
| 4 | `teste_encoder/` | Giro, botão, duplo clique |
| 5 | `teste_x9c503/` | Aferição/validação X9C104S |
| 6 | `teste_x9c104_exemplo/` | API de referência do driver |

## Estrutura do código

Todos os módulos têm comentários em português no cabeçalho e nas funções públicas.

| Arquivo | Função |
|---------|--------|
| `config.h` | Pinos, PID, setpoint, períodos, X9C104, DS18B20, LCD, serial debug |
| `Controle_temperatura_ESP32.ino` | Programa principal — `setup()` / `loop()` e tarefas periódicas |
| `pid_controller.*` | PID com limite na integral e anti-windup (back-calculation) |
| `potenciometro_map.*` | Saída PID 0..1 → passo 0..99 |
| `x9c104s.*` | Driver INC / U/D / CS; pulsos extras nos extremos (aferição) |
| `sensor_ds18b20.*` | DS18B20 assíncrono (12 bits, ~750 ms) |
| `lcd_i2c_compat.h` | Compatibilidade e scan I2C (NewLiquidCrystal / marcoschwartz) |
| `display_lcd.*` | Layout das 4 linhas e estados |
| `encoder_rotativo.*` | Setpoint e eventos (giro, clique, duplo clique) |
| `buzzer.*` | Feedback sonoro |

### Fluxo do `loop()`

O `loop()` **não usa `delay()`** (exceto splash curto no `setup()`). Quatro tarefas, cada uma no intervalo de `config.h`:

| Tarefa | Período | Conteúdo |
|--------|---------|----------|
| `tarefaInterfaceUsuario()` | 10 ms (`PERIODO_LOOP_MS`) | Encoder + buzzer |
| `tarefaLeituraSensor()` | 800 ms (`PERIODO_SENSOR_MS`) | DS18B20 — conversão em paralelo (~750 ms) |
| `tarefaMalhaPid()` | 100 ms (`PERIODO_PID_MS`) | PID + comando ao X9C104S |
| `tarefaAtualizarDisplay()` | 300 ms (`PERIODO_LCD_MS`) | LCD (cache; só redesenha se mudou) |

**Auxiliares no `.ino`:** filtro de temperatura (média móvel de **3** amostras), modo seguro se o sensor falhar, detecção de meta para buzzer, ligar/desligar malha, mensagens de transição no LCD.

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
| Girar encoder | Passo **0,25 °C** (`SETPOINT_PASSO_C`) |
| Segurar botão + girar | Passo fino **0,005 °C** (`SETPOINT_PASSO_FINO_C`) |
| Faixa | **10,0** a **45,0 °C** |
| Valor inicial | **38,0 °C** (`SETPOINT_PADRAO_C`) |

### Malha PID

| Ação | Comportamento |
|------|----------------|
| **Duplo clique** no botão | Liga/desliga a malha (`ENCODER_DUPLO_CLIQUE_MS` = 450 ms) |
| **Clique simples** (malha ligada) | Reinicia o PID (zera integral) |
| Boot | Malha **desligada** por padrão (`CONTROLE_INICIA_LIGADO = false`) — duplo clique para ligar |

Com a malha desligada: saída PID em mínimo, potenciômetro no passo 0.

### Buzzer

| Evento | Som |
|--------|-----|
| Entrou na faixa do alvo (± `HISTERESE_BUZZER_C`, default 0,2 °C) | 3 tons ascendentes |
| Saiu da faixa (tinha atingido e afastou) | 3 tons descendentes |
| Rotação do encoder | Clique curto |
| Duplo clique / clique com malha ligada | Confirmação |

### Ajustes em `config.h`

| Constante | Uso |
|-----------|-----|
| `X9C_INVERTE_DIRECAO` | `1` se a potência variar no sentido errado |
| `SERIAL_DEBUG_MALHA` | `true`: linha `[MALHA]` a cada passo PID (SP, PV, P, I, D, OUT, POT, META, ACT) |
| `LCD_I2C_ADDR` / `LCD_PCF8574_LAYOUT_YWROBOT` | Endereço e mapeamento do módulo I2C |
| `CONTROLE_INICIA_LIGADO` | `true` para iniciar já regulando |

### X9C104S (calibração no firmware)

Constantes de aferição em `config.h`: passo 99 ≈ **90,9 kΩ**; máximo físico **91,8 kΩ** (+1 pulso extra na subida); **1** pulso extra na descida no mínimo; modo **só borda de subida** (`X9C_PULSO_SO_SUBIDA = 1`).

## Segurança

Controle na **linha de potência** do chuveiro exige projeto elétrico adequado (DR, terra, isolamento). Este firmware **não substitui** proteções de segurança.

Em **falha do sensor** ou leitura inválida: wiper no **mínimo**, PID reiniciado, mensagem no LCD e serial.

## Testes em bancada (firmware principal)

1. LCD — splash *Controle Temperatura* / *ESP32 + PID*; depois linhas de operação.
2. DS18B20 — temperatura coerente na linha *Atual*.
3. Encoder — limites 10–45 °C, passos fino/grosso, beeps.
4. Duplo clique — *Controle PID OFF* → ON; potência 0 % quando off.
5. X9C104 — `%` na linha 4 acompanha o PID (e `[MALHA]` na serial se debug ativo).
6. Malha fechada — carga térmica controlada (água); buzzer na entrada/saída da faixa do alvo.
