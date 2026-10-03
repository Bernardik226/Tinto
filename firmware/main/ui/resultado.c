#include "resultado.h"
#include "chrome.h"
#include "grid.h"
#include "rolagem.h"
#include "../tela/icones.h"

#define MARGEM GRID_MARGEM

// A marca redonda do desenho vira QUADRADO com visto: em 1 bit um círculo
// de 44 px é escada.
#define MARCA 34

// O corpo, a partir de `y`. A MESMA função mede e pinta.
static int corpo(bitmap_t *bm, const vista_resultado_t *v, int y)
{
    const int x = MARGEM + 1, larg = bm->l - (MARGEM + 1) * 2;

    // ── a marca só é cheia quando TUDO deu certo ────────────────────────
    // Sucesso parcial não ganha marca geral de sucesso.
    int mx = (bm->l - MARCA) / 2;
    if (v->tudo_certo) {
        // O glifo PRIMEIRO, e a inversão depois: preto sobre preto sumia.
        gfx_icone(bm, mx + (MARCA - 15) / 2, y + (MARCA - 15) / 2, ICO_VISTO);
        gfx_negativo(bm, mx, y, MARCA, MARCA);
    } else {
        gfx_ret(bm, mx, y, MARCA, MARCA, false);
        gfx_texto(bm, mx + (MARCA - gfx_largura(F_TITULO, "!")) / 2,
                  y + (MARCA - gfx_altura_linha(F_TITULO)) / 2, F_TITULO, "!");
    }
    y += MARCA + 10;

    gfx_texto(bm, (bm->l - gfx_largura(F_EDITORIAL, v->frase)) / 2, y,
              F_EDITORIAL, v->frase);
    y += gfx_altura_linha(F_EDITORIAL) + 5;

    y = gfx_paragrafo(bm, x + 6, y, larg - 12, 2, F_MIUDA, v->sub) + 10;

    // ── uma linha por ação, com o estado DELA ─────────────────────────
    for (int i = 0; i < v->n; i++) {
        gfx_hlin(bm, x, y, larg, 1);
        y += 7;

        // Cheia quando deu certo, vazada quando não.
        if (v->linhas[i].ok) gfx_icone(bm, x - 1, y, ICO_VISTO);
        else                 gfx_ret(bm, x + 2, y + 3, 8, 8, false);

        // QUEBRA, não corta: é a última tela do fluxo. O estado fica na PRIMEIRA
        // linha, à direita.
        int we = gfx_largura(F_MIUDA, v->linhas[i].estado);
        gfx_texto(bm, x + larg - we, y + 2, F_MIUDA, v->linhas[i].estado);

        y = gfx_paragrafo(bm, x + 16, y, larg - 16 - we - 6, 3, F_CORPO_P,
                          v->linhas[i].oque);
        y = gfx_paragrafo(bm, x + 16, y, larg - 16, 2, F_MIUDA,
                          v->linhas[i].destino) + 5;
    }

    return y;
}

static int altura_do_corpo(const bitmap_t *bm, const vista_resultado_t *v)
{
    bitmap_t *medida = rolagem_rascunho(bm);
    if (!medida) return 0;
    return corpo(medida, v, 0);
}

int tela_resultado_paradas(bitmap_t *bm, const vista_resultado_t *v)
{
    int area = bm->a - RODAPE_A - 8 - (BARRA_A + 14);
    return rolagem_paradas(altura_do_corpo(bm, v), area);
}

// Só leitura: conferir e sair. Pode passar da tela, e rola.
void tela_resultado(bitmap_t *bm, const vista_resultado_t *v)
{
    gfx_limpa(bm, false);

    barra_t b = { v->titulo, v->hora, v->bateria, v->wifi, false, v->sinc };
    chrome_barra(bm, &b);

    const int topo  = BARRA_A + 14;
    const int fundo = bm->a - RODAPE_A - 8;
    const int area  = fundo - topo;
    int alto   = altura_do_corpo(bm, v);
    int desloc = rolagem_desloc(v->cursor, alto, area);

    corpo(bm, v, topo - desloc);

    gfx_limpa_ret(bm, 0, 0, bm->l, BARRA_A + 4);
    chrome_barra(bm, &b);
    gfx_limpa_ret(bm, 0, fundo + 2, bm->l, bm->a - RODAPE_A - fundo - 2);
    rolagem_setas(bm, desloc, alto, area, topo, fundo);

    chrome_rodape(bm, v->rodape_esq, v->rodape_dir, false);
}
