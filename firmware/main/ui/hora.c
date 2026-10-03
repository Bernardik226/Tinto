#include "hora.h"
#include "chrome.h"
#include "grid.h"
#include "../tela/texto.h"
#include <string.h>

// ── o mostrador ──────────────────────────────────────────────────────
// Duas casas centradas: o número É a tela. A casa sob o cursor em NEGATIVO,
// como nos campos de Data e hora.
void tela_mostrador(bitmap_t *bm, const vista_mostrador_t *v)
{
    gfx_limpa(bm, false);

    barra_t b = { v->titulo, v->hora, v->bateria, v->wifi, false, v->sinc };
    chrome_barra(bm, &b);

    int y = GRID_BARRA_A + 20;

    // O kicker e o nome do item: sem eles é um relógio sem dono.
    gfx_texto(bm, GRID_MARGEM, y, F_MIUDA, v->kicker);
    y += gfx_altura_linha(F_MIUDA) + 2;
    gfx_texto_ate(bm, GRID_MARGEM, y, F_CORPO, v->nome, bm->l - GRID_MARGEM * 2);
    y += gfx_altura_linha(F_CORPO) + 26;

    // A largura antes de desenhar: casas e dois pontos são um bloco centrado.
    const int wc = gfx_largura(F_TITULO, "00");
    const int wd = gfx_largura(F_TITULO, ":");
    const int vao = 8;
    const int larg = wc * 2 + wd + vao * 2;

    int x = (bm->l - larg) / 2;
    const int alt = gfx_altura_linha(F_TITULO);

    for (int c = 0; c < 2; c++) {
        int cx = c == 0 ? x : x + wc + vao + wd + vao;

        // Número sobre o papel e o negativo por cima.
        gfx_texto(bm, cx, y, F_TITULO, v->casa[c]);
        if (v->campo == c) gfx_negativo(bm, cx - 5, y - 4, wc + 10, alt + 8);
    }

    gfx_texto(bm, x + wc + vao, y, F_TITULO, ":");

    // As legendas sob cada casa: sem elas "07 30" é um número partido.
    y += alt + 6;
    gfx_texto(bm, x, y, F_MIUDA, "hora");
    gfx_texto(bm, x + wc + vao + wd + vao, y, F_MIUDA, "min");

    chrome_rodape(bm, v->rodape_esq, v->rodape_dir, false);
}
