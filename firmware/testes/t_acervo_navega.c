// firmware/testes/t_acervo_navega.c — da Home ao leitor, pelos botões.
// O Acervo obedece a mesma gramática: OK faz a ação da linha, ▶ entra, BACK
// volta, MENU abre a gaveta da tela.
#include "teste.h"
#include "dado/acervo.h"
#include "uso/acervo.h"
#include "vista/acervo.h"
#include "vista/menu.h"
#include "ui/acervo.h"
#include "ui/grid.h"

static app_t ap;
static const hal_t *hal;

#define HOJE ((data_t){2026, 9, 4})

static void poe_obra(const char *id, const char *titulo, obra_estado_t est)
{
    obra_t o;
    memset(&o, 0, sizeof o);
    snprintf(o.id,     sizeof o.id,     "%s", id);
    snprintf(o.titulo, sizeof o.titulo, "%s", titulo);
    o.estado = est;
    o.tamanho = 200;
    o.baixado = est == OBRA_AQUI ? 200 : 0;
    ESPERA_IGUAL(acervo_grava_meta(hal, &o), OK);
}

static void liga_no_acervo(void)
{
    hal = pc_liga();
    pc_relogio(HOJE, 9, 14);
    app_liga(&ap, hal);
    ap.estado.rede = REDE_LIGADA;
    ap.estado.tem_token = true;
}

// ── a Home abre o Acervo ────────────────────────────────────────────
void t_acervo_navega_a_home_abre_o_acervo(void)
{
    COMECA("acervo · o cartão do Acervo na Home abre a Biblioteca");

    liga_no_acervo();
    poe_obra("ob:a", "O estrangeiro", OBRA_AQUI);

    // O segundo cartão é o Acervo.
    pc_botao(IN_DIR);
    app_passo(&ap);
    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_ACERVO);

    vista_acervo_t v;
    vista_acervo(&ap.estado, &v);
    ESPERA(v.n >= 1);
    TERMINA();
}

void t_acervo_navega_ao_entrar_pede_o_catalogo_online(void)
{
    COMECA("acervo · entrar na Biblioteca atualiza o catálogo online");

    liga_no_acervo();
    pc_nuvem_rota_zera();

    pc_botao(IN_DIR);
    app_passo(&ap);
    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_ACERVO);
    ESPERA_CONTEM(pc_nuvem_rota(), "/v1/acervo");
    TERMINA();
}

void t_acervo_navega_a_gaveta_e_do_acervo(void)
{
    COMECA("acervo · a gaveta filtra obras e abre Anotações");

    liga_no_acervo();
    ap.estado.pilha[++ap.estado.profundidade] = TELA_ACERVO;
    ap.estado.acervo_filtro = ACERVO_SO_LIVROS;

    vista_menu_t v;
    vista_menu(&ap.estado, 12, &v);
    ESPERA_IGUAL(v.n, 6);
    ESPERA_TEXTO(v.linhas[2].texto, "Livros");
    ESPERA(v.linhas[2].marcado);
    ESPERA_TEXTO(v.linhas[2].valor, "");
    ESPERA_TEXTO(v.linhas[5].texto, "Anotações");

    pc_botao(IN_MENU);
    app_passo(&ap);
    ap.estado.cursor_overlay = 5;
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_ANOTACOES);
    TERMINA();
}

// ── o OK faz o que a MARCA promete ──────────────────────────────────
// Aqui abre; só online, começa a transferência.
void t_acervo_navega_o_ok_abre_o_que_esta_aqui(void)
{
    COMECA("acervo · o OK abre a obra local e baixa a que é só online");

    liga_no_acervo();
    poe_obra("ob:a", "O estrangeiro", OBRA_AQUI);
    ESPERA_IGUAL(acervo_grava_texto(hal, "ob:a", "Continuei até a esquina.",
                                    true), OK);
    ESPERA_IGUAL(uso_carregar_acervo(hal, &ap.estado), OK);

    ap.estado.pilha[++ap.estado.profundidade] = TELA_ACERVO;
    ap.estado.cursor = 0;

    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_LEITOR);
    ESPERA(ap.estado.pagina[0]);
    TERMINA();
}

void t_acervo_navega_continuar_lendo_e_um_foco_real(void)
{
    COMECA("acervo · Continuar lendo é o primeiro foco e abre sua obra");
    liga_no_acervo();
    poe_obra("ob:a", "Primeira obra", OBRA_AQUI);
    poe_obra("ob:b", "Livro em andamento", OBRA_AQUI);
    ESPERA_IGUAL(acervo_grava_texto(hal, "ob:a", "Primeiro texto.", true), OK);
    ESPERA_IGUAL(acervo_grava_texto(hal, "ob:b", "Texto retomado.", true), OK);
    obra_t andamento;
    ESPERA_IGUAL(acervo_le_meta(hal, "ob:b", &andamento), OK);
    andamento.offset_texto = 3;
    andamento.aberta_em = HOJE;
    ESPERA_IGUAL(acervo_grava_meta(hal, &andamento), OK);
    ESPERA_IGUAL(uso_carregar_acervo(hal, &ap.estado), OK);
    ap.estado.pilha[++ap.estado.profundidade] = TELA_ACERVO;
    ap.estado.cursor = 0;
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_LEITOR);
    ESPERA_TEXTO(ap.estado.obra_aberta.id, "ob:b");
    TERMINA();
}

void t_acervo_navega_continuar_lembra_a_ultima_do_mesmo_dia(void)
{
    COMECA("acervo · Continuar lendo lembra a última aberta no mesmo dia");
    liga_no_acervo();
    poe_obra("ob:a", "Primeira obra", OBRA_AQUI);
    poe_obra("ob:b", "Última obra", OBRA_AQUI);
    ESPERA_IGUAL(acervo_grava_texto(hal, "ob:a", "Texto A.", true), OK);
    ESPERA_IGUAL(acervo_grava_texto(hal, "ob:b", "Texto B.", true), OK);

    obra_t o;
    ESPERA_IGUAL(acervo_le_meta(hal, "ob:a", &o), OK);
    o.offset_texto = 1; o.aberta_em = HOJE;
    ESPERA_IGUAL(acervo_grava_meta(hal, &o), OK);
    ESPERA_IGUAL(acervo_le_meta(hal, "ob:b", &o), OK);
    o.offset_texto = 1; o.aberta_em = HOJE;
    ESPERA_IGUAL(acervo_grava_meta(hal, &o), OK);
    ESPERA_IGUAL(uso_carregar_acervo(hal, &ap.estado), OK);

    ap.estado.pilha[++ap.estado.profundidade] = TELA_ACERVO;
    vista_acervo_t v;
    vista_acervo(&ap.estado, &v);
    int cursor_b = -1;
    for (int i = 0; i < v.n; i++)
        if (strcmp(ap.estado.acervo[v.linhas[i].indice].id, "ob:b") == 0)
            cursor_b = i + (v.tem_destaque ? 1 : 0);
    ESPERA(cursor_b >= 0);
    ap.estado.cursor = (int16_t)cursor_b;
    pc_botao(IN_OK); app_passo(&ap);
    ESPERA_TEXTO(ap.estado.obra_aberta.id, "ob:b");

    pc_botao(IN_VOLTAR); app_passo(&ap);
    ESPERA_IGUAL(uso_carregar_acervo(hal, &ap.estado), OK);
    ap.estado.cursor = 0;
    pc_botao(IN_OK); app_passo(&ap);
    ESPERA_TEXTO(ap.estado.obra_aberta.id, "ob:b");
    TERMINA();
}

void t_acervo_navega_a_obra_remota_comeca_a_baixar(void)
{
    COMECA("acervo · abrir uma obra que não está aqui começa a baixar");

    liga_no_acervo();
    poe_obra("ob:b", "Contos escolhidos", OBRA_SO_ONLINE);
    ESPERA_IGUAL(uso_carregar_acervo(hal, &ap.estado), OK);

    ap.estado.pilha[++ap.estado.profundidade] = TELA_ACERVO;
    ap.estado.cursor = 0;
    pc_nuvem_rota_zera();

    pc_botao(IN_OK);
    app_passo(&ap);

    // Não abre o leitor: não há o que ler ainda.
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_ACERVO);
    ESPERA_CONTEM(pc_nuvem_rota(), "/conteudo");
    TERMINA();
}

void t_acervo_navega_mantem_a_obra_do_cursor_visivel(void)
{
    COMECA("acervo · a estante rola para manter a obra selecionada visível");

    liga_no_acervo();
    for (int i = 0; i < 8; i++) {
        char id[16], titulo[32];
        snprintf(id, sizeof id, "ob:%d", i);
        snprintf(titulo, sizeof titulo, "Obra %d", i);
        poe_obra(id, titulo, OBRA_SO_ONLINE);
    }
    ESPERA_IGUAL(uso_carregar_acervo(hal, &ap.estado), OK);

    ap.estado.pilha[++ap.estado.profundidade] = TELA_ACERVO;
    ap.estado.cursor = 0;
    for (int i = 0; i < 7; i++) {
        pc_botao(IN_BAIXO);
        app_passo(&ap);
    }
    ESPERA_IGUAL(ap.estado.cursor, 7);

    vista_acervo_t v;
    vista_acervo(&ap.estado, &v);
    int x = 0, y = 0, l = 0, a = 0;
    tela_acervo_linha_area(&v, v.cursor, &x, &y, &l, &a);

    // A faixa do item selecionado existe no quadro atual.
    ESPERA(a > 0);
    ESPERA(y >= GRID_BARRA_A && y + a <= GRID_RODAPE_Y);
    TERMINA();
}

// ── ◀▶ viram PÁGINA no leitor ───────────────────────────────────────
void t_acervo_navega_as_setas_viram_pagina(void)
{
    COMECA("leitor · ◀▶ viram página, e BACK volta para o Acervo");

    liga_no_acervo();
    poe_obra("ob:a", "O estrangeiro", OBRA_AQUI);

    static char longo[3000];
    longo[0] = '\0';
    for (int i = 0; i < 40; i++)
        strncat(longo, "Continuei ate a esquina e o ceu estava limpido. ",
                sizeof longo - strlen(longo) - 1);
    ESPERA_IGUAL(acervo_grava_texto(hal, "ob:a", longo, true), OK);
    ESPERA_IGUAL(uso_carregar_acervo(hal, &ap.estado), OK);

    ap.estado.pilha[++ap.estado.profundidade] = TELA_ACERVO;
    ap.estado.cursor = 0;
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_LEITOR);

    int32_t primeira = ap.estado.obra_aberta.offset_texto;
    // A primeira virada sai da capa.
    pc_botao(IN_DIR);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.obra_aberta.offset_texto, primeira);

    pc_botao(IN_DIR);
    app_passo(&ap);
    ESPERA(ap.estado.obra_aberta.offset_texto > primeira);

    pc_botao(IN_ESQ);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.obra_aberta.offset_texto, primeira);

    // O BACK devolve a Biblioteca com a posição GRAVADA.
    pc_botao(IN_DIR);
    app_passo(&ap);
    int32_t onde = ap.estado.obra_aberta.offset_texto;

    pc_botao(IN_VOLTAR);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_ACERVO);

    obra_t lida;
    ESPERA_IGUAL(acervo_le_meta(hal, "ob:a", &lida), OK);
    ESPERA_IGUAL(lida.offset_texto, onde);
    TERMINA();
}

// ── a memória do livro é DEVOLVIDA (PSRAM) ──────────────────────────
void t_acervo_navega_o_livro_devolve_a_memoria(void)
{
    COMECA("leitor · o texto é emprestado e devolvido ao sair");

    liga_no_acervo();
    poe_obra("ob:a", "O estrangeiro", OBRA_AQUI);
    ESPERA_IGUAL(acervo_grava_texto(hal, "ob:a", "Continuei ate a esquina.",
                                    true), OK);
    ESPERA_IGUAL(uso_carregar_acervo(hal, &ap.estado), OK);

    int antes = pc_emprestados();

    ap.estado.pilha[++ap.estado.profundidade] = TELA_ACERVO;
    ap.estado.cursor = 0;
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_LEITOR);
    ESPERA(pc_emprestados() > antes);

    pc_botao(IN_VOLTAR);
    app_passo(&ap);
    ESPERA_IGUAL(pc_emprestados(), antes);

    // Abrir duas vezes não empresta duas.
    pc_botao(IN_OK);
    app_passo(&ap);
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA(pc_emprestados() <= antes + 1);
    TERMINA();
}

void t_leitor_usa_flash_na_capa_e_parcial_entre_textos(void)
{
    COMECA("leitor · capa usa flash curto; páginas de texto usam parcial");
    liga_no_acervo();
    poe_obra("ob:parcial", "Livro", OBRA_AQUI);
    static char longo[3000];
    longo[0] = '\0';
    for (int i = 0; i < 50; i++)
        strncat(longo, "Uma pagina precisa continuar leve e legivel. ",
                sizeof longo - strlen(longo) - 1);
    ESPERA_IGUAL(acervo_grava_texto(hal, "ob:parcial", longo, true), OK);
    ESPERA_IGUAL(uso_carregar_acervo(hal, &ap.estado), OK);
    ap.estado.pilha[++ap.estado.profundidade] = TELA_ACERVO;
    ap.estado.cursor = 0;
    pc_botao(IN_OK); app_passo(&ap);

    int flashes = pc_pinturas(PINTURA_TELA_NOVA);
    pc_botao(IN_DIR); app_passo(&ap);
    ESPERA(pc_pinturas(PINTURA_TELA_NOVA) > flashes);

    int antes = pc_pinturas(PINTURA_FOCO);
    pc_botao(IN_DIR); app_passo(&ap);
    ESPERA(pc_pinturas(PINTURA_FOCO) > antes);

    pc_botao(IN_ESQ); app_passo(&ap);       // texto 2 → texto 1
    flashes = pc_pinturas(PINTURA_TELA_NOVA);
    pc_botao(IN_ESQ); app_passo(&ap);       // texto 1 → capa
    ESPERA(pc_pinturas(PINTURA_TELA_NOVA) > flashes);
    TERMINA();
}

void t_leitor_menu_tem_alinhamento(void)
{
    COMECA("leitor · a gaveta oferece alinhamento do texto");
    liga_no_acervo();
    ap.estado.profundidade = 1;
    ap.estado.pilha[1] = TELA_LEITOR;
    vista_menu_t v;
    vista_menu(&ap.estado, 8, &v);
    bool achou = false;
    for (int i = 0; i < v.n; i++)
        if (strcmp(v.linhas[i].texto, "Alinhamento") == 0) achou = true;
    ESPERA(achou);
    TERMINA();
}
