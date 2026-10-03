// nucleo/leitor.h — paginar sem perder o lugar.
//
// A posição canônica é o OFFSET no texto; página e porcentagem derivam dele.
// Trocar a letra repagina tudo, e a página 46 de uma fonte é outro trecho na
// seguinte. PURO: recebe o texto e devolve onde a página começa e termina.
#ifndef NUCLEO_LEITOR_H
#define NUCLEO_LEITOR_H

#include "tipos.h"

// O que cabe numa página do vidro, com folga para acento (2 bytes em UTF-8).
#define LEITOR_PAGINA 1200

// Quanto de obra cabe na RAM de uma vez: 512 KB, emprestados da PSRAM e
// devolvidos (`hal/memoria_hal.h`). Um static de 96 KB já estourou a DRAM.
// Quando não couber, `emprestar` devolve menos (`minimo`) e o leitor abre o
// que veio.
#define LEITOR_TEXTO_MAX  (512 * 1024)
#define LEITOR_TEXTO_MIN  (32 * 1024)

// Os três tamanhos da tela. O nome é o que a pessoa vê; o número sai da
// largura da fonte.
typedef enum {
    LEITOR_PEQUENA = 0,
    LEITOR_MEDIA,
    LEITOR_GRANDE,
} leitor_tamanho_t;

typedef enum {
    LEITOR_SERIFADA = 0,
    LEITOR_SEM_SERIFA,
    LEITOR_MONO,
    LEITOR_LITERATA,
    LEITOR_ATKINSON,
    LEITOR_INTER,
    LEITOR_SOURCE_SERIF,
    LEITOR_SERIFADA_FORTE,
    LEITOR_SEM_SERIFA_FORTE,
    LEITOR_MONO_FORTE,
    LEITOR_LITERATA_FORTE,
    LEITOR_ATKINSON_FORTE,
    LEITOR_INTER_FORTE,
    LEITOR_SOURCE_SERIF_FORTE,
} leitor_familia_t;

typedef enum {
    LEITOR_JUSTIFICADO = 0,
    LEITOR_ESQUERDA,
    LEITOR_CENTRO,
    LEITOR_DIREITA,
} leitor_alinhamento_t;

typedef int32_t (*leitor_quebra_fn)(const char *texto, int32_t tamanho,
                                    leitor_tamanho_t letra,
                                    leitor_familia_t familia);

typedef struct {
    const char *texto;      // não é dono: o texto mora no cache de quem abriu
    int32_t tamanho;

    int32_t inicio;         // onde a página atual começa — a VERDADE
    leitor_tamanho_t letra;
    leitor_familia_t familia;
    leitor_alinhamento_t alinhamento;
    leitor_quebra_fn quebra;

    // Onde cada página visitada começou, para o voltar ser exato: paginar de
    // trás para frente não dá o mesmo corte.
    int32_t trilha[64];
    int8_t  n_trilha;

    // Contagem cooperativa: o livro abre antes de contar todas as páginas.
    int32_t conta_offset;
    int32_t conta_alvo;
    int     conta_paginas;
    int     conta_numero;
} leitor_t;

// Abre o texto na posição guardada. `onde` é o offset, e não a página.
void leitor_abre(leitor_t *l, const char *texto, int32_t tamanho,
                 int32_t onde);

// Copia a página atual para `out` e devolve onde ela TERMINA.
int32_t leitor_pagina(leitor_t *l, char *out, size_t max);

void leitor_avanca(leitor_t *l);
void leitor_volta(leitor_t *l);

// Repagina no tamanho novo sem mexer na posição.
void leitor_recompoe(leitor_t *l, leitor_tamanho_t letra);
void leitor_familia(leitor_t *l, leitor_familia_t familia);
void leitor_quebra_com(leitor_t *l, leitor_quebra_fn quebra);

int32_t leitor_posicao(const leitor_t *l);
bool    leitor_no_fim(const leitor_t *l);
int     leitor_pct(const leitor_t *l);

// Quantas páginas a obra tem e em qual estamos (derivados do offset). A
// conta é uma varredura feita aos poucos: `limite` páginas por chamada.
// Devolve true quando acabou; recomeça ao abrir e ao trocar fonte ou família.
bool leitor_conta_passo(leitor_t *l, int limite, int *total, int *numero);

#endif
