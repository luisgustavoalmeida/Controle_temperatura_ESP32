/**
 * config.h — Constantes globais do firmware
 *
 * Controle de temperatura do chuveiro elétrico via ESP32 (NodeMCU-32S).
 * A malha PID ajusta a potência (0..100 %) com 1 ou 2× TPL0501 em série.
 *
 * Onde alterar no dia a dia:
 *   • MODO_POT_REDE, RESISTOR_PARALELO_KOHM — topologia elétrica
 *   • POT_AFERIDO_KOHM_MAX_A/B, REQ_IDEAL_POTENCIA_MIN_KOHM — após teste_tpl0501
 *   • PID_GANHO_* — ajuste fino da malha
 *   • MALHA_INICIA_ATIVA, SERIAL_DEPURAR_MALHA — comportamento no boot
 *
 * Fiação GPIO: imagens/Imagem pinos ESP32.png
 */

#ifndef CONFIG_H
#define CONFIG_H

// ===========================================================================
// 1. Pinos GPIO
//    Números conforme silkscreen do NodeMCU-32S (barra direita / esquerda).
// ===========================================================================

/** DQ do DS18B20 — pull-up 4,7 kΩ entre este pino e 3,3 V (obrigatório). */
#define PINO_SENSOR_TEMP       4

/** Chip select do TPL0501 A (ativo em LOW). */
#define PINO_POT_CS_A          5
/** Chip select do TPL0501 B — só usado quando POT_USA_DOIS_CHIPS = 1. */
#define PINO_POT_CS_B          16
/** Clock SPI compartilhado entre os dois TPL0501. */
#define PINO_POT_SCLK          18
/** Dados SPI (MOSI) compartilhados entre os dois TPL0501. */
#define PINO_POT_MOSI          23

/** Barramento I2C do módulo LCD (PCF8574). */
#define PINO_I2C_SDA           21
#define PINO_I2C_SCL           22

/** Encoder rotativo KY-040 ou equivalente (quadratura + botão). */
#define PINO_ENCODER_CLK       25   // fase A (CLK)
#define PINO_ENCODER_DT        26   // fase B (DT)
#define PINO_ENCODER_BOTAO     27   // SW — pressionado = LOW (INPUT_PULLUP)

/** Sinal do buzzer ativo (+); terminal (−) no GND. */
#define PINO_BUZZER            32

// ===========================================================================
// 2. LCD I2C (módulo PCF8574 — display 20 colunas × 4 linhas)
// ===========================================================================

/**
 * Biblioteca do LCD:
 *   1 = NewLiquidCrystal (Arduino IDE, layout YWROBOT)
 *   0 = LiquidCrystal I2C marcoschwartz (PlatformIO padrão em platformio.ini)
 */
#ifndef LCD_USA_NEW_LIQUIDCRYSTAL
#define LCD_USA_NEW_LIQUIDCRYSTAL  1
#endif

/** 1 = mapeamento PCF8574 dos módulos baratos (Rs, En, D4-D7, backlight). */
#define LCD_LAYOUT_YWROBOT         1

/** Endereço I2C do PCF8574 — 0x27 ou 0x3F (use teste_lcd para descobrir). */
#define LCD_ENDERECO_I2C           0x27

/** Dimensões físicas do display. */
#define LCD_COLUNAS                20
#define LCD_LINHAS                 4

/** Tempo que mensagens “Standby” / “Ligando malha” ficam na tela [ms]. */
#define LCD_MSG_TRANSICAO_MS       2500

/** Intervalo entre frames da animação “PID.” na linha 4 ao buscar temperatura [ms]. */
#define LCD_ANIM_PID_MS            400

// ===========================================================================
// 3. PID e alvo de temperatura (encoder)
// ===========================================================================

/** Ganho proporcional — reação à diferença SP − PV (°C). */
#define PID_GANHO_KP           0.025f
/** Ganho integral — corrige erro persistente (cuidado com overshoot). */
#define PID_GANHO_KI           0.0013f
/** Ganho derivativo — amortecimento ante mudanças rápidas de PV. */
#define PID_GANHO_KD           0.01f

/** Limites da saída do PID: 0 = potência mínima, 1 = potência máxima. */
#define PID_SAIDA_MIN          0.0f
#define PID_SAIDA_MAX          1.0f

/** Faixa permitida do alvo de temperatura no encoder [°C]. */
#define ALVO_TEMP_MIN_C        10.0f
#define ALVO_TEMP_MAX_C        45.0f
/** Alvo exibido e aplicado ao ligar o equipamento [°C]. */
#define ALVO_TEMP_PADRAO_C     38.0f

/** Incremento por detente do encoder — giro normal [°C]. */
#define ALVO_TEMP_PASSO_C      0.25f
/** Incremento com botão pressionado + giro (ajuste fino) [°C]. */
#define ALVO_TEMP_PASSO_FINO_C 0.01f

/**
 * Após parar de girar o encoder, espera este tempo antes de copiar
 * o alvo do LCD para o setpoint real do PID [ms].
 */
#define ALVO_TEMP_PAUSA_MS     1500

/**
 * Contagens brutas da ISR por clique mecânico do encoder.
 * KY-040 com CLK em CHANGE ≈ 2; use 1 se o serial já mostrar ±1 por detente.
 */
#define ENCODER_CONTAGENS_POR_DETENTE  2

/** Janela máxima entre dois cliques para contar como duplo clique [ms]. */
#define ENCODER_DUPLO_CLIQUE_MS        450
/** Duração mínima para aceitar um clique (debounce) [ms]. */
#define ENCODER_CLIQUE_MIN_MS          50
/** Duração máxima de um clique curto (acima disso = longo) [ms]. */
#define ENCODER_CLIQUE_MAX_MS          450
/** Segurar o botão sem girar dispara reinício do PID [ms]. */
#define ENCODER_CLIQUE_LONGO_MS        800

/**
 * Ao religar da standby com clique: reinicia o PID se
 * (alvo − temperatura) > este valor [°C] (água muito fria).
 */
#define STANDBY_RELIGA_REINICIA_DELTA_C  3.0f

/**
 * Faixa em torno do alvo para aviso sonoro e texto “Temp OK” no LCD [°C].
 * NÃO pausa o PID nem o potenciômetro — só buzzer e display.
 */
#define BUZZER_HISTERESE_C     0.2f

/**
 * true  = malha PID já ativa no boot (chuveiro regulando).
 * false = inicia em standby; clique no encoder para ligar (padrão seguro).
 */
#define MALHA_INICIA_ATIVA     false

// ===========================================================================
// 4. TPL0501 — potenciômetro digital e rede do chuveiro
// ===========================================================================

/** Resistência nominal do TPL0501-100 conforme datasheet [Ω]. */
#define POT_RESISTENCIA_NOMINAL_OHM  100000

/** Último passo válido do wiper (256 posições: 0..255). */
#define POT_PASSOS_MAX               255

/** Frequência SPI — máximo 250 kHz na folha de dados do TPL0501 [Hz]. */
#define POT_SPI_FREQUENCIA_HZ        250000

// --- Modo de ligação elétrica (escolha UMA opção em MODO_POT_REDE) ---------
//   MODO_POT_UNICO                 — 1× TPL0501
//   MODO_POT_UNICO_PARALELO        — 1× + resistor fixo em paralelo
//   MODO_POT_DUPLO_SERIE           — 2× em série (escada intercalada A↔B)
//   MODO_POT_DUPLO_SERIE_PARALELO  — 2× em série + resistor em paralelo
#define MODO_POT_UNICO                    0
#define MODO_POT_UNICO_PARALELO           1
#define MODO_POT_DUPLO_SERIE              2
#define MODO_POT_DUPLO_SERIE_PARALELO     3

/**
 * Modo ativo da rede de saída.
 * Testes PlatformIO podem definir MODO_POT_REDE_SOBRESCREVER via build_flags.
 */
#ifdef MODO_POT_REDE_SOBRESCREVER
#define MODO_POT_REDE                     MODO_POT_REDE_SOBRESCREVER
#else
#define MODO_POT_REDE                     MODO_POT_DUPLO_SERIE_PARALELO
#endif

/** Derivado: 1 se há resistor fixo em paralelo com a saída (não editar). */
#if (MODO_POT_REDE == MODO_POT_UNICO_PARALELO) \
 || (MODO_POT_REDE == MODO_POT_DUPLO_SERIE_PARALELO)
#define REDE_COM_RESISTOR_PARALELO        1
#else
#define REDE_COM_RESISTOR_PARALELO        0
#endif

/** Derivado: 1 se dois TPL0501 em série (não editar). */
#if (MODO_POT_REDE == MODO_POT_DUPLO_SERIE) \
 || (MODO_POT_REDE == MODO_POT_DUPLO_SERIE_PARALELO)
#define POT_USA_DOIS_CHIPS                1
#else
#define POT_USA_DOIS_CHIPS                0
#endif

/**
 * Resistor fixo em paralelo com a saída do chuveiro [kΩ].
 * Medir com multímetro (circuito desenergizado). Usado em modos *_PARALELO.
 */
#define RESISTOR_PARALELO_KOHM            730.0f

/**
 * Referência de 0 % de potência na escala do PID:
 *   REF_POTENCIA_MIN_IDEAL  — Req medida no multímetro (ex.: 150 kΩ)
 *   REF_POTENCIA_MIN_FISICA — máximo que o hardware alcança (passos 255/255)
 */
#define REF_POTENCIA_MIN_IDEAL            0
#define REF_POTENCIA_MIN_FISICA           1
#define REF_POTENCIA_MINIMA               REF_POTENCIA_MIN_FISICA

/**
 * Limite opcional de Req na saída [kΩ]. 0 = sem teto extra.
 * Se > 0, capa a potência mínima que o PID pode pedir.
 */
#define REQ_MAXIMA_SAIDA_KOHM             0.0f

/**
 * Modo duplo: níveis da escada intercalada chip A ↔ chip B (0..510).
 * Derivado de POT_PASSOS_MAX — não editar.
 */
#define POT_PASSOS_INTERCALADOS_MAX       (2 * POT_PASSOS_MAX)

/**
 * Serial debug: imprime dPOT se |potência física − OUT| > este limiar (0..1).
 * Usado em imprimirSerialMalha() quando SERIAL_DEPURAR_MALHA = true.
 */
#define SERIAL_TOLERANCIA_ERRO_POT        0.005f

// --- Aferição hardware (preencher com teste_tpl0501, comando l) ------------

/**
 * Resistência VL–VW medida no passo 255 de cada chip [kΩ].
 * Estimativa inicial do datasheet; recalibrar no hardware.
 */
#define POT_AFERIDO_KOHM_MAX_A             96.5f
#define POT_AFERIDO_KOHM_MAX_B             95.3f

/**
 * Req na saída da rede para 0 % de potência no multímetro [kΩ].
 * Comando ae no teste_tpl0501 (A=B=255). Não confundir com R_série interna.
 */
#define REQ_IDEAL_POTENCIA_MIN_KOHM       150.0f

/** Incremento estimado de R por passo de cada chip [kΩ] — derivado (não editar). */
#define POT_KOHM_POR_PASSO_A  (POT_AFERIDO_KOHM_MAX_A / (float)POT_PASSOS_MAX)
#define POT_KOHM_POR_PASSO_B  (POT_AFERIDO_KOHM_MAX_B / (float)POT_PASSOS_MAX)

/**
 * 1 = inverte o mapeamento lógico passo 0↔255 (use se a potência variar ao contrário).
 */
#define POT_INVERTE_SENTIDO               0

/**
 * Histerese opcional: só move o potenciômetro se |OUT − OUT_anterior| > limiar.
 * 0 = aplica sempre que OUT mudar (comportamento atual da malha).
 */
#define POT_HISTERESIS_SAIDA_ATIVA        0
#define POT_HISTERESIS_SAIDA_LIMIAR       0.0f

// ===========================================================================
// 5. Sensor DS18B20 (temperatura da água)
// ===========================================================================

/**
 * Resolução da conversão A/D: 9, 10, 11 ou 12 bits.
 * 12 bits ≈ 750 ms — PERIODO_SENSOR_MS deve ser >= SENSOR_TEMPO_CONVERSAO_MS.
 */
#define SENSOR_RESOLUCAO_BITS      12

/** Tempo mínimo de conversão @ 12 bits conforme folha de dados [ms]. */
#define SENSOR_TEMPO_CONVERSAO_MS  750

/**
 * Amostras na média móvel da temperatura antes do PID (Controle_temperatura_ESP32.ino).
 * 3 = suaviza ruído do DS18B20; aumente se o display/PID oscilar.
 */
#define FILTRO_TEMP_AMOSTRAS       3

// ===========================================================================
// 6. Períodos do loop principal [ms] e Serial
// ===========================================================================

/** Intervalo do cálculo PID e atualização do comando ao TPL0501 [ms]. */
#define PERIODO_PID_MS             100

/** Igual ao PID — sem atraso extra entre OUT e movimento do potenciômetro. */
#define PERIODO_ATUADOR_POT_MS     PERIODO_PID_MS

/** Intervalo entre leituras do DS18B20 — nunca menor que a conversão [ms]. */
#define PERIODO_SENSOR_MS          750
#if PERIODO_SENSOR_MS < SENSOR_TEMPO_CONVERSAO_MS
#warning PERIODO_SENSOR_MS menor que SENSOR_TEMPO_CONVERSAO_MS - leitura invalida
#endif

/** Atualização do LCD (temperatura, % potência, estados) [ms]. */
#define PERIODO_LCD_MS             100

/** Encoder e buzzer — polling rápido sem bloquear o loop [ms]. */
#define PERIODO_LOOP_MS            10

/** Velocidade do Monitor Serial [baud]. */
#define SERIAL_VELOCIDADE          115200

/**
 * true  = imprime linha [MALHA] a cada PERIODO_PID_MS (SP, PV, OUT, passos).
 * false = Serial silenciosa em operação normal (recomendado em produção).
 */
#define SERIAL_DEPURAR_MALHA       true

#endif // CONFIG_H
