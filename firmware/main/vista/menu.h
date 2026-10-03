// vista/menu.h — T-14 (a gaveta) e T-25 (ajustes).
// Por dentro são a mesma lista de rótulo à esquerda e estado à direita, e
// dividem a struct.
#ifndef VISTA_MENU_H
#define VISTA_MENU_H

#include "../nucleo/estado.h"
#include "cartao.h"
#include "../tela/icones.h"
#include "linhas.h"

#define MENU_MAX 12

// ── a janela de rolagem ─────────────────────────────────────────────
// ACOMPANHA o cursor em vez de paginar: na maior parte dos passos nada se
// move, e em e-ink paginar é refresh cheio.
typedef struct {
    char titulo[20];
    char hora[9];
    int  bateria;
    // -1 esconde o ícone do rádio: num aparelho que funciona offline, ícone
    // permanente seria lembrete do que não faz falta.
    int  wifi;

    // O indicador de espera do HUD, e a fase dele.
    icone_id sinc;   // o que a sincronização faz, como forma

    // Seção antes da linha i, ou "": "Conta", "Aparelho", "Sistema".
    char secao[MENU_MAX][28];
    linha_acao_t linhas[MENU_MAX];

    // Para ONDE a linha leva (`TELA_QUANTAS` = alterna ali mesmo, ou informa).
    // Despachar pelo rótulo já deixou a tela sem porta ao renomear "Conexões".
    tela_id destino[MENU_MAX];

    // A descrição curta de um destino ("Google, voz e este Tinto"): faz de
    // Ajustes um índice.
    char sub[MENU_MAX][32];
    int  n;

    // O rodapé explicativo. 96: a frase que explica o porquê é mais longa.
    char nota[96];

    // Lista vazia explicada no MEIO do corpo: no rodapé, ficava longe do vão.
    char vazio[44];

    // Os pontinhos ao lado do `vazio`, 0 a 3, ou -1.
    int  pontos;
    int  cursor;
    int  primeira;      // a primeira linha visível — o começo da PÁGINA

    // O fim da janela, exclusivo. A vista decide: a UI mede pixel e a vista
    // linha, e a folga desenhava a mesma rede duas vezes.
    int  ultima;

    // Página atual e total, para o contador. `paginas == 1` não mostra "1 / 1".
    int  pagina, paginas;

    char rodape_esq[22], rodape_dir[22];
} vista_menu_t;

// Pôr uma linha e achar a janela que contém o cursor (`cabem` em LINHAS).
// A tela de Wi-Fi é a mesma lista.
void vista_menu_leva(vista_menu_t *o, tela_id destino);

void vista_menu_poe(vista_menu_t *o, const char *secao, icone_id ico,
                    const char *texto, const char *valor);
void vista_menu_rola(vista_menu_t *o, int cabem);

void vista_menu   (const estado_t *e, int cabem, vista_menu_t *out);
void vista_ajustes(const estado_t *e, int cabem, vista_menu_t *out);

// Aparência e Som: o card diz o estado; a lista, só valores alteráveis.
// Tudo local: se a pessoa escolhe, é ajuste.
void vista_aparencia(const estado_t *e, vista_cartao_t *out);
void vista_som(const estado_t *e, vista_cartao_t *out);
int  vista_aparencia_paradas(const estado_t *e);
int  vista_som_paradas(const estado_t *e);

#endif
