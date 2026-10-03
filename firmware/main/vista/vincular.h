// vista/vincular.h — o QR e o código de seis letras.
// Antes de conectar, apresenta o app e o código; depois, o mesmo QR segue em
// Minha conta (o app gerencia Acervo, aparelhos e voz). O código VEM do
// servidor: inventar seis letras seria mostrar um que nunca vale.
#ifndef VISTA_VINCULAR_H
#define VISTA_VINCULAR_H

#include "campos.h"
#include "../nucleo/estado.h"

typedef struct {
    char titulo[20];
    char hora[9];
    int  bateria;
    int  wifi;
    icone_id sinc;   // o que a sincronização faz, como forma

    // As seis letras, ou vazio enquanto o servidor não respondeu.
    char codigo[8];

    // Esperando a resposta: diferente de "ainda não pedi".
    bool esperando;

    // Nada pedido ou expirou: a tela oferece GERAR. Pedir ao abrir gastava um
    // código de uso único a cada olhada.
    bool pode_gerar;

    // Conta já conectada: só a porta permanente para o app, sem código.
    bool somente_app;

    // "vale 5 min" · "expirou". Em MINUTOS: segundos seriam trezentos
    // redesenhos.
    char prazo[20];

    // Ainda sem registro no servidor: o código só nasce depois.
    bool sem_token;

    // O MAC do eFuse: identifica o aparelho ao admin antes do cadastro.
    char id[24];

    // Os três passos são TEXTO: moram na vista.
    char passo[3][44];

    // Os pontinhos da espera pelo pareamento.
    int  pontos;

    char rodape_esq[22], rodape_dir[22];
} vista_vincular_t;

void vista_vincular(const estado_t *e, vista_vincular_t *out);

#endif
