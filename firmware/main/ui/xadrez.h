#ifndef UI_XADREZ_H
#define UI_XADREZ_H

#include "../tela/bitmap.h"
#include "../vista/xadrez.h"

void tela_jogos(bitmap_t *bm, const vista_xadrez_t *v);
void tela_xadrez(bitmap_t *bm, const vista_xadrez_t *v);
void tela_xadrez_menu_area(bitmap_t *bm, const vista_xadrez_t *v,
                           int *x, int *y, int *l, int *a);

#endif
