#include "leitor.h"
#include <string.h>

// Quanto texto cabe numa página, por tamanho de letra. Aproximado pela
// geometria do vidro (~20 caracteres por linha na serifa de 17 px, ~14
// linhas); exata precisa ser só a quebra (palavra e UTF-8).
static int32_t cabe_na_pagina(leitor_tamanho_t letra)
{
    switch (letra) {
    // Conservadores de propósito: se a página guardar mais do que a tela
    // desenha, o "avançar" pula o trecho atrás do "…". Contam BYTES (acento = 2).
    case LEITOR_PEQUENA: return 340;   // letra menor, mais texto
    case LEITOR_GRANDE:  return 150;
    case LEITOR_MEDIA:
    default:             return 230;
    }
}

// Em UTF-8, `10xxxxxx` é continuação: cortar ali desenha um quadrado.
static bool comeca_caractere(const char *t, int32_t i)
{
    return (t[i] & 0xC0) != 0x80;
}

// Onde termina a página que começa em `de`: anda até o teto, volta ao
// começo de caractere e recua até o último espaço.
static int32_t fim_da_pagina(const leitor_t *l, int32_t de)
{
    if (l->quebra) {
        int32_t n = l->quebra(l->texto + de, l->tamanho - de,
                              l->letra, l->familia);
        if (n > 0 && n <= l->tamanho - de) return de + n;
    }
    int32_t teto = de + cabe_na_pagina(l->letra);
    if (teto >= l->tamanho) return l->tamanho;

    int32_t i = teto;
    while (i > de && !comeca_caractere(l->texto, i)) i--;

    int32_t corte = i;
    while (corte > de && l->texto[corte - 1] != ' ' &&
           l->texto[corte - 1] != '\n') corte--;

    // Palavra maior que a página: corta no caractere, senão a página fica
    // vazia para sempre.
    return corte > de ? corte : i;
}

void leitor_abre(leitor_t *l, const char *texto, int32_t tamanho,
                 int32_t onde)
{
    memset(l, 0, sizeof *l);
    l->texto = texto;
    l->tamanho = tamanho > 0 ? tamanho : 0;
    l->letra = LEITOR_MEDIA;
    l->familia = LEITOR_SERIFADA;
    l->alinhamento = LEITOR_JUSTIFICADO;

    if (onde < 0) onde = 0;
    if (onde > l->tamanho) onde = l->tamanho;

    // Posição guardada no meio de um caractere (o texto mudou): recua até o
    // começo dele.
    while (onde > 0 && onde < l->tamanho && !comeca_caractere(texto, onde))
        onde--;

    l->inicio = onde;
    l->conta_alvo = onde;
    l->conta_numero = 1;
}

int32_t leitor_pagina(leitor_t *l, char *out, size_t max)
{
    if (!l || !out || !max) return 0;
    out[0] = '\0';
    if (!l->texto || l->tamanho <= 0) return 0;

    int32_t fim = fim_da_pagina(l, l->inicio);
    size_t n = (size_t)(fim - l->inicio);
    if (n >= max) n = max - 1;

    memcpy(out, l->texto + l->inicio, n);
    out[n] = '\0';
    return fim;
}

void leitor_avanca(leitor_t *l)
{
    if (!l || leitor_no_fim(l)) return;

    int32_t fim = fim_da_pagina(l, l->inicio);
    if (fim <= l->inicio) return;

    // A trilha guarda de onde viemos, para o voltar ser exato. Cheia, perde a
    // mais antiga.
    int max = (int)(sizeof l->trilha / sizeof l->trilha[0]);
    if (l->n_trilha >= max) {
        memmove(l->trilha, l->trilha + 1,
                sizeof l->trilha[0] * (size_t)(max - 1));
        l->n_trilha--;
    }
    l->trilha[l->n_trilha++] = l->inicio;

    l->inicio = fim;
}

void leitor_volta(leitor_t *l)
{
    if (!l) return;

    if (l->n_trilha > 0) {
        l->inicio = l->trilha[--l->n_trilha];
        return;
    }

    // Sem trilha (reabriu no meio e apertou ◀): recompõe do começo. Lento, mas
    // exato.
    if (l->inicio <= 0) return;

    int32_t de = 0, anterior = 0;
    while (de < l->inicio) {
        anterior = de;
        int32_t fim = fim_da_pagina(l, de);
        if (fim <= de) break;
        de = fim;
    }
    l->inicio = anterior;
}

// A composição mudou: a trilha e a contagem recomeçam.
static void zera_conta(leitor_t *l)
{
    l->n_trilha = 0;
    l->conta_offset = 0;
    l->conta_alvo = l->inicio;
    l->conta_paginas = 0;
    l->conta_numero = 1;
}

void leitor_recompoe(leitor_t *l, leitor_tamanho_t letra)
{
    if (!l) return;

    // A POSIÇÃO não se move: ela é offset justamente para isso. A trilha guarda
    // quebras da composição velha e vai fora.
    l->letra = letra;
    zera_conta(l);
}

void leitor_familia(leitor_t *l, leitor_familia_t familia)
{
    if (!l) return;
    l->familia = familia;
    zera_conta(l);
}

void leitor_quebra_com(leitor_t *l, leitor_quebra_fn quebra)
{
    if (!l) return;
    l->quebra = quebra;
    zera_conta(l);
}

int32_t leitor_posicao(const leitor_t *l) { return l ? l->inicio : 0; }

bool leitor_no_fim(const leitor_t *l)
{
    if (!l || l->tamanho <= 0 || l->inicio >= l->tamanho) return true;
    // "Fim" é a última página VISÍVEL. Esperar `inicio == tamanho` deixaria o ▶
    // avançar para uma página vazia.
    return fim_da_pagina(l, l->inicio) >= l->tamanho;
}

bool leitor_conta_passo(leitor_t *l, int limite, int *total, int *numero)
{
    if (!l || limite <= 0) return false;
    if (l->tamanho <= 0) {
        if (total) *total = 1;
        if (numero) *numero = 1;
        return true;
    }
    while (limite-- > 0 && l->conta_offset < l->tamanho) {
        int32_t de = l->conta_offset;
        int32_t fim = fim_da_pagina(l, de);
        if (fim <= de) break;
        l->conta_paginas++;
        if (l->conta_alvo >= de && l->conta_alvo < fim)
            l->conta_numero = l->conta_paginas;
        l->conta_offset = fim;
    }
    if (l->conta_offset < l->tamanho) return false;
    if (total) *total = l->conta_paginas > 0 ? l->conta_paginas : 1;
    if (numero) *numero = l->conta_numero;
    return true;
}

int leitor_pct(const leitor_t *l)
{
    if (!l || l->tamanho <= 0) return 0;
    long p = (long)l->inicio * 100 / l->tamanho;
    return p > 100 ? 100 : (int)p;
}
