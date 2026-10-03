#include "teste.h"
#include "dado/xadrez.h"
#include "jogos/xadrez_maquina.h"
#include "vista/xadrez.h"

void t_xadrez_da_home_ao_primeiro_lance_local(void)
{
    COMECA("xadrez · Home, Jogos, partida local e primeiro lance");
    const hal_t *hal = pc_liga();
    app_t ap;
    app_liga(&ap, hal);

    ap.estado.lancador = 2;
    pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_JOGOS);

    int contraste = pc_pinturas(PINTURA_TELA_NOVA);
    pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_XADREZ);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_MODOS);
    ESPERA(pc_pinturas(PINTURA_TELA_NOVA) > contraste);

    pc_botao(IN_BAIXO); pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_PREPARAR_LOCAL);
    pc_botao(IN_BAIXO); pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_TABULEIRO);

    ap.estado.xadrez.cursor = XZ_E2;
    pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.origem, XZ_E2);
    pc_botao(IN_CIMA); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.cursor, XZ_E3);
    pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(xadrez_tipo(xadrez_peca_em(&ap.estado.xadrez.posicao, XZ_E3)),
                  XZ_PEAO);
    ESPERA_IGUAL(ap.estado.xadrez.posicao.turno, XZ_PRETAS);
    TERMINA();
}

static void entra_xadrez(app_t *ap)
{
    ap->estado.lancador = 2;
    pc_botao(IN_OK); app_passo(ap);
    pc_botao(IN_OK); app_passo(ap);
}

static void inicia_local(app_t *ap)
{
    pc_botao(IN_BAIXO); pc_botao(IN_OK); app_passo(ap);
    pc_botao(IN_BAIXO); pc_botao(IN_OK); app_passo(ap);
}

void t_xadrez_saida_preserva_ou_abandona_partida(void)
{
    COMECA("xadrez · sair permite continuar, salvar ou abandonar");
    const hal_t *hal = pc_liga();
    app_t ap; app_liga(&ap, hal); entra_xadrez(&ap);
    inicia_local(&ap);
    ap.estado.xadrez.cursor = XZ_E2;
    pc_botao(IN_OK); pc_botao(IN_CIMA); pc_botao(IN_OK); app_passo(&ap);
    ESPERA(pc_tem_arquivo("/TINTO/jogos/xadrez.json"));

    pc_botao(IN_VOLTAR); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_SAIR);
    pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_TABULEIRO);
    pc_botao(IN_VOLTAR); pc_botao(IN_BAIXO); pc_botao(IN_BAIXO); pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_ABANDONAR);
    pc_botao(IN_BAIXO); pc_botao(IN_OK); app_passo(&ap);
    ESPERA(!pc_tem_arquivo("/TINTO/jogos/xadrez.json"));
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_JOGOS);
    TERMINA();
}

void t_xadrez_salvamento_corrompido_tem_saida_segura(void)
{
    COMECA("xadrez · salvamento corrompido não vira posição falsa");
    const hal_t *hal = pc_liga();
    pc_poe_arquivo("/TINTO/jogos/xadrez.json", "{quebrado}");
    app_t ap; app_liga(&ap, hal); entra_xadrez(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_ERRO);
    pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_MODOS);
    ESPERA(!pc_tem_arquivo("/TINTO/jogos/xadrez.json"));
    TERMINA();
}

void t_xadrez_back_limpa_ao_trocar_tabuleiro_e_saida(void)
{
    COMECA("xadrez · BACK troca superfícies com limpeza de tela nova");
    const hal_t *hal = pc_liga();
    app_t ap; app_liga(&ap, hal); entra_xadrez(&ap);
    inicia_local(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_TABULEIRO);

    int novas = pc_pinturas(PINTURA_TELA_NOVA);
    pc_botao(IN_VOLTAR); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_SAIR);
    ESPERA(pc_pinturas(PINTURA_TELA_NOVA) > novas);

    novas = pc_pinturas(PINTURA_TELA_NOVA);
    pc_botao(IN_VOLTAR); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_TABULEIRO);
    ESPERA(pc_pinturas(PINTURA_TELA_NOVA) > novas);
    TERMINA();
}

void t_xadrez_back_segurado_volta_para_home(void)
{
    COMECA("xadrez · BACK segurado continua sendo a saída global");
    const hal_t *hal = pc_liga();
    app_t ap; app_liga(&ap, hal); entra_xadrez(&ap);
    inicia_local(&ap);

    pc_segura(IN_VOLTAR, 800); app_passo(&ap);

    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_HOME);
    ESPERA_IGUAL(ap.estado.profundidade, 0);
    TERMINA();
}

void t_xadrez_maquina_responde_e_salva_sem_bloquear_back(void)
{
    COMECA("xadrez · máquina responde, salva e não bloqueia o BACK");
    const hal_t *hal = pc_liga();
    app_t ap; app_liga(&ap, hal); entra_xadrez(&ap);

    pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_PREPARAR_MAQUINA);
    ESPERA_IGUAL(ap.estado.xadrez.dificuldade, XZM_MEDIA);
    pc_botao(IN_BAIXO); pc_botao(IN_DIR); /* difícil, para manter busca fatiada */
    pc_botao(IN_BAIXO); pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_TABULEIRO);
    ESPERA_IGUAL(ap.estado.xadrez.modo, XZ_MODO_MAQUINA);

    ap.estado.xadrez.cursor = XZ_E2;
    pc_botao(IN_OK); pc_botao(IN_CIMA); pc_botao(IN_OK); app_passo(&ap);
    ESPERA(ap.estado.xadrez.maquina_pensando);
    ESPERA_IGUAL(ap.estado.xadrez.posicao.turno, XZ_PRETAS);

    pc_botao(IN_VOLTAR); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_SAIR);
    pc_botao(IN_VOLTAR); app_passo(&ap);
    for (int i = 0; i < 200 && ap.estado.xadrez.maquina_pensando; i++)
        app_passo(&ap);
    ESPERA(!ap.estado.xadrez.maquina_pensando);
    ESPERA_IGUAL(ap.estado.xadrez.posicao.turno, XZ_BRANCAS);
    ESPERA(pc_tem_arquivo("/TINTO/jogos/xadrez.json"));
    TERMINA();
}

void t_xadrez_maquina_com_pretas_da_pessoa_joga_primeiro(void)
{
    COMECA("xadrez · escolhendo pretas, a máquina abre a partida");
    const hal_t *hal = pc_liga();
    app_t ap; app_liga(&ap, hal); entra_xadrez(&ap);
    pc_botao(IN_OK); app_passo(&ap);
    pc_botao(IN_DIR); pc_botao(IN_BAIXO); pc_botao(IN_BAIXO);
    pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.cor_humana, XZ_PRETAS);
    ESPERA_IGUAL(ap.estado.xadrez.cor_baixo, XZ_PRETAS);
    for (int i = 0; i < 200 && ap.estado.xadrez.maquina_pensando; i++)
        app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.posicao.turno, XZ_PRETAS);
    ESPERA_IGUAL(ap.estado.xadrez.posicao.numero_lance, 1);
    TERMINA();
}

void t_xadrez_maquina_dificil_nao_espera_dezenas_de_voltas(void)
{
    COMECA("xadrez V4 · busca difícil termina em até quinze voltas");
    const hal_t *hal = pc_liga();
    app_t ap; app_liga(&ap, hal); entra_xadrez(&ap);
    pc_botao(IN_OK); app_passo(&ap);
    pc_botao(IN_DIR);                /* pessoa joga de pretas */
    pc_botao(IN_BAIXO); pc_botao(IN_DIR); /* difícil */
    pc_botao(IN_BAIXO); pc_botao(IN_OK); app_passo(&ap);
    ESPERA(ap.estado.xadrez.maquina_pensando);

    int voltas = 0;
    while (ap.estado.xadrez.maquina_pensando && voltas < 100) {
        app_passo(&ap);
        voltas++;
    }
    ESPERA(voltas <= 15);
    ESPERA(!ap.estado.xadrez.maquina_pensando);
    TERMINA();
}

void t_xadrez_continuar_so_aparece_com_salvamento_valido(void)
{
    COMECA("xadrez · continuar só aparece com a última partida válida");
    const hal_t *hal = pc_liga();
    app_t primeira; app_liga(&primeira, hal); entra_xadrez(&primeira);
    inicia_local(&primeira);
    primeira.estado.xadrez.cursor = XZ_E2;
    pc_botao(IN_OK); pc_botao(IN_CIMA); pc_botao(IN_OK); app_passo(&primeira);

    app_t reinicio; app_liga(&reinicio, hal); entra_xadrez(&reinicio);
    ESPERA_IGUAL(reinicio.estado.xadrez.pagina, XZ_PAG_INICIO);
    ESPERA(reinicio.estado.xadrez.tem_salva);
    pc_botao(IN_OK); app_passo(&reinicio);
    ESPERA_IGUAL(reinicio.estado.xadrez.pagina, XZ_PAG_TABULEIRO);
    ESPERA_IGUAL(xadrez_tipo(xadrez_peca_em(
        &reinicio.estado.xadrez.posicao, XZ_E3)), XZ_PEAO);
    TERMINA();
}

void t_xadrez_cancelar_promocao_nao_sai_da_partida(void)
{
    COMECA("xadrez · cancelar promoção volta à escolha do lance");
    const hal_t *hal = pc_liga();
    app_t ap; app_liga(&ap, hal); entra_xadrez(&ap);
    inicia_local(&ap);

    xadrez_pos_t *p = &ap.estado.xadrez.posicao;
    memset(p, 0, sizeof *p);
    p->casa[XZ_CASA('e', 1)] = XZ_REI;
    p->casa[XZ_CASA('e', 8)] = XZ_REI | 8;
    p->casa[XZ_CASA('a', 7)] = XZ_PEAO;
    p->en_passant = -1;
    p->numero_lance = 1;
    ap.estado.xadrez.cursor = XZ_CASA('a', 7);

    pc_botao(IN_OK); pc_botao(IN_CIMA); pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_PROMOCAO);
    ESPERA_IGUAL(ap.estado.xadrez.origem, XZ_CASA('a', 7));

    pc_botao(IN_VOLTAR); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_XADREZ);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_TABULEIRO);
    ESPERA_IGUAL(ap.estado.xadrez.origem, XZ_CASA('a', 7));

    pc_botao(IN_VOLTAR); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_TABULEIRO);
    ESPERA_IGUAL(ap.estado.xadrez.origem, -1);
    TERMINA();
}

void t_xadrez_v3_opcoes_menu_giro_e_historico(void)
{
    COMECA("xadrez V3 · opções, menu, giro e histórico são alcançáveis");
    const hal_t *hal = pc_liga();
    app_t ap; app_liga(&ap, hal); entra_xadrez(&ap);

    pc_botao(IN_BAIXO); pc_botao(IN_BAIXO); pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_OPCOES);
    pc_botao(IN_BAIXO); pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.orientacao, XZ_HORIZONTAL);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_MODOS);

    inicia_local(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.cor_baixo, XZ_BRANCAS);
    int telas_novas = pc_pinturas(PINTURA_TELA_NOVA);
    pc_botao(IN_MENU); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_MENU);
    pc_botao(IN_ESQ); pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_TABULEIRO);
    ESPERA_IGUAL(ap.estado.xadrez.cor_baixo, XZ_BRANCAS);
    ESPERA_IGUAL(ap.estado.xadrez.orientacao, XZ_HORIZONTAL_ESQUERDA);
    ESPERA(pc_pinturas(PINTURA_TELA_NOVA) > telas_novas);

    pc_botao(IN_MENU); app_passo(&ap);
    pc_botao(IN_DIR); pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.orientacao, XZ_VERTICAL);

    pc_botao(IN_MENU); app_passo(&ap);
    pc_botao(IN_BAIXO); pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.orientacao, XZ_HORIZONTAL_DIREITA);

    pc_botao(IN_MENU); pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_HISTORICO);
    ESPERA_IGUAL(ap.estado.xadrez.historico_total, 0);
    pc_botao(IN_VOLTAR); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_TABULEIRO);
    TERMINA();
}

void t_xadrez_v3_confirma_empate_e_abandono(void)
{
    COMECA("xadrez V3 · empate e abandono exigem confirmação");
    const hal_t *hal = pc_liga();
    app_t ap; app_liga(&ap, hal); entra_xadrez(&ap); inicia_local(&ap);

    ap.estado.xadrez.cursor = XZ_E2;
    pc_botao(IN_OK); pc_botao(IN_CIMA); pc_botao(IN_OK); app_passo(&ap);

    pc_botao(IN_MENU); pc_botao(IN_BAIXO); pc_botao(IN_BAIXO);
    pc_botao(IN_BAIXO);
    pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_EMPATE);
    pc_botao(IN_BAIXO); pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.resultado, XZ_EMPATE_ACORDO);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_RESULTADO);

    app_t reinicio; app_liga(&reinicio, hal); entra_xadrez(&reinicio);
    ESPERA_IGUAL(reinicio.estado.xadrez.resultado, XZ_EMPATE_ACORDO);
    ESPERA_IGUAL(reinicio.estado.xadrez.pagina, XZ_PAG_FINAL);

    ap.estado.xadrez.pagina = XZ_PAG_TABULEIRO;
    ap.estado.xadrez.resultado = XZ_EM_CURSO;
    pc_botao(IN_VOLTAR); pc_botao(IN_BAIXO); pc_botao(IN_BAIXO);
    pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_ABANDONAR);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_XADREZ);
    pc_botao(IN_VOLTAR); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_TABULEIRO);
    TERMINA();
}

void t_xadrez_v3_mate_mostra_tabuleiro_antes_do_resultado(void)
{
    COMECA("xadrez V3 · mate preserva a posição antes do resultado");
    const hal_t *hal = pc_liga();
    app_t ap; app_liga(&ap, hal); entra_xadrez(&ap); inicia_local(&ap);
    xadrez_pos_t *p = &ap.estado.xadrez.posicao;
    ESPERA(xadrez_joga(p, (xadrez_mov_t){XZ_CASA('f',2), XZ_CASA('f',3), 0, 0}));
    ESPERA(xadrez_joga(p, (xadrez_mov_t){XZ_CASA('e',7), XZ_CASA('e',5), 0, 0}));
    ESPERA(xadrez_joga(p, (xadrez_mov_t){XZ_CASA('g',2), XZ_CASA('g',4), 0, 0}));
    ap.estado.xadrez.origem = XZ_CASA('d', 8);
    ap.estado.xadrez.cursor = XZ_CASA('h', 4);
    pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.resultado, XZ_MATE_PRETAS);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_FINAL);
    pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_RESULTADO);
    TERMINA();
}

void t_xadrez_revanche_repete_opcoes_e_troca_as_cores(void)
{
    COMECA("xadrez · revanche repete opções, troca cores e começa limpa");
    const hal_t *hal = pc_liga();
    app_t ap; app_liga(&ap, hal); entra_xadrez(&ap);
    pc_botao(IN_OK); app_passo(&ap);
    pc_botao(IN_BAIXO); pc_botao(IN_DIR); /* difícil */
    pc_botao(IN_BAIXO); pc_botao(IN_OK); app_passo(&ap);

    xadrez_app_t *x = &ap.estado.xadrez;
    x->pagina = XZ_PAG_RESULTADO;
    x->resultado = XZ_MATE_BRANCAS;
    x->menu_cursor = 0;
    x->mostrar_ajuda = true;
    pc_botao(IN_OK); app_passo(&ap);

    ESPERA_IGUAL(x->pagina, XZ_PAG_TABULEIRO);
    ESPERA_IGUAL(x->modo, XZ_MODO_MAQUINA);
    ESPERA_IGUAL(x->dificuldade, XZM_DIFICIL);
    ESPERA_IGUAL(x->cor_humana, XZ_PRETAS);
    ESPERA_IGUAL(x->cor_baixo, XZ_PRETAS);
    ESPERA_IGUAL(x->resultado, XZ_EM_CURSO);
    ESPERA_IGUAL(x->n_lances, 0);
    ESPERA_IGUAL(x->placar_a2, 2);
    ESPERA_IGUAL(x->placar_b2, 0);
    ESPERA(x->mostrar_ajuda);
    ESPERA(x->maquina_pensando);
    ESPERA(x->tem_salva);
    ESPERA_IGUAL(xadrez_tipo(xadrez_peca_em(&x->posicao, XZ_E2)), XZ_PEAO);

    app_t reinicio; app_liga(&reinicio, hal); entra_xadrez(&reinicio);
    ESPERA_IGUAL(reinicio.estado.xadrez.placar_a2, 2);
    ESPERA_IGUAL(reinicio.estado.xadrez.placar_b2, 0);

    pc_botao(IN_OK); app_passo(&reinicio);
    reinicio.estado.xadrez.pagina = XZ_PAG_RESULTADO;
    reinicio.estado.xadrez.resultado = XZ_EMPATE_ACORDO;
    reinicio.estado.xadrez.menu_cursor = 2;
    pc_botao(IN_OK); app_passo(&reinicio);
    ESPERA_IGUAL(reinicio.estado.xadrez.pagina, XZ_PAG_MODOS);
    ESPERA_IGUAL(reinicio.estado.xadrez.placar_a2, 0);
    ESPERA_IGUAL(reinicio.estado.xadrez.placar_b2, 0);
    ESPERA(!pc_tem_arquivo("/TINTO/jogos/xadrez.json"));
    TERMINA();
}

void t_xadrez_v3_direcional_acompanha_horizontal_e_giro(void)
{
    COMECA("xadrez V3 · direcional acompanha o aparelho e o tabuleiro");
    const hal_t *hal = pc_liga();
    app_t ap; app_liga(&ap, hal); entra_xadrez(&ap); inicia_local(&ap);
    ap.estado.xadrez.orientacao = XZ_HORIZONTAL;
    ap.estado.xadrez.cursor = XZ_E2;
    pc_botao(IN_CIMA); app_passo(&ap); /* cima física aponta à esquerda no quadro girado */
    ESPERA_IGUAL(ap.estado.xadrez.cursor, XZ_CASA('d', 2));

    ap.estado.xadrez.cor_baixo = XZ_PRETAS;
    ap.estado.xadrez.posicao.turno = XZ_PRETAS;
    ap.estado.xadrez.cursor = XZ_CASA('e', 7);
    pc_botao(IN_CIMA); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.cursor, XZ_CASA('f', 7));

    /* Na gaveta, baixo acompanha o lado para o qual a tela foi girada. */
    ap.estado.xadrez.pagina = XZ_PAG_MENU;
    ap.estado.xadrez.menu_cursor = 0;
    vista_xadrez_t v;
    vista_xadrez(&ap.estado, &v);
    ESPERA_IGUAL(v.cursor, ap.estado.xadrez.cursor);
    pc_botao(IN_ESQ); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.menu_cursor, 1);

    ap.estado.xadrez.orientacao = XZ_HORIZONTAL_ESQUERDA;
    ap.estado.xadrez.pagina = XZ_PAG_TABULEIRO;
    ap.estado.xadrez.cor_baixo = XZ_BRANCAS;
    ap.estado.xadrez.posicao.turno = XZ_BRANCAS;
    ap.estado.xadrez.cursor = XZ_E2;
    pc_botao(IN_CIMA); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.cursor, XZ_CASA('f', 2));

    ap.estado.xadrez.pagina = XZ_PAG_MENU;
    ap.estado.xadrez.menu_cursor = 0;
    pc_botao(IN_DIR); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.menu_cursor, 1);
    TERMINA();
}

void t_xadrez_gaveta_alterna_ajuda_sem_trocar_tela(void)
{
    COMECA("xadrez · gaveta alterna e salva ajuda sem piscada de tela nova");
    const hal_t *hal = pc_liga();
    app_t ap; app_liga(&ap, hal); entra_xadrez(&ap); inicia_local(&ap);
    ESPERA(!ap.estado.xadrez.mostrar_ajuda);

    ap.estado.xadrez.cursor = XZ_E2;
    pc_botao(IN_OK); pc_botao(IN_CIMA); pc_botao(IN_OK); app_passo(&ap);
    int novas = pc_pinturas(PINTURA_TELA_NOVA);

    pc_botao(IN_MENU); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_MENU);
    ESPERA_IGUAL(pc_pinturas(PINTURA_TELA_NOVA), novas);

    pc_botao(IN_BAIXO); pc_botao(IN_BAIXO); pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.xadrez.pagina, XZ_PAG_TABULEIRO);
    ESPERA(ap.estado.xadrez.mostrar_ajuda);
    ESPERA_IGUAL(pc_pinturas(PINTURA_TELA_NOVA), novas);

    app_t reinicio; app_liga(&reinicio, hal); entra_xadrez(&reinicio);
    ESPERA(reinicio.estado.xadrez.mostrar_ajuda);
    TERMINA();
}
