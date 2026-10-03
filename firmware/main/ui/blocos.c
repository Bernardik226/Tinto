#include "blocos.h"
#include <stdio.h>
#include "chrome.h"
#include "grid.h"
#include "../tela/icones.h"
#include <string.h>

#define MARGEM GRID_MARGEM

// ── o dia como linha do tempo ───────────────────────────────────────
// A pergunta da tela do dia é de FORMA (como o dia se distribui), e a régua
// responde. Cada bloco desenha o seu pedaço da linha contínua, sem emenda. A
// marca do evento é um quadrado cheio na primeira linha de texto: cheio
// contra vazado é o que sobrevive de longe em 1 bit.
int bloco_timeline(bitmap_t *bm, int y, const linha_captura_t *l,
                   bool cursor, bool primeiro, bool ultimo, int col_h)
{
    // A calha da hora vem medida da página ("07:00" em 24 h, "11:30 am" em
    // 12 h): o pior caso custaria trinta pixels do título.
    const int REGUA = MARGEM + col_h + 8;
    const int TEXTO = REGUA + 10;

    int alto = gfx_altura_linha(F_CORPO) + 6;
    if (l->sub[0]) alto += gfx_altura_linha(F_MIUDA);

    if (cursor) chrome_cursor(bm, y - 2, alto);

    // A hora encostada na régua pela DIREITA: assim forma coluna.
    int wh = gfx_largura(F_MIUDA, l->hora);
    gfx_texto(bm, REGUA - 6 - wh, y + 2, F_MIUDA, l->hora);

    // O primeiro pedaço começa na marca e o último termina nela.
    const int marca_y = y + 3;
    int de  = primeiro ? marca_y : y - 2;
    int ate = ultimo   ? marca_y + 5 : y + alto;
    if (ate > de) gfx_vlin(bm, REGUA, de, ate - de, 1);

    gfx_ret(bm, REGUA - 2, marca_y, 5, 5, true);

    // A CAIXA do híbrido: sem ela a linha seria um compromisso, e o OK faria o
    // que a tela não anunciou. Evento não tem caixa (RN-2B).
    int texto_x = TEXTO;
    if (l->caixa)
        texto_x = chrome_caixa(bm, TEXTO, y, l->feita);

    const int UTIL = bm->l - MARGEM - texto_x;
    gfx_texto_ate(bm, texto_x, y, F_CORPO, l->titulo, UTIL);

    // Feita fica RISCADA onde está (RN-35).
    if (l->feita) {
        // Até onde o TEXTO foi (`gfx_texto_ate` trunca).
        int w = gfx_largura(F_CORPO, l->titulo);
        chrome_risco(bm, texto_x, y, w < UTIL ? w : UTIL, F_CORPO);
    }

    if (l->sub[0])
        gfx_texto_ate(bm, texto_x, y + gfx_altura_linha(F_CORPO) + 1, F_MIUDA,
                      l->sub, UTIL);

    return y + alto;
}

// Ícone, texto à esquerda e valor à direita, numa linha. Sem teto para o
// rótulo os dois ocupam o mesmo pixel (ilegível em 1 bit). RN-B7: quem
// trunca é o rótulo; o valor é o que se veio buscar.
static int util_do_texto(const linha_acao_t *a, int larg)
{
    int usado = a->icone != ICO_NENHUM ? ICONES[a->icone].l + 8 : 0;
    if (a->icone != ICO_NENHUM && a->icone2 != ICO_NENHUM)
        usado += ICONES[a->icone2].l + 4;
    int valor = a->valor[0] ? gfx_largura(F_MIUDA, a->valor) + 10 : 0;
    if (a->marcado) valor += 12;
    int util  = larg - usado - valor;
    return util < 0 ? 0 : util;
}

int bloco_acao_largura_do_texto(const linha_acao_t *a, int larg)
{
    int usado = a->icone != ICO_NENHUM ? ICONES[a->icone].l + 8 : 0;
    int w = gfx_largura(F_CORPO, a->texto);
    int util = util_do_texto(a, larg);
    return usado + (w < util ? w : util);
}

int bloco_acao_em(bitmap_t *bm, int x0, int y, int larg,
                  const linha_acao_t *a, bool cursor)
{
    int alto = gfx_altura_linha(F_CORPO) + 7;

    if (cursor) chrome_cursor_em(bm, x0 - 4, y - 2, larg + 8, alto);

    int x = x0;
    if (a->icone != ICO_NENHUM) {
        gfx_icone(bm, x, y + 1, a->icone);
        x += ICONES[a->icone].l + 8;

        // O segundo ícone cola no primeiro: sinal + cadeado, uma informação.
        if (a->icone2 != ICO_NENHUM) {
            gfx_icone(bm, x - 4, y + 1, a->icone2);
            x += ICONES[a->icone2].l + 4;
        }
    }

    char texto[sizeof a->texto + 4];
    snprintf(texto, sizeof texto, "%s", a->texto);
    int util = util_do_texto(a, larg);
    if (gfx_largura(F_CORPO, texto) > util) {
        int n = gfx_cabe(F_CORPO, texto, util);
        if (n > 1) n -= 1;
        texto[n] = '\0';
        snprintf(texto + n, sizeof texto - (size_t)n, "…");
    }
    gfx_texto(bm, x, y, F_CORPO, texto);

    // O valor em MIÚDA: é o estado, não o comando.
    if (a->valor[0]) {
        int w = gfx_largura(F_MIUDA, a->valor);
        gfx_texto(bm, x0 + larg - w, y + 3, F_MIUDA, a->valor);
    }
    if (a->marcado) {
        int cx = x0 + larg - 4, cy = y + alto / 2 - 1;
        for (int dy = -3; dy <= 3; dy++)
            for (int dx = -3; dx <= 3; dx++)
                if (dx * dx + dy * dy <= 9) gfx_pixel(bm, cx + dx, cy + dy, true);
    }
    return y + alto;
}

int bloco_acao(bitmap_t *bm, int y, const linha_acao_t *a, bool cursor)
{
    return bloco_acao_em(bm, MARGEM, y, bm->l - MARGEM * 2, a, cursor);
}

// ── o DESTINO: uma linha que leva a outra tela ──────────────────────
// Ícone, título, descrição e seta; focada, inverte inteira. Margem 10, ícone
// 22, 48 px de linha, filete embaixo. A altura CRESCE se o título quebra.
// `serif` na raiz de Ajustes; corpo do sistema nas listas.
int bloco_destino(bitmap_t *bm, int y, int larg, icone_id ico,
                  const char *titulo, const char *sub, const char *valor,
                  bool foco, bool serif)
{
    const int X    = (bm->l - larg) / 2;
    // As medidas moram em `grid.h`, para todas as telas usarem as mesmas.
    const int PAD  = GRID_DESTINO_PAD;
    const int COL  = GRID_DESTINO_COL;
    const int SETA = gfx_largura(F_CORPO_P, ">") + 3;

    fonte_t ft = serif ? F_EDITORIAL : F_CORPO;

    // O VALOR à direita, antes da seta ("18 min", "3 de 5"): conferir sem
    // entrar.
    int wv  = (valor && valor[0]) ? gfx_largura(F_MIUDA, valor) + 6 : 0;
    int util = larg - COL - SETA - wv - PAD;

    int topo = y;
    y += PAD;

    int ty = gfx_paragrafo(bm, X + COL, y, util, 2, ft, titulo);
    if (sub && sub[0]) {
        // A descrição usa a largura CHEIA: o valor mora na linha do título.
        gfx_texto_ate(bm, X + COL, ty + 1, F_MIUDA, sub,
                      larg - COL - SETA - PAD);
        ty += gfx_altura_linha(F_MIUDA) + 1;
    }
    y = ty + PAD;

    // 48 é o mínimo: alturas diferentes fazem o olho procurar a próxima.
    if (y - topo < GRID_DESTINO_A) y = topo + GRID_DESTINO_A;

    // Ícone e seta CENTRADOS na linha inteira (com título de duas linhas).
    int meio = topo + (y - topo) / 2;
    if (ico != ICO_NENHUM)
        gfx_icone(bm, X + 2, meio - ICONES[ico].a / 2, ico);
    gfx_texto(bm, X + larg - SETA + 2, meio - gfx_altura_linha(F_CORPO_P) / 2,
              F_CORPO_P, ">");
    if (wv)
        gfx_texto(bm, X + larg - SETA - wv + 6,
                  meio - gfx_altura_linha(F_MIUDA) / 2, F_MIUDA, valor);

    if (foco) gfx_negativo(bm, X, topo, larg, y - topo);

    // O filete embaixo, sempre: separa as linhas fora de foco.
    gfx_hlin(bm, X, y, larg, 1);
    return y + 1;
}
