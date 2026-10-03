#include "teste.h"
#include "dado/xadrez.h"
#include "ui/xadrez.h"
#include "tela/texto.h"
#include <stdlib.h>

#define XZ_UI_CASA 29

static void renderiza(bitmap_t *bm, uint8_t *px, vista_xadrez_t *v)
{
    memset(px, 0, (TELA_L / 8) * TELA_A);
    bitmap_liga(bm, px, TELA_L, TELA_A);
    v->pagina = XZ_PAG_TABULEIRO;
    v->cursor = 255;
    v->origem = -1;
    tela_xadrez(bm, v);
}

void t_xadrez_ui_desenha_tabuleiro_e_pecas(void)
{
    COMECA("xadrez · o tabuleiro cabe no vidro e tem todas as peças");
    static uint8_t px[(TELA_L / 8) * TELA_A];
    bitmap_t bm; bitmap_liga(&bm, px, TELA_L, TELA_A);
    vista_xadrez_t v; memset(&v, 0, sizeof v);
    v.pagina = XZ_PAG_TABULEIRO;
    v.cursor = XZ_E2; v.origem = -1; v.bateria = 80;
    snprintf(v.hora, sizeof v.hora, "%s", "12:00");
    snprintf(v.subtitulo, sizeof v.subtitulo, "%s", "Brancas jogam");
    snprintf(v.contexto, sizeof v.contexto, "%s", "Peão · e2");
    snprintf(v.ajuda, sizeof v.ajuda, "%s", "OK escolhe a peça");
    xadrez_pos_t p; xadrez_nova(&p); memcpy(v.casa, p.casa, sizeof v.casa);
    gfx_zera_faltantes();
    tela_xadrez(&bm, &v);
    int tinta = 0;
    for (int y = 68; y < 300; y++) for (int x = 4; x < 236; x++)
        tinta += gfx_le(&bm, x, y);
    ESPERA(tinta > 10000);
    /* Casa escura: branca fica cheia de papel; preta, só vazada. */
    int papel_branca = 0, papel_preta = 0;
    for (int j = 2; j < 27; j++) for (int i = 2; i < 27; i++) {
        papel_branca += !gfx_le(&bm, 33 + i, 242 + j); /* b2 */
        papel_preta += !gfx_le(&bm,  4 + i,  97 + j); /* a7 */
    }
    ESPERA(papel_branca * 2 > papel_preta * 3);
    ESPERA_IGUAL(gfx_faltantes(), 0);
    TERMINA();
}

void t_xadrez_ui_promocao_nao_mostra_texto_atras(void)
{
    COMECA("xadrez · promoção ocupa um painel sem texto por baixo");
    static uint8_t a[(TELA_L / 8) * TELA_A], b[(TELA_L / 8) * TELA_A];
    bitmap_t ba, bb;
    vista_xadrez_t v; memset(&v, 0, sizeof v);
    v.pagina = XZ_PAG_PROMOCAO; v.cursor = 255; v.origem = -1;
    v.promocao = XZ_DAMA;
    snprintf(v.contexto, sizeof v.contexto, "%s", "Peão preto · a4");
    snprintf(v.ajuda, sizeof v.ajuda, "%s", "texto que ficava atrás");
    bitmap_liga(&ba, a, TELA_L, TELA_A); tela_xadrez(&ba, &v);
    snprintf(v.contexto, sizeof v.contexto, "%s", "outro contexto");
    snprintf(v.ajuda, sizeof v.ajuda, "%s", "outra ajuda");
    bitmap_liga(&bb, b, TELA_L, TELA_A); tela_xadrez(&bb, &v);
    for (int y = 309; y < 376; y++) for (int x = 0; x < TELA_L; x++)
        ESPERA_IGUAL(gfx_le(&ba, x, y), gfx_le(&bb, x, y));
    TERMINA();
}

void t_xadrez_ui_centraliza_e_gira_peca_local(void)
{
    COMECA("xadrez · glifo é central e fica de frente para cada jogador");
    static uint8_t normal[(TELA_L / 8) * TELA_A];
    static uint8_t girada[(TELA_L / 8) * TELA_A];
    bitmap_t bn, bg;
    vista_xadrez_t v; memset(&v, 0, sizeof v);
    v.casa[XZ_CASA('b', 1)] = XZ_CAVALO | 8;
    v.cor_baixo = XZ_BRANCAS;
    v.modo = XZ_MODO_MAQUINA;
    renderiza(&bn, normal, &v);

    const int sx = 33, sy = 271;
    int xmin = sx + XZ_UI_CASA, xmax = sx;
    int ymin = sy + XZ_UI_CASA, ymax = sy;
    for (int y = sy + 1; y < sy + XZ_UI_CASA - 1; y++)
        for (int x = sx + 1; x < sx + XZ_UI_CASA - 1; x++) if (gfx_le(&bn, x, y)) {
            if (x < xmin) xmin = x;
            if (x > xmax) xmax = x;
            if (y < ymin) ymin = y;
            if (y > ymax) ymax = y;
        }
    ESPERA(xmax >= xmin);
    ESPERA(abs((xmin - sx) - (sx + XZ_UI_CASA - 1 - xmax)) <= 1);
    ESPERA(ymax - ymin + 1 >= 19);

    v.modo = XZ_MODO_LOCAL;
    renderiza(&bg, girada, &v);
    for (int y = 1; y < XZ_UI_CASA - 1; y++) for (int x = 1; x < XZ_UI_CASA - 1; x++)
        ESPERA_IGUAL(gfx_le(&bg, sx + x, sy + y),
                     gfx_le(&bn, sx + XZ_UI_CASA - 1 - x,
                            sy + XZ_UI_CASA - 1 - y));
    TERMINA();
}

void t_xadrez_ui_dama_tem_cinco_pontas_e_base_nitida(void)
{
    COMECA("xadrez · dama tem cinco pontas e ocupa a casa com nitidez");
    static uint8_t px[(TELA_L / 8) * TELA_A];
    bitmap_t bm;
    vista_xadrez_t v; memset(&v, 0, sizeof v);
    v.casa[XZ_CASA('b', 1)] = XZ_DAMA | 8;
    v.cor_baixo = XZ_BRANCAS;
    v.modo = XZ_MODO_MAQUINA;
    renderiza(&bm, px, &v);

    const int sx = 33, sy = 271;
    int xmin = sx + XZ_UI_CASA, xmax = sx, ymin = sy + XZ_UI_CASA, ymax = sy;
    for (int y = sy + 1; y < sy + XZ_UI_CASA - 1; y++)
        for (int x = sx + 1; x < sx + XZ_UI_CASA - 1; x++) if (gfx_le(&bm, x, y)) {
            if (x < xmin) xmin = x;
            if (x > xmax) xmax = x;
            if (y < ymin) ymin = y;
            if (y > ymax) ymax = y;
        }
    ESPERA(xmax - xmin + 1 >= 19);
    ESPERA(ymax - ymin + 1 >= 20);
    int pontas = 0; bool em_tinta = false;
    for (int x = xmin; x <= xmax; x++) {
        bool tinta = gfx_le(&bm, x, ymin);
        if (tinta && !em_tinta) pontas++;
        em_tinta = tinta;
    }
    ESPERA_IGUAL(pontas, 5);
    TERMINA();
}

void t_xadrez_ui_cheia_e_vazada_usam_a_mesma_silhueta(void)
{
    COMECA("xadrez · cheia e vazada preservam o mesmo desenho da peça");
    static uint8_t px[(TELA_L / 8) * TELA_A];
    bitmap_t bm;
    vista_xadrez_t v; memset(&v, 0, sizeof v);
    v.casa[XZ_CASA('a', 1)] = XZ_CAVALO | 8; /* vazada na casa escura */
    v.casa[XZ_CASA('b', 1)] = XZ_CAVALO | 8; /* cheia na casa clara */
    v.cor_baixo = XZ_BRANCAS;
    v.modo = XZ_MODO_MAQUINA;
    renderiza(&bm, px, &v);

    const int ax = 4, bx = 33, sy = 271;
    int vazada = 0, cheia = 0;
    for (int y = 1; y < XZ_UI_CASA - 1; y++)
        for (int x = 1; x < XZ_UI_CASA - 1; x++) {
            bool contorno = !gfx_le(&bm, ax + x, sy + y);
            bool corpo = gfx_le(&bm, bx + x, sy + y);
            if (contorno) {
                vazada++;
                ESPERA(corpo);
            }
            if (corpo) cheia++;
        }
    ESPERA(vazada > 0);
    ESPERA(cheia > vazada);
    TERMINA();
}

void t_xadrez_ui_v3_desenha_opcoes_gaveta_e_resultado(void)
{
    COMECA("xadrez V3 · opções, gaveta e resultado têm telas próprias");
    static uint8_t px[(TELA_L / 8) * TELA_A];
    bitmap_t bm; vista_xadrez_t v; memset(&v, 0, sizeof v);
    bitmap_liga(&bm, px, TELA_L, TELA_A);
    v.pagina = XZ_PAG_OPCOES; v.cursor = 1;
    tela_xadrez(&bm, &v);
    int icone = 0;
    for (int y = 120; y < 240; y++) for (int x = 8; x < 42; x++)
        icone += gfx_le(&bm, x, y);
    ESPERA(icone > 40);

    xadrez_pos_t p; xadrez_nova(&p); memcpy(v.casa, p.casa, sizeof v.casa);
    v.pagina = XZ_PAG_MENU; v.cursor = 0; v.modo = XZ_MODO_LOCAL;
    v.cor_baixo = XZ_BRANCAS; v.origem = -1;
    tela_xadrez(&bm, &v);
    int pequeno = 0;
    for (int y = 316; y < 342; y++) for (int x = 8; x < 30; x++)
        pequeno += gfx_le(&bm, x, y);
    ESPERA(pequeno > 10); /* pictograma simplificado do histórico */

    v.pagina = XZ_PAG_RESULTADO; v.resultado = XZ_EMPATE_ACORDO;
    snprintf(v.ajuda, sizeof v.ajuda, "%s", "Empate aceito pelos jogadores");
    tela_xadrez(&bm, &v);
    ESPERA(gfx_le(&bm, 8, 211)); /* primeira ação começa por uma divisória */
    TERMINA();
}

void t_xadrez_ui_v3_horizontal_usa_o_vidro_deitado(void)
{
    COMECA("xadrez V3 · horizontal gira um tabuleiro de 200 pixels");
    static uint8_t px[(TELA_L / 8) * TELA_A];
    bitmap_t bm; vista_xadrez_t v; memset(&v, 0, sizeof v);
    bitmap_liga(&bm, px, TELA_L, TELA_A);
    xadrez_pos_t p; xadrez_nova(&p); memcpy(v.casa, p.casa, sizeof v.casa);
    v.pagina = XZ_PAG_TABULEIRO; v.orientacao = XZ_HORIZONTAL;
    v.cor_baixo = XZ_BRANCAS; v.cursor = 255; v.origem = -1;
    tela_xadrez(&bm, &v);
    /* Limites do quadrado lógico 8,20..207,219 depois da rotação. */
    ESPERA(gfx_le(&bm, 20, 8));
    ESPERA(gfx_le(&bm, 219, 207));
    ESPERA(!gfx_le(&bm, 10, 8));
    TERMINA();
}

void t_xadrez_ui_historico_descreve_os_lances(void)
{
    COMECA("xadrez · histórico traduz notação, peça e consequência");
    char codigo[24], descricao[48], acao[48];
    xadrez_hist_item_t h = {
        .lance = {XZ_CASA('c', 4), XZ_CASA('f', 7), 0, 0},
        .peca = XZ_BISPO, .capturada = XZ_PEAO, .cor = XZ_BRANCAS,
        .marcas = XZ_HIST_CAPTURA | XZ_HIST_XEQUE,
    };
    vista_xadrez_descreve_lance(&h, codigo, sizeof codigo,
                                descricao, sizeof descricao,
                                acao, sizeof acao);
    ESPERA_IGUAL(strcmp(codigo, "Bc4xf7+"), 0);
    ESPERA_IGUAL(strcmp(descricao, "Bispo de c4 para f7"), 0);
    ESPERA_IGUAL(strcmp(acao, "Capturou peão · xeque"), 0);

    h = (xadrez_hist_item_t){
        .lance = {XZ_CASA('e', 7), XZ_CASA('e', 8), XZ_DAMA, 0},
        .peca = XZ_PEAO, .cor = XZ_BRANCAS,
        .marcas = XZ_HIST_PROMOCAO | XZ_HIST_MATE,
    };
    vista_xadrez_descreve_lance(&h, codigo, sizeof codigo,
                                descricao, sizeof descricao,
                                acao, sizeof acao);
    ESPERA_IGUAL(strcmp(codigo, "e7-e8=D#"), 0);
    ESPERA_IGUAL(strcmp(acao, "Promoveu a dama · xeque-mate"), 0);
    TERMINA();
}

void t_xadrez_ui_icones_dizem_qual_acao_e_peca(void)
{
    COMECA("xadrez · ícones distinguem jogo, ação e cor escolhida");
    static uint8_t a[(TELA_L / 8) * TELA_A], b[(TELA_L / 8) * TELA_A];
    bitmap_t ba, bb; vista_xadrez_t v; memset(&v, 0, sizeof v);

    bitmap_liga(&ba, a, TELA_L, TELA_A);
    v.cursor = 255;
    tela_jogos(&ba, &v);
    int cavalo = 0;
    for (int y = 109; y < 138; y++) for (int x = 11; x < 40; x++)
        cavalo += gfx_le(&ba, x, y);
    ESPERA(cavalo > 150);
    ESPERA(!gfx_le(&ba, 16, 134)); /* sem a carta do ícone de área */
    int segunda_linha = 0;
    for (int y = 149; y < 164; y++) for (int x = 46; x < 232; x++)
        segunda_linha += gfx_le(&ba, x, y);
    ESPERA(segunda_linha > 10);

    memset(a, 0, sizeof a); bitmap_liga(&ba, a, TELA_L, TELA_A);
    v.pagina = XZ_PAG_INICIO; v.cursor = 255;
    tela_xadrez(&ba, &v);
    int play = 0, mais = 0;
    for (int y = 260; y < 278; y++) for (int x = 30; x < 42; x++)
        play += gfx_le(&ba, x, y);
    for (int y = 308; y < 326; y++) for (int x = 30; x < 42; x++)
        mais += gfx_le(&ba, x, y);
    ESPERA(play > 15); /* play no canto inferior direito do tabuleiro */
    ESPERA(mais > 15); /* Nova mantém o + no mesmo canto */

    memset(a, 0, sizeof a); bitmap_liga(&ba, a, TELA_L, TELA_A);
    v.pagina = XZ_PAG_PREPARAR_MAQUINA; v.cor_baixo = XZ_BRANCAS;
    tela_xadrez(&ba, &v);
    memset(b, 0, sizeof b); bitmap_liga(&bb, b, TELA_L, TELA_A);
    v.cor_baixo = XZ_PRETAS; tela_xadrez(&bb, &v);
    int branco = 0, preto = 0;
    for (int y = 241; y < 270; y++) for (int x = 11; x < 40; x++) {
        branco += gfx_le(&ba, x, y); preto += gfx_le(&bb, x, y);
    }
    ESPERA(preto > branco);
    int bandeira = 0;
    for (int y = 337; y < 366; y++) for (int x = 11; x < 40; x++)
        bandeira += gfx_le(&bb, x, y);
    ESPERA(bandeira > 45);
    TERMINA();
}

void t_xadrez_ui_capa_guia_a_escolha_da_partida(void)
{
    COMECA("xadrez · capa permanece na escolha e muda na preparação");
    static uint8_t inicio[(TELA_L / 8) * TELA_A];
    static uint8_t modos[(TELA_L / 8) * TELA_A];
    static uint8_t maquina[(TELA_L / 8) * TELA_A];
    static uint8_t local[(TELA_L / 8) * TELA_A];
    bitmap_t bi, bm, bma, bl;
    vista_xadrez_t v; memset(&v, 0, sizeof v);
    v.cursor = 255; v.bateria = 80;
    snprintf(v.hora, sizeof v.hora, "%s", "09:14");

    bitmap_liga(&bi, inicio, TELA_L, TELA_A);
    v.pagina = XZ_PAG_INICIO; tela_xadrez(&bi, &v);
    bitmap_liga(&bm, modos, TELA_L, TELA_A);
    v.pagina = XZ_PAG_MODOS; tela_xadrez(&bm, &v);

    /* Moldura dupla e cartaz ocupam a mesma região nas duas escolhas. */
    ESPERA(gfx_le(&bi, 7, 31));
    ESPERA(gfx_le(&bi, 232, 211));
    ESPERA(gfx_le(&bi, 9, 33));
    ESPERA(!gfx_le(&bi, 235, 34)); /* quadro simétrico, sem sombra deslocada */
    for (int y = 23; y < 216; y++) for (int x = 0; x < TELA_L; x++)
        ESPERA_IGUAL(gfx_le(&bi, x, y), gfx_le(&bm, x, y));

    /* Casa escura é tinta sólida: trama de 1 px borra no vidro. */
    int casa_escura = 0, titulo = 0;
    for (int y = 58; y < 70; y++) for (int x = 44; x < 60; x++)
        casa_escura += gfx_le(&bi, x, y);
    for (int y = 115; y < 160; y++) for (int x = 70; x < 172; x++)
        titulo += gfx_le(&bi, x, y);
    ESPERA_IGUAL(casa_escura, 12 * 16);
    ESPERA(titulo > 100);

    /* A lista usa o seletor do Tinto, sem setas decorativas no canto. */
    int setas = 0;
    for (int y = 262; y < 278; y++) for (int x = 220; x < 232; x++)
        setas += gfx_le(&bm, x, y);
    ESPERA_IGUAL(setas, 0);

    bitmap_liga(&bma, maquina, TELA_L, TELA_A);
    v.pagina = XZ_PAG_PREPARAR_MAQUINA; tela_xadrez(&bma, &v);
    bitmap_liga(&bl, local, TELA_L, TELA_A);
    v.pagina = XZ_PAG_PREPARAR_LOCAL; tela_xadrez(&bl, &v);
    int rotulos_diferentes = 0;
    for (int y = 36; y < 49; y++) for (int x = 13; x < 150; x++)
        rotulos_diferentes += gfx_le(&bma, x, y) != gfx_le(&bl, x, y);
    ESPERA(rotulos_diferentes > 20);
    int diferentes = 0;
    for (int y = 54; y < 205; y++) for (int x = 13; x < 227; x++)
        diferentes += gfx_le(&bma, x, y) != gfx_le(&bl, x, y);
    ESPERA(diferentes > 250);
    TERMINA();
}

void t_xadrez_ui_acoes_e_placar_sao_legiveis(void)
{
    COMECA("xadrez · título, ações e placar não viram glifos quebrados");
    static uint8_t atual[(TELA_L / 8) * TELA_A];
    static uint8_t titulo[(TELA_L / 8) * TELA_A];
    bitmap_t ba, bt; vista_xadrez_t v; memset(&v, 0, sizeof v);
    bitmap_liga(&ba, atual, TELA_L, TELA_A);
    bitmap_liga(&bt, titulo, TELA_L, TELA_A);

    v.pagina = XZ_PAG_MODOS; v.cursor = 255;
    tela_xadrez(&ba, &v);
    gfx_texto_ate(&bt, 45, 238, F_TITULO, "Contra a máquina", 183);
    for (int y = 238; y < 261; y++) for (int x = 45; x < 228; x++)
        ESPERA_IGUAL(gfx_le(&ba, x, y), gfx_le(&bt, x, y));

    memset(atual, 0, sizeof atual); bitmap_liga(&ba, atual, TELA_L, TELA_A);
    v.pagina = XZ_PAG_RESULTADO; v.resultado = XZ_MATE_BRANCAS;
    snprintf(v.ajuda, sizeof v.ajuda, "%s", "Brancas venceram");
    gfx_zera_faltantes();
    tela_xadrez(&ba, &v);
    ESPERA_IGUAL(gfx_faltantes(), 0);

    v.resultado = XZ_EMPATE_ACORDO;
    gfx_zera_faltantes();
    tela_xadrez(&ba, &v);
    ESPERA_IGUAL(gfx_faltantes(), 0);

    memset(atual, 0, sizeof atual); bitmap_liga(&ba, atual, TELA_L, TELA_A);
    v.pagina = XZ_PAG_ABANDONAR; v.cursor = 255;
    tela_xadrez(&ba, &v);
    /* Um X limpo: diagonais no centro, sem as barras do pictograma antigo. */
    ESPERA(gfx_le(&ba, 18, 189));
    ESPERA(gfx_le(&ba, 30, 201));
    ESPERA(!gfx_le(&ba, 16, 187));
    TERMINA();
}

void t_xadrez_ajuda_e_ultima_jogada(void)
{
    COMECA("xadrez · ajuda controla instrução e destinos; repouso mostra último lance");
    estado_t e; memset(&e, 0, sizeof e);
    xadrez_nova(&e.xadrez.posicao);
    e.xadrez.pagina = XZ_PAG_TABULEIRO;
    e.xadrez.cursor = XZ_E2;
    e.xadrez.origem = -1;
    e.xadrez.mostrar_ajuda = false;

    vista_xadrez_t v;
    vista_xadrez(&e, &v);
    ESPERA_TEXTO(v.contexto, "Partida pronta");
    ESPERA_TEXTO(v.ajuda, "");

    e.xadrez.mostrar_ajuda = true;

    ESPERA(xadrez_joga(&e.xadrez.posicao,
                       (xadrez_mov_t){XZ_E2, XZ_E4, 0, 0}));
    vista_xadrez(&e, &v);
    ESPERA_TEXTO(v.contexto, "Última · Peão e2–e4");

    e.xadrez.origem = XZ_CASA('e', 7);
    e.xadrez.cursor = XZ_CASA('e', 7);
    vista_xadrez(&e, &v);
    ESPERA_TEXTO(v.contexto, "Peão preto · e7");
    ESPERA(v.destinos != 0);

    e.xadrez.mostrar_ajuda = false;
    vista_xadrez(&e, &v);
    ESPERA_TEXTO(v.ajuda, "");
    ESPERA_IGUAL(v.destinos, 0);
    TERMINA();
}
