// ui/lancador.h — a Home 2x2.
#ifndef UI_LANCADOR_H
#define UI_LANCADOR_H

#include "../tela/bitmap.h"
#include "../vista/lancador.h"

// ── a régua da grade ─────────────────────────────────────────────────
// 106×136, 8 de vão horizontal e 10 de vertical fecham os 240 px com a
// margem de 10 e os 4 px do relevo à direita:
//
//     10 + 106 + 8 + 106 + 4(relevo) = 234, dentro dos 240
#define LANCADOR_CARTAO_L   106
#define LANCADOR_CARTAO_A   136
#define LANCADOR_VAO_X        8
#define LANCADOR_VAO_Y       10

// O respiro interno do cartão e a largura que sobra para o texto. É
// CONTRATO: a legenda e o teste que impede o vazamento medem contra ele.
#define LANCADOR_PAD_X        9
#define LANCADOR_PAD_Y       10
#define LANCADOR_CARTAO_UTIL  (LANCADOR_CARTAO_L - 2 * LANCADOR_PAD_X)

// Onde a grade começa, abaixo do cabeçalho.
#define LANCADOR_GRADE_Y     84

void tela_lancador(bitmap_t *bm, const vista_lancador_t *v);

// Onde está o cartão `i` (0..3): mover o foco refresca só os dois cartões
// que mudaram.
void tela_lancador_cartao(int i, int *x, int *y, int *l, int *a);

#endif
