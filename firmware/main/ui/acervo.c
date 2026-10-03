#include "acervo.h"
#include "chrome.h"
#include "../tela/icones.h"
#include "../tela/qr.h"
#include "grid.h"
#include "pagina.h"
#include "../tela/texto.h"
#include <stdio.h>
#include <string.h>

// As medidas da tela aprovada: cartaz de 103 px com capa de 50, linha de 67
// com capa de 38.
#define CARTAZ_A   103
// 42 e não 50: a capa é reconhecimento, o título é leitura; cada pixel da
// moldura sai do nome da obra.
#define CAPA_L      42
#define CAPA_A      75
#define LINHA_A     67
#define MINI_L      34
#define MINI_A      49
// 24 e não 32: a marca tem 22 px, e o resto vai para o título.
#define MARCA_L     24
// Ícone e título forte numa faixa própria. Capa e refresh usam a mesma
// medida: desenho e motor concordam onde termina o cabeçalho.
#define SECAO_A      26

static int miolo_y(void) { return GRID_BARRA_A + 10; }

// O QR da vinculação em 3 px por módulo. Amostrar por MÓDULO preserva os
// quadrados; reduzir a imagem quebraria as bordas.
static int qr_do_aplicativo(bitmap_t *bm, int y)
{
    const int escala = 3;
    const int lado = QR_MODULOS * escala;
    const int x0 = (bm->l - lado) / 2;

    for (int my = 0; my < QR_MODULOS; my++) {
        const int sy = my * QR_ESCALA;
        const uint8_t *linha = &QR_BITS[sy * QR_BYTES_LINHA];
        for (int mx = 0; mx < QR_MODULOS; mx++) {
            const int sx = mx * QR_ESCALA;
            if (linha[sx / 8] & (0x80u >> (sx % 8)))
                gfx_ret(bm, x0 + mx * escala, y + my * escala,
                        escala, escala, true);
        }
    }
    return y + lado;
}

// A inicial da obra em serifa, dentro de uma moldura.
static void capa(bitmap_t *bm, int x, int y, int l, int a,
                 const char *titulo, fonte_t f)
{
    gfx_ret(bm, x, y, l, a, false);

    char inicial[2] = { titulo && titulo[0] ? titulo[0] : '?', '\0' };
    int tw = gfx_largura(f, inicial);
    int th = gfx_altura_linha(f);
    gfx_texto(bm, x + (l - tw) / 2, y + (a - th) / 2, f, inicial);
}

// A marca do estado, sem legenda: seta para baixo é baixar, o quadrado
// enchendo é a transferência, o tique é a cópia que abre.
static void marca(bitmap_t *bm, int x, int y, acervo_marca_t m, int pct)
{
    const int meio = y + 8;

    switch (m) {
    case ACERVO_SO_MEMORIA:
        // Memória integrada do Tinto, sem sugerir cartão removível.
        gfx_icone(bm, x, y, ICO_CAT_CARTAO);
        break;

    case ACERVO_LOCAL:
        // O TIQUE em diagonal, a pixel (não há linha inclinada): duas retas davam
        // um canto "⌐", não um tique.
        for (int i = 0; i < 4; i++) {
            gfx_pixel(bm, x + 2 + i, meio + i, true);
            gfx_pixel(bm, x + 2 + i, meio + i + 1, true);
        }
        for (int i = 0; i < 7; i++) {
            gfx_pixel(bm, x + 6 + i, meio + 3 - i, true);
            gfx_pixel(bm, x + 6 + i, meio + 4 - i, true);
        }
        break;

    case ACERVO_VINDO: {
        // A barra da TRANSFERÊNCIA, na marca, para o olho achar sem ler.
        const int larg = 22;
        gfx_ret(bm, x, meio - 3, larg, 7, false);
        int cheio = pct <= 0 ? 0 : (pct >= 100 ? larg - 2 : (larg - 2) * pct / 100);
        if (cheio > 0) gfx_ret(bm, x + 1, meio - 2, cheio, 5, true);
        break;
    }

    case ACERVO_BAIXAR:
    default:
        // Baixar: moldura arredondada e seta cheia, com cantos em degraus.
        gfx_hlin(bm, x + 3, y,     14, 2);
        gfx_hlin(bm, x + 3, y + 18, 14, 2);
        gfx_vlin(bm, x,     y + 3, 14, 2);
        gfx_vlin(bm, x + 18, y + 3, 14, 2);
        gfx_pixel(bm, x + 2,  y + 1,  true);
        gfx_pixel(bm, x + 1,  y + 2,  true);
        gfx_pixel(bm, x + 17, y + 1,  true);
        gfx_pixel(bm, x + 18, y + 2,  true);
        gfx_pixel(bm, x + 2,  y + 18, true);
        gfx_pixel(bm, x + 1,  y + 17, true);
        gfx_pixel(bm, x + 17, y + 18, true);
        gfx_pixel(bm, x + 18, y + 17, true);

        gfx_vlin(bm, x + 9, y + 4, 8, 3);
        for (int i = 0; i < 5; i++)
            gfx_hlin(bm, x + 5 + i, y + 10 + i, 11 - i * 2, 1);
        break;
    }
}

// Onde a lista começa: depois da barra, do cartaz (se há) e do rótulo.
static int topo_da_lista(const vista_acervo_t *v)
{
    int topo = miolo_y();
    // Com "Continuar lendo", a lista vem depois do cabeçalho DA SEÇÃO e do
    // cartaz; sem o cabeçalho na conta, a capa caía 31 px acima.
    if (v->tem_destaque) topo += SECAO_A + 5 + CARTAZ_A + 14;
    return topo + SECAO_A + 6;
}

int tela_acervo_cabem(const vista_acervo_t *v)
{
    int espaco = GRID_RODAPE_Y - topo_da_lista(v);
    int cabem = espaco / LINHA_A;
    return cabem < 0 ? 0 : cabem;
}

static int inicio_da_pagina(const vista_acervo_t *v)
{
    int cabem = tela_acervo_cabem(v);
    if (cabem <= 0 || v->cursor < 0) return 0;
    return (v->cursor / cabem) * cabem;
}

bool tela_acervo_capa_area(const vista_acervo_t *v, int i,
                           int *x, int *y, int *l, int *a)
{
    if (!v) return false;
    if (i == -1 && v->tem_destaque) {
        if (x) *x = GRID_MARGEM + 10;
        if (y) *y = miolo_y() + SECAO_A + 5
                    + (CARTAZ_A - CAPA_A) / 2;
        if (l) *l = CAPA_L;
        if (a) *a = CAPA_A;
        return true;
    }
    int inicio = inicio_da_pagina(v);
    int slot = i - inicio;
    if (i < 0 || i >= v->n || slot < 0 || slot >= tela_acervo_cabem(v))
        return false;
    if (x) *x = GRID_MARGEM;
    if (y) *y = topo_da_lista(v) + slot * LINHA_A + (LINHA_A - MINI_A) / 2;
    if (l) *l = MINI_L;
    if (a) *a = MINI_A;
    return true;
}

void tela_acervo_linha_area(const vista_acervo_t *v, int i,
                            int *x, int *y, int *l, int *a)
{
    if (x) *x = 0;
    if (l) *l = TELA_L;

    // Linha que não cabe não tem área: o motor sujaria pixels que não existem.
    int inicio = inicio_da_pagina(v);
    int slot = i - inicio;
    if (i < 0 || i >= v->n || slot < 0 || slot >= tela_acervo_cabem(v)) {
        if (y) *y = 0;
        if (a) *a = 0;
        return;
    }

    if (y) *y = topo_da_lista(v) + slot * LINHA_A;
    if (a) *a = LINHA_A;
}

void tela_acervo(bitmap_t *bm, const vista_acervo_t *v)
{
    gfx_limpa(bm, false);

    barra_t b = { v->titulo, v->hora, v->bateria, v->wifi, false, v->sinc };
    chrome_barra(bm, &b);

    int y = miolo_y();

    // ── o vazio DIZ que está vazio ──────────────────────────────────────
    if (v->vazio[0] && !v->tem_destaque && v->n == 0) {
        // No MEIO da tela: encostado no alto parece cabeçalho.
        int vy = GRID_BARRA_A + 10;

        // O LIVRO ABERTO dá corpo ao vazio; uma moldura vazia parecia imagem que
        // não carregou.
        gfx_icone(bm, (TELA_L - ICONES[ICO_ACERVO_GRANDE].l) / 2,
                  vy, ICO_ACERVO_GRANDE);
        vy += ICONES[ICO_ACERVO_GRANDE].a + 4;

        // Em DUAS linhas: "Seu Acervo está vazio" não cabe numa, e truncado parece
        // defeito.
        vy = gfx_paragrafo_centro(bm, 20, vy, TELA_L - 40, 2, F_EDITORIAL,
                                  v->vazio) + 5;

        // E COMO sair dele: quem manda livro é o app.
        int sw = gfx_largura(F_MIUDA, v->vazio_como);
        if (sw > TELA_L - 16) sw = TELA_L - 16;
        gfx_texto_ate(bm, (TELA_L - sw) / 2, vy, F_MIUDA,
                      v->vazio_como, TELA_L - 16);
        // Mais ar antes do QR, para ele ser o gesto e não parte da linha.
        vy += gfx_altura_linha(F_MIUDA) + 15;

        qr_do_aplicativo(bm, vy);

        chrome_rodape(bm, v->rodape_esq, "", false);
        return;
    }

    // ── CONTINUAR LENDO ──────────────────────────────────────────────────
    // "Onde eu estava?" merece a resposta em corpo grande, antes da lista.
    if (v->tem_destaque) {
        int hx = gfx_icone(bm, GRID_MARGEM, y, ICO_CONTINUAR_LENDO) + 5;
        int ty = y + (ICONES[ICO_CONTINUAR_LENDO].a
                      - gfx_altura_linha(F_TITULO)) / 2;
        gfx_texto(bm, hx, ty, F_TITULO, "Continuar lendo");
        y += SECAO_A + 5;

        const int larg = TELA_L - GRID_MARGEM * 2;
        gfx_ret(bm, GRID_MARGEM, y, larg, CARTAZ_A, false);

        capa(bm, GRID_MARGEM + 10, y + (CARTAZ_A - CAPA_A) / 2,
             CAPA_L, CAPA_A, v->destaque.titulo, F_EDITORIAL);

        int tx = GRID_MARGEM + 10 + CAPA_L + 10;
        int util = TELA_L - GRID_MARGEM - 8 - tx;
        // Duas linhas menores: o destaque revela o nome em vez de reticências.
        ty = y + 15;
        ty = gfx_paragrafo(bm, tx, ty, util, 2, F_CORPO,
                           v->destaque.titulo) + 5;
        gfx_texto_ate(bm, tx, ty, F_MIUDA, v->destaque.legenda, util);

        // Número preciso e barra de relance, os dois da LEITURA.
        int pct = 0;
        (void)sscanf(v->destaque.legenda, "%d", &pct);
        const int bx = tx, by = y + CARTAZ_A - 17;
        const int bl = util;
        gfx_ret(bm, bx, by, bl, 7, false);
        int cheio = pct <= 0 ? 0 : (pct >= 100 ? bl - 2 : (bl - 2) * pct / 100);
        if (cheio > 0) gfx_ret(bm, bx + 1, by + 1, cheio, 5, true);

        // O cartão inteiro inverte, como o foco da Home; a capa real é aplicada
        // depois, como folha branca no bloco preto.
        if (v->destaque_focado) chrome_cursor(bm, y, CARTAZ_A);

        y += CARTAZ_A + 14;
    }

    // ── a BIBLIOTECA ─────────────────────────────────────────────────────
    int hx = gfx_icone(bm, GRID_MARGEM, y, ICO_BIBLIOTECA) + 5;
    int titulo_y = y + (ICONES[ICO_BIBLIOTECA].a
                        - gfx_altura_linha(F_TITULO)) / 2;
    gfx_texto(bm, hx, titulo_y, F_TITULO, "Biblioteca");
    y += SECAO_A + 6;

    int cabem = tela_acervo_cabem(v);
    int inicio = inicio_da_pagina(v);
    int fim = inicio + cabem;
    if (fim > v->n) fim = v->n;

    for (int i = inicio; i < fim; i++) {
        const linha_obra_t *l = &v->linhas[i];
        if (y + LINHA_A > GRID_RODAPE_Y) break;

        if (i == v->cursor) chrome_cursor(bm, y, LINHA_A - 4);

        capa(bm, GRID_MARGEM, y + (LINHA_A - MINI_A) / 2, MINI_L, MINI_A,
             l->titulo, F_CORPO);

        int tx = GRID_MARGEM + MINI_L + 8;
        int util_titulo = TELA_L - GRID_MARGEM - tx;
        (void)gfx_paragrafo(bm, tx, y + 7, util_titulo, 2, F_CORPO_P,
                            l->titulo);
        int util_legenda = TELA_L - GRID_MARGEM - MARCA_L - 4 - tx;
        gfx_texto_ate(bm, tx, y + LINHA_A - gfx_altura_linha(F_MIUDA) - 8,
                      F_MIUDA, l->legenda, util_legenda);

        marca(bm, TELA_L - GRID_MARGEM - 20,
              y + LINHA_A - 28, l->marca, l->pct);

        gfx_hlin(bm, GRID_MARGEM, y + LINHA_A - 2,
                 TELA_L - GRID_MARGEM * 2, 1);
        y += LINHA_A;
    }

    // O que não coube é CONTADO: obra que some sem rastro parece perdida.
    int paginas = cabem > 0 ? (v->n + cabem - 1) / cabem : 1;
    int pagina = cabem > 0 ? inicio / cabem + 1 : 1;
    if (paginas > 1) pagina_marca(bm, pagina, paginas);
    chrome_rodape(bm, v->rodape_esq, v->rodape_dir, false);
}
