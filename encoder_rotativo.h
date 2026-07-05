/**
 * encoder_rotativo.h — Encoder KY-040 (CLK, DT, SW)
 *
 * Quadratura (CLK) e botão (SW) em ISR; debounce e gestos no loop.
 * Giro normal / fino (botao+giro), clique, duplo clique, clique longo e
 * segurar 3 s sem girar (troca modo PID ↔ potência).
 */

#ifndef ENCODER_ROTATIVO_H
#define ENCODER_ROTATIVO_H

#include <Arduino.h>

enum ModoAjusteEncoder {
  ENCODER_AJUSTE_TEMPERATURA,
  ENCODER_AJUSTE_POTENCIA
};

class EncoderRotativo {
public:
  void iniciar();
  void atualizar();

  void definirModoAjuste(ModoAjusteEncoder modo);
  ModoAjusteEncoder modoAjuste() const;

  float setpointC() const;
  void definirSetpoint(float c);

  float alvoPotenciaPercent() const;
  void definirAlvoPotenciaPercent(float pct);

  bool consumirEventoRotacao();
  bool consumirEventoClique();
  bool consumirEventoDuploClique();
  bool consumirEventoCliqueLongo();
  bool consumirEventoTrocaModo();

private:
  volatile int _delta;
  volatile bool _cliquePendente;
  volatile bool _duploCliquePendente;
  volatile bool _cliqueLongoPendente;
  volatile bool _trocaModoPendente;
  int _passosGiroPendentes;
  bool _aguardandoPossivelDuplo;
  ModoAjusteEncoder _modoAjuste;
  float _setpoint;
  float _alvoPotenciaPct;
  unsigned long _ultimoRotacaoMs;
  unsigned long _ultimoCliqueMs;
  unsigned long _millisUltimoSoltarBotao;
  bool _botaoEstavaPressionado;
  unsigned long _millisBotaoPressionado;
  bool _giroComBotaoPressionado;
  bool _trocaModoJaDisparou;
  int _acumuladorDetente;

  volatile bool _botaoEstavelPressionado;
  volatile bool _bordaBotaoPendente;
  volatile int8_t _botaoLeituraIsr;
  volatile unsigned long _botaoBordaMs;

  void processarBotao(unsigned long agora);
  void registrarSoltarBotao(unsigned long agora, unsigned long duracao);
  void aplicarPassoGiro(int passos, bool botaoPressionado);

  static void isrEncoder();
  static void isrBotao();
};

#endif // ENCODER_ROTATIVO_H
