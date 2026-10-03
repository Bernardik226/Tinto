// vista/anotacoes.h — a lista do que não é evento nem tarefa.
// Anotação não vai ao Google (RN-25) e não se acha pelo dia: a pessoa lembra
// que falou, não quando. Lista por recência; o dia é rótulo, não destino.
#ifndef VISTA_ANOTACOES_H
#define VISTA_ANOTACOES_H

#include "../nucleo/estado.h"
#include "../tela/icones.h"
#include "linhas.h"

typedef struct {
    char titulo[24];
    char hora[9];
    int  bateria;
    int  wifi;
    icone_id sinc;   // o que a sincronização faz, como forma

    // A mesma linha das capturas: hora, título e o começo do corpo.
    linha_captura_t linhas[ANOTACOES_MAX];

    // O rótulo do dia de cada linha, vazio quando continua o dia anterior.
    // Linha de seção própria empurraria a lista a cada grupo.
    char dia[ANOTACOES_MAX][14];   // "HOJE" · "ONTEM" · "SEG 18 AGO"

    int  n;
    int  pagina, paginas;   // 1-based; paginas 1 = não pagina
    char vazio[128];    // o convite, quando não há nenhuma
    int  cursor;
    int16_t alvo;       // qual item o OK abre; -1 = nenhum
    char rodape_esq[22], rodape_dir[22];
} vista_anotacoes_t;

void vista_anotacoes(const estado_t *e, vista_anotacoes_t *out);
int  vista_anotacoes_linhas(const estado_t *e);

#endif
