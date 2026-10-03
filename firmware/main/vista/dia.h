// vista/dia.h — T-06, um dia aberto pelo calendário.
// A régua com os compromissos e o que é DESTE dia em tarefas (prazo ou
// conclusão; ver `vista_tarefa_no_dia`). A pendência sem dia mora na Agenda.
#ifndef VISTA_DIA_H
#define VISTA_DIA_H

#include "../nucleo/estado.h"
#include "../tela/icones.h"
#include "linhas.h"
#include "agenda.h"

// Itens por seção. Cabe no orçamento de memória e sobra para um dia real;
// 32 já estourou a pilha da APP (boot loop). O que passa vira "+ N não
// couberam".
#define DIA_MAX 16

typedef struct {
    char titulo[20];        // "Seg 10 ago"
    char hora[9];
    int  bateria;
    // -1 esconde o ícone do rádio: num aparelho que funciona offline, ícone
    // permanente seria lembrete do que não faz falta.
    int  wifi;
    icone_id sinc;   // o que a sincronização faz, como forma

    char rotulo[24];        // "Aconteceu" · "Hoje"
    char quando[16];        // "há 2 dias" · ""

    // ── duas seções ──────────────────────────────────────────────────────
    // Compromissos em cima, tarefas embaixo: a mesma ordem da Agenda.
    linha_captura_t compromissos[DIA_MAX];
    int  n_compromissos;

    // As tarefas deste dia, com a MESMA linha da Agenda: duas gramáticas para
    // a mesma coisa obrigariam a aprender o aparelho duas vezes.
    linha_trabalho_t tarefas[DIA_MAX];
    int  n_tarefas;

    // As listas, como cabeçalho do que está dentro.
    grupo_trabalho_t grupos[AGENDA_MAX_GRUPOS];

    // Quantos ficaram de fora do teto: contados, não somem.
    int  mais_compromissos, mais_tarefas;
    int              n_grupos;

    char vazio[40];
    int  cursor;
    char rodape_esq[22], rodape_dir[22];
} vista_dia_t;

void vista_dia(const estado_t *e, vista_dia_t *out);
int  vista_dia_linhas(const estado_t *e);

#endif
