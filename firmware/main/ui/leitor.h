// ui/leitor.h — a página no vidro: texto em serifa, margens largas e a
// régua no rodapé. Sem cursor nem ícone; a tela inteira é o conteúdo.
#ifndef UI_LEITOR_H
#define UI_LEITOR_H

#include "../tela/bitmap.h"
#include "../vista/leitor.h"

void tela_leitor(bitmap_t *bm, const vista_leitor_t *v);

#endif
