#include "confirma.h"
#include "chrome.h"
#include "grid.h"
#include "../tela/texto.h"
#include <string.h>

#define MARGEM GRID_MARGEM

// A pergunta destrutiva: rótulo miúdo, a pergunta em serifa, o que
// acontece e o que NÃO acontece, e as duas saídas no rodapé, "não" primeiro.
// (`gfx_texto` devolve o X do fim, não o Y.)
void tela_confirma(bitmap_t *bm, const vista_confirma_t *v)
{
    gfx_limpa(bm, false);

    barra_t b = { v->titulo, v->hora, v->bateria, v->wifi, false, v->sinc };
    int y = chrome_barra(bm, &b) + 12;

    const int larg = bm->l - MARGEM * 2;

    gfx_texto(bm, MARGEM, y, F_MIUDA, "confirmação necessária");
    y += gfx_altura_linha(F_MIUDA) + 4;

    // A pergunta QUEBRA: cortada, deixa de ser pergunta.
    y = gfx_paragrafo(bm, MARGEM, y, larg, 3, F_EDITORIAL, v->pergunta) + 8;

    y = chrome_filete(bm, y, FILETE_FINO) + 8;

    // ── as duas saídas, ancoradas no rodapé ─────────────────────────────
    // Decisão que muda de lugar com o texto é decisão que se procura.
    const int alt  = gfx_altura_linha(F_CORPO) + 12;
    const int base = bm->a - RODAPE_A - 10 - alt * 2 - 6;

    // A explicação ocupa o que sobra entre o filete e as saídas.
    int cabem = (base - 10 - y) / gfx_altura_linha(F_MIUDA);
    if (cabem > 8) cabem = 8;
    if (cabem > 0) gfx_paragrafo(bm, MARGEM, y, larg, cabem, F_MIUDA,
                                 v->explica);

    // O "não" PRIMEIRO: o que o cursor encontra é o que se aperta sem querer.
    const char *op[2] = { v->nao, v->sim };
    for (int i = 0; i < 2; i++) {
        int oy = base + (alt + 6) * i;
        gfx_ret(bm, MARGEM, oy, larg, alt, false);
        gfx_texto_ate(bm, MARGEM + 10, oy + 6, F_CORPO, op[i], larg - 20);
        if (i == v->cursor) gfx_negativo(bm, MARGEM, oy, larg, alt);
    }

    chrome_rodape(bm, v->rodape_esq, v->rodape_dir, false);
}
