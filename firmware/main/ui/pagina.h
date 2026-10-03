// ui/pagina.h — a paginação do sistema.
//
// Não há scroll: rolar 56 px por toque deixava o texto sobre o próprio
// fantasma no parcial. Página esconde, por isso o CONTADOR ("2 / 3"): tem
// mais, quanto mais, onde estou. A conta mora aqui para toda tela.
//
//     nada se perde por não caber.
//
// Título que não cabe vira página, não reticências.
#ifndef UI_PAGINA_H
#define UI_PAGINA_H

#include "../tela/bitmap.h"
#include <stdbool.h>

// Quantas páginas o conteúdo ocupa. Nunca menos que uma.
int  pagina_total(int alto, int area);

// Em qual página cai um deslocamento, contada de 1, nunca além do total.
int  pagina_atual(int desloc, int alto, int area);

// O contador, no centro do rodapé. DEPOIS de `chrome_rodape`, que limpa a
// faixa.
void pagina_marca(bitmap_t *bm, int atual, int total);

#endif
