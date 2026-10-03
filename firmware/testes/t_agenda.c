// firmware/testes/t_agenda.c — a Agenda decide certo antes de desenhar.
// Tudo em vista/, PURO: testa-se a DECISÃO, sem pixel.
#include "teste.h"
#include "dado/cartao.h"
#include "ui/dia.h"
#include "vista/agenda.h"
#include "vista/dia.h"
#include "ui/agenda.h"
#include "vista/menu.h"
#include "vista/calendario.h"
#include "vista/campos.h"
#include "ui/grid.h"
#include "tela/texto.h"
#include "nucleo/data.h"

static estado_t e;

static void dia_limpo(int hora, int minuto)
{
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje    = (data_t){2026, 8, 12};   // quarta
    e.hora    = (int8_t)hora;
    e.minuto  = (int8_t)minuto;
    e.bateria = 78;
    e.cursor  = 0;

    // No aparelho o dia visto nunca é zero (`app_liga` o iguala a hoje).
    e.dia_visto = e.hoje;
}

// Reespelha as pendentes de `itens`, para o teste que mexe no item depois
// de pô-lo.
static void espelha_pendentes(void)
{
    e.n_pendentes = 0;
    for (int i = 0; i < e.n_itens && e.n_pendentes < PENDENTES_MAX; i++)
        if (e.itens[i].tipo == TIPO_TAREFA || e.itens[i].tipo == TIPO_LISTA)
            e.pendentes[e.n_pendentes++] = e.itens[i];
    e.pendentes_validas = true;
}

static void poe(const char *titulo, tipo_t tipo, const char *hora,
                data_t vence, bool feita)
{
    item_t *it = &e.itens[e.n_itens++];
    memset(it, 0, sizeof *it);
    snprintf(it->id,     sizeof it->id,     "id%d", e.n_itens);
    snprintf(it->titulo, sizeof it->titulo, "%s", titulo);
    snprintf(it->hora,   sizeof it->hora,   "%s", hora ? hora : "");
    it->tipo  = tipo;
    it->vence = vence;
    it->feita = feita;

    // Todo item mora num dia; o `memset` o deixaria em {0,0,0}.
    it->dia = e.hoje;

    // Feita, foi feita HOJE: é onde ela aparece.
    if (feita) it->feita_em = e.hoje;

    // A tarefa vive nos DOIS caches, como no aparelho: `itens` (o dia) e
    // `pendentes` (as abertas, e as feitas — RN-35 as mantém riscadas).
    if ((tipo == TIPO_TAREFA || tipo == TIPO_LISTA) &&
        e.n_pendentes < PENDENTES_MAX)
        e.pendentes[e.n_pendentes++] = *it;

    e.pendentes_validas = true;
}

#define SEM_DATA (data_t){0,0,0}

// ── RN-31 ───────────────────────────────────────────────────────────
void t_agenda_mostra_o_proximo(void)
{
    COMECA("a agenda começa no próximo compromisso, com o quanto falta");

    dia_limpo(9, 14);
    poe("Dentista",           TIPO_EVENTO, "14:00", SEM_DATA, false);
    poe("Aula de finlandês",  TIPO_EVENTO, "19:30", SEM_DATA, false);

    vista_agenda_t v;
    vista_agenda(&e, &v);

    ESPERA_IGUAL(v.n_agenda, 2);
    ESPERA_TEXTO(v.agenda[0].hora,   "14:00");
    ESPERA_TEXTO(v.agenda[0].oque,   "Dentista");
    ESPERA_TEXTO(v.agenda[0].quando, "em 4h46");
    ESPERA(v.agenda[0].proximo);

    // Só o primeiro leva "quanto falta" (RN-32).
    ESPERA_TEXTO(v.agenda[1].quando, "");
    ESPERA(!v.agenda[1].proximo);

    TERMINA();
}

// Sem compromisso a régua não desenha nada: quem fala do dia vazio é a
// marca d'água (a linha "nada marcado" parecia item e abria o dia).
void t_sem_compromisso_a_agenda_nao_desenha_nada(void)
{
    COMECA("sem compromisso, a agenda não inventa uma linha");

    dia_limpo(9, 2);
    poe("mandar histórico p/ Oulu", TIPO_TAREFA, NULL,
        (data_t){2026,8,10}, false);
    e.pendentes[0].prazo = (data_t){2026,8,14};

    vista_agenda_t v;
    vista_agenda(&e, &v);

    ESPERA_IGUAL(v.n_agenda, 0);

    // Agendada dia 10 e com prazo dia 14: continua visível no dia 12.
    ESPERA_IGUAL(v.n_trabalho, 1);
    ESPERA_IGUAL(vista_agenda_linhas(&e), 1);

    TERMINA();
}


// Dia cheio: nada some nem vira "+ N"; a Agenda pagina (1/2, 2/2).
void t_dia_cheio_corta_com_mais(void)
{
    COMECA("dia cheio: tudo entra, o cursor anda por todas, e a tela pagina");

    dia_limpo(8, 15);
    poe("Daily do time",     TIPO_EVENTO, "09:00", SEM_DATA, false);
    poe("Revisão",           TIPO_EVENTO, "11:00", SEM_DATA, false);
    poe("Cliente",           TIPO_EVENTO, "14:30", SEM_DATA, false);
    poe("Academia",          TIPO_EVENTO, "18:00", SEM_DATA, false);
    for (int i = 0; i < 7; i++) {
        char t[40];
        snprintf(t, sizeof t, "tarefa %d", i);
        poe(t, TIPO_TAREFA, NULL, SEM_DATA, false);
    }

    vista_agenda_t v;
    vista_agenda(&e, &v);

    ESPERA_IGUAL(v.n_agenda, 4);
    ESPERA_TEXTO(v.agenda[0].hora, "09:00");
    ESPERA_IGUAL(v.n_trabalho, 7);
    ESPERA_IGUAL(v.n_abertas, 7);
    ESPERA_IGUAL(vista_agenda_linhas(&e), 11);

    // A primeira página e a do fim são quadros diferentes.
    static uint8_t m1[TELA_L / 8 * TELA_A], m2[TELA_L / 8 * TELA_A];
    bitmap_t b1, b2;
    bitmap_liga(&b1, m1, TELA_L, TELA_A);
    bitmap_liga(&b2, m2, TELA_L, TELA_A);
    e.cursor = 0;
    vista_agenda(&e, &v);
    tela_agenda(&b1, &v);
    e.cursor = 10;
    vista_agenda(&e, &v);
    ESPERA_IGUAL(v.cursor_trabalho, 6);
    tela_agenda(&b2, &v);
    ESPERA(memcmp(m1, m2, sizeof m1) != 0);

    TERMINA();
}

// ── a ordem das tarefas ─────────────────────────────────────────────
void t_atrasada_sobe_e_feita_fica_no_lugar(void)
{
    COMECA("atrasada sobe, e a feita fica riscada ONDE ESTAVA");

    dia_limpo(10, 0);
    poe("já fiz essa",     TIPO_TAREFA, NULL, SEM_DATA, true);
    poe("sem data",        TIPO_TAREFA, NULL, SEM_DATA, false);
    poe("vence hoje",      TIPO_TAREFA, NULL, (data_t){2026,8,12}, false);
    poe("atrasada",        TIPO_TAREFA, NULL, (data_t){2026,8,10}, false);

    vista_agenda_t v;
    vista_agenda(&e, &v);

    // A atrasada acompanha até ser feita (RN-34) e encabeça.
    ESPERA_IGUAL(v.n_trabalho, 4);
    ESPERA_TEXTO(v.trabalho[0].titulo, "atrasada");
    ESPERA_TEXTO(v.trabalho[0].selo, "seg");
    ESPERA(v.trabalho[0].selo_negativo);
    ESPERA_TEXTO(v.trabalho[1].titulo, "vence hoje");

    // O selo some no dia da tarefa.
    ESPERA_TEXTO(v.trabalho[1].selo, "");

    // A feita fica onde estava: reordenar debaixo do cursor fazia o OK
    // seguinte acertar outra tarefa.
    ESPERA_TEXTO(v.trabalho[2].titulo, "já fiz essa");
    ESPERA(v.trabalho[2].feita);
    ESPERA_TEXTO(v.trabalho[3].titulo, "sem data");

    TERMINA();
}

// ── o selo, e a razão de ele não ser bloco ──────────────────────────
void t_atrasada_e_selo_nao_bloco(void)
{
    COMECA("atrasado é SELO na linha, não uma categoria à parte");

    dia_limpo(10, 0);
    poe("de segunda",   TIPO_TAREFA, NULL, (data_t){2026,8,10}, false);
    poe("de muito atrás", TIPO_TAREFA, NULL, (data_t){2026,7,20}, false);
    poe("de hoje",      TIPO_TAREFA, NULL, (data_t){2026,8,12}, false);

    vista_agenda_t v;
    vista_agenda(&e, &v);

    // As três entram, e o atraso é um SELO na linha, não um bloco à parte.
    ESPERA_IGUAL(v.n_trabalho, 3);
    ESPERA_TEXTO(v.trabalho[0].titulo, "de muito atrás");
    ESPERA_TEXTO(v.trabalho[0].selo, "23 d");
    ESPERA(v.trabalho[0].selo_negativo);
    ESPERA_TEXTO(v.trabalho[1].titulo, "de segunda");
    ESPERA_TEXTO(v.trabalho[1].selo, "seg");
    ESPERA_TEXTO(v.trabalho[2].titulo, "de hoje");
    ESPERA_TEXTO(v.trabalho[2].selo, "");

    TERMINA();
}

void t_filete_marca_o_que_veio_de_fora(void)
{
    COMECA("filete grosso à esquerda = veio do Google");

    dia_limpo(10, 0);
    poe("nasceu aqui", TIPO_TAREFA, NULL, SEM_DATA, false);
    poe("veio de lá",  TIPO_TAREFA, NULL, SEM_DATA, false);
    e.itens[1].origem = ORIGEM_GOOGLE;
    espelha_pendentes();      // o aparelho lê o cartão já mexido

    vista_agenda_t v;
    vista_agenda(&e, &v);

    ESPERA(!v.trabalho[0].de_fora);
    ESPERA(v.trabalho[1].de_fora);

    TERMINA();
}


// O convite some depois dos primeiros dias. (O "horizonte" — "o próximo:
// qui 28" — saiu porque afirmava o que não sabia: marcas só do mês corrente,
// zero quando não carregadas, e contadas a partir de hoje.)

void t_o_convite_some_depois_dos_primeiros_dias(void)
{
    COMECA("o convite de falar some quando já há o que mostrar");

    dia_limpo(14, 0);
    snprintf(e.nome, sizeof e.nome, "%s", "eu@x.com");
    e.marcas_validas = true;
    e.marcas_ano = 2026;
    e.marcas_mes = 8;
    e.marcas_evento = 1u << (28 - 1);

    vista_agenda_t v;
    vista_agenda(&e, &v);

    // Instrução que fica é aviso que ninguém lê.
    ESPERA_TEXTO(v.convite, "");

    TERMINA();
}

// Anotação não entra nas tarefas (achado pela prova em PNG).
void t_anotacao_nao_entra_no_trabalho(void)
{
    COMECA("anotação não vira linha de trabalho — só conta nas capturas");

    dia_limpo(10, 0);
    poe("uma tarefa",      TIPO_TAREFA,   NULL, SEM_DATA, false);
    poe("ideia do painel", TIPO_ANOTACAO, NULL, SEM_DATA, false);
    poe("gravação 14:20",  TIPO_NADA,     NULL, SEM_DATA, false);
    poe("lista do mercado", TIPO_LISTA,   NULL, SEM_DATA, false);

    vista_agenda_t v;
    vista_agenda(&e, &v);

    // Só a TAREFA é linha; a lista é cabeçalho de grupo.
    ESPERA_IGUAL(v.n_trabalho, 1);
    ESPERA_IGUAL(v.n_abertas, 1);

    TERMINA();
}

// ── o rodapé diz o que o OK faz AGORA ───────────────────────────────
void t_rodape_segue_o_cursor(void)
{
    COMECA("o rodapé diz a saída e o que o OK faz na linha do cursor");

    dia_limpo(10, 0);
    poe("por fazer", TIPO_TAREFA, NULL, SEM_DATA, false);
    poe("já feita",  TIPO_TAREFA, NULL, SEM_DATA, true);

    vista_agenda_t v;

    // Sem compromisso, a linha 0 é a primeira TAREFA.
    e.cursor = 0;
    vista_agenda(&e, &v);

    // A esquerda é a SAÍDA. O ▶ abre o item sem ser anunciado.
    ESPERA_TEXTO(v.rodape_esq, "BACK início");
    ESPERA_TEXTO(v.rodape_dir, "OK marcar");

    e.cursor = 1;
    vista_agenda(&e, &v);
    ESPERA_TEXTO(v.rodape_dir, "OK desmarcar");

    TERMINA();
}

void t_repouso_nao_tem_cursor(void)
{
    COMECA("travado = sem cursor, e o rodapé cala");

    dia_limpo(10, 0);
    poe("uma tarefa", TIPO_TAREFA, NULL, SEM_DATA, false);
    e.travado = true;

    vista_agenda_t v;
    vista_agenda(&e, &v);
    ESPERA_IGUAL(v.cursor, -1);
    ESPERA_TEXTO(v.rodape_dir, "");

    TERMINA();
}

// ── e o dia mais comum de todos ─────────────────────────────────────
void t_dia_vazio_nao_quebra(void)
{
    COMECA("cartão vazio: a home continua inteira");

    dia_limpo(9, 14);
    vista_agenda_t v;
    vista_agenda(&e, &v);

    // Dia vazio: a frase diz o FATO, e não há linha de "nada marcado".
    ESPERA_CONTEM(v.vazio, "livre");
    ESPERA_IGUAL(v.n_agenda, 0);
    ESPERA_IGUAL(v.n_trabalho, 0);

    TERMINA();
}

// O evento de dia inteiro aparece e vem primeiro (filtrar por hora o
// esconderia calado).
void t_evento_de_dia_inteiro_aparece_e_vem_primeiro(void)
{
    COMECA("evento de dia inteiro aparece na home, e vem antes dos com hora");

    dia_limpo(9, 14);
    poe("Reunião de alinhamento", TIPO_EVENTO, "11:00", SEM_DATA, false);
    poe("Academia",              TIPO_EVENTO, "18:30", SEM_DATA, false);

    item_t *inteiro = &e.itens[e.n_itens++];
    memset(inteiro, 0, sizeof *inteiro);
    snprintf(inteiro->id,     sizeof inteiro->id,     "%s", "entrega");
    snprintf(inteiro->titulo, sizeof inteiro->titulo, "%s", "Entrega do documento");
    inteiro->tipo        = TIPO_EVENTO;
    inteiro->dia_inteiro = true;

    vista_agenda_t v;
    vista_agenda(&e, &v);

    ESPERA_IGUAL(v.n_agenda, 3);
    ESPERA_TEXTO(v.agenda[0].oque, "Entrega do documento");
    ESPERA_TEXTO(v.agenda[0].hora, "dia");
    ESPERA(v.agenda[0].indice >= 0);      // leva a item, não é o aviso

    // "em 4 h" não descreve o dia todo.
    ESPERA_TEXTO(v.agenda[0].quando, "");

    TERMINA();
}

// O dia INTEIRO fica, mesmo o que já passou: é dado real, e o calendário
// mostra (decidido no vidro).
void t_o_dia_inteiro_fica_na_agenda(void)
{
    COMECA("às 21h o que já aconteceu continua na agenda");

    dia_limpo(21, 0);
    poe("Reunião cedo", TIPO_EVENTO, "09:00", SEM_DATA, false);
    poe("Jantar",       TIPO_EVENTO, "22:00", SEM_DATA, false);

    vista_agenda_t v;
    vista_agenda(&e, &v);

    ESPERA_IGUAL(v.n_agenda, 2);

    // A contagem é só do que ainda vai chegar.
    ESPERA_TEXTO(v.agenda[0].quando, "");
    ESPERA(v.agenda[1].quando[0] != '\0');
    TERMINA();
}

// Sem rede, a Agenda DIZ isso (T-07).
void t_a_home_diz_quando_ha_coisa_esperando_rede(void)
{
    COMECA("T-07 · sem rede, a home diz isso colado no rodapé");

    dia_limpo(9, 14);
    e.rede = REDE_DESLIGADA;

    vista_agenda_t v;
    vista_agenda(&e, &v);

        // Campo próprio, colado no rodapé, e não o `aviso`.
    ESPERA(v.sem_rede);
    ESPERA_TEXTO(v.aviso, "");

    TERMINA();
}

// T-08: o limite estourou e a tela diz que continua gravando.
void t_a_home_avisa_o_limite_sem_assustar(void)
{
    COMECA("T-08 · estourado o mês, a home diz que continua gravando");

    dia_limpo(9, 14);
    e.rede = REDE_LIGADA;
    e.quota.usados_s = 30 * 60;
    e.quota.limite_s = 30 * 60;

    vista_agenda_t v;
    vista_agenda(&e, &v);

    ESPERA_TEXTO(v.aviso, "sem transcrever até virar");

    TERMINA();
}


// As tarefas vêm por LISTA, com o nome no cabeçalho (antes eram duas zonas
// repetindo os nomes).
void t_o_trabalho_vem_agrupado_pela_lista(void)
{
    COMECA("as tarefas vêm agrupadas, e o título do grupo é a lista");

    dia_limpo(10, 0);

    // O `local` de uma tarefa é o NOME da lista.
    poe("Pagar IPVA", TIPO_TAREFA, NULL, SEM_DATA, false);
    snprintf(e.itens[0].local, sizeof e.itens[0].local, "%s", "My Tasks");

    poe("Leite", TIPO_TAREFA, NULL, SEM_DATA, false);
    snprintf(e.itens[1].local, sizeof e.itens[1].local, "%s", "Compras");

    poe("Conta de luz", TIPO_TAREFA, NULL, SEM_DATA, false);
    snprintf(e.itens[2].local, sizeof e.itens[2].local, "%s", "My Tasks");

    // A LISTA traz a contagem do que existe lá dentro.
    poe("My Tasks", TIPO_LISTA, NULL, SEM_DATA, false);
    e.itens[3].lista_n = 7;
    e.itens[3].lista_k = 2;
    espelha_pendentes();      // os `local` foram escritos depois do `poe`

    vista_agenda_t v;
    vista_agenda(&e, &v);

    // A lista não é linha: é o cabeçalho.
    ESPERA_IGUAL(v.n_trabalho, 3);
    ESPERA_IGUAL(v.n_grupos, 2);

    // As de "My Tasks" ficam juntas.
    ESPERA_IGUAL(v.trabalho[0].grupo, v.trabalho[1].grupo);
    ESPERA(v.trabalho[2].grupo != v.trabalho[0].grupo);

    ESPERA_TEXTO(v.grupos[v.trabalho[0].grupo].titulo, "My Tasks");
    ESPERA_TEXTO(v.grupos[v.trabalho[2].grupo].titulo, "Compras");

    // A contagem da lista vira a do grupo.
    ESPERA_TEXTO(v.grupos[v.trabalho[0].grupo].contagem, "2 de 7");

    TERMINA();
}

// Tarefa recém-falada, ainda sem lista, também tem grupo.
void t_tarefa_sem_lista_ainda_tem_grupo(void)
{
    COMECA("tarefa recém-falada, sem lista ainda, não some da home");

    dia_limpo(10, 0);
    poe("comprar pasta térmica", TIPO_TAREFA, NULL, SEM_DATA, false);

    vista_agenda_t v;
    vista_agenda(&e, &v);

    ESPERA_IGUAL(v.n_trabalho, 1);
    ESPERA_IGUAL(v.n_grupos, 1);

    // O grupo existe, com título vazio: "TAREFAS" logo abaixo de "POR FAZER"
    // repetiria o rótulo.
    ESPERA_TEXTO(v.grupos[0].titulo, "");
    ESPERA_TEXTO(v.grupos[0].contagem, "");

    TERMINA();
}

// Lista sem pendência não vira cabeçalho.
void t_lista_sem_tarefa_no_aparelho_nao_vira_cabecalho(void)
{
    COMECA("lista sem tarefa aqui não ocupa linha nenhuma");

    dia_limpo(10, 0);
    poe("Compras", TIPO_LISTA, NULL, SEM_DATA, false);
    e.itens[0].lista_n = 4;
    e.itens[0].lista_k = 0;

    vista_agenda_t v;
    vista_agenda(&e, &v);

    ESPERA_IGUAL(v.n_trabalho, 0);
    ESPERA_IGUAL(v.n_grupos, 0);

    // E o dia conta como vazio.
    ESPERA(v.vazio[0] != '\0');

    TERMINA();
}


// ── a Agenda tem saída, e o rodapé a diz ────────────────────────────
// A esquerda do rodapé é a saída (BACK volta à Home); em repouso as setas
// moram no cabeçalho.
static void poe_tarefa(int i, const char *titulo)
{
    item_t *it = &e.itens[i];
    memset(it, 0, sizeof *it);
    snprintf(it->id,     sizeof it->id,     "090%d-t", i);
    snprintf(it->titulo, sizeof it->titulo, "%s", titulo);
    it->tipo = TIPO_TAREFA;
    it->dia  = e.hoje;
    if (i + 1 > e.n_itens) e.n_itens = (int16_t)(i + 1);
    e.itens_validos = true;

    // Nos dois caches, como no aparelho (ver `poe`).
    if (e.n_pendentes < PENDENTES_MAX) e.pendentes[e.n_pendentes++] = *it;
    e.pendentes_validas = true;
}

void t_a_agenda_anuncia_a_saida_no_rodape(void)
{
    COMECA("o rodapé da Agenda diz a saída, que agora existe");

    dia_limpo(9, 14);
    poe_tarefa(0, "Pagar IPVA");
    poe_tarefa(1, "Conta de luz");

    vista_agenda_t v;

    // ── em repouso: as setas andam entre os dias ──
    e.cursor = -1;
    vista_agenda(&e, &v);
    ESPERA_CONTEM(v.rodape_esq, "BACK");
    ESPERA_SEM(v.rodape_esq, "abrir");

    // À direita, silêncio: em repouso o OK não faz nada.
    ESPERA_TEXTO(v.rodape_dir, "");

    // ── com uma tarefa sob o cursor: RN-37, o OK marca feita ──
    e.cursor = 0;
    vista_agenda(&e, &v);
    ESPERA_CONTEM(v.rodape_esq, "BACK");
    ESPERA_CONTEM(v.rodape_dir, "OK");

    // ── travado, o rodapé cala ──
    e.travado = true;
    vista_agenda(&e, &v);
    ESPERA_TEXTO(v.rodape_esq, "");
    ESPERA_TEXTO(v.rodape_dir, "");
    TERMINA();
}


// A gaveta FECHA em vez de voltar (é pop-over), e não oferece Ajustes nem
// Anotações (navegação concorrente).
void t_a_gaveta_fecha_em_vez_de_voltar(void)
{
    COMECA("a gaveta diz que FECHA, e não oferece o que a Home já oferece");

    dia_limpo(9, 14);

    vista_menu_t v;
    vista_menu(&e, 8, &v);

    ESPERA_CONTEM(v.rodape_esq, "fechar");
    ESPERA_SEM(v.rodape_esq, "hoje");

    ESPERA_SEM(v.nota, "hoje");
    ESPERA_CONTEM(v.nota, "Home");

    // Ajustes saiu: é cartão da Home.
    for (int i = 0; i < v.n; i++)
        ESPERA(strcmp(v.linhas[i].texto, "Ajustes") != 0);

    // O que é ação da Agenda continua: o calendário.
    bool tem_calendario = false, tem_anotacoes = false;
    for (int i = 0; i < v.n; i++) {
        if (strcmp(v.linhas[i].texto, "Calendário") == 0) tem_calendario = true;
        if (strcmp(v.linhas[i].texto, "Anotações")  == 0) tem_anotacoes  = true;
    }
    ESPERA(tem_calendario);

    // E Anotações mora no Acervo.
    ESPERA(!tem_anotacoes);
    TERMINA();
}


// A Agenda é uma ÁREA: o nome na barra e a data no cabeçalho.
void t_a_agenda_leva_o_dia_no_cabecalho(void)
{
    COMECA("a barra diz Agenda, e o dia vira cabeçalho com a data por extenso");

    dia_limpo(9, 14);                    // 12/08/2026, uma quarta
    poe_tarefa(0, "Pagar IPVA");

    vista_agenda_t v;
    vista_agenda(&e, &v);

    ESPERA_TEXTO(v.titulo, "Agenda");

    ESPERA_TEXTO(v.dia_semana, "QUARTA-FEIRA");

    ESPERA_TEXTO(v.dia_longo, "12 de agosto");

    // A navegação de dia mora junto do dia.
    ESPERA_CONTEM(v.nav_dia, "◀");
    ESPERA_CONTEM(v.nav_dia, "▶");
    TERMINA();
}


// O cabeçalho mostra o DIA VISTO, e as setas dizem onde se está.
void t_o_cabecalho_segue_o_dia_visto(void)
{
    COMECA("o cabeçalho e as setas seguem o dia visto, não o dia de hoje");

    dia_limpo(9, 14);                       // 12/08/2026, quarta
    poe_tarefa(0, "Pagar IPVA");

    vista_agenda_t v;

    e.dia_visto = e.hoje;
    vista_agenda(&e, &v);
    ESPERA_TEXTO(v.dia_semana, "QUARTA-FEIRA");
    ESPERA_TEXTO(v.dia_longo,  "12 de agosto");
    ESPERA_CONTEM(v.nav_dia,   "hoje");

    e.dia_visto = data_soma_dias(e.hoje, 1);
    vista_agenda(&e, &v);
    ESPERA_TEXTO(v.dia_semana, "QUINTA-FEIRA");
    ESPERA_TEXTO(v.dia_longo,  "13 de agosto");
    ESPERA_CONTEM(v.nav_dia,   "amanhã");
    ESPERA_SEM(v.nav_dia,      "hoje");

    e.dia_visto = data_soma_dias(e.hoje, -1);
    vista_agenda(&e, &v);
    ESPERA_TEXTO(v.dia_semana, "TERÇA-FEIRA");
    ESPERA_TEXTO(v.dia_longo,  "11 de agosto");
    ESPERA_CONTEM(v.nav_dia,   "ontem");
    TERMINA();
}


// O local aparece em todo card, não só no primeiro.
void t_o_local_aparece_em_qualquer_evento(void)
{
    COMECA("todo card de evento leva o local, não só o primeiro");

    dia_limpo(9, 0);
    for (int i = 0; i < 2; i++) {
        item_t *it = &e.itens[i];
        memset(it, 0, sizeof *it);
        snprintf(it->id,     sizeof it->id,     "g:ev%d", i);
        snprintf(it->titulo, sizeof it->titulo, "Compromisso %d", i);
        snprintf(it->hora,   sizeof it->hora,   "1%d:00", 4 + i);
        snprintf(it->local,  sizeof it->local,  "Rua Bahia, 21%d", i);
        it->tipo   = TIPO_EVENTO;
        it->origem = ORIGEM_GOOGLE;
        it->dia    = e.hoje;
    }
    e.n_itens = 2;
    e.itens_validos = true;

    vista_agenda_t v;
    vista_agenda(&e, &v);

    ESPERA_IGUAL(v.n_agenda, 2);
    ESPERA_CONTEM(v.agenda[0].onde, "Rua Bahia");
    ESPERA_CONTEM(v.agenda[1].onde, "Rua Bahia");

    // A contagem continua só do PRÓXIMO.
    ESPERA(v.agenda[0].quando[0] != '\0');
    ESPERA_TEXTO(v.agenda[1].quando, "");
    TERMINA();
}


// O cabeçalho não encavala: os SETE dias contra as TRÊS setas, porque o
// pior caso chega em março.
void t_o_cabecalho_do_dia_nao_encavala(void)
{
    COMECA("dia da semana e setas cabem juntos, nos sete dias");

    dia_limpo(9, 14);

    for (int d = 0; d < 7; d++) {
        for (int p = -1; p <= 1; p++) {
            e.hoje      = data_soma_dias((data_t){2026, 8, 9}, d);  // um domingo
            e.dia_visto = data_soma_dias(e.hoje, p);

            vista_agenda_t v;
            vista_agenda(&e, &v);

            int usado = gfx_largura(F_MIUDA, v.dia_semana)
                      + gfx_largura(F_MIUDA, v.nav_dia)
                      + 10;                      // o respiro mínimo entre eles
            ESPERA(usado <= GRID_UTIL);
        }
    }
    TERMINA();
}



// O calendário tem a moldura da Agenda: o nome da área na barra, o mês no
// cabeçalho.
void t_o_calendario_tem_a_moldura_da_agenda(void)
{
    COMECA("o calendário diz Calendário na barra, e o mês vira cabeçalho");

    dia_limpo(9, 14);
    e.marcas_validas = true;
    e.marcas_ano = 2026; e.marcas_mes = 8;

    vista_cal_t v;
    vista_calendario(&e, &v);

    ESPERA_TEXTO(v.titulo, "Calendário");
    ESPERA_CONTEM(v.mes, "Agosto");
    ESPERA_CONTEM(v.mes, "2026");

    // As setas andam de DIA (o desenho diz "◀ mês ▶" e erra).
    ESPERA_CONTEM(v.nav_mes, "dia");
    ESPERA_SEM(v.nav_mes, "mês");
    TERMINA();
}

// O resumo do calendário conta em PALAVRAS, e a esquerda do rodapé volta
// a ser a saída.
void t_o_resumo_do_calendario_conta_em_palavras(void)
{
    COMECA("o resumo conta eventos e tarefas, e o rodapé volta a ter a saída");

    dia_limpo(9, 14);
    poe("Dentista",   TIPO_EVENTO, "14:00", SEM_DATA, false);
    poe("Aula",       TIPO_EVENTO, "19:30", SEM_DATA, false);
    // COM prazo no dia visto: tarefa sem prazo não pertence a dia nenhum.
    poe("Pagar IPVA", TIPO_TAREFA, NULL,    e.dia_visto, false);
    e.marcas_validas = true;
    e.marcas_ano = 2026; e.marcas_mes = 8;

    vista_cal_t v;
    vista_calendario(&e, &v);

    ESPERA_CONTEM(v.contagem, "2 evento");
    ESPERA_CONTEM(v.contagem, "1 tarefa");

    ESPERA_CONTEM(v.rodape_esq, "BACK");
    ESPERA_SEM(v.rodape_esq, "evento");
    ESPERA_CONTEM(v.rodape_dir, "OK");
    TERMINA();
}


// O dia vazio não promete a semana: fala só do dia na tela.
void t_o_dia_vazio_nao_promete_a_semana(void)
{
    COMECA("o dia vazio fala só do dia, e não afirma nada sobre a semana");

    dia_limpo(9, 14);
    e.n_itens = 0;
    e.itens_validos = true;

    vista_agenda_t v;

    e.marcas_validas = false;
    vista_agenda(&e, &v);
    ESPERA(v.vazio[0] != '\0');
    ESPERA_SEM(v.vazio, "semana");

    e.marcas_validas = true;
    e.marcas_ano = 2026; e.marcas_mes = 8;
    e.marcas_evento = 1u << 27;          // tem coisa no dia 28
    vista_agenda(&e, &v);
    ESPERA_SEM(v.vazio, "semana");

    e.dia_visto = data_soma_dias(e.hoje, 1);
    vista_agenda(&e, &v);
    ESPERA(v.vazio[0] != '\0');
    ESPERA_SEM(v.vazio, "semana");
    TERMINA();
}


// O HUD diz O QUE a sincronização faz: quatro formas distintas, nenhuma
// girando; o "processando" cobre o resto.
void t_o_hud_distingue_subir_de_descer_de_falhar(void)
{
    COMECA("o HUD mostra formas distintas para subir, descer e falhar");

    dia_limpo(9, 14);

    e.sinc = SINC_OCIOSO;
    ESPERA_IGUAL(vista_sinc_da_barra(&e), ICO_NENHUM);

    e.sinc = SINC_RECEBENDO;
    ESPERA_IGUAL(vista_sinc_da_barra(&e), ICO_DESCENDO);

    e.sinc = SINC_ENVIANDO;
    ESPERA_IGUAL(vista_sinc_da_barra(&e), ICO_SUBINDO);

    e.sinc = SINC_ERRO;
    ESPERA_IGUAL(vista_sinc_da_barra(&e), ICO_SYNC_ERRO);

    // Ocioso, mas esperando outra coisa: varrendo redes.
    e.sinc = SINC_OCIOSO;
    e.wifi_procurando = true;
    ESPERA_IGUAL(vista_sinc_da_barra(&e), ICO_SINCRONIZA);
    TERMINA();
}


// A tarefa FEITA se prende ao dia em que foi feita; a pendente aparece
// nos três dias.
void t_a_tarefa_feita_se_prende_ao_dia(void)
{
    COMECA("a feita fica no dia dela; a pendente aparece nos três");

    dia_limpo(9, 0);

    poe("já fiz ontem", TIPO_TAREFA, NULL, SEM_DATA, true);
    e.pendentes[0].feita_em = data_soma_dias(e.hoje, -1);

    poe("ainda aberta", TIPO_TAREFA, NULL, SEM_DATA, false);
    e.pendentes[1].dia = data_soma_dias(e.hoje, -1);

    vista_agenda_t v;

    // HOJE: só a aberta.
    e.dia_visto = e.hoje;
    vista_agenda(&e, &v);
    ESPERA_IGUAL(v.n_trabalho, 1);
    ESPERA_TEXTO(v.trabalho[0].titulo, "ainda aberta");

    // ONTEM: as duas, a feita riscada.
    e.dia_visto = data_soma_dias(e.hoje, -1);
    vista_agenda(&e, &v);
    ESPERA_IGUAL(v.n_trabalho, 2);

    bool achou_feita = false;
    for (int i = 0; i < v.n_trabalho; i++)
        if (v.trabalho[i].feita) achou_feita = true;
    ESPERA(achou_feita);
    TERMINA();
}

// RN-65: o item ilegível APARECE dizendo que não deu para ler.
void t_item_ilegivel_aparece_na_agenda(void)
{
    COMECA("RN-65 · o item que não deu pra ler aparece na Agenda");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = e.dia_visto = (data_t){2026, 8, 21};
    e.hora = 9; e.minuto = 14;

    item_t *it = &e.itens[e.n_itens++];
    memset(it, 0, sizeof *it);
    snprintf(it->id, sizeof it->id, "%s", "0901-ruim");
    it->tipo    = TIPO_EVENTO;
    it->origem  = ORIGEM_AQUI;
    it->defeito = true;
    it->dia     = e.hoje;
    snprintf(it->hora, sizeof it->hora, "%s", "09:30");
    e.itens_validos = true;

    vista_agenda_t v;
    vista_agenda(&e, &v);

    ESPERA_IGUAL(v.n_agenda, 1);
    ESPERA_CONTEM(v.agenda[0].oque, "não consegui ler");
    TERMINA();
}

// O dia aberto pelo calendário lista tarefa como a Agenda: com índice,
// lista e parada de cursor (era texto morto).
void t_o_dia_do_calendario_lista_tarefas_como_a_agenda(void)
{
    COMECA("o dia aberto pelo calendário lista tarefas navegáveis, por lista");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){ 2026, 9, 2 };
    e.dia_visto = (data_t){ 2026, 9, 10 };
    e.hora = 9; e.minuto = 14;

    struct { const char *t; const char *lista; } T[] = {
        { "Levar os exames",  "Minhas tarefas" },
        { "Pagar o IPVA",     "Casa"           },
    };
    for (size_t i = 0; i < sizeof T / sizeof T[0]; i++) {
        item_t *it = &e.itens[e.n_itens++];
        memset(it, 0, sizeof *it);
        snprintf(it->id,     sizeof it->id,     "t%d", (int)i);
        snprintf(it->titulo, sizeof it->titulo, "%s", T[i].t);
        snprintf(it->local,  sizeof it->local,  "%s", T[i].lista);
        it->tipo  = TIPO_TAREFA;
        it->dia   = e.hoje;              // falada hoje
        it->vence = e.dia_visto;         // vence no dia aberto

        // Nos DOIS caches, como no aparelho.
        e.pendentes[e.n_pendentes++] = *it;
    }
    e.itens_validos = true;
    e.pendentes_validas = true;

    vista_dia_t v;
    vista_dia(&e, &v);

    ESPERA_IGUAL(v.n_tarefas, 2);

    ESPERA(v.tarefas[0].indice >= 0);
    ESPERA(v.tarefas[1].indice >= 0);

    ESPERA_IGUAL(v.n_grupos, 2);
    ESPERA(v.tarefas[0].grupo != v.tarefas[1].grupo);

    ESPERA_IGUAL(vista_dia_linhas(&e), v.n_compromissos + v.n_tarefas);
    TERMINA();
}

// A tarefa concluída mora no dia da CONCLUSÃO, não no da criação.
void t_tarefa_concluida_mora_no_dia_da_conclusao(void)
{
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    COMECA("a tarefa concluída aparece no dia em que foi concluída");

    e.hoje = e.dia_visto = (data_t){ 2026, 9, 2 };
    e.hora = 9; e.minuto = 14;

    item_t *it = &e.itens[e.n_itens++];
    memset(it, 0, sizeof *it);
    snprintf(it->id,     sizeof it->id,     "%s", "t1");
    snprintf(it->titulo, sizeof it->titulo, "%s", "Levar os exames");
    it->tipo     = TIPO_TAREFA;
    it->dia      = (data_t){ 2026, 8, 12 };   // falada em agosto
    it->feita    = true;
    it->feita_em = e.hoje;                    // fechada hoje
    e.itens_validos = true;
    e.pendentes[0] = *it;
    e.n_pendentes = 1;
    e.pendentes_validas = true;

    vista_agenda_t v;
    vista_agenda(&e, &v);

    ESPERA_IGUAL(v.n_trabalho, 1);
    ESPERA(v.trabalho[0].feita);

    e.dia_visto = (data_t){ 2026, 9, 1 };
    vista_agenda(&e, &v);
    ESPERA_IGUAL(v.n_trabalho, 0);

    e.dia_visto = (data_t){ 2026, 8, 12 };
    vista_agenda(&e, &v);
    ESPERA_IGUAL(v.n_trabalho, 0);
    TERMINA();
}

// As telas de tarefa leem a mesma fonte (`pendentes`): a concluída hoje
// pode morar na pasta de agosto.
void t_as_telas_de_tarefa_leem_a_mesma_fonte(void)
{
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    COMECA("o dia e o \"tudo\" mostram a concluída, como a Agenda mostra");

    e.hoje = e.dia_visto = (data_t){ 2026, 9, 2 };
    e.hora = 9; e.minuto = 14;

    item_t *p = &e.pendentes[e.n_pendentes++];
    memset(p, 0, sizeof *p);
    snprintf(p->id,     sizeof p->id,     "%s", "t1");
    snprintf(p->titulo, sizeof p->titulo, "%s", "Comprar componentes");
    p->tipo     = TIPO_TAREFA;
    p->dia      = (data_t){ 2026, 8, 12 };
    p->feita    = true;
    p->feita_em = e.hoje;
    e.pendentes_validas = true;
    e.itens_validos     = true;

    vista_agenda_t h;
    vista_agenda(&e, &h);
    ESPERA_IGUAL(h.n_trabalho, 1);

    vista_dia_t d;
    vista_dia(&e, &d);
    ESPERA_IGUAL(d.n_tarefas, 1);
    ESPERA(d.tarefas[0].feita);
    TERMINA();
}

// O foco no navegador de dia aparece: o ◀▶ faz duas coisas, e a tela diz
// qual.
void t_o_navegador_de_dia_mostra_que_esta_em_foco(void)
{
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    COMECA("o ◀ hoje ▶ mostra quando é ele que o direcional move");

    dia_limpo(9, 14);
    poe("Dentista", TIPO_EVENTO, "14:00", SEM_DATA, false);

    vista_agenda_t v;

    e.cursor = -1;
    vista_agenda(&e, &v);
    ESPERA(v.nav_focado);

    e.cursor = 0;
    vista_agenda(&e, &v);
    ESPERA(!v.nav_focado);
    TERMINA();
}

// O dia do calendário pagina em vez de escrever sob o rodapé.
void t_o_dia_pagina_quando_tem_coisa_demais(void)
{
    COMECA("Dia · com muita coisa ele pagina, e diz em que página está");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = e.dia_visto = (data_t){2026, 9, 2};
    e.hora = 9; e.minuto = 14;

    for (int i = 0; i < 14; i++) {
        item_t it;
        memset(&it, 0, sizeof it);
        snprintf(it.id, sizeof it.id, "e%d", i);
        snprintf(it.titulo, sizeof it.titulo, "compromisso número %d", i);
        snprintf(it.hora, sizeof it.hora, "%02d:00", 7 + i);
        it.tipo = TIPO_EVENTO;
        it.dia  = e.hoje;
        e.itens[e.n_itens++] = it;
    }
    e.itens_validos = true;

    vista_dia_t v;
    vista_dia(&e, &v);

    static uint8_t bits[(TELA_L + 7) / 8 * TELA_A];
    bitmap_t bm;
    bitmap_liga(&bm, bits, TELA_L, TELA_A);

    ESPERA(tela_dia_paginas(&bm, &v) > 1);

    // Com o cursor no fim, a janela salta para a página que o contém.
    v.cursor = 0;
    ESPERA_IGUAL(tela_dia_pagina(&bm, &v), 1);

    v.cursor = 13;
    ESPERA_IGUAL(tela_dia_pagina(&bm, &v), tela_dia_paginas(&bm, &v));

    TERMINA();
}

// Nenhum compromisso do dia fica de fora da vista: o teto é o do cache.
void t_nenhum_compromisso_do_dia_fica_de_fora(void)
{
    COMECA("Dia · doze compromissos aparecem doze, e não dez");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = e.dia_visto = (data_t){2026, 9, 2};

    for (int i = 0; i < 12; i++) {
        item_t it;
        memset(&it, 0, sizeof it);
        snprintf(it.id, sizeof it.id, "e%d", i);
        snprintf(it.titulo, sizeof it.titulo, "compromisso %d", i);
        snprintf(it.hora, sizeof it.hora, "%02d:00", 7 + i);
        it.tipo = TIPO_EVENTO;
        it.dia  = e.hoje;
        e.itens[e.n_itens++] = it;
    }
    e.itens_validos = true;

    vista_dia_t v;
    vista_dia(&e, &v);

    ESPERA_IGUAL(v.n_compromissos, 12);

    TERMINA();
}

// O cartão é varrido UMA vez por quadro: as atrasadas saem das pendentes,
// não de outra varredura.
void t_o_cartao_e_varrido_uma_vez_por_quadro(void)
{
    COMECA("cartão · atrasadas saem das pendentes, sem segunda varredura");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 9, 3}, 9, 0);
    static app_t ap3;
    app_liga(&ap3, hal);
    app_passo(&ap3);

    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id, sizeof it.id, "%s", "t1");
    snprintf(it.titulo, sizeof it.titulo, "%s", "Pagar o IPVA");
    it.tipo = TIPO_TAREFA;
    it.dia  = (data_t){2026, 9, 2};
    ESPERA_IGUAL(cartao_grava_item(hal, it.dia, &it), OK);

    estado_invalida_cartao(&ap3.estado);
    ap3.estado.dia_visto = ap3.estado.hoje;

    int antes = pc_listagens();
    ap3.precisa_desenhar = true;      // é o desenho que monta o quadro
    app_passo(&ap3);
    int gastou = pc_listagens() - antes;

    bool achou = false;
    for (int i = 0; i < ap3.estado.n_itens; i++)
        if (strstr(ap3.estado.itens[i].titulo, "IPVA")) achou = true;
    ESPERA(achou);

    // Uma varredura: a raiz mais uma listagem por dia.
    ESPERA(gastou < 12);

    TERMINA();
}
