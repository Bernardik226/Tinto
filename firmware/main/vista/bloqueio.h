// vista/bloqueio.h — T-01, o aparelho parado.
// E-ink segura a imagem com o chip dormindo: o repouso é a cara do objeto na
// mesa. Mostra o dia, sem cursor nem navegação.
#ifndef VISTA_BLOQUEIO_H
#define VISTA_BLOQUEIO_H

#include "../nucleo/estado.h"
#include "../tela/icones.h"

typedef struct {
    char hora[9];         // "09:14" — em 46 px, o que se lê de 2 m
    // "terça, 1 de setembro", POR EXTENSO: é lido de longe, e aqui sobra
    // espaço.
    char data[32];
    char proximo[38];     // "14:00 · Dentista" ou "nada mais hoje"
    char falta[16];       // "em 4h46"
    char onde[40];        // o local do compromisso, quando há um

    // Como sair (só o Power): aparelho que não responde nem explica parece
    // quebrado.
    char saida[28];
    // O resto de HOJE pelo nome: primeiro o que tem hora. Sem contador — número
    // solto era onde nascia dado falso.
    char resto[3][40];    // "14:00 · Dentista", "· Pagar o boleto"
    int  n_resto;
    char mais[20];        // "+ 2 por fazer" · "+ 3 mais" — vazio quando coube
    char aviso[28];       // "desbloqueie para gravar"
    int  bateria;
    // -1 esconde o ícone do rádio: num aparelho que funciona offline, ícone
    // permanente seria lembrete do que não faz falta.
    int  wifi;
    icone_id sinc;   // o que a sincronização faz, como forma
    bool carregando;
} vista_bloqueio_t;

void vista_bloqueio(const estado_t *e, vista_bloqueio_t *out);

#endif
