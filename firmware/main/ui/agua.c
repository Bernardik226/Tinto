// ui/agua.c — ver agua.h.
#include "agua.h"
#include "../tela/texto.h"
#include <string.h>

// Sem trama: em 1 bit "claro" é tirar pixel, e numa haste de 2 px sobra 1
// — no e-ink vira sujeira. A hierarquia vem de TAMANHO e espaço: a miúda,
// sozinha e centrada num espaço grande.

// Respiro menor que o de lista: as duas linhas são a MESMA frase.
#define ENTRE 2

// Uma linha da frase, até o `\n` ou o fim. Devolve o comprimento em bytes.
static int corta(const char *ini)
{
    int n = 0;
    while (ini[n] && ini[n] != '\n') n++;
    return n;
}

int ui_agua_altura(const char *frase)
{
    if (!frase || !*frase) return 0;

    int alt = 0;
    const char *p = frase;
    while (*p) {
        int n = corta(p);
        alt += gfx_altura_linha(F_MIUDA) + ENTRE;
        p += n;
        if (*p == '\n') p++;
    }
    return alt - ENTRE;
}

int ui_agua(bitmap_t *bm, int x, int y, int l, int a, const char *frase)
{
    if (!frase || !*frase || l <= 0) return y;

    int alt = ui_agua_altura(frase);

    // Centrada, mas nunca acima do topo da zona.
    int topo = y + (a - alt) / 2;
    if (topo < y) topo = y;

    int cy = topo;
    const char *p = frase;
    while (*p) {
        int n = corta(p);

        // Cópia: `gfx_texto` lê até o `\0`, e a frase tem mais linhas depois.
        char linha[64];
        if (n > (int)sizeof linha - 1) n = (int)sizeof linha - 1;
        memcpy(linha, p, (size_t)n);
        linha[n] = '\0';

        int w = gfx_largura(F_MIUDA, linha);
        gfx_texto(bm, x + (l - w) / 2, cy, F_MIUDA, linha);
        cy += gfx_altura_linha(F_MIUDA) + ENTRE;

        p += n;
        if (*p == '\n') p++;
    }


    return cy - ENTRE;
}
