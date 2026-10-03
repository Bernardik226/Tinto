#include "xadrez.h"
#include "campos.h"
#include <stdio.h>
#include <string.h>

void vista_xadrez_descreve_lance(const xadrez_hist_item_t *h,
                                 char *codigo, size_t codigo_cap,
                                 char *descricao, size_t descricao_cap,
                                 char *acao, size_t acao_cap)
{
    static const char iniciais[] = {' ', ' ', 'C', 'B', 'T', 'D', 'R'};
    static const char *nomes[] = {"Casa", "Peão", "Cavalo", "Bispo", "Torre", "Dama", "Rei"};
    static const char *objetos[] = {"peça", "peão", "cavalo", "bispo", "torre", "dama", "rei"};
    int tipo = xadrez_tipo(h->peca);
    char de[3] = {(char)('a' + (h->lance.de & 7)), (char)('1' + (h->lance.de >> 3)), '\0'};
    char para[3] = {(char)('a' + (h->lance.para & 7)), (char)('1' + (h->lance.para >> 3)), '\0'};
    const char *fim = h->marcas & XZ_HIST_MATE ? "#" : h->marcas & XZ_HIST_XEQUE ? "+" : "";

    if (h->marcas & XZ_HIST_ROQUE) {
        bool pequeno = (h->lance.para & 7) == 6;
        snprintf(codigo, codigo_cap, "%s%s", pequeno ? "O-O" : "O-O-O", fim);
        snprintf(descricao, descricao_cap, "Roque %s", pequeno ? "pequeno" : "grande");
    } else {
        char peca[2] = {iniciais[tipo], '\0'};
        char promocao[3] = "";
        if (h->marcas & XZ_HIST_PROMOCAO)
            snprintf(promocao, sizeof promocao, "=%c", iniciais[h->lance.promocao]);
        snprintf(codigo, codigo_cap, "%s%s%s%s%s%s", tipo == XZ_PEAO ? "" : peca,
                 de, h->marcas & XZ_HIST_CAPTURA ? "x" : "-", para, promocao, fim);
        if (h->marcas & XZ_HIST_PROMOCAO)
            snprintf(descricao, descricao_cap, "Peão de %s para %s", de, para);
        else
            snprintf(descricao, descricao_cap, "%s de %s para %s", nomes[tipo], de, para);
    }

    if (h->marcas & XZ_HIST_PROMOCAO) {
        const char *artigo = h->lance.promocao == XZ_DAMA ||
                            h->lance.promocao == XZ_TORRE ? "a" : "o";
        snprintf(acao, acao_cap, "Promoveu %s %s", artigo, objetos[h->lance.promocao]);
    }
    else if (h->marcas & XZ_HIST_EN_PASSANT)
        snprintf(acao, acao_cap, "Capturou peão · en passant");
    else if (h->marcas & XZ_HIST_CAPTURA)
        snprintf(acao, acao_cap, "Capturou %s", objetos[xadrez_tipo(h->capturada)]);
    else
        snprintf(acao, acao_cap, "%s", h->marcas & XZ_HIST_ROQUE ? "Roque" : "Movimento");

    size_t usado = strlen(acao);
    if (usado < acao_cap && h->marcas & (XZ_HIST_XEQUE | XZ_HIST_MATE))
        snprintf(acao + usado, acao_cap - usado, " · %s",
                 h->marcas & XZ_HIST_MATE ? "xeque-mate" : "xeque");
}

static void barra(const estado_t *e, vista_xadrez_t *v)
{
    vista_hora_da_barra(e, v->hora, sizeof v->hora);
    v->bateria = e->bateria;
    v->wifi = vista_wifi_da_barra(e);
    v->sinc = vista_sinc_da_barra(e);
}

void vista_jogos(const estado_t *e, vista_xadrez_t *out)
{
    memset(out, 0, sizeof *out);
    barra(e, out);
    out->pagina = XZ_PAG_INICIO;
    snprintf(out->titulo, sizeof out->titulo, "%s", "Jogos");
    snprintf(out->subtitulo, sizeof out->subtitulo, "%s", "Uma pausa tranquila");
}

void vista_xadrez(const estado_t *e, vista_xadrez_t *out)
{
    memset(out, 0, sizeof *out);
    barra(e, out);
    const xadrez_app_t *x = &e->xadrez;
    out->pagina = (xadrez_pagina_t)x->pagina;
    out->tem_salva = x->tem_salva;
    out->salvamento_falhou = x->salvamento_falhou;
    out->maquina_pensando = x->maquina_pensando;
    out->mostrar_ajuda = x->mostrar_ajuda;
    out->modo = x->modo;
    out->cor_humana = x->cor_humana;
    out->dificuldade = x->dificuldade;
    out->orientacao = x->orientacao;
    out->n_lances = x->n_lances;
    out->historico_total = x->historico_total;
    out->historico_desloc = x->historico_desloc;
    memcpy(out->historico, x->historico, sizeof out->historico);
    out->cursor = x->pagina == XZ_PAG_TABULEIRO || x->pagina == XZ_PAG_PROMOCAO ||
                  x->pagina == XZ_PAG_MENU || x->pagina == XZ_PAG_FINAL
                ? x->cursor : x->menu_cursor;
    out->menu_cursor = x->menu_cursor;
    out->promocao = x->promocao.promocao;
    out->cor_baixo = x->cor_baixo;
    out->origem = x->origem;
    out->turno = x->posicao.turno; out->ultimo = x->posicao.ultimo;
    out->resultado = x->resultado;
    out->rei_xeque = 255;
    memcpy(out->casa, x->posicao.casa, sizeof out->casa);
    if (xadrez_em_xeque(&x->posicao, (xadrez_cor_t)x->posicao.turno))
        for (int i = 0; i < 64; i++)
            if (xadrez_tipo(out->casa[i]) == XZ_REI &&
                xadrez_cor(out->casa[i]) == (xadrez_cor_t)x->posicao.turno) {
                out->rei_xeque = (uint8_t)i;
                break;
            }
    if (x->mostrar_ajuda && x->origem >= 0) {
        xadrez_mov_t m[XADREZ_MOV_MAX];
        int n = xadrez_movimentos(&x->posicao, x->origem, m, XADREZ_MOV_MAX);
        for (int i = 0; i < n; i++) out->destinos |= UINT64_C(1) << m[i].para;
    }
    snprintf(out->titulo, sizeof out->titulo, "%s", "Xadrez");
    if (out->pagina == XZ_PAG_TABULEIRO || out->pagina == XZ_PAG_PROMOCAO ||
        out->pagina == XZ_PAG_MENU || out->pagina == XZ_PAG_FINAL ||
        out->pagina == XZ_PAG_EMPATE || out->pagina == XZ_PAG_ABANDONAR) {
        if (out->maquina_pensando) {
            snprintf(out->subtitulo, sizeof out->subtitulo, "%s", "Máquina joga");
            snprintf(out->contexto, sizeof out->contexto, "%s", "Máquina pensando…");
            if (out->mostrar_ajuda)
                snprintf(out->ajuda, sizeof out->ajuda, "%s", "A busca não bloqueia os botões");
            return;
        }
        snprintf(out->subtitulo, sizeof out->subtitulo, "%s jogam",
                 out->turno == XZ_BRANCAS ? "Brancas" : "Pretas");
        static const char *nomes[] = {"Casa", "Peão", "Cavalo", "Bispo", "Torre", "Dama", "Rei"};
        if (out->origem >= 0) {
            int c = out->cursor & 7, l = (out->cursor >> 3) + 1;
            uint8_t p = out->casa[out->cursor];
            if (p)
                snprintf(out->contexto, sizeof out->contexto, "%s %s · %c%d",
                         nomes[xadrez_tipo(p)],
                         xadrez_cor(p) == XZ_BRANCAS ? "branco" : "preto", 'a' + c, l);
            else
                snprintf(out->contexto, sizeof out->contexto, "Casa · %c%d", 'a' + c, l);
        } else if (out->ultimo.de != out->ultimo.para) {
            uint8_t p = out->casa[out->ultimo.para];
            snprintf(out->contexto, sizeof out->contexto,
                     "Última · %s %c%d–%c%d", nomes[xadrez_tipo(p)],
                     'a' + (out->ultimo.de & 7), 1 + (out->ultimo.de >> 3),
                     'a' + (out->ultimo.para & 7), 1 + (out->ultimo.para >> 3));
        } else {
            snprintf(out->contexto, sizeof out->contexto, "%s", "Partida pronta");
        }
        if (out->mostrar_ajuda)
            snprintf(out->ajuda, sizeof out->ajuda, "%s",
                     out->origem >= 0 ? "Escolha uma casa marcada" : "OK escolhe a peça");
    } else if (out->pagina == XZ_PAG_RESULTADO) {
        snprintf(out->subtitulo, sizeof out->subtitulo, "%s", "Partida encerrada");
        const char *r = "Empate";
        if (out->resultado == XZ_MATE_BRANCAS) r = "Brancas venceram";
        else if (out->resultado == XZ_MATE_PRETAS) r = "Pretas venceram";
        snprintf(out->contexto, sizeof out->contexto, "%s", r);
        if (out->resultado == XZ_EMPATE_AFOGAMENTO)
            snprintf(out->ajuda, sizeof out->ajuda, "%s", "Rei afogado");
        else if (out->resultado == XZ_EMPATE_50_LANCES)
            snprintf(out->ajuda, sizeof out->ajuda, "%s", "Regra dos 50 lances");
        else if (out->resultado == XZ_EMPATE_REPETICAO)
            snprintf(out->ajuda, sizeof out->ajuda, "%s", "Posição repetida três vezes");
        else if (out->resultado == XZ_EMPATE_MATERIAL)
            snprintf(out->ajuda, sizeof out->ajuda, "%s", "Material insuficiente");
        else if (out->resultado == XZ_EMPATE_ACORDO)
            snprintf(out->ajuda, sizeof out->ajuda, "%s", "Empate aceito pelos jogadores");

        uint16_t a2 = x->placar_a2, b2 = x->placar_b2;
        if (out->resultado == XZ_MATE_BRANCAS || out->resultado == XZ_MATE_PRETAS) {
            bool brancas = out->resultado == XZ_MATE_BRANCAS;
            bool primeiro_brancas = out->modo == XZ_MODO_MAQUINA
                                  ? out->cor_humana == XZ_BRANCAS
                                  : out->cor_baixo == XZ_BRANCAS;
            if (brancas == primeiro_brancas) a2 += 2;
            else b2 += 2;
        } else if (out->resultado != XZ_EM_CURSO) {
            a2++; b2++;
        }
        out->placar_a2 = a2; out->placar_b2 = b2;
    }
}
