// vista/resultado.h — o que a gravação DE FATO criou. PURO.
// O Conferir é a proposta; esta só existe DEPOIS da resposta, e cada linha
// diz o que aconteceu com ELA: sucesso parcial não se disfarça de total.
#ifndef VISTA_RESULTADO_H
#define VISTA_RESULTADO_H

#include "../nucleo/estado.h"
#include "../tela/icones.h"

typedef struct {
    char titulo[20];
    char hora[9];
    int  bateria;
    int  wifi;
    icone_id sinc;

    char frase[28];    // "2 ações concluídas"
    char sub[44];      // "A gravação criou os dois itens abaixo."
    bool tudo_certo;   // a marca geral só quando TODAS deram certo

    struct {
        // Título e destino inteiros: é a última tela do fluxo de voz.
        char oque[68];
        char destino[40];
        char estado[12];   // "criado" · "criada" · "guardada" · "falhou"
        bool ok;
    } linhas[RESULTADOS_MAX];
    int  n;

    // A parada de leitura. A tela é só texto: todas as paradas rolam.
    int  cursor;

    char rodape_esq[22], rodape_dir[22];
} vista_resultado_t;

void vista_resultado(const estado_t *e, vista_resultado_t *out);

#endif
