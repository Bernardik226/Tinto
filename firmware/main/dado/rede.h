// dado/rede.h — a rede sem fio que o aparelho lembra. Uma só.
// O cartão guarda só o NOME (aparece na tela, não é segredo); a senha fica na
// NVS, pelo `hal->segredo_*` (ver hal/segredo_esp.c).
#ifndef DADO_REDE_H
#define DADO_REDE_H

#include "../nucleo/tipos.h"
#include "../hal/hal.h"

typedef struct {
    char nome[33];
    char senha[65];
} rede_salva_t;

// Ausente devolve ERR_ARQUIVO com a struct zerada: nunca ter visto Wi-Fi é
// normal.
erro_t rede_carrega(const hal_t *hal, rede_salva_t *out);
erro_t rede_grava  (const hal_t *hal, const rede_salva_t *rede);
erro_t rede_esquece(const hal_t *hal);

#endif
