#ifndef VISTA_XADREZ_H
#define VISTA_XADREZ_H

#include "../nucleo/estado.h"
#include "../tela/icones.h"
#include <stddef.h>

typedef struct {
    xadrez_pagina_t pagina;
    char hora[9], titulo[24], subtitulo[48], contexto[48], ajuda[48];
    int bateria, wifi;
    icone_id sinc;
    uint8_t casa[64], turno, cursor, menu_cursor, promocao, cor_baixo, rei_xeque;
    uint8_t modo, dificuldade, orientacao, cor_humana;
    uint16_t n_lances, historico_total, historico_desloc, placar_a2, placar_b2;
    xadrez_hist_item_t historico[4];
    int8_t origem;
    uint64_t destinos;
    xadrez_mov_t ultimo;
    xadrez_estado_t resultado;
    bool tem_salva, salvamento_falhou, maquina_pensando, mostrar_ajuda;
} vista_xadrez_t;

void vista_jogos(const estado_t *e, vista_xadrez_t *out);
void vista_xadrez(const estado_t *e, vista_xadrez_t *out);
void vista_xadrez_descreve_lance(const xadrez_hist_item_t *h,
                                 char *codigo, size_t codigo_cap,
                                 char *descricao, size_t descricao_cap,
                                 char *acao, size_t acao_cap);

#endif
