// jogos/xadrez.h — regras puras do xadrez, sem tela, cartão ou ESP-IDF.
#ifndef JOGOS_XADREZ_H
#define JOGOS_XADREZ_H

#include <stdbool.h>
#include <stdint.h>

#define XADREZ_MOV_MAX 256
#define XZ_CASA(coluna, linha) (((linha) - 1) * 8 + ((coluna) - 'a'))

enum {
    XZ_E2 = XZ_CASA('e', 2), XZ_E3 = XZ_CASA('e', 3),
    XZ_E4 = XZ_CASA('e', 4), XZ_E5 = XZ_CASA('e', 5),
};

typedef enum { XZ_BRANCAS = 0, XZ_PRETAS = 1 } xadrez_cor_t;
typedef enum { XZ_MODO_LOCAL = 0, XZ_MODO_MAQUINA = 1 } xadrez_modo_t;
typedef enum {
    XZ_VERTICAL = 0,
    XZ_HORIZONTAL_DIREITA,
    XZ_HORIZONTAL_ESQUERDA,
} xadrez_orientacao_t;
#define XZ_HORIZONTAL XZ_HORIZONTAL_DIREITA
typedef enum {
    XZ_NENHUMA = 0, XZ_PEAO, XZ_CAVALO, XZ_BISPO,
    XZ_TORRE, XZ_DAMA, XZ_REI
} xadrez_peca_t;

typedef struct {
    uint8_t de, para, promocao, flags;
} xadrez_mov_t;

typedef struct {
    uint8_t casa[64];
    uint8_t turno, roques;
    int8_t en_passant;
    uint16_t meio_lances, numero_lance;
    xadrez_mov_t ultimo;
    uint64_t repeticao[101];
    uint8_t n_repeticao;
} xadrez_pos_t;

typedef enum {
    XZ_EM_CURSO = 0,
    XZ_MATE_BRANCAS, XZ_MATE_PRETAS,
    XZ_EMPATE_AFOGAMENTO, XZ_EMPATE_50_LANCES,
    XZ_EMPATE_REPETICAO, XZ_EMPATE_MATERIAL, XZ_EMPATE_ACORDO
} xadrez_estado_t;

enum {
    XZ_HIST_CAPTURA = 1u, XZ_HIST_ROQUE = 2u,
    XZ_HIST_EN_PASSANT = 4u, XZ_HIST_PROMOCAO = 8u,
    XZ_HIST_XEQUE = 16u, XZ_HIST_MATE = 32u,
};

typedef struct {
    xadrez_mov_t lance;
    uint16_t numero;
    uint8_t peca, capturada, cor, marcas;
} xadrez_hist_item_t;

void xadrez_nova(xadrez_pos_t *p);
int xadrez_movimentos(const xadrez_pos_t *p, int origem,
                      xadrez_mov_t *out, int max);
bool xadrez_joga(xadrez_pos_t *p, xadrez_mov_t m);
bool xadrez_em_xeque(const xadrez_pos_t *p, xadrez_cor_t cor);
xadrez_estado_t xadrez_estado(const xadrez_pos_t *p);
int xadrez_navega_peca(const xadrez_pos_t *p, int atual, int dx, int dy);
int xadrez_navega_destino(const xadrez_pos_t *p, int origem, int atual,
                          int dx, int dy);

uint8_t xadrez_peca_em(const xadrez_pos_t *p, int casa);
xadrez_peca_t xadrez_tipo(uint8_t peca);
xadrez_cor_t xadrez_cor(uint8_t peca);

#endif
