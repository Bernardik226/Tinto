// ui/acervo.h — a Biblioteca no vidro.
// O cartaz de "Continuar lendo" com capa de 50 px, linhas de 67 px com capa
// de 38, e a marca do estado à direita. A capa é a inicial da obra em
// serifa: numa lista de títulos parecidos, é o que o olho pega primeiro.
#ifndef UI_ACERVO_H
#define UI_ACERVO_H

#include "../tela/bitmap.h"
#include "../vista/acervo.h"

void tela_acervo(bitmap_t *bm, const vista_acervo_t *v);

// Onde a linha `i` está: o motor suja só essa faixa quando o cursor anda.
void tela_acervo_linha_area(const vista_acervo_t *v, int i,
                            int *x, int *y, int *l, int *a);

// Quantas linhas cabem, com e sem o cartaz. Geometria, por isso aqui: sem
// ela a última linha caía por cima do rodapé.
int tela_acervo_cabem(const vista_acervo_t *v);

// `i == -1` devolve a capa do destaque; 0..n-1, a da linha.
bool tela_acervo_capa_area(const vista_acervo_t *v, int i,
                           int *x, int *y, int *l, int *a);

#endif
