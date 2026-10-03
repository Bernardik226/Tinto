// vista/leitor.h — a página, em conteúdo puro.
// O rodapé "◀ 38% · 46/121 ▶" responde quanto foi, onde estou e quanto falta.
#ifndef VISTA_LEITOR_H
#define VISTA_LEITOR_H

#include "../nucleo/estado.h"
#include "../tela/icones.h"
#include "../tela/fontes.h"

typedef struct {
    char titulo[40];       // o da OBRA, na barra: é ele que dá o lugar
    char hora[9];
    int  bateria;
    int  wifi;
    icone_id sinc;

    char texto[LEITOR_PAGINA];
    char regua[28];        // "◀ 38% · 46/121 ▶"
    bool barra_progresso;
    int progresso_pct;
    bool capa;
    fonte_t fonte;
    leitor_alinhamento_t alinhamento;

    // O fim PERGUNTA, não conclui: chegar ao fim folheando não é ter lido.
    bool no_fim;
    char pergunta[40];     // "Você chegou ao fim"
    char sim[26], nao[26];

    // Cópia danificada não abre: oferece baixar de novo se houver origem
    // online; senão, explica que ela volta pelo app.
    char erro[40];

    char rodape_esq[22];
} vista_leitor_t;

void vista_leitor(const estado_t *e, vista_leitor_t *out);
fonte_t vista_fonte_leitor(leitor_tamanho_t tam, leitor_familia_t fam);

#endif
