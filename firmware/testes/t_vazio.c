// firmware/testes/t_vazio.c — o que as telas dizem quando não têm nada.
// Painel em branco tem a cara de um quadro que falhou no meio. Estes testes
// contam TINTA no corpo.
#include "teste.h"
#include "vista/menu.h"
#include "vista/bloqueio.h"
#include "uso/uso.h"
#include "dado/cartao.h"
#include "vista/agenda.h"
#include "vista/dia.h"
#include "vista/campos.h"
#include "vista/nota.h"
#include "vista/calendario.h"
#include "ui/agenda.h"
#include "ui/dia.h"
#include "ui/calendario.h"
#include "ui/agua.h"
#include "ui/grid.h"
#include "nucleo/data.h"

static uint8_t memoria[(TELA_L + 7) / 8 * TELA_A];
static bitmap_t bm;
static estado_t e;

static void limpo(void)
{
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje    = (data_t){2026, 8, 26};
    e.hora    = 9; e.minuto = 41;
    e.hora_confiavel = true;
    e.bateria = 78;
    e.cursor  = -1;
    bitmap_liga(&bm, memoria, TELA_L, TELA_A);
}

// Tinta entre a barra e o rodapé (que nunca estão vazios).
static int tinta_no_corpo(void)
{
    int n = 0;
    for (int y = GRID_BARRA_Y + BARRA_A; y < TELA_A - RODAPE_A; y++)
        for (int x = 0; x < TELA_L; x++)
            if (gfx_le(&bm, x, y)) n++;
    return n;
}

// ── a Agenda ────────────────────────────────────────────────────────

void t_a_home_vazia_diz_que_esta_vazia(void)
{
    COMECA("home vazia · a marca d'água é desenhada, não só montada");

    limpo();

    vista_agenda_t v;
    vista_agenda(&e, &v);

    ESPERA_CONTEM(v.vazio, "livre");

    tela_agenda(&bm, &v);

    // E o DESENHO a mostra (a frase morria dentro da struct).
    ESPERA(tinta_no_corpo() > 0);

    TERMINA();
}

// É marca d'água: rarefeita, para não competir com um compromisso.
void t_a_marca_dagua_e_mais_clara_que_a_tinta_cheia(void)
{
    COMECA("marca d'água · esmaecida, senão compete com o compromisso");

    bitmap_liga(&bm, memoria, TELA_L, TELA_A);
    gfx_limpa(&bm, false);
    gfx_texto(&bm, 10, 40, F_CORPO, "Seu dia está livre.");

    int cheia = 0;
    for (int y = 30; y < 70; y++)
        for (int x = 0; x < TELA_L; x++) if (gfx_le(&bm, x, y)) cheia++;

    gfx_limpa(&bm, false);
    ui_agua(&bm, 10, 30, TELA_L - 20, 40, "Seu dia está livre.");

    int agua = 0;
    for (int y = 30; y < 70; y++)
        for (int x = 0; x < TELA_L; x++) if (gfx_le(&bm, x, y)) agua++;

    ESPERA(agua > 0);          // legível: não é apagar, é rarefazer
    ESPERA(agua < cheia);      // e mais clara que a mesma frase em tinta

    TERMINA();
}


// ── o dia ───────────────────────────────────────────────────────────

void t_o_dia_vazio_usa_a_mesma_marca_dagua(void)
{
    COMECA("dia vazio · a mesma frase da home, e não uma linha solta");

    limpo();
    e.dia_visto = e.hoje;

    vista_dia_t v;
    vista_dia(&e, &v);

    // A mesma família de frase da Agenda.
    ESPERA_CONTEM(v.vazio, "Nada marcado");

    tela_dia(&bm, &v);
    ESPERA(tinta_no_corpo() > 0);

    TERMINA();
}

// O dia tem duas seções, e nenhuma é captura.
void t_o_dia_tem_duas_secoes_e_nenhuma_e_captura(void)
{
    COMECA("dia · compromissos e a fazer, o vocabulário de 26/08");

    limpo();
    e.dia_visto = e.hoje;
    e.n_itens = 2;

    e.itens[0].tipo = TIPO_EVENTO;
    e.itens[0].origem = ORIGEM_GOOGLE;
    snprintf(e.itens[0].hora, sizeof e.itens[0].hora, "%s", "15:00");
    snprintf(e.itens[0].titulo, sizeof e.itens[0].titulo, "%s", "Dentista");
    snprintf(e.itens[0].local, sizeof e.itens[0].local, "%s", "R. Bahia, 210");

    e.itens[1].tipo = TIPO_TAREFA;
    snprintf(e.itens[1].titulo, sizeof e.itens[1].titulo, "%s",
             "Comprar pasta térmica");
    // Ela VENCE no dia aberto.
    e.itens[1].vence = e.dia_visto;

    // Nos DOIS caches, como no aparelho.
    e.pendentes[e.n_pendentes++] = e.itens[1];
    e.pendentes_validas = true;

    vista_dia_t v;
    vista_dia(&e, &v);

    ESPERA_IGUAL(v.n_compromissos, 1);
    ESPERA_TEXTO(v.compromissos[0].titulo, "Dentista");
    // O local desce como subtítulo.
    ESPERA_TEXTO(v.compromissos[0].sub, "R. Bahia, 210");

    ESPERA_IGUAL(v.n_tarefas, 1);
    ESPERA_TEXTO(v.tarefas[0].titulo, "Comprar pasta térmica");

    TERMINA();
}

// ── o calendário ────────────────────────────────────────────────────

// Uma legenda só, a do Google (duas discordando é pior que nenhuma).
void t_o_calendario_tem_uma_legenda_so_e_e_a_do_google(void)
{
    COMECA("calendário · o vocabulário das marcas é o do Google");

    limpo();
    e.dia_visto = e.hoje;

    // A legenda é a CONTAGEM do dia sob o cursor; por isso o dia tem conteúdo.
    item_t *ev = &e.itens[e.n_itens++];
    memset(ev, 0, sizeof *ev);
    snprintf(ev->id,     sizeof ev->id,     "%s", "g:dent");
    snprintf(ev->titulo, sizeof ev->titulo, "%s", "Dentista");
    snprintf(ev->hora,   sizeof ev->hora,   "%s", "14:00");
    ev->tipo = TIPO_EVENTO;
    ev->dia  = e.hoje;

    item_t *ta = &e.itens[e.n_itens++];
    memset(ta, 0, sizeof *ta);
    snprintf(ta->id,     sizeof ta->id,     "%s", "0900-ipva");
    snprintf(ta->titulo, sizeof ta->titulo, "%s", "Pagar IPVA");
    ta->tipo  = TIPO_TAREFA;
    ta->dia   = e.hoje;
    ta->vence = e.dia_visto;   // é o prazo que a põe num dia
    e.pendentes[e.n_pendentes++] = *ta;
    e.pendentes_validas = true;

    e.itens_validos = true;

    vista_cal_t v;
    vista_calendario(&e, &v);

    ESPERA_CONTEM(v.contagem, "evento");
    ESPERA_CONTEM(v.contagem, "tarefa");

    ESPERA_SEM(v.contagem,    "captura");
    ESPERA_SEM(v.rodape_esq,  "captura");
    ESPERA_SEM(v.rodape_dir,  "captura");

    // A saída na esquerda do rodapé.
    ESPERA_CONTEM(v.rodape_esq, "BACK");

    TERMINA();
}

// A tira mostra o que o dia sob o cursor tem.
void t_a_tira_mostra_o_que_o_dia_sob_o_cursor_tem(void)
{
    COMECA("calendário · a tira diz o que há no dia sob o cursor");

    limpo();
    e.dia_visto = e.hoje;
    e.marcas_validas = true;
    e.marcas_ano = 2026; e.marcas_mes = 8;
    e.marcas_evento = 1u << (26 - 1);

    e.n_itens = 2;
    e.itens[0].tipo = TIPO_EVENTO;
    snprintf(e.itens[0].hora, sizeof e.itens[0].hora, "%s", "15:00");
    snprintf(e.itens[0].titulo, sizeof e.itens[0].titulo, "%s", "Dentista");
    e.itens[1].tipo  = TIPO_TAREFA;
    e.itens[1].vence = e.dia_visto;   // é o prazo que a põe neste dia
    snprintf(e.itens[1].titulo, sizeof e.itens[1].titulo, "%s", "Pagar IPVA");

    // Nos DOIS caches, como no aparelho.
    e.pendentes[e.n_pendentes++] = e.itens[1];
    e.pendentes_validas = true;

    vista_cal_t v;
    vista_calendario(&e, &v);

    ESPERA_CONTEM(v.tira_rot, "26");
    ESPERA_CONTEM(v.tira_rot, "hoje");
    ESPERA_IGUAL(v.n_tira, 1);
    ESPERA_CONTEM(v.tira[0], "Dentista");
    ESPERA_CONTEM(v.tira[0], "15:00");
    // A tarefa vira contagem.
    ESPERA_CONTEM(v.tira_mais, "1 por fazer");
    ESPERA_TEXTO(v.vazio, "");

    TERMINA();
}

// O mês vazio diz o nome do mês; a grade continua.
void t_o_mes_vazio_diz_o_nome_do_mes(void)
{
    COMECA("calendário · mês vazio, e ele diz qual mês");

    limpo();
    e.dia_visto = (data_t){2026, 9, 3};

    vista_cal_t v;
    vista_calendario(&e, &v);

    ESPERA_CONTEM(v.vazio, "setembro");
    ESPERA_IGUAL(v.n_tira, 0);

    tela_calendario(&bm, &v);
    ESPERA(tinta_no_corpo() > 0);

    TERMINA();
}

// ── a tira acompanha o cursor ───────────────────────────────────────
// Andar muda o dia visto e invalida o cache; sem recarregar antes do
// desenho, a tira mostraria o dia anterior.
void t_a_tira_acompanha_o_cursor_na_grade(void)
{
    COMECA("calendário · andar na grade muda a tira junto");

    static app_t ap;
    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 26}, 9, 41);
    app_liga(&ap, hal);
    app_passo(&ap);

    // Um evento no dia 27, nada no 26.
    item_t ev;
    memset(&ev, 0, sizeof ev);
    snprintf(ev.id, sizeof ev.id, "%s", "g:amanha");
    snprintf(ev.titulo, sizeof ev.titulo, "%s", "Dentista");
    snprintf(ev.hora, sizeof ev.hora, "%s", "15:00");
    ev.tipo = TIPO_EVENTO;
    ev.origem = ORIGEM_GOOGLE;
    ev.dia = (data_t){2026, 8, 27};
    ESPERA_IGUAL(cartao_grava_item(hal, ev.dia, &ev), OK);

    // Abre o calendário pela gaveta da Agenda.
    ENTRA_NA_AGENDA(&ap);
    pc_botao(IN_MENU);
    app_passo(&ap);
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_CALENDARIO);

    vista_cal_t v;
    vista_calendario(&ap.estado, &v);
    ESPERA_IGUAL(v.cursor_dia, 26);
    ESPERA_IGUAL(v.n_tira, 0);

    // ▶ anda um dia; a tira vem junto.
    pc_botao(IN_DIR);
    app_passo(&ap);

    vista_calendario(&ap.estado, &v);
    ESPERA_IGUAL(v.cursor_dia, 27);
    ESPERA_IGUAL(v.n_tira, 1);
    ESPERA_CONTEM(v.tira[0], "Dentista");
    ESPERA_CONTEM(v.tira_rot, "27");

    // E de volta: o 26 continua vazio.
    pc_botao(IN_ESQ);
    app_passo(&ap);
    vista_calendario(&ap.estado, &v);
    ESPERA_IGUAL(v.cursor_dia, 26);
    ESPERA_IGUAL(v.n_tira, 0);

    TERMINA();
}

// ── os menus paginam e dizem a página ───────────────────────────────
// Página inteira: as linhas caem sempre no mesmo lugar.
void t_os_menus_paginam_em_vez_de_deslizar(void)
{
    COMECA("menus · a lista vira páginas, e o contador diz qual");

    vista_menu_t v;
    memset(&v, 0, sizeof v);
    for (int i = 0; i < 9; i++) {
        char nome[8];
        snprintf(nome, sizeof nome, "op %d", i);
        vista_menu_poe(&v, "", ICO_NENHUM, nome, "");
    }

    // Quatro por página: três páginas.
    v.cursor = 0;
    vista_menu_rola(&v, 4);
    ESPERA_IGUAL(v.primeira, 0);
    ESPERA_IGUAL(v.pagina, 1);
    ESPERA_IGUAL(v.paginas, 3);

    // A última linha da página não vira a página.
    v.cursor = 3;
    vista_menu_rola(&v, 4);
    ESPERA_IGUAL(v.primeira, 0);
    ESPERA_IGUAL(v.pagina, 1);

    // A primeira da seguinte salta o BLOCO inteiro.
    v.cursor = 4;
    vista_menu_rola(&v, 4);
    ESPERA_IGUAL(v.primeira, 4);
    ESPERA_IGUAL(v.pagina, 2);

    v.cursor = 8;
    vista_menu_rola(&v, 4);
    ESPERA_IGUAL(v.primeira, 8);
    ESPERA_IGUAL(v.pagina, 3);

    // Lista que cabe não tem contador.
    vista_menu_t curta;
    memset(&curta, 0, sizeof curta);
    vista_menu_poe(&curta, "", ICO_NENHUM, "só uma", "");
    vista_menu_rola(&curta, 4);
    ESPERA_IGUAL(curta.paginas, 1);

    TERMINA();
}

// A LEGENDA pesa: com ela a linha ocupa duas, senão as últimas redes sumiam
// sem página.
void t_a_legenda_pesa_na_conta_da_pagina(void)
{
    COMECA("menus · a legenda ocupa linha, e a conta sabe disso");

    vista_menu_t v;
    memset(&v, 0, sizeof v);
    for (int i = 0; i < 6; i++) {
        char nome[8];
        snprintf(nome, sizeof nome, "op %d", i);
        vista_menu_poe(&v, "", ICO_NENHUM, nome, "");
        snprintf(v.sub[i], sizeof v.sub[0], "%s", "a descrição dela");
    }

    // Seis linhas com legenda: três por página.
    v.cursor = 0;
    vista_menu_rola(&v, 6);
    ESPERA_IGUAL(v.paginas, 2);

    // A última linha está numa página que existe.
    v.cursor = 5;
    vista_menu_rola(&v, 6);
    ESPERA_IGUAL(v.pagina, 2);
    ESPERA(v.primeira <= 5);

    TERMINA();
}

// A página diz onde ACABA (senão a primeira da seguinte aparecia duas
// vezes).
void t_a_pagina_diz_onde_acaba_e_nao_so_onde_comeca(void)
{
    COMECA("menus · a página tem fim, e ele é da mesma conta que o começo");

    vista_menu_t v;
    memset(&v, 0, sizeof v);
    for (int i = 0; i < 9; i++) {
        char nome[8];
        snprintf(nome, sizeof nome, "op %d", i);
        vista_menu_poe(&v, "", ICO_NENHUM, nome, "");
    }

    v.cursor = 0;
    vista_menu_rola(&v, 4);
    ESPERA_IGUAL(v.primeira, 0);
    ESPERA_IGUAL(v.ultima, 4);        // 0,1,2,3 — e a 4 é da página 2

    v.cursor = 4;
    vista_menu_rola(&v, 4);
    ESPERA_IGUAL(v.primeira, 4);
    ESPERA_IGUAL(v.ultima, 8);

    v.cursor = 8;
    vista_menu_rola(&v, 4);
    ESPERA_IGUAL(v.primeira, 8);
    ESPERA_IGUAL(v.ultima, 9);

    TERMINA();
}

// ── a tela bloqueada ────────────────────────────────────────────────
// Informação de relance, nenhum cursor; o Power devolve onde se estava.
void t_a_tela_bloqueada_no_desenho_da_fase_3(void)
{
    COMECA("Bloqueio · a data por extenso, e o que fazer para sair");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = e.dia_visto = (data_t){2026, 9, 1};   // uma terça
    e.hora = 11; e.minuto = 17;
    e.bateria = 81;
    e.hora_confiavel = true;

    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id, sizeof it.id, "%s", "e1");
    snprintf(it.titulo, sizeof it.titulo, "%s", "Dentista");
    snprintf(it.hora, sizeof it.hora, "%s", "14:00");
    snprintf(it.local, sizeof it.local, "%s", "Rua Bahia, 210");
    it.tipo = TIPO_EVENTO;
    it.dia  = e.hoje;
    e.itens[e.n_itens++] = it;
    e.itens_validos = true;

    vista_bloqueio_t v;
    vista_bloqueio(&e, &v);

    ESPERA_TEXTO(v.hora, "11:17");

    // A data POR EXTENSO.
    ESPERA_CONTEM(v.data, "terça");
    ESPERA_CONTEM(v.data, "setembro");

    ESPERA_CONTEM(v.proximo, "Dentista");
    ESPERA_CONTEM(v.onde, "Rua Bahia");

    // E o que fazer para sair.
    ESPERA_CONTEM(v.saida, "ower");

    TERMINA();
}

// Dia livre é informação.
void t_a_tela_bloqueada_diz_dia_livre(void)
{
    COMECA("Bloqueio · sem nada marcado, o vazio vira uma frase calma");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = e.dia_visto = (data_t){2026, 9, 6};   // um domingo
    e.hora = 9; e.minuto = 14;
    e.bateria = 44;
    e.hora_confiavel = true;
    e.itens_validos = true;

    vista_bloqueio_t v;
    vista_bloqueio(&e, &v);

    ESPERA_CONTEM(v.data, "domingo");
    ESPERA_CONTEM(v.proximo, "Dia livre");
    ESPERA_IGUAL(v.n_resto, 0);

    TERMINA();
}

// Andar um dia na grade lê o dia e mais nada: pendentes e marcas não
// dependem do dia visto (custavam meio segundo por seta).
void t_andar_um_dia_na_grade_le_o_dia_e_mais_nada(void)
{
    static app_t ap;
    COMECA("calendário · uma seta custa um dia de cartão, não o mês");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 26}, 9, 41);
    app_liga(&ap, hal);
    app_passo(&ap);

    for (int d = 20; d <= 31; d++) {
        item_t ev;
        memset(&ev, 0, sizeof ev);
        snprintf(ev.id,     sizeof ev.id,     "g:%d", d);
        snprintf(ev.titulo, sizeof ev.titulo, "Compromisso %d", d);
        snprintf(ev.hora,   sizeof ev.hora,   "%s", "15:00");
        ev.tipo   = TIPO_EVENTO;
        ev.origem = ORIGEM_GOOGLE;
        ev.dia    = (data_t){2026, 8, (int8_t)d};
        ESPERA_IGUAL(cartao_grava_item(hal, ev.dia, &ev), OK);
    }

    ENTRA_NA_AGENDA(&ap);
    pc_botao(IN_MENU);
    app_passo(&ap);
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_CALENDARIO);

    // O primeiro quadro paga as marcas do mês; mede-se da segunda seta.
    pc_botao(IN_DIR);
    app_passo(&ap);

    int antes = pc_listagens();
    pc_botao(IN_DIR);
    app_passo(&ap);
    int gastou = pc_listagens() - antes;

    // A tira andou junto.
    vista_cal_t v;
    vista_calendario(&ap.estado, &v);
    ESPERA_IGUAL(v.cursor_dia, 28);
    ESPERA_IGUAL(v.n_tira, 1);

    // A raiz mais a pasta do dia novo.
    ESPERA(gastou < 6);

    TERMINA();
}

// O dia é uma linha do tempo: dia inteiro primeiro, depois por hora.
void t_o_dia_lista_em_ordem_crescente_de_horario(void)
{
    COMECA("dia · os compromissos sobem em ordem de horário");

    limpo();
    e.dia_visto = e.hoje;
    e.n_itens = 4;

    e.itens[0].tipo = TIPO_EVENTO;
    snprintf(e.itens[0].hora,   sizeof e.itens[0].hora,   "%s", "18:30");
    snprintf(e.itens[0].titulo, sizeof e.itens[0].titulo, "%s", "Jantar");

    e.itens[1].tipo = TIPO_EVENTO;
    snprintf(e.itens[1].hora,   sizeof e.itens[1].hora,   "%s", "09:00");
    snprintf(e.itens[1].titulo, sizeof e.itens[1].titulo, "%s", "Reunião");

    e.itens[2].tipo = TIPO_EVENTO;
    e.itens[2].dia_inteiro = true;
    snprintf(e.itens[2].titulo, sizeof e.itens[2].titulo, "%s", "Feriado");

    e.itens[3].tipo = TIPO_EVENTO;
    snprintf(e.itens[3].hora,   sizeof e.itens[3].hora,   "%s", "14:00");
    snprintf(e.itens[3].titulo, sizeof e.itens[3].titulo, "%s", "Dentista");

    vista_dia_t v;
    vista_dia(&e, &v);

    ESPERA_IGUAL(v.n_compromissos, 4);
    ESPERA_TEXTO(v.compromissos[0].titulo, "Feriado");
    ESPERA_TEXTO(v.compromissos[1].titulo, "Reunião");
    ESPERA_TEXTO(v.compromissos[2].titulo, "Dentista");
    ESPERA_TEXTO(v.compromissos[3].titulo, "Jantar");

    // O `indice` acompanha o item, não a posição (o OK abre o item certo).
    ESPERA_IGUAL(v.compromissos[1].indice, 1);
    ESPERA_IGUAL(v.compromissos[3].indice, 0);

    TERMINA();
}

// Item sem título nunca aparece em branco, em nenhuma tela.
void t_item_sem_titulo_nunca_aparece_em_branco(void)
{
    COMECA("nenhuma lista desenha linha em branco por falta de título");

    limpo();
    e.dia_visto = e.hoje;
    e.config.valor[AJUSTE_HORA24] = 1;
    e.n_itens = 1;

    e.itens[0].tipo   = TIPO_EVENTO;
    e.itens[0].origem = ORIGEM_GOOGLE;
    e.itens[0].dia    = e.hoje;
    snprintf(e.itens[0].hora, sizeof e.itens[0].hora, "%s", "15:08");
    e.itens[0].titulo[0] = '\0';

    // No dia.
    vista_dia_t d;
    vista_dia(&e, &d);
    ESPERA_IGUAL(d.n_compromissos, 1);
    ESPERA_TEXTO(d.compromissos[0].titulo, "(Sem título)");

    // Na Agenda.
    vista_agenda_t h;
    vista_agenda(&e, &h);
    ESPERA(h.n_agenda >= 1);
    ESPERA_TEXTO(h.agenda[0].oque, "(Sem título)");

    // E no detalhe.
    e.aberto = e.itens[0];
    e.aberto_valido = true;
    vista_nota_t n;
    vista_nota(&e, &n);
    ESPERA_TEXTO(n.tl, "(Sem título)");

    TERMINA();
}

void t_tarefa_aparece_da_data_ate_o_prazo(void)
{
    COMECA("dia · tarefa permanece visível da data até o prazo");

    limpo();
    e.n_pendentes = 1;
    e.pendentes[0].tipo  = TIPO_TAREFA;
    e.pendentes[0].vence = e.hoje;
    e.pendentes[0].prazo = data_soma_dias(e.hoje, 2);
    snprintf(e.pendentes[0].titulo, sizeof e.pendentes[0].titulo,
             "%s", "Entregar relatório");

    vista_dia_t v;
    e.dia_visto = data_soma_dias(e.hoje, 1);
    vista_dia(&e, &v);
    ESPERA_IGUAL(v.n_tarefas, 1);

    e.dia_visto = data_soma_dias(e.hoje, 3);
    vista_dia(&e, &v);
    ESPERA_IGUAL(v.n_tarefas, 0);

    TERMINA();
}

// Tarefa com HORA é compromisso e não vira atrasada; só um prazo próprio a
// traz para as tarefas.
void t_tarefa_com_hora_nao_vira_atrasada(void)
{
    COMECA("híbrido · com hora fica no dia dela; só o prazo próprio a traz");

    limpo();
    item_t *t = &e.pendentes[e.n_pendentes++];
    t->tipo  = TIPO_TAREFA;
    t->vence = data_soma_dias(e.hoje, -1);             // academia, ontem
    snprintf(t->hora, sizeof t->hora, "%s", "08:00");
    snprintf(t->titulo, sizeof t->titulo, "%s", "Academia");

    // Hoje: nem na Agenda, nem no dia, nem no bloqueio.
    ESPERA(!vista_tarefa_no_dia(&e, t, e.hoje, true));
    ESPERA(!vista_tarefa_no_dia(&e, t, e.hoje, false));
    ESPERA(!vista_tarefa_no_dia(&e, t, t->vence, false));   // lá é régua

    // Com prazo próprio, vale como tarefa com prazo.
    t->prazo = data_soma_dias(e.hoje, 2);
    ESPERA(vista_tarefa_no_dia(&e, t, e.hoje, true));      // acompanha
    ESPERA(!vista_tarefa_no_dia(&e, t, e.hoje, false));    // o dia: só o dele
    ESPERA(vista_tarefa_no_dia(&e, t, t->prazo, false));
    TERMINA();
}

// A gravação não entra na régua: ela já está dentro do item que criou.
void t_a_gravacao_nao_entra_na_regua_do_dia(void)
{
    COMECA("dia · a gravação não vira linha ao lado do evento que ela criou");

    limpo();
    e.dia_visto = e.hoje;
    e.n_itens = 2;

    // O evento que a fala criou.
    e.itens[0].tipo = TIPO_EVENTO;
    snprintf(e.itens[0].hora,   sizeof e.itens[0].hora,   "%s", "15:00");
    snprintf(e.itens[0].titulo, sizeof e.itens[0].titulo, "%s", "Dentista");
    snprintf(e.itens[0].nota,   sizeof e.itens[0].nota,   "%s", "nt:a1");

    // E a gravação: mesma nota, sem título.
    e.itens[1].tipo = TIPO_EVENTO;
    snprintf(e.itens[1].hora, sizeof e.itens[1].hora, "%s", "14:58");
    snprintf(e.itens[1].nota, sizeof e.itens[1].nota, "%s", "nt:a1");
    e.itens[1].titulo[0] = '\0';

    vista_dia_t v;
    vista_dia(&e, &v);

    ESPERA_IGUAL(v.n_compromissos, 1);
    ESPERA_TEXTO(v.compromissos[0].titulo, "Dentista");

    // E não conta como "não coube".
    ESPERA_IGUAL(v.mais_compromissos, 0);

    // Evento do Google sem título continua aparecendo.
    memset(&e.itens[1].nota, 0, sizeof e.itens[1].nota);
    vista_dia(&e, &v);
    ESPERA_IGUAL(v.n_compromissos, 2);

    TERMINA();
}

// O bloqueio mostra o que tem hora só hoje, e as tarefas abertas pela
// regra da Agenda. Lista não entra.
void t_bloqueio_mostra_so_hoje_pelo_nome(void)
{
    COMECA("bloqueio · com hora só hoje; tarefa aberta até ser feita");
    static estado_t e;
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;
    e.hoje = (data_t){2026, 10, 1};
    e.hora = 9; e.minuto = 0; e.hora_confiavel = true;

    struct { tipo_t tp; const char *t, *h; data_t dia; } hora[] = {
        { TIPO_EVENTO, "Dentista", "14:00", {2026, 10, 1} },
        { TIPO_TAREFA, "Academia", "08:00", {2026, 10, 1} },   // já passou
        { TIPO_EVENTO, "Reunião",  "10:00", {2026, 10, 1} },
        { TIPO_EVENTO, "Amanhã",   "10:00", {2026, 10, 2} },   // outro dia
    };
    for (size_t i = 0; i < sizeof hora / sizeof hora[0]; i++) {
        item_t *x = &e.itens[e.n_itens++];
        x->tipo = hora[i].tp; x->dia = hora[i].dia;
        snprintf(x->titulo, sizeof x->titulo, "%s", hora[i].t);
        snprintf(x->hora, sizeof x->hora, "%s", hora[i].h);
    }

    struct { tipo_t tp; const char *t; data_t dia; bool feita; } tar[] = {
        { TIPO_LISTA,  "Compras no mercado", {2026, 10, 1}, false },
        { TIPO_TAREFA, "Falada ontem",       {2026, 9, 30}, false },
        { TIPO_TAREFA, "Já feita",           {2026, 10, 1}, true  },
    };
    for (size_t i = 0; i < sizeof tar / sizeof tar[0]; i++) {
        item_t *x = &e.pendentes[e.n_pendentes++];
        x->tipo = tar[i].tp; x->dia = tar[i].dia; x->feita = tar[i].feita;
        snprintf(x->titulo, sizeof x->titulo, "%s", tar[i].t);
    }
    e.pendentes_validas = true;

    vista_bloqueio_t v;
    vista_bloqueio(&e, &v);
    ESPERA_CONTEM(v.proximo, "Reunião");          // o próximo com hora
    ESPERA_IGUAL(v.n_resto, 2);
    ESPERA_CONTEM(v.resto[0], "Dentista");
    ESPERA_CONTEM(v.resto[1], "Falada ontem");    // a tarefa fica até ser feita
    ESPERA_TEXTO(v.mais, "");

    // Tarefas demais: três linhas e o resto contado.
    for (int i = 0; i < 4; i++) {
        item_t *x = &e.pendentes[e.n_pendentes++];
        x->tipo = TIPO_TAREFA; x->dia = e.hoje;
        snprintf(x->titulo, sizeof x->titulo, "Tarefa %d", i);
    }
    vista_bloqueio(&e, &v);
    ESPERA_IGUAL(v.n_resto, 3);
    ESPERA_TEXTO(v.mais, "+ 3 por fazer");
    TERMINA();
}
