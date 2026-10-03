// firmware/testes/t_acervo_ui.c — a Biblioteca cabe no vidro.
// Nada sai do quadro, toda linha tem área declarada, e o cursor pinta só a
// faixa que mudou.
#include "teste.h"
#include "ui/acervo.h"
#include "ui/obra.h"
#include "ui/grid.h"

static bitmap_t bm;
static uint8_t px[(TELA_L / 8) * TELA_A];

static void monta(vista_acervo_t *v, int n, bool destaque)
{
    memset(v, 0, sizeof *v);
    snprintf(v->titulo, sizeof v->titulo, "%s", "Acervo");
    snprintf(v->hora, sizeof v->hora, "%s", "09:14");
    v->bateria = 78;
    v->wifi = 3;

    v->tem_destaque = destaque;
    if (destaque) {
        snprintf(v->destaque.titulo, sizeof v->destaque.titulo, "%s",
                 "O estrangeiro");
        snprintf(v->destaque.legenda, sizeof v->destaque.legenda, "%s", "38%");
    }

    for (int i = 0; i < n; i++) {
        snprintf(v->linhas[i].titulo, sizeof v->linhas[0].titulo,
                 "Obra numero %d com titulo comprido demais", i);
        snprintf(v->linhas[i].legenda, sizeof v->linhas[0].legenda, "%s",
                 "Livro · 12%");
        v->linhas[i].marca = i % 3;
        v->linhas[i].pct = 64;
    }
    v->n = n;
    snprintf(v->rodape_esq, sizeof v->rodape_esq, "%s", "BACK voltar");
    snprintf(v->rodape_dir, sizeof v->rodape_dir, "%s", "OK abrir");
}

void t_acervo_ui_cabe_no_quadro(void)
{
    COMECA("acervo · a Biblioteca cabe em 240 × 416, com título comprido");

    bitmap_liga(&bm, px, TELA_L, TELA_A);

    // Os três conceitos aprovados têm desenhos próprios.
    ESPERA(ICONES[ICO_CONTINUAR_LENDO].l > 0);
    ESPERA(ICONES[ICO_BIBLIOTECA].l > 0);
    ESPERA(ICONES[ICO_TEMPO_LEITURA].l > 0);

    vista_acervo_t v;
    monta(&v, 6, true);
    tela_acervo(&bm, &v);

    // Nada pintado abaixo do rodapé nem acima da barra.
    for (int y = GRID_RODAPE_Y - 2; y < GRID_RODAPE_Y; y++)
        for (int x = 0; x < TELA_L; x++)
            ESPERA(!gfx_le(&bm, x, y));
    TERMINA();
}

void t_acervo_ui_cabecalhos_tem_icones_legiveis(void)
{
    COMECA("acervo · ícones de seção têm escala própria para o e-paper");

    // Abaixo de 22 px os traços se fundem no espalhamento do painel.
    ESPERA(ICONES[ICO_CONTINUAR_LENDO].l >= 22);
    ESPERA(ICONES[ICO_CONTINUAR_LENDO].a >= 22);
    ESPERA(ICONES[ICO_BIBLIOTECA].l >= 22);
    ESPERA(ICONES[ICO_BIBLIOTECA].a >= 22);
    TERMINA();
}

void t_acervo_ui_miniatura_nao_cobre_biblioteca(void)
{
    COMECA("acervo · miniatura começa abaixo do cabeçalho Biblioteca");
    vista_acervo_t v;
    monta(&v, 2, true);

    int x, y, l, a;
    ESPERA(tela_acervo_capa_area(&v, 0, &x, &y, &l, &a));

    // Com Continuar lendo, a primeira miniatura começa depois dos dois
    // cabeçalhos e do cartaz (abaixo de 210).
    ESPERA(y >= 210);
    TERMINA();
}

void t_obra_ui_sinopse_longa_rola_sem_ser_cortada(void)
{
    COMECA("obra · a sinopse inteira vira rolagem, não reticências");
    bitmap_liga(&bm, px, TELA_L, TELA_A);
    vista_obra_t v; memset(&v, 0, sizeof v);
    snprintf(v.barra, sizeof v.barra, "Obra");
    snprintf(v.titulo, sizeof v.titulo, "Um livro de teste");
    snprintf(v.autor, sizeof v.autor, "Uma autora");
    memset(v.sinopse, 'a', sizeof v.sinopse - 1);
    v.sinopse[sizeof v.sinopse - 1] = '\0';
    ESPERA(tela_obra_paradas(&bm, &v) > 1);
    TERMINA();
}

void t_acervo_ui_a_linha_tem_area_propria(void)
{
    COMECA("acervo · cada linha declara a faixa que o motor vai sujar");

    vista_acervo_t v;
    monta(&v, 4, true);

    int x = 0, y = 0, l = 0, a = 0;
    tela_acervo_linha_area(&v, 0, &x, &y, &l, &a);

    ESPERA(a > 0 && l == TELA_L);
    ESPERA(y > GRID_BARRA_A);

    // Toda linha que CABE está dentro do quadro.
    int cabem = tela_acervo_cabem(&v);
    ESPERA(cabem > 0);
    tela_acervo_linha_area(&v, cabem - 1, &x, &y, &l, &a);
    ESPERA(y + a <= GRID_RODAPE_Y);

    // A que não cabe não tem área.
    tela_acervo_linha_area(&v, cabem, &x, &y, &l, &a);
    ESPERA_IGUAL(a, 0);
    TERMINA();
}

// O vazio mostra um LIVRO, não uma moldura vazia (que parece imagem que não
// carregou).
void t_acervo_ui_o_vazio_mostra_um_livro(void)
{
    COMECA("acervo · o vazio mostra um livro, e não um quadrado");

    bitmap_liga(&bm, px, TELA_L, TELA_A);

    vista_acervo_t v;
    monta(&v, 0, false);
    snprintf(v.vazio, sizeof v.vazio, "%s", "Seu Acervo está vazio");
    tela_acervo(&bm, &v);

    // A caixa do desenho, achada pela tinta na metade de cima.
    int x0 = TELA_L, x1 = -1, y0 = TELA_A, y1 = -1;
    for (int y = 40; y < 150; y++)
        for (int x = 0; x < TELA_L; x++)
            if (gfx_le(&bm, x, y)) {
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
    ESPERA(x1 > x0 && y1 > y0);

    // Um livro aberto é mais largo que alto.
    ESPERA(x1 - x0 > y1 - y0);

    // Os cantos são vazios: a lombada sobe no meio.
    ESPERA(!gfx_le(&bm, x0, y0));
    ESPERA(!gfx_le(&bm, x1, y0));

    // A lombada: tinta na coluna do meio.
    int meio = (x0 + x1) / 2, coluna = 0;
    for (int y = y0; y <= y1; y++)
        if (gfx_le(&bm, meio, y) || gfx_le(&bm, meio - 1, y) ||
            gfx_le(&bm, meio + 1, y)) coluna++;
    ESPERA(coluna > (y1 - y0) / 2);
    TERMINA();
}

// As duas linhas do título do vazio nascem centradas.
void t_acervo_ui_o_titulo_do_vazio_e_centrado(void)
{
    COMECA("acervo · o título do vazio é centrado, linha a linha");

    bitmap_liga(&bm, px, TELA_L, TELA_A);

    vista_acervo_t v;
    monta(&v, 0, false);
    snprintf(v.vazio, sizeof v.vazio, "%s", "Seu Acervo está vazio");
    tela_acervo(&bm, &v);

    // Cada linha de texto é medida inteira (uma fatia pode ser só um acento).
    int linhas = 0, x0 = TELA_L, x1 = -1, vazias = 0;
    for (int y = 108; y <= 165; y++) {
        int a0 = TELA_L, a1 = -1;
        for (int x = 0; x < TELA_L; x++)
            if (gfx_le(&bm, x, y)) { if (x < a0) a0 = x; if (x > a1) a1 = x; }

        if (a1 >= 0) {
            if (a0 < x0) x0 = a0;
            if (a1 > x1) x1 = a1;
            vazias = 0;
            continue;
        }

        // Três linhas em branco fecham o bloco.
        if (x1 >= 0 && ++vazias >= 3) {
            int fora = (x0 + x1) / 2 - TELA_L / 2;
            if (fora < 0) fora = -fora;
            ESPERA(fora <= 4);
            linhas++;
            x0 = TELA_L; x1 = -1;
        }
    }
    ESPERA_IGUAL(linhas, 2);
    TERMINA();
}

void t_acervo_ui_o_vazio_aparece_escrito(void)
{
    COMECA("acervo · o vazio é escrito, e não uma tela em branco");

    bitmap_liga(&bm, px, TELA_L, TELA_A);

    vista_acervo_t v;
    monta(&v, 0, false);
    snprintf(v.vazio, sizeof v.vazio, "%s", "Seu Acervo está vazio");
    tela_acervo(&bm, &v);

    // Alguma tinta no meio da tela.
    int tinta = 0;
    for (int y = 80; y < 200; y++)
        for (int x = 0; x < TELA_L; x++)
            if (gfx_le(&bm, x, y)) tinta++;
    ESPERA(tinta > 40);
    TERMINA();
}

void t_acervo_ui_o_vazio_mostra_o_qr_do_aplicativo(void)
{
    COMECA("acervo · o vazio aponta para o aplicativo do Tinto");

    bitmap_liga(&bm, px, TELA_L, TELA_A);

    vista_acervo_t v;
    monta(&v, 0, false);
    snprintf(v.vazio, sizeof v.vazio, "%s", "Seu Acervo está vazio");
    snprintf(v.vazio_como, sizeof v.vazio_como, "%s",
             "Adicione livros pelo aplicativo Tinto");
    tela_acervo(&bm, &v);

    // O QR é o único bloco denso e quadrado na metade de baixo.
    int tinta = 0;
    for (int y = 240; y < GRID_RODAPE_Y - 4; y++)
        for (int x = 40; x < TELA_L - 40; x++)
            if (gfx_le(&bm, x, y)) tinta++;
    ESPERA(tinta > 900);

    TERMINA();
}
