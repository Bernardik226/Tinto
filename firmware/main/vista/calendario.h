// vista/calendario.h — T-20, o mês.
// Navegação, não destino: é aqui que se vê outro dia. Em 1 bit não há cor,
// então as marcas têm FORMAS diferentes (evento e tarefa), em cantos
// distintos da célula.
#ifndef VISTA_CALENDARIO_H
#define VISTA_CALENDARIO_H

#include "../nucleo/estado.h"
#include "../tela/icones.h"

typedef struct {
    // A barra diz "Agenda": o calendário é a segunda vista da Agenda, mesma
    // moldura, outro cabeçalho.
    char titulo[16];        // "Calendário"
    char mes[20];           // "Agosto 2026" — o cabeçalho
    char nav_mes[12];       // "◀▶ dia"

    // Quantos eventos e tarefas o dia sob o cursor tem, em palavras: ensina as
    // marcas no contexto e some quando não há o que ensinar.
    char contagem[28];      // "2 eventos · 3 tarefas"
    char hora[9];
    int  bateria;
    // -1 esconde o ícone do rádio: num aparelho que funciona offline, ícone
    // permanente seria lembrete do que não faz falta.
    int  wifi;
    icone_id sinc;   // o que a sincronização faz, como forma

    int  primeiro_dw;       // dia da semana do dia 1 (0 = domingo)
    int  n_dias;            // dias do mês
    int  hoje;              // 0 = hoje não é neste mês
    int  cursor_dia;        // o dia sob o cursor

    bool tem_tarefa[32];   // índice 1..31
    bool tem_evento[32];

    // ── a tira do dia sob o cursor ──────────────────────────────────────
    // Responde "tem o quê aí?" sem sair da grade: é o que faz o ◀▶ valer a pena.
    char tira_rot[24];      // "ter 26 · hoje"
    char tira[3][40];       // "15:00 Dentista" — as primeiras do dia
    int  n_tira;
    char tira_mais[24];     // "+ 2 por fazer"

    // O mês inteiro sem nada: a grade continua e a marca d'água entra no lugar
    // da tira.
    char vazio[40];         // "Nada marcado em setembro."

    // 28 BYTES: os símbolos da legenda são multibyte.
    char rodape_esq[28], rodape_dir[22];
} vista_cal_t;

void vista_calendario(const estado_t *e, vista_cal_t *out);

// A mesma grade para ESCOLHER um dia: o cabeçalho vira o item que muda e a
// tira, a confirmação. Rota própria: aqui o OK carimba a data; lá abre o dia.
void vista_escolher_dia(const estado_t *e, vista_cal_t *out);

#endif
