// firmware/testes/t_liga.c — o aparelho liga, recebe evento e não trava.
#include "teste.h"

static app_t ap;   // estático: ENGENHARIA §3

#define HOJE ((data_t){2026, 8, 18})

// Escreve o meta cru: exercita o parser junto.
static void tarefa_no_cartao(const char *id, const char *titulo)
{
    char caminho[128], json[256];
    snprintf(caminho, sizeof caminho,
             "/TINTO/itens/2026-08-18/%s/meta.json", id);
    snprintf(json, sizeof json,
             "{\"v\":1,\"t\":\"%s\",\"h\":\"\",\"tp\":2,\"o\":0,"
             "\"ok\":false,\"m\":false,\"d\":\"\"}", titulo);
    pc_poe_arquivo(caminho, json);
}

void t_liga_e_desenha(void)
{
    COMECA("liga, desenha a página e o seletor, e não trava");

    const hal_t *hal = pc_liga();
    app_liga(&ap, hal);
    app_passo(&ap);

    // DOIS quadros: a página sem o seletor no completo, e o seletor num parcial
    // (EINK §5.5).
    ESPERA_IGUAL(pc_quadros(), 2);

    int l = 0, a = 0;
    pc_tela(&l, &a);
    ESPERA_IGUAL(l, TELA_L);
    ESPERA_IGUAL(a, TELA_A);

    TERMINA();
}

void t_tres_eventos(void)
{
    COMECA("três eventos entram e o estado responde");

    const hal_t *hal = pc_liga();
    pc_relogio(HOJE, 9, 14);
    tarefa_no_cartao("0900-ipva",  "Pagar IPVA");
    tarefa_no_cartao("0901-luz",   "Conta de luz");
    tarefa_no_cartao("0902-pasta", "Comprar pasta térmica");

    app_liga(&ap, hal);
    app_passo(&ap);
    ENTRA_NA_AGENDA(&ap);

    pc_botao(IN_BAIXO);
    pc_botao(IN_BAIXO);
    pc_botao(IN_CIMA);
    app_passo(&ap);

    // Quatro: o OK que abre a Agenda também é evento.
    ESPERA_IGUAL(ap.estado.eventos_vistos, 4);
    ESPERA_IGUAL(ap.estado.cursor, 1);

    TERMINA();
}

// O cursor para na última linha que existe.
void t_cursor_para_na_ultima_linha(void)
{
    COMECA("o cursor para na última linha que existe");

    const hal_t *hal = pc_liga();
    pc_relogio(HOJE, 9, 14);
    tarefa_no_cartao("0900-ipva", "Pagar IPVA");
    tarefa_no_cartao("0901-luz",  "Conta de luz");

    app_liga(&ap, hal);
    ENTRA_NA_AGENDA(&ap);
    for (int i = 0; i < 20; i++) pc_botao(IN_BAIXO);
    app_passo(&ap);

    // Duas tarefas, duas linhas: índice máximo 1.
    ESPERA_IGUAL(ap.estado.cursor, 1);

    TERMINA();
}

// A Agenda nasce em repouso; subir além do topo volta a ele (e libera ◀▶
// para os dias).
void t_cursor_nao_passa_do_topo(void)
{
    COMECA("subir além do topo devolve a home ao repouso");

    const hal_t *hal = pc_liga();
    app_liga(&ap, hal);
    ENTRA_NA_AGENDA(&ap);

    for (int i = 0; i < 5; i++) pc_botao(IN_CIMA);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.cursor, -1);
    TERMINA();
}

// A gaveta é pop-over: fechar não é voltar.
void t_menu_abre_e_fecha_a_gaveta(void)
{
    COMECA("RN-3F · MENU abre a gaveta por cima, sem empilhar");

    const hal_t *hal = pc_liga();
    app_liga(&ap, hal);
    app_passo(&ap);

    // Dentro de uma área: na Home o MENU não abre nada.
    ENTRA_NA_AGENDA(&ap);

    pc_botao(IN_MENU);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_MENU);
    ESPERA_IGUAL(ap.estado.profundidade, 1);   // a pilha não mexeu

    pc_botao(IN_MENU);          // de novo: fecha
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_NADA);

    TERMINA();
}

// O BACK tira da frente o que está por cima, depois volta de tela.
void t_back_fecha_a_gaveta_antes_de_voltar(void)
{
    COMECA("o BACK fecha a gaveta antes de voltar de tela");

    const hal_t *hal = pc_liga();
    pc_relogio(HOJE, 9, 14);
    tarefa_no_cartao("0900-ipva", "Pagar IPVA");
    app_liga(&ap, hal);
    ENTRA_NA_AGENDA(&ap);

    // Na Agenda, onde a gaveta existe.
    pc_botao(IN_MENU);          // abre a gaveta por cima
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.profundidade, 1);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_MENU);

    pc_botao(IN_VOLTAR);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_NADA);
    ESPERA_IGUAL(ap.estado.profundidade, 1);   // ainda na Agenda
    ESPERA_IGUAL(ap.estado.pilha[1], TELA_AGENDA);

    pc_botao(IN_VOLTAR);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.profundidade, 0);   // só então volta à Home

    TERMINA();
}

// RN-3F: o ◀ não volta.
void t_esquerda_nao_volta(void)
{
    COMECA("RN-3F · ◀ não volta um nível — quem volta é o BACK");

    const hal_t *hal = pc_liga();
    pc_relogio(HOJE, 9, 14);
    tarefa_no_cartao("0900-ipva", "Pagar IPVA");
    app_liga(&ap, hal);
    ENTRA_NA_AGENDA(&ap);

    pc_botao(IN_BAIXO);
    pc_botao(IN_BAIXO);
    pc_botao(IN_DIR);           // entra na tarefa
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.profundidade, 2);

    pc_botao(IN_ESQ);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.profundidade, 2);   // continua onde estava

    pc_botao(IN_VOLTAR);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.profundidade, 1);   // volta à Agenda, não à Home

    TERMINA();
}

// RN-38: BACK segurado volta à Home, de qualquer profundidade.
void t_back_segurado_volta_pra_home(void)
{
    COMECA("RN-38 · BACK segurado volta pra Home de qualquer profundidade");

    const hal_t *hal = pc_liga();
    pc_relogio(HOJE, 9, 14);
    tarefa_no_cartao("0900-ipva",  "Pagar IPVA");
    tarefa_no_cartao("0901-luz",   "Conta de luz");
    tarefa_no_cartao("0902-pasta", "Comprar pasta térmica");

    app_liga(&ap, hal);
    ENTRA_NA_AGENDA(&ap);
    pc_botao(IN_BAIXO);
    pc_botao(IN_BAIXO);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.cursor, 2);

    pc_botao(IN_DIR);           // entra no item
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.profundidade, 2);

    pc_segura(IN_VOLTAR, 800);  // segurado
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.profundidade, 0);
    ESPERA_IGUAL(ap.estado.cursor, 0);

    // E a raiz é a HOME.
    ESPERA_IGUAL(ap.estado.pilha[0], TELA_HOME);

    TERMINA();
}

void t_cartao_le_o_que_escreveu(void)
{
    COMECA("o cartão de mentira devolve o que foi escrito");

    const hal_t *hal = pc_liga();
    pc_poe_arquivo("/TINTO/itens/2026-08-14/0914-nota/meta.json",
                     "{\"v\":1,\"t\":\"ideia do painel\"}");

    char buf[256];
    ESPERA_IGUAL(hal->ler("/TINTO/itens/2026-08-14/0914-nota/meta.json",
                          buf, sizeof buf), OK);
    ESPERA_TEXTO(buf, "{\"v\":1,\"t\":\"ideia do painel\"}");

    ESPERA_IGUAL(hal->ler("/TINTO/nao-existe", buf, sizeof buf), ERR_ARQUIVO);

    TERMINA();
}

void t_docar_chega_no_estado(void)
{
    COMECA("docar chega no estado pelo evento, não por consulta");

    const hal_t *hal = pc_liga();
    app_liga(&ap, hal);
    ESPERA(!ap.estado.docado);

    pc_docar(true);
    app_passo(&ap);
    ESPERA(ap.estado.docado);

    TERMINA();
}

// ── a barra acompanha a carga, por FAIXA ────────────────────────────
// Quatro estados de barra: um quadro por ponto percentual seria à toa.
void t_a_carga_atualiza_a_topbar_por_faixa(void)
{
    static app_t ap;
    COMECA("a carga acompanha o fuel gauge, e redesenha por faixa");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 25}, 9, 14);
    pc_bateria(88);
    app_liga(&ap, hal);
    app_passo(&ap);

    pc_tick();
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.bateria, 88);

    // Dentro da mesma faixa, nada redesenha.
    pc_bateria(80);
    pc_tick();
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.bateria, 80);
    int quadros = pc_quadros();

    pc_bateria(76);
    pc_tick();
    app_passo(&ap);
    ESPERA_IGUAL(pc_quadros(), quadros);

    // Trocou de faixa: um quadro.
    pc_bateria(40);
    pc_tick();
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.bateria, 40);

    // "Não sei" mantém o último valor.
    pc_bateria(-1);
    pc_tick();
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.bateria, 40);
    TERMINA();
}

// A gaveta é o submenu DA PÁGINA: na Agenda tem o calendário; no
// calendário, nada; no detalhe, as ações do item.
void t_gaveta_e_submenu_da_pagina(void)
{
    COMECA("a gaveta abre na Agenda e no detalhe; no calendário, não");

    const hal_t *hal = pc_liga();
    pc_relogio(HOJE, 9, 14);
    tarefa_no_cartao("0900-ipva", "Pagar IPVA");
    app_liga(&ap, hal);
    ENTRA_NA_AGENDA(&ap);

    // Na Agenda: abre.
    pc_botao(IN_MENU);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_MENU);
    pc_botao(IN_MENU);
    app_passo(&ap);

    // No detalhe: renomear e apagar.
    pc_botao(IN_BAIXO);
    pc_botao(IN_BAIXO);
    pc_botao(IN_DIR);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_NOTA);

    pc_botao(IN_MENU);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_ACOES);
    pc_botao(IN_MENU);
    app_passo(&ap);

    // No calendário: não.
    pc_botao(IN_VOLTAR);
    app_passo(&ap);
    ap.estado.pilha[ap.estado.profundidade] = TELA_CALENDARIO;
    pc_botao(IN_MENU);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_NADA);

    TERMINA();
}

// Apagar e renomear abrem pelo MENU (o ▶ é "entra no item", RN-37).
void t_o_menu_abre_as_acoes_do_item(void)
{
    COMECA("no detalhe, o MENU abre renomear e apagar");

    const hal_t *hal = pc_liga();
    pc_relogio(HOJE, 9, 14);
    tarefa_no_cartao("0900-ipva", "Pagar IPVA");
    app_liga(&ap, hal);
    ap.estado.rede = REDE_LIGADA;
    ENTRA_NA_AGENDA(&ap);

    pc_botao(IN_BAIXO);
    pc_botao(IN_BAIXO);
    pc_botao(IN_DIR);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_NOTA);

    pc_botao(IN_MENU);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_ACOES);

    // E fecha no mesmo botão (RN-3F).
    pc_botao(IN_MENU);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_NADA);

    TERMINA();
}
