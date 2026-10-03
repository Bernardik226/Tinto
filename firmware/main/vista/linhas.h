// vista/linhas.h — os tipos de linha que MAIS DE UMA tela usa.
//
//   linha de captura → capturas · dia
//   linha de ação    → tarefa · ajustes · conexões
#ifndef VISTA_LINHAS_H
#define VISTA_LINHAS_H

#include "../nucleo/tipos.h"
#include "../tela/icones.h"

typedef struct {
    char hora[9];      // "07:12"
    char titulo[40];
    char sub[40];      // "anotação · 0:23" · "tarefa · ↗ no Google"

    // A tela ignora: é para o app saber em quem o ▶ entra.
    int16_t indice;

    // A linha se MARCA: o híbrido na régua. Evento não tem caixa (RN-2B):
    // "feito" não existe no Calendar.
    bool caixa;
    bool feita;
} linha_captura_t;

typedef struct {
    char texto[28];    // "mudar a data"
    char valor[20];    // "seg 10 ago" ou ""

    // O ícone deixa o olho pular direto para a linha. ICO_NENHUM = começa no
    // texto.
    icone_id icone;

    // Um segundo ícone colado no primeiro: o cadeado ao lado do sinal ("vai
    // pedir senha?"). ICO_NENHUM = não desenha.
    icone_id icone2;

    // Esta linha INFORMA e o cursor pula: parar onde o OK não faz nada é
    // oferecer ação que não existe.
    bool so_leitura;
    bool marcado;      // opção ativa; desenhada, não depende de glifo
} linha_acao_t;

#endif
