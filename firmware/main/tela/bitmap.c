#include "bitmap.h"
#include <string.h>

void bitmap_liga(bitmap_t *bm, uint8_t *memoria, int l, int a)
{
    bm->bits  = memoria;
    bm->l     = l;
    bm->a     = a;
    bm->passo = (l + 7) / 8;
}

void gfx_limpa(bitmap_t *bm, bool tinta)
{
    memset(bm->bits, tinta ? 0xFF : 0x00, (size_t)bm->passo * bm->a);
}

void gfx_pixel(bitmap_t *bm, int x, int y, bool tinta)
{
    if (x < 0 || y < 0 || x >= bm->l || y >= bm->a) return;
    uint8_t *b = &bm->bits[(size_t)y * bm->passo + x / 8];
    uint8_t  m = (uint8_t)(0x80u >> (x % 8));
    if (tinta) *b |= m; else *b &= (uint8_t)~m;
}

bool gfx_le(const bitmap_t *bm, int x, int y)
{
    if (x < 0 || y < 0 || x >= bm->l || y >= bm->a) return false;
    return (bm->bits[(size_t)y * bm->passo + x / 8] >> (7 - x % 8)) & 1;
}

void gfx_ret(bitmap_t *bm, int x, int y, int l, int a, bool preenche)
{
    if (l <= 0 || a <= 0) return;
    if (preenche) {
        for (int j = y; j < y + a; j++)
            for (int i = x; i < x + l; i++)
                gfx_pixel(bm, i, j, true);
        return;
    }
    gfx_hlin(bm, x, y,         l, 1);
    gfx_hlin(bm, x, y + a - 1, l, 1);
    gfx_vlin(bm, x,         y, a, 1);
    gfx_vlin(bm, x + l - 1, y, a, 1);
}

void gfx_hlin(bitmap_t *bm, int x, int y, int comprimento, int espessura)
{
    for (int j = 0; j < espessura; j++)
        for (int i = 0; i < comprimento; i++)
            gfx_pixel(bm, x + i, y + j, true);
}

void gfx_vlin(bitmap_t *bm, int x, int y, int altura, int espessura)
{
    for (int i = 0; i < espessura; i++)
        for (int j = 0; j < altura; j++)
            gfx_pixel(bm, x + i, y + j, true);
}

void gfx_negativo(bitmap_t *bm, int x, int y, int l, int a)
{
    for (int j = y; j < y + a; j++)
        for (int i = x; i < x + l; i++)
            gfx_pixel(bm, i, j, !gfx_le(bm, i, j));
}

void gfx_gira_horario(bitmap_t *destino, const bitmap_t *origem)
{
    gfx_limpa(destino, false);
    for (int y = 0; y < origem->a; y++)
        for (int x = 0; x < origem->l; x++)
            if (gfx_le(origem, x, y))
                gfx_pixel(destino, destino->l - 1 - y, x, true);
}

void gfx_limpa_ret(bitmap_t *bm, int x, int y, int l, int a)
{
    for (int j = y; j < y + a; j++)
        for (int i = x; i < x + l; i++)
            gfx_pixel(bm, i, j, false);
}

void gfx_esmaece(bitmap_t *bm, int x, int y, int l, int a, int passo)
{
    if (passo < 2) return;          // 1 manteria tudo: nada a fazer

    for (int j = y; j < y + a; j++)
        for (int i = x; i < x + l; i++)
            // `(i + j) % passo` é a diagonal. Só apaga o que já é tinta.
            if (((i + j) % passo) != 0 && gfx_le(bm, i, j))
                gfx_pixel(bm, i, j, false);
}
