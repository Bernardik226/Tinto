// tela/texto.h — desenhar e medir texto. PURO.
// O `y` é sempre o TOPO da linha, nunca a base: empilhar é somar altura.
#ifndef TELA_TEXTO_H
#define TELA_TEXTO_H

#include "bitmap.h"
#include "fontes.h"

// Devolve o x final — encadear texto na mesma linha é passar o retorno.
int gfx_texto(bitmap_t *bm, int x, int y, fonte_t f, const char *utf8);

// RN-B6: mede sem desenhar. Soma AVANÇOS, então o tracking sobrevive.
int gfx_largura(fonte_t f, const char *utf8);
int gfx_altura_linha(fonte_t f);

// Do topo até a linha de base: para assentar duas fontes na mesma base.
int gfx_ascent(fonte_t f);

// Quantos BYTES cabem em `largura` px, sem partir caractere (RN-B7).
int gfx_cabe(fonte_t f, const char *utf8, int largura);

// Quebra por palavra dentro da caixa. Devolve o y de baixo.
// Uma linha nesta largura, com reticência quando não coube; o recuo é por
// CARACTERE (meio UTF-8 vira caixa vazada).
int gfx_texto_ate(bitmap_t *bm, int x, int y, fonte_t f, const char *texto,
                  int largura);

int gfx_paragrafo(bitmap_t *bm, int x, int y, int largura, int max_linhas,
                  fonte_t f, const char *utf8);

// Cada linha centrada na largura: o bloco de vazio tem uma linha cheia e
// uma curta.
int gfx_paragrafo_centro(bitmap_t *bm, int x, int y, int largura,
                         int max_linhas, fonte_t f, const char *utf8);

// Quantos bytes cabem, desenhados sem reticência. Devolve o offset da
// página seguinte.
int gfx_pagina(bitmap_t *bm, int x, int y, int largura, int max_linhas,
               fonte_t f, const char *utf8, int tamanho);

typedef enum {
    TEXTO_JUSTIFICADO = 0,
    TEXTO_ESQUERDA,
    TEXTO_CENTRO,
    TEXTO_DIREITA,
} texto_alinhamento_t;

int gfx_pagina_alinhada(bitmap_t *bm, int x, int y, int largura,
                        int max_linhas, fonte_t f, const char *utf8,
                        int tamanho, texto_alinhamento_t alinhamento);

// Glifo que a fonte não tem vira caixa vazada (tofu) e conta aqui. Não
// aborta (texto do Google pode trazer qualquer coisa), mas os testes exigem
// zero.
int  gfx_faltantes(void);
void gfx_zera_faltantes(void);

#endif
