#include "vincular.h"
#include "chrome.h"
#include "grid.h"
#include "../tela/qr.h"
#include "../tela/texto.h"
#include <stdio.h>
#include <string.h>

#define MARGEM GRID_MARGEM

// O QR é um BITMAP PRONTO (tela/qr.c, gerado por firmware/ferramentas/qr.py):
// o endereço do app não muda, e codificar em execução seria Reed-Solomon
// para desenhar sempre o mesmo.
static void poe_qr(bitmap_t *bm, int y)
{
    int x0 = (bm->l - QR_LADO) / 2;

    for (int y2 = 0; y2 < QR_LADO; y2++) {
        const uint8_t *linha = &QR_BITS[y2 * QR_BYTES_LINHA];
        for (int x = 0; x < QR_LADO; x++)
            if (linha[x / 8] & (0x80u >> (x % 8)))
                gfx_pixel(bm, x0 + x, y + y2, true);
    }
}

void tela_vincular(bitmap_t *bm, const vista_vincular_t *v)
{
    gfx_limpa(bm, false);

    barra_t b = { v->titulo, v->hora, v->bateria, v->wifi, false,
                  v->sinc };
    int y = chrome_barra(bm, &b) + 6;

    // ── sem token, o QR ainda não entra ─────────────────────────────────
    // Sem registro não há código; no lugar, o número do aparelho e o aviso.
    if (v->sem_token) {
        y += 24;
        int larg = gfx_largura(F_TITULO, v->id);
        gfx_texto(bm, (bm->l - larg) / 2, y, F_TITULO, v->id);
        y += gfx_altura_linha(F_TITULO) + 22;

        for (int i = 0; i < 3; i++)
            y = gfx_paragrafo(bm, MARGEM, y, bm->l - MARGEM * 2, 2, F_MIUDA,
                              v->passo[i]) + 4;

        if (v->pontos > 0) chrome_rodape_espera(v->pontos);
    chrome_rodape(bm, v->rodape_esq, v->rodape_dir, false);
        return;
    }

    if (v->somente_app) {
        // A barra diz ONDE se está; este card diz O QUE é o app.
        const int cx = GRID_MARGEM_DESTINO;
        const int cl = bm->l - GRID_MARGEM_DESTINO * 2;
        const int ca = 62;
        gfx_ret(bm, cx, y, cl, ca, false);
        gfx_hlin(bm, cx + 3, y + ca, cl, 2);
        gfx_vlin(bm, cx + cl, y + 3, ca, 2);

        const int iy = y + (ca - ICONES[ICO_TINTO].a) / 2;
        gfx_icone(bm, cx + 11, iy, ICO_TINTO);
        const int tx = cx + 11 + ICONES[ICO_TINTO].l + 10;
        gfx_texto(bm, tx, y + 10, F_EDITORIAL, "Tinto App");
        gfx_texto_ate(bm, tx, y + 36, F_MIUDA, "Seu Tinto no celular",
                      cx + cl - 9 - tx);
        y += ca + 12;

        poe_qr(bm, y);
        y += QR_LADO + 12;
        for (int i = 0; i < 2; i++)
            y = gfx_paragrafo_centro(bm, MARGEM, y, bm->l - MARGEM * 2,
                                     2, F_MIUDA, v->passo[i]) + 5;
        if (v->id[0]) {
            char id[40];
            snprintf(id, sizeof id, "Este Tinto · %s", v->id);
            int w = gfx_largura(F_MIUDA, id);
            if (w > bm->l - MARGEM * 2) w = bm->l - MARGEM * 2;
            gfx_texto_ate(bm, (bm->l - w) / 2, y + 3, F_MIUDA,
                          id, bm->l - MARGEM * 2);
        }
        chrome_rodape(bm, v->rodape_esq, "", false);
        return;
    }

    poe_qr(bm, y);
    y += QR_LADO + 8;

    // O código GRANDE e centrado: lido aqui, digitado no celular.
    const char *texto = v->esperando ? "······" : v->codigo;
    int larg = gfx_largura(F_TITULO, texto);
    gfx_texto(bm, (bm->l - larg) / 2, y, F_TITULO, texto);
    y += gfx_altura_linha(F_TITULO) + 3;

    if (v->prazo[0]) {
        larg = gfx_largura(F_MIUDA, v->prazo);
        gfx_texto(bm, (bm->l - larg) / 2, y, F_MIUDA, v->prazo);
        y += gfx_altura_linha(F_MIUDA) + 6;
    }

    for (int i = 0; i < 3; i++)
        y = gfx_paragrafo(bm, MARGEM, y, bm->l - MARGEM * 2, 2, F_MIUDA,
                          v->passo[i]) + 2;

    if (v->pontos > 0) chrome_rodape_espera(v->pontos);
    chrome_rodape(bm, v->rodape_esq, v->rodape_dir, false);
}
