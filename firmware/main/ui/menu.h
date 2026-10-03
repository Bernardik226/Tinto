#ifndef UI_MENU_H
#define UI_MENU_H

#include "../vista/menu.h"
#include "../tela/bitmap.h"

// Tela cheia: as listas de Ajustes.
void tela_menu(bitmap_t *bm, const vista_menu_t *v);

// A faixa da manchete: a área do parcial enquanto os pontinhos andam.
void tela_menu_manchete_area(bitmap_t *bm, int *x, int *y, int *l, int *a);

// Por cima do que já está lá: a gaveta.
void tela_menu_popover(bitmap_t *bm, const vista_menu_t *v);

// O retângulo da caixa, sombra incluída. O e-ink precisa APAGAR a região
// antes: tinta assentada pela waveform completa não sai num parcial, e a
// caixa ficava transparente (EINK.md §5.6).
void tela_menu_popover_area(bitmap_t *bm, const vista_menu_t *v,
                            int *x, int *y, int *l, int *a);

#endif
