#include "xadrez.h"
#include "chrome.h"
#include "rolagem.h"
#include "faixa.h"
#include "blocos.h"
#include "../tela/fontes.h"
#include "../tela/texto.h"
#include <stdio.h>
#include <string.h>

#define M 8
#define TAB_X 4
#define TAB_Y 68
#define CASA_V 29

static void desenha_peca(bitmap_t *bm, int x, int y, uint8_t p, bool escura,
                         bool girada, int casa);

static void robo_mini(bitmap_t *bm, int x, int y)
{
    gfx_ret(bm, x + 3, y + 5, 23, 19, false);
    gfx_ret(bm, x + 8, y + 11, 3, 3, true);
    gfx_ret(bm, x + 19, y + 11, 3, 3, true);
    gfx_hlin(bm, x + 11, y + 19, 8, 1);
    gfx_vlin(bm, x + 14, y, 5, 2);
}

static void peao_mini(bitmap_t *bm, int x, int y, bool cheio)
{
    static const uint16_t forma[] = {
        0x038, 0x07c, 0x0fe, 0x0fe, 0x07c, 0x038,
        0x038, 0x07c, 0x07c, 0x0fe, 0x0fe, 0x1ff,
        0x1ff, 0x3ff, 0x7ff, 0x7ff, 0x7ff,
    };
    for (int j = 0; j < 17; j++) for (int i = 0; i < 11; i++) {
        uint16_t bit = (uint16_t)(1u << (10 - i));
        if (!(forma[j] & bit)) continue;
        bool borda = j == 0 || j == 16 || i == 0 || i == 10;
        if (!borda && !cheio)
            borda = !(forma[j - 1] & bit) || !(forma[j + 1] & bit) ||
                    !(forma[j] & (bit << 1)) || !(forma[j] & (bit >> 1));
        if (cheio || borda) gfx_pixel(bm, x + i, y + j, true);
    }
}

static int cabecalho(bitmap_t *bm, const vista_xadrez_t *v, const char *titulo)
{
    gfx_limpa(bm, false);
    barra_t b = { titulo, v->hora, v->bateria, v->wifi, false, v->sinc };
    return chrome_barra(bm, &b);
}

static void icone_linha(bitmap_t *bm, int x, int y, const char *titulo,
                        bool cartaz)
{
    if (!strcmp(titulo, "Xadrez")) {
        gfx_icone(bm, x, y, ICO_XADREZ);
    } else if (cartaz && (!strcmp(titulo, "Continuar partida") ||
                         !strcmp(titulo, "Nova partida"))) {
        for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++)
            if ((r + c) & 1) gfx_ret(bm, x + c * 5, y + r * 5, 5, 5, true);
        gfx_ret(bm, x, y, 20, 20, false);
        if (titulo[0] == 'N') { gfx_hlin(bm, x + 20, y + 23, 8, 2); gfx_vlin(bm, x + 23, y + 20, 8, 2); }
        else for (int i = 0; i < 8; i++) gfx_vlin(bm, x + 20 + i, y + 20 + i / 2, 8 - i, 1);
    } else if (!strcmp(titulo, "Contra a máquina")) {
        robo_mini(bm, x, y);
    } else if (!strcmp(titulo, "Duas pessoas")) {
        peao_mini(bm, x + 2, y + 6, false);
        peao_mini(bm, x + 16, y + 6, true);
    } else if (!strcmp(titulo, "Opções")) {
        for (int py = 1; py < 28; py++) for (int px = 1; px < 28; px++) {
            int dx = px - 14, dy = py - 14, ax = dx < 0 ? -dx : dx;
            int ay = dy < 0 ? -dy : dy, d2 = dx * dx + dy * dy;
            bool aro = d2 >= 45 && d2 <= 105;
            bool reto = ((ax <= 2 && ay >= 9 && ay <= 13) ||
                         (ay <= 2 && ax >= 9 && ax <= 13));
            bool diagonal = d2 >= 85 && d2 <= 145 &&
                            (ax - ay <= 2 && ay - ax <= 2);
            if ((aro || reto || diagonal) && d2 >= 24)
                gfx_pixel(bm, x + px, y + py, true);
        }
    } else if (!strcmp(titulo, "Vertical") || !strcmp(titulo, "Horizontal")) {
        bool h = titulo[0] == 'H';
        gfx_ret(bm, x + (h ? 0 : 6), y + (h ? 7 : 1), h ? 29 : 17, h ? 16 : 27, false);
        gfx_pixel(bm, x + 2, y + 3, true); gfx_pixel(bm, x + 3, y + 2, true);
        gfx_pixel(bm, x + 26, y + 26, true); gfx_pixel(bm, x + 27, y + 25, true);
    } else if (!strcmp(titulo, "Dificuldade")) {
        gfx_ret(bm, x + 2, y + 18, 5, 8, true); gfx_ret(bm, x + 11, y + 11, 5, 15, true);
        gfx_ret(bm, x + 20, y + 3, 5, 23, true);
    } else if (!strcmp(titulo, "Começar partida")) {
        gfx_vlin(bm, x + 6, y + 2, 25, 2);
        gfx_hlin(bm, x + 3, y + 26, 9, 2);
        for (int i = 0; i < 15; i++) gfx_vlin(bm, x + 8 + i, y + 3, 12 - i / 2, 1);
    } else if (!strcmp(titulo, "Ver histórico")) {
        for (int i = 0; i < 3; i++) {
            gfx_ret(bm, x + 2, y + 5 + i * 7, 3, 3, true);
            gfx_hlin(bm, x + 9, y + 6 + i * 7, 18 - i * 3, 1);
        }
    } else if (!strcmp(titulo, "Salvar e sair")) {
        gfx_ret(bm, x + 3, y + 2, 23, 25, false);
        gfx_ret(bm, x + 8, y + 3, 13, 8, true);
        gfx_ret(bm, x + 9, y + 16, 11, 8, false);
    } else if (!strcmp(titulo, "Abandonar") ||
               !strcmp(titulo, "Encerrar confronto")) {
        for (int i = 0; i < 15; i++) {
            gfx_ret(bm, x + 7 + i, y + 7 + i, 2, 2, true);
            gfx_ret(bm, x + 21 - i, y + 7 + i, 2, 2, true);
        }
    } else if (!strcmp(titulo, "Substituir")) {
        gfx_hlin(bm, x + 5, y + 5, 20, 3); gfx_hlin(bm, x + 5, y + 22, 20, 3);
        for (int i = 0; i < 15; i++) {
            gfx_pixel(bm, x + 7 + i, y + 7 + i, true);
            gfx_pixel(bm, x + 21 - i, y + 7 + i, true);
        }
    } else if (!strcmp(titulo, "Aceitar empate")) {
        gfx_hlin(bm, x + 4, y + 10, 21, 3); gfx_hlin(bm, x + 4, y + 19, 21, 3);
    } else if (!strcmp(titulo, "Voltar ao início") ||
               !strcmp(titulo, "Continuar jogando") ||
               !strcmp(titulo, "Continuar partida") ||
               !strcmp(titulo, "Manter partida")) {
        gfx_hlin(bm, x + 8, y + 14, 15, 2);
        for (int i = 0; i < 6; i++) {
            gfx_ret(bm, x + 7 + i, y + 9 + i, 2, 2, true);
            gfx_ret(bm, x + 7 + i, y + 19 - i, 2, 2, true);
        }
    } else if (!strcmp(titulo, "Revanche")) {
        gfx_hlin(bm, x + 7, y + 6, 13, 2);
        gfx_vlin(bm, x + 20, y + 7, 13, 2);
        gfx_hlin(bm, x + 9, y + 20, 12, 2);
        for (int i = 0; i < 5; i++)
            gfx_pixel(bm, x + 5 + i, y + 6 + i, true);
    } else if (!strcmp(titulo, "Voltar a Jogos")) {
        for (int r = 0; r < 2; r++) for (int c = 0; c < 2; c++)
            gfx_ret(bm, x + 3 + c * 13, y + 3 + r * 13, 9, 9, c + r > 0);
    } else {
        gfx_ret(bm, x + 8, y + 3, 13, 13, false);
        gfx_ret(bm, x + 4, y + 18, 21, 8, false);
    }
}

static void linha(bitmap_t *bm, int y, const char *titulo, const char *sub, bool foco)
{
    gfx_hlin(bm, M, y, bm->l - M * 2, 1);
    icone_linha(bm, M + 3, y + 9, titulo, false);
    gfx_texto(bm, M + 38, y + 9, F_TITULO, titulo);
    gfx_texto_ate(bm, M + 38, y + 34, F_MIUDA, sub, bm->l - M * 2 - 42);
    if (foco) gfx_negativo(bm, M, y + 1, bm->l - M * 2, 55);
}

void tela_jogos(bitmap_t *bm, const vista_xadrez_t *v)
{
    int y = cabecalho(bm, v, "Jogos") + 12;
    gfx_texto(bm, M, y, F_MIUDA, "UMA PAUSA TRANQUILA");
    y += 24; gfx_texto(bm, M, y, F_EDITORIAL, "Jogos");
    y += 36;
    gfx_hlin(bm, M, y, bm->l - M * 2, 1);
    icone_linha(bm, M + 3, y + 9, "Xadrez", false);
    gfx_texto(bm, M + 38, y + 9, F_TITULO, "Xadrez");
    gfx_paragrafo(bm, M + 38, y + 34, bm->l - M * 2 - 38, 2, F_MIUDA,
                  "Desafie seus amigos ou a máquina!");
    if (v->cursor != 255) gfx_negativo(bm, M, y + 1, bm->l - M * 2, 70);
    chrome_rodape(bm, "BACK Home", "OK abrir", false);
}

static uint32_t codigo_peca(uint8_t p)
{
    static const uint8_t ordem[] = {0,5,4,3,2,1,0};
    return 0x265a + ordem[xadrez_tipo(p)];
}

static void pixel_peca(bitmap_t *bm, int x, int y, int px, int py,
                       bool escura, bool girada, int casa)
{
    if (girada) { px = casa - 1 - px; py = casa - 1 - py; }
    gfx_pixel(bm, x + px, y + py, !escura);
}

static void desenha_dama(bitmap_t *bm, int x, int y, uint8_t p,
                         bool escura, bool girada, int casa)
{
    static const uint32_t mascara[] = {
        0x22222, 0x77777, 0x33766, 0x1b76c, 0x1bfec,
        0x0fff8, 0x0fff8, 0x0fff8, 0x07ff0, 0x07ff0,
        0x07ff0, 0x03fe0, 0x03fe0, 0x07ff0, 0x0fff8,
        0x0fff8, 0x1fffc, 0x3fffe, 0x3fffe, 0x7ffff,
    };
    const int largura = 19, altura = 20;
    bool preenchida = ((xadrez_cor(p) == XZ_PRETAS) != escura);
    int x0 = (casa - largura) / 2, y0 = (casa - altura) / 2;
    for (int j = 0; j < altura; j++) for (int i = 0; i < largura; i++) {
        uint32_t bit = UINT32_C(1) << (largura - 1 - i);
        if (!(mascara[j] & bit)) continue;
        bool borda = j == 0 || j == altura - 1 || i == 0 || i == largura - 1;
        if (!borda && !preenchida) {
            borda = !(mascara[j - 1] & bit) || !(mascara[j + 1] & bit) ||
                    !(mascara[j] & (bit << 1)) || !(mascara[j] & (bit >> 1));
        }
        if (preenchida || borda)
            pixel_peca(bm, x, y, x0 + i, y0 + j, escura, girada, casa);
    }
}

static void desenha_peca(bitmap_t *bm, int x, int y, uint8_t p, bool escura,
                         bool girada, int casa)
{
    if (xadrez_tipo(p) == XZ_DAMA) {
        desenha_dama(bm, x, y, p, escura, girada, casa);
        return;
    }
    const fonte_dados_t *fd = &FONTES[F_XADREZ];
    const glifo_t *g = fonte_glifo(F_XADREZ, codigo_peca(p));
    if (!g) return;
    int x0 = (casa - g->l) / 2;
    int y0 = (casa - g->a) / 2;
    int passo = (g->l + 7) / 8;
    const uint8_t *bits = &fd->bits[g->offset];
    bool preenchida = ((xadrez_cor(p) == XZ_PRETAS) != escura);
    for (int j = 0; j < g->a; j++) for (int i = 0; i < g->l; i++) {
        uint8_t bit = (uint8_t)(0x80u >> (i % 8));
        if (!(bits[j * passo + i / 8] & bit)) continue;
        bool borda = j == 0 || j == g->a - 1 || i == 0 || i == g->l - 1;
        if (!borda && !preenchida) {
            bool cima = bits[(j - 1) * passo + i / 8] & bit;
            bool baixo = bits[(j + 1) * passo + i / 8] & bit;
            bool esq = bits[j * passo + (i - 1) / 8] &
                       (uint8_t)(0x80u >> ((i - 1) % 8));
            bool dir = bits[j * passo + (i + 1) / 8] &
                       (uint8_t)(0x80u >> ((i + 1) % 8));
            borda = !cima || !baixo || !esq || !dir;
        }
        if (preenchida || borda)
            pixel_peca(bm, x, y, x0 + i, y0 + j, escura, girada, casa);
    }
}

static void contorno(bitmap_t *bm, int x, int y, int recuo, bool tinta, int casa)
{
    for (int i = recuo; i < casa - recuo; i++) {
        gfx_pixel(bm, x + i, y + recuo, tinta);
        gfx_pixel(bm, x + i, y + casa - 1 - recuo, tinta);
        gfx_pixel(bm, x + recuo, y + i, tinta);
        gfx_pixel(bm, x + casa - 1 - recuo, y + i, tinta);
    }
}

static void tabuleiro_em(bitmap_t *bm, const vista_xadrez_t *v,
                         int ox, int oy, int tamanho)
{
    for (int r = 0; r < 8; r++) for (int c = 0; c < 8; c++) {
        int casa = v->cor_baixo == XZ_PRETAS ? r * 8 + (7 - c) : (7 - r) * 8 + c;
        int x = ox + c * tamanho, y = oy + r * tamanho;
        bool escura = (r + c) & 1;
        if (escura) gfx_ret(bm, x, y, tamanho, tamanho, true);
        if (v->casa[casa]) {
            bool girada = v->modo == XZ_MODO_LOCAL &&
                          xadrez_cor(v->casa[casa]) != v->cor_baixo;
            desenha_peca(bm, x, y, v->casa[casa], escura, girada, tamanho);
        }
        bool tinta = !escura;
        if (v->ultimo.de != v->ultimo.para &&
            (casa == v->ultimo.de || casa == v->ultimo.para))
            contorno(bm, x, y, 0, tinta, tamanho);
        if (casa == v->origem) { contorno(bm, x, y, 1, tinta, tamanho); contorno(bm, x, y, 4, tinta, tamanho); }
        if (v->destinos & (UINT64_C(1) << casa)) {
            if (v->casa[casa]) contorno(bm, x, y, 4, tinta, tamanho);
            else gfx_ret(bm, x + tamanho / 2 - 3, y + tamanho / 2 - 3, 6, 6, tinta);
        }
        if (casa == v->rei_xeque) {
            gfx_hlin(bm, x + 3, y + 3, 8, 2);
            gfx_vlin(bm, x + 3, y + 3, 8, 2);
        }
        if (casa == v->cursor) contorno(bm, x, y, 2, tinta, tamanho);
    }
    gfx_ret(bm, ox, oy, tamanho * 8, tamanho * 8, false);
}

static void tabuleiro(bitmap_t *bm, const vista_xadrez_t *v)
{
    tabuleiro_em(bm, v, TAB_X, TAB_Y, CASA_V);
}

static bool bit_da_peca(const uint8_t *bits, int passo, int l, int a,
                        int x, int y)
{
    if (x < 0 || y < 0 || x >= l || y >= a) return false;
    return bits[y * passo + x / 8] & (uint8_t)(0x80u >> (x % 8));
}

static void peca_cartaz(bitmap_t *bm, int x, int y, uint8_t p,
                        int escala, bool cheia)
{
    const fonte_dados_t *fd = &FONTES[F_XADREZ];
    const glifo_t *g = fonte_glifo(F_XADREZ, codigo_peca(p));
    if (!g) return;
    int passo = (g->l + 7) / 8;
    const uint8_t *bits = &fd->bits[g->offset];
    for (int j = 0; j < g->a; j++) for (int i = 0; i < g->l; i++) {
        if (!bit_da_peca(bits, passo, g->l, g->a, i, j)) continue;
        bool borda = !bit_da_peca(bits, passo, g->l, g->a, i - 1, j) ||
                     !bit_da_peca(bits, passo, g->l, g->a, i + 1, j) ||
                     !bit_da_peca(bits, passo, g->l, g->a, i, j - 1) ||
                     !bit_da_peca(bits, passo, g->l, g->a, i, j + 1);
        if (cheia || borda)
            gfx_ret(bm, x + i * escala, y + j * escala,
                    escala, escala, true);
    }
}

static void fundo_xadrez(bitmap_t *bm, int x, int y, int l, int a)
{
    const int casa = 26;
    for (int py = 0, r = 0; py < a; py += casa, r++)
        for (int px = 0, c = 0; px < l; px += casa, c++)
            if ((r + c) & 1)
                gfx_ret(bm, x + px, y + py,
                        px + casa <= l ? casa : l - px,
                        py + casa <= a ? casa : a - py, true);
}

static void robo_cartaz(bitmap_t *bm, int x, int y)
{
    gfx_ret(bm, x + 4, y + 7, 44, 35, false);
    gfx_ret(bm, x, y + 15, 5, 18, true);
    gfx_ret(bm, x + 48, y + 15, 5, 18, true);
    gfx_ret(bm, x + 13, y + 18, 7, 7, true);
    gfx_ret(bm, x + 32, y + 18, 7, 7, true);
    gfx_hlin(bm, x + 17, y + 33, 19, 2);
    gfx_vlin(bm, x + 25, y, 7, 2);
    gfx_ret(bm, x + 22, y, 8, 5, true);
}

static void cartaz_xadrez(bitmap_t *bm, const vista_xadrez_t *v)
{
    const int x = 7, y = 31, l = 226, a = 181;
    gfx_ret(bm, x, y, l, a, false);
    gfx_ret(bm, x + 2, y + 2, l - 4, a - 4, false);
    const char *rotulo = v->pagina == XZ_PAG_PREPARAR_MAQUINA
                       ? "CONTRA A MÁQUINA"
                       : v->pagina == XZ_PAG_PREPARAR_LOCAL
                       ? "DUAS PESSOAS" : "XADREZ";
    gfx_texto(bm, x + 7, y + 6, F_MIUDA, rotulo);

    const int qx = x + 6, qy = y + 22, ql = l - 12, qa = a - 29;
    gfx_ret(bm, qx, qy, ql, qa, false);
    gfx_ret(bm, qx + 3, qy + 3, ql - 6, qa - 6, false);
    fundo_xadrez(bm, qx + 4, qy + 4, ql - 8, qa - 8);

    /* O miolo branco faz texto e silhuetas sobreviverem à trama. */
    gfx_limpa_ret(bm, qx + 35, qy + 45, ql - 70, 62);
    if (v->pagina == XZ_PAG_PREPARAR_MAQUINA) {
        robo_cartaz(bm, qx + 48, qy + 53);
        gfx_texto(bm, qx + 108, qy + 65, F_TITULO, "vs");
        peca_cartaz(bm, qx + 142, qy + 50, XZ_REI, 2, false);
    } else if (v->pagina == XZ_PAG_PREPARAR_LOCAL) {
        peca_cartaz(bm, qx + 69, qy + 48, XZ_PEAO, 2, false);
        peca_cartaz(bm, qx + 116, qy + 48, XZ_PEAO, 2, true);
    } else {
        int tw = gfx_largura(F_EDITORIAL, "Xadrez");
        gfx_texto(bm, qx + (ql - tw) / 2, qy + 65, F_EDITORIAL, "Xadrez");
        gfx_limpa_ret(bm, qx + 4, qy + 51, 31, 47);
        gfx_limpa_ret(bm, qx + ql - 35, qy + 51, 31, 47);
        peca_cartaz(bm, qx + 7, qy + 54, XZ_REI, 1, false);
        peca_cartaz(bm, qx + ql - 32, qy + 54, XZ_TORRE, 1, false);
        gfx_limpa_ret(bm, qx + 80, qy + 111, 54, 32);
        peca_cartaz(bm, qx + 82, qy + 112, XZ_PEAO, 1, false);
        peca_cartaz(bm, qx + 108, qy + 112, XZ_PEAO, 1, false);
    }
}

static void linha_cartaz(bitmap_t *bm, int y, const char *titulo,
                         const char *sub, bool foco)
{
    const int altura = 53;
    const int texto_l = bm->l - M * 2 - 41;
    gfx_hlin(bm, M, y, bm->l - M * 2, 1);
    icone_linha(bm, M + 3, y + 8, titulo, true);
    gfx_texto_ate(bm, M + 37, y + 5, F_TITULO, titulo, texto_l);
    gfx_texto_ate(bm, M + 37, y + 27, F_MIUDA, sub, texto_l);
    if (foco) gfx_negativo(bm, M, y + 1, bm->l - M * 2, altura - 1);
}

static void lista_inicio(bitmap_t *bm, const vista_xadrez_t *v)
{
    cabecalho(bm, v, "Tinto");
    cartaz_xadrez(bm, v);
    int y = 217;
    gfx_texto(bm, M, y, F_MIUDA, "OPÇÕES");
    y += 16;
    if (v->pagina == XZ_PAG_INICIO) {
        linha_cartaz(bm, y, "Continuar partida", "Retome de onde parou", v->cursor == 0);
        linha_cartaz(bm, y + 53, "Nova partida", "Comece uma nova mesa", v->cursor == 1);
        linha_cartaz(bm, y + 106, "Opções", "Orientação do tabuleiro", v->cursor == 2);
    } else if (v->pagina == XZ_PAG_MODOS) {
        linha_cartaz(bm, y, "Contra a máquina", "Jogue no seu ritmo", v->cursor == 0);
        linha_cartaz(bm, y + 53, "Duas pessoas", "No mesmo Tinto", v->cursor == 1);
        linha_cartaz(bm, y + 106, "Opções", "Orientação do tabuleiro", v->cursor == 2);
    } else if (v->pagina == XZ_PAG_PREPARAR_MAQUINA) {
        static const char *nivel[] = {"◀ Fácil ▶", "◀ Média ▶", "◀ Difícil ▶"};
        linha_cartaz(bm, y, "Suas peças",
              v->cor_baixo == XZ_PRETAS ? "◀ Pretas ▶" : "◀ Brancas ▶", v->cursor == 0);
        gfx_limpa_ret(bm, M + 3, y + 8, 29, 29);
        desenha_peca(bm, M + 3, y + 8,
                     XZ_PEAO | (v->cor_baixo == XZ_PRETAS ? 8 : 0),
                     false, false, 29);
        linha_cartaz(bm, y + 53, "Dificuldade", nivel[v->dificuldade], v->cursor == 1);
        linha_cartaz(bm, y + 106, "Começar partida", "", v->cursor == 2);
    } else {
        linha_cartaz(bm, y, "Peças embaixo",
              v->cor_baixo == XZ_PRETAS ? "◀ Pretas ▶" : "◀ Brancas ▶", v->cursor == 0);
        gfx_limpa_ret(bm, M + 3, y + 8, 29, 29);
        desenha_peca(bm, M + 3, y + 8,
                     XZ_PEAO | (v->cor_baixo == XZ_PRETAS ? 8 : 0),
                     false, false, 29);
        linha_cartaz(bm, y + 53, "Começar partida", "", v->cursor == 1);
    }
    chrome_rodape(bm,
                  v->pagina == XZ_PAG_INICIO ? "BACK Jogos" :
                  v->pagina == XZ_PAG_MODOS ? "BACK Xadrez" : "BACK modos",
                  v->pagina <= XZ_PAG_MODOS ? "OK escolher" : "◀ ▶ alterar", false);
}

static void opcoes(bitmap_t *bm, const vista_xadrez_t *v)
{
    int y = cabecalho(bm, v, "Xadrez") + 12;
    gfx_texto(bm, M, y, F_MIUDA, "ORIENTAÇÃO DO TABULEIRO");
    y += 25; gfx_texto(bm, M, y, F_EDITORIAL, "Orientação");
    y += 42;
    linha(bm, y, "Vertical", "Padrão · 240 × 416", v->cursor == 0);
    linha(bm, y + 62, "Horizontal", "Partida · 416 × 240", v->cursor == 1);
    chrome_rodape(bm, "BACK voltar", "OK aplicar", false);
}

static void confirma(bitmap_t *bm, const vista_xadrez_t *v)
{
    int y = cabecalho(bm, v, "Xadrez") + 18;
    if (v->pagina == XZ_PAG_ERRO) {
        gfx_texto(bm, M, y, F_EDITORIAL, "Partida danificada");
        gfx_texto_ate(bm, M, y + 42, F_CORPO,
                      "O salvamento não pôde ser lido. O restante do cartão não será alterado.",
                      bm->l - M * 2);
        chrome_rodape(bm, "BACK Jogos", "OK nova partida", false);
        return;
    }
    const bool sair = v->pagina == XZ_PAG_SAIR;
    const bool abandonar = v->pagina == XZ_PAG_ABANDONAR;
    const bool empate = v->pagina == XZ_PAG_EMPATE;
    const char *pergunta = sair ? "Sair da partida?" :
        abandonar ? "Abandonar partida?" : empate ? "Empate proposto" :
        "Substituir partida?";
    gfx_texto(bm, M, y, F_EDITORIAL, pergunta);
    y += 54;
    if (sair) {
        linha(bm, y, "Continuar jogando", "Voltar ao tabuleiro", v->cursor == 0);
        linha(bm, y + 58, "Salvar e sair", "Continue depois", v->cursor == 1);
        linha(bm, y + 116, "Abandonar", "Apaga esta partida", v->cursor == 2);
    } else if (abandonar) {
        gfx_texto_ate(bm, M, y - 18, F_MIUDA,
                      "A partida salva será apagada.", bm->l - M * 2);
        linha(bm, y + 20, "Continuar partida", "Nada será apagado", v->cursor == 0);
        linha(bm, y + 78, "Abandonar", "Encerrar esta partida", v->cursor == 1);
    } else if (empate) {
        linha(bm, y, "Continuar partida", "Recusar proposta", v->cursor == 0);
        linha(bm, y + 58, "Aceitar empate", "Encerrar sem vencedor", v->cursor == 1);
    } else {
        linha(bm, y, "Manter partida", "Nada será apagado", v->cursor == 0);
        char detalhe[40];
        snprintf(detalhe, sizeof detalhe, "%u lances serão apagados", v->n_lances);
        linha(bm, y + 58, "Substituir", detalhe, v->cursor == 1);
    }
    chrome_rodape(bm, "BACK cancelar", "OK escolher", false);
}

static void menu_partida(bitmap_t *bm, const vista_xadrez_t *v)
{
    int n = v->modo == XZ_MODO_LOCAL ? 4 : 3;
    int altura = FAIXA_TOPO + 7 + gfx_altura_linha(F_MIUDA) + 6
               + n * (gfx_altura_linha(F_CORPO) + 7) + 4;
    faixa_t f = faixa_abre(bm, altura, false);
    linha_acao_t itens[] = {
        {.texto = "Histórico",     .icone = ICO_LISTA},
        {.texto = "Girar tela",    .icone = ICO_SINCRONIZA},
        {.texto = "Mostrar ajuda", .icone = ICO_VISTO},
        {.texto = "Propor empate", .icone = ICO_PESSOA},
    };
    snprintf(itens[2].valor, sizeof itens[2].valor, "%s",
             v->mostrar_ajuda ? "Ativada" : "Desativada");
    int y = f.y;
    gfx_texto(bm, f.x, y, F_MIUDA, "Partida");
    y += gfx_altura_linha(F_MIUDA) + 6;
    for (int i = 0; i < n; i++) {
        y = bloco_acao_em(bm, f.x, y, f.util, &itens[i], v->menu_cursor == i);
    }
    faixa_fecha(bm, &f);
}

void tela_xadrez_menu_area(bitmap_t *bm, const vista_xadrez_t *v,
                           int *x, int *y, int *l, int *a)
{
    if (v->orientacao == XZ_VERTICAL) {
        int n = v->modo == XZ_MODO_LOCAL ? 4 : 3;
        int altura = FAIXA_TOPO + 7 + gfx_altura_linha(F_MIUDA) + 6
                   + n * (gfx_altura_linha(F_CORPO) + 7) + 4;
        ret_t r = grid_faixa_de(altura);
        *x = r.x; *y = r.y; *l = r.l; *a = r.a;
        return;
    }
    const int rx = 216, ry = 65, rl = 192, ra = 134;
    if (v->orientacao == XZ_HORIZONTAL_DIREITA) {
        *x = bm->l - ry - ra; *y = rx; *l = ra; *a = rl;
    } else {
        *x = ry; *y = bm->a - rx - rl; *l = ra; *a = rl;
    }
}

static void historico(bitmap_t *bm, const vista_xadrez_t *v)
{
    int y = cabecalho(bm, v, "Histórico") + 10;
    if (v->historico_total) {
        char faixa[24];
        int inicio = v->historico_desloc + 1;
        int fim = inicio + 3;
        if (fim > v->historico_total) fim = v->historico_total;
        snprintf(faixa, sizeof faixa, "%d–%d de %u", inicio, fim, v->historico_total);
        int largura = gfx_largura(F_MIUDA, faixa);
        gfx_texto_ate(bm, M, y, F_MIUDA, "RECENTES PRIMEIRO",
                      bm->l - M * 2 - largura - 8);
        gfx_texto(bm, bm->l - M - largura, y, F_MIUDA, faixa);
    } else {
        gfx_texto(bm, M, y, F_MIUDA, "RECENTES PRIMEIRO");
    }
    y += 27;
    if (!v->historico_total) {
        gfx_texto(bm, M, y + 18, F_EDITORIAL, "Nenhum lance ainda");
        gfx_texto_ate(bm, M, y + 58, F_CORPO,
                      "O histórico aparece depois do primeiro movimento.", bm->l - M * 2);
    } else {
        int n = v->historico_total - v->historico_desloc;
        if (n > 4) n = 4;
        for (int i = 0; i < n; i++) {
            char lance[24], meta[30], descricao[48], acao[48];
            vista_xadrez_descreve_lance(&v->historico[i], lance, sizeof lance,
                                        descricao, sizeof descricao, acao, sizeof acao);
            snprintf(meta, sizeof meta, "%u · %s", v->historico[i].numero,
                     v->historico[i].cor == XZ_BRANCAS ? "Brancas" : "Pretas");
            int iy = y + i * 61;
            gfx_hlin(bm, M, iy, bm->l - M * 2, 1);
            gfx_texto(bm, M, iy + 5, F_MIUDA, meta);
            gfx_texto(bm, bm->l - M - gfx_largura(F_MIUDA, lance), iy + 5,
                      F_MIUDA, lance);
            gfx_texto_ate(bm, M, iy + 23, F_CORPO, descricao, bm->l - M * 2);
            gfx_texto_ate(bm, M, iy + 43, F_MIUDA, acao, bm->l - M * 2);
        }
    }
    chrome_rodape(bm, "BACK partida", "▲ ▼ movimentos", false);
}

static const char *texto_resultado(const vista_xadrez_t *v)
{
    if (v->resultado == XZ_MATE_BRANCAS) return "1–0";
    if (v->resultado == XZ_MATE_PRETAS) return "0–1";
    return "½–½";
}

static void placar_confronto(bitmap_t *bm, const vista_xadrez_t *v, int y)
{
    char pontos[24];
    char a[8], b[8];
    if (v->placar_a2 & 1u) snprintf(a, sizeof a, "%u,5", v->placar_a2 / 2u);
    else snprintf(a, sizeof a, "%u", v->placar_a2 / 2u);
    if (v->placar_b2 & 1u) snprintf(b, sizeof b, "%u,5", v->placar_b2 / 2u);
    else snprintf(b, sizeof b, "%u", v->placar_b2 / 2u);
    snprintf(pontos, sizeof pontos, "%s × %s", a, b);
    int pw = gfx_largura(F_TITULO, pontos);
    gfx_texto(bm, (bm->l - pw) / 2, y + 4, F_TITULO, pontos);

    if (v->modo == XZ_MODO_MAQUINA) {
        desenha_peca(bm, 35, y, XZ_REI | (v->cor_humana ? 8 : 0), false, false, 29);
        robo_mini(bm, bm->l - 64, y);
    } else {
        uint8_t primeira = v->cor_baixo ? 8 : 0;
        uint8_t segunda = primeira ^ 8;
        desenha_peca(bm, 17, y, XZ_REI | primeira, false, false, 29);
        desenha_peca(bm, 39, y + 3, XZ_PEAO | primeira, false, false, 24);
        desenha_peca(bm, bm->l - 65, y, XZ_REI | segunda, false, false, 29);
        desenha_peca(bm, bm->l - 43, y + 3, XZ_PEAO | segunda, false, false, 24);
    }
}

static void resultado(bitmap_t *bm, const vista_xadrez_t *v)
{
    int y = cabecalho(bm, v, "Xadrez") + 10;
    gfx_texto(bm, M, y, F_MIUDA, "RESULTADO DA PARTIDA");
    gfx_texto(bm, M, y + 20, F_ENORME, texto_resultado(v));
    gfx_texto_ate(bm, M, y + 76, F_CORPO, v->contexto, bm->l - M * 2);
    gfx_texto_ate(bm, M, y + 97, F_MIUDA, v->ajuda, bm->l - M * 2);
    gfx_texto(bm, M, y + 116, F_MIUDA, "PLACAR DO CONFRONTO");
    placar_confronto(bm, v, y + 132);
    y += 169;
    linha(bm, y, "Revanche", "Mesmo jogo · troca cores", v->cursor == 0);
    linha(bm, y + 58, "Ver histórico", "Rever os movimentos", v->cursor == 1);
    linha(bm, y + 116, "Encerrar confronto", "Zerar placar e voltar", v->cursor == 2);
    chrome_rodape(bm, "BACK Jogos", "OK escolher", false);
}

static void horizontal(bitmap_t *bm, const vista_xadrez_t *v)
{
    bitmap_t *r = rolagem_rascunho(bm);
    if (!r) { gfx_limpa(bm, false); return; }
    bitmap_t h; bitmap_liga(&h, r->bits, bm->a, bm->l); gfx_limpa(&h, false);
    tabuleiro_em(&h, v, 8, 20, 25);
    gfx_texto(&h, 224, 20, F_MIUDA, "XADREZ");
    gfx_texto_ate(&h, 224, 47, F_TITULO,
                  v->pagina == XZ_PAG_FINAL ? "Xeque-mate" : v->subtitulo, 176);
    gfx_hlin(&h, 224, 78, 176, 1);
    gfx_texto_ate(&h, 224, 92, F_CORPO, v->contexto, 176);
    gfx_texto_ate(&h, 224, 129, F_MIUDA, v->ajuda, 176);
    if (v->pagina == XZ_PAG_MENU) {
        const char *itens[] = {"Histórico", "Girar tela",
                               v->mostrar_ajuda ? "Ajuda · ativada" : "Ajuda · desativada",
                               "Propor empate"};
        int n = v->modo == XZ_MODO_LOCAL ? 4 : 3;
        gfx_limpa_ret(&h, 216, 65, 192, 134);
        gfx_ret(&h, 216, 65, 192, 134, false);
        for (int i = 0; i < n; i++) {
            int iy = 74 + i * 29;
            gfx_texto(&h, 230, iy, F_CORPO, itens[i]);
            if (v->menu_cursor == i) gfx_negativo(&h, 222, iy - 3, 178, 26);
        }
    } else if (v->pagina == XZ_PAG_PROMOCAO) {
        static const uint8_t pecas[] = {XZ_DAMA, XZ_TORRE, XZ_BISPO, XZ_CAVALO};
        gfx_limpa_ret(&h, 216, 75, 192, 124);
        gfx_texto(&h, 224, 82, F_TITULO, "Promover peão");
        for (int i = 0; i < 4; i++) {
            int px = 224 + i * 43;
            gfx_ret(&h, px, 116, 37, 37, pecas[i] == v->promocao);
            desenha_peca(&h, px + 4, 120,
                         pecas[i] | (v->turno == XZ_PRETAS ? 8 : 0),
                         pecas[i] == v->promocao, false, 29);
        }
    }
    gfx_texto(&h, 224, 207, F_MIUDA,
              v->pagina == XZ_PAG_FINAL ? "OK resultado" : "BACK sair · MENU ações");
    if (v->orientacao == XZ_HORIZONTAL_DIREITA) {
        gfx_gira_horario(bm, &h);
    } else {
        gfx_limpa(bm, false);
        for (int y = 0; y < h.a; y++) for (int x = 0; x < h.l; x++)
            if (gfx_le(&h, x, y)) gfx_pixel(bm, y, h.l - 1 - x, true);
    }
}

static void promocao(bitmap_t *bm, const vista_xadrez_t *v)
{
    static const uint8_t pecas[] = { XZ_DAMA, XZ_TORRE, XZ_BISPO, XZ_CAVALO };
    for (int i = 0; i < 4; i++) {
        int x = M + i * 56;
        gfx_ret(bm, x, 326, 50, 50, pecas[i] == v->promocao);
        desenha_peca(bm, x + 10, 336,
                     pecas[i] | (v->turno == XZ_PRETAS ? 8 : 0),
                     pecas[i] == v->promocao,
                     v->modo == XZ_MODO_LOCAL && v->turno != v->cor_baixo,
                     CASA_V);
    }
}

void tela_xadrez(bitmap_t *bm, const vista_xadrez_t *v)
{
    if (v->pagina <= XZ_PAG_PREPARAR_LOCAL) { lista_inicio(bm, v); return; }
    if (v->pagina == XZ_PAG_OPCOES) { opcoes(bm, v); return; }
    if (v->pagina == XZ_PAG_HISTORICO) { historico(bm, v); return; }
    if (v->pagina == XZ_PAG_RESULTADO) { resultado(bm, v); return; }
    if (v->pagina == XZ_PAG_SUBSTITUIR || v->pagina == XZ_PAG_SAIR ||
        v->pagina == XZ_PAG_ABANDONAR || v->pagina == XZ_PAG_EMPATE ||
        v->pagina == XZ_PAG_ERRO) { confirma(bm, v); return; }
    if (v->orientacao != XZ_VERTICAL &&
        (v->pagina == XZ_PAG_TABULEIRO || v->pagina == XZ_PAG_PROMOCAO ||
         v->pagina == XZ_PAG_MENU || v->pagina == XZ_PAG_FINAL)) {
        horizontal(bm, v); return;
    }
    int y = cabecalho(bm, v, "Xadrez") + 6;
    gfx_texto(bm, M, y, F_TITULO, v->subtitulo);
    tabuleiro(bm, v);
    y = TAB_Y + CASA_V * 8 + 9;
    gfx_hlin(bm, M, y, bm->l - M * 2, 1);
    if (v->pagina == XZ_PAG_PROMOCAO) {
        promocao(bm, v);
    } else if (v->pagina != XZ_PAG_MENU) {
        gfx_texto(bm, M, y + 8, F_TITULO,
            v->pagina == XZ_PAG_FINAL ? "Xeque-mate" : v->contexto);
        gfx_texto(bm, M, y + 32, F_MIUDA,
            v->salvamento_falhou ? "Partida ainda não salva" : v->ajuda);
    }
    if (v->pagina == XZ_PAG_MENU) menu_partida(bm, v);
    chrome_rodape(bm,
                  v->pagina == XZ_PAG_FINAL ? "" :
                  v->pagina == XZ_PAG_MENU ? "BACK fechar" : "BACK sair",
                  v->pagina == XZ_PAG_PROMOCAO || v->pagina == XZ_PAG_MENU ? "OK escolher" :
                  v->pagina == XZ_PAG_FINAL ? "OK resultado" : "MENU ações", false);
}
