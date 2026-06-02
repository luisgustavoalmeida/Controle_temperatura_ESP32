/**
 * config.h — Constantes globais do controle de temperatura do chuveiro (ESP32)
 *
 * Placa: NodeMCU-32S (ESP-WROOM-32). USB na parte inferior; diagrama em
 * imagens/Imagem pinos ESP32.png (CC BY 4.0, ioxhop.com).
 *
 * ---------------------------------------------------------------------------
 * Alimentação da placa (NodeMCU-32S)
 * ---------------------------------------------------------------------------
 *   USB (micro) .......... alimenta a placa (~5 V → regulador → 3,3 V)
 *   3,3 V (barra esq.) ... saída para VCC de DS18B20, encoder e buzzer
 *   GND (barra esq./dir.)  terra comum — ligar TODOS os GND dos módulos aqui
 *   VIN (barra esq.) ...... entrada 5–12 V externa (opcional, sem USB)
 *   EN .................... não ligar (reset; só botão da placa)
 *
 * ---------------------------------------------------------------------------
 * DS18B20 (sensor 1-Wire)
 * ---------------------------------------------------------------------------
 *   VDD .................. 3,3 V
 *   GND .................. GND
 *   DQ (data) ............ GPIO 4  (ADC10, TOUCH0 — barra direita)
 *   Pull-up 1-Wire ....... resistor 4,7 kΩ entre DQ e 3,3 V (obrigatório)
 *
 * ---------------------------------------------------------------------------
 * Encoder rotativo (ex.: módulo KY-040 ou encoder mecânico + resistor)
 * ---------------------------------------------------------------------------
 *   CLK (A) .............. GPIO 25  (DAC1 — barra esquerda)
 *   DT  (B) .............. GPIO 26  (DAC2)
 *   SW  (botão) .......... GPIO 27  (TOUCH7)
 *   + / VCC .............. 3,3 V
 *   GND .................. GND
 *   COM (comum) .......... GND  ← terminal comum do eixo/botão (encoder
 *                            mecânico sem PCB); no KY-040 não há pino COM
 *                            separado — use apenas + e GND do módulo
 *   Pull-up .............. firmware usa INPUT_PULLUP em CLK, DT e SW;
 *                            módulos KY-040 já trazem resistores na PCB
 *
 * ---------------------------------------------------------------------------
 * LCD 20×4 com módulo I2C (PCF8574)
 * ---------------------------------------------------------------------------
 *   VCC .................. 5 V (módulos comuns) ou 3,3 V se o módulo for 3,3 V
 *   GND .................. GND
 *   SDA .................. GPIO 21  (I2C SDA — barra direita)
 *   SCL .................. GPIO 22  (I2C SCL)
 *   Contraste (V0) ....... trimpot do módulo (ajuste mecânico, sem GPIO)
 *   Backlight (A/K) ...... em geral já ligado no módulo I2C
 *
 * ---------------------------------------------------------------------------
 * Buzzer (ativo recomendado)
 * ---------------------------------------------------------------------------
 *   + / sinal ............ GPIO 32  (TOUCH9 — barra esquerda)
 *   − / GND .............. GND
 *   (opcional) ........... resistor 100 Ω em série no fio do sinal, se muito alto
 *
 * ---------------------------------------------------------------------------
 * X9C104S — CI 8 pinos (potenciômetro digital 100 kΩ)
 * ---------------------------------------------------------------------------
 *   Pino 1  INC ........... GPIO 16  (RX2)
 *   Pino 2  U/D ........... GPIO 17  (TX2)
 *   Pino 3  VH / RH ....... terminal alto do pot (circuito do chuveiro — não é GPIO)
 *   Pino 4  VSS ........... GND
 *   Pino 5  VW / RW ....... cursor (substitui o cursor do potenciômetro original)
 *   Pino 6  VL / RL ....... terminal baixo do pot (em geral GND do circuito)
 *   Pino 7  CS ............ GPIO 5   (VSPI SS)
 *   Pino 8  VCC ........... 5 V (alimentação do CI; níveis dos GPIO ESP são 3,3 V)
 *
 * ---------------------------------------------------------------------------
 * GPIO usados pelo firmware (resumo)
 * ---------------------------------------------------------------------------
 *   GPIO 4, 5, 16, 17, 21, 22 — barra direita | GPIO 25, 26, 27, 32 — esquerda
 *
 * Não usar: GPIO 0/2/15 (boot), GPIO 6–11 (flash), GPIO 34–39 (só entrada).
 * Corrente por GPIO: até 12 mA (recomendado 6 mA).
 *
 * Ganhos PID: projeto Malha_PID_temperatura (tuning robusto)
 */

#ifndef CONFIG_H
#define CONFIG_H

// ---------------------------------------------------------------------------
// Pinos GPIO — sinais digitais (ver cabeçalho para VCC, GND, COM e VH/VL/VW)
// ---------------------------------------------------------------------------
#define PIN_DS18B20        4   // DS18B20 DQ + pull-up 4,7 kΩ → 3,3 V
#define PIN_X9C_CS         5   // X9C104 pino 7 (CS)
#define PIN_X9C_INC        16  // X9C104 pino 1 (INC)
#define PIN_X9C_UD         17  // X9C104 pino 2 (U/D)
#define PIN_I2C_SDA        21  // LCD SDA
#define PIN_I2C_SCL        22  // LCD SCL
#define PIN_ENC_CLK        25  // encoder CLK (A)
#define PIN_ENC_DT         26  // encoder DT (B)
#define PIN_ENC_SW         27  // encoder SW (botão → GND ao pressionar)
#define PIN_BUZZER         32  // buzzer + ; buzzer − → GND

// Ligações que NÃO são GPIO (apenas referência de fiação):
//   Placa: 3,3 V, GND, (opc.) VIN | Encoder: + → 3,3 V, GND/COM → GND
//   DS18B20: VDD → 3,3 V, GND → GND | LCD: VCC → 5 V ou 3,3 V, GND → GND
//   X9C104: pino 4 VSS → GND, pino 8 VCC → 5 V; pinos 3/5/6 → circuito do pot
//   Buzzer: (−) → GND

// ---------------------------------------------------------------------------
// LCD I2C (módulo PCF8574 — LCD 20×4)
// ---------------------------------------------------------------------------
// Biblioteca no Arduino IDE:
//   1 = NewLiquidCrystal (pasta NewLiquidCrystal_lib)
//   0 = LiquidCrystal I2C por marcoschwartz / Frank de Brabander
// PlatformIO pode sobrescrever via build_flags em platformio.ini
#ifndef LCD_USE_NEW_LIQUIDCRYSTAL
#define LCD_USE_NEW_LIQUIDCRYSTAL  1
#endif

// Mapeamento do chip PCF8574 no modulo I2C (quase todos os LCD 1602/2004):
//   1 = YWROBOT / marcoschwartz (Rs=0 En=2 D4-D7=4-7 BL=3) — use este
//   0 = padrao interno NewLiquidCrystal (En=6...) — raro em modulos baratos
#define LCD_PCF8574_LAYOUT_YWROBOT  1

// Endereco I2C do PCF8574: 0x27 ou 0x3F (veja varredura no teste_lcd / jumpers A0-A2)
#define LCD_I2C_ADDR       0x27
#define LCD_COLS           20
#define LCD_ROWS           4

// ---------------------------------------------------------------------------
// PID — Malha_PID_temperatura (run_simulation.py / tuning robusto)
// ---------------------------------------------------------------------------
#define PID_KP             0.032f
#define PID_KI             0.002f
#define PID_KD             0.015f
#define PID_SAIDA_MIN      0.0f
#define PID_SAIDA_MAX      1.0f

// ---------------------------------------------------------------------------
// Setpoint do usuário (encoder)
// ---------------------------------------------------------------------------
#define SETPOINT_MIN_C     10.0f
#define SETPOINT_MAX_C     45.0f
#define SETPOINT_PADRAO_C  38.0f
#define SETPOINT_PASSO_C       0.25f   // giro normal
#define SETPOINT_PASSO_FINO_C  0.005f  // botao pressionado + giro
/** Apos parar de girar o encoder, espera este tempo para aplicar o alvo no PID */
#define SETPOINT_APLICAR_PAUSA_MS  1500

// Botao do encoder: duplo clique = standby; clique = religar; longo = reiniciar PID
#define ENCODER_DUPLO_CLIQUE_MS    450
#define ENCODER_CLIQUE_MIN_MS      50
#define ENCODER_CLIQUE_MAX_MS      450   // clique curto (ate aqui; abaixo do longo)
#define ENCODER_CLIQUE_LONGO_MS    800   // segurar sem girar
/** Ao religar com clique: reinicia PID se (alvo - temp) > este valor [C] */
#define STANDBY_RELIGA_REINICIA_DELTA_C  3.0f
#define DISPLAY_MSG_TRANSICAO_MS   2500
#define CONTROLE_INICIA_LIGADO     false

// Faixa apenas para aviso sonoro (buzzer) — NAO pausa o PID nem o potenciometro
#define HISTERESE_BUZZER_C 0.2f
#define HISTERESE_TEMP_C   HISTERESE_BUZZER_C  /* alias — codigo antigo */

// ---------------------------------------------------------------------------
// X9C104S — potenciômetro digital 100 kΩ, 100 taps (0–99)
// ---------------------------------------------------------------------------
#define X9C_NOMINAL_OHM    100000
#define X9C_PASSOS_MAX     99
// Afericao 2025: passo 99 = 90,9 kΩ; max fisico = 91,8 kΩ (+1 pulso extra SUBIDA)
#define X9C_KOHM_PASSO_99  90.9f
#define X9C_MAX_KOHM_VL_VW 91.8f
#define X9C_KOHM_POR_PASSO (X9C_KOHM_PASSO_99 / (float)X9C_PASSOS_MAX)
// Pulsos SUBIDA alem do passo 99 para atingir 91,8 kΩ (nao incrementa contador FW)
#define X9C_PULSOS_EXTRA_MAX 1
// Pulsos DESCIDA alem do passo 0 para ancorar minimo fisico (~0 kΩ)
#define X9C_PULSOS_EXTRA_MIN 1
#define X9C_PULSO_US       5
// 1 = um tap por pulso (so borda de subida) — CONFIRMADO na afericao
#define X9C_PULSO_SO_SUBIDA 1
// INC mantido em HIGH ou LOW entre taps (CS alto) para nao perder borda/contagem
// Se a potência subir no sentido errado, altere para 1
#define X9C_INVERTE_DIRECAO 0

// ---------------------------------------------------------------------------
// DS18B20 — conversão assíncrona
// ---------------------------------------------------------------------------
#define DS18_RESOLUCAO_BITS      12   // 9/10/11/12 — 12 bits ≈ 750 ms de conversão
#define DS18_TEMPO_CONVERSAO_MS  750  // tempo mínimo @ 12 bits (folha de dados)
// PERIODO_SENSOR_MS deve ser >= DS18_TEMPO_CONVERSAO_MS

// ---------------------------------------------------------------------------
// Períodos do loop principal [ms]
// ---------------------------------------------------------------------------
#define PERIODO_PID_MS     100
#define PERIODO_SENSOR_MS  800   // >= DS18_TEMPO_CONVERSAO_MS (nunca leia antes de 750 ms)
#if PERIODO_SENSOR_MS < DS18_TEMPO_CONVERSAO_MS
#warning PERIODO_SENSOR_MS menor que DS18_TEMPO_CONVERSAO_MS - leitura pode ficar invalida
#endif
#define PERIODO_LCD_MS     300
#define DISPLAY_ANIM_PID_MS  400   // frames "PID." na linha 4 ao buscar temperatura
#define PERIODO_LOOP_MS    10

// ---------------------------------------------------------------------------
// Serial debug
// ---------------------------------------------------------------------------
#define SERIAL_BAUD              115200
/** true: imprime so dados da malha a cada PERIODO_PID_MS (Monitor 115200) */
#define SERIAL_DEBUG_MALHA       true

// ---------------------------------------------------------------------------
// Organização do firmware (leitura do código)
// ---------------------------------------------------------------------------
// Controle_temperatura_ESP32.ino
//   tarefaInterfaceUsuario()  — encoder + buzzer (PERIODO_LOOP_MS)
//   tarefaLeituraSensor()     — DS18B20 (PERIODO_SENSOR_MS)
//   tarefaMalhaPid()          — PID + X9C104S (PERIODO_PID_MS)
//   tarefaAtualizarDisplay()  — LCD (PERIODO_LCD_MS)
//
// Módulos: pid_controller, potenciometro_map, x9c104s, sensor_ds18b20,
//          display_lcd, encoder_rotativo, buzzer

#endif // CONFIG_H
