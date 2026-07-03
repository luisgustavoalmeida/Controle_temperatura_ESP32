/**
 * display_lcd.h — LCD 20×4 I2C (PCF8574)
 *
 * Layout:
 *   Linha 0 — MM:SS (00:00) + energia X.XXXkWh (meio) + "on" (cols 19-20 se ZC)
 *   Linha 1 — alvo (°C ou %)
 *   Linha 2 — temperatura medida
 *   Linha 3 — potência % + modo / estado
 */

#ifndef DISPLAY_LCD_H
#define DISPLAY_LCD_H

#include <Arduino.h>

enum EstadoSistema {
  ESTADO_INICIALIZANDO,
  ESTADO_PID_ATIVO,
  ESTADO_POTENCIA_ATIVO,
  ESTADO_SENSOR_ERRO,
  ESTADO_AGUARDE_SENSOR,
  ESTADO_CONTROLE_DESLIGADO
};

enum ModoControle {
  MODO_CONTROLE_PID,
  MODO_CONTROLE_POTENCIA
};

enum MensagemTransicao {
  MSG_NENHUMA,
  MSG_ATIVANDO_MALHA,
  MSG_DESATIVANDO_MALHA,
  MSG_TROCANDO_MODO,
  MSG_DESLIGA_INATIVIDADE
};

class DisplayLCD {
public:
  bool iniciar();
  void splashInicializacao();

  /** potenciaCmd01 = OUT 0..1 (PID ou potência manual) exibido como % na linha 3 */
  void atualizar(float setpointC, float alvoPotenciaPct, float atualC, float potenciaCmd01,
                 EstadoSistema estado,
                 ModoControle modoControle, bool metaAtingida, bool controleAtivo,
                 bool chuveiroEnergizado, MensagemTransicao msgTransicao,
                 bool setpointPendenteNaMalha,
                 int8_t direcaoAjusteAlvoTemp, uint32_t tempoUsoSeg, float energiaWh);

  /** Pisca backlight — rede/chuveiro presente (zero-cross). */
  void piscarRedePresente();
  void piscarRedeAusente();

  void invalidarCache();

  /** Pisca backlight na transição de meta (como o buzzer). */
  void piscarMetaAtingida();
  void piscarForaDaMeta();

  /** Giro do encoder religa backlight após timeout de inatividade. */
  void notificarAtividadeEncoder();

  /**
   * Economia com PID off + sequência de piscar; chamar a cada PERIODO_LOOP_MS.
   */
  void atualizarIluminacao(bool controleAtivo, unsigned long ultimaAtividadeEncoderMs);

private:
  enum PadraoPiscarIluminacao {
    PISCAR_NENHUM,
    PISCAR_META_ATINGIDA,
    PISCAR_FORA_DA_META,
    PISCAR_REDE_PRESENTE,
    PISCAR_REDE_AUSENTE
  };

  bool _iluminacaoLigada;
  bool _desligadoPorInatividade;
  bool _piscandoIluminacao;
  PadraoPiscarIluminacao _padraoPiscar;
  uint8_t _fasePiscar;
  unsigned long _proximaFasePiscarMs;

  void aplicarIluminacao(bool ligada);
  void iniciarPiscar(PadraoPiscarIluminacao padrao);
  static uint8_t quantidadePiscadas(PadraoPiscarIluminacao padrao);
  static uint16_t duracaoFasePiscarMs(uint8_t fase, PadraoPiscarIluminacao padrao);
  void finalizarPiscarIluminacao(unsigned long agora, bool controleAtivo,
                                 unsigned long ultimaAtividadeEncoderMs);

  bool _ok;
  float _ultimoSetpoint;
  float _ultimoAlvoPotencia;
  float _ultimoAtual;
  float _ultimaPotCmd;
  EstadoSistema _ultimoEstado;
  ModoControle _ultimoModoControle;
  bool _ultimaMeta;
  bool _ultimoControleAtivo;
  bool _ultimoChuveiroEnergizado;
  MensagemTransicao _ultimaMsgTransicao;
  uint8_t _ultimoFrameAnimPid;
  bool _ultimoSetpointPendente;
  int8_t _ultimaDirecaoAjusteAlvo;
  uint32_t _ultimoTempoUsoSeg;
  float _ultimaEnergiaWh;

  void escreverLinha(uint8_t linha, const char* texto);
  static void montarLinhaBuscandoTemp(char* buffer, size_t tam, float percentual,
                                      uint8_t frameAnim);
  static void montarLinhaUsoEnergia(char* buffer, size_t tam, uint32_t tempoSeg,
                                    float energiaWh, bool controleAtivo,
                                    bool chuveiroEnergizado);
};

#endif // DISPLAY_LCD_H
