/**
 * teste_x9c104_exemplo.ino — Exemplo de uso do X9C104S via Serial
 *
 * Sketch de referencia apos afericao em bancada. Demonstra:
 *   - incrementos relativos (+ / - / +5)
 *   - posicao absoluta por passo (p50) ou por kΩ (45.5)
 *   - homing (h) e maximo (m)
 *   - driver calibrado (pulso SUBIDA, +1 no maximo, estado INC guardado)
 *
 * Hardware: pinos em config.h | Multimetro: VL-VW (pinos 6-5 do X9C)
 * Serial: 115200 baud | Fim de linha: Nova linha (NL)
 *
 * Comandos:
 *   h          homing (passo 0)
 *   m          maximo (passo 99 + pulso extra -> ~91,8 kΩ)
 *   p50        passo absoluto 0..99
 *   45.5       resistencia alvo em kΩ
 *   + / -      +/- 1 passo (envia pulso mesmo no 0 ou 99)
 *   +5 / -10   incremento relativo
 *   ?          ajuda
 */

#include "../../config.h"
#include "x9c_exemplo.h"

#define PAUSA_ENTRE_TAPS_MS  5

X9CExemplo pot;

// ---------------------------------------------------------------------------
// Utilitarios de parsing Serial
// ---------------------------------------------------------------------------

/** Normaliza numero digitado (aceita virgula decimal). */
String normalizarNumero(String s) {
  s.trim();
  s.replace(',', '.');
  return s;
}

/**
 * Interpreta string como float.
 * Motivo: usuario digita "45,5" no Monitor Serial em PT-BR.
 */
bool parseFloatBr(const String& s, float* out) {
  String n = normalizarNumero(s);
  if (n.length() == 0) {
    return false;
  }

  for (unsigned i = 0; i < n.length(); i++) {
    char c = n.charAt(i);
    if (c != '.' && !isDigit((unsigned char)c)) {
      return false;
    }
  }

  *out = n.toFloat();
  return true;
}

/**
 * Interpreta +, -, +5, -10, +p3 etc.
 * Retorna delta em passos (positivo = subir resistencia).
 */
bool parseIncremento(const String& lower, int* delta) {
  if (lower == "+" || lower == "++") {
    *delta = 1;
    return true;
  }
  if (lower == "-") {
    *delta = -1;
    return true;
  }
  if (lower.length() < 2) {
    return false;
  }

  char sinal = lower.charAt(0);
  if (sinal != '+' && sinal != '-') {
    return false;
  }

  unsigned pos = 1;
  if (pos < lower.length() && lower.charAt(pos) == 'p') {
    pos++;
  }
  if (pos >= lower.length()) {
    return false;
  }

  for (unsigned i = pos; i < lower.length(); i++) {
    if (!isDigit((unsigned char)lower.charAt(i))) {
      return false;
    }
  }

  int n = lower.substring(pos).toInt();
  *delta = (sinal == '+') ? n : -n;
  return true;
}

// ---------------------------------------------------------------------------
// Feedback Serial
// ---------------------------------------------------------------------------

/**
 * Imprime passo, estimativa kΩ e nivel do INC.
 * Motivo: comparar com multimetro e depurar sincronismo de bordas.
 */
void imprimirEstado() {
  const char* ancora = pot.ancoradoNoMinimo() ? "MIN"
                     : pot.ancoradoNoMaximo() ? "MAX"
                     : "-";
  Serial.printf("[OK] passo %u/%u | VL-VW ~ %.2f kΩ | INC %s | ancora %s\n",
                pot.passoAtual(), X9C_PASSOS_MAX,
                pot.resistenciaEstimadaKohm(),
                pot.incEmNivelAlto() ? "HIGH" : "LOW",
                ancora);
}

void imprimirAjuda() {
  Serial.println();
  Serial.println("--- Comandos ---");
  Serial.println("  h           homing -> passo 0 (~0 kΩ)");
  Serial.println("  m           maximo -> passo 99 + extra (~91,8 kΩ)");
  Serial.println("  p50         passo absoluto (0..99)");
  Serial.printf("  45.5        alvo em kΩ (0..%.1f)\n", X9C_MAX_KOHM_VL_VW);
  Serial.println("  + / -       +/- 1 passo (1o +/- no 0/99 desancora, nao muda passo)");
  Serial.println("  +5 / -10    incremento relativo");
  Serial.println("  ?           esta ajuda");
  Serial.println();
  Serial.println("Medir sempre VL-VW (pinos 6-5). Pulso: so SUBIDA (config.h).");
  Serial.println();
}

// ---------------------------------------------------------------------------
// Acoes de alto nivel (exemplo de API publica)
// ---------------------------------------------------------------------------

/** Executa homing e mostra estado. */
void cmdHoming() {
  Serial.println("[INFO] Homing...");
  pot.reiniciarMinimo();
  delay(PAUSA_ENTRE_TAPS_MS);
  imprimirEstado();
}

/** Vai ao maximo calibrado (99 + pulso extra). */
void cmdMaximo() {
  Serial.println("[INFO] Maximo (passo 99 + pulso extra)...");
  pot.reiniciarParaMaximo();
  delay(PAUSA_ENTRE_TAPS_MS);
  imprimirEstado();
}

/** Posiciona por passo absoluto. */
void cmdPassoAbsoluto(uint8_t passo) {
  Serial.printf("[INFO] Passo alvo: %u\n", passo);
  pot.irParaPasso(passo);
  delay(PAUSA_ENTRE_TAPS_MS);
  imprimirEstado();
}

/** Posiciona por resistencia em kΩ. */
void cmdKohm(float kohm) {
  Serial.printf("[INFO] Alvo: %.2f kΩ\n", kohm);
  pot.ajustarKohm(kohm);
  delay(PAUSA_ENTRE_TAPS_MS);
  imprimirEstado();
}

/** Incremento relativo; 1o +/- em MIN/MAX desancora sem mudar passo. */
void cmdIncremento(int delta) {
  uint8_t antes = pot.passoAtual();
  bool eraMin = pot.ancoradoNoMinimo();
  bool eraMax = pot.ancoradoNoMaximo();

  Serial.printf("[INFO] Incremento %+d passo(s)\n", delta);
  pot.incrementarPassos(delta);

  if (delta > 0 && eraMin && pot.passoAtual() == antes) {
    Serial.println("[INFO] 1o + no minimo: pulso de saida (passo ainda 0).");
  } else if (delta < 0 && eraMax && pot.passoAtual() == antes) {
    Serial.println("[INFO] 1o - no maximo: pulso de saida (passo ainda 99).");
  }

  delay(PAUSA_ENTRE_TAPS_MS);
  imprimirEstado();
}

// ---------------------------------------------------------------------------
// Interpretador de linha Serial
// ---------------------------------------------------------------------------

/**
 * Processa uma linha do Monitor Serial.
 * Ordem: ajuda -> homing -> max -> incremento -> passo -> kΩ.
 */
void processarEntrada(String entrada) {
  entrada.trim();
  if (entrada.length() == 0) {
    return;
  }

  String lower = entrada;
  lower.toLowerCase();

  if (lower == "?" || lower == "ajuda" || lower == "help") {
    imprimirAjuda();
    return;
  }

  if (lower == "h" || lower == "home" || lower == "zero" || lower == "z") {
    cmdHoming();
    return;
  }

  if (lower == "m" || lower == "max" || lower == "maximo") {
    cmdMaximo();
    return;
  }

  int delta = 0;
  if (parseIncremento(lower, &delta)) {
    cmdIncremento(delta);
    return;
  }

  if (lower.startsWith("p") || lower.startsWith("s")) {
    if (lower.length() < 2) {
      Serial.println("[ERRO] Use p50 para passo absoluto.");
      return;
    }
    for (unsigned i = 1; i < lower.length(); i++) {
      if (!isDigit((unsigned char)lower.charAt(i))) {
        Serial.println("[ERRO] Ex.: p50");
        return;
      }
    }
    int passo = lower.substring(1).toInt();
    if (passo < 0 || passo > (int)X9C_PASSOS_MAX) {
      Serial.printf("[ERRO] Passo invalido (0..%u).\n", X9C_PASSOS_MAX);
      return;
    }
    cmdPassoAbsoluto((uint8_t)passo);
    return;
  }

  float kohm = 0.0f;
  if (parseFloatBr(entrada, &kohm)) {
    cmdKohm(kohm);
    return;
  }

  Serial.println("[ERRO] Invalido. Ex.: h  m  p50  45.5  +  -  +5  ?");
}

// ---------------------------------------------------------------------------
// Arduino
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);

  Serial.println();
  Serial.println("========================================");
  Serial.println("  X9C104S — exemplo de uso (Serial)");
  Serial.println("  115200 | Nova linha | VL-VW pinos 6-5");
  Serial.println("========================================");
  Serial.printf("[INFO] ~%.3f kΩ/passo | passo 99 ~ %.1f kΩ | max ~ %.1f kΩ\n",
                X9C_KOHM_POR_PASSO, X9C_KOHM_PASSO_99, X9C_MAX_KOHM_VL_VW);

  pot.iniciar();
  cmdHoming();
  imprimirAjuda();
  Serial.print("> ");
}

void loop() {
  if (!Serial.available()) {
    return;
  }

  String linha = Serial.readStringUntil('\n');
  linha.trim();
  if (linha.length() == 0) {
    return;
  }

  Serial.println(linha);
  processarEntrada(linha);
  Serial.print("> ");
}
