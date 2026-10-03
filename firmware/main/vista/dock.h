// vista/dock.h — o Tinto deitado, carregando (416×240, horizontal).
// Olhado de longe: relógio maior e a linha do DEPOIS ("ainda dá tempo?").
#ifndef VISTA_DOCK_H
#define VISTA_DOCK_H

#include "../nucleo/estado.h"
#include "../tela/icones.h"

typedef struct {
    char faixa[40];       // "na dock · terça, 1 de setembro"
    char carga[24];       // "81% · carregando"
    char hora[9];
    char saudacao[20];    // "Boa tarde."

    char kicker[32];      // "próximo compromisso · em 2h43"
    char proximo[64];     // "14:00 · Dentista"
    char onde[48];
    char depois[64];      // "Depois: Aula de finlandês · 19:30"

    // Na dock não se opera: dizer isso evita apertar achando que quebrou.
    char travado[40];
} vista_dock_t;

void vista_dock(const estado_t *e, vista_dock_t *out);

#endif
