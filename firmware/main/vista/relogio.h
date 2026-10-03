// vista/relogio.h — Ajustes → Data e hora.
// RN-6G: aqui o contador vira hora, à mão ou pela rede. Nunca os dois: o
// sync apagaria o ajuste manual sem avisar.
#ifndef VISTA_RELOGIO_H
#define VISTA_RELOGIO_H

#include "../nucleo/estado.h"
#include "../tela/icones.h"

typedef struct {
    char titulo[20];
    char hora[9];
    int  bateria;
    // -1 esconde o ícone do rádio: num aparelho que funciona offline, ícone
    // permanente seria lembrete do que não faz falta.
    int  wifi;
    icone_id sinc;   // o que a sincronização faz, como forma

    bool pela_rede;

    // Com conta, o fuso vem do Google já com horário de verão: aqui é leitura.
    bool fuso_do_google;    // a rede manda: os campos viram leitura
    bool editavel;     // o contrário — e é o app que move o cursor

    // O cursor está no interruptor, navegável nos dois sentidos (já foi de mão
    // única).
    bool no_interruptor;

    // O fuso, editável enquanto não há conta: sem ele o relógio fica em UTC
    // com cara de certo. Quando o backend manda, sobrescreve.
    bool no_fuso;
    char fuso[10];      // "-03:00" · "+00:00"
    char interruptor[12];   // "ligada" · "desligada"

    // dia · mês · ano · hora · minuto, na ordem em que se lê
    char valor[5][7];
    int  campo;        // qual está sob o cursor, -1 quando não se edita

    char nota[80];
    char rodape_esq[22], rodape_dir[22];
} vista_relogio_t;

void vista_relogio(const estado_t *e, vista_relogio_t *out);

#endif
