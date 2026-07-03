/**
 * buzzer.h — Buzzer ativo (PINO_BUZZER)
 *
 * Padrões: clique encoder, confirmação, meta atingida, fora da meta.
 * Não bloqueia o loop — use atualizar() periodicamente.
 */

#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>

enum BuzzerPadrao {
  BUZZ_NENHUM,
  BUZZ_CLIQUE,           // 1 beep curto — rotação do encoder
  BUZZ_CONFIRMACAO,      // 2 beeps — botão do encoder
  BUZZ_META_ATINGIDA,    // 3 tons ascendentes — entrou na faixa do alvo
  BUZZ_FORA_DA_META,     // 3 tons descendentes — saiu da faixa do alvo
  BUZZ_REDE_PRESENTE,    // 1 tom agudo — zero-cross detectado (chuveiro ON)
  BUZZ_REDE_AUSENTE      // 1 tom grave — zero-cross perdido (chuveiro OFF)
};

class Buzzer {
public:
  void iniciar();
  void atualizar();

  void tocarClique();
  void tocarConfirmacao();
  void tocarMetaAtingida();
  void tocarForaDaMeta();
  void tocarRedePresente();
  void tocarRedeAusente();

private:
  BuzzerPadrao _padraoAtual;
  uint8_t _faseSequencia;       // alterna ligar/desligar tom
  unsigned long _proximaAcaoMs;
  unsigned long _proximaAcaoCliqueUs;
  bool _tocando;

  void iniciarPadrao(BuzzerPadrao padrao);
  void pararSinal();
  void iniciarPulsoClique();
  static uint16_t frequenciaHz(uint8_t indiceNota, BuzzerPadrao padrao);
  static uint16_t duracaoMs(uint8_t indiceNota, BuzzerPadrao padrao);
  static uint8_t quantidadeNotas(BuzzerPadrao padrao);
};

#endif // BUZZER_H
