#include "dia.h"
#include "agua.h"
#include "chrome.h"
#include "grid.h"
#include "blocos.h"
#include "rolagem.h"
#include <stdio.h>
#include <string.h>

#define MARGEM GRID_MARGEM

// Rótulo de seção: miúdo à esquerda, contagem ou tempo à direita.
static int secao(bitmap_t *bm, int y, const char *rot, const char *dir)
{
    gfx_texto(bm, MARGEM, y, F_MIUDA, rot);
    if (dir && *dir) {
        int w = gfx_largura(F_MIUDA, dir);
        gfx_texto(bm, bm->l - MARGEM - w, y, F_MIUDA, dir);
    }
    return y + gfx_altura_linha(F_MIUDA) + 5;
}

// ── as páginas do dia ────────────────────────────────────────────────
// Quebra por LINHA: cada página começa no topo e nada reaparece deslocado. O
// mesmo laço mede e desenha.
static int desenha(bitmap_t *bm, const vista_dia_t *v, int de, int ate,
                   bool pintando)
{
    int y = BARRA_A + 8;
    int linha = 0;

    // O rótulo do dia só na PRIMEIRA página.
    if (v->rotulo[0] && de == 0) y = secao(bm, y, v->rotulo, v->quando);

    if (v->n_compromissos) {
        bool algum = false;
        for (int i = 0; i < v->n_compromissos; i++)
            if (linha + i >= de && linha + i < ate) algum = true;

        if (algum) {
            char n[8];
            snprintf(n, sizeof n, "%d", v->n_compromissos);
            y = secao(bm, y, "COMPROMISSOS", n);
        }
        // A régua nasce no primeiro compromisso DESTA página e morre no último.
        int prim = -1, ult = -1, col_h = 0;
        for (int i = 0; i < v->n_compromissos; i++) {
            int ln = linha + i;
            if (ln < de || ln >= ate) continue;
            if (prim < 0) prim = i;
            ult = i;

            int w = gfx_largura(F_MIUDA, v->compromissos[i].hora);
            if (w > col_h) col_h = w;
        }

        for (int i = 0; i < v->n_compromissos; i++, linha++) {
            if (linha < de || linha >= ate) continue;
            y = bloco_timeline(bm, y, &v->compromissos[i],
                               linha == v->cursor, i == prim, i == ult,
                               col_h);
        }

        // O que não coube no teto, dito: mentir a conta seria pior.
        if (v->mais_compromissos && ate >= v->n_compromissos) {
            char aviso[40];
            snprintf(aviso, sizeof aviso, "+ %d não couberam",
                     v->mais_compromissos);
            gfx_texto(bm, MARGEM, y, F_MIUDA, aviso);
            y += gfx_altura_linha(F_MIUDA) + 4;
        }
    }

    if (v->n_tarefas) {
        bool algum = false;
        for (int i = 0; i < v->n_tarefas; i++)
            if (linha + i >= de && linha + i < ate) algum = true;

        if (algum) {
            y += 6;
            y = chrome_filete(bm, y, FILETE_FINO) + 6;
            char n[8];
            snprintf(n, sizeof n, "%d", v->n_tarefas);
            y = secao(bm, y, "TAREFAS", n);
        }

        int alt = gfx_altura_linha(F_CORPO_P);
        int grupo_atras = -1;

        for (int i = 0; i < v->n_tarefas; i++, linha++) {
            const linha_trabalho_t *t = &v->tarefas[i];
            if (linha < de || linha >= ate) continue;

            // O cabeçalho do grupo onde ele começa, e de novo no topo da página
            // seguinte se atravessar.
            if (t->grupo != grupo_atras && t->grupo >= 0 &&
                v->grupos[t->grupo].titulo[0]) {
                grupo_atras = t->grupo;
                y = secao(bm, y, v->grupos[t->grupo].titulo,
                          v->grupos[t->grupo].contagem);
            }

            if (!pintando) { y += alt + 3; continue; }

            int x = chrome_caixa(bm, MARGEM, y + 2, t->feita);
            int fim = gfx_texto_ate(bm, x, y, F_CORPO_P, t->titulo,
                                    bm->l - MARGEM - x);
            if (t->feita) chrome_risco(bm, x, y, fim - x, F_CORPO_P);

            // A tarefa é parada de cursor porque ABRE.
            if (linha == v->cursor)
                gfx_negativo(bm, MARGEM - 4, y - 2,
                             bm->l - (MARGEM - 4) * 2, alt + 4);
            y += alt + 3;
        }

        // O que não coube no teto, como nos compromissos.
        if (v->mais_tarefas && ate >= v->n_compromissos + v->n_tarefas) {
            char aviso[40];
            snprintf(aviso, sizeof aviso, "+ %d não couberam", v->mais_tarefas);
            gfx_texto(bm, MARGEM, y, F_MIUDA, aviso);
            y += gfx_altura_linha(F_MIUDA) + 4;
        }
    }

    return y;
}

// Onde cada página começa: devolve quantas são e enche `inicio`.
static int fronteiras(bitmap_t *bm, const vista_dia_t *v, int *inicio, int max)
{
    bitmap_t *medida = rolagem_rascunho(bm);
    if (!medida) { inicio[0] = 0; return 1; }

    const int total = v->n_compromissos + v->n_tarefas;
    const int fundo = bm->a - RODAPE_A - 8;

    int n = 0;
    inicio[n++] = 0;

    int de = 0;
    for (int ate = 1; ate <= total && n < max; ate++) {
        if (desenha(medida, v, de, ate, false) <= fundo) continue;

        // Estourou: a página anterior vai até a de trás e a nova começa NELA. O
        // `ate > de + 1` impede travar numa linha maior que a página.
        if (ate > de + 1) {
            de = ate - 1;
            inicio[n++] = de;
        }
    }
    return n;
}

#define DIA_PAGINAS_MAX 16

int tela_dia_paginas(bitmap_t *bm, const vista_dia_t *v)
{
    int inicio[DIA_PAGINAS_MAX];
    return fronteiras(bm, v, inicio, DIA_PAGINAS_MAX);
}

int tela_dia_pagina(bitmap_t *bm, const vista_dia_t *v)
{
    int inicio[DIA_PAGINAS_MAX];
    int n = fronteiras(bm, v, inicio, DIA_PAGINAS_MAX);
    int c = v->cursor < 0 ? 0 : v->cursor;

    int qual = 1;
    for (int i = 0; i < n; i++) if (c >= inicio[i]) qual = i + 1;
    return qual;
}

void tela_dia(bitmap_t *bm, const vista_dia_t *v)
{
    memset(bm->bits, 0, (size_t)((bm->l + 7) / 8) * (size_t)bm->a);

    barra_t b = { v->titulo, v->hora, v->bateria, v->wifi, false, v->sinc };
    chrome_barra(bm, &b);

    int inicio[DIA_PAGINAS_MAX];
    int n = fronteiras(bm, v, inicio, DIA_PAGINAS_MAX);
    int pag = tela_dia_pagina(bm, v);
    int de  = inicio[pag - 1];
    int ate = pag < n ? inicio[pag] : v->n_compromissos + v->n_tarefas;

    int y = desenha(bm, v, de, ate, true);

    // ── o dia vazio ──────────────────────────────────────────────────────
    // A marca d'água, centrada no que sobrou.
    if (v->vazio[0]) {
        int fundo = bm->a - RODAPE_A - 10;
        if (fundo - y > gfx_altura_linha(F_CORPO))
            ui_agua(bm, MARGEM, y + 8, bm->l - MARGEM * 2, fundo - y - 8,
                    v->vazio);
    }

    chrome_rodape_pagina(pag, n);
    chrome_rodape(bm, v->rodape_esq, v->rodape_dir, false);
}
