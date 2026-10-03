#ifndef UI_OBRA_H
#define UI_OBRA_H
#include "../tela/bitmap.h"
#include "../vista/obra.h"
void tela_obra(bitmap_t *bm, const vista_obra_t *v);
int tela_obra_paradas(bitmap_t *bm, const vista_obra_t *v);
bool tela_obra_capa_area(const vista_obra_t *v,int *x,int *y,int *l,int *a);
#endif
