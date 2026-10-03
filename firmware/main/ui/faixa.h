// ui/faixa.h — a peça que cobre a tela, e a única que pode.
//
// Falar, saltar de tela e recusar um gesto aparecem por cima, e usam a mesma
// faixa, que garante:
//   1. Ancorada no rodapé: nunca alcança a barra nem cobre o cartaz.
//   2. Limpa o próprio chão: o conteúdo de trás não atravessa.
//   3. Não toca pixel de fora: é parcial de retângulo fixo, o quadro mais
//      barato do painel.
//
//     faixa_t f = faixa_abre(bm, FAIXA_VOZ_A, true);
//     ... desenha dentro de f.x, f.y, f.util ...
//     faixa_botoes(bm, &f, "solte pra pausar", "OK terminei");
//     faixa_fecha(bm, &f);
//
// O negativo se aplica por último (`faixa_fecha`), como no `chrome_rodape`.
#ifndef UI_FAIXA_H
#define UI_FAIXA_H

#include "grid.h"

// ── as três alturas ──────────────────────────────────────────────────
// Cada uma é o que o conteúdo daquele uso exige.

// VOZ · o tempo grande com a onda, duas linhas de miúda e os botões.
#define FAIXA_VOZ_A     96

// SALTOS · "ir para" e os três destinos.
#define FAIXA_SALTOS_A  96

// AVISO · ícone, título, DUAS linhas de miúda e os botões. 88: com 72 a
// segunda linha caía nos botões (a conta está no teste).
#define FAIXA_AVISO_A   88

// Dois pixels, o peso do FILETE_GROSSO: a faixa é outra superfície.
#define FAIXA_TOPO      2

typedef struct {
    ret_t r;         // o retângulo inteiro, ancorado no rodapé
    int16_t x;       // onde o conteúdo começa — a margem já aplicada
    int16_t y;       // idem, já abaixo do filete de topo
    int16_t util;    // a largura que o conteúdo tem
    bool  negativo;  // estado ativo: inverte tudo no fecha
} faixa_t;

// Reserva o retângulo, limpa o chão e desenha o filete. Devolve onde o
// conteúdo começa.
faixa_t faixa_abre(bitmap_t *bm, int altura, bool negativo);

// O espaço vertical do conteúdo, até os botões. Quem desenha pergunta aqui
// em vez de somar alturas à mão (o erro só aparece na prova em PNG).
int faixa_conteudo_a(const faixa_t *f);

// A linha de botões, no pé: saída à esquerda, o que o OK faz à direita.
void faixa_botoes(bitmap_t *bm, const faixa_t *f,
                  const char *esq, const char *dir);

// Aplica o negativo, se houver. Sempre a última chamada.
void faixa_fecha(bitmap_t *bm, const faixa_t *f);

#endif
