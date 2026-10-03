// tela/fontes.h — as fontes, geradas no tamanho de uso. PURO.
// Geradas por firmware/ferramentas/fontes.py com hinting, a caixa medindo a
// TINTA; as armadilhas estão comentadas no gerador.
#ifndef TELA_FONTES_H
#define TELA_FONTES_H

#include "../nucleo/tipos.h"

// A ORDEM é a da tabela `FONTES` do gerador: o `fontes.c` indexa este enum
// por posição. Quem manda nos tamanhos é a tabela.
typedef enum {
    F_MIUDA = 0,   // 11 px — rótulos, selos, rodapé
    F_CORPO,       // 14 px — o texto do sistema
    F_CORPO_P,     // 12 px — as linhas de tarefa, um degrau abaixo
    F_TITULO,      // 17 px bold — título de item, cartaz
    F_ENORME,      // 46 px bold — relógio e cartaz. SÓ DÍGITOS
    F_CITACAO,     // 14 px itálico — a transcrição crua (RN-28)

    // DejaVu SERIF bold, a serifa editorial. Mesmo corpo do F_TITULO: trocar
    // uma pela outra não faz a composição pular. ~4 KB de flash; usada com
    // contenção.
    F_EDITORIAL,
    F_XADREZ,      // 27 px — somente as doze peças do tabuleiro
    F_LEITOR_SERIF_P, F_LEITOR_SERIF_M, F_LEITOR_SERIF_G,
    F_LEITOR_SANS_P,  F_LEITOR_SANS_M,  F_LEITOR_SANS_G,
    F_LEITOR_MONO_P,  F_LEITOR_MONO_M,  F_LEITOR_MONO_G,
    F_LEITOR_LITERATA_P, F_LEITOR_LITERATA_M, F_LEITOR_LITERATA_G,
    F_LEITOR_ATKINSON_P, F_LEITOR_ATKINSON_M, F_LEITOR_ATKINSON_G,
    F_LEITOR_INTER_P, F_LEITOR_INTER_M, F_LEITOR_INTER_G,
    F_LEITOR_SOURCE_P, F_LEITOR_SOURCE_M, F_LEITOR_SOURCE_G,
    F_LEITOR_SERIF_FORTE_P, F_LEITOR_SERIF_FORTE_M, F_LEITOR_SERIF_FORTE_G,
    F_LEITOR_SANS_FORTE_P, F_LEITOR_SANS_FORTE_M, F_LEITOR_SANS_FORTE_G,
    F_LEITOR_MONO_FORTE_P, F_LEITOR_MONO_FORTE_M, F_LEITOR_MONO_FORTE_G,
    F_LEITOR_LITERATA_FORTE_P, F_LEITOR_LITERATA_FORTE_M, F_LEITOR_LITERATA_FORTE_G,
    F_LEITOR_ATKINSON_FORTE_P, F_LEITOR_ATKINSON_FORTE_M, F_LEITOR_ATKINSON_FORTE_G,
    F_LEITOR_INTER_FORTE_P, F_LEITOR_INTER_FORTE_M, F_LEITOR_INTER_FORTE_G,
    F_LEITOR_SOURCE_FORTE_P, F_LEITOR_SOURCE_FORTE_M, F_LEITOR_SOURCE_FORTE_G,

    F_QUANTAS
} fonte_t;

typedef struct {
    uint16_t codigo;   // ponto de código Unicode
    uint8_t  l, a;     // caixa da TINTA, não do vetor
    int8_t   esq;      // deslocamento horizontal
    int8_t   topo;     // acima da linha de base
    uint8_t  avanco;   // quanto o cursor anda — inclui os side bearings
    uint16_t offset;   // onde os bits começam no blob
} glifo_t;

typedef struct {
    const glifo_t *glifos;
    const uint8_t *bits;
    uint16_t       n;
    uint8_t        altura_linha;
    uint8_t        altura_x;
    uint8_t        ascent;
} fonte_dados_t;

extern const fonte_dados_t FONTES[];

// NULL quando o glifo não existe — e quem chama TEM que reagir.
const glifo_t *fonte_glifo(fonte_t f, uint32_t codigo);

#endif
