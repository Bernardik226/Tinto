#include "vazia.h"
#include "chrome.h"
#include <string.h>

void tela_vazia(bitmap_t *bm, const vista_vazia_t *v)
{
    memset(bm->bits, 0, (size_t)((bm->l + 7) / 8) * (size_t)bm->a);

    barra_t b = { v->titulo, v->hora, v->bateria, v->wifi, false, v->sinc };
    int y = chrome_barra(bm, &b);

    y += 14;
    gfx_texto(bm, 10, y, F_MIUDA, v->aviso);

    chrome_rodape(bm, v->rodape_esq, "", false);
}
