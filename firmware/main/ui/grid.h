// ui/grid.h — a geometria do sistema, num lugar só. PURO.
//
// Toda função recebe um `y` e devolve o de baixo (o empilhamento). Este
// arquivo resolve a outra metade: onde cada coisa pode existir. Regra:
// nenhuma peça flutua; todo retângulo tem nome aqui.
//
//     0 ┌────────────────────────┐
//       │  BARRA    23 px        │  só chrome_barra escreve
//    23 ├────────────────────────┤
//       │  MIOLO                 │  a tela desenha aqui, e só aqui
//       │  ┌──────────────────┐  │
//       │  │  FAIXA   96 px   │  │  voz · saltos · exceção — A MESMA
//   300 └──┴──────────────────┴──┤
//       │  RODAPÉ   20 px        │  só chrome_rodape escreve
//   416 └────────────────────────┘
//
// Um só lugar cobre a tela (a faixa): duas peças nunca disputam um pixel.
#ifndef UI_GRID_H
#define UI_GRID_H

#include <stdbool.h>
#include "../tela/bitmap.h"
#include "chrome.h"

// ── as margens ───────────────────────────────────────────────────────
// Uma margem, um valor.
#define GRID_MARGEM   10
#define GRID_UTIL     (TELA_L - 2 * GRID_MARGEM)

// ── as margens que fogem do padrão, e por quê ────────────────────────
// Estar aqui as torna decisões. Quem não está nesta lista usa `GRID_MARGEM`.

// A raiz de Ajustes e as listas de destino: a linha inteira inverte, e dez
// de margem comeriam a largura que faz os títulos caberem.
#define GRID_MARGEM_DESTINO  6

// Sete colunas em 240 px: com 6 de margem cada uma tem 32 e o número centra.
#define GRID_MARGEM_GRADE    6

// O teclado tem dez colunas: mesma conta, mais apertada.
#define GRID_MARGEM_TECLADO  8

// As telas de LEITURA (repouso, aviso, primeiro uso) respiram mais: a única
// margem escolhida por conforto, não por caber.
#define GRID_MARGEM_TEXTO   14

// ── o DESTINO: a peça que leva a outra tela ──────────────────────────
// Ícone, título, descrição embaixo, seta, filete. Focada, a linha inteira
// inverte (em 1 bit, seleção é inversão).
//
//     ┌──────────────────────────────────────┐
//     │ [22]  Minha conta                  > │  ← 48 px, ou mais se o
//     │       Google, voz e este Tinto       │     título quebrar
//     ├──────────────────────────────────────┤
//
// Medido na régua: "Armazenamento" tem 172 px na serifa, e a coluna precisa
// sobrar 176 depois do ícone e da seta.
#define GRID_DESTINO_A     48   // altura mínima da linha
#define GRID_DESTINO_ICONE 22   // o ícone, e é o do desenho da fase 3
#define GRID_DESTINO_COL   26   // onde o texto começa: ícone + 4 de ar
#define GRID_DESTINO_PAD    4   // o respiro de cima e de baixo

// ── as três zonas ───────────────────────────────────────────────────
#define GRID_BARRA_Y    0
#define GRID_BARRA_A    BARRA_A

#define GRID_MIOLO_Y    (GRID_BARRA_Y + GRID_BARRA_A)
#define GRID_MIOLO_A    (TELA_A - GRID_BARRA_A - RODAPE_A)

#define GRID_RODAPE_Y   (TELA_A - RODAPE_A)
#define GRID_RODAPE_A   RODAPE_A

// ── a faixa ancorada ────────────────────────────────────────────────
// 96 px: o que a voz precisa mostrar ao mesmo tempo. Colada no rodapé, onde
// está o polegar, e sem cobrir o cartaz.
#define GRID_FAIXA_A    96
#define GRID_FAIXA_Y    (GRID_RODAPE_Y - GRID_FAIXA_A)

typedef struct { int16_t x, y, l, a; } ret_t;

static inline ret_t grid_barra(void)
{
    return (ret_t){ 0, GRID_BARRA_Y, TELA_L, GRID_BARRA_A };
}

static inline ret_t grid_miolo(void)
{
    return (ret_t){ 0, GRID_MIOLO_Y, TELA_L, GRID_MIOLO_A };
}

static inline ret_t grid_rodape(void)
{
    return (ret_t){ 0, GRID_RODAPE_Y, TELA_L, GRID_RODAPE_A };
}

// A faixa padrão; outra altura é `grid_faixa_de`, sempre ancorada.
static inline ret_t grid_faixa(void)
{
    return (ret_t){ 0, GRID_FAIXA_Y, TELA_L, GRID_FAIXA_A };
}

// Uma faixa de outra altura, encostada no rodapé. O teto é o miolo: a
// barra nunca some.
static inline ret_t grid_faixa_de(int altura)
{
    if (altura > GRID_MIOLO_A) altura = GRID_MIOLO_A;
    if (altura < 0)            altura = 0;
    return (ret_t){ 0, (int16_t)(GRID_RODAPE_Y - altura), TELA_L, (int16_t)altura };
}

static inline bool grid_contem(ret_t r, int x, int y)
{
    return x >= r.x && x < r.x + r.l && y >= r.y && y < r.y + r.a;
}

#endif
