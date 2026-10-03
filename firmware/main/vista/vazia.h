// vista/vazia.h — a moldura de reserva. PURO.
// Tela sem conteúdo para mostrar (ex.: o item aberto sumiu) mostra barra e
// rodapé, em vez de um BACK que finge que nada aconteceu.
#ifndef VISTA_VAZIA_H
#define VISTA_VAZIA_H

#include "../nucleo/estado.h"
#include "../tela/icones.h"

typedef struct {
    char titulo[20];       // "TAREFA" — o nome da tela, em caps
    char hora[9];
    int  bateria;
    // -1 esconde o ícone do rádio: num aparelho que funciona offline, ícone
    // permanente seria lembrete do que não faz falta.
    int  wifi;
    icone_id sinc;   // o que a sincronização faz, como forma
    char aviso[32];
    char rodape_esq[22];
} vista_vazia_t;

void vista_vazia(const estado_t *e, vista_vazia_t *out);

#endif
