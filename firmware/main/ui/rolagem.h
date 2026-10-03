// ui/rolagem.h — o corpo que não cabe na tela (detalhe, Conferir,
// resultado). Mede num bitmap invisível, desloca pelo cursor e desenha as
// setas. PURO: recebe alturas e devolve números.
#ifndef UI_ROLAGEM_H
#define UI_ROLAGEM_H

#include "../tela/bitmap.h"

// ── o bitmap de rascunho, UM para todo o sistema ─────────────────────
// Medir desenhando: a função que pinta é a que mede. Seis cópias estáticas
// custavam 73 KB de RAM interna e o cartão parou de montar. O desenho é
// sequencial: nunca há duas medições ao mesmo tempo.
bitmap_t *rolagem_rascunho(const bitmap_t *como);

// Pouco mais de três linhas de corpo por parada: não pula texto e não
// custa dez toques.
#define ROLAGEM_PASSO 56

// Paradas de LEITURA: uma quando cabe, mais uma por passo de transbordo.
int rolagem_paradas(int alto, int area);

// Onde o corpo começa, dado o cursor. Nunca além do fim.
int rolagem_desloc(int parada, int alto, int area);

// Texto contínuo (Detalhe e Conferir): avança em trechos curtos, sem
// páginas numeradas.
int rolagem_suave_paradas(int alto, int area);
int rolagem_suave_desloc(int parada, int alto, int area);
void rolagem_setas_laterais(bitmap_t *bm, int desloc, int alto, int area,
                            int topo, int fundo);

// As setas nas bordas, com o chão limpo atrás: senão saem por cima de uma
// palavra.
void rolagem_setas(bitmap_t *bm, int desloc, int alto, int area,
                   int topo, int fundo);

#endif
