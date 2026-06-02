/**
 * buzzer.h — Buzzer ativo no GPIO 32 (função tone() do Arduino)
 *
 * Todas as sequências são não bloqueantes: chame atualizar() no loop rápido.
 */

#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>

enum BuzzerPadrao {
  BUZZ_NENHUM,
  BUZZ_CLIQUE,           // 1 beep curto — rotação do encoder
  BUZZ_CONFIRMACAO,      // 2 beeps — botão do encoder
  BUZZ_META_ATINGIDA,    // 3 tons ascendentes — entrou na faixa do alvo
  BUZZ_FORA_DA_META      // 3 tons descendentes — saiu da faixa do alvo
};

class Buzzer {
public:
  void iniciar();
  void atualizar();

  void tocarClique();
  void tocarConfirmacao();
  void tocarMetaAtingida();
  void tocarForaDaMeta();

private:
  BuzzerPadrao _padraoAtual;
  uint8_t _faseSequencia;       // alterna ligar/desligar tom
  unsigned long _proximaAcaoMs;
  bool _tocando;

  void iniciarPadrao(BuzzerPadrao padrao);
  static uint16_t frequenciaHz(uint8_t indiceNota, BuzzerPadrao padrao);
  static uint16_t duracaoMs(uint8_t indiceNota, BuzzerPadrao padrao);
  static uint8_t quantidadeNotas(BuzzerPadrao padrao);
};

#endif // BUZZER_H
