#include "conferir.h"
#include "chrome.h"
#include "grid.h"
#include "rolagem.h"
#include "blocos.h"
#include "../tela/icones.h"
#include <stdio.h>
#include <string.h>

#define MARGEM GRID_MARGEM

// ── o corpo do Conferir ─────────────────────────────────────────────
// A MESMA função mede e pinta (somar à mão já errou três vezes aqui).
// `y_cursor` volta com o topo da linha selecionada, para a rolagem.
static int corpo(bitmap_t *bm, const vista_conferir_t *v, int y, int *y_cursor)
{
    const int larg = bm->l - MARGEM * 2;
    *y_cursor = y;

    // Quantas coisas nasceram desta fala, antes de qualquer cartão.
    if (v->kicker[0]) {
        gfx_texto(bm, MARGEM, y, F_MIUDA, v->kicker);
        y += gfx_altura_linha(F_MIUDA) + 7;
    }

    // Um CARTÃO por coisa: sem moldura, duas ações viravam seis linhas
    // seguidas.
    for (int i = 0; i < v->n_res; i++) {
        const linha_resultado_t *r = &v->res[i];
        if (i == v->cursor_res) *y_cursor = y;

        int topo_cartao = y;
        const int P = 6;                 // o respiro dentro do cartão
        y += P;

        // TRÊS linhas: tipo e destino numa só (RN-4D). O endereço saiu: não muda a
        // decisão.
        gfx_texto_ate(bm, MARGEM + P, y, F_MIUDA, r->tipo, larg - P * 2);
        y += gfx_altura_linha(F_MIUDA) + 3;

        int titulo_y = y;
        y = gfx_paragrafo(bm, MARGEM + P, y, larg - P * 2, 2, F_TITULO,
                          r->titulo) + 2;

        // Apagar RISCA o título: diz "vai sumir" antes do botão.
        if (r->apagando) {
            int meio = titulo_y + gfx_altura_linha(F_TITULO) / 2;
            gfx_hlin(bm, MARGEM + P, meio,
                     gfx_largura(F_TITULO, r->titulo), 1);
        }

        // O ANTES em cima e o DEPOIS embaixo. No apagar só há o antes, e ele
        // responde "qual deles?".
        if (r->antes[0]) {
            gfx_texto_ate(bm, MARGEM + P, y, F_MIUDA, r->antes,
                          larg - P * 2);
            y += gfx_altura_linha(F_MIUDA) + 1;
        }

        if (!r->apagando) {
            gfx_texto_ate(bm, MARGEM + P, y, F_MIUDA, r->quando,
                          larg - P * 2);
            y += gfx_altura_linha(F_MIUDA) + 1;
        }

        // A moldura por último, quando se sabe onde o cartão terminou.
        gfx_ret(bm, MARGEM, topo_cartao, larg, y - topo_cartao + P - 4, false);
        y += P + 3;
    }

    // A frase crua: sem ela, conferir viraria confiar.
    if (v->falou[0]) {
        // Sem rótulo: o itálico entre aspas já diz que é fala.
        y += 4;
        // INTEIRA, sem reticências; o que não couber, a tela rola.
        char citado[FALA_MAX + 8];
        snprintf(citado, sizeof citado, "\u201c%s\u201d", v->falou);
        y = gfx_paragrafo(bm, MARGEM + 4, y, larg - 8, 40, F_CITACAO, citado) + 12;
    }

    // ── enviando: as decisões saem, e o trabalho aparece ────────────────
    // Sem os botões e com os pontos andando: apertar de novo duplicava o
    // dentista.
    if (v->enviando) {
        gfx_texto(bm, MARGEM, y, F_TITULO, "Enviando");
        chrome_pontinhos_um_aceso(bm, MARGEM + gfx_largura(F_TITULO, "Enviando") + 8,
                                  y + gfx_altura_linha(F_TITULO) / 2 - 4,
                                  v->pontos);
        return y + gfx_altura_linha(F_TITULO) + 6;
    }

    for (int i = 0; i < 2; i++) {
        if (i == v->cursor) *y_cursor = y;
        y = bloco_acao(bm, y, &v->decisao[i], i == v->cursor);
    }
    return y;
}

// ── a tela ───────────────────────────────────────────────────────────
// TELA CHEIA, e só com resultado (a espera é da faixa de voz). O que não
// cabe ROLA em vez de encolher: encolher tira o que se veio conferir.
void tela_conferir(bitmap_t *bm, const vista_conferir_t *v)
{
    gfx_limpa(bm, false);

    const int topo = BARRA_A + 10;
    const int base = bm->a - RODAPE_A - 6;
    const int area = base - topo;

    // A medição, num bitmap que ninguém vê.
    int alto = area, y_cursor = 0;
    bitmap_t *medida = rolagem_rascunho(bm);
    if (medida) alto = corpo(medida, v, 0, &y_cursor);

    // Rola o MÍNIMO para a linha do cursor aparecer, nunca além do fim.
    // DECIDINDO, o alvo é o fim do conteúdo: as duas saídas visíveis juntas.
    int desloc = 0;
    if (alto > area) {
        int alvo = v->cursor >= 0 ? alto
                 : y_cursor + gfx_altura_linha(F_CORPO) + 9;
        if (alvo > area) desloc = alvo - area;
        if (desloc > alto - area) desloc = alto - area;
    }

    int fim = corpo(bm, v, topo - desloc, &y_cursor);

    // A barra por cima do que rolou para fora: pintá-la depois é mais barato
    // que recortar todas as primitivas.
    barra_t b = { v->titulo, v->hora, v->bateria, v->wifi, false, v->sinc };
    chrome_barra(bm, &b);

    // A seta é a pista de que rolar adianta.
    if (desloc > 0)
        gfx_texto(bm, bm->l - MARGEM - gfx_largura(F_MIUDA, "▲"),
                  BARRA_A + 1, F_MIUDA, "▲");
    if (fim > base)
        gfx_texto(bm, bm->l - MARGEM - gfx_largura(F_MIUDA, "▼"),
                  base - gfx_altura_linha(F_MIUDA) + 2, F_MIUDA, "▼");

    // Sem os pontos do rodapé quando o corpo já diz "Enviando" com os dele.
    if (v->pontos > 0 && !v->enviando) chrome_rodape_espera(v->pontos);
    chrome_rodape(bm, v->rodape_esq, v->rodape_dir, false);
}
