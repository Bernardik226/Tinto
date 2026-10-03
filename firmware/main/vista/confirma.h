// vista/confirma.h — a pergunta destrutiva de duas opções.
// Nasce no NÃO (RN-6C) e diz o que NÃO se perde antes de perguntar. A de
// formatar vive no fluxo de inicialização, fora da pilha (RN-6F), e não usa
// esta struct.
#ifndef VISTA_CONFIRMA_H
#define VISTA_CONFIRMA_H

#include "campos.h"
#include "../nucleo/estado.h"

typedef struct {
    char titulo[20];
    char hora[9];
    int  bateria;
    int  wifi;
    icone_id sinc;   // o que a sincronização faz, como forma

    char pergunta[44];
    char explica[168];    // o que acontece, e sobretudo o que NÃO acontece
    char nao[24], sim[24];
    int  cursor;          // 0 = não, e é onde ela sempre nasce

    char rodape_esq[22], rodape_dir[22];
} vista_confirma_t;

// Desconectar a conta Google deste aparelho.
void vista_desconectar(const estado_t *e, vista_confirma_t *out);

// Esquecer a rede salva: a destrutiva repete o nome da rede.
void vista_esquecer_rede(const estado_t *e, vista_confirma_t *out);

// Restaurar o aparelho: a mesma confirmação das outras duas destrutivas.
void vista_restaurar(const estado_t *e, vista_confirma_t *out);
void vista_descartar_obra(const estado_t *e, vista_confirma_t *out);

// Apagar uma rotina: a pergunta diz que sai a SÉRIE, não o dia.
void vista_apagar_rotina(const estado_t *e, vista_confirma_t *out);

#endif
