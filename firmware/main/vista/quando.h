// vista/quando.h — a data de um item, mudada à mão.
// Adiar é o gesto mais comum de uma agenda. Não é a tela de Data e hora (que
// acerta o relógio): os dias vêm prontos, com o nome do dia da semana. Usa o
// card do sistema: informa onde a coisa está, e os destinos agem.
#ifndef VISTA_QUANDO_H
#define VISTA_QUANDO_H

#include "cartao.h"

// O que cada destino FAZ: mudar um rótulo não muda o OK.
typedef enum {
    QUANDO_DIA = 0,     // uma data pronta — o valor está em `datas[i]`
    QUANDO_CALENDARIO,  // escolher no mês
    QUANDO_SEM_DATA,    // tirar o prazo, e a tarefa volta para "sem data"
    QUANDO_HORA,        // um horário pronto — o valor está em `horas[i]`
    QUANDO_DIA_INTEIRO, // tirar a hora, e o evento ocupa o dia
    QUANDO_RELOGIO,     // escolher a hora livre, no mostrador
} quando_acao_t;

typedef struct {
    vista_cartao_t cartao;

    // O que cada linha faz e para que dia, na ordem de `cartao.dest`.
    quando_acao_t acao[CARTAO_DESTINOS_MAX];
    data_t        datas[CARTAO_DESTINOS_MAX];
    char          horas[CARTAO_DESTINOS_MAX][6];
} vista_quando_t;

void vista_quando(const estado_t *e, vista_quando_t *out);

// A HORA de um item que já tem hora (evento ou híbrido).
void vista_horario(const estado_t *e, vista_quando_t *out);

// ── o MOSTRADOR ──────────────────────────────────────────────────────
// Duas casas grandes: ◀▶ troca de casa, ▲▼ muda o número. O par da grade do
// mês: lá qualquer dia, aqui qualquer hora.
typedef struct {
    char titulo[20];
    char hora[9];
    int  bateria;
    int  wifi;
    icone_id sinc;

    char kicker[28];   // "MUDAR A HORA"
    char nome[40];     // o item que vai mudar

    char casa[2][4];   // "07" e "30", como se desenham
    int  campo;        // 0 = hora, 1 = minuto — qual está sob o cursor

    char rodape_esq[22], rodape_dir[26];
} vista_mostrador_t;

void vista_escolher_hora(const estado_t *e, vista_mostrador_t *out);

#endif
