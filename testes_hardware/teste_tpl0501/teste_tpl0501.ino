/**
 * teste_tpl0501.ino — Teste e aferição TPL0501 (SPI, 256 taps)
 *
 * Guia completo: README.md nesta pasta.
 *
 * Modo 2× série + paralelo (malha principal):
 *   pio run -t upload   (default_envs = esp32dev_paralelo)
 *
 * AFERIÇÃO — ordem recomendada:
 *   v  → teste rápido Req na rede
 *   aa → aferir chip A (multímetro L–W, B=0)
 *   ab → aferir chip B (multímetro L–W, A=0)
 *   ae → aferir Req na rede (A=B, com paralelo)
 *   l  → copiar #define para config.h
 *   (ou `a` = aa+ab+ae de uma vez)
 */

#include "../../config.h"
#include <SPI.h>
#include "../../potenciometro_map.h"
#include "../../atuador_potenciometro.h"

#define PAUSA_MS            8
#define PAUSA_VARREDURA_MS  3000
#define PAUSA_AFERICAO_MS   400
#define TIMEOUT_MEDICAO_MS  20000   // 20 s sem resposta → pula o ponto
#define TPL0501_DATASHEET_MAX_KOHM  100.0f
#define AFERICAO_MAX_PONTOS  48

static bool g_abortarAfericao = false;

/** Pontos usados na aferição rápida (3 por fase — termina em ~1 min) */
static const uint8_t PONTOS_AFERICAO[] = {0, 128, POT_PASSOS_MAX};
static const unsigned N_PONTOS_AFERICAO =
    sizeof(PONTOS_AFERICAO) / sizeof(PONTOS_AFERICAO[0]);

/** Onde colocar o multímetro em cada fase da aferição */
enum class CtxMedicao : uint8_t {
  CHIP_A_LW,
  CHIP_B_LW,
  REQ_REDE
};

struct MedicaoAfericao {
  uint8_t passoA;
  uint8_t passoB;
  float medKohm;
  bool preenchido;
};

static MedicaoAfericao g_afericoes[AFERICAO_MAX_PONTOS];
static int g_nAfericoes = 0;

AtuadorPotenciometro atuador;

void imprimirStatus(const char* prefixo);

/** Salto direto via SPI (sem slew passo a passo da malha) — uso na aferição */
void moverPassosRapido(uint8_t alvoA, uint8_t alvoB) {
#if POT_USA_DOIS_CHIPS
  atuador.definirPassoIsolado('A', alvoA);
  atuador.definirPassoIsolado('B', alvoB);
#else
  (void)alvoB;
  atuador.definirPassoIsolado('A', alvoA);
#endif
  delay(PAUSA_MS);
}

/**
 * Aguarda linha Serial com timeout. Retorna:
 *   1 = valor lido | 0 = Enter/pulou | -1 = timeout | -2 = abortou (q)
 */
int aguardarLinhaSerial(String* linha, uint32_t timeoutMs) {
  uint32_t t0 = millis();
  uint32_t ultimoAviso = 0;
  while (millis() - t0 < timeoutMs) {
    if (Serial.available()) {
      *linha = Serial.readStringUntil('\n');
      linha->trim();
      if (*linha == "q" || *linha == "Q") {
        g_abortarAfericao = true;
        return -2;
      }
      return linha->length() > 0 ? 1 : 0;
    }
    if (millis() - ultimoAviso >= 5000) {
      ultimoAviso = millis();
      uint32_t restante = (timeoutMs - (millis() - t0)) / 1000;
      Serial.print(F("       ... aguardando ("));
      Serial.print(restante);
      Serial.println(F(" s) — Enter=pular | q=abortar"));
    }
    delay(50);
  }
  return -1;
}

// ---------------------------------------------------------------------------
// Utilitários Serial
// ---------------------------------------------------------------------------

String normalizarNumero(String s) {
  s.trim();
  s.replace(',', '.');
  return s;
}

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

bool passoValido(int passo) {
  return passo >= 0 && passo <= (int)POT_PASSOS_MAX;
}

bool parseDoisPassos(const String& s, uint8_t* passoA, uint8_t* passoB) {
  int barra = s.indexOf('/');
  if (barra < 0) {
    int unico = s.toInt();
    if (!passoValido(unico)) {
      return false;
    }
    *passoA = (uint8_t)unico;
    *passoB = (uint8_t)unico;
    return true;
  }
  int a = s.substring(0, barra).toInt();
  int b = s.substring(barra + 1).toInt();
  if (!passoValido(a) || !passoValido(b)) {
    return false;
  }
  *passoA = (uint8_t)a;
  *passoB = (uint8_t)b;
  return true;
}

float refDatasheetChipKohm(uint8_t passo) {
  if (passo > POT_PASSOS_MAX) {
    passo = POT_PASSOS_MAX;
  }
  return TPL0501_DATASHEET_MAX_KOHM * (float)passo / (float)POT_PASSOS_MAX;
}

float refEstimadaRede(uint8_t passoA, uint8_t passoB) {
  float rSerie = refDatasheetChipKohm(passoA) + refDatasheetChipKohm(passoB);
  if (reqParaleloEstaAtivo()) {
    return (RESISTOR_PARALELO_KOHM * rSerie) / (RESISTOR_PARALELO_KOHM + rSerie);
  }
  return rSerie;
}

void imprimirTabelaAfericao() {
  if (g_nAfericoes == 0) {
    return;
  }
  Serial.println();
  Serial.println(F("--- TABELA AFERICAO ---"));
  Serial.println(F("passoA;passoB;medido_kOhm;ctx"));
  for (int i = 0; i < g_nAfericoes; i++) {
    Serial.print(g_afericoes[i].passoA);
    Serial.print(';');
    Serial.print(g_afericoes[i].passoB);
    Serial.print(';');
    Serial.print(g_afericoes[i].medKohm, 2);
    Serial.print(';');
    if (g_afericoes[i].passoB == 0 && g_afericoes[i].passoA != 0) {
      Serial.println(F("chipA_LW"));
    } else if (g_afericoes[i].passoA == 0 && g_afericoes[i].passoB != 0) {
      Serial.println(F("chipB_LW"));
    } else {
      Serial.println(F("Req_rede"));
    }
  }
}

// ---------------------------------------------------------------------------
// Aferição — acumula medições para gerar config.h
// ---------------------------------------------------------------------------

void limparAfericoes() {
  g_nAfericoes = 0;
}

bool registrarMedicao(uint8_t passoA, uint8_t passoB, float medKohm) {
  for (int i = 0; i < g_nAfericoes; i++) {
    if (g_afericoes[i].passoA == passoA && g_afericoes[i].passoB == passoB) {
      g_afericoes[i].medKohm = medKohm;
      g_afericoes[i].preenchido = true;
      return true;
    }
  }
  if (g_nAfericoes >= AFERICAO_MAX_PONTOS) {
    return false;
  }
  g_afericoes[g_nAfericoes].passoA = passoA;
  g_afericoes[g_nAfericoes].passoB = passoB;
  g_afericoes[g_nAfericoes].medKohm = medKohm;
  g_afericoes[g_nAfericoes].preenchido = true;
  g_nAfericoes++;
  return true;
}

bool buscarMedicao(uint8_t passoA, uint8_t passoB, float* medKohm) {
  for (int i = 0; i < g_nAfericoes; i++) {
    if (g_afericoes[i].passoA == passoA && g_afericoes[i].passoB == passoB
        && g_afericoes[i].preenchido) {
      *medKohm = g_afericoes[i].medKohm;
      return true;
    }
  }
  return false;
}

void imprimirLimitesSugeridos() {
  Serial.println();
  Serial.println(F("========== LIMITES SUGERIDOS (cole em config.h) =========="));

#if POT_USA_DOIS_CHIPS
  float medA255 = 0.0f;
  float medB255 = 0.0f;
  bool temA = buscarMedicao(POT_PASSOS_MAX, 0, &medA255);
  bool temB = buscarMedicao(0, POT_PASSOS_MAX, &medB255);

  if (temA) {
    Serial.print(F("#define POT_AFERIDO_KOHM_MAX_A     "));
    Serial.print(medA255, 1);
    Serial.println(F("f"));
    Serial.print(F("#define POT_AFERIDO_KOHM_MAX_A           "));
    Serial.print(medA255, 1);
    Serial.println(F("f"));
  } else {
    Serial.println(F("// Chip A passo 255: execute 'a' ou meça com pa255"));
  }

  if (temB) {
    Serial.print(F("#define POT_AFERIDO_KOHM_MAX_B     "));
    Serial.print(medB255, 1);
    Serial.println(F("f"));
    Serial.print(F("#define POT_AFERIDO_KOHM_MAX_B           "));
    Serial.print(medB255, 1);
    Serial.println(F("f"));
  } else {
    Serial.println(F("// Chip B passo 255: execute 'a' ou meça com pb255"));
  }

  float medReq255 = 0.0f;
  if (buscarMedicao(POT_PASSOS_MAX, POT_PASSOS_MAX, &medReq255)) {
    Serial.print(F("#define REQ_IDEAL_POTENCIA_MIN_KOHM               "));
    Serial.print(medReq255, 1);
    Serial.println(F("f"));
  } else if (reqParaleloEstaAtivo()) {
    Serial.println(F("// REQ_IDEAL_POTENCIA_MIN_KOHM: Req medida na rede com A=B=255 (comando ae)"));
  }

  if (reqParaleloEstaAtivo()) {
    Serial.println();
    Serial.print(F("// Resistor fixo em paralelo (medir com multimetro): RESISTOR_PARALELO_KOHM = "));
    Serial.print(RESISTOR_PARALELO_KOHM, 0);
    Serial.println(F(" (ajuste se necessario)"));
  }
#else
  float med255 = 0.0f;
  if (buscarMedicao(POT_PASSOS_MAX, 0, &med255)) {
    Serial.print(F("#define POT_AFERIDO_KOHM_MAX_A     "));
    Serial.print(med255, 1);
    Serial.println(F("f"));
    Serial.print(F("#define POT_AFERIDO_KOHM_MAX_A           "));
    Serial.print(med255, 1);
    Serial.println(F("f"));
  } else {
    Serial.println(F("// Passo 255: execute 'a' ou mova com p255 e meça L–W"));
  }
#endif

  Serial.println(F("=========================================================="));
  Serial.println();
}

bool solicitarMedicaoUsuario(uint8_t passoA, uint8_t passoB, CtxMedicao ctx,
                             unsigned indice, unsigned total) {
  if (g_abortarAfericao) {
    return false;
  }

  Serial.println();
  Serial.print(F("[MEDIR "));
  Serial.print(indice);
  Serial.print(F("/"));
  Serial.print(total);
  Serial.print(F("] "));
  switch (ctx) {
    case CtxMedicao::CHIP_A_LW:
      Serial.print(F("Chip A L-W | passo A="));
      Serial.print(passoA);
      Serial.print(F(" | ref ~ "));
      Serial.print(refDatasheetChipKohm(passoA), 1);
      break;
    case CtxMedicao::CHIP_B_LW:
      Serial.print(F("Chip B L-W | passo B="));
      Serial.print(passoB);
      Serial.print(F(" | ref ~ "));
      Serial.print(refDatasheetChipKohm(passoB), 1);
      break;
    case CtxMedicao::REQ_REDE:
      Serial.print(F("Req REDE | A=B="));
      Serial.print(passoA);
      Serial.print(F(" | ref ~ "));
      Serial.print(refEstimadaRede(passoA, passoB), 1);
      break;
  }
  Serial.println(F(" kΩ"));
  Serial.println(F("       Valor medido (kΩ) | Enter=pular | q=abortar:"));

  String linha;
  int res = aguardarLinhaSerial(&linha, TIMEOUT_MEDICAO_MS);
  if (res == -2 || g_abortarAfericao) {
    Serial.println(F("[ABORT] Afericao cancelada (q)."));
    return false;
  }
  if (res == -1) {
    Serial.println(F("[INFO] Timeout — ponto pulado."));
    return true;
  }
  if (res == 0) {
    Serial.println(F("[INFO] Pulado."));
    return true;
  }

  float med = 0.0f;
  if (!parseFloatBr(linha, &med)) {
    Serial.println(F("[ERRO] Valor invalido — ponto pulado."));
    return true;
  }

  if (registrarMedicao(passoA, passoB, med)) {
    Serial.print(F("[OK] Registrado "));
    Serial.print(med, 2);
    Serial.println(F(" kΩ"));
  }
  return true;
}

bool parseIncrementoRelativo(const String& lower, int* delta) {
  if (lower.length() < 2) {
    return false;
  }
  char sinal = lower.charAt(0);
  if (sinal != '+' && sinal != '-') {
    return false;
  }
  String num = lower.substring(1);
  for (unsigned i = 0; i < num.length(); i++) {
    if (!isDigit((unsigned char)num.charAt(i))) {
      return false;
    }
  }
  int n = num.toInt();
  *delta = (sinal == '+') ? n : -n;
  return true;
}

// ---------------------------------------------------------------------------
// Comandos de movimento
// ---------------------------------------------------------------------------

void cmdPassosAbsolutos(uint8_t alvoA, uint8_t alvoB, bool silencioso) {
  if (!silencioso) {
    Serial.print(F("[INFO] Passos alvo A="));
    Serial.print(alvoA);
    Serial.print(F(" B="));
    Serial.println(alvoB);
  }
  atuador.definirPassosAlvo(alvoA, alvoB);
  delay(PAUSA_MS);
  if (!silencioso) {
    imprimirStatus("[OK]");
  }
}

void cmdValidacaoRapida() {
  static const uint8_t pontos[] = {0, 128, POT_PASSOS_MAX};
  Serial.println();
  Serial.println(F("========== VALIDACAO RAPIDA (passo 1 de 5) =========="));
#if POT_USA_DOIS_CHIPS && REDE_COM_RESISTOR_PARALELO
  Serial.println(F("Multimetro: REDE DE SAIDA (chuveiro + resistor paralelo)."));
  Serial.println(F("A=B em 0, 128 e 255 — Req deve SUBIR a cada passo."));
#elif POT_USA_DOIS_CHIPS
  Serial.println(F("Multimetro: soma L-W chip A + L-W chip B (R_serie)."));
#else
  Serial.println(F("Multimetro: L-W do unico chip."));
#endif

  for (unsigned i = 0; i < sizeof(pontos) / sizeof(pontos[0]); i++) {
    uint8_t p = pontos[i];
    Serial.println();
    Serial.print(F("--- Parada "));
    Serial.print(i + 1);
    Serial.print(F("/3: A=B="));
    Serial.println(p);
#if POT_USA_DOIS_CHIPS
    moverPassosRapido(p, p);
    imprimirStatus("[POS]");
#if REDE_COM_RESISTOR_PARALELO
    Serial.print(F("[REF] Req datasheet ~ "));
    Serial.print(refEstimadaRede(p, p), 1);
#else
    Serial.print(F("[REF] R_serie datasheet ~ "));
    Serial.print(refDatasheetChipKohm(p) * 2.0f, 1);
#endif
#else
    moverPassosRapido(p, 0);
    imprimirStatus("[POS]");
    Serial.print(F("[REF] L-W datasheet ~ "));
    Serial.print(refDatasheetChipKohm(p), 1);
#endif
    Serial.println(F(" kΩ — meça agora e anote"));
    delay(PAUSA_AFERICAO_MS);
  }

  Serial.println();
  Serial.println(F("[OK] Validacao concluida. Proximo: aa (chip A), ab (chip B), ae (Req rede)."));
}

#if POT_USA_DOIS_CHIPS
/** Aferição: 3 pontos (0/128/255), salto SPI direto, timeout 20 s por ponto */
bool aferirFaixaPassos(CtxMedicao ctx) {
  for (unsigned i = 0; i < N_PONTOS_AFERICAO; i++) {
    if (g_abortarAfericao) {
      return false;
    }
    uint8_t p = PONTOS_AFERICAO[i];
    uint8_t a = p;
    uint8_t b = p;
    if (ctx == CtxMedicao::CHIP_A_LW) {
      b = 0;
    } else if (ctx == CtxMedicao::CHIP_B_LW) {
      a = 0;
    }
    Serial.print(F("[POS] Movendo para A="));
    Serial.print(a);
    Serial.print(F(" B="));
    Serial.println(b);
    moverPassosRapido(a, b);
    delay(PAUSA_AFERICAO_MS);
    if (!solicitarMedicaoUsuario(a, b, ctx, i + 1, N_PONTOS_AFERICAO)) {
      return false;
    }
  }
  return true;
}

void cmdAferirChipA() {
  g_abortarAfericao = false;
  Serial.println();
  Serial.println(F("========== AFERICAO CHIP A (3 pontos) =========="));
  Serial.println(F("Multimetro: L-W chip A | B=0 | max 20 s por ponto | q=abortar"));
  if (!aferirFaixaPassos(CtxMedicao::CHIP_A_LW)) {
    return;
  }
  imprimirTabelaAfericao();
  Serial.println(F("[OK] Chip A concluido. Proximo: ab"));
}

void cmdAferirChipB() {
  g_abortarAfericao = false;
  Serial.println();
  Serial.println(F("========== AFERICAO CHIP B (3 pontos) =========="));
  Serial.println(F("Multimetro: L-W chip B | A=0 | max 20 s por ponto | q=abortar"));
  if (!aferirFaixaPassos(CtxMedicao::CHIP_B_LW)) {
    return;
  }
  imprimirTabelaAfericao();
  Serial.println(F("[OK] Chip B concluido. Proximo: ae"));
}

void cmdAferirReqRede() {
  g_abortarAfericao = false;
  Serial.println();
  Serial.println(F("========== AFERICAO REQ REDE (3 pontos) =========="));
  Serial.println(F("Multimetro na REDE DE SAIDA (nao L-W de chip) | q=abortar"));
  if (!aferirFaixaPassos(CtxMedicao::REQ_REDE)) {
    return;
  }
  imprimirTabelaAfericao();
  imprimirLimitesSugeridos();
  Serial.println(F("[OK] Req concluida. Digite l para constantes config.h"));
}
#endif

void cmdAfericaoCompleta() {
  g_abortarAfericao = false;
  limparAfericoes();
  Serial.println();
  Serial.println(F("========== AFERICAO COMPLETA (9 pontos, ~3 min) =========="));
  Serial.println(F("3 pontos por fase (0/128/255). Enter=pular | q=abortar a qualquer momento."));
#if POT_USA_DOIS_CHIPS
  cmdAferirChipA();
  if (g_abortarAfericao) {
    Serial.println(F("[FIM] Afericao interrompida."));
    return;
  }
  cmdAferirChipB();
  if (g_abortarAfericao) {
    Serial.println(F("[FIM] Afericao interrompida."));
    return;
  }
  cmdAferirReqRede();
#else
  for (unsigned i = 0; i < N_PONTOS_AFERICAO; i++) {
    if (g_abortarAfericao) {
      break;
    }
    uint8_t p = PONTOS_AFERICAO[i];
    moverPassosRapido(p, 0);
    delay(PAUSA_AFERICAO_MS);
    if (!solicitarMedicaoUsuario(p, 0, CtxMedicao::CHIP_A_LW, i + 1, N_PONTOS_AFERICAO)) {
      break;
    }
  }
  imprimirTabelaAfericao();
  imprimirLimitesSugeridos();
#endif
  if (!g_abortarAfericao) {
    Serial.println(F("[OK] Afericao completa."));
  }
}
// ---------------------------------------------------------------------------
// Feedback
// ---------------------------------------------------------------------------

void imprimirPinos() {
  Serial.print(F("  CS_A="));
  Serial.print(PINO_POT_CS_A);
  Serial.print(F(" SCLK="));
  Serial.print(PINO_POT_SCLK);
  Serial.print(F(" MOSI="));
  Serial.println(PINO_POT_MOSI);
#if POT_USA_DOIS_CHIPS
  Serial.print(F("  CS_B="));
  Serial.println(PINO_POT_CS_B);
#endif
}

void imprimirCabecalhoMapa() {
  Serial.print(F("[MAP] "));
  Serial.print(potenciometroModoRedeNome());
  Serial.print(F(" | passos 0.."));
  Serial.print(POT_PASSOS_MAX);
  if (reqParaleloEstaAtivo()) {
    Serial.print(F(" | R_par "));
    Serial.print(RESISTOR_PARALELO_KOHM, 0);
    Serial.print(F(" kΩ"));
  }
  Serial.print(F(" | R_serie max "));
  Serial.print(rpotSerieMaximaKohm(), 1);
  Serial.println(F(" kΩ"));
}

void imprimirStatus(const char* prefixo) {
  uint8_t pa = atuador.passoAtualA();
  uint8_t pb = atuador.passoAtualB();
  float rSerie = rpotSerieEstimadaKohm(pa, pb);
  float req = reqEquivKohmDePassos(pa, pb);
  float pct = passoParaPotenciaCombinadaPercentual(pa, pb);

  Serial.print(prefixo);
#if POT_USA_DOIS_CHIPS
  Serial.print(F(" A="));
  Serial.print(pa);
  Serial.print(F(" B="));
  Serial.print(pb);
#else
  Serial.print(F(" passo="));
  Serial.print(pa);
#endif
  Serial.print(F(" | R_serie ~ "));
  Serial.print(rSerie, 1);
  Serial.print(F(" kΩ | Req ~ "));
  Serial.print(req, 1);
  Serial.print(F(" kΩ | pot "));
  Serial.print(pct, 1);
  Serial.println(F(" %"));
}

void imprimirGuiaUso() {
  Serial.println();
  Serial.println(F("######## ROTEIRO — AFERICAO ########"));
  Serial.print(F("Modo ativo: "));
  Serial.println(potenciometroModoRedeNome());
#if POT_USA_DOIS_CHIPS && REDE_COM_RESISTOR_PARALELO
  Serial.println();
  Serial.println(F("  1) v   Teste rapido — Req na REDE (0/128/255)"));
  Serial.println(F("  2) aa  Aferir chip A — multimetro L-W do chip A (B=0)"));
  Serial.println(F("  3) ab  Aferir chip B — multimetro L-W do chip B (A=0)"));
  Serial.println(F("  4) ae  Aferir Req na REDE — multimetro na saida+paralelo"));
  Serial.println(F("  5) l   Copiar #define para config.h"));
  Serial.println();
  Serial.println(F("Atalho: a = passos 2+3+4 (~3 min) | q aborta | Enter pula"));
  Serial.println(F("Mais detalhes: README.md nesta pasta | ? = todos os comandos"));
#elif POT_USA_DOIS_CHIPS
  Serial.println(F("  v → aa → ab → ae → l   (ver README.md)"));
#else
  Serial.println(F("  v → a → l   (1 chip: multimetro L-W)"));
#endif
  Serial.println(F("####################################"));
}

void imprimirAjuda() {
  imprimirGuiaUso();
  Serial.println();
  Serial.println(F("--- AFERICAO ---"));
  Serial.println(F("  v           validacao rapida (0/128/255)"));
#if POT_USA_DOIS_CHIPS
  Serial.println(F("  aa          aferir chip A (L-W, B=0)"));
  Serial.println(F("  ab          aferir chip B (L-W, A=0)"));
  Serial.println(F("  ae          aferir Req na rede (A=B, com paralelo)"));
#endif
  Serial.println(F("  a           afericao completa (9 pontos, ~3 min)"));
  Serial.println(F("  Enter       pula ponto | q aborta | timeout 20 s"));
  Serial.println(F("  l           constantes para config.h"));
  Serial.println();
  Serial.println(F("--- MOVIMENTO ---"));
  Serial.println(F("  h / m       minimo (passo 0) / maximo (passo 255)"));
  Serial.println(F("  s           status (passos, R_serie, Req, pot %)"));
  Serial.println(F("  p128        passo A=B"));
#if POT_USA_DOIS_CHIPS
  Serial.println(F("  pa128 pb64  passo so chip A ou B"));
  Serial.println(F("  p64/192     passos A/B diferentes"));
  Serial.println(F("  ++ / --     intercalado (como malha PID)"));
  Serial.println(F("  +a -a +b -b um chip por vez"));
#else
  Serial.println(F("  + / -       +/- 1 passo"));
#endif
  Serial.println(F("  u50         potencia 50 % | r120 Req alvo"));
  Serial.println(F("  t           varredura 0..100 %"));
  Serial.println(F("  g / ?       este guia / ajuda completa"));
  Serial.println();
}

// ---------------------------------------------------------------------------
// Comandos de movimento (continuação)
// ---------------------------------------------------------------------------

void cmdHoming() {
  Serial.println(F("[INFO] Homing..."));
  atuador.reiniciarParaMinimo();
  delay(PAUSA_MS);
  imprimirStatus("[OK]");
}

void cmdMaximo() {
  Serial.println(F("[INFO] Maximo (passo 255)..."));
#if POT_USA_DOIS_CHIPS
  moverPassosRapido(POT_PASSOS_MAX, POT_PASSOS_MAX);
#else
  moverPassosRapido(POT_PASSOS_MAX, 0);
#endif
  imprimirStatus("[OK]");
}

void cmdPotenciaPercentual(float pct) {
  if (pct < 0.0f) pct = 0.0f;
  if (pct > 100.0f) pct = 100.0f;
  Serial.print(F("[INFO] Potencia alvo "));
  Serial.print(pct, 1);
  Serial.println(F(" %"));
  atuador.definirSaidaNormalizada(pct / 100.0f);
  delay(PAUSA_MS);
  imprimirStatus("[OK]");
}

void cmdReqAlvo(float reqKohm) {
  Serial.print(F("[INFO] Req alvo "));
  Serial.print(reqKohm, 1);
  Serial.println(F(" kΩ"));
  uint8_t pa = 0;
  uint8_t pb = 0;
  passosDeReqEquivKohm(reqKohm, &pa, &pb);
  Serial.print(F("[INFO] Passos calculados A="));
  Serial.print(pa);
  Serial.print(F(" B="));
  Serial.println(pb);
  atuador.definirPassosAlvo(pa, pb);
  delay(PAUSA_MS);
  imprimirStatus("[OK]");
}

void cmdPassoChipUnico(char chip, int delta) {
  uint8_t pa = atuador.passoAtualA();
  uint8_t pb = atuador.passoAtualB();
  if (chip == 'a' || chip == 'A') {
    int alvo = (int)pa + delta;
    if (alvo < 0) alvo = 0;
    if (alvo > (int)POT_PASSOS_MAX) alvo = POT_PASSOS_MAX;
    pa = (uint8_t)alvo;
  } else {
    int alvo = (int)pb + delta;
    if (alvo < 0) alvo = 0;
    if (alvo > (int)POT_PASSOS_MAX) alvo = POT_PASSOS_MAX;
    pb = (uint8_t)alvo;
  }
  cmdPassosAbsolutos(pa, pb, false);
}

void cmdIncrementoIntercalado(int delta) {
  uint8_t pa = atuador.passoAtualA();
  uint8_t pb = atuador.passoAtualB();
  int na = (int)pa + delta;
  int nb = (int)pb + delta;
  if (na < 0) na = 0;
  if (nb < 0) nb = 0;
  if (na > (int)POT_PASSOS_MAX) na = POT_PASSOS_MAX;
  if (nb > (int)POT_PASSOS_MAX) nb = POT_PASSOS_MAX;
  Serial.print(F("[INFO] Intercalado A/B -> "));
  Serial.print(na);
  Serial.print(F("/"));
  Serial.println(nb);
  cmdPassosAbsolutos((uint8_t)na, (uint8_t)nb, false);
}

void cmdIncrementoUnico(int delta) {
  uint8_t pa = atuador.passoAtualA();
  int alvo = (int)pa + delta;
  if (alvo < 0) alvo = 0;
  if (alvo > (int)POT_PASSOS_MAX) alvo = POT_PASSOS_MAX;
  cmdPassosAbsolutos((uint8_t)alvo, 0, false);
}

void cmdVarredura() {
  Serial.println(F("[INFO] Varredura 0..100 % (11 degraus, ~40 s)"));
  static const uint8_t passosPct[] = {0, 25, 51, 76, 102, 128, 153, 179, 204, 230, 255};
  for (unsigned i = 0; i < sizeof(passosPct) / sizeof(passosPct[0]); i++) {
    uint8_t p = passosPct[i];
    Serial.print(F("--- ~"));
    Serial.print((int)i * 10);
    Serial.print(F(" % (passo "));
    Serial.print(p);
    Serial.println(F(") ---"));
#if POT_USA_DOIS_CHIPS
    moverPassosRapido(p, p);
#else
    moverPassosRapido(p, 0);
#endif
    imprimirStatus("[OK]");
    delay(3000);
  }
  Serial.println(F("[OK] Varredura concluida."));
}

// ---------------------------------------------------------------------------
// Interpretador Serial
// ---------------------------------------------------------------------------

void processarEntrada(String entrada) {
  entrada.trim();
  if (entrada.length() == 0) {
    return;
  }

  String lower = entrada;
  lower.toLowerCase();

  if (lower == "?" || lower == "ajuda" || lower == "help" || lower == "g" || lower == "guia") {
    imprimirAjuda();
    return;
  }
  if (lower == "h" || lower == "home" || lower == "zero") {
    cmdHoming();
    return;
  }
  if (lower == "m" || lower == "max") {
    cmdMaximo();
    return;
  }
  if (lower == "s" || lower == "status") {
    imprimirStatus("[OK]");
    return;
  }
  if (lower == "t" || lower == "teste") {
    cmdVarredura();
    return;
  }
  if (lower == "v") {
    cmdValidacaoRapida();
    return;
  }
  if (lower == "a") {
    cmdAfericaoCompleta();
    return;
  }
#if POT_USA_DOIS_CHIPS
  if (lower == "aa") {
    cmdAferirChipA();
    return;
  }
  if (lower == "ab") {
    cmdAferirChipB();
    return;
  }
  if (lower == "ae") {
    cmdAferirReqRede();
    return;
  }
#endif
  if (lower == "l") {
    imprimirLimitesSugeridos();
    return;
  }

#if POT_USA_DOIS_CHIPS
  if (lower == "++") {
    cmdIncrementoIntercalado(1);
    return;
  }
  if (lower == "--") {
    cmdIncrementoIntercalado(-1);
    return;
  }

  if ((lower.startsWith("+") || lower.startsWith("-")) && lower.length() >= 2) {
    char chip = lower.charAt(lower.length() - 1);
    if (chip == 'a' || chip == 'b') {
      int delta = (lower.charAt(0) == '+') ? 1 : -1;
      String num = lower.substring(1, lower.length() - 1);
      if (num.length() > 0) {
        delta = num.toInt();
        if (lower.charAt(0) == '-') delta = -delta;
      }
      cmdPassoChipUnico(chip, delta);
      return;
    }
  }
#else
  if (lower == "+" || lower == "++") {
    cmdIncrementoUnico(1);
    return;
  }
  if (lower == "-") {
    cmdIncrementoUnico(-1);
    return;
  }
  if (lower.startsWith("+") || lower.startsWith("-")) {
    int delta = 0;
    if (parseIncrementoRelativo(lower, &delta)) {
      cmdIncrementoUnico(delta);
      return;
    }
  }
#endif

#if POT_USA_DOIS_CHIPS
  if (lower.startsWith("pa") || lower.startsWith("pb")) {
    char chip = lower.charAt(1);
    int passo = lower.substring(2).toInt();
    if (!passoValido(passo)) {
      Serial.print(F("[ERRO] Passo invalido (0.."));
      Serial.print(POT_PASSOS_MAX);
      Serial.println(F(")."));
      return;
    }
    uint8_t pa = atuador.passoAtualA();
    uint8_t pb = atuador.passoAtualB();
    if (chip == 'a') pa = (uint8_t)passo;
    else pb = (uint8_t)passo;
    cmdPassosAbsolutos(pa, pb, false);
    return;
  }
#endif

  if (lower.startsWith("p") && lower.length() > 1) {
    uint8_t pa = 0;
    uint8_t pb = 0;
    if (!parseDoisPassos(lower.substring(1), &pa, &pb)) {
      Serial.println(F("[ERRO] Use p128 ou p64/192."));
      return;
    }
    cmdPassosAbsolutos(pa, pb, false);
    return;
  }

  if (lower.startsWith("u")) {
    float pct = 0.0f;
    if (!parseFloatBr(lower.substring(1), &pct)) {
      Serial.println(F("[ERRO] Use u50 para 50 %."));
      return;
    }
    cmdPotenciaPercentual(pct);
    return;
  }

  if (lower.startsWith("r")) {
    float req = 0.0f;
    if (!parseFloatBr(lower.substring(1), &req)) {
      Serial.println(F("[ERRO] Use r120 para Req 120 kΩ."));
      return;
    }
    cmdReqAlvo(req);
    return;
  }

  float kohm = 0.0f;
  if (parseFloatBr(lower, &kohm)) {
#if POT_USA_DOIS_CHIPS
    Serial.println(F("[INFO] Modo duplo: use r<kΩ> para Req ou p<passo>."));
#else
    uint8_t passo = 0;
    passosDeReqEquivKohm(kohm, &passo, &passo);
    Serial.print(F("[INFO] kΩ alvo "));
    Serial.print(kohm, 1);
    Serial.print(F(" -> passo "));
    Serial.println(passo);
    cmdPassosAbsolutos(passo, 0, false);
#endif
    return;
  }

  Serial.println(F("[ERRO] Comando invalido. Digite ? para ajuda."));
}

// ---------------------------------------------------------------------------
// Arduino
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(SERIAL_VELOCIDADE);
  delay(500);

  Serial.println();
  Serial.println(F("========================================"));
#if POT_USA_DOIS_CHIPS && REDE_COM_RESISTOR_PARALELO
  Serial.println(F("  TESTE TPL0501 — 2 chips + resistor paralelo"));
#else
  Serial.print(F("  TESTE TPL0501 — "));
  Serial.println(potenciometroModoRedeNome());
#endif
  Serial.println(F("  Leia README.md | Serial 115200 NL"));
  Serial.println(F("========================================"));
  imprimirPinos();
  imprimirCabecalhoMapa();

  atuador.iniciar();
  cmdHoming();
  imprimirGuiaUso();
  Serial.println(F("Digite v para comecar o teste rapido, ou ? para ajuda."));
  Serial.print(F("> "));
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
  Serial.print(F("> "));
}
