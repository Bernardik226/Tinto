#ifndef VISTA_OBRA_H
#define VISTA_OBRA_H
#include "../nucleo/estado.h"
#include "../tela/icones.h"
typedef struct {
    char barra[12], hora[9]; int bateria, wifi; icone_id sinc;
    char titulo[OBRA_TITULO], autor[OBRA_AUTOR], sinopse[OBRA_SINOPSE];
    char tipo[16], tempo[32], progresso[24], adicionada[20];
    int cursor;
} vista_obra_t;
void vista_obra(const estado_t *e, vista_obra_t *out);
#endif
