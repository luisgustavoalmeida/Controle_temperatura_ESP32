# Controle de temperatura — ESP32 + PID + chuveiro

Firmware para **Arduino IDE** ou **PlatformIO**, em placa **ESP32** (referência de fiação: **NodeMCU-32S** / ESP-WROOM-32; na IDE use **ESP32 Dev Module**). Malha fechada PID com atuador **RobotDyn AC Light Dimmer** (corte de fase TRIAC via **rbdimmerESP32**), sensor **DS18B20**, display **LCD 20×4 I2C**, **encoder rotativo** e **buzzer**.

A regulagem **não pausa** quando a temperatura entra na faixa do alvo: o PID continua atuando. A histerese (`BUZZER_HISTERESE_C`) vale só para o **buzzer** e para o texto **Temp OK** no LCD.

Os ganhos PID foram calibrados no projeto irmão [`Malha_PID_temperatura`](../Malha_PID_temperatura) (portados de `pid_controller.py`):

| Parâmetro | Valor |
|-----------|-------|
| Kp | 0,025 |
| Ki | 0,0013 |
| Kd | 0,01 |

Saída do PID: **0,0** a **1,0** = potência no dimmer (1 = máx., 0 = mínima/off). O LCD exibe o percentual correspondente na linha 3.

## Pinagem (GPIO)

Valores definidos em `config.h` (diagrama detalhado no cabeçalho do arquivo e em `imagens/Imagem pinos ESP32.png`, se existir):

| Função | GPIO | Observação |
|--------|------|------------|
| DS18B20 (DQ) | 4 | Pull-up **4,7 kΩ** entre DQ e 3,3 V |
| Dimmer ZC (zero-cross) | 5 | Entrada de detecção de cruzamento por zero |
| Dimmer PSM (disparo) | 18 | Saída de disparo do TRIAC (gate do BTA41) |
| I2C SDA (LCD) | 21 | |
| I2C SCL (LCD) | 22 | |
| Encoder CLK (A) | 25 | `INPUT_PULLUP` |
| Encoder DT (B) | 26 | |
| Encoder SW | 27 | Botão para GND ao pressionar |
| Buzzer | 32 | Ativo recomendado; (−) em GND |

**Alimentação (resumo):** DS18B20, encoder, buzzer e módulo dimmer (lógica) em **3,3 V**; LCD em **5 V** ou **3,3 V** conforme o módulo. O dimmer atua na **linha de potência** do chuveiro (TRIAC + zero-cross) — não use GPIO direto em tensão de rede.

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

Arquivo [`platformio.ini`](platformio.ini): ambiente `esp32dev`, **pioarduino** (Arduino-ESP32 3.x), `monitor_speed = 115200`.

Dependências declaradas: **OneWire**, **DallasTemperature**, **LiquidCrystal I2C** (marcoschwartz), **rbdimmerESP32**. Para compilar com o mesmo LCD da Arduino IDE (**NewLiquidCrystal** + layout YWROBOT), instale a biblioteca correspondente e mantenha `LCD_USA_NEW_LIQUIDCRYSTAL = 1` em `config.h`; ou defina `LCD_USA_NEW_LIQUIDCRYSTAL = 0` e use apenas a lib do `platformio.ini`.

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

## Estrutura do código

Todos os módulos têm comentários em português no cabeçalho e nas funções públicas.

| Arquivo | Função |
|---------|--------|
| `config.h` | Pinos, PID, setpoint, períodos, dimmer, DS18B20, LCD, serial debug |
| `Controle_temperatura_ESP32.ino` | Programa principal — `setup()` / `loop()` e tarefas periódicas |
| `pid_controller.*` | PID com limite na integral e anti-windup (back-calculation) |
| `atuador_dimmer.*` | Atuador da malha — PID 0..1 → nível 0..100 % (rbdimmerESP32) |
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
| `tarefaMalhaPid()` | 100 ms (`PERIODO_PID_MS`) | PID + dimmer |
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
| **Duplo clique** (malha ligada) | Standby: dimmer 0 %, malha **pausada** (OUT/integral preservados) |
| **Clique simples** (malha desligada) | Religa e aplica a OUT memorizada |
| **Clique simples** (desligada e temp. muito abaixo do alvo) | Religa com `pid.reiniciar()` se `SP − PV > STANDBY_RELIGA_REINICIA_DELTA_C` (default 3 °C) |
| **Clique longo** (~800 ms, sem girar) | Reinicia o PID (`ENCODER_CLIQUE_LONGO_MS`) |
| Boot | Malha **desligada** por padrão (`MALHA_INICIA_ATIVA = false`) — **clique simples** para ligar |

Em standby o dimmer fica em 0 %; a saída da malha (`saidaPid`) permanece na RAM até religar.

### Buzzer

| Evento | Som |
|--------|-----|
| Entrou na faixa do alvo (± `BUZZER_HISTERESE_C`, default 0,2 °C) | 3 tons ascendentes |
| Saiu da faixa (tinha atingido e afastou) | 3 tons descendentes |
| Rotação do encoder | Clique curto |
| Duplo clique / clique (religar) / clique longo | Confirmação |

### Atuador dimmer (RobotDyn + rbdimmerESP32)

O PID entrega **OUT 0..1**, convertido em **0..100 %** no dimmer (`atuador_dimmer.*`). Pinos em `config.h`:

| Constante | Uso |
|-----------|-----|
| `PINO_DIMMER_ZC` | Zero-cross (GPIO 5) |
| `PINO_DIMMER_PSM` | Disparo TRIAC (GPIO 18) |
| `DIMMER_CURVA_TIPO` | `LINEAR`, `RMS` (chuveiro) ou `LOGARITMICA` |
| `DIMMER_FREQUENCIA_REDE_HZ` | `0` = detecção automática; `60` para rede fixa |
| `DIMMER_HISTERESIS_SAIDA_*` | Opcional — reduz atualizações se OUT oscilar pouco |

### Ajustes em `config.h` (demais)

| Constante | Uso |
|-----------|-----|
| `SERIAL_DEPURAR_MALHA` | `true`: linha `[MALHA]` a cada passo PID (SP, PV, P, I, D, OUT, PCT, DIM, META, ACT) |
| `LCD_ENDERECO_I2C` / `LCD_LAYOUT_YWROBOT` | Endereço e mapeamento do módulo I2C |
| `MALHA_INICIA_ATIVA` | `true` para iniciar já regulando |
| `FILTRO_TEMP_AMOSTRAS` | Amostras na média móvel da temperatura antes do PID |

## Segurança

Controle na **linha de potência** do chuveiro exige projeto elétrico adequado (DR, terra, isolamento). Este firmware **não substitui** proteções de segurança.

Em **falha do sensor** ou leitura inválida: dimmer no **mínimo**, PID reiniciado, mensagem no LCD e serial.

## Testes em bancada (firmware principal)

1. LCD — splash *Controle Temperatura* / *ESP32 + PID*; depois linhas de operação.
2. DS18B20 — temperatura coerente na linha *Atual*.
3. Encoder — limites 10–45 °C, passos fino/grosso, beeps.
4. Duplo clique — standby (dimmer 0 %); clique simples religa com OUT preservada.
5. Dimmer — `%` na linha 3 acompanha o PID (e `[MALHA]` na serial se debug ativo).
6. Malha fechada — carga térmica controlada (água); buzzer na entrada/saída da faixa do alvo.
