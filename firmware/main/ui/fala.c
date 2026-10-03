#include "fala.h"
#include "chrome.h"
#include "grid.h"
#include <string.h>

#define MARGEM GRID_MARGEM
#define BARRA_H 9

// Um card: o mês, o usado em grande, a barra do que resta e quando renova.
void tela_fala(bitmap_t *bm, const vista_fala_t *v)
{
    gfx_limpa(bm, false);

    barra_t b = { v->titulo, v->hora, v->bateria, v->wifi, false, v->sinc };
    int y = chrome_barra(bm, &b) + 9;

    const int cx    = GRID_MARGEM_DESTINO;
    const int clarg = bm->l - GRID_MARGEM_DESTINO * 2;
    const int util  = clarg - 16;
    int topo = y;
    y += 8;

    gfx_texto_ate(bm, cx + 8, y, F_MIUDA, v->mes, util);
    y += gfx_altura_linha(F_MIUDA) + 4;

    // A fonte grande só tem dígitos: nada de `gfx_texto_ate` (o "…" sairia
    // quadrado). Ao lado só a unidade; o resto em linhas próprias.
    int x = gfx_texto(bm, cx + 8, y, F_ENORME, v->usados);
    if (v->unidade[0]) {
        int na_base = y + gfx_ascent(F_ENORME) - gfx_ascent(F_CORPO_P);
        gfx_texto(bm, x + 6, na_base, F_CORPO_P, v->unidade);
    }
    y += gfx_altura_linha(F_ENORME);
    gfx_texto_ate(bm, cx + 8, y, F_CORPO_P, v->de, util);
    y += gfx_altura_linha(F_CORPO_P) + 6;

    if (v->restam[0]) {
        gfx_ret(bm, cx + 8, y, util, BARRA_H, false);
        int cheio = (util - 2) * v->pct / 100;
        if (cheio > 0) gfx_ret(bm, cx + 9, y + 1, cheio, BARRA_H - 2, true);
        y += BARRA_H + 4;
        gfx_texto_ate(bm, cx + 8, y, F_MIUDA, v->restam, util);
        y += gfx_altura_linha(F_MIUDA);
    }
    if (v->renova[0]) {
        gfx_texto_ate(bm, cx + 8, y, F_MIUDA, v->renova, util);
        y += gfx_altura_linha(F_MIUDA);
    }
    y += 6;

    gfx_ret(bm, cx, topo, clarg, y - topo, false);
    y += 12;

    gfx_paragrafo(bm, MARGEM, y, bm->l - MARGEM * 2, 3, F_MIUDA, v->explica);

    chrome_rodape(bm, v->rodape_esq, "", false);
}
