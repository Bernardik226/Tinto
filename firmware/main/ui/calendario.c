#include "calendario.h"
#include "agua.h"
#include "chrome.h"
#include "grid.h"
#include "../tela/icones.h"
#include <stdio.h>
#include <string.h>

#define MARGEM GRID_MARGEM_GRADE
#define COL    33      // 7 colunas em 240 px, com a margem
#define LIN    38      // número + marcadores, sem os dois se tocarem

// A célula tem duas faixas: o número em cima, as marcas embaixo. O
// negativo de "hoje" cobre só o número, senão engole as marcas.
#define FAIXA_NUM   20      // a altura de linha do corpo
#define FAIXA_MARCA 22      // onde a de baixo começa

// TAREFA: quadrado cheio, canto de baixo à esquerda. Cantos e FORMAS
// diferentes: em 1 bit a forma é a única que distingue.
static void marca_tarefa(bitmap_t *bm, int x, int y)
{
    gfx_icone(bm, x + 3, y + FAIXA_MARCA + 2, ICO_MARCA_TAREFA);
}

// EVENTO: bolinha cheia, canto de baixo à direita.
static void marca_evento(bitmap_t *bm, int x, int y)
{
    gfx_icone(bm, x + COL - 11, y + FAIXA_MARCA + 1, ICO_MARCA_EVENTO);
}

void tela_calendario(bitmap_t *bm, const vista_cal_t *v)
{
    memset(bm->bits, 0, (size_t)((bm->l + 7) / 8) * (size_t)bm->a);

    barra_t b = { v->titulo, v->hora, v->bateria, v->wifi, false, v->sinc };
    int y = chrome_barra(bm, &b);

    // ── o cabeçalho do mês ───────────────────────────────────────────────
    // Em serifa, como a data da Agenda: é o título do que se lê. 40 px (uma
    // linha só): a grade precisa do resto.
    gfx_texto(bm, MARGEM, y + 7, F_EDITORIAL, v->mes);

    if (v->nav_mes[0])
        gfx_texto(bm, bm->l - MARGEM - gfx_largura(F_MIUDA, v->nav_mes),
                  y + 12, F_MIUDA, v->nav_mes);

    y += 40;
    gfx_hlin(bm, MARGEM, y - 1, bm->l - MARGEM * 2, 1);
    y += 6;

    static const char *DW[] = { "D", "S", "T", "Q", "Q", "S", "S" };
    for (int c = 0; c < 7; c++) {
        int w = gfx_largura(F_MIUDA, DW[c]);
        gfx_texto(bm, MARGEM + c * COL + (COL - w) / 2, y, F_MIUDA, DW[c]);
    }
    y += gfx_altura_linha(F_MIUDA) + 4;
    y = chrome_filete(bm, y, FILETE_FINO) + 3;

    int col = v->primeiro_dw;
    for (int dia = 1; dia <= v->n_dias; dia++) {
        int x = MARGEM + col * COL;

        char n[4];
        snprintf(n, sizeof n, "%d", dia);
        int w = gfx_largura(F_CORPO, n);

        gfx_texto(bm, x + (COL - w) / 2, y + 2, F_CORPO, n);

        // Hoje em negativo: inverter DEPOIS de escrever (preencher antes deixaria
        // preto sobre preto).
        if (dia == v->hoje)
            gfx_negativo(bm, x + 1, y, COL - 2, FAIXA_NUM);

        // O cursor abraça a célula inteira, número e marcas.
        if (dia == v->cursor_dia)
            gfx_ret(bm, x, y - 2, COL - 1, LIN - 2, false);

        if (v->tem_tarefa[dia]) marca_tarefa(bm, x, y);
        if (v->tem_evento[dia])  marca_evento(bm, x, y);

        if (++col == 7) { col = 0; y += LIN; }
    }
    if (col) y += LIN;

    // ── a tira do dia sob o cursor ──────────────────────────────────────
    // A lista da tela do Dia, em miniatura.
    y = chrome_filete(bm, y + 2, FILETE_FINO) + 6;

    int fundo = bm->a - RODAPE_A - 8;

    if (v->vazio[0]) {
        ui_agua(bm, MARGEM, y, bm->l - MARGEM * 2, fundo - y, v->vazio);
        chrome_rodape(bm, v->rodape_esq, v->rodape_dir, false);
        return;
    }

    gfx_texto(bm, MARGEM, y, F_MIUDA, v->tira_rot);

    // A contagem em palavras à direita do rótulo, ensinando as marcas no
    // contexto. MEDE antes: se não cabe ao lado do rótulo, desce uma linha (a
    // tira mostra um item a menos, que segue alcançável pelo OK).
    if (v->contagem[0]) {
        const int util = bm->l - MARGEM * 2;
        int wr = gfx_largura(F_MIUDA, v->tira_rot);
        int wc = gfx_largura(F_MIUDA, v->contagem);

        if (wr + 8 + wc <= util) {
            gfx_texto(bm, bm->l - MARGEM - wc, y, F_MIUDA, v->contagem);
            y += gfx_altura_linha(F_MIUDA) + 4;
        } else {
            y += gfx_altura_linha(F_MIUDA) + 2;
            gfx_texto(bm, bm->l - MARGEM - wc, y, F_MIUDA, v->contagem);
            y += gfx_altura_linha(F_MIUDA) + 4;
        }
    } else {
        y += gfx_altura_linha(F_MIUDA) + 4;
    }

    for (int i = 0; i < v->n_tira && y + gfx_altura_linha(F_CORPO_P) <= fundo;
         i++) {
        char linha[44];
        snprintf(linha, sizeof linha, "%s", v->tira[i]);
        int n = gfx_cabe(F_CORPO_P, linha, bm->l - MARGEM * 2);
        if (n < (int)strlen(linha)) {
            // Recua um CARACTERE, não um byte: `gfx_cabe` devolve um corte seguro em
            // UTF-8, e `n -= 1` deixaria meio acento.
            if (n > 1) n -= 1;
            while (n > 0 && ((unsigned char)linha[n] & 0xC0) == 0x80) n--;
            linha[n] = '\0';
            snprintf(linha + n, sizeof linha - (size_t)n, "…");
        }
        gfx_texto(bm, MARGEM, y, F_CORPO_P, linha);
        y += gfx_altura_linha(F_CORPO_P) + 2;
    }

    // O que não coube, esmaecido: é a promessa do OK, não um item.
    if (v->tira_mais[0] && y + gfx_altura_linha(F_MIUDA) <= fundo) {
        gfx_texto(bm, MARGEM, y, F_MIUDA, v->tira_mais);
        gfx_esmaece(bm, MARGEM, y, gfx_largura(F_MIUDA, v->tira_mais),
                    gfx_altura_linha(F_MIUDA), 2);
    }

    // Dia vazio num mês com coisas: rótulo sozinho leria como lista que não
    // carregou.
    if (!v->n_tira && !v->tira_mais[0]) {
        int w = gfx_largura(F_CORPO_P, "nada neste dia");
        gfx_texto(bm, (bm->l - w) / 2, y + 2, F_CORPO_P, "nada neste dia");
        gfx_esmaece(bm, (bm->l - w) / 2, y + 2, w,
                    gfx_altura_linha(F_CORPO_P), 2);
    }

    chrome_rodape(bm, v->rodape_esq, v->rodape_dir, false);
}
