#include "gravador.h"
#include "chrome.h"
#include "grid.h"
#include "../tela/icones.h"
#include <string.h>

#define MARGEM GRID_MARGEM

// A onda anda com o quadro e não mede nada: responde "está me ouvindo?". A
// barra de nível do microfone era instável e mentia sobre a captação.
static void medidor(bitmap_t *bm, int y, int quadro, bool pausado)
{
    static const int ALTURAS[8] = { 3, 7, 12, 16, 12, 7, 4, 9 };
    const int L = 190, A = 12, LARG = 3, VAO = 3;
    int x = (bm->l - L) / 2;

    gfx_ret(bm, x, y, L, A, false);
    // Pausado fica VAZADO: mesma silhueta, sem mudar a forma da tela.
    if (pausado) return;

    int n = (L - 8) / (LARG + VAO);
    for (int i = 0; i < n; i++) {
        int a = ALTURAS[(i + quadro) & 7];
        if (a > A - 4) a = A - 4;
        gfx_ret(bm, x + 4 + i * (LARG + VAO), y + (A - a) / 2,
                LARG, a, true);
    }
}


// ── o pop-over: gravando fora da home ───────────────────────────────
// A tela de trás continua visível: falar não tira ninguém do lugar. A
// geometria num lugar só: quem apaga e quem desenha leem o mesmo
// retângulo.
static void area(bitmap_t *bm, const vista_grav_t *v,
                 int *x, int *y, int *l, int *a)
{
    const int M = 12;
    int alto = v->confirmando
             ? 30 + gfx_altura_linha(F_TITULO) * 2 + gfx_altura_linha(F_CORPO) * 2 + 30
             : 34 + gfx_altura_linha(F_ENORME) + gfx_altura_linha(F_MIUDA) + 18;
    *x = M;
    *y = (bm->a - alto) / 2;
    *l = bm->l - M * 2 + 4;
    *a = alto + 4;
}

void tela_gravador_popover_area(bitmap_t *bm, const vista_grav_t *v,
                                int *x, int *y, int *l, int *a)
{
    area(bm, v, x, y, l, a);
}

void tela_gravador_popover(bitmap_t *bm, const vista_grav_t *v)
{
    const int M = 12;
    int alto = v->confirmando
             ? 30 + gfx_altura_linha(F_TITULO) * 2 + gfx_altura_linha(F_CORPO) * 2 + 30
             : 34 + gfx_altura_linha(F_ENORME) + gfx_altura_linha(F_MIUDA) + 18;
    int topo = (bm->a - alto) / 2;
    int larg = bm->l - M * 2;

    gfx_ret(bm, M + 4, topo + 4, larg, alto, true);      // a sombra
    gfx_limpa_ret(bm, M, topo, larg, alto);
    gfx_ret(bm, M, topo, larg, alto, false);

    int y = topo + 10;
    if (v->confirmando) {
        y = gfx_paragrafo(bm, M + 10, y, larg - 20, 2, F_TITULO, v->pergunta);
        y += 4;
        gfx_texto(bm, M + 10, y, F_MIUDA, v->perde);
        y += gfx_altura_linha(F_MIUDA) + 14;

        const char *op[2] = { "Não, continuar", "Sim, descartar" };
        for (int i = 0; i < 2; i++) {
            int alt = gfx_altura_linha(F_CORPO) + 6;
            if ((i == 1) == v->sim_selecionado)
                gfx_ret(bm, M + 6, y - 2, larg - 12, alt, false);
            gfx_texto(bm, M + 14, y, F_CORPO, op[i]);
            y += alt + 4;
        }
        return;
    }

    gfx_icone(bm, M + 10, y + 4, ICO_MIC);
    gfx_texto(bm, M + 10 + ICONES[ICO_MIC].l + 8, y + 2, F_MIUDA, v->estado);
    y += gfx_altura_linha(F_MIUDA) + 8;

    gfx_texto(bm, M + 10, y, F_ENORME, v->tempo);
    y += gfx_altura_linha(F_ENORME) - 4;

    if (v->trecho_txt[0]) {
        int w = gfx_largura(F_MIUDA, v->trecho_txt);
        gfx_texto(bm, M + larg - 10 - w, y - gfx_altura_linha(F_MIUDA) - 4,
                  F_MIUDA, v->trecho_txt);
    }

    medidor(bm, y + 2, v->onda, v->pausado);
}
