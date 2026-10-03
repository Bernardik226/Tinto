// ui/agenda.c — a Agenda no vidro.
// Só composição: a tela pergunta "este campo existe?", nunca "o que este
// dado significa?". De cima para baixo:
//     AGENDA    o que acontece no dia — os compromissos, na régua
//     TRABALHO  o que falta fazer, agrupado pela lista
#include "agenda.h"
#include "agua.h"
#include "chrome.h"
#include "grid.h"
#include "rolagem.h"
#include "../tela/icones.h"
#include <stdio.h>
#include <string.h>

#define MARGEM GRID_MARGEM

// ── os compromissos, em cards ───────────────────────────────────────
// Todo evento é o mesmo card, e o destaque é do GESTO (o foco), não do dado.
// O `proximo` da vista diz quem leva a contagem "em 2h43".

// ── o cabeçalho de dia ───────────────────────────────────────────────
// 54 px: dia da semana em miúda caixa alta, a data por extenso em SERIFA e
// as setas à direita; o filete fecha. O único lugar da Agenda com serifa.
#define CABECA_A 54

static int cabeca_do_dia(bitmap_t *bm, int y, const vista_agenda_t *v)
{
    const int base = y;

    gfx_texto(bm, GRID_MARGEM, y + 8, F_MIUDA, v->dia_semana);
    gfx_texto(bm, GRID_MARGEM, y + 8 + gfx_altura_linha(F_MIUDA),
              F_EDITORIAL, v->dia_longo);

    // As setas vão na linha do DIA DA SEMANA: com a data (serifa 17) somavam
    // 233 px numa faixa de 220. No pior caso ali ("SEGUNDA-FEIRA" + "◀ amanhã ▶")
    // são 180.
    if (v->nav_dia[0]) {
        int nw = gfx_largura(F_MIUDA, v->nav_dia);
        int nx = bm->l - GRID_MARGEM - nw;
        gfx_texto(bm, nx, y + 8, F_MIUDA, v->nav_dia);

        // Moldura, não inversão, quando o direcional é do dia: um retângulo preto
        // no cabeçalho puxaria o olho para longe do conteúdo.
        if (v->nav_focado)
            gfx_ret(bm, nx - 5, y + 4, nw + 10,
                    gfx_altura_linha(F_MIUDA) + 7, false);
    }

    gfx_hlin(bm, GRID_MARGEM, base + CABECA_A - 1,
             bm->l - GRID_MARGEM * 2, 1);
    return base + CABECA_A;
}

// ── o card de evento ─────────────────────────────────────────────────
// Margem 10, coluna de hora, vão 6, filete de 3, vão 6. O foco inverte o
// BLOCO DE CONTEÚDO, não a coluna da hora, que fica legível.
// A coluna da hora é MEDIDA (a hora mais larga visível, piso de 36): em
// 12 h, "9:00 am" não cabia nos 36 fixos.
#define EV_HORA_MIN 36
#define EV_VAO       6
#define EV_FILETE    3

static int cartao_evento(bitmap_t *bm, int y, const linha_agenda_t *l,
                         bool foco, int col_h)
{
    const int xf   = MARGEM + col_h + EV_VAO;       // o filete
    const int xc   = xf + EV_FILETE + EV_VAO;       // o corpo
    const int larg = bm->l - MARGEM - xc;

    // A altura sai do CONTEÚDO (local e contagem acrescentam linhas).
    int alt = 2 + gfx_altura_linha(F_CORPO);
    if (l->onde[0])   alt += gfx_altura_linha(F_MIUDA);
    if (l->quando[0]) alt += gfx_altura_linha(F_MIUDA);
    alt += 6;

    // A hora sobe 3 px para começar junto com a primeira linha de corpo.
    gfx_texto(bm, MARGEM, y + 3, F_MIUDA, l->hora);

    // O filete vertical é o que faz o card ser card em 1 bit.
    gfx_ret(bm, xf, y, EV_FILETE, alt, true);

    int cy = y + 2;

    // A CAIXA do híbrido: anuncia que a linha se marca. Evento não tem caixa
    // (RN-2B): "feito" não existe no Calendar.
    int tx = xc;
    if (l->caixa) tx = chrome_caixa(bm, xc, cy, l->feita);

    const int cabe = bm->l - MARGEM - tx;
    gfx_texto_ate(bm, tx, cy, F_CORPO, l->oque, cabe);

    // Riscada ONDE ESTÁ (RN-35). O risco vai até onde o TEXTO parou
    // (`gfx_texto_ate` trunca), não até o fim do título inteiro.
    if (l->feita) {
        int w = gfx_largura(F_CORPO, l->oque);
        chrome_risco(bm, tx, cy, w < cabe ? w : cabe, F_CORPO);
    }

    cy += gfx_altura_linha(F_CORPO);

    if (l->onde[0]) {
        gfx_texto_ate(bm, xc, cy, F_MIUDA, l->onde, larg);
        cy += gfx_altura_linha(F_MIUDA);
    }
    if (l->quando[0]) gfx_texto_ate(bm, xc, cy, F_MIUDA, l->quando, larg);

    if (foco) gfx_negativo(bm, xc - 3, y, bm->l - MARGEM - xc + 3, alt);

    return y + alt + 4;
}

// A coluna da hora sai da hora mais larga DESTE dia.
static int coluna_da_hora(const vista_agenda_t *v)
{
    int col_h = EV_HORA_MIN;
    for (int i = 0; i < v->n_agenda; i++) {
        int w = gfx_largura(F_MIUDA, v->agenda[i].hora);
        if (w > col_h) col_h = w;
    }
    return col_h;
}

static int altura_do_cartao(const linha_agenda_t *l)
{
    int alt = 2 + gfx_altura_linha(F_CORPO) + 6 + 4;
    if (l->onde[0])   alt += gfx_altura_linha(F_MIUDA);
    if (l->quando[0]) alt += gfx_altura_linha(F_MIUDA);
    return alt;
}

static int rotulo_por_fazer(bitmap_t *bm, int y, const vista_agenda_t *v)
{
    // Em NEGRITO, rótulo e número: não há miúda bold, então o texto é
    // desenhado duas vezes, um pixel ao lado.
    char n[8];
    snprintf(n, sizeof n, "%d", v->n_abertas);
    int xn = bm->l - MARGEM - gfx_largura(F_MIUDA, n) - 1;
    for (int d = 0; d < 2; d++) {
        gfx_texto(bm, MARGEM + d, y, F_MIUDA, "POR FAZER");
        gfx_texto(bm, xn + d, y, F_MIUDA, n);
    }

    // O aviso de estouro fica no rótulo: é sobre a lista inteira.
    if (v->aviso[0]) {
        int w = gfx_largura(F_MIUDA, v->aviso);
        gfx_texto(bm, bm->l - MARGEM - gfx_largura(F_MIUDA, n) - w - 10, y,
                  F_MIUDA, v->aviso);
    }
    return y + gfx_altura_linha(F_MIUDA) + 6;
}

// O cabeçalho da lista: o NOME no Google, miúdo em caixa alta, e à direita
// quanto ela tem ("3 de 12").
static int cabecalho_do_grupo(bitmap_t *bm, int y, const grupo_trabalho_t *g)
{
    char t[sizeof g->titulo + 4];
    snprintf(t, sizeof t, "%s", g->titulo);
    for (char *c = t; *c; c++)
        if (*c >= 'a' && *c <= 'z') *c -= 32;

    int wc = g->contagem[0] ? gfx_largura(F_MIUDA, g->contagem) : 0;
    gfx_texto_ate(bm, MARGEM, y, F_MIUDA, t,
                  bm->l - MARGEM * 2 - (wc ? wc + 10 : 0));
    if (wc) gfx_texto(bm, bm->l - MARGEM - wc, y, F_MIUDA, g->contagem);
    return y + gfx_altura_linha(F_MIUDA) + 4;
}

// ── uma PÁGINA da Agenda ─────────────────────────────────────────────
// Compromissos e depois tarefas são unidades; a página é o que cabe entre o
// cabeçalho e o rodapé. Desenha a partir de `de` e devolve a primeira que
// não coube: a mesma função pinta e mede. Toda página leva ao menos uma.
static int pagina(bitmap_t *bm, int y, const vista_agenda_t *v, int de,
                  int fundo, bool cursor)
{
    const int n     = v->n_agenda + v->n_trabalho;
    const int col_h = coluna_da_hora(v);
    int u = de;

    if (u < v->n_agenda) y += 7;
    for (; u < v->n_agenda; u++) {
        if (u > de && y + altura_do_cartao(&v->agenda[u]) > fundo) return u;
        y = cartao_evento(bm, y, &v->agenda[u],
                          cursor && u == v->cursor_agenda, col_h);
    }
    if (u >= n) return n;

    const int alt  = gfx_altura_linha(F_CORPO_P);
    const int miu  = gfx_altura_linha(F_MIUDA);
    const int xcx  = MARGEM + ICONES[ICO_CAIXA].l + 4;   // depois da caixa

    // O filete separa agenda e tarefas quando os dois estão na página; o
    // rótulo vem em toda página com tarefa.
    int antes = (u > de ? 4 + 9 : 0) + miu + 6;
    if (u > de && y + antes + alt > fundo) return u;
    if (u > de) y = chrome_filete(bm, y + 4, FILETE_FINO) + 8;
    y = rotulo_por_fazer(bm, y, v);

    int grupo_aberto = -1;
    for (; u < n; u++) {
        int i = u - v->n_agenda;
        const linha_trabalho_t *l = &v->trabalho[i];

        // O nome da lista quando ela muda, e no topo de toda página: a tarefa
        // nunca aparece sem dizer de que lista é.
        bool cabecalho = l->grupo != grupo_aberto && l->grupo >= 0 &&
                         l->grupo < v->n_grupos &&
                         v->grupos[l->grupo].titulo[0];

        int ws   = l->selo[0] ? gfx_largura(F_MIUDA, l->selo) + 14 : 0;
        int util = bm->l - MARGEM - ws - xcx;
        int linhas = gfx_cabe(F_CORPO_P, l->titulo, util) <
                     (int)strlen(l->titulo) ? 2 : 1;
        int precisa = (cabecalho ? (grupo_aberto >= 0 ? 6 : 0) + miu + 4 : 0)
                    + alt * linhas;
        if (u > de && y + precisa > fundo) return u;

        if (cabecalho) {
            if (grupo_aberto >= 0) y += 6;
            grupo_aberto = l->grupo;
            y = cabecalho_do_grupo(bm, y, &v->grupos[grupo_aberto]);
        }

        // A tarefa em até duas linhas: uma cortava o que fazer.
        int x   = chrome_caixa(bm, MARGEM, y, l->feita);
        int fim = gfx_paragrafo(bm, x, y, util, 2, F_CORPO_P, l->titulo);
        int altura = fim - y < alt ? alt : fim - y;

        if (l->de_fora)              chrome_origem(bm, y, altura);
        if (cursor && i == v->cursor_trabalho) chrome_cursor(bm, y, altura);
        if (l->feita) chrome_risco(bm, x, y, util, F_CORPO_P);
        if (l->selo[0])
            chrome_selo(bm, bm->l - MARGEM - ws + 8, y + 2, l->selo,
                        l->selo_negativo);
        y += altura + 4;
    }
    return n;
}

static int fundo_da_pagina(const bitmap_t *bm, const vista_agenda_t *v)
{
    int fundo = bm->a - RODAPE_A - 8;
    if (v->sem_rede) fundo -= gfx_altura_linha(F_MIUDA) + 6;
    return fundo;
}

// ── o dia vazio ──────────────────────────────────────────────────────
// A marca d'água no meio do corpo e o convite no pé, cada um mais fraco.
// Painel em branco parece quadro que falhou.
static void zona_vazia(bitmap_t *bm, int y, const vista_agenda_t *v)
{
    if (!v->vazio[0]) return;

    // O corpo é o que sobra até o rodapé; a marca d'água fica no MEIO dele.
    int fundo = bm->a - RODAPE_A - 10;
    int alt   = fundo - y;
    if (alt < gfx_altura_linha(F_CORPO)) return;

    // Reserva o que vem depois ANTES de centrar, senão cai sobre o rodapé.
    int abaixo = 0;
    if (v->convite[0])
        abaixo += gfx_altura_linha(F_MIUDA) * 2 + 12;

    int ay = ui_agua(bm, MARGEM, y, bm->l - MARGEM * 2, alt - abaixo,
                     v->vazio) + 14;

    // ── o convite, só nos primeiros dias ─────────────────────────────────
    // Em tinta cheia e miúda (esmaecido, perdia metade da haste). A vista decide
    // quando some.
    if (v->convite[0])
        (void)gfx_paragrafo(bm, MARGEM + 8, ay, bm->l - MARGEM * 2 - 16,
                            3, F_MIUDA, v->convite);
}

// ── as páginas, medidas DESENHANDO ───────────────────────────────────
// Num bitmap invisível, com a mesma `pagina()`: a soma não discorda do
// desenho se ela É o desenho. `inicio[k]` é a primeira unidade da página k+1.
#define AGENDA_PAGINAS_MAX (AGENDA_MAX_COMPROMISSOS + AGENDA_MAX_TRABALHO)

static int fronteiras(bitmap_t *bm, const vista_agenda_t *v, int topo,
                      int *inicio)
{
    const int n = v->n_agenda + v->n_trabalho;
    bitmap_t *rasc = rolagem_rascunho(bm);
    if (!rasc) { inicio[0] = 0; return 1; }
    bitmap_t medida = *rasc;

    int paginas = 0, de = 0;
    do {
        inicio[paginas++] = de;
        de = pagina(&medida, topo, v, de, fundo_da_pagina(bm, v), false);
    } while (de < n && paginas < AGENDA_PAGINAS_MAX);
    return paginas;
}

void tela_agenda(bitmap_t *bm, const vista_agenda_t *v)
{
    gfx_limpa(bm, false);

    barra_t b = { .titulo = v->titulo, .hora = v->hora,
                  .bateria = v->bateria, .wifi = v->wifi , .sinc = v->sinc };
    int y = chrome_barra(bm, &b);
    y = cabeca_do_dia(bm, y, v);

    // A página é a da linha em FOCO, que sobrevive ao quadro sem seletor.
    int inicio[AGENDA_PAGINAS_MAX];
    int paginas = fronteiras(bm, v, y, inicio);
    int pag = 0;
    while (pag + 1 < paginas && inicio[pag + 1] <= v->foco) pag++;

    if (v->n_agenda + v->n_trabalho)
        y = pagina(bm, y, v, inicio[pag], fundo_da_pagina(bm, v), true);
    zona_vazia(bm, y + 6, v);

    // ── SEM REDE, colado no rodapé ──
    // Estado do aparelho mora no rodapé.
    if (v->sem_rede) {
        int alt = gfx_altura_linha(F_MIUDA);
        int ay  = bm->a - RODAPE_A - alt - 4;
        gfx_icone(bm, MARGEM, ay - 1, ICO_SEM_REDE);
        gfx_texto(bm, MARGEM + ICONES[ICO_SEM_REDE].l + 6, ay, F_MIUDA,
                  "SEM REDE");
    }

    if (paginas > 1) chrome_rodape_pagina(pag + 1, paginas);
    chrome_rodape(bm, v->rodape_esq, v->rodape_dir, false);
}
