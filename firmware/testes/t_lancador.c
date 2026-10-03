// firmware/testes/t_lancador.c — a Home 2x2: Agenda, Acervo, Jogos e
// Ajustes. Entrar numa área e voltar devolve o cartão de onde se saiu.
#include "teste.h"
#include "tela/mapa.h"
#include "vista/lancador.h"
#include "ui/lancador.h"
#include "ui/chrome.h"
#include "ui/grid.h"
#include "nucleo/data.h"
#include "dado/cartao.h"
#include "vista/agenda.h"

static app_t        ap;
static const hal_t *hal;

#define HOJE ((data_t){2026, 9, 1})

static void liga(void)
{
    hal = pc_liga();
    pc_relogio(HOJE, 11, 17);
    app_liga(&ap, hal);
    ap.estado.rede = REDE_LIGADA;
    app_passo(&ap);
}

static tela_id topo(void)
{
    return ap.estado.pilha[ap.estado.profundidade];
}

// ── o chão do sistema ───────────────────────────────────────────────
void t_lancador_o_aparelho_liga_na_home(void)
{
    COMECA("o aparelho liga na Home, e não mais direto na agenda");

    liga();
    ESPERA_IGUAL(topo(), TELA_HOME);
    ESPERA_IGUAL(ap.estado.profundidade, 0);
    TERMINA();
}

// ── o primeiro cartão é a Agenda, em foco no boot ───────────────────
void t_lancador_o_ok_abre_a_agenda(void)
{
    COMECA("o foco nasce na Agenda, e o OK entra nela");

    liga();
    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA_IGUAL(topo(), TELA_AGENDA);
    ESPERA_IGUAL(ap.estado.profundidade, 1);
    TERMINA();
}

// Entrar na Agenda pede o delta na hora: o Acervo suspende o pull dela, e
// voltar mostraria o cache antigo.
void t_lancador_entrar_na_agenda_pede_o_delta(void)
{
    COMECA("entrar na Agenda retoma a sincronização imediatamente");

    liga();
    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", "Convidado");
    ap.estado.nuvem_ultimo_ms = 0;
    ap.estado.agora_ms = 6000;
    pc_nuvem_rota_zera();

    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA_IGUAL(topo(), TELA_AGENDA);
    ESPERA_CONTEM(pc_nuvem_rota(), "/v1/pull");
    TERMINA();
}

// ── na Home o BACK não tem para onde ir, e não inventa ──────────────
void t_lancador_o_back_na_home_nao_cria_tela(void)
{
    COMECA("na Home o BACK não empilha nada — ela é a raiz");

    liga();
    pc_botao(IN_VOLTAR);
    app_passo(&ap);

    ESPERA_IGUAL(topo(), TELA_HOME);
    ESPERA_IGUAL(ap.estado.profundidade, 0);
    TERMINA();
}

// ── a grade anda em duas dimensões ──────────────────────────────────
//
//     0 Agenda    1 Acervo
//     2 Jogos     3 Ajustes
//
// A borda não dá a volta (pareceria botão falhando).
void t_lancador_a_grade_anda_em_duas_dimensoes(void)
{
    COMECA("o foco anda na grade 2x2, e a borda não dá a volta");

    liga();
    ESPERA_IGUAL(ap.estado.lancador, 0);          // Agenda

    pc_botao(IN_DIR);   app_passo(&ap);
    ESPERA_IGUAL(ap.estado.lancador, 1);          // Acervo

    pc_botao(IN_DIR);   app_passo(&ap);
    ESPERA_IGUAL(ap.estado.lancador, 1);          // a borda segura

    pc_botao(IN_BAIXO); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.lancador, 3);          // Ajustes

    pc_botao(IN_BAIXO); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.lancador, 3);          // a borda de baixo segura

    pc_botao(IN_ESQ);   app_passo(&ap);
    ESPERA_IGUAL(ap.estado.lancador, 2);          // Jogos

    pc_botao(IN_ESQ);   app_passo(&ap);
    ESPERA_IGUAL(ap.estado.lancador, 2);          // a borda esquerda segura

    pc_botao(IN_CIMA);  app_passo(&ap);
    ESPERA_IGUAL(ap.estado.lancador, 0);          // de volta à Agenda

    pc_botao(IN_CIMA);  app_passo(&ap);
    ESPERA_IGUAL(ap.estado.lancador, 0);          // a borda de cima segura
    TERMINA();
}

// ── cada cartão abre a sua área ─────────────────────────────────────
void t_lancador_cada_cartao_abre_a_sua_area(void)
{
    COMECA("os quatro cartões abrem Agenda, Acervo, Jogos e Ajustes");

    static const struct { int cartao; tela_id area; } ROTA[] = {
        { 0, TELA_AGENDA }, { 1, TELA_ACERVO },
        { 2, TELA_JOGOS }, { 3, TELA_AJUSTES },
    };

    for (int i = 0; i < 4; i++) {
        liga();
        ap.estado.lancador = (int8_t)ROTA[i].cartao;

        pc_botao(IN_OK);
        app_passo(&ap);

        ESPERA_IGUAL(topo(), ROTA[i].area);
        ESPERA_IGUAL(ap.estado.profundidade, 1);
    }
    TERMINA();
}

// ── voltar devolve o cartão de onde se saiu ─────────────────────────
// Por isso o foco não mora no `cursor`, que `empilha` zera.
void t_lancador_voltar_devolve_o_cartao_de_onde_se_saiu(void)
{
    COMECA("voltar de uma área devolve a Home com aquele cartão em foco");

    liga();
    pc_botao(IN_DIR);   app_passo(&ap);      // Acervo
    pc_botao(IN_BAIXO); app_passo(&ap);      // Ajustes
    ESPERA_IGUAL(ap.estado.lancador, 3);

    pc_botao(IN_OK);    app_passo(&ap);
    ESPERA_IGUAL(topo(), TELA_AJUSTES);

    pc_botao(IN_VOLTAR); app_passo(&ap);
    ESPERA_IGUAL(topo(), TELA_HOME);
    ESPERA_IGUAL(ap.estado.lancador, 3);     // e não voltou pra Agenda
    TERMINA();
}

// ── na Home o MENU não abre gaveta: a Home já é o espaço de opções ──
void t_lancador_o_menu_nao_abre_gaveta_na_home(void)
{
    COMECA("na Home o MENU não abre gaveta — ela já é o espaço de opções");

    liga();
    pc_botao(IN_MENU);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_NADA);
    ESPERA_IGUAL(topo(), TELA_HOME);
    ESPERA_IGUAL(ap.estado.profundidade, 0);

    // Dentro de uma área o MENU continua existindo.
    pc_botao(IN_OK);    app_passo(&ap);
    pc_botao(IN_MENU);  app_passo(&ap);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_MENU);

    // Mas a gaveta da Agenda é só dela: Ajustes e Jogos não a abrem.
    liga();
    ENTRA_NOS_AJUSTES(&ap);
    pc_botao(IN_MENU);  app_passo(&ap);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_NADA);

    liga();
    pc_botao(IN_BAIXO); app_passo(&ap);
    pc_botao(IN_OK);    app_passo(&ap);
    ESPERA_IGUAL(topo(), TELA_JOGOS);
    pc_botao(IN_MENU);  app_passo(&ap);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_NADA);
    TERMINA();
}

// ── os quatro cartões, na ordem da grade, com legenda ───────────────
void t_lancador_a_vista_monta_os_quatro_cartoes(void)
{
    COMECA("a vista entrega Agenda, Acervo, Jogos e Ajustes, nesta ordem");

    liga();
    vista_lancador_t v;
    vista_lancador(&ap.estado, &v);

    ESPERA_TEXTO(v.cartoes[0].titulo, "Agenda");
    ESPERA_TEXTO(v.cartoes[1].titulo, "Acervo");
    ESPERA_TEXTO(v.cartoes[2].titulo, "Jogos");
    ESPERA_TEXTO(v.cartoes[3].titulo, "Ajustes");

    // Nenhuma legenda vazia.
    for (int i = 0; i < LANCADOR_CARTOES; i++)
        ESPERA(v.cartoes[i].legenda[0] != '\0');

    ESPERA_TEXTO(v.cartoes[2].legenda, "Xadrez");   // e só ele, na V1
    TERMINA();
}

// ── o foco vem do estado ────────────────────────────────────────────
void t_lancador_a_vista_leva_o_foco_do_estado(void)
{
    COMECA("o cartão em foco é o que o estado diz, e não outro");

    liga();
    pc_botao(IN_BAIXO); app_passo(&ap);      // Jogos

    vista_lancador_t v;
    vista_lancador(&ap.estado, &v);
    ESPERA_IGUAL(v.foco, 2);
    TERMINA();
}

// ── a saudação segue a hora ─────────────────────────────────────────
void t_lancador_a_saudacao_segue_a_hora(void)
{
    COMECA("a saudação muda com o período do dia");

    static const struct { int h; const char *diz; } HORAS[] = {
        {  7, "Bom dia."   },
        { 14, "Boa tarde." },
        { 21, "Boa noite." },
    };

    for (int i = 0; i < 3; i++) {
        hal = pc_liga();
        pc_relogio(HOJE, HORAS[i].h, 30);
        app_liga(&ap, hal);
        app_passo(&ap);

        vista_lancador_t v;
        vista_lancador(&ap.estado, &v);
        ESPERA_TEXTO(v.saudacao, HORAS[i].diz);

        // A linha de cima diz de quem é o aparelho, e não muda com a hora.
        ESPERA_CONTEM(v.sobre, "TINTO DE");
    }
    TERMINA();
}

// ── sem hora confiável, a saudação não inventa (RN-6G) ──────────────
void t_lancador_sem_hora_a_saudacao_nao_inventa(void)
{
    COMECA("sem hora confiável a saudação não afirma período do dia");

    liga();
    ap.estado.hora_confiavel = false;

    vista_lancador_t v;
    vista_lancador(&ap.estado, &v);

    ESPERA_SEM(v.saudacao, "dia");
    ESPERA_SEM(v.saudacao, "tarde");
    ESPERA_SEM(v.saudacao, "noite");
    ESPERA(v.saudacao[0] != '\0');    // e não fica um buraco na tela
    TERMINA();
}

// ── o rodapé da Home não promete BACK ───────────────────────────────
void t_lancador_o_rodape_nao_promete_back(void)
{
    COMECA("na Home o rodapé não oferece BACK, porque ele não faz nada");

    liga();
    vista_lancador_t v;
    vista_lancador(&ap.estado, &v);

    ESPERA_TEXTO(v.rodape_esq, "");
    ESPERA(v.rodape_dir[0] != '\0');
    ESPERA_CONTEM(v.rodape_dir, "OK");
    TERMINA();
}

// ── a barra da Home mostra a data ───────────────────────────────────
void t_lancador_a_barra_mostra_a_data(void)
{
    COMECA("na Home a esquerda da barra é a data, não o nome da tela");

    liga();
    vista_lancador_t v;
    vista_lancador(&ap.estado, &v);

    ESPERA_CONTEM(v.data_curta, "SET");
    ESPERA_CONTEM(v.data_curta, "1");
    ESPERA_TEXTO(v.hora, "11:17");
    TERMINA();
}

// ── o foco preenche o cartão inteiro ────────────────────────────────
// Em 1 bit seleção é inversão. Mede-se densidade de tinta.
static int tinta_no_cartao(int i)
{
    int x, y, l, a, n = 0;
    tela_lancador_cartao(i, &x, &y, &l, &a);
    for (int yy = y; yy < y + a; yy++)
        for (int xx = x; xx < x + l; xx++)
            if (gfx_le(&ap.tela, xx, yy)) n++;
    return n;
}

static void desenha(void)
{
    ap.precisa_desenhar = true;
    app_desenha(&ap);
}

void t_lancador_o_foco_preenche_o_cartao(void)
{
    COMECA("o cartão em foco é um bloco preto; os outros três são papel");

    liga();
    desenha();

    int x, y, l, a;
    tela_lancador_cartao(0, &x, &y, &l, &a);
    const int area = l * a;
    ESPERA(area > 0);

    // O foco nasce na Agenda.
    ESPERA(tinta_no_cartao(0) > area * 6 / 10);

    // Os outros três são papel com traço.
    for (int i = 1; i < LANCADOR_CARTOES; i++)
        ESPERA(tinta_no_cartao(i) < area * 3 / 10);
    TERMINA();
}

// ── mover o foco troca qual cartão está cheio ───────────────────────
void t_lancador_mover_o_foco_troca_o_bloco_preto(void)
{
    COMECA("o bloco preto acompanha o joystick, e só um cartão por vez");

    liga();
    pc_botao(IN_DIR); app_passo(&ap);   // Acervo
    desenha();

    int x, y, l, a;
    tela_lancador_cartao(0, &x, &y, &l, &a);
    const int area = l * a;

    ESPERA(tinta_no_cartao(1) > area * 6 / 10);
    ESPERA(tinta_no_cartao(0) < area * 3 / 10);
    ESPERA(tinta_no_cartao(2) < area * 3 / 10);
    ESPERA(tinta_no_cartao(3) < area * 3 / 10);
    TERMINA();
}

// ── os quatro cartões cabem, e nenhum invade a moldura ──────────────
void t_lancador_a_grade_cabe_sem_invadir_a_moldura(void)
{
    COMECA("os quatro cartões cabem em 240×416 sem tocar barra nem rodapé");

    liga();

    for (int i = 0; i < LANCADOR_CARTOES; i++) {
        int x, y, l, a;
        tela_lancador_cartao(i, &x, &y, &l, &a);

        ESPERA(x >= 0 && y >= 0);
        ESPERA_IGUAL(l, LANCADOR_CARTAO_L);
        ESPERA_IGUAL(a, LANCADOR_CARTAO_A);

        // +4 é o relevo, fora do cartão.
        ESPERA(x + l + 4 <= TELA_L);
        ESPERA(y >= BARRA_A);
        ESPERA(y + a + 4 <= TELA_A - RODAPE_A);
    }
    TERMINA();
}

// ── cada cartão leva ícone e dois textos ────────────────────────────
// Sem isto, um cartão só com moldura passaria nos testes de foco.
void t_lancador_cada_cartao_desenha_icone_e_textos(void)
{
    COMECA("nenhum cartão sai só com a moldura — todos têm ícone e texto");

    liga();
    pc_botao(IN_BAIXO); app_passo(&ap);   // tira o foco da Agenda: papel
    desenha();

    int x, y, l, a;
    tela_lancador_cartao(0, &x, &y, &l, &a);
    const int perimetro = 2 * (l + a);

    // Só a moldura daria ~484 px de tinta.
    for (int i = 0; i < LANCADOR_CARTOES; i++) {
        if (i == 2) continue;             // este está em foco
        ESPERA(tinta_no_cartao(i) > perimetro + 200);
    }
    TERMINA();
}

// ── o texto do cartão cabe no cartão ────────────────────────────────
// Medido pelo texto (`gfx_largura`), que diz qual palavra vazou.
void t_lancador_o_texto_do_cartao_cabe_no_cartao(void)
{
    COMECA("nenhum título ou legenda vaza a moldura do cartão");

    liga();
    vista_lancador_t v;
    vista_lancador(&ap.estado, &v);

    for (int i = 0; i < LANCADOR_CARTOES; i++) {
        ESPERA(v.cartoes[i].titulo[0] && v.cartoes[i].legenda[0]);
        ESPERA(gfx_largura(F_TITULO, v.cartoes[i].titulo)
               <= LANCADOR_CARTAO_UTIL);
        ESPERA(gfx_largura(F_MIUDA, v.cartoes[i].legenda)
               <= LANCADOR_CARTAO_UTIL);
    }
    TERMINA();
}

// ── o completo não assenta o cartão em foco (EINK §5.5) ─────────────
// Entrar é a página sem seletor no completo, e o seletor depois num
// parcial. O ramo da Home não tirava o seletor, e o cartão preto ficava
// carimbado no vidro.
void t_lancador_o_completo_nao_assenta_o_cartao_em_foco(void)
{
    COMECA("a waveform completa pinta a Home SEM o cartão em foco");

    liga();

    const uint8_t *assentado = pc_tela_assentada();
    ESPERA(assentado != NULL);

    // A tinta de cada cartão NAQUELE quadro.
    bitmap_t q;
    bitmap_liga(&q, (uint8_t *)assentado, TELA_L, TELA_A);

    int x, y, l, a;
    tela_lancador_cartao(0, &x, &y, &l, &a);
    const int area = l * a;

    for (int i = 0; i < LANCADOR_CARTOES; i++) {
        int cx, cy, cl, ca, tinta = 0;
        tela_lancador_cartao(i, &cx, &cy, &cl, &ca);
        for (int yy = cy; yy < cy + ca; yy++)
            for (int xx = cx; xx < cx + cl; xx++)
                if (gfx_le(&q, xx, yy)) tinta++;

        // Nenhum cartão é bloco preto no quadro completo.
        ESPERA(tinta < area * 3 / 10);
    }
    TERMINA();
}

// ── entrar em Ajustes pelo cartão mede a memória ────────────────────
// Senão o menu mostraria o número do boot.
void t_lancador_entrar_em_ajustes_mede_a_memoria(void)
{
    COMECA("abrir Ajustes pelo cartão mede o cartão, como a gaveta media");

    liga();

    // Envelhece a medida: senão o teste não prova nada.
    ap.estado.espaco_total_kb = 0;
    ap.estado.espaco_usado_kb = 0;

    pc_botao(IN_DIR);   app_passo(&ap);
    pc_botao(IN_BAIXO); app_passo(&ap);
    pc_botao(IN_OK);    app_passo(&ap);

    ESPERA_IGUAL(topo(), TELA_AJUSTES);
    ESPERA(ap.estado.espaco_total_kb > 0);
    TERMINA();
}

// ── a Agenda anda ontem/hoje/amanhã, e para aí ──────────────────────
// Limite de produto: o ◀▶ é o "e amanhã?" de relance; procurar é o
// calendário. E não empilha.
void t_lancador_a_agenda_anda_um_dia_para_cada_lado(void)
{
    COMECA("a Agenda anda entre ontem, hoje e amanhã — e para nas bordas");

    liga();
    pc_botao(IN_OK); app_passo(&ap);          // entra na Agenda
    ESPERA_IGUAL(topo(), TELA_AGENDA);

    const data_t hoje   = ap.estado.hoje;
    const data_t ontem  = data_soma_dias(hoje, -1);
    const data_t amanha = data_soma_dias(hoje,  1);
    const int    fundo  = ap.estado.profundidade;

    // O cabeçalho é o que está selecionado quando nada mais está.
    ap.estado.cursor = -1;

    pc_botao(IN_DIR); app_passo(&ap);
    ESPERA(data_igual(ap.estado.dia_visto, amanha));
    ESPERA_IGUAL(topo(), TELA_AGENDA);           // não empilhou
    ESPERA_IGUAL(ap.estado.profundidade, fundo);

    pc_botao(IN_DIR); app_passo(&ap);
    ESPERA(data_igual(ap.estado.dia_visto, amanha));   // a borda segura

    pc_botao(IN_ESQ); app_passo(&ap);
    ESPERA(data_igual(ap.estado.dia_visto, hoje));

    pc_botao(IN_ESQ); app_passo(&ap);
    ESPERA(data_igual(ap.estado.dia_visto, ontem));

    pc_botao(IN_ESQ); app_passo(&ap);
    ESPERA(data_igual(ap.estado.dia_visto, ontem));     // e a outra também
    ESPERA_IGUAL(topo(), TELA_AGENDA);
    TERMINA();
}

// ── sair e voltar devolve HOJE ──────────────────────────────────────
// Ontem e amanhã são legítimos; o que está fora deles volta a hoje.
void t_lancador_voltar_para_a_agenda_devolve_hoje(void)
{
    COMECA("um dia fora dos três é puxado de volta para hoje");

    liga();
    pc_botao(IN_OK); app_passo(&ap);

    // Um dia distante, como o calendário deixaria.
    ap.estado.dia_visto     = data_soma_dias(ap.estado.hoje, 18);
    ap.estado.itens_validos = false;

    // Quem corrige é o `garante_cache`, ao montar o quadro.
    ap.precisa_desenhar = true;
    app_desenha(&ap);
    ESPERA(data_igual(ap.estado.dia_visto, ap.estado.hoje));

    // Amanhã fica.
    ap.estado.cursor = -1;
    pc_botao(IN_DIR); app_passo(&ap);
    ap.precisa_desenhar = true;
    app_desenha(&ap);
    ESPERA(data_igual(ap.estado.dia_visto,
                      data_soma_dias(ap.estado.hoje, 1)));
    TERMINA();
}

// ── as tarefas pendentes são as mesmas nos três dias ────────────────
// Andar entre os dias troca os compromissos e deixa as tarefas paradas (vinham
// do cache do dia e sumiam ao vencer).
static void tarefa_no_dia(const hal_t *h, data_t dia, const char *id,
                          const char *titulo)
{
    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,     sizeof it.id,     "%s", id);
    snprintf(it.titulo, sizeof it.titulo, "%s", titulo);
    it.tipo  = TIPO_TAREFA;
    it.dia   = dia;
    it.vence = dia;
    cartao_grava_item(h, dia, &it);
}

void t_lancador_as_tarefas_nao_mudam_com_o_dia(void)
{
    COMECA("o prazo fica à vista em HOJE, antes e depois de vencer");

    hal = pc_liga();
    pc_relogio(HOJE, 9, 0);

    const data_t ontem  = data_soma_dias(HOJE, -1);
    const data_t amanha = data_soma_dias(HOJE,  1);

    tarefa_no_dia(hal, ontem,  "0900-a", "Atrasada de ontem");
    tarefa_no_dia(hal, HOJE,   "0901-b", "Para hoje");
    tarefa_no_dia(hal, amanha, "0902-c", "Para amanhã");

    app_liga(&ap, hal);
    ap.estado.rede = REDE_LIGADA;
    app_passo(&ap);
    pc_botao(IN_OK); app_passo(&ap);          // entra na Agenda

    // HOJE acumula o trabalho aberto; o SELO diz de que dia é cada uma.
    vista_agenda_t v;
    vista_agenda(&ap.estado, &v);
    ESPERA_IGUAL(v.n_trabalho, 3);
    ESPERA_TEXTO(v.trabalho[0].titulo, "Atrasada de ontem");
    ESPERA(v.trabalho[0].selo[0] != '\0');
    ESPERA(v.trabalho[0].selo_negativo);
    ESPERA_TEXTO(v.trabalho[1].titulo, "Para hoje");
    ESPERA_TEXTO(v.trabalho[1].selo, "");
    ESPERA_TEXTO(v.trabalho[2].titulo, "Para amanhã");
    ESPERA(v.trabalho[2].selo[0] != '\0');
    ESPERA(!v.trabalho[2].selo_negativo);

    // Amanhã mostra só a de amanhã: ontem e amanhã não recebem pendência
    // (RN-36).
    ap.estado.cursor = -1;
    pc_botao(IN_DIR); app_passo(&ap);
    ESPERA(data_igual(ap.estado.dia_visto, amanha));

    vista_agenda(&ap.estado, &v);
    ESPERA_IGUAL(v.n_trabalho, 1);
    ESPERA_TEXTO(v.trabalho[0].titulo, "Para amanhã");

    // E em ontem também.
    pc_botao(IN_ESQ); app_passo(&ap);
    pc_botao(IN_ESQ); app_passo(&ap);
    ESPERA(data_igual(ap.estado.dia_visto, ontem));

    vista_agenda(&ap.estado, &v);
    ESPERA_IGUAL(v.n_trabalho, 1);
    ESPERA_TEXTO(v.trabalho[0].titulo, "Atrasada de ontem");
    TERMINA();
}

// ── um dia vazio é vazio, não o dia anterior ────────────────────────
// Dia sem pasta deixava os itens de hoje na RAM.
void t_lancador_um_dia_sem_pasta_nao_herda_o_dia_anterior(void)
{
    COMECA("amanhã sem nada marcado aparece vazio, e não repete hoje");

    hal = pc_liga();
    pc_relogio(HOJE, 9, 0);

    item_t ev;
    memset(&ev, 0, sizeof ev);
    snprintf(ev.id,     sizeof ev.id,     "%s", "g:hoje");
    snprintf(ev.titulo, sizeof ev.titulo, "%s", "Dentista");
    snprintf(ev.hora,   sizeof ev.hora,   "%s", "14:00");
    ev.tipo   = TIPO_EVENTO;
    ev.origem = ORIGEM_GOOGLE;
    ev.dia    = HOJE;
    cartao_grava_item(hal, HOJE, &ev);

    app_liga(&ap, hal);
    ap.estado.rede = REDE_LIGADA;
    app_passo(&ap);
    pc_botao(IN_OK); app_passo(&ap);          // entra na Agenda

    vista_agenda_t v;
    vista_agenda(&ap.estado, &v);
    ESPERA_IGUAL(v.n_agenda, 1);

    // Amanhã: nada marcado, nenhuma pasta.
    ap.estado.cursor = -1;
    pc_botao(IN_DIR); app_passo(&ap);
    ap.precisa_desenhar = true;
    app_desenha(&ap);

    ESPERA(data_igual(ap.estado.dia_visto, data_soma_dias(HOJE, 1)));

    vista_agenda(&ap.estado, &v);
    ESPERA_IGUAL(v.n_agenda, 0);
    ESPERA(v.vazio[0] != '\0');
    TERMINA();
}

// ── a contagem regressiva só existe HOJE ────────────────────────────
// Com `agora = -1`, um evento de amanhã às 14:00 mostrava "em 14 h".
void t_lancador_a_contagem_regressiva_so_existe_hoje(void)
{
    COMECA("fora de hoje, o evento não anuncia quanto falta");

    hal = pc_liga();
    pc_relogio(HOJE, 9, 0);

    const data_t amanha = data_soma_dias(HOJE, 1);
    item_t ev;
    memset(&ev, 0, sizeof ev);
    snprintf(ev.id,     sizeof ev.id,     "%s", "g:amanha");
    snprintf(ev.titulo, sizeof ev.titulo, "%s", "Dentista");
    snprintf(ev.hora,   sizeof ev.hora,   "%s", "14:00");
    ev.tipo   = TIPO_EVENTO;
    ev.origem = ORIGEM_GOOGLE;
    ev.dia    = amanha;
    cartao_grava_item(hal, amanha, &ev);

    app_liga(&ap, hal);
    ap.estado.rede = REDE_LIGADA;
    app_passo(&ap);
    pc_botao(IN_OK); app_passo(&ap);

    ap.estado.cursor = -1;
    pc_botao(IN_DIR); app_passo(&ap);
    ap.precisa_desenhar = true;
    app_desenha(&ap);

    vista_agenda_t v;
    vista_agenda(&ap.estado, &v);
    ESPERA_IGUAL(v.n_agenda, 1);
    ESPERA_TEXTO(v.agenda[0].quando, "");
    TERMINA();
}

// ── o detalhe do dia é do CALENDÁRIO, não da Agenda ─────────────────
void t_lancador_a_agenda_nao_abre_o_detalhe_do_dia(void)
{
    COMECA("a Agenda não abre detalhe de dia — quem faz isso é o calendário");

    liga();
    pc_botao(IN_OK); app_passo(&ap);

    // Nenhum gesto da Agenda chega em TELA_DIA.
    const entrada_t GESTOS[] = { IN_ESQ, IN_DIR, IN_DIR, IN_ESQ,
                                 IN_CIMA, IN_BAIXO, IN_OK };
    for (unsigned i = 0; i < sizeof GESTOS / sizeof GESTOS[0]; i++) {
        pc_botao(GESTOS[i]);
        app_passo(&ap);
        ESPERA(topo() != TELA_DIA);
    }
    TERMINA();
}

// O cabeçalho diz o NOME do dono, não o e-mail da conta.
void t_o_cabecalho_diz_o_nome_e_nao_o_email(void)
{
    estado_t e;
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    COMECA("a Home diz o nome do dono, não o e-mail da conta");

    e.hoje = (data_t){ 2026, 9, 2 };
    e.hora = 9; e.minuto = 14;
    e.hora_confiavel = true;

    snprintf(e.nome, sizeof e.nome, "%s", "usuario@exemplo.com");
    snprintf(e.inicio.nome_pendente, sizeof e.inicio.nome_pendente,
             "%s", "Usuário");

    vista_lancador_t v;
    vista_lancador(&e, &v);

    ESPERA_TEXTO(v.sobre, "TINTO DE USUÁRIO");
    ESPERA(strstr(v.sobre, "@") == NULL);

    // Sem dono, o genérico.
    e.inicio.nome_pendente[0] = '\0';
    vista_lancador(&e, &v);
    ESPERA_TEXTO(v.sobre, "SEU TINTO");

    // O nome não estoura a faixa (24 caracteres, RN-B7).
    char cheio[NOME_UTF8_MAX];
    for (int i = 0; i < NOME_CARACTERES_MAX; i++) cheio[i] = 'M';
    cheio[NOME_CARACTERES_MAX] = '\0';
    snprintf(e.inicio.nome_pendente, sizeof e.inicio.nome_pendente,
             "%s", cheio);
    vista_lancador(&e, &v);

    // A vista entrega as duas formas; a moldura sai primeiro.
    ESPERA_CONTEM(v.sobre, "MMM");
    ESPERA_TEXTO(v.dono, "MMMMMMMMMMMMMMMMMMMMMMMM");
    TERMINA();
}
