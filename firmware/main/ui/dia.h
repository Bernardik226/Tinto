#ifndef UI_DIA_H
#define UI_DIA_H

#include "../vista/dia.h"
#include "../tela/bitmap.h"

void tela_dia(bitmap_t *bm, const vista_dia_t *v);

// Quantas páginas o dia ocupa e em qual está o cursor. A quebra é por
// LINHA: página que corta uma linha some com ela das duas.
int  tela_dia_paginas(bitmap_t *bm, const vista_dia_t *v);
int  tela_dia_pagina(bitmap_t *bm, const vista_dia_t *v);

#endif
