#include "ajustes.h"
#include "chrome.h"
#include "grid.h"
#include "blocos.h"

void tela_ajustes(bitmap_t *bm, const vista_menu_t *v)
{
    gfx_limpa(bm, false);

    barra_t b = { v->titulo, v->hora, v->bateria, v->wifi, false, v->sinc };
    int y = chrome_barra(bm, &b) + 9;

    // O filete de cima, par do que fecha a última linha.
    const int larg = bm->l - GRID_MARGEM_DESTINO * 2;
    gfx_hlin(bm, GRID_MARGEM_DESTINO, y, larg, 1);
    y += 1;

    for (int i = 0; i < v->n; i++)
        y = bloco_destino(bm, y, larg, v->linhas[i].icone,
                          v->linhas[i].texto, v->sub[i], NULL,
                          i == v->cursor, true);

    chrome_rodape(bm, v->rodape_esq, v->rodape_dir, false);
}
