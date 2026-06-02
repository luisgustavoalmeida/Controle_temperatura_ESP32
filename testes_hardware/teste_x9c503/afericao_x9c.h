/**
 * afericao_x9c.h — tipos do modo afericao (header separado para Arduino IDE)
 */

#ifndef AFERICAO_X9C_H
#define AFERICAO_X9C_H

#include <Arduino.h>

#define AFERIR_MAX_LINHAS    56
#define MEDIDA_NAO_INFORMADA (-1.0f)

struct AfericaoLinha {
  char id[5];
  char secao[14];
  char cmd[14];
  uint8_t passoFw;
  float estKohm;
  float medKohm;
};

#endif // AFERICAO_X9C_H
