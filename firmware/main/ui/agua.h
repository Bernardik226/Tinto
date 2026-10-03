// ui/agua.h — a marca d'água do vazio, a mesma em toda tela.
// Responde "está vazio ou quebrou?". ESMAECIDA (`gfx_esmaece`): é legenda de
// um espaço, e em tinta cheia pareceria um compromisso visto de longe.
// CENTRADA na zona vazia: no topo, leria como a primeira linha de uma lista
// que não veio.
#ifndef UI_AGUA_H
#define UI_AGUA_H

#include "../tela/bitmap.h"

// A frase centrada no retângulo, esmaecida; `\n` quebra linha. Devolve o y
// de baixo do bloco.
int ui_agua(bitmap_t *bm, int x, int y, int l, int a, const char *frase);

// A altura que `ui_agua` vai ocupar, para reservar o espaço antes.
int ui_agua_altura(const char *frase);

#endif
