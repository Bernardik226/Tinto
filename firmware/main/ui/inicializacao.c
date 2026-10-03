#include "inicializacao.h"
#include "chrome.h"
#include "../tela/icones.h"
#include "grid.h"
#include "../tela/logo_tinto.h"
#include <string.h>
#include <stdio.h>

// As medidas do desenho normativo: 11 px de margem lateral, 13 de topo,
// ação e opções empurradas para o pé.
#define MARGEM GRID_MARGEM
#define TOPO     13
#define UTIL     (240 - MARGEM * 2)

// ── o microSD físico ─────────────────────────────────────────────────
// Corpo de canto cortado sozinho parece folha de papel; o que o faz microSD
// são os CONTATOS na base.
#define SD_L       70
#define SD_A       92
#define SD_BORDA    5
#define SD_CHANFRO 20
#define SELO_R     21
#define SD_CONTATOS 6

static void desenha_sd(bitmap_t *bm, int x, int y, bool com_x)
{
    gfx_ret(bm, x, y, SD_L, SD_A, true);
    gfx_limpa_ret(bm, x + SD_BORDA, y + SD_BORDA,
                  SD_L - SD_BORDA * 2, SD_A - SD_BORDA * 2);

    // O chanfro no canto dos contatos: é a marca de orientação da peça.
    for (int i = 0; i < SD_CHANFRO; i++)
        gfx_limpa_ret(bm, x + SD_L - SD_CHANFRO + i, y, SD_CHANFRO - i, 1);
    for (int i = 0; i < SD_BORDA; i++) {
        int d = SD_CHANFRO - i;
        for (int p = 0; p < d; p++)
            gfx_pixel(bm, x + SD_L - d + p, y + i + (d - 1 - p), true);
    }

    // As trilhas, coladas na borda de cima a partir da esquerda.
    int cx = x + SD_BORDA + 4;
    int ct = y + SD_BORDA + 4;
    int passo = 7;
    for (int i = 0; i < SD_CONTATOS; i++) {
        int alt = (i >= SD_CONTATOS - 2) ? 16 : 26;   // as do chanfro são curtas
        gfx_ret(bm, cx + i * passo, ct, 4, alt, true);
    }

    if (!com_x) return;

    // O selo do X sobre a quina, meio para fora: nega o cartão sem parecer
    // parte dele.
    int sx = x + SD_L - 2;
    int sy = y + SD_A - 4;
    for (int dy = -SELO_R; dy <= SELO_R; dy++)
        for (int dx = -SELO_R; dx <= SELO_R; dx++)
            if (dx * dx + dy * dy <= SELO_R * SELO_R)
                gfx_pixel(bm, sx + dx, sy + dy, true);

    for (int i = -10; i <= 10; i++)
        for (int e = -1; e <= 1; e++) {
            gfx_pixel(bm, sx + i + e, sy + i, false);
            gfx_pixel(bm, sx + i + e, sy - i, false);
        }
}

// ── uma opção da lista ───────────────────────────────────────────────
// Quadrado de 9 px cheio quando selecionado, e a linha com contorno: dois
// sinais, para a seleção sobreviver de longe.
#define OPCAO_TEXTO_X (MARGEM + 22)
#define OPCAO_UTIL    (UTIL - 22 - 5)

// Quantas linhas a nota ocupa, medida com os avanços reais da fonte.
static int linhas_da_nota(const char *nota)
{
    if (!nota || !nota[0]) return 0;
    int linhas = 0;
    const char *p = nota;
    while (*p && linhas < 2) {
        int n = gfx_cabe(F_MIUDA, p, OPCAO_UTIL);
        if (n <= 0) break;
        p += n;
        while (*p == ' ') p++;
        linhas++;
    }
    return linhas;
}

static int desenha_opcao(bitmap_t *bm, int y, const char *texto,
                         const char *nota, bool selecionada)
{
    int nl = linhas_da_nota(nota);
    int altura = 8 + gfx_altura_linha(F_MIUDA) + 8;
    if (nl) altura += gfx_altura_linha(F_MIUDA) * nl + 2;

    gfx_ret(bm, MARGEM + 5, y + 9, 9, 9, false);
    if (selecionada) {
        gfx_ret(bm, MARGEM + 7, y + 11, 5, 5, true);
        gfx_ret(bm, MARGEM, y, UTIL, altura, false);
        gfx_ret(bm, MARGEM + 1, y + 1, UTIL - 2, altura - 2, false);
    }

    int ty = y + 8;
    gfx_texto(bm, OPCAO_TEXTO_X, ty, F_MIUDA, texto);
    if (nl)
        gfx_paragrafo(bm, OPCAO_TEXTO_X,
                      ty + gfx_altura_linha(F_MIUDA) + 2,
                      OPCAO_UTIL, nl, F_MIUDA, nota);

    gfx_hlin(bm, MARGEM, y + altura, UTIL, 1);
    return y + altura + 1;
}

void tela_inicializacao(bitmap_t *bm, const vista_inicializacao_t *v)
{
    gfx_limpa(bm, false);

    barra_t b = { .titulo = v->barra, .hora = v->hora,
                  .bateria = v->bateria, .wifi = v->wifi };
    int y = chrome_barra(bm, &b) + TOPO;

    // ── boas-vindas: a marca, e só ela ──────────────────────────────────
    if (v->logo) {
        int lx = (bm->l - LOGO_TINTO_L) / 2;
        int ly = y + 46;
        int passo = (LOGO_TINTO_L + 7) / 8;
        for (int py = 0; py < LOGO_TINTO_A; py++)
            for (int px = 0; px < LOGO_TINTO_L; px++)
                if (LOGO_TINTO_BITS[py * passo + (px >> 3)] & (0x80 >> (px & 7)))
                    gfx_pixel(bm, lx + px, ly + py, true);
        y = ly + LOGO_TINTO_A + 34;
    }

    // ── data e hora ──────────────────────────────────────────────────────
    if (v->n_campos > 0) {
        if (v->titulo[0]) {
            y = gfx_paragrafo(bm, MARGEM, y, UTIL, 2, F_TITULO, v->titulo);
            y += 10;
        }
        if (v->corpo[0]) {
            y = gfx_paragrafo(bm, MARGEM, y, UTIL, 5, F_MIUDA, v->corpo);
            y += 22;
        }

        // Cada coluna tem a largura do que está escrito ("2026" e "9 am" não cabem
        // em dois dígitos).
        int larg[5] = { 0 };
        for (int i = 0; i < v->n_campos && i < 5; i++) {
            int wv = gfx_largura(F_TITULO, v->campos[i]);
            int we = gfx_largura(F_MIUDA, v->etiquetas[i]);
            larg[i] = wv > we ? wv : we;
        }
        // O vão entre colunas é o que sobra, com teto: quatro vãos e um a mais
        // entre data e hora.
        int total = 0;
        for (int i = 0; i < v->n_campos && i < 5; i++) total += larg[i];
        int vao = (UTIL - 4 - total) / 5;
        if (vao > 16) vao = 16;
        if (vao < 8)  vao = 8;
        int x = MARGEM + 2;
        for (int i = 0; i < v->n_campos; i++) {
            int alt = gfx_altura_linha(F_TITULO) + 10;
            if (i == v->campo_ativo) {
                gfx_ret(bm, x - 4, y - 4, larg[i] + 8, alt + 8, false);
                gfx_ret(bm, x - 3, y - 3, larg[i] + 6, alt + 6, false);
            }
            // Valor e etiqueta centrados na coluna.
            int wv = gfx_largura(F_TITULO, v->campos[i]);
            int we = gfx_largura(F_MIUDA, v->etiquetas[i]);
            gfx_texto(bm, x + (larg[i] - wv) / 2, y + 3, F_TITULO,
                      v->campos[i]);
            gfx_texto(bm, x + (larg[i] - we) / 2, y + alt + 8, F_MIUDA,
                      v->etiquetas[i]);
            // O respiro maior depois do ano separa data e hora.
            x += larg[i] + (i == 2 ? 2 * vao : vao);
        }

        chrome_rodape(bm, v->rodape_esq, v->rodape_dir, false);
        return;
    }

    // ── "Prazer, {nome}." ────────────────────────────────────────────────
    if (v->nome[0]) {
        y += 40;
        y = gfx_paragrafo(bm, MARGEM, y, UTIL, 3, F_TITULO, v->nome);
        y += 15;
        gfx_paragrafo(bm, MARGEM, y, UTIL, 5, F_MIUDA, v->nota);
        chrome_rodape(bm, v->rodape_esq, v->rodape_dir, false);
        return;
    }

    if (v->icone_sd) {
        desenha_sd(bm, (bm->l - SD_L) / 2, y, v->icone_x);
        y += SD_A + 22;
    }

    if (v->rotulo[0]) {
        gfx_texto(bm, MARGEM, y, F_MIUDA, v->rotulo);
        y += gfx_altura_linha(F_MIUDA) + 7;
    }

    if (v->titulo[0]) {
        int y_fim = gfx_paragrafo(bm, MARGEM, y, UTIL, 2, F_TITULO, v->titulo);

        // Os pontinhos da espera ao lado da última linha do título.
        if (v->pontos >= 0) {
            const char *ultima = strrchr(v->titulo, '\n');
            ultima = ultima ? ultima + 1 : v->titulo;
            int fim  = MARGEM + gfx_largura(F_TITULO, ultima);
            int base = y_fim - 9;
            chrome_pontinhos(bm, fim + 6, base, v->pontos);
        }
        y = y_fim + 10;
    }

    if (v->corpo[0]) {
        y = gfx_paragrafo(bm, MARGEM, y, UTIL, 9, F_MIUDA, v->corpo);
        y += 12;
    }

    // ── as etapas da operação bloqueante ────────────────────────────────
    // Marca no que passou, quadrado cheio no atual, vazio no que falta. Nada
    // gira: muda a forma, uma vez por etapa.
    for (int i = 0; i < v->n_etapas; i++) {
        int cx = MARGEM + 5, cy = y + 4;

        if (i < v->etapa_atual) {
            gfx_icone(bm, MARGEM, y, ICO_VISTO);
        } else if (i == v->etapa_atual) {
            gfx_ret(bm, cx, cy, 9, 9, true);
        } else {
            gfx_ret(bm, cx, cy, 9, 9, false);
        }

        gfx_texto(bm, MARGEM + 22, y, F_CORPO_P, v->etapas[i]);
        y += gfx_altura_linha(F_CORPO_P) + 8;
    }
    if (v->n_etapas) y += 8;

    // O destaque, grande e centrado: algo para digitar em outro lugar.
    if (v->destaque[0]) {
        int w = gfx_largura(F_TITULO, v->destaque);
        gfx_texto(bm, (bm->l - w) / 2, y + 6, F_TITULO, v->destaque);
        y += gfx_altura_linha(F_TITULO) + 18;
    }

    // Daqui em diante tudo ancora no PÉ: ação e opções onde o polegar espera.
    int pe = bm->a;
    if (v->rodape_esq[0] || v->rodape_dir[0]) {
        chrome_rodape(bm, v->rodape_esq, v->rodape_dir, false);
        pe -= RODAPE_A;
    }

    if (v->n_opcoes > 0 && v->alerta[0]) {
        // O alerta em negrito acima das opções.
        y = gfx_paragrafo(bm, MARGEM, y, UTIL, 2, F_CORPO, v->alerta);
        y += 10;
    }

    if (v->n_opcoes > 0) {
        int altura = 0;
        for (int i = 0; i < v->n_opcoes; i++) {
            altura += 8 + gfx_altura_linha(F_MIUDA) + 8 + 1;
            int nl = linhas_da_nota(v->notas[i]);
            if (nl) altura += gfx_altura_linha(F_MIUDA) * nl + 2;
        }
        int oy = pe - altura - 8;
        gfx_hlin(bm, MARGEM, oy, UTIL, 1);
        oy += 1;
        for (int i = 0; i < v->n_opcoes; i++)
            oy = desenha_opcao(bm, oy, v->opcoes[i], v->notas[i],
                               i == v->selecionada);
        return;
    }

    if (v->n_estados > 0) {
        int alt = gfx_altura_linha(F_MIUDA) + 16;
        int ey = pe - alt * v->n_estados - 8;
        chrome_filete(bm, ey, FILETE_GROSSO);
        ey += 4;
        for (int i = 0; i < v->n_estados; i++) {
            gfx_texto(bm, MARGEM + 2, ey + 8, F_MIUDA, v->estados[i]);
            gfx_hlin(bm, MARGEM, ey + alt, UTIL, 1);
            ey += alt;
        }
        return;
    }

    if (v->alerta[0]) {
        // Três linhas: a frase da tela de ausência tem 74 caracteres.
        int linhas = gfx_altura_linha(F_CORPO) * 3;
        int ay = pe - linhas - 16;
        chrome_filete(bm, ay, FILETE_GROSSO);
        gfx_paragrafo(bm, MARGEM, ay + 12, UTIL, 3, F_CORPO, v->alerta);
    }
}
