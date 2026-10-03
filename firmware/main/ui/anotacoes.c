#include "anotacoes.h"
#include "chrome.h"
#include "grid.h"
#include "blocos.h"
#include <stdio.h>
#include <string.h>

#define MARGEM GRID_MARGEM

// O dia é RÓTULO à direita da hora, não linha de seção: aparece e some sem
// mover a lista.
void tela_anotacoes(bitmap_t *bm, const vista_anotacoes_t *v)
{
    gfx_limpa(bm, false);

    barra_t b = { v->titulo, v->hora, v->bateria, v->wifi, false,
                  v->sinc };
    int y = chrome_barra(bm, &b) + 8;

    if (!v->n) {
        gfx_paragrafo(bm, MARGEM, y + 6, bm->l - MARGEM * 2, 6, F_CORPO,
                      v->vazio);
        chrome_rodape(bm, v->rodape_esq, v->rodape_dir, false);
        return;
    }

    for (int i = 0; i < v->n; i++) {
        const linha_captura_t *l = &v->linhas[i];

        if (v->dia[i][0]) {
            if (i) y += 6;
            gfx_texto(bm, MARGEM, y, F_MIUDA, v->dia[i]);
            y += gfx_altura_linha(F_MIUDA) + 3;
            y = chrome_filete(bm, y, FILETE_FINO) + 5;
        }

        // A altura do que a linha TEM: subtítulo reservado sem existir deixava a
        // moldura do cursor solta.
        int alt = gfx_altura_linha(F_CORPO);
        if (l->sub[0]) alt += gfx_altura_linha(F_MIUDA);

        // Corta entre linhas, nunca no meio de uma.
        if (y + alt > bm->a - RODAPE_A - 6) break;

        if (i == v->cursor) chrome_cursor(bm, y, alt);

        // A hora em miúda: a coluna que o olho usa. Sem hora, sem coluna.
        int col = MARGEM;
        if (l->hora[0]) {
            gfx_texto(bm, MARGEM, y + 2, F_MIUDA, l->hora);
            col = MARGEM + gfx_largura(F_MIUDA, "00:00") + 8;
        }

        char t[sizeof l->titulo + 4];
        snprintf(t, sizeof t, "%s", l->titulo[0] ? l->titulo : "sem título");
        int util = bm->l - MARGEM - col;
        int nc = gfx_cabe(F_CORPO, t, util);
        if (nc < (int)strlen(t)) {
            if (nc > 1) nc -= 1;
            t[nc] = '\0';
            snprintf(t + nc, sizeof t - (size_t)nc, "…");
        }
        gfx_texto(bm, col, y, F_CORPO, t);
        y += gfx_altura_linha(F_CORPO);

        if (l->sub[0]) {
            gfx_texto(bm, col, y, F_MIUDA, l->sub);
            y += gfx_altura_linha(F_MIUDA);
        }
        y += 5;
    }

    // "2 / 3" no rodapé, só quando pagina.
    if (v->paginas > 1) chrome_rodape_pagina(v->pagina, v->paginas);
    chrome_rodape(bm, v->rodape_esq, v->rodape_dir, false);
}
