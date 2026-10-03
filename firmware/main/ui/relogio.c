#include "relogio.h"
#include "chrome.h"
#include "grid.h"
#include <string.h>

#define MARGEM GRID_MARGEM

// Os cinco campos numa linha, com separadores que dizem o que são
// (27/08/2026  10:02). O campo sob o cursor em negativo.
static int campos(bitmap_t *bm, int y, const vista_relogio_t *v)
{
    static const char *SEPARADOR[5] = { "/", "/", "", ":", "" };

    int alt = gfx_altura_linha(F_TITULO);
    int larg = 0;
    for (int i = 0; i < 5; i++) {
        larg += gfx_largura(F_TITULO, v->valor[i]);
        larg += gfx_largura(F_TITULO, SEPARADOR[i]);
        if (i == 2) larg += 12;   // o ar entre a data e a hora
    }

    int x = (bm->l - larg) / 2;
    for (int i = 0; i < 5; i++) {
        int w = gfx_largura(F_TITULO, v->valor[i]);
        gfx_texto(bm, x, y, F_TITULO, v->valor[i]);
        if (i == v->campo) gfx_negativo(bm, x - 2, y - 2, w + 4, alt + 4);
        x += w;
        x = gfx_texto(bm, x, y, F_TITULO, SEPARADOR[i]);
        if (i == 2) x += 12;
    }
    return y + alt;
}

void tela_relogio(bitmap_t *bm, const vista_relogio_t *v)
{
    memset(bm->bits, 0, (size_t)bm->passo * (size_t)bm->a);

    barra_t b = { v->titulo, v->hora, v->bateria, v->wifi, false, v->sinc };
    int y = chrome_barra(bm, &b) + 14;
    int larg = bm->l - MARGEM * 2;

    // O interruptor primeiro: decide se o resto é editável. É LINHA NAVEGÁVEL,
    // com a moldura do cursor.
    if (v->no_interruptor)
        chrome_cursor(bm, y, gfx_altura_linha(F_CORPO));

    gfx_texto(bm, MARGEM, y, F_CORPO, "Hora pela internet");
    int w = gfx_largura(F_CORPO_P, v->interruptor);
    gfx_texto(bm, bm->l - MARGEM - w, y, F_CORPO_P, v->interruptor);
    y += gfx_altura_linha(F_CORPO) + 8;

    gfx_hlin(bm, MARGEM, y, larg, 1);
    y += 10;

    // O FUSO entre o interruptor e os campos: com a rede mandando, o NTP dá o
    // instante e o fuso diz que horas é aqui.
    if (v->no_fuso) chrome_cursor(bm, y, gfx_altura_linha(F_CORPO));
    gfx_texto(bm, MARGEM, y, F_CORPO, "Fuso");
    {
        int w = gfx_largura(F_CORPO, v->fuso);
        gfx_texto(bm, bm->l - MARGEM - w, y, F_CORPO, v->fuso);
    }
    y += gfx_altura_linha(F_CORPO) + 8;

    gfx_hlin(bm, MARGEM, y, larg, 1);
    y += 14;

    y = campos(bm, y, v) + 14;

    if (v->nota[0]) {
        gfx_hlin(bm, MARGEM, y, larg, 1);
        gfx_paragrafo(bm, MARGEM, y + 8, larg, 3, F_MIUDA, v->nota);
    }

    chrome_rodape(bm, v->rodape_esq, v->rodape_dir, false);
}
