#ifndef UI_GRAVADOR_H
#define UI_GRAVADOR_H

#include "../vista/gravador.h"
#include "../tela/bitmap.h"


// O gravador por cima do que já está na tela.
void tela_gravador_popover(bitmap_t *bm, const vista_grav_t *v);

// O retângulo da caixa, sombra incluída. Ver tela_menu_popover_area.
void tela_gravador_popover_area(bitmap_t *bm, const vista_grav_t *v,
                                int *x, int *y, int *l, int *a);

#endif
