#ifndef DADO_XADREZ_H
#define DADO_XADREZ_H

#include "../hal/hal.h"
#include "../jogos/xadrez.h"

typedef struct {
    xadrez_pos_t posicao;
    uint8_t modo, cor_humana, cor_baixo, dificuldade, orientacao, cursor;
    int8_t origem;
    xadrez_estado_t resultado;
    uint16_t n_lances, placar_a2, placar_b2;
    bool mostrar_ajuda;
} xadrez_salvo_t;

erro_t xadrez_carrega(const hal_t *hal, xadrez_salvo_t *out);
erro_t xadrez_salva_lance(const hal_t *hal, const xadrez_salvo_t *jogo,
                          xadrez_mov_t lance);
erro_t xadrez_salva_opcoes(const hal_t *hal, const xadrez_salvo_t *jogo);
erro_t xadrez_historico(const hal_t *hal, uint16_t deslocamento,
                        xadrez_hist_item_t *out, int max, uint16_t *total);
erro_t xadrez_apaga(const hal_t *hal);

#endif
