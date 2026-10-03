// tela/bitmap.h — o buffer de 1 bit e as primitivas de desenho. PURO.
// Não conhece o Tinto. Bit 1 = PRETO, como o e-ink pensa.
#ifndef TELA_BITMAP_H
#define TELA_BITMAP_H

#include "../nucleo/tipos.h"

// O painel em pé. Deitado na dock: 416 × 240 (RN-3C).
#define TELA_L 240
#define TELA_A 416

typedef struct {
    uint8_t *bits;   // 1bpp, MSB primeiro
    int      l, a;   // largura, altura em pixels
    int      passo;  // bytes por linha = (l + 7) / 8
} bitmap_t;

void bitmap_liga(bitmap_t *bm, uint8_t *memoria, int l, int a);

// Toda função de desenho recebe um y e devolve o de baixo; as primitivas
// são a exceção, desenham onde mandam.
void gfx_limpa (bitmap_t *bm, bool tinta);

// Apaga uma região: o chão de um pop-over antes de desenhar por cima.
void gfx_limpa_ret(bitmap_t *bm, int x, int y, int l, int a);
void gfx_pixel (bitmap_t *bm, int x, int y, bool tinta);
bool gfx_le    (const bitmap_t *bm, int x, int y);
void gfx_ret   (bitmap_t *bm, int x, int y, int l, int a, bool preenche);
void gfx_hlin  (bitmap_t *bm, int x, int y, int comprimento, int espessura);
void gfx_vlin  (bitmap_t *bm, int x, int y, int altura, int espessura);
void gfx_negativo(bitmap_t *bm, int x, int y, int l, int a);
void gfx_gira_horario(bitmap_t *destino, const bitmap_t *origem);

// ── cinza num painel sem cinza ──────────────────────────────────────
// Mantém 1 pixel a cada `passo` numa trama DIAGONAL e apaga o resto. Em
// xadrez de colunas uma haste inteira cai ou fica; na diagonal todo traço
// perde a mesma fração. 2 é meio-tom, 3 o cinza claro que o e-ink segura;
// acima de 4 vira textura.
void gfx_esmaece(bitmap_t *bm, int x, int y, int l, int a, int passo);

#endif
