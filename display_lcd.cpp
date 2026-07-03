/**
 * display_lcd.cpp — Renderização do LCD com cache (evita flicker)
 *
 * Só redesenha linhas quando setpoint, PV, OUT ou estado mudam.
 * Animação "PID." na linha 4 usa LCD_ANIM_PID_MS (config.h).
 */

#include "display_lcd.h"
#include "config.h"
#include "lcd_i2c_compat.h"
#include <string.h>
#include <math.h>
#include <LiquidCrystal_I2C.h>

static LiquidCrystal_I2C lcd(LCD_I2C_CTOR_ARGS);

void DisplayLCD::escreverLinha(uint8_t linha, const char* texto) {
  lcd.setCursor(0, linha);
  lcd.print(texto);
  int len = strlen(texto);
  for (int i = len; i < LCD_COLUNAS; i++) {
    lcd.print(' ');
  }
}

void DisplayLCD::invalidarCache() {
  _ultimoSetpoint = -999.0f;
  _ultimoAlvoPotencia = -999.0f;
  _ultimoAtual = -999.0f;
  _ultimaPotCmd = -1.0f;
  _ultimoEstado = ESTADO_INICIALIZANDO;
  _ultimaMsgTransicao = MSG_NENHUMA;
  _ultimoFrameAnimPid = 255;
  _ultimoSetpointPendente = false;
  _ultimaDirecaoAjusteAlvo = 0;
  _ultimoTempoUsoSeg = 0xFFFFFFFFUL;
  _ultimaEnergiaWh = -1.0f;
  _ultimoChuveiroEnergizado = false;
}

void DisplayLCD::montarLinhaUsoEnergia(char* buffer, size_t tam, uint32_t tempoSeg,
                                       float energiaWh, bool controleAtivo,
                                       bool chuveiroEnergizado) {
  (void)controleAtivo;

  if (tempoSeg > 359999UL) {
    tempoSeg = 359999UL;
  }

  uint32_t totalMinutos = tempoSeg / 60UL;
  uint32_t segRest = tempoSeg % 60UL;

  char tempo[12];
  snprintf(tempo, sizeof(tempo), "%02lu:%02lu",
           (unsigned long)totalMinutos, (unsigned long)segRest);

  char energia[12];
  snprintf(energia, sizeof(energia), "%.3fkWh", energiaWh / 1000.0f);

  const char* ligado = chuveiroEnergizado ? "on" : "  ";

  // 8 + 8 + 2 espacos + 2 = 20 colunas (posicoes estaveis com/sem zero-cross)
  snprintf(buffer, tam, "%-8s%-8s  %-2s", tempo, energia, ligado);
}

void DisplayLCD::montarLinhaBuscandoTemp(char* buffer, size_t tam, float percentual,
                                         uint8_t frameAnim) {
  static const char* sufixos[] = {"", ".", "..", "..."};
  frameAnim %= 4;
  snprintf(buffer, tam, "Pot:%5.1f%% | PID%s", percentual, sufixos[frameAnim]);
}

void DisplayLCD::aplicarIluminacao(bool ligada) {
  if (ligada) {
    lcdI2cBacklightOn(lcd);
  } else {
    lcdI2cBacklightOff(lcd);
  }
  _iluminacaoLigada = ligada;
}

uint8_t DisplayLCD::quantidadePiscadas(PadraoPiscarIluminacao padrao) {
  switch (padrao) {
    case PISCAR_META_ATINGIDA:
    case PISCAR_FORA_DA_META:
    case PISCAR_REDE_PRESENTE:
    case PISCAR_REDE_AUSENTE:
      return 3;
    default:
      return 0;
  }
}

uint16_t DisplayLCD::duracaoFasePiscarMs(uint8_t fase, PadraoPiscarIluminacao padrao) {
  if (fase % 2 == 0) {
    if (padrao == PISCAR_FORA_DA_META || padrao == PISCAR_REDE_AUSENTE) {
      return LCD_PISCAR_FORA_META_LIGADO_MS;
    }
    return LCD_PISCAR_META_LIGADO_MS;
  }
  return LCD_PISCAR_META_PAUSA_MS;
}

void DisplayLCD::iniciarPiscar(PadraoPiscarIluminacao padrao) {
  _padraoPiscar = padrao;
  _fasePiscar = 0;
  _piscandoIluminacao = true;
  _desligadoPorInatividade = false;
  _proximaFasePiscarMs = millis();
}

void DisplayLCD::finalizarPiscarIluminacao(unsigned long agora, bool controleAtivo,
                                           unsigned long ultimaAtividadeEncoderMs) {
  _piscandoIluminacao = false;
  _padraoPiscar = PISCAR_NENHUM;
  if (!controleAtivo &&
      (agora - ultimaAtividadeEncoderMs) >= LCD_BACKLIGHT_INATIVIDADE_MS) {
    _desligadoPorInatividade = true;
    aplicarIluminacao(false);
    return;
  }
  _desligadoPorInatividade = false;
  aplicarIluminacao(true);
}

void DisplayLCD::piscarMetaAtingida() {
  iniciarPiscar(PISCAR_META_ATINGIDA);
}

void DisplayLCD::piscarForaDaMeta() {
  iniciarPiscar(PISCAR_FORA_DA_META);
}

void DisplayLCD::piscarRedePresente() {
  iniciarPiscar(PISCAR_REDE_PRESENTE);
}

void DisplayLCD::piscarRedeAusente() {
  iniciarPiscar(PISCAR_REDE_AUSENTE);
}

void DisplayLCD::notificarAtividadeEncoder() {
  _desligadoPorInatividade = false;
  if (!_piscandoIluminacao && !_iluminacaoLigada) {
    aplicarIluminacao(true);
  }
}

void DisplayLCD::atualizarIluminacao(bool controleAtivo,
                                     unsigned long ultimaAtividadeEncoderMs) {
  unsigned long agora = millis();

  if (_piscandoIluminacao) {
    if (agora < _proximaFasePiscarMs) {
      return;
    }

    uint8_t totalFases = quantidadePiscadas(_padraoPiscar) * 2;
    if (_fasePiscar >= totalFases) {
      finalizarPiscarIluminacao(agora, controleAtivo, ultimaAtividadeEncoderMs);
      return;
    }

    aplicarIluminacao(_fasePiscar % 2 == 0);
    _proximaFasePiscarMs = agora + duracaoFasePiscarMs(_fasePiscar, _padraoPiscar);
    _fasePiscar++;
    return;
  }

  if (controleAtivo) {
    if (_desligadoPorInatividade || !_iluminacaoLigada) {
      _desligadoPorInatividade = false;
      aplicarIluminacao(true);
    }
    return;
  }

  if ((agora - ultimaAtividadeEncoderMs) >= LCD_BACKLIGHT_INATIVIDADE_MS) {
    if (!_desligadoPorInatividade) {
      _desligadoPorInatividade = true;
      aplicarIluminacao(false);
    }
    return;
  }

  if (_desligadoPorInatividade) {
    return;
  }

  if (!_iluminacaoLigada) {
    aplicarIluminacao(true);
  }
}

bool DisplayLCD::iniciar() {
  lcdI2cWireBegin();
  lcdI2cIniciar(lcd);
  _ok = true;
  invalidarCache();
  _ultimaMeta = false;
  _ultimoControleAtivo = true;
  _ultimoFrameAnimPid = 255;
  _iluminacaoLigada = true;
  _desligadoPorInatividade = false;
  _piscandoIluminacao = false;
  _padraoPiscar = PISCAR_NENHUM;
  _fasePiscar = 0;
  _proximaFasePiscarMs = 0;
  return _ok;
}

void DisplayLCD::splashInicializacao() {
  lcd.clear();
  escreverLinha(0, "Controle Temperatura");
  escreverLinha(1, "ESP32 + Dimmer");
  escreverLinha(2, "Inicializando...");
  escreverLinha(3, "I2C 0x27 OK");
}

void DisplayLCD::atualizar(float setpointC, float alvoPotenciaPct, float atualC,
                           float potenciaCmd01, EstadoSistema estado, ModoControle modoControle,
                           bool metaAtingida,
                           bool controleAtivo, bool chuveiroEnergizado,
                           MensagemTransicao msgTransicao,
                           bool setpointPendenteNaMalha, int8_t direcaoAjusteAlvoTemp,
                           uint32_t tempoUsoSeg, float energiaWh) {
  if (!_ok) {
    return;
  }

  bool modoPotencia = (modoControle == MODO_CONTROLE_POTENCIA);
  bool buscandoTemperatura =
      !modoPotencia && controleAtivo && (estado == ESTADO_PID_ATIVO) &&
      !metaAtingida && (msgTransicao == MSG_NENHUMA);
  uint8_t frameAnimPid =
      buscandoTemperatura
          ? (uint8_t)((millis() / LCD_ANIM_PID_MS) % 4)
          : 0;

  bool tempoOuEnergiaMudou =
      (tempoUsoSeg != _ultimoTempoUsoSeg) ||
      (fabsf(energiaWh - _ultimaEnergiaWh) > 0.05f);

  bool precisaAtualizar =
      tempoOuEnergiaMudou ||
      (fabsf(setpointC - _ultimoSetpoint) > 0.001f) ||
      (fabsf(alvoPotenciaPct - _ultimoAlvoPotencia) > 0.01f) ||
      (fabsf(atualC - _ultimoAtual) > 0.01f) ||
      (fabsf(potenciaCmd01 - _ultimaPotCmd) > 0.001f) ||
      (estado != _ultimoEstado) || (modoControle != _ultimoModoControle) ||
      (metaAtingida != _ultimaMeta) || (controleAtivo != _ultimoControleAtivo) ||
      (chuveiroEnergizado != _ultimoChuveiroEnergizado) ||
      (msgTransicao != _ultimaMsgTransicao) ||
      (setpointPendenteNaMalha != _ultimoSetpointPendente) ||
      (direcaoAjusteAlvoTemp != _ultimaDirecaoAjusteAlvo) ||
      (buscandoTemperatura && frameAnimPid != _ultimoFrameAnimPid);

  if (!precisaAtualizar) {
    return;
  }

  _ultimoSetpoint = setpointC;
  _ultimoAlvoPotencia = alvoPotenciaPct;
  _ultimoAtual = atualC;
  _ultimaPotCmd = potenciaCmd01;
  _ultimoEstado = estado;
  _ultimoModoControle = modoControle;
  _ultimaMeta = metaAtingida;
  _ultimoControleAtivo = controleAtivo;
  _ultimoChuveiroEnergizado = chuveiroEnergizado;
  _ultimaMsgTransicao = msgTransicao;
  _ultimoFrameAnimPid = frameAnimPid;
  _ultimoSetpointPendente = setpointPendenteNaMalha;
  _ultimaDirecaoAjusteAlvo = direcaoAjusteAlvoTemp;
  _ultimoTempoUsoSeg = tempoUsoSeg;
  _ultimaEnergiaWh = energiaWh;

  char buffer[21];

  montarLinhaUsoEnergia(buffer, sizeof(buffer), tempoUsoSeg, energiaWh, controleAtivo,
                        chuveiroEnergizado);
  escreverLinha(0, buffer);

  if (modoPotencia) {
    float fracPot = fabsf(alvoPotenciaPct - roundf(alvoPotenciaPct));
    if (fracPot < 0.05f) {
      snprintf(buffer, sizeof(buffer), "Alvo: %5.0f %%", alvoPotenciaPct);
    } else {
      snprintf(buffer, sizeof(buffer), "Alvo: %5.1f %%", alvoPotenciaPct);
    }
  } else if (setpointPendenteNaMalha) {
    char seta = (direcaoAjusteAlvoTemp < 0) ? '<' : '>';
    snprintf(buffer, sizeof(buffer), "Alvo: %6.2f %c", setpointC, seta);
  } else {
    snprintf(buffer, sizeof(buffer), "Alvo: %6.2f C", setpointC);
  }
  escreverLinha(1, buffer);

  if (estado == ESTADO_SENSOR_ERRO) {
    escreverLinha(2, "Atual: sensor falhou");
  } else if (estado == ESTADO_AGUARDE_SENSOR) {
    escreverLinha(2, "Atual: aguarde leit.");
  } else if (isnan(atualC)) {
    escreverLinha(2, "Atual:   --- C");
  } else {
    snprintf(buffer, sizeof(buffer), "Atual: %6.2f C", atualC);
    escreverLinha(2, buffer);
  }

  float pctCmd = potenciaCmd01 * 100.0f;
  if (pctCmd < 0.0f) {
    pctCmd = 0.0f;
  }
  if (pctCmd > 100.0f) {
    pctCmd = 100.0f;
  }

  if (estado == ESTADO_SENSOR_ERRO) {
    escreverLinha(3, "Pot: MIN | FALHA SENS");
  } else if (!controleAtivo || estado == ESTADO_CONTROLE_DESLIGADO) {
    if (modoPotencia) {
      escreverLinha(3, "Pot:   0% | POT OFF");
    } else {
      escreverLinha(3, "Pot:   0% | PID OFF");
    }
  } else if (msgTransicao == MSG_ATIVANDO_MALHA) {
    if (modoPotencia) {
      escreverLinha(3, "POT: iniciando...");
    } else {
      escreverLinha(3, "PID: iniciando...");
    }
  } else if (msgTransicao == MSG_DESATIVANDO_MALHA) {
    if (modoPotencia) {
      escreverLinha(3, "POT: encerrando...");
    } else {
      escreverLinha(3, "PID: encerrando...");
    }
  } else if (msgTransicao == MSG_DESLIGA_INATIVIDADE) {
    escreverLinha(3, "40 min sem uso");
  } else if (msgTransicao == MSG_TROCANDO_MODO) {
    if (modoPotencia) {
      escreverLinha(3, "Modo: POTENCIA");
    } else {
      escreverLinha(3, "Modo: PID");
    }
  } else if (modoPotencia) {
    snprintf(buffer, sizeof(buffer), "Pot:%5.1f%% | POT ON", pctCmd);
    escreverLinha(3, buffer);
  } else if (metaAtingida) {
    snprintf(buffer, sizeof(buffer), "Pot:%5.1f%% | Temp OK", pctCmd);
    escreverLinha(3, buffer);
  } else {
    montarLinhaBuscandoTemp(buffer, sizeof(buffer), pctCmd, frameAnimPid);
    escreverLinha(3, buffer);
  }
}
