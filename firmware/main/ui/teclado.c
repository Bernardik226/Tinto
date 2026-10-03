#include "teclado.h"
#include "chrome.h"
#include "grid.h"
#include <stdio.h>
#include <string.h>

#define MARGEM GRID_MARGEM_TECLADO

void tela_teclado(bitmap_t *bm, const vista_teclado_t *v)
{
    gfx_limpa(bm, false);

    barra_t b = { v->titulo, v->hora, v->bateria, v->wifi, false, v->sinc };
    int y = chrome_barra(bm, &b) + 9;

    // O kicker: de qual rede é a senha, com a linha inteira.
    if (v->kicker[0]) {
        gfx_texto_ate(bm, MARGEM, y, F_MIUDA, v->kicker, bm->l - MARGEM * 2);
        y += gfx_altura_linha(F_MIUDA) + 5;
    }

    // O digitado numa caixa: sem ela não se sabe onde o texto acaba.
    int alt = gfx_altura_linha(F_CORPO) + 10;
    gfx_ret(bm, MARGEM, y, bm->l - MARGEM * 2, alt, false);
    gfx_texto(bm, MARGEM + 6, y + 5, F_CORPO, v->texto);

    // À direita: quantos caracteres já foram (senha mascarada) ou quantos ainda
    // cabem (nome à vista).
    char n[20];
    if (v->contagem[0]) snprintf(n, sizeof n, "%s", v->contagem);
    else                snprintf(n, sizeof n, "%d", v->restam);
    gfx_texto(bm, bm->l - MARGEM - 6 - gfx_largura(F_MIUDA, n), y + 7,
              F_MIUDA, n);
    y += alt + 14;

    // Célula quadrada e igual em todo modo: o cursor não pula.
    int cl = (bm->l - MARGEM * 2) / TEC_COLS;
    int ca = 30;
    for (int l = 0; l < TEC_LINS; l++) {
        for (int c = 0; c < TEC_COLS; c++) {
            if (!v->teclas[l][c][0]) continue;
            int x = MARGEM + c * cl;
            int yy = y + l * ca;

            // Escrever primeiro, inverter DEPOIS: preencher antes deixa preto sobre
            // preto e a tecla some.
            fonte_t f = strlen(v->teclas[l][c]) > 1 ? F_MIUDA : F_TITULO;
            int w = gfx_largura(f, v->teclas[l][c]);
            gfx_texto(bm, x + (cl - w) / 2, yy + (ca - gfx_altura_linha(f)) / 2,
                      f, v->teclas[l][c]);

            if (l == v->cur_lin && c == v->cur_col)
                gfx_negativo(bm, x + 1, yy, cl - 2, ca - 3);
        }
    }
    y += TEC_LINS * ca + 6;

    if (v->nota[0]) {
        gfx_hlin(bm, MARGEM, y, bm->l - MARGEM * 2, 1);
        gfx_paragrafo(bm, MARGEM, y + 6, bm->l - MARGEM * 2, 2, F_MIUDA,
                      v->nota);
    }

    chrome_rodape(bm, v->rodape_esq, v->rodape_dir, false);
}
