// ui/ajustes.h — a raiz de Ajustes: cinco DESTINOS editoriais (título em
// serifa, descrição embaixo, a linha inteira invertendo no foco). Não é
// `tela_menu`: é "para onde eu vou", não "o que eu mudo aqui".
#ifndef UI_AJUSTES_H
#define UI_AJUSTES_H

#include "../vista/menu.h"
#include "../tela/bitmap.h"

void tela_ajustes(bitmap_t *bm, const vista_menu_t *v);

#endif
