// hal/segredo_esp.h — o cofre, por uma porta de tipos primitivos.
//
// `int` e não `erro_t` (colisão com o lwip); quem traduz é o `hal_esp.c`.
// 0 = deu certo, -1 = não. Chave ausente é -1 com o destino zerado.
#ifndef HAL_SEGREDO_ESP_H
#define HAL_SEGREDO_ESP_H

#include <stddef.h>

int hal_segredo_grava(const char *chave, const char *valor);
int hal_segredo_le(const char *chave, char *out, size_t max);
int hal_segredo_apaga(const char *chave);

#endif
