#include "cartao.h"
#include "chrome.h"
#include "grid.h"
#include "blocos.h"
#include "rolagem.h"
#include "../tela/logo_tinto.h"
#include "../tela/icones.h"
#include <string.h>

// O card do topo tem SOMBRA, um traço a 2 px: a única peça com relevo, e
// diz "isto não se aperta" (tudo o que se aperta é plano e inverte).
static int card(bitmap_t *bm, const vista_cartao_t *v, int y)
{
    const int x = GRID_MARGEM_DESTINO;
    const int larg = bm->l - GRID_MARGEM_DESTINO * 2;
    int topo = y;

    y += 8;

    // O ícone de sair ANTES do texto, para o kicker saber de que largura
    // dispõe.
    int wsair = v->tem_sair ? ICONES[ICO_SAIR].l + 10 : 0;
    if (v->tem_sair) {
        int sx = x + larg - ICONES[ICO_SAIR].l - 7;
        gfx_icone(bm, sx, y - 1, ICO_SAIR);
        // EXATAMENTE -1: o -2 é "nenhum seletor", o primeiro quadro de toda
        // entrada.
        if (v->cursor == -1)
            gfx_negativo(bm, sx - 4, y - 4,
                         ICONES[ICO_SAIR].l + 8, ICONES[ICO_SAIR].a + 6);
    }

    // O LOGOTIPO no lugar do kicker e do nome, quando o assunto é o aparelho.
    if (v->logo) {
        const int passo = (LOGO_TINTO_L + 7) / 8;
        const int esc = 2;                 // metade: 198 px não cabem em 228
        int lx = x + 10, ly = y + 2;
        for (int py = 0; py < LOGO_TINTO_A; py += esc)
            for (int px = 0; px < LOGO_TINTO_L; px += esc)
                if (LOGO_TINTO_BITS[py * passo + (px >> 3)] & (0x80 >> (px & 7)))
                    gfx_pixel(bm, lx + px / esc, ly + py / esc, true);
        y += LOGO_TINTO_A / esc + 8;
        goto fatos;
    }

    // O ícone empurra kicker e nome juntos: em x diferentes o nome pareceria
    // legenda do ícone.
    int col = 0;
    if (v->icone != ICO_NENHUM) {
        gfx_icone(bm, x + 8, y + 4, v->icone);
        col = ICONES[v->icone].l + 8;
    }

    int fim_kicker = gfx_texto_ate(bm, x + 8 + col, y, F_MIUDA, v->kicker,
                                   larg - 16 - col - wsair);

    chrome_pontinhos(bm, fim_kicker + 6, y + 3, v->pontos);

    y += gfx_altura_linha(F_MIUDA) + 2;

    // DUAS linhas para o nome: "Buscando agendas" não cabe numa em serifa.
    y = gfx_paragrafo(bm, x + 8 + col, y, larg - 16 - col, 2, F_EDITORIAL,
                      v->nome) + 4;

fatos:
    // A barra de progresso antes dos fatos: baixando, o quanto já foi é a
    // informação.
    if (v->barra_pct >= 0) {
        const int bl = larg - 16, bh = 9;
        gfx_ret(bm, x + 8, y, bl, bh, false);
        int cheio = (bl - 2) * v->barra_pct / 100;
        if (cheio > 0) gfx_ret(bm, x + 9, y + 1, cheio, bh - 2, true);
        y += bh + 6;
    }

    // Os fatos em forma de tabela, rótulo à esquerda e valor à direita.
    if (v->n_fatos) {
        gfx_hlin(bm, x + 8, y, larg - 16, 1);
        y += 5;
        for (int i = 0; i < v->n_fatos; i++) {
            // O valor CEDE ao rótulo (um e-mail longo passava por cima de "Google"). O
            // valor em destaque, o rótulo miúdo: é o negrito dito por tamanho.
            int wr   = gfx_largura(F_MIUDA, v->fatos[i].rotulo);
            int cabe = larg - 16 - wr - 8;
            int w    = gfx_largura(F_CORPO_P, v->fatos[i].valor);
            if (w > cabe) w = cabe;

            gfx_texto(bm, x + 8, y + 2, F_MIUDA, v->fatos[i].rotulo);
            gfx_texto_ate(bm, x + larg - 8 - w, y, F_CORPO_P,
                          v->fatos[i].valor, w);
            y += gfx_altura_linha(F_CORPO_P) + 1;
        }
        y += 2;
    } else {
        y += 4;
    }

    gfx_ret(bm, x, topo, larg, y - topo, false);
    gfx_hlin(bm, x + 3, y,          larg, 2);   // a sombra: duas linhas
    gfx_vlin(bm, x + larg, topo + 3, y - topo, 2);
    return y + 6;
}

// O corpo inteiro, a partir de `y`. A MESMA função mede e pinta.
static int corpo(bitmap_t *bm, const vista_cartao_t *v, int y)
{
    y = card(bm, v, y);

    // A explicação fica FORA do card: o card diz o que se tem, ela o que está
    // acontecendo.
    if (v->corpo[0]) {
        y = gfx_paragrafo(bm, GRID_MARGEM_DESTINO, y, bm->l - GRID_MARGEM_DESTINO * 2, 4, F_MIUDA,
                          v->corpo) + 10;
    }

    // ── as linhas passivas, antes dos destinos ──────────────────────────
    // Sem moldura e sem ">": a forma diz que não se apertam.
    if (v->n_info) {
        if (v->secao_info[0]) {
            gfx_texto(bm, GRID_MARGEM_DESTINO, y, F_MIUDA, v->secao_info);
            y += gfx_altura_linha(F_MIUDA) + 2;
        }
        for (int i = 0; i < v->n_info; i++) {
            gfx_hlin(bm, GRID_MARGEM_DESTINO, y,
                     bm->l - GRID_MARGEM_DESTINO * 2, 1);
            y += 5;

            int x = GRID_MARGEM_DESTINO + 4;
            if (v->info[i].ico != ICO_NENHUM) {
                gfx_icone(bm, x, y + 1, v->info[i].ico);
                x += ICONES[v->info[i].ico].l + 6;
            }

            int wv = gfx_largura(F_CORPO_P, v->info[i].valor);
            gfx_texto_ate(bm, x, y, F_CORPO_P, v->info[i].rotulo,
                          bm->l - GRID_MARGEM_DESTINO - x - wv - 8);
            gfx_texto(bm, bm->l - GRID_MARGEM_DESTINO - 4 - wv, y, F_CORPO_P,
                      v->info[i].valor);
            y += gfx_altura_linha(F_CORPO_P) + 1;

            if (v->info[i].sub[0]) {
                gfx_texto_ate(bm, x, y, F_MIUDA, v->info[i].sub,
                              bm->l - GRID_MARGEM_DESTINO * 2 - 8);
                y += gfx_altura_linha(F_MIUDA) + 1;
            }
            y += 4;
        }
        y += 4;
    }

    if (v->secao[0]) {
        gfx_texto(bm, GRID_MARGEM_DESTINO, y, F_MIUDA, v->secao);
        y += gfx_altura_linha(F_MIUDA) + 2;
    }

    if (v->n_dest) {
        gfx_hlin(bm, GRID_MARGEM_DESTINO, y, bm->l - GRID_MARGEM_DESTINO * 2, 1);
        y += 1;
    }

    for (int i = 0; i < v->n_dest; i++)
        y = bloco_destino(bm, y, bm->l - GRID_MARGEM_DESTINO * 2, v->dest[i].ico,
                          v->dest[i].titulo, v->dest[i].sub,
                          v->dest[i].valor, i == v->cursor, false);

    return y;
}

// A altura do corpo, medida no rascunho. Pública para o teste vigiar que
// a tela não rola.
int tela_cartao_altura(const bitmap_t *bm, const vista_cartao_t *v)
{
    bitmap_t *medida = rolagem_rascunho(bm);
    if (!medida) return 0;
    return corpo(medida, v, 0);
}

int tela_cartao_area(const bitmap_t *bm)
{
    return (bm->a - RODAPE_A - 6) - (BARRA_A + 9);
}

// Esta tela NÃO ROLA: rolando, o UC8253 deixava o card impresso sobre a
// lista. Se um estado novo crescer o corpo, o teste de altura quebra antes
// do vidro — e a resposta é tirar linha, não rolar.
void tela_cartao(bitmap_t *bm, const vista_cartao_t *v)
{
    gfx_limpa(bm, false);

    barra_t b = { v->titulo, v->hora, v->bateria, v->wifi, false, v->sinc };
    chrome_barra(bm, &b);

    corpo(bm, v, BARRA_A + 9);

    // No ícone de sair o OK desconecta, e o rodapé diz isso.
    const char *dir = (v->cursor == -1 && v->tem_sair) ? "OK desconectar"
                                                       : v->rodape_dir;
    if (v->paginas > 1) chrome_rodape_pagina(v->pagina, v->paginas);
    chrome_rodape(bm, v->rodape_esq, dir, false);
}
