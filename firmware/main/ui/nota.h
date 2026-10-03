// ui/nota.h — T-16, o detalhe do item.
#ifndef UI_NOTA_H
#define UI_NOTA_H

#include "../vista/nota.h"
#include "../tela/bitmap.h"

// Onde começa a coluna do VALOR, medida pelo rótulo mais largo: fixa,
// "QUANDO" encostava no valor.
int tela_nota_coluna(const vista_nota_t *v);

// Paradas de cursor: as de LEITURA (rolagem, só quando transborda) mais os
// botões. Geometria: o app pergunta aqui até onde o ▼ anda.
int  tela_nota_paradas(bitmap_t *bm, const vista_nota_t *v);

// Nesta parada, o FIM do corpo aparece? O teste prova que rolar alcança o
// fim.
bool tela_nota_fim_visivel(bitmap_t *bm, const vista_nota_t *v, int parada);

void tela_nota(bitmap_t *bm, const vista_nota_t *v);

// Quantas linhas o título ocupa: prova que não é cortado.
int  tela_nota_titulo_linhas(bitmap_t *bm, const vista_nota_t *v);

#endif
