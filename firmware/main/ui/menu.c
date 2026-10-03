#include "menu.h"
#include "agua.h"
#include "chrome.h"
#include "grid.h"
#include "pagina.h"
#include "faixa.h"
#include "blocos.h"
#include "../tela/icones.h"
#include <string.h>

#define MARGEM GRID_MARGEM

// A gaveta é uma FAIXA ancorada no rodapé, não caixa flutuante: peça que
// flutua disputa pixel com o de trás. Assim o cartaz nunca é coberto, o
// retângulo é sempre o mesmo (parcial barato) e nada vaza. A altura segue o
// número de destinos.
static int altura(const vista_menu_t *v)
{
    return FAIXA_TOPO + 7
         + gfx_altura_linha(F_MIUDA) + 6              // o rótulo
         + v->n * (gfx_altura_linha(F_CORPO) + 7)     // os destinos
         + 4;
}

static void area(bitmap_t *bm, const vista_menu_t *v,
                 int *x, int *y, int *l, int *a)
{
    (void)bm;
    ret_t r = grid_faixa_de(altura(v));
    *x = r.x; *y = r.y; *l = r.l; *a = r.a;
}

void tela_menu_popover_area(bitmap_t *bm, const vista_menu_t *v,
                            int *x, int *y, int *l, int *a)
{
    area(bm, v, x, y, l, a);
}

void tela_menu_popover(bitmap_t *bm, const vista_menu_t *v)
{
    faixa_t f = faixa_abre(bm, altura(v), false);

    int y = f.y;
    gfx_texto(bm, f.x, y, F_MIUDA, v->titulo);
    y += gfx_altura_linha(F_MIUDA) + 6;

    // `bloco_acao_em` e não desenho próprio: ele já trunca o texto contra o
    // valor.
    for (int i = 0; i < v->n; i++)
        y = bloco_acao_em(bm, f.x, y, f.util, &v->linhas[i], i == v->cursor);

    faixa_fecha(bm, &f);

    // O rodapé diz o que o OK faz AGORA, na gaveta; e continua sendo do CHROME
    // (o teste de moldura idêntica defende isso).
    chrome_rodape_pagina(v->pagina, v->paginas);
    chrome_rodape(bm, v->rodape_esq, v->rodape_dir, false);
}

void tela_menu(bitmap_t *bm, const vista_menu_t *v)
{
    memset(bm->bits, 0, (size_t)((bm->l + 7) / 8) * (size_t)bm->a);

    barra_t b = { v->titulo, v->hora, v->bateria, v->wifi, false, v->sinc };
    int y = chrome_barra(bm, &b) + 8;

    // ── a manchete, quando a tela é um ERRO com saída ───────────────────
    // Com linhas, `vazio` é o TÍTULO do que aconteceu e a linha embaixo é a
    // única coisa a fazer.
    if (v->vazio[0] && (v->n || v->pontos >= 0)) {
        int largura = bm->l - MARGEM * 2;
        if (gfx_largura(F_EDITORIAL, v->vazio) <= largura) {
            int fim = gfx_texto(bm, MARGEM, y, F_EDITORIAL, v->vazio);
            chrome_pontinhos(bm, fim + 8, y + gfx_altura_linha(F_EDITORIAL) - 8,
                             v->pontos);
            y += gfx_altura_linha(F_EDITORIAL) + 10;
        } else {
            // Numa linha só, a manchete longa saía cortada no vidro.
            y = gfx_paragrafo(bm, MARGEM, y, largura, 2, F_EDITORIAL, v->vazio)
              + 10;
        }
    }

    // Até o fim da PÁGINA que a vista definiu (`ultima`, exclusivo; zero = a
    // lista inteira).
    int fim = v->ultima > 0 ? v->ultima : v->n;
    for (int i = v->primeira; i < fim; i++) {
        // A seção é DIVISÓRIA: miúda, filete embaixo e ar em volta.
        if (v->secao[i][0]) {
            if (i) y += 8;
            gfx_texto(bm, MARGEM, y, F_MIUDA, v->secao[i]);

            // Os pontinhos da varredura à DIREITA do rótulo: não empurram nada.
            if (v->pontos > 0)
                chrome_pontinhos(bm, bm->l - MARGEM - (v->pontos * 8 - 3),
                                 y + 2, v->pontos);

            y += gfx_altura_linha(F_MIUDA) + 3;
            y = chrome_filete(bm, y, FILETE_FINO) + 6;
        }
        // Corta entre linhas, nunca meia linha.
        if (y + gfx_altura_linha(F_CORPO) + 7 > bm->a - RODAPE_A - 6) break;

        // O foco depois, cobrindo título e legenda: o destino inteiro está
        // selecionado.
        int topo_da_linha = y;
        y = bloco_acao(bm, y, &v->linhas[i], false);

        // A descrição do destino, fora da moldura do foco: invertida junto,
        // engordaria o bloco preto.
        if (v->sub[i][0]) {
            gfx_texto_ate(bm, MARGEM + 24, y - 2, F_MIUDA, v->sub[i],
                          bm->l - MARGEM * 2 - 24);
            y += gfx_altura_linha(F_MIUDA) + 4;
        }

        if (i == v->cursor)
            gfx_negativo(bm, MARGEM - 4, topo_da_linha - 2,
                         bm->l - (MARGEM - 4) * 2, y - topo_da_linha);
    }

    // O vazio centrado no que sobrou, antes da nota. Só quando a manchete não
    // o desenhou.
    if (v->vazio[0] && v->n == 0 && v->pontos < 0) {
        int fundo = bm->a - RODAPE_A - gfx_altura_linha(F_MIUDA) * 2 - 20;
        if (fundo - y > gfx_altura_linha(F_CORPO))
            ui_agua(bm, MARGEM, y + 8, bm->l - MARGEM * 2, fundo - y - 8,
                    v->vazio);
    }

    // TRÊS linhas: a nota diz o que fazer e para quê.
    if (v->nota[0]) {
        int ny = bm->a - RODAPE_A - gfx_altura_linha(F_MIUDA) * 3 - 10;
        if (ny > y + 6) {
            gfx_hlin(bm, MARGEM, ny - 6, bm->l - MARGEM * 2, 1);
            gfx_paragrafo(bm, MARGEM, ny, bm->l - MARGEM * 2, 3, F_MIUDA, v->nota);
        }
    }

    chrome_rodape_pagina(v->pagina, v->paginas);
    chrome_rodape(bm, v->rodape_esq, v->rodape_dir, false);
}

// A faixa da manchete, para o parcial que anima os pontinhos (tela cheia
// por segundo seria fantasma).
void tela_menu_manchete_area(bitmap_t *bm, int *x, int *y, int *l, int *a)
{
    *x = 0;
    *y = BARRA_A + 4;
    *l = bm->l;
    *a = gfx_altura_linha(F_EDITORIAL) + 10;
}
