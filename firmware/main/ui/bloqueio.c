#include "bloqueio.h"
#include "chrome.h"
#include "grid.h"
#include "../tela/icones.h"
#include <stdio.h>
#include <string.h>

#define MARGEM GRID_MARGEM_TEXTO

// Sem barra e sem HUD: nada aqui se aperta. O rodapé tem uma frase só
// ("power para desbloquear"). Sem ícones nas linhas: a categoria a pessoa
// já sabe pelo formato.
void tela_bloqueio(bitmap_t *bm, const vista_bloqueio_t *v)
{
    gfx_limpa(bm, false);

    int y = 34;
    gfx_texto(bm, MARGEM, y, F_ENORME, v->hora);
    y += gfx_altura_linha(F_ENORME) - 8;
    // Até duas linhas: cortada na borda, o mês some.
    y = gfx_paragrafo(bm, MARGEM, y, bm->l - MARGEM * 2, 2, F_CORPO, v->data)
      + 26;

    gfx_hlin(bm, MARGEM, y, bm->l - MARGEM * 2, 2);
    y += 14;

    // O kicker diz o QUE e QUANDO numa linha ("próximo · em 2h43").
    if (v->falta[0]) {
        // "próximo · em 4h46": "compromisso" não cabe com o tempo.
        char kick[40];
        snprintf(kick, sizeof kick, "próximo · %s", v->falta);
        gfx_texto_ate(bm, MARGEM, y, F_MIUDA, kick, bm->l - MARGEM * 2);
        y += gfx_altura_linha(F_MIUDA) + 3;
    }

    gfx_texto_ate(bm, MARGEM, y, F_CORPO, v->proximo, bm->l - MARGEM * 2);
    y += gfx_altura_linha(F_CORPO);

    // ONDE: o que faz sair de casa na hora certa.
    if (v->onde[0]) {
        gfx_texto_ate(bm, MARGEM, y, F_MIUDA, v->onde, bm->l - MARGEM * 2);
        y += gfx_altura_linha(F_MIUDA);
    }
    y += 16;

    // O resto de hoje, pelo nome.
    for (int i = 0; i < v->n_resto; i++) {
        gfx_texto_ate(bm, MARGEM, y, F_CORPO, v->resto[i], bm->l - MARGEM * 2);
        y += gfx_altura_linha(F_CORPO) + 2;
    }
    if (v->mais[0]) {
        gfx_texto(bm, MARGEM, y, F_MIUDA, v->mais);
    }

    // O aviso responde sem fingir tela navegável: faixa curta, em alto
    // contraste, que aparece e some.
    int by = bm->a - 30;
    if (v->aviso[0]) {
        int ah = gfx_altura_linha(F_MIUDA) + 12;
        int ay = by - ah - 12;
        gfx_texto(bm, MARGEM + 8, ay + 6, F_MIUDA, v->aviso);
        gfx_negativo(bm, MARGEM, ay, bm->l - MARGEM * 2, ah);
    }

    // A bateria no rodapé: é o que importa de um aparelho dormindo.
    gfx_ret(bm, MARGEM, by, 19, 11, false);
    gfx_ret(bm, MARGEM + 19, by + 3, 2, 5, true);
    int cheio = v->bateria * 15 / 100;
    if (cheio > 0) gfx_ret(bm, MARGEM + 2, by + 2, cheio, 7, true);

    // A única saída, em linha PRÓPRIA acima da bateria (ao lado, encavalavam).
    if (v->saida[0]) {
        int w = gfx_largura(F_MIUDA, v->saida);
        gfx_texto(bm, (bm->l - w) / 2, by - gfx_altura_linha(F_MIUDA) - 8,
                  F_MIUDA, v->saida);
    }

    char pct[8];
    snprintf(pct, sizeof pct, "%d%%", v->bateria);
    gfx_texto(bm, MARGEM + 28, by - 1, F_MIUDA, pct);
    if (v->carregando)
        gfx_texto(bm, MARGEM + 28 + gfx_largura(F_MIUDA, pct) + 8, by - 1,
                  F_MIUDA, "na dock");
}
