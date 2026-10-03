#include "leitor.h"
#include "chrome.h"
#include "grid.h"
#include "../tela/texto.h"
#include <string.h>

// Margem maior que a do sistema: linha longa de texto corrido cansa.
#define MARGEM_LEITURA 14

void tela_leitor(bitmap_t *bm, const vista_leitor_t *v)
{
    gfx_limpa(bm, false);

    // A capa é página inteira, sem HUD nem rodapé; o app aplica o bitmap
    // depois.
    if (v->capa) return;

    barra_t b = { v->titulo, v->hora, v->bateria, v->wifi, false, v->sinc };
    chrome_barra(bm, &b);

    const int larg = TELA_L - MARGEM_LEITURA * 2;
    int y = GRID_BARRA_A + 10;

    // ── a cópia danificada NÃO abre ─────────────────────────────────────
    // Dita em palavras, ocupando a tela.
    if (v->erro[0]) {
        gfx_paragrafo(bm, MARGEM_LEITURA, y + 60, larg, 3, F_EDITORIAL,
                      v->erro);
        chrome_rodape(bm, v->rodape_esq, "", false);
        return;
    }

    // ── o texto, em serifa, e SÓ o que cabe ─────────────────────────────
    // Contado pela geometria: com número fixo, a última linha saía cortada sob
    // o rodapé.
    const int cabem = (GRID_RODAPE_Y - 4 - y) / gfx_altura_linha(v->fonte);
    (void)gfx_pagina_alinhada(bm, MARGEM_LEITURA, y, larg,
                              cabem > 0 ? cabem : 1, v->fonte, v->texto,
                              (int)strlen(v->texto),
                              (texto_alinhamento_t)v->alinhamento);

    // ── o fim PERGUNTA ───────────────────────────────────────────────────
    // Uma faixa sobre a última página, com as duas saídas.
    if (v->no_fim) {
        const int alt = 96;
        const int topo = GRID_RODAPE_Y - alt - 6;

        gfx_limpa_ret(bm, 0, topo, TELA_L, alt);
        gfx_hlin(bm, 0, topo, TELA_L, 2);

        int ty = topo + 12;
        gfx_texto(bm, MARGEM_LEITURA, ty, F_EDITORIAL, v->pergunta);
        ty += gfx_altura_linha(F_EDITORIAL) + 10;

        gfx_ret(bm, MARGEM_LEITURA, ty, larg, 26, false);
        gfx_texto(bm, MARGEM_LEITURA + 10, ty + 6, F_MIUDA, v->sim);
        ty += 30;
        gfx_texto(bm, MARGEM_LEITURA + 10, ty + 4, F_MIUDA, v->nao);
    }

    // A régua à direita: "◀ 38% · 46/121 ▶".
    chrome_rodape(bm, v->rodape_esq, v->regua, false);
    if (v->barra_progresso) {
        const int x=82, yb=GRID_RODAPE_Y+6, l=55;
        gfx_ret(bm,x,yb,l,7,false);
        int cheio=(l-2)*(v->progresso_pct<0?0:(v->progresso_pct>100?100:v->progresso_pct))/100;
        if(cheio) gfx_ret(bm,x+1,yb+1,cheio,5,true);
    }
}
