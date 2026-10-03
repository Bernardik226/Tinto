// vista/acervo.h — a Biblioteca, em conteúdo puro.
//
//   · CONTINUAR LENDO — a última obra ABERTA, com a porcentagem lida.
//   · a legenda de cada obra ("Livro · 12%"): porcentagem só no que foi
//     começado; 0% faria toda obra nova parecer abandonada.
//
// Os dois progressos ficam separados: baixando mostra a transferência; no
// cartão, a leitura.
#ifndef VISTA_ACERVO_H
#define VISTA_ACERVO_H

#include "../nucleo/estado.h"
#include "../tela/icones.h"

#define ACERVO_LINHAS 12   // o lote da especificação

// O que a marca da linha promete quando o OK for apertado.
typedef enum {
    ACERVO_BAIXAR = 0,   // só online: o OK começa a transferência
    ACERVO_VINDO,        // baixando: o OK não faz nada, ela está vindo
    ACERVO_LOCAL,        // aqui: o OK abre o leitor
    ACERVO_SO_MEMORIA,   // abre, mas a origem já saiu do Acervo online
} acervo_marca_t;

typedef struct {
    char titulo[64];
    char legenda[32];      // "Livro · 12%"
    acervo_marca_t marca;
    int  pct;              // o que a barra desenha, ou -1
    int16_t indice;        // qual obra — a tela ignora
} linha_obra_t;

typedef struct {
    char titulo[12];       // "Acervo"
    char hora[9];
    int  bateria;
    int  wifi;
    icone_id sinc;

    // Some quando nenhuma obra foi aberta: "Continuar lendo" sem obra é convite
    // para lugar nenhum.
    bool tem_destaque;
    bool destaque_focado;
    struct { char titulo[64]; char legenda[32]; int16_t indice; } destaque;

    linha_obra_t linhas[ACERVO_LINHAS];
    int n;
    int mais;              // quantas ficaram fora do lote

    char vazio[40];        // "Seu Acervo está vazio"
    char vazio_como[48];   // e COMO sair do vazio
    char erro[40];         // "Não foi possível baixar"

    int cursor;
    char rodape_esq[22], rodape_dir[22];
} vista_acervo_t;

void vista_acervo(const estado_t *e, vista_acervo_t *out);

#endif
