// vista/agenda.h — dados crus → linhas prontas. PURO.
// vista/ decide o que aparece; tela/ só pinta. A struct é plana e já
// formatada ("14:00", "3 d"): formatar é decidir, e decidir é testável.
#ifndef VISTA_AGENDA_H
#define VISTA_AGENDA_H

#include "../nucleo/tipos.h"
#include "../tela/icones.h"
#include "../nucleo/estado.h"

// Tetos de MEMÓRIA, não de tela: a Agenda pagina (ui/agenda.c). Por isso a
// vista é grande e vive em memória estática, não na pilha.
#define AGENDA_MAX_TRABALHO 24
#define AGENDA_MAX_COMPROMISSOS   12

// ── a régua de compromissos ─────────────────────────────────────────
// RN-31: o bloco nunca fica vazio; sem compromisso, diz que não há.
typedef struct {
    char hora[9];        // "14:00" · "dia" no evento de dia inteiro
    char oque[30];
    char quando[12];     // "em 4 h" · "agora" · ""
    bool proximo;        // o primeiro que ainda vai acontecer: vira CARTAZ

    // Onde acontece (o `l` do contrato). Só o cartaz mostra: endereço truncado
    // nas linhas de baixo não leva a lugar nenhum.
    char onde[34];

    // -1 = a linha não leva a item nenhum ("nada marcado"). Não dá para usar a
    // hora vazia: o dia inteiro também não tem hora.
    int16_t indice;

    // O híbrido (tarefa com hora) fica na régua e se marca: a caixa anuncia o
    // que o OK faz. É o que faz hábito funcionar — vale na Agenda e no dia.
    bool caixa;
    bool feita;
} linha_agenda_t;

// ── as tarefas, em GRUPOS ────────────────────────────────────────────
// No Google Tasks toda tarefa mora numa lista com nome; o nome é o título do
// grupo e as tarefas vêm abaixo. Lista sem tarefa pendente não aparece.
#define AGENDA_MAX_GRUPOS 8

typedef struct {
    char titulo[36];     // "MY TASKS" — o nome da lista, como está no Google
    char contagem[10];   // "2 de 7" — o que a lista tem lá dentro, ou ""
} grupo_trabalho_t;

typedef struct {
    // O título INTEIRO: quem quebra e põe reticência é a tela, que conhece a
    // largura.
    char titulo[64];
    char selo[10];       // "seg" · "3 d" · "" — vazio = sem selo
    bool selo_negativo;  // atrasada = selo em negativo
    bool feita;          // fica riscada ONDE ESTÁ (RN-35)
    bool de_fora;        // filete grosso à esquerda (origem Google)
    int8_t  grupo;       // de qual lista ela é
    int16_t indice;      // qual item do cache — a tela ignora
} linha_trabalho_t;

typedef struct {
    // ── a barra, e o cabeçalho de dia ────────────────────────────────────
    // As áreas mostram o próprio nome na barra; a data desce para o cabeçalho,
    // em serifa.
    char  titulo[10];       // "Agenda"
    char  dia_semana[20];   // "QUARTA-FEIRA"
    char  dia_longo[24];    // "12 de agosto"
    char  nav_dia[16];      // "◀ hoje ▶"

    // O direcional está no "◀ hoje ▶"? Com linha selecionada o ◀▶ age na
    // linha; sem seleção, troca o dia. A tela precisa mostrar qual.
    bool  nav_focado;
    char  hora[9];          // "09:14"
    int   bateria;
    // -1 esconde o ícone do rádio: num aparelho que funciona offline, ícone
    // permanente seria lembrete do que não faz falta.
    int  wifi;

    // O indicador de espera do HUD, e a fase dele.
    icone_id sinc;   // o que a sincronização faz, como forma

    linha_agenda_t   agenda[AGENDA_MAX_COMPROMISSOS];
    int              n_agenda;

    linha_trabalho_t trabalho[AGENDA_MAX_TRABALHO];
    int              n_trabalho;

    grupo_trabalho_t grupos[AGENDA_MAX_GRUPOS];
    int              n_grupos;
    int              n_abertas;         // pro rótulo "POR FAZER  3"
    char             aviso[30];         // "8 não couberam" — RN-4B, aqui também

    // Sem rede o dia aparece igual (o cartão tem tudo). Estado do aparelho:
    // mora no rodapé, não ao lado do rótulo.
    bool             sem_rede;

    // ── o dia vazio ──────────────────────────────────────────────────────
    //   `vazio` sozinho     — este dia não tem nada
    //   `vazio` + `convite` — o aparelho nunca recebeu nem criou nada
    // "Seu dia está livre", dito como fato e não como falta. O convite some cedo:
    // instrução que fica vira aviso que ninguém lê.
    char             vazio[44];
    char             convite[80];

    // ── onde o cursor está ──
    // A tela pergunta "este campo existe?", nunca "o que este número significa?".
    int  cursor;                     // -1 = sem cursor (repouso)
    // A linha cuja PÁGINA se mostra: o cursor, mas sobrevive ao quadro sem
    // seletor (EINK §5.5).
    int  foco;
    int  cursor_agenda;              // qual linha de agenda, ou -1

    int  cursor_trabalho;            // qual linha de trabalho, ou -1

    // O que a linha do cursor abre, para o app não refazer a conta. -1 = nada.
    int16_t alvo;

    // Em QUAL cache o `alvo` indexa: eventos em `itens` (o dia), tarefas em
    // `pendentes` (o intervalo).
    bool    alvo_pendente;

    // 28: "◀ ontem  amanhã ▶" tem 22 BYTES (setas multibyte), e cortar no meio
    // de um caractere desenha caixas no vidro — passa nos testes de vista.
    char rodape_esq[28], rodape_dir[22];
} vista_agenda_t;

void vista_agenda(const estado_t *e, vista_agenda_t *out);

// Quantas linhas o cursor visita: compromissos e tarefas, em todas as
// páginas.
int vista_agenda_linhas(const estado_t *e);

// De que lista é a tarefa, e em qual grupo ela entra (criado se faltar; -1
// se não cabe). A Agenda e o dia agrupam pela mesma regra.
const char *vista_lista_da_tarefa(const item_t *it);
int vista_grupo(grupo_trabalho_t *grupos, int *n, const char *nome);

#endif
