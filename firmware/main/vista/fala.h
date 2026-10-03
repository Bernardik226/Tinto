// vista/fala.h — T-26, quanto ainda dá para falar este mês.
// A moeda é MINUTO, nunca dinheiro. O número grande é o usado ("12", não
// "12:00", que parece relógio). RN-52: só exibe o que o servidor mandou.
#ifndef VISTA_FALA_H
#define VISTA_FALA_H

#include "../nucleo/estado.h"
#include "../tela/icones.h"

typedef struct {
    char titulo[16];
    char hora[9];
    int  bateria;
    int  wifi;
    icone_id sinc;

    char mes[24];        // "agosto"
    // Só DÍGITOS: a fonte grande não tem letras. A unidade vai miúda ao lado.
    char usados[8];      // "12" · "45" · "–" sem conta
    char unidade[4];     // "min" · "s" (menos de um minuto) · "" sem conta
    char de[28];         // "usados de 120 min" · "sem conta pareada"
    int  pct;            // 0-100, quanto RESTA: a barra esvazia com o uso
    char restam[24];     // "restam 18 min"
    char renova[24];     // "renova em 19 dias"
    char explica[96];

    char rodape_esq[22];
} vista_fala_t;

void vista_fala(const estado_t *e, vista_fala_t *out);

#endif
