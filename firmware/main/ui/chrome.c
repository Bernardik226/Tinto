#include <stdio.h>
#include "chrome.h"
#include "../vista/wifi.h"
#include "../tela/icones.h"
#include <string.h>

#include "grid.h"

#define MARGEM GRID_MARGEM

// O ícone do sinal, o MESMO na barra e na lista: três níveis, a régua das
// palavras (33% e 66% separam forte, média e fraca). `forca` 0 é
// "conectando": nível mínimo, sem sumir (a barra dançaria).
icone_id chrome_icone_wifi(int forca)
{
    return vista_icone_wifi(forca);
}

static void desenha_wifi(bitmap_t *bm, int x, int y, int forca)
{
    (void)y;
    icone_id i = chrome_icone_wifi(forca);
    gfx_icone(bm, x, (BARRA_A - ICONES[i].a) / 2, i);
}

int chrome_barra(bitmap_t *bm, const barra_t *b)
{
    // Limpa a própria faixa antes de escrever: conteúdo de baixo não atravessa
    // o topo. O rodapé faz o mesmo.
    gfx_limpa_ret(bm, 0, 0, bm->l, BARRA_A);

    // O título PARA antes do canto de estado (110 px: sync, Wi-Fi, hora e
    // bateria); hora e bateria nunca se movem e nada escreve sobre elas. Tudo
    // centrado na altura da própria peça, numa conta só.
    if (b->titulo)
        gfx_texto_ate(bm, 7, (BARRA_A - gfx_altura_linha(F_MIUDA)) / 2,
                      F_MIUDA, b->titulo, bm->l - 110 - 7 - 4);

    // A ordem é [sync] [wi-fi] [hora] [bateria]: os voláteis à ESQUERDA, e
    // hora e bateria nunca se movem.
    int x = bm->l - 7;

    // ── bateria: contorno e PREENCHIMENTO SÓLIDO ─────────────────────────
    // Traços separados por 1 px viram moiré sem antialiasing. Uma barra contínua
    // dá quinze passos de resolução em 15 px.
    const int BL = 19, BA = 11;            // corpo, sem o terminal
    x -= BL + 2;
    int by = (BARRA_A - BA) / 2;

    gfx_ret(bm, x, by, BL, BA, false);                  // parede
    gfx_ret(bm, x + BL, by + 3, 2, BA - 6, true);       // terminal

    if (b->carregando) {
        // O raio, dentro do corpo: cinco degraus em 7 px de altura útil.
        int cx = x + BL / 2, cy = by + BA / 2;
        gfx_hlin(bm, cx - 1, cy - 3, 4, 1);
        gfx_hlin(bm, cx - 2, cy - 2, 4, 1);
        gfx_hlin(bm, cx - 3, cy - 1, 5, 1);
        gfx_hlin(bm, cx - 1, cy,     5, 1);
        gfx_hlin(bm, cx - 2, cy + 1, 4, 1);
        gfx_hlin(bm, cx - 3, cy + 2, 4, 1);
    } else {
        // 15 px de interior; enche da esquerda para a direita.
        int dentro = BL - 4;
        int cheio  = b->bateria * dentro / 100;
        if (cheio > dentro) cheio = dentro;

        if (cheio > 0)
            gfx_ret(bm, x + 2, by + 2, cheio, BA - 4, true);

        // Vazia não fica igual a "não sei": um traço na base diz que a medida
        // existe e é zero.
        if (cheio == 0)
            gfx_hlin(bm, x + 2, by + BA - 3, dentro, 1);
    }

    if (b->hora && b->hora[0]) {
        x -= 6 + gfx_largura(F_CORPO, b->hora);
        // Um pixel abaixo do centro: dígitos não têm descendente, e centrar a
        // caixa da fonte deixa a tinta acima da bateria.
        gfx_texto(bm, x, (BARRA_A - gfx_altura_linha(F_CORPO)) / 2 + 1,
                  F_CORPO, b->hora);
    }

    // À esquerda da hora, e só com rede.
    if (b->wifi >= 0) {
        x -= 20;
        desenha_wifi(bm, x, 0, b->wifi);
    }

    // ── a sincronização, à esquerda de tudo o que é volátil ─────────────
    // Formas DISTINTAS: subir, descer, o ciclo e o erro. Nenhuma gira. Qual
    // delas é decisão da vista.
    if (b->sinc != ICO_NENHUM) {
        x -= ICONES[b->sinc].l + 7;
        gfx_icone(bm, x, (BARRA_A - ICONES[b->sinc].a) / 2, b->sinc);
    }

    // Sem inversão: a barra fica em PAPEL, com tinta por cima.
    return BARRA_A;
}

// ── filetes ─────────────────────────────────────────────────────────
int chrome_filete(bitmap_t *bm, int y, filete_t f)
{
    int esp = f == FILETE_GROSSO ? 2 : 1;
    if (f == FILETE_GROSSO) gfx_hlin(bm, 0, y, bm->l, esp);
    else                    gfx_hlin(bm, MARGEM, y, bm->l - MARGEM * 2, esp);
    return y + esp;
}

// ── rodapé ──────────────────────────────────────────────────────────
// O contador mora aqui: quem reserva o centro tem de ser quem escreve as
// bordas (desenhado por fora, saía "BACK conexão1 / 2").
static int paginas_atual, paginas_total;
static int espera_pontos;

// A seta da paginação: triângulo de sete por quatro, cheio, sem contorno
// (contorno em 7 px vira mancha).
#define SETA_L 7
#define SETA_A 4

static void seta_mini(bitmap_t *bm, int x, int y, bool baixo)
{
    for (int i = 0; i < SETA_A; i++) {
        int larg = SETA_L - i * 2;
        if (larg <= 0) break;
        int linha = baixo ? y + i : y + (SETA_A - 1 - i);
        gfx_ret(bm, x + i, linha, larg, 1, true);
    }
}

icone_id chrome_seta_da_pagina(int atual, int total)
{
    if (total <= 1) return ICO_NENHUM;
    return atual < total ? ICO_DESCENDO : ICO_SUBINDO;
}

int chrome_pontinhos(bitmap_t *bm, int x, int y, int n)
{
    for (int i = 0; i < n; i++) gfx_ret(bm, x + i * 8, y, 5, 5, true);
    return n > 0 ? n * 8 - 3 : 0;
}

void chrome_pontinhos_um_aceso(bitmap_t *bm, int x, int y, int aceso)
{
    for (int i = 0; i < 3; i++, x += 13) {
        if (i == aceso) gfx_ret(bm, x, y, 8, 8, true);
        else            gfx_ret(bm, x, y + 2, 4, 4, false);
    }
}

void chrome_rodape_espera(int pontos)
{
    espera_pontos = pontos;
}

int chrome_pixels_do_rodape(const bitmap_t *bm)
{
    int n = 0;
    for (int y = bm->a - RODAPE_A; y < bm->a; y++)
        for (int x = 0; x < bm->l; x++)
            if (gfx_le(bm, x, y)) n++;
    return n;
}

void chrome_rodape_pagina(int atual, int total)
{
    paginas_atual = atual;
    paginas_total = total;
}

void chrome_rodape(bitmap_t *bm, const char *esq, const char *dir, bool negativo)
{
    int y = bm->a - RODAPE_A;

    // A faixa é DELE: limpa antes, senão a gaveta escrevia o rodapé por cima do
    // de baixo ("BACKendário").
    gfx_limpa_ret(bm, 0, y, bm->l, RODAPE_A);
    gfx_hlin(bm, 0, y, bm->l, 1);

    // O contador no centro do VÃO entre os rótulos, não da tela, e calculado
    // antes deles: são os rótulos que cedem ("BACK cone…" ainda se lê; "1/2"
    // não sobrevive a corte).
    int w_esq = esq ? gfx_largura(F_MIUDA, esq) : 0;
    int w_dir = dir ? gfx_largura(F_MIUDA, dir) : 0;
    int meia  = 7 + w_esq + (bm->l - 14 - w_esq - w_dir) / 2;
    int borda_esq = bm->l, borda_dir = 0;

    // A ESPERA no mesmo centro do contador: tela que espera não está sendo
    // navegada, e o que muda vem primeiro.
    if (espera_pontos > 0) {
        int larg = espera_pontos * 8 - 3;
        chrome_pontinhos(bm, meia - larg / 2, y + 8, espera_pontos);
        borda_esq = meia - larg / 2 - 6;
        borda_dir = meia + larg / 2 + 6;
    }
    else
    if (paginas_total > 1) {
        char txt[12];
        // "1/2" sem espaços: os quinze pixels que poupavam reticências ao lado.
        snprintf(txt, sizeof txt, "%d/%d", paginas_atual, paginas_total);
        // O número e a SETA como um bloco: onde se está e qual botão leva adiante.
        // Seta miúda desenhada aqui (o ícone de 13 px comia sete pixels).
        icone_id seta = chrome_seta_da_pagina(paginas_atual, paginas_total);
        int wt = gfx_largura(F_MIUDA, txt);
        int ws = seta != ICO_NENHUM ? SETA_L + 4 : 0;
        int x  = meia - (wt + ws) / 2;

        gfx_texto(bm, x, y + 4, F_MIUDA, txt);
        if (seta != ICO_NENHUM)
            seta_mini(bm, x + wt + 4, y + (RODAPE_A - SETA_A) / 2,
                      seta == ICO_DESCENDO);

        borda_esq = x - 5;
        borda_dir = x + wt + ws + 5;
    }

    if (esq) gfx_texto_ate(bm, 7, y + 4, F_MIUDA, esq,
                           (borda_esq < bm->l ? borda_esq : bm->l - 7) - 7);
    if (dir) {
        int larg = gfx_largura(F_MIUDA, dir);
        int x = bm->l - larg - 7;
        int piso = borda_dir > 0 ? borda_dir : 7;
        if (x < piso) { larg = bm->l - 7 - piso; x = piso; }
        gfx_texto_ate(bm, x, y + 4, F_MIUDA, dir, larg);
    }

    // negativo = ESTADO ATIVO (gravando), não "há cursor".
    if (negativo) gfx_negativo(bm, 0, y, bm->l, RODAPE_A);

    // Zera depois de usar: o contador é DESTE quadro.
    paginas_atual = paginas_total = 0;
    espera_pontos = 0;
}

// ── cursor ──────────────────────────────────────────────────────────
// A moldura com largura dada de fora, para dentro de um pop-over.
void chrome_cursor_em(bitmap_t *bm, int x, int y, int larg, int altura)
{
    gfx_ret(bm, x, y - 3, larg, altura + 6, false);
}

void chrome_cursor(bitmap_t *bm, int y, int altura)
{
    chrome_cursor_em(bm, MARGEM - 6, y, bm->l - (MARGEM - 6) * 2, altura);
}

// ── selo ────────────────────────────────────────────────────────────
int chrome_selo(bitmap_t *bm, int x, int y, const char *texto, bool negativo)
{
    if (!texto || !*texto) return x;
    int l = gfx_largura(F_MIUDA, texto) + 8;
    int a = gfx_altura_linha(F_MIUDA) + 1;

    gfx_texto(bm, x + 4, y, F_MIUDA, texto);
    if (negativo) gfx_negativo(bm, x, y - 1, l, a);
    return x + l + 4;
}

// ── caixa de marcar ─────────────────────────────────────────────────
int chrome_caixa(bitmap_t *bm, int x, int y, bool marcada)
{
    return gfx_icone(bm, x, y - 2, marcada ? ICO_CAIXA_ON : ICO_CAIXA) + 4;
}

void chrome_risco(bitmap_t *bm, int x, int y, int largura, fonte_t f)
{
    gfx_hlin(bm, x, y + FONTES[f].ascent - FONTES[f].altura_x / 2, largura, 1);
}

// ── origem ──────────────────────────────────────────────────────────
void chrome_origem(bitmap_t *bm, int y, int altura)
{
    gfx_vlin(bm, 2, y, altura, 3);
}
