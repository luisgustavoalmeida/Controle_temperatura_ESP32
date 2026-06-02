/**
 * Teste X9C104S — ajuste por valor na Serial
 *
 * Monitor Serial: 115200 baud | Line ending: Nova linha (NL ou NL+CR)
 *
 * Digite um valor e Enter — o wiper ajusta (pausa entre taps):
 *   40      -> VL-VW ~ 40 kOhm (passo 40, ~1 kOhm/passo com m up)
 *   25.5    -> VL-VW ~ 25,5 kOhm
 *   + / -     -> +1 / -1 passo (incremento)
 *   +5 / -3   -> +5 / -3 passos
 *   +p5 / -p50 -> igual +5 / -50 passos
 *   p50     -> passo 50 (0..99), opcional
 *   m up/down/full -> modo de pulso INC (teste de bordas)
 *   a            -> afericao guiada completa (tabela longa)
 *   v            -> validacao ancora MIN/MAX (~5 min)
 *   b max / min  -> buscar fim real com pulsos extras
 *   ?            -> ajuda
 */

#include "../../config.h"
#include "afericao_x9c.h"
#include <math.h>
#include <string.h>

#define X9C_TEST_PAUSA_MS    5
#define X9C_TEST_PULSO_US    5
#define X9C_AFERIR_PAUSA_MS  400

enum ModoPulsoInc {
  PULSO_COMPLETO = 0,  // LOW -> HIGH -> LOW (padrao)
  PULSO_SUBIDA   = 1,  // so borda de subida (CS baixo)
  PULSO_DESCIDA  = 2,  // so borda de descida (CS baixo)
};

static const char* nomeModoPulso(ModoPulsoInc m) {
  switch (m) {
    case PULSO_SUBIDA:  return "SUBIDA";
    case PULSO_DESCIDA: return "DESCIDA";
    default:            return "COMPLETO";
  }
}

static uint8_t passoAtual = 0;
static ModoPulsoInc modoPulso = PULSO_SUBIDA;
static bool incNivelAlto = false;
static bool ancoradoMin = false;
static bool ancoradoMax = false;

static AfericaoLinha aferirLog[AFERIR_MAX_LINHAS];
static int aferirCount = 0;
static bool aferirAtivo = false;

bool parseFloatBr(const String& s, float* out);
bool parseIncrementoPassos(const String& lower, int* delta);

float resistenciaVlVwOhm(uint8_t passo) {
  return (float)passo * X9C_KOHM_POR_PASSO * 1000.0f;
}

uint8_t passoDeKohm(float kohm) {
  if (kohm < 0.0f) kohm = 0.0f;
  if (kohm > X9C_MAX_KOHM_VL_VW) kohm = X9C_MAX_KOHM_VL_VW;
  uint8_t p = (uint8_t)(kohm / X9C_KOHM_POR_PASSO + 0.5f);
  if (p > X9C_PASSOS_MAX) p = X9C_PASSOS_MAX;
  return p;
}

void definirIncIdle(bool alto) {
  digitalWrite(PIN_X9C_CS, HIGH);
  digitalWrite(PIN_X9C_INC, alto ? HIGH : LOW);
  incNivelAlto = alto;
  delayMicroseconds(X9C_TEST_PULSO_US);
}

void prepararIncAntesPulso(ModoPulsoInc modo) {
  // Com CS alto, INC pode mudar de nivel sem contar tap (datasheet X9C).
  if (modo == PULSO_SUBIDA) {
    if (incNivelAlto) {
      definirIncIdle(false);
    }
  } else if (modo == PULSO_DESCIDA) {
    if (!incNivelAlto) {
      definirIncIdle(true);
    }
  } else if (incNivelAlto) {
    definirIncIdle(false);
  }
}

void aplicarDirecao(bool subir) {
#if X9C_INVERTE_DIRECAO
  digitalWrite(PIN_X9C_UD, subir ? LOW : HIGH);
#else
  digitalWrite(PIN_X9C_UD, subir ? HIGH : LOW);
#endif
}

void pulsoIncRaw(ModoPulsoInc modo) {
  switch (modo) {
    case PULSO_SUBIDA:
      digitalWrite(PIN_X9C_INC, HIGH);
      incNivelAlto = true;
      delayMicroseconds(X9C_TEST_PULSO_US);
      break;
    case PULSO_DESCIDA:
      digitalWrite(PIN_X9C_INC, LOW);
      incNivelAlto = false;
      delayMicroseconds(X9C_TEST_PULSO_US);
      break;
    default:
      digitalWrite(PIN_X9C_INC, HIGH);
      incNivelAlto = true;
      delayMicroseconds(X9C_TEST_PULSO_US);
      digitalWrite(PIN_X9C_INC, LOW);
      incNivelAlto = false;
      delayMicroseconds(X9C_TEST_PULSO_US);
      break;
  }
}

void tapHardware(bool subir, ModoPulsoInc modo) {
  prepararIncAntesPulso(modo);
  digitalWrite(PIN_X9C_CS, LOW);
  aplicarDirecao(subir);
  pulsoIncRaw(modo);
  digitalWrite(PIN_X9C_CS, HIGH);
}

void aplicarPulsosExtraMaximo() {
#if X9C_PULSOS_EXTRA_MAX > 0
  for (int i = 0; i < X9C_PULSOS_EXTRA_MAX; i++) {
    tapHardware(true, PULSO_SUBIDA);
    delay(X9C_TEST_PAUSA_MS);
  }
#endif
}

void aplicarPulsosExtraMinimo() {
#if X9C_PULSOS_EXTRA_MIN > 0
  for (int i = 0; i < X9C_PULSOS_EXTRA_MIN; i++) {
    tapHardware(false, PULSO_SUBIDA);
    delay(X9C_TEST_PAUSA_MS);
  }
#endif
}

void ancorarMinimo() {
  aplicarPulsosExtraMinimo();
  ancoradoMin = true;
  ancoradoMax = false;
}

void ancorarMaximo() {
  aplicarPulsosExtraMaximo();
  ancoradoMax = true;
  ancoradoMin = false;
}

void finalizarPosicao(uint8_t alvo) {
  if (modoPulso != PULSO_SUBIDA) {
    return;
  }
  if (alvo == 0) {
    ancorarMinimo();
  } else if (alvo == X9C_PASSOS_MAX) {
    ancorarMaximo();
  } else {
    ancoradoMin = false;
    ancoradoMax = false;
  }
}

/**
 * Tap subida com ancoragem (so modo SUBIDA).
 * 1o + com ancora MIN: desancora, passo permanece 0.
 */
bool tapSubirContado() {
  if (modoPulso != PULSO_SUBIDA) {
    tapHardware(true, modoPulso);
    if (passoAtual < X9C_PASSOS_MAX) {
      passoAtual++;
    }
    return true;
  }

  if (ancoradoMin) {
    tapHardware(true, PULSO_SUBIDA);
    ancoradoMin = false;
    return false;
  }

  tapHardware(true, PULSO_SUBIDA);
  if (passoAtual < X9C_PASSOS_MAX) {
    passoAtual++;
  }
  ancoradoMax = false;
  return true;
}

/**
 * Tap descida com ancoragem (so modo SUBIDA).
 * 1o - com ancora MAX: desancora, passo permanece 99.
 */
bool tapDescerContado() {
  if (modoPulso != PULSO_SUBIDA) {
    tapHardware(false, modoPulso);
    if (passoAtual > 0) {
      passoAtual--;
    }
    return true;
  }

  if (ancoradoMax) {
    tapHardware(false, PULSO_SUBIDA);
    ancoradoMax = false;
    return false;
  }

  tapHardware(false, PULSO_SUBIDA);
  if (passoAtual > 0) {
    passoAtual--;
  }
  ancoradoMin = false;
  return true;
}

void pulsoInc() {
  pulsoIncRaw(modoPulso);
}

void moverUmPasso(bool subir) {
  tapHardware(subir, modoPulso);
}

void umPasso(bool subir) {
  if (subir) {
    tapSubirContado();
  } else {
    tapDescerContado();
  }
}

const char* nomeAncora() {
  if (ancoradoMin) return "MIN";
  if (ancoradoMax) return "MAX";
  return "-";
}

void imprimirEstado() {
  float rVlVw = resistenciaVlVwOhm(passoAtual) / 1000.0f;
  Serial.printf("[OK] passo %u/99 | VL-VW ~ %.2f kOhm | INC %s | ancora %s\n",
                passoAtual, rVlVw, incNivelAlto ? "HIGH" : "LOW", nomeAncora());
}

void reiniciarMinimo() {
  Serial.println("[INFO] Homing -> passo 0 (+ ancora MIN)");
  ModoPulsoInc salvo = modoPulso;
  modoPulso = PULSO_SUBIDA;
  for (int i = 0; i <= X9C_PASSOS_MAX; i++) {
    tapHardware(false, PULSO_SUBIDA);
    delay(10);
  }
  passoAtual = 0;
  definirIncIdle(false);
  ancorarMinimo();
  modoPulso = salvo;
}

void irParaPasso(uint8_t alvo) {
  if (alvo > X9C_PASSOS_MAX) {
    alvo = X9C_PASSOS_MAX;
  }

  if (alvo == passoAtual) {
    if (modoPulso == PULSO_SUBIDA) {
      if (alvo == 0 && !ancoradoMin) {
        finalizarPosicao(0);
      } else if (alvo == X9C_PASSOS_MAX && !ancoradoMax) {
        finalizarPosicao(X9C_PASSOS_MAX);
      }
    }
    imprimirEstado();
    return;
  }

  Serial.printf("[INFO] Ajustando passo %u -> %u ...\n", passoAtual, alvo);

  while (passoAtual < alvo) {
    tapSubirContado();
    delay(X9C_TEST_PAUSA_MS);
  }
  while (passoAtual > alvo) {
    tapDescerContado();
    delay(X9C_TEST_PAUSA_MS);
  }

  finalizarPosicao(alvo);
  imprimirEstado();
}

void incrementarPassos(int delta) {
  if (delta == 0) {
    return;
  }

  bool eraMin = ancoradoMin;
  bool eraMax = ancoradoMax;
  uint8_t passoInicio = passoAtual;

  Serial.printf("[INFO] Incremento %+d | pulso %s\n", delta, nomeModoPulso(modoPulso));

  if (delta > 0) {
    for (int i = 0; i < delta; i++) {
      tapSubirContado();
      delay(X9C_TEST_PAUSA_MS);
    }
  } else {
    for (int i = 0; i < -delta; i++) {
      tapDescerContado();
      delay(X9C_TEST_PAUSA_MS);
    }
  }

  if (delta > 0 && eraMin && passoAtual == passoInicio) {
    Serial.println("[INFO] 1o + no MIN: pulso saida (passo ainda 0).");
  } else if (delta < 0 && eraMax && passoAtual == passoInicio) {
    Serial.println("[INFO] 1o - no MAX: pulso saida (passo ainda 99).");
  }

  imprimirEstado();
}

void ajustarKohm(float kohm) {
  uint8_t alvo = passoDeKohm(kohm);
  Serial.printf("[INFO] Valor pedido: %.2f kOhm\n", kohm);
  irParaPasso(alvo);
}

bool aguardarEnter(const char* msg) {
  Serial.println(msg);
  Serial.println("[INFO] Meça VL-VW e pressione Enter para continuar...");
  while (!Serial.available()) {
    delay(50);
  }
  while (Serial.available()) Serial.read();
  return true;
}

void executarNPulsosModo(bool subir, ModoPulsoInc modo, int n) {
  for (int i = 0; i < n; i++) {
    tapHardware(subir, modo);
    if (subir) {
      if (passoAtual < X9C_PASSOS_MAX) passoAtual++;
    } else if (passoAtual > 0) {
      passoAtual--;
    }
    delay(X9C_TEST_PAUSA_MS);
  }
}

void testeRapidoPulsos(int n) {
  if (n <= 0) n = 5;
  if (n > 20) n = 20;

  uint8_t inicio = passoAtual;
  Serial.println();
  Serial.printf("[TESTE] %d pulso(s) modo %s a partir do passo %u\n",
                n, nomeModoPulso(modoPulso), inicio);
  Serial.printf("[TESTE] Delta estimado firmware: %.1f kOhm\n",
                (float)n * X9C_KOHM_POR_PASSO);

  executarNPulsosModo(true, modoPulso, n);

  Serial.printf("[TESTE] Passo %u -> %u | VL-VW ~ %.2f kOhm\n",
                inicio, passoAtual,
                resistenciaVlVwOhm(passoAtual) / 1000.0f);
  Serial.println("[TESTE] Se pulso COMPLETO mover ~2x mais que SUBIDA/DESCIDA,");
  Serial.println("        cada borda INC conta um tap (~2 kOhm no total).");
}

void testeComparativoBordas() {
  const int n = 5;
  ModoPulsoInc modoSalvo = modoPulso;

  Serial.println();
  Serial.println("========================================");
  Serial.println("  TESTE COMPARATIVO — BORDAS INC");
  Serial.println("  Hipotese: pulso completo = subida + descida");
  Serial.println("  Meça VL-VW (pinos 6-5) apos cada fase.");
  Serial.println("========================================");

  reiniciarMinimo();
  imprimirEstado();
  aguardarEnter("[FASE 0] Referencia em passo 0.");

  Serial.printf("[FASE 1] %d pulsos so SUBIDA (LOW->HIGH, CS baixo)\n", n);
  modoPulso = PULSO_SUBIDA;
  executarNPulsosModo(true, PULSO_SUBIDA, n);
  imprimirEstado();
  Serial.printf("[FASE 1] Esperado se 1 borda = 1 tap: ~%.0f kOhm\n",
                (float)n * X9C_KOHM_POR_PASSO);
  aguardarEnter("[FASE 1] Anote a resistencia medida.");

  reiniciarMinimo();
  Serial.printf("[FASE 2] %d pulsos so DESCIDA (HIGH->LOW, CS baixo, mesma dir.)\n", n);
  modoPulso = PULSO_DESCIDA;
  executarNPulsosModo(true, PULSO_DESCIDA, n);
  imprimirEstado();
  Serial.printf("[FASE 2] Esperado se 1 borda = 1 tap: ~%.0f kOhm\n",
                (float)n * X9C_KOHM_POR_PASSO);
  aguardarEnter("[FASE 2] Anote a resistencia medida.");

  reiniciarMinimo();
  Serial.printf("[FASE 3] %d pulsos COMPLETOS (subida + descida)\n", n);
  modoPulso = PULSO_COMPLETO;
  passoAtual = 0;
  executarNPulsosModo(true, PULSO_COMPLETO, n);
  imprimirEstado();
  Serial.printf("[FASE 3] Se ambas bordas contam: ~%.0f kOhm\n",
                (float)n * 2.0f * X9C_KOHM_POR_PASSO);
  Serial.printf("[FASE 3] Se so 1 borda conta: ~%.0f kOhm\n",
                (float)n * X9C_KOHM_POR_PASSO);
  aguardarEnter("[FASE 3] Anote a resistencia medida.");

  Serial.println();
  Serial.println("[RESULTADO] Compare as 3 medicoes:");
  Serial.println("  Confirmado: subida ~0,9 k e descida ~0,9 k -> ambas bordas contam.");
  Serial.println("  Pulso completo ~2,8 k -> evite; use 'm up' (padrao) no dia a dia.");
  Serial.println("  Com m up, +1 deve mover ~0,18 k no inicio (nao linear no curso).");

  modoPulso = modoSalvo;
  Serial.printf("[INFO] Modo pulso restaurado: %s\n", nomeModoPulso(modoPulso));
}

bool definirModoPulso(const String& lower);

void aferirLimparLog() {
  aferirCount = 0;
}

void aferirRegistrar(const char* id, const char* secao, const char* cmd) {
  if (aferirCount >= AFERIR_MAX_LINHAS) return;
  AfericaoLinha* L = &aferirLog[aferirCount++];
  strncpy(L->id, id, sizeof(L->id) - 1);
  L->id[sizeof(L->id) - 1] = '\0';
  strncpy(L->secao, secao, sizeof(L->secao) - 1);
  L->secao[sizeof(L->secao) - 1] = '\0';
  strncpy(L->cmd, cmd, sizeof(L->cmd) - 1);
  L->cmd[sizeof(L->cmd) - 1] = '\0';
  L->passoFw = passoAtual;
  L->estKohm = resistenciaVlVwOhm(passoAtual) / 1000.0f;
  L->medKohm = MEDIDA_NAO_INFORMADA;
}

void aferirImprimirCabecalho() {
  Serial.println();
  Serial.println("| ID  | Secao        | Comando      | Passo | Est kOhm | Medido kOhm |");
  Serial.println("|-----|--------------|--------------|-------|----------|-------------|");
}

void aferirImprimirLinha(const AfericaoLinha* L) {
  if (L->medKohm != MEDIDA_NAO_INFORMADA) {
    Serial.printf("| %-3s | %-12s | %-12s | %5u | %8.2f | %11.2f |\n",
                  L->id, L->secao, L->cmd, L->passoFw, L->estKohm, L->medKohm);
  } else {
    Serial.printf("| %-3s | %-12s | %-12s | %5u | %8.2f | ___________ |\n",
                  L->id, L->secao, L->cmd, L->passoFw, L->estKohm);
  }
}

void aferirImprimirTabelaCompleta() {
  Serial.println();
  Serial.println("========== TABELA AFERICAO — COPIE PARA O CHAT ==========");
  Serial.println("Multimetro: VL-VW (pinos 6-5) | Modo pulso: m up salvo apos teste");
  aferirImprimirCabecalho();
  for (int i = 0; i < aferirCount; i++) {
    aferirImprimirLinha(&aferirLog[i]);
  }
  Serial.println("=========================================================");
  Serial.println("Preencha Medido kOhm onde estiver em branco e cole Serial + tabela.");
}

float aferirAguardarMedida(const char* id) {
  Serial.println();
  Serial.printf("[MEDIR %s] Passo FW=%u | Est=%.2f kOhm | INC=%s | ancora %s\n",
                id, passoAtual, resistenciaVlVwOhm(passoAtual) / 1000.0f,
                incNivelAlto ? "HIGH" : "LOW", nomeAncora());
  Serial.println("  Digite kOhm medido (ex: 45,5) + Enter");
  Serial.println("  Enter vazio = pular | q = abortar afericao");
  Serial.print("  > ");

  while (!Serial.available()) {
    delay(50);
  }

  String s = Serial.readStringUntil('\n');
  s.trim();
  Serial.println(s);

  if (s.length() == 0) return MEDIDA_NAO_INFORMADA;
  if (s.equalsIgnoreCase("q") || s.equalsIgnoreCase("sair")) {
    return -999.0f;
  }

  float v = 0.0f;
  if (parseFloatBr(s, &v)) return v;
  Serial.println("[AVISO] Numero invalido — medida ignorada.");
  return MEDIDA_NAO_INFORMADA;
}

bool aferirRegistrarMedicao(int idx, float med) {
  if (idx < 0 || idx >= aferirCount) return false;
  aferirLog[idx].medKohm = med;
  aferirImprimirLinha(&aferirLog[idx]);
  return true;
}

bool aferirAposComando(const char* id, const char* secao, const char* cmd) {
  delay(X9C_AFERIR_PAUSA_MS);
  aferirRegistrar(id, secao, cmd);
  int idx = aferirCount - 1;
  aferirImprimirLinha(&aferirLog[idx]);

  float med = aferirAguardarMedida(id);
  if (med == -999.0f) return false;
  if (med != MEDIDA_NAO_INFORMADA) {
    aferirRegistrarMedicao(idx, med);
  }
  return true;
}

void aferirIrPasso(uint8_t alvo) {
  irParaPasso(alvo);
  delay(X9C_AFERIR_PAUSA_MS);
}

void testeAfericaoCompleta() {
  aferirAtivo = true;
  ModoPulsoInc modoSalvo = modoPulso;
  modoPulso = PULSO_SUBIDA;
  aferirLimparLog();

  Serial.println();
  Serial.println("############################################################");
  Serial.println("#  AFERICAO X9C104S — preencha Medido kOhm a cada passo   #");
  Serial.println("#  Multimetro em VL-VW (pinos 6-5)                       #");
  Serial.println("#  Enter = pular medida | q = abortar                     #");
  Serial.println("############################################################");
  aferirImprimirCabecalho();

  // --- Secao 1: escala linear (modo SUBIDA) ---
  reiniciarMinimo();
  if (!aferirAposComando("S00", "escala", "h")) goto fim;

  {
    const uint8_t pontos[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 99};
    for (unsigned i = 0; i < sizeof(pontos); i++) {
      char id[5];
      char cmd[14];
      snprintf(id, sizeof(id), "S%02u", i + 1);
      snprintf(cmd, sizeof(cmd), "p%u", pontos[i]);
      aferirIrPasso(pontos[i]);
      if (!aferirAposComando(id, "escala", cmd)) goto fim;
    }
  }

  // --- Secao 2: passar do fim (p99 + pulsos extras) ---
  aferirIrPasso(99);
  if (!aferirAposComando("F00", "fim+", "p99 base")) goto fim;

  incrementarPassos(1);
  if (!aferirAposComando("F01", "fim+", "+1 extra")) goto fim;
  incrementarPassos(1);
  if (!aferirAposComando("F02", "fim+", "+1 extra")) goto fim;
  incrementarPassos(3);
  if (!aferirAposComando("F03", "fim+", "+3 extra")) goto fim;
  incrementarPassos(5);
  if (!aferirAposComando("F04", "fim+", "+5 extra")) goto fim;
  incrementarPassos(10);
  if (!aferirAposComando("F05", "fim+", "+10 extra")) goto fim;

  // --- Secao 3: passar do inicio (h + pulsos para baixo) ---
  reiniciarMinimo();
  if (!aferirAposComando("I00", "ini-", "h ref")) goto fim;
  incrementarPassos(-1);
  if (!aferirAposComando("I01", "ini-", "-1 extra")) goto fim;
  incrementarPassos(-1);
  if (!aferirAposComando("I02", "ini-", "-1 extra")) goto fim;
  incrementarPassos(-3);
  if (!aferirAposComando("I03", "ini-", "-3 extra")) goto fim;
  incrementarPassos(-5);
  if (!aferirAposComando("I04", "ini-", "-5 extra")) goto fim;

  // --- Secao 4: bordas INC (5 pulsos cada, desde zero) ---
  {
    const struct {
      ModoPulsoInc modo;
      const char* idRef;
      const char* idPul;
      const char* secao;
    } bordas[] = {
      {PULSO_SUBIDA,   "B00", "B01", "borda-UP"},
      {PULSO_DESCIDA,  "D00", "D01", "borda-DN"},
      {PULSO_COMPLETO, "C00", "C01", "borda-FULL"},
    };

    for (unsigned t = 0; t < 3; t++) {
      reiniciarMinimo();
      if (!aferirAposComando(bordas[t].idRef, bordas[t].secao, "h ref")) goto fim;

      modoPulso = bordas[t].modo;
      executarNPulsosModo(true, bordas[t].modo, 5);
      char cmd[14];
      snprintf(cmd, sizeof(cmd), "5x %s", nomeModoPulso(bordas[t].modo));
      if (!aferirAposComando(bordas[t].idPul, bordas[t].secao, cmd)) goto fim;
    }
    modoPulso = PULSO_SUBIDA;
  }

  // --- Secao 5: reversibilidade ---
  aferirIrPasso(50);
  if (!aferirAposComando("R00", "reversao", "p50")) goto fim;
  incrementarPassos(-10);
  if (!aferirAposComando("R01", "reversao", "-10")) goto fim;
  incrementarPassos(10);
  if (!aferirAposComando("R02", "reversao", "+10 volta")) goto fim;
  incrementarPassos(-5);
  if (!aferirAposComando("R03", "reversao", "-5")) goto fim;
  incrementarPassos(5);
  if (!aferirAposComando("R04", "reversao", "+5 volta")) goto fim;

  // --- Secao 6: caça ao fim exato (95..99 + extras) ---
  {
    const uint8_t fim[] = {95, 96, 97, 98, 99};
    for (unsigned i = 0; i < sizeof(fim); i++) {
      char id[5];
      char cmd[14];
      snprintf(id, sizeof(id), "X%02u", i);
      snprintf(cmd, sizeof(cmd), "p%u", fim[i]);
      aferirIrPasso(fim[i]);
      if (!aferirAposComando(id, "fim-exato", cmd)) goto fim;
    }
  }
  incrementarPassos(1);
  if (!aferirAposComando("X05", "fim-exato", "+1 apos p99")) goto fim;
  incrementarPassos(2);
  if (!aferirAposComando("X06", "fim-exato", "+2 apos p99")) goto fim;

  // --- Secao 7: homing e contagem (+p99 desde zero) ---
  reiniciarMinimo();
  if (!aferirAposComando("H00", "contagem", "h ref")) goto fim;
  incrementarPassos(99);
  if (!aferirAposComando("H01", "contagem", "+p99 desde 0")) goto fim;
  incrementarPassos(1);
  if (!aferirAposComando("H02", "contagem", "+1 apos +p99")) goto fim;

fim:
  modoPulso = modoSalvo;
  aferirAtivo = false;
  aferirImprimirTabelaCompleta();
  Serial.println("[INFO] Afericao concluida. Prompt interativo restaurado.");
}

void buscarLimite(bool buscarMaximo) {
  Serial.println();
  Serial.printf("[BUSCAR %s] Pulsos extras com contador FW limitado 0..99\n",
                buscarMaximo ? "MAX" : "MIN");
  Serial.println("  + / -     -> 1 pulso");
  Serial.println("  +5 / -5   -> 5 pulsos");
  Serial.println("  p50       -> ir ao passo");
  Serial.println("  h         -> homing");
  Serial.println("  t         -> imprimir linha na tabela");
  Serial.println("  q         -> sair");

  if (buscarMaximo) {
    aferirIrPasso(99);
  } else {
    reiniciarMinimo();
  }
  imprimirEstado();

  while (true) {
    Serial.print("[buscar]> ");
    while (!Serial.available()) delay(50);
    String entrada = Serial.readStringUntil('\n');
    entrada.trim();
    if (entrada.length() == 0) continue;
    Serial.println(entrada);

    String lower = entrada;
    lower.toLowerCase();

    if (lower == "q" || lower == "sair") break;
    if (lower == "h") { reiniciarMinimo(); imprimirEstado(); continue; }
    if (lower == "t") {
      Serial.printf("| BUS | %-8s | passo %u | est %.2f kOhm | med _____ |\n",
                    buscarMaximo ? "fim" : "ini", passoAtual,
                    resistenciaVlVwOhm(passoAtual) / 1000.0f);
      continue;
    }
    if (lower.startsWith("p") && lower.length() > 1) {
      int p = lower.substring(1).toInt();
      if (p >= 0 && p <= (int)X9C_PASSOS_MAX) {
        irParaPasso((uint8_t)p);
        continue;
      }
    }
    int delta = 0;
    if (parseIncrementoPassos(lower, &delta)) {
      incrementarPassos(delta);
      continue;
    }
    Serial.println("[ERRO] Use +, -, +5, p50, h, t, q");
  }
  Serial.println("[INFO] Busca encerrada.");
}

void testeValidacaoRapida() {
  ModoPulsoInc modoSalvo = modoPulso;
  modoPulso = PULSO_SUBIDA;
  aferirLimparLog();

  Serial.println();
  Serial.println("========== VALIDACAO ANCORAGEM (v) ==========");
  Serial.println("Multimetro VL-VW (pinos 6-5) | Modo SUBIDA");
  Serial.println("Referencia esperada:");
  Serial.println("  V00 h       -> 0 kOhm | ancora MIN");
  Serial.println("  V01 + (1o)  -> 0 kOhm | ancora - (saida MIN, passo 0)");
  Serial.println("  V02 + (2o)  -> ~0,9 kOhm | passo 1");
  Serial.println("  V03 p50     -> ~45,5 kOhm");
  Serial.println("  V04 p99     -> ~91,8 kOhm | ancora MAX");
  Serial.println("  V05 - (1o)  -> ~91,8 kOhm | ancora - (saida MAX, passo 99)");
  Serial.println("  V06 - (2o)  -> passo 98, resistencia desce");
  Serial.println("  V07 h       -> 0 kOhm | ancora MIN");
  Serial.println("  V08 p50 -10 +10 -> reversao ~45,5 kOhm");
  aferirImprimirCabecalho();

  reiniciarMinimo();
  if (!aferirAposComando("V00", "ancora", "h ancora MIN")) goto fimValidar;

  incrementarPassos(1);
  if (!aferirAposComando("V01", "ancora", "+1o saida MIN")) goto fimValidar;

  incrementarPassos(1);
  if (!aferirAposComando("V02", "ancora", "+2o passo 1")) goto fimValidar;

  aferirIrPasso(50);
  if (!aferirAposComando("V03", "ancora", "p50")) goto fimValidar;

  aferirIrPasso(99);
  if (!aferirAposComando("V04", "ancora", "p99 ancora MAX")) goto fimValidar;

  incrementarPassos(-1);
  if (!aferirAposComando("V05", "ancora", "-1o saida MAX")) goto fimValidar;

  incrementarPassos(-1);
  if (!aferirAposComando("V06", "ancora", "-2o passo 98")) goto fimValidar;

  reiniciarMinimo();
  if (!aferirAposComando("V07", "ancora", "h ancora MIN")) goto fimValidar;

  aferirIrPasso(50);
  if (!aferirAposComando("V08a", "reversao", "p50")) goto fimValidar;

  incrementarPassos(-10);
  if (!aferirAposComando("V08b", "reversao", "-10")) goto fimValidar;

  incrementarPassos(10);
  if (!aferirAposComando("V08c", "reversao", "+10 volta")) goto fimValidar;

fimValidar:
  modoPulso = modoSalvo;
  aferirImprimirTabelaCompleta();
  Serial.println("[INFO] Validacao concluida. Cole a tabela no chat.");
}

bool definirModoPulso(const String& lower) {
  if (lower == "m" || lower == "modo" || lower == "mode") {
    Serial.printf("[INFO] Modo pulso atual: %s\n", nomeModoPulso(modoPulso));
    Serial.println("  m full | m up | m down");
    return true;
  }
  if (lower == "m full" || lower == "m completo" || lower == "m c") {
    modoPulso = PULSO_COMPLETO;
    Serial.println("[INFO] Modo pulso: COMPLETO (subida + descida)");
    return true;
  }
  if (lower == "m up" || lower == "m subida" || lower == "m u") {
    modoPulso = PULSO_SUBIDA;
    Serial.println("[INFO] Modo pulso: SUBIDA (so borda LOW->HIGH)");
    return true;
  }
  if (lower == "m down" || lower == "m descida" || lower == "m d") {
    modoPulso = PULSO_DESCIDA;
    Serial.println("[INFO] Modo pulso: DESCIDA (so borda HIGH->LOW)");
    return true;
  }
  return false;
}

bool parseIncrementoPassos(const String& lower, int* delta) {
  if (lower == "+" || lower == "++") {
    *delta = 1;
    return true;
  }
  if (lower == "-") {
    *delta = -1;
    return true;
  }
  if (lower.length() < 2) return false;

  char sinal = lower.charAt(0);
  if (sinal != '+' && sinal != '-') return false;

  unsigned pos = 1;
  if (pos < lower.length() && lower.charAt(pos) == 'p') {
    pos++;
  }
  if (pos >= lower.length()) return false;

  for (unsigned i = pos; i < lower.length(); i++) {
    if (!isDigit((unsigned char)lower.charAt(i))) return false;
  }

  int n = lower.substring(pos).toInt();
  *delta = (sinal == '+') ? n : -n;
  return true;
}

String normalizarNumero(String s) {
  s.trim();
  s.replace(',', '.');
  return s;
}

bool parseFloatBr(const String& s, float* out) {
  String n = normalizarNumero(s);
  if (n.length() == 0) return false;

  bool ok = true;
  bool ponto = false;
  for (unsigned i = 0; i < n.length(); i++) {
    char c = n.charAt(i);
    if (c == '.' && !ponto) {
      ponto = true;
      continue;
    }
    if (!isDigit((unsigned char)c)) {
      ok = false;
      break;
    }
  }
  if (!ok) return false;

  *out = n.toFloat();
  return true;
}

void imprimirAjuda() {
  Serial.println();
  Serial.println("Digite um valor e Enter:");
  Serial.printf("  40 ou 40.5   resistencia VL-VW em kOhm (0 a %.0f, ~%.1f kOhm/passo)\n",
                (float)X9C_PASSOS_MAX * X9C_KOHM_POR_PASSO, X9C_KOHM_POR_PASSO);
  Serial.println("  + / -        +1 / -1 passo (envia pulso mesmo no 0 ou 99)");
  Serial.println("  +5 / -3      +5 / -3 passos (ou +p5 / -p50)");
  Serial.println("  p50          passo absoluto 0 a 99");
  Serial.println("  h            homing (zero)");
  Serial.println("  v            validacao ancora MIN/MAX (tabela)");
  Serial.println("  a            afericao completa (tabela longa)");
  Serial.println("  b max / b min  buscar fim/inicio com pulsos extras");
  Serial.printf("  m up/down/full  modo pulso INC (atual: %s)\n", nomeModoPulso(modoPulso));
  Serial.println("  t            teste rapido de bordas (5 pulsos x 3 modos)");
  Serial.println("  ?            esta ajuda");
  Serial.println();
}

void processarEntrada(String entrada) {
  entrada.trim();
  if (entrada.length() == 0) return;

  String lower = entrada;
  lower.toLowerCase();

  if (lower == "?" || lower == "ajuda" || lower == "help") {
    imprimirAjuda();
    return;
  }
  if (lower == "h" || lower == "home" || lower == "zero" || lower == "z") {
    reiniciarMinimo();
    imprimirEstado();
    return;
  }

  if (definirModoPulso(lower)) return;

  if (lower == "v" || lower == "validar" || lower == "validacao") {
    testeValidacaoRapida();
    imprimirEstado();
    return;
  }

  if (lower == "a" || lower == "aferir" || lower == "afericao") {
    testeAfericaoCompleta();
    imprimirEstado();
    return;
  }
  if (lower == "b max" || lower == "bmax") {
    buscarLimite(true);
    return;
  }
  if (lower == "b min" || lower == "bmin") {
    buscarLimite(false);
    return;
  }

  if (lower == "t" || lower == "test" || lower == "teste" || lower == "bordas") {
    testeComparativoBordas();
    imprimirEstado();
    return;
  }
  if (lower.startsWith("t") && lower.length() > 1) {
    String num = lower.substring(1);
    num.trim();
    if (num.length() > 0) {
      for (unsigned i = 0; i < num.length(); i++) {
        if (!isDigit((unsigned char)num.charAt(i))) {
          Serial.println("[ERRO] Use t ou t5 (numero de pulsos).");
          return;
        }
      }
      testeRapidoPulsos(num.toInt());
      return;
    }
  }

  int delta = 0;
  if (parseIncrementoPassos(lower, &delta)) {
    incrementarPassos(delta);
    return;
  }

  // Passo explicito: p50 ou s50
  if (lower.startsWith("p") || lower.startsWith("s")) {
    if (lower.length() < 2) {
      Serial.println("[ERRO] Use p50 ou s50 para passo absoluto.");
      return;
    }
    for (unsigned i = 1; i < lower.length(); i++) {
      if (!isDigit((unsigned char)lower.charAt(i))) {
        Serial.println("[ERRO] Valor invalido. Ex.: p50  s25");
        return;
      }
    }
    int passo = lower.substring(1).toInt();
    if (passo < 0 || passo > X9C_PASSOS_MAX) {
      Serial.printf("[ERRO] Passo invalido. Use 0..%u\n", X9C_PASSOS_MAX);
      return;
    }
    Serial.printf("[INFO] Passo pedido: %d\n", passo);
    irParaPasso((uint8_t)passo);
    return;
  }

  // Valor numerico = kOhm
  float kohm = 0.0f;
  if (parseFloatBr(entrada, &kohm)) {
    ajustarKohm(kohm);
    return;
  }

  Serial.println("[ERRO] Valor invalido. Ex.: 90  +  -  +5  p50  (? = ajuda)");
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(800);

  Serial.println();
  Serial.println("========================================");
  Serial.println("  TESTE X9C104S — ajuste por Serial");
  Serial.println("  115200 baud | Nova linha no final");
  Serial.println("========================================");
  Serial.printf("[INFO] Modo pulso: %s | 'v' = validacao ancora | 'a' = afericao completa\n",
                nomeModoPulso(modoPulso));

  pinMode(PIN_X9C_CS, OUTPUT);
  pinMode(PIN_X9C_INC, OUTPUT);
  pinMode(PIN_X9C_UD, OUTPUT);
  digitalWrite(PIN_X9C_UD, LOW);
  incNivelAlto = false;
  definirIncIdle(false);

  reiniciarMinimo();
  imprimirEstado();
  imprimirAjuda();
  Serial.print("> ");
}

void loop() {
  if (!Serial.available()) return;

  String entrada = Serial.readStringUntil('\n');
  entrada.trim();

  if (entrada.length() == 0) return;

  Serial.println(entrada);
  processarEntrada(entrada);
  Serial.print("> ");
}
