// vista/inicializacao.h — o texto das telas do primeiro uso e da mídia.
// A vista escolhe a palavra; a ui, o pixel; quem decide a fase é uso/.
// RN-A1: estado em poucas palavras, o porquê, e a ação no imperativo.
#ifndef VISTA_INICIALIZACAO_H
#define VISTA_INICIALIZACAO_H

#include "../nucleo/estado.h"
#include "../tela/icones.h"
#include "../tela/fontes.h"

typedef struct {
    char rotulo[32];        // "Recuperação", "Confirmação" — vazio quando não há
    char titulo[40];
    int  pontos;            // espera: 0-3 pontinhos ao lado do título; -1 sem
    char corpo[192];
    // 128: a instrução de bloqueio tem 73 caracteres, e cortada não instrui.
    char alerta[128];

    char opcoes[2][48];
    char notas[2][48];      // a linha miúda sob cada opção
    int  n_opcoes;
    int  selecionada;

    // As três etapas da operação bloqueante, com a que passou marcada e a
    // atual cheia: dizem se ainda anda e quanto falta.
    char etapas[3][32];
    int  n_etapas;
    int  etapa_atual;

    bool icone_sd;          // o microSD físico, desenhado
    bool icone_x;           // com o X de ausente
    bool logo;              // a marca, nas boas-vindas


    // A conclusão: três estados, e nenhum promete rede.
    char estados[3][32];
    int  n_estados;

    // Algo curto que a pessoa vai digitar em outro lugar (o número do
    // aparelho): grande e centrado, como o código de vincular.
    char destaque[24];

    char nome[NOME_UTF8_MAX];   // "Prazer, {nome}."
    char nota[160];             // a linha miúda sob o nome

    // A data e a hora em campos separados: o cursor anda entre eles e o valor
    // sobe e desce.
    char campos[5][7];
    char etiquetas[5][8];
    int  n_campos;
    int  campo_ativo;

    char barra[24];
    char hora[9];           // a barra do sistema é a mesma de toda tela
    int  bateria;
    // -1 esconde o ícone do rádio: num aparelho que funciona offline, ícone
    // permanente seria lembrete do que não faz falta.
    int  wifi;
    icone_id sinc;   // o que a sincronização faz, como forma
    char rodape_esq[24];
    char rodape_dir[24];} vista_inicializacao_t;

void vista_inicializacao(const estado_t *e, vista_inicializacao_t *out);

#endif
