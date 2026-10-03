// ui/faixa.c — ver faixa.h.
#include "faixa.h"
#include "../tela/texto.h"
#include <string.h>

// O mesmo respiro em cima e embaixo: diferentes leem como desalinho.
#define RESPIRO   7

// A altura dos botões: miúda + respiro. Quem precisa saber pergunta ao
// `faixa_t`.
#define BOTOES_A  (gfx_altura_linha(F_MIUDA) + RESPIRO)

// O teto de um rótulo de botão: o dos títulos (RN-B7).
#define ROTULO_MAX 64

// Copia até `bytes` RECUANDO até a fronteira de caractere: o corte do
// tamanho do buffer é o que ninguém lembra, e meio "ê" vira tofu.
static void trunca(char *destino, size_t tamanho, const char *origem, int bytes)
{
    if (bytes > (int)tamanho - 1) bytes = (int)tamanho - 1;
    if (bytes < 0) bytes = 0;

    // 10xxxxxx é continuação: o corte está no meio de um caractere.
    while (bytes > 0 && ((unsigned char)origem[bytes] & 0xC0) == 0x80) bytes--;

    memcpy(destino, origem, (size_t)bytes);
    destino[bytes] = '\0';
}

faixa_t faixa_abre(bitmap_t *bm, int altura, bool negativo)
{
    // O grid decide o retângulo, mesmo com pedido absurdo: nenhuma altura
    // errada alcança a barra.
    ret_t r = grid_faixa_de(altura);

    faixa_t f = {
        .r        = r,
        .x        = (int16_t)GRID_MARGEM,
        .y        = (int16_t)(r.y + FAIXA_TOPO + RESPIRO),
        .util     = (int16_t)GRID_UTIL,
        .negativo = negativo,
    };

    // 1. o chão: o texto de trás não atravessa.
    gfx_limpa_ret(bm, r.x, r.y, r.l, r.a);

    // 2. o filete de topo: daqui para baixo é outra superfície.
    gfx_hlin(bm, r.x, r.y, r.l, FAIXA_TOPO);

    return f;
}

int faixa_conteudo_a(const faixa_t *f)
{
    int a = (f->r.y + f->r.a - BOTOES_A) - f->y;
    return a > 0 ? a : 0;
}

void faixa_botoes(bitmap_t *bm, const faixa_t *f, const char *esq,
                  const char *dir)
{
    int y = f->r.y + f->r.a - BOTOES_A;

    // A direita primeiro: ela dita o que sobra, e a saída à esquerda não pode
    // ser empurrada para fora.
    int larg_dir = 0;
    if (dir && *dir) {
        char corte[ROTULO_MAX];
        trunca(corte, sizeof corte, dir, gfx_cabe(F_MIUDA, dir, f->util / 2));
        larg_dir = gfx_largura(F_MIUDA, corte);
        gfx_texto(bm, f->x + f->util - larg_dir, y, F_MIUDA, corte);
    }

    if (esq && *esq) {
        // O que sobra, menos um respiro: encostados, os rótulos leem como uma
        // frase.
        int sobra = f->util - larg_dir - RESPIRO;
        char corte[ROTULO_MAX];
        trunca(corte, sizeof corte, esq, gfx_cabe(F_MIUDA, esq, sobra));
        gfx_texto(bm, f->x, y, F_MIUDA, corte);
    }
}

void faixa_fecha(bitmap_t *bm, const faixa_t *f)
{
    if (f->negativo) gfx_negativo(bm, f->r.x, f->r.y, f->r.l, f->r.a);
}
