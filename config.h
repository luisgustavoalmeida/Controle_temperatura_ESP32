/**
 * config.h — Constantes globais do firmware
 *
 * Controle de temperatura do chuveiro elétrico via ESP32 (NodeMCU-32S).
 * A malha PID ajusta a potência (0..100 %) via RobotDyn AC Light Dimmer (TRIAC).
 *
 * Onde alterar no dia a dia:
 *   • PINO_DIMMER_ZC / PINO_DIMMER_PSM — fiação do módulo dimmer
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

/** RobotDyn — disparo TRIAC (PSM / DIMMER). */
#define PINO_DIMMER_PSM        18
/** RobotDyn — detecção zero-cross (ZC / ZERO-C). */
#define PINO_DIMMER_ZC         5

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
#define PID_GANHO_KP           0.04f
/** Ganho integral — corrige erro persistente (cuidado com overshoot). */
#define PID_GANHO_KI           0.0013f
/** Ganho derivativo — amortecimento ante mudanças rápidas de PV. */
#define PID_GANHO_KD           0.375f

/** Limites da saída do PID: 0 = potência mínima, 1 = potência máxima. */
#define PID_SAIDA_MIN          0.0f
#define PID_SAIDA_MAX          1.0f

/** Faixa permitida do alvo de temperatura no encoder [°C]. */
#define ALVO_TEMP_MIN_C        10.0f
#define ALVO_TEMP_MAX_C        45.0f
/** Alvo exibido e aplicado ao ligar o equipamento [°C]. */
#define ALVO_TEMP_PADRAO_C     40.0f

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

/** Estabilização elétrica após borda do botão (ISR + debounce) [ms]. */
#define ENCODER_BOTAO_DEBOUNCE_MS      20
/** Janela máxima entre dois cliques para contar como duplo clique [ms]. */
#define ENCODER_DUPLO_CLIQUE_MS        500
/** Duração mínima de pressão válida (abaixo = repique mecânico) [ms]. */
#define ENCODER_CLIQUE_MIN_MS          20
/** Duração máxima de um clique curto (acima disso = longo) [ms]. */
#define ENCODER_CLIQUE_MAX_MS          450
/** Segurar e soltar (sem girar) reinicia o PID no modo temperatura [ms]. */
#define ENCODER_CLIQUE_LONGO_MS        800
/** Segurar o botão sem girar por este tempo troca PID ↔ potência [ms]. */
#define ENCODER_TROCA_MODO_MS          3000

/**
 * Ao religar da standby com clique: reinicia o PID se
 * (alvo − temperatura) > este valor [°C] (água muito fria).
 */
#define STANDBY_RELIGA_REINICIA_DELTA_C  3.0f

/**
 * Faixa em torno do alvo para aviso sonoro, piscar do backlight e texto “Temp OK” [°C].
 * NÃO pausa o PID nem o dimmer — só buzzer e display.
 */
#define BUZZER_HISTERESE_C     0.2f

/**
 * true  = malha PID já ativa no boot (chuveiro regulando).
 * false = inicia em standby; clique no encoder para ligar (padrão seguro).
 */
#define MALHA_INICIA_ATIVA     false

// ===========================================================================
// 3b. Modo potência manual (encoder ajusta saída direta, sem PID)
//     100 % = potência máxima no dimmer | 0 % = mínima / off
// ===========================================================================

/** Faixa do alvo de potência no encoder [%]. */
#define ALVO_POT_MIN_PCT        0.0f
#define ALVO_POT_MAX_PCT        100.0f
/** Alvo exibido ao entrar no modo potência [%]. */
#define ALVO_POT_PADRAO_PCT     0.0f

/** Incremento por detente — giro normal: ±1 na parte inteira, mantém décimos [%]. */
#define ALVO_POT_PASSO_PCT      1.0f
/** Incremento com botão + giro: décimos (75,1 → 75,2 → 75,3) [%]. */
#define ALVO_POT_PASSO_FINO_PCT 0.1f

// ===========================================================================
// 4. RobotDyn AC Light Dimmer (rbdimmerESP32 — requer Arduino-ESP32 3.x)
// ===========================================================================

/** Limite fixo da rbdimmerESP32: niveis 1..2 nao disparam o TRIAC (delay = 0). */
#define DIMMER_BIBLIOTECA_NIVEL_MIN    3

/**
 * Minimo quando OUT > 0 [%]. 0 = sempre OFF (standby).
 * Se DIMMER_NIVEL_MIN < 3, o firmware usa 3 (limite da biblioteca).
 */
#define DIMMER_NIVEL_MIN               0
#define DIMMER_NIVEL_MAX               98

#if DIMMER_NIVEL_MIN < DIMMER_BIBLIOTECA_NIVEL_MIN
#define DIMMER_NIVEL_MIN_EFETIVO       DIMMER_BIBLIOTECA_NIVEL_MIN
#else
#define DIMMER_NIVEL_MIN_EFETIVO       DIMMER_NIVEL_MIN
#endif

/**
 * Fase da rede (0 = monofásico). Frequência [Hz]: 60 = Brasil (padrão);
 * 50 para rede europeia; 0 = detecção automática (mais lento no boot).
 */
#define DIMMER_FASE_REDE               0
#define DIMMER_FREQUENCIA_REDE_HZ      60

/**
 * Modo de curva do dimmer (rbdimmerESP32) — como o nível 0..100 % vira ângulo de disparo.
 *
 * Escolha UMA linha ativa (comente as outras):
 *
 *   DIMMER_CURVA_LINEAR      — atraso proporcional ao nível; tensão sobe rápido no início.
 *   DIMMER_CURVA_RMS         — compensa corte de fase p/ carga resistiva (chuveiro/lâmpada).
 *   DIMMER_CURVA_LOGARITMICA — curva log; pensada p/ LED; útil p/ comparar na bancada.
 *
 * PlatformIO (sem editar config.h):
 *   pio run -e esp32dev_curva_linear -t upload
 *   pio run -e esp32dev_curva_rms     -t upload   (padrão = esp32dev)
 *   pio run -e esp32dev_curva_log     -t upload
 *
 * Nota: níveis 1..2 % são sempre OFF na biblioteca (mínimo útil ≈ 3 %).
 */
#define DIMMER_CURVA_LINEAR            0
#define DIMMER_CURVA_RMS               1
#define DIMMER_CURVA_LOGARITMICA       2

/**
 * Curva ativa (0=LINEAR, 1=RMS, 2=LOG).
 * ATENCAO: ambientes PlatformIO esp32dev_curva_* passam -D DIMMER_CURVA_TIPO=N
 * e IGNORAM esta linha. Confira no boot serial: curva_cfg= e curva_lib=.
 */
#ifndef DIMMER_CURVA_TIPO
#define DIMMER_CURVA_TIPO              DIMMER_CURVA_LINEAR
#endif
// Alternativas (descomente #undef + UMA linha; nao use com -e esp32dev_curva_*):
// #undef DIMMER_CURVA_TIPO
// #define DIMMER_CURVA_TIPO           DIMMER_CURVA_RMS
// #define DIMMER_CURVA_TIPO           DIMMER_CURVA_LOGARITMICA

#if (DIMMER_CURVA_TIPO != DIMMER_CURVA_LINEAR) && \
    (DIMMER_CURVA_TIPO != DIMMER_CURVA_RMS) && \
    (DIMMER_CURVA_TIPO != DIMMER_CURVA_LOGARITMICA)
#error DIMMER_CURVA_TIPO invalido: use DIMMER_CURVA_LINEAR, RMS ou LOGARITMICA
#endif

#if DIMMER_CURVA_TIPO == DIMMER_CURVA_RMS
#define DIMMER_CURVA_NOME              "RMS"
#elif DIMMER_CURVA_TIPO == DIMMER_CURVA_LOGARITMICA
#define DIMMER_CURVA_NOME              "LOG"
#else
#define DIMMER_CURVA_NOME              "LINEAR"
#endif

/**
 * Nivel maximo enviado ao rbdimmer quando o comando pede 100 % (nao use 100).
 * Mesmo teto para LINEAR, RMS e LOG — ajuste conforme bancada (96 = ~4 % atraso).
 *
 * Motivo: nivel 100 na LINEAR da biblioteca gera atraso 0 us (TRIAC nao dispara);
 * niveis 99-100 ficam no limite minimo do timer e podem piscar.
 */
#define DIMMER_NIVEL_PLENO_ESTAVEL     100

/**
 * Serial debug: imprime dDIM se |nível dimmer − OUT| > este limiar (0..1).
 */
#define SERIAL_TOLERANCIA_ERRO_DIM       0.005f

/**
 * Histerese opcional: só atualiza o dimmer se |OUT − OUT_anterior| > limiar.
 * 0 = aplica sempre que OUT mudar.
 */
#define DIMMER_HISTERESIS_SAIDA_ATIVA    0
#define DIMMER_HISTERESIS_SAIDA_LIMIAR   0.0f

// ---------------------------------------------------------------------------
// Calibracao: potencia linear (PID e modo potencia manual)
// ---------------------------------------------------------------------------

/**
 * 1 = OUT/alvo 0..100 %% significa potencia real linear (via tabela de bancada).
 * 0 = OUT 0..1 mapeia direto em nivel 0..DIMMER_NIVEL_MAX (sem tabela).
 */
#define DIMMER_USA_CALIBRACAO_POTENCIA_LINEAR  0

/** Potencia maxima medida na bancada com comando 100 %% [W] — referencia da tabela. */
#define DIMMER_CAL_POT_REFERENCIA_W            135.108f

/**
 * Tabela medida com DIMMER_CURVA_LINEAR, DIMMER_NIVEL_MAX e carga da bancada.
 * COMANDO [%]: percentual no encoder / saida antes da calibracao inversa.
 * POTENCIA [%]: potencia medida em %% de DIMMER_CAL_POT_REFERENCIA_W.
 * Mantenha pontos em ordem crescente de COMANDO; edite apos novos testes.
 */
#define DIMMER_CAL_POT_NUM_PONTOS              21

#define DIMMER_CAL_COMANDO_PCT_TABLE \
    0.0f,   5.0f,  10.0f,  15.0f,  20.0f,  25.0f,  30.0f,  35.0f,  40.0f,  45.0f, \
   50.0f,  55.0f,  60.0f,  65.0f,  70.0f,  75.0f,  80.0f,  85.0f,  90.0f,  95.0f, 100.0f

#define DIMMER_CAL_POTENCIA_PCT_TABLE \
    0.0f,   0.113f,   0.542f,   1.40f,   3.13f,   6.00f,  10.36f,  16.47f, \
   22.53f,  30.39f,  38.25f,  47.29f,  56.03f,  63.56f,  72.71f,  80.76f, \
   88.20f,  93.16f,  95.60f,  98.84f, 100.0f

// ===========================================================================
// 5. Sensor DS18B20 (temperatura da água)
// ===========================================================================

/**
 * Resolucao A/D do DS18B20 — escolha UMA linha (9, 10, 11 ou 12 bits).
 * Tempo de conversao; PERIODO_SENSOR_MS e PERIODO_PID_MS derivam daqui.
 *
 * | Bits | SENSOR_TEMPO_CONVERSAO_MS | Precisao tipica |
 * |------|---------------------------|-----------------|
 * |  9   |  94                       | +-0,5 C         |
 * | 10   | 188                       | +-0,25 C        |
 * | 11   | 375                       | +-0,125 C       |
 * | 12   | 750                       | +-0,0625 C      |
 */
#define SENSOR_RESOLUCAO_BITS      11

#if SENSOR_RESOLUCAO_BITS == 9
#define SENSOR_TEMPO_CONVERSAO_MS  100
#elif SENSOR_RESOLUCAO_BITS == 10
#define SENSOR_TEMPO_CONVERSAO_MS  200
#elif SENSOR_RESOLUCAO_BITS == 11
#define SENSOR_TEMPO_CONVERSAO_MS  380
#elif SENSOR_RESOLUCAO_BITS == 12
#define SENSOR_TEMPO_CONVERSAO_MS  760
#else
#error SENSOR_RESOLUCAO_BITS invalido: use 9, 10, 11 ou 12
#endif

/**
 * Amostras na média móvel da temperatura antes do PID (Controle_temperatura_ESP32.ino).
 * 3 = suaviza ruído do DS18B20; aumente se o display/PID oscilar.
 */
#define FILTRO_TEMP_AMOSTRAS       3

/**
 * Robustez da leitura DS18B20 (Controle_temperatura_ESP32.ino).
 * Falhas isoladas sao ignoradas; modo seguro e LCD so apos N falhas seguidas.
 */
#define SENSOR_FALHAS_ANTES_ERRO     4
#define SENSOR_SUCESSOS_RECUPERACAO  2
#define SENSOR_LEITURA_MIN_C         5.0f
#define SENSOR_LEITURA_MAX_C         60.0f
/** Salto maximo entre leituras consecutivas [C] — rejeita picos no barramento. */
#define SENSOR_SALTO_MAXIMO_C        6.0f
/** A cada N falhas, reescaneia o barramento 1-Wire (cabos soltos). */
#define SENSOR_REENUM_INTERVALO      5

// ===========================================================================
// 6. Períodos do loop principal [ms] e Serial
// ===========================================================================

/** Intervalo entre tentativas de leitura do DS18B20 (= tempo de conversao) [ms]. */
#define PERIODO_SENSOR_MS          SENSOR_TEMPO_CONVERSAO_MS

/**
 * Intervalo do calculo PID e atualizacao do comando ao dimmer [ms].
 * Igual ao sensor: um passo de controle por leitura nova (derivada coerente).
 */
#define PERIODO_PID_MS             PERIODO_SENSOR_MS

/** Igual ao PID — sem atraso extra entre OUT e atualizacao do dimmer. */
#define PERIODO_ATUADOR_DIMMER_MS  PERIODO_PID_MS

/** Atualização do LCD (temperatura, % potência, estados) [ms]. */
#define PERIODO_LCD_MS             100

/**
 * Com controle desligado: apaga backlight após este tempo sem interação no encoder [ms].
 */
#define LCD_BACKLIGHT_INATIVIDADE_MS  30000

/**
 * Com controle ligado: desliga automaticamente (standby, pot. 0 %) após este tempo
 * sem qualquer interação no encoder (giro, clique ou botão pressionado) [ms].
 * 40 min = 40 × 60 × 1000.
 */
#define AUTO_DESLIGA_INATIVIDADE_MS   (40UL * 60UL * 1000UL)

/** Potência elétrica equivalente a 100 % de saída do chuveiro [W]. */
#define POTENCIA_MAX_WATTS            6000.0f

/**
 * Sem pulso de zero-cross por este tempo → rede/chuveiro considerados desligados
 * para o cronômetro e a energia (60 Hz ≈ 16,7 ms/ciclo; margem ~5 ciclos).
 */
#define MEDIDOR_ZC_TIMEOUT_MS         160

/**
 * Com malha ligada: entra em standby (igual duplo clique) se o chuveiro/rede
 * permanecer sem zero-cross por este tempo [ms]. 10 min = 10 × 60 × 1000.
 */
#define STANDBY_SEM_ZC_MS             (10UL * 60UL * 1000UL)

/** Piscar backlight ao entrar/sair da meta (espelha duração do buzzer) [ms]. */
#define LCD_PISCAR_META_LIGADO_MS     120
#define LCD_PISCAR_FORA_META_LIGADO_MS 140
#define LCD_PISCAR_META_PAUSA_MS      40

/** Encoder e buzzer — polling rápido sem bloquear o loop [ms]. */
#define PERIODO_LOOP_MS            5

/** Clique do encoder — pulso curto em micros (mais seco que millis). */
#define BUZZ_CLIQUE_HZ               3000
#define BUZZ_CLIQUE_US               800UL

/** Zero-cross: tom ao detectar chuveiro ligado (rede presente). */
#define BUZZ_ZC_LIGADO_HZ            2200
#define BUZZ_ZC_LIGADO_MS            150

/** Zero-cross: tom ao detectar chuveiro desligado (rede ausente). */
#define BUZZ_ZC_DESLIGADO_HZ         900
#define BUZZ_ZC_DESLIGADO_MS         200

/** Velocidade do Monitor Serial [baud]. */
#define SERIAL_VELOCIDADE          115200

/**
 * true  = imprime linha [MALHA] a cada PERIODO_PID_MS (SP, PV, OUT, passos).
 * false = Serial silenciosa em operação normal (recomendado em produção).
 */
#define SERIAL_DEPURAR_MALHA       false

#endif // CONFIG_H
