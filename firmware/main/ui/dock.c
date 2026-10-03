#include "dock.h"
#include "chrome.h"
#include "grid.h"
#include "rolagem.h"
#include "../tela/texto.h"
#include <string.h>

#define MARGEM 16

// ── a dock é DEITADA, e o painel não gira ───────────────────────────
// Girar pelo controlador mudaria o framebuffer de toda tela. 416×240 e
// 240×416 têm os mesmos 12 480 bytes: a tela é desenhada num rascunho
// horizontal com as primitivas de sempre e copiada girada.
void tela_dock(bitmap_t *bm, const vista_dock_t *v)
{
    // O rascunho lido como HORIZONTAL: mesmos bytes, outro passo.
    bitmap_t *r = rolagem_rascunho(bm);
    if (!r) { gfx_limpa(bm, false); return; }

    bitmap_t h;
    bitmap_liga(&h, r->bits, bm->a, bm->l);
    gfx_limpa(&h, false);

    int y = 18;

    // ── a faixa de cima ──────────────────────────────────────────────────
    // A carga não se corta; a data cede.
    int wc = gfx_largura(F_MIUDA, v->carga);
    gfx_texto(&h, h.l - MARGEM - wc, y, F_MIUDA, v->carga);
    gfx_texto_ate(&h, MARGEM, y, F_MIUDA, v->faixa,
                  h.l - MARGEM * 2 - wc - 12);
    y += gfx_altura_linha(F_MIUDA) + 10;

    gfx_hlin(&h, MARGEM, y, h.l - MARGEM * 2, 1);
    y += 18;

    // ── a hora, grande, e a saudação ao lado ────────────────────────────
    // Lida de dois metros.
    int x = gfx_texto(&h, MARGEM, y, F_ENORME, v->hora);
    gfx_texto(&h, x + 18,
              y + gfx_altura_linha(F_ENORME) - gfx_altura_linha(F_CORPO) - 4,
              F_CORPO, v->saudacao);
    y += gfx_altura_linha(F_ENORME) + 16;

    // ── o que vem, e o que vem DEPOIS ───────────────────────────────────
    if (v->kicker[0]) {
        gfx_texto(&h, MARGEM, y, F_MIUDA, v->kicker);
        y += gfx_altura_linha(F_MIUDA) + 4;
    }

    gfx_texto_ate(&h, MARGEM, y, F_CORPO, v->proximo, h.l - MARGEM * 2);
    y += gfx_altura_linha(F_CORPO) + 2;

    if (v->onde[0]) {
        gfx_texto_ate(&h, MARGEM, y, F_MIUDA, v->onde, h.l - MARGEM * 2);
        y += gfx_altura_linha(F_MIUDA) + 2;
    }
    if (v->depois[0]) {
        y += 6;
        gfx_texto_ate(&h, MARGEM, y, F_MIUDA, v->depois, h.l - MARGEM * 2);
    }

    // ── e o aviso, no pé ─────────────────────────────────────────────────
    // O painel não se opera deitado.
    if (v->travado[0]) {
        int w = gfx_largura(F_MIUDA, v->travado);
        gfx_texto(&h, (h.l - w) / 2, h.a - gfx_altura_linha(F_MIUDA) - 12,
                  F_MIUDA, v->travado);
    }

    gfx_gira_horario(bm, &h);
}
