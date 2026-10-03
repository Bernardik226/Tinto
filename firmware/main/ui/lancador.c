// ui/lancador.c — a Home 2x2. Compõe, não decide.
#include "lancador.h"
#include "chrome.h"
#include "grid.h"
#include "../tela/icones.h"
#include "../tela/icones.h"

// ── o cabeçalho, entre a barra e a grade ─────────────────────────────
// A marca e a saudação fazem o aparelho parecer objeto, não menu.
#define CABECA_Y      35

// Conteúdo alinhado à ESQUERDA: centrado, as legendas de tamanhos
// diferentes dançariam, e a grade perderia a coluna de leitura.
#define ICONE_PX      44
#define ICONE_VAO     18   // do ícone até o título

// O relevo: duas barras sólidas para a direita e para baixo, FORA do
// cartão (a grade reserva 4 px). Em 1 bit não há sombra em degradê.
#define RELEVO         3
#define RELEVO_OFF     1   // onde a barra começa, depois da parede

void tela_lancador_cartao(int i, int *x, int *y, int *l, int *a)
{
    *l = LANCADOR_CARTAO_L;
    *a = LANCADOR_CARTAO_A;
    *x = GRID_MARGEM + (i % 2) * (LANCADOR_CARTAO_L + LANCADOR_VAO_X);
    *y = LANCADOR_GRADE_Y + (i / 2) * (LANCADOR_CARTAO_A + LANCADOR_VAO_Y);
}

// Um cartão: tinta sobre papel, e o foco inverte no fim (em 1 bit, branco
// é ausência de tinta: não se desenha, se inverte).
static void cartao(bitmap_t *bm, int i, const cartao_lancador_t *c, bool foco)
{
    int x, y, l, a;
    tela_lancador_cartao(i, &x, &y, &l, &a);

    if (!foco) {
        // O relevo só no cartão em PAPEL: no foco seria preto sobre preto.
        gfx_ret(bm, x + RELEVO,     y + a + RELEVO_OFF, l + RELEVO_OFF, RELEVO, true);
        gfx_ret(bm, x + l + RELEVO_OFF, y + RELEVO,     RELEVO, a + RELEVO_OFF, true);

        // A parede não vai no cartão em foco: invertida, viraria um fio branco
        // dentro do bloco.
        gfx_ret(bm, x, y, l, a, false);
    }

    const int cx = x + LANCADOR_PAD_X;
    int cy = y + LANCADOR_PAD_Y;

    gfx_icone(bm, cx, cy, c->icone);
    cy += ICONE_PX + ICONE_VAO;

    gfx_texto(bm, cx, cy, F_TITULO, c->titulo);
    cy += gfx_altura_linha(F_TITULO) + 3;

    gfx_texto(bm, cx, cy, F_MIUDA, c->legenda);

    if (foco) gfx_negativo(bm, x, y, l, a);
}

void tela_lancador(bitmap_t *bm, const vista_lancador_t *v)
{
    // A tela limpa o próprio papel: o app pode montar o mesmo quadro duas
    // vezes no mesmo bitmap (sem e com seletor, EINK §5.5), e o
    // `gfx_negativo` ALTERNA — o foco piscava.
    gfx_limpa(bm, false);

    barra_t b = {
        .titulo       = v->data_curta,
        .hora         = v->hora,
        .bateria      = v->bateria,
        .wifi         = v->wifi,
        .carregando   = false,
        .sinc = v->sinc,
    };
    chrome_barra(bm, &b);

    // Sem filete entre a barra e o conteúdo: o espaço já separa.
    // ── a marca, e ao lado quem é o dono ─────────────────────────────────
    // O logo amarra as duas linhas do cabeçalho.
    int y  = CABECA_Y;
    int lx = GRID_MARGEM;
    int lw = ICONES[ICO_TINTO].l;
    int la = ICONES[ICO_TINTO].a;
    int alt_texto = gfx_altura_linha(F_MIUDA) + gfx_altura_linha(F_EDITORIAL);

    gfx_icone(bm, lx, y + (alt_texto - la) / 2, ICO_TINTO);

    int tx = lx + lw + 6;
    // Sai a moldura antes do nome; só sem caber nem o nome ele é cortado.
    int util = bm->l - GRID_MARGEM - tx;
    const char *quem = gfx_largura(F_MIUDA, v->sobre) <= util ? v->sobre
                     : v->dono[0]                             ? v->dono
                                                              : v->sobre;
    gfx_texto_ate(bm, tx, y, F_MIUDA, quem, util);

    // Um pixel a menos entre as duas linhas: são uma coisa só.
    y += gfx_altura_linha(F_MIUDA) - 1;

    // A serifa só aqui nesta tela: uma frase editorial e o resto em sans.
    gfx_texto(bm, tx, y, F_EDITORIAL, v->saudacao);

    for (int i = 0; i < LANCADOR_CARTOES; i++)
        cartao(bm, i, &v->cartoes[i], i == v->foco);

    chrome_rodape(bm, v->rodape_esq, v->rodape_dir, false);
}
