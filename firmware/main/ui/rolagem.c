#include "rolagem.h"
#include "chrome.h"
#include "pagina.h"
#include "../tela/texto.h"

bitmap_t *rolagem_rascunho(const bitmap_t *como)
{
    static uint8_t bits[(TELA_L / 8) * TELA_A];
    static bitmap_t bm;

    if ((size_t)como->passo * (size_t)como->a > sizeof bits) return NULL;

    bitmap_liga(&bm, bits, como->l, como->a);
    gfx_limpa(&bm, false);
    return &bm;
}

// ── páginas, e não passos ───────────────────────────────────────────
// Andar 56 px por toque deixava o texto sobre o próprio fantasma. Página
// inteira troca todo o conteúdo de uma vez.
int rolagem_paradas(int alto, int area)
{
    if (area <= 0 || alto <= area) return 1;
    return (alto + area - 1) / area;
}

int rolagem_desloc(int parada, int alto, int area)
{
    if (area <= 0 || alto <= area) return 0;
    if (parada < 0) parada = 0;

    int ultima = rolagem_paradas(alto, area) - 1;
    if (parada > ultima) parada = ultima;

    // SEM clamp no fim: clampar a última página a poria num deslocamento
    // quebrado, sobre o fantasma da anterior. Sobra branco, de propósito.
    return parada * area;
}

int rolagem_suave_paradas(int alto, int area)
{
    if (area <= 0 || alto <= area) return 1;
    return 1 + (alto - area + ROLAGEM_PASSO - 1) / ROLAGEM_PASSO;
}

int rolagem_suave_desloc(int parada, int alto, int area)
{
    if (area <= 0 || alto <= area || parada <= 0) return 0;
    int desloc = parada * ROLAGEM_PASSO;
    int ultimo = alto - area;
    return desloc < ultimo ? desloc : ultimo;
}

void rolagem_setas_laterais(bitmap_t *bm, int desloc, int alto, int area,
                            int topo, int fundo)
{
    if (alto <= area) return;

    const int x = bm->l - 13;
    if (desloc > 0) {
        gfx_limpa_ret(bm, x - 2, topo, 12, 14);
        gfx_texto(bm, x, topo, F_MIUDA, "▲");
    }
    if (desloc < alto - area) {
        int y = fundo - gfx_altura_linha(F_MIUDA);
        gfx_limpa_ret(bm, x - 2, y, 12, 14);
        gfx_texto(bm, x, y, F_MIUDA, "▼");
    }
}

// O "tem mais" virou o CONTADOR do rodapé. A assinatura mantém topo e
// fundo, sem uso, para não mexer nos chamadores.
void rolagem_setas(bitmap_t *bm, int desloc, int alto, int area,
                   int topo, int fundo)
{
    (void)topo; (void)fundo;
    pagina_marca(bm, pagina_atual(desloc, alto, area),
                 pagina_total(alto, area));
}
