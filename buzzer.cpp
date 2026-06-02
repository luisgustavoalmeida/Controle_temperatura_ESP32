/**
 * buzzer.cpp — Máquina de estados para tocar melodias sem delay()
 *
 * Cada nota usa dois passos: ligar tom (tone) e silenciar (noTone).
 * _faseSequencia avança até quantidadeNotas * 2.
 */

#include "buzzer.h"
#include "config.h"

void Buzzer::iniciar() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  _padraoAtual = BUZZ_NENHUM;
  _faseSequencia = 0;
  _proximaAcaoMs = 0;
  _tocando = false;
  noTone(PIN_BUZZER);
}

void Buzzer::iniciarPadrao(BuzzerPadrao padrao) {
  if (padrao == BUZZ_CLIQUE && _tocando) {
    noTone(PIN_BUZZER);
  }
  _padraoAtual = padrao;
  _faseSequencia = 0;
  _tocando = true;
  _proximaAcaoMs = millis();
}

void Buzzer::tocarClique() {
  iniciarPadrao(BUZZ_CLIQUE);
}

void Buzzer::tocarConfirmacao() {
  iniciarPadrao(BUZZ_CONFIRMACAO);
}

void Buzzer::tocarMetaAtingida() {
  iniciarPadrao(BUZZ_META_ATINGIDA);
}

void Buzzer::tocarForaDaMeta() {
  iniciarPadrao(BUZZ_FORA_DA_META);
}

uint8_t Buzzer::quantidadeNotas(BuzzerPadrao padrao) {
  switch (padrao) {
    case BUZZ_CLIQUE: return 1;
    case BUZZ_CONFIRMACAO: return 2;
    case BUZZ_META_ATINGIDA: return 3;
    case BUZZ_FORA_DA_META: return 3;
    default: return 0;
  }
}

uint16_t Buzzer::frequenciaHz(uint8_t indiceNota, BuzzerPadrao padrao) {
  switch (padrao) {
    case BUZZ_CLIQUE:
      return 2000;
    case BUZZ_CONFIRMACAO:
      return (indiceNota == 0) ? 1800 : 2400;
    case BUZZ_META_ATINGIDA:
      if (indiceNota == 0) return 1500;
      if (indiceNota == 1) return 2000;
      return 2500;
    case BUZZ_FORA_DA_META:
      if (indiceNota == 0) return 2500;
      if (indiceNota == 1) return 2000;
      return 1500;
    default:
      return 0;
  }
}

uint16_t Buzzer::duracaoMs(uint8_t indiceNota, BuzzerPadrao padrao) {
  (void)indiceNota;
  switch (padrao) {
    case BUZZ_CLIQUE: return 30;
    case BUZZ_CONFIRMACAO: return 100;
    case BUZZ_META_ATINGIDA: return 120;
    case BUZZ_FORA_DA_META: return 140;
    default: return 0;
  }
}

void Buzzer::atualizar() {
  if (!_tocando) {
    return;
  }

  unsigned long agora = millis();
  if (agora < _proximaAcaoMs) {
    return;
  }

  uint8_t totalFases = quantidadeNotas(_padraoAtual) * 2;
  if (_faseSequencia >= totalFases) {
    noTone(PIN_BUZZER);
    _tocando = false;
    _padraoAtual = BUZZ_NENHUM;
    return;
  }

  if (_faseSequencia % 2 == 0) {
    uint8_t indiceNota = _faseSequencia / 2;
    tone(PIN_BUZZER, frequenciaHz(indiceNota, _padraoAtual));
    _proximaAcaoMs = agora + duracaoMs(indiceNota, _padraoAtual);
  } else {
    noTone(PIN_BUZZER);
    _proximaAcaoMs = agora + 40;  // pausa entre notas
  }
  _faseSequencia++;
}
