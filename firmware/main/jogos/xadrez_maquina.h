// Oponente local determinístico. A busca é retomável: cada passo visita
// no máximo a fatia pedida e nunca segura o laço do aparelho até terminar.
#ifndef JOGOS_XADREZ_MAQUINA_H
#define JOGOS_XADREZ_MAQUINA_H

#include "xadrez.h"

typedef enum { XZM_FACIL = 0, XZM_MEDIA, XZM_DIFICIL } xadrez_dificuldade_t;
typedef enum { XZM_BUSCANDO = 0, XZM_PRONTO, XZM_SEM_LANCE }
    xadrez_maquina_estado_t;

typedef struct {
    xadrez_pos_t raiz, apos_raiz;
    xadrez_mov_t candidatos[XADREZ_MOV_MAX];
    xadrez_mov_t respostas[XADREZ_MOV_MAX];
    xadrez_mov_t melhor;
    uint32_t nos, teto;
    int melhor_valor, pior_resposta;
    uint16_t n_candidatos, candidato, n_respostas, resposta;
    uint8_t dificuldade, cor;
    bool raiz_aberta;
    xadrez_maquina_estado_t estado;
} xadrez_maquina_t;

void xadrez_maquina_inicia(xadrez_maquina_t *m, const xadrez_pos_t *p,
                           xadrez_dificuldade_t dificuldade);
xadrez_maquina_estado_t xadrez_maquina_passo(xadrez_maquina_t *m,
                                              uint16_t fatia_nos,
                                              xadrez_mov_t *out);

#endif
