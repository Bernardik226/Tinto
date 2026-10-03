// ui/chrome.h — as peças do sistema: barra, rodapé, cursor, selos.
// Conhecem a linguagem visual, não o conteúdo. Nenhuma tela escreve na barra
// nem no rodapé por conta própria.
#ifndef UI_CHROME_H
#define UI_CHROME_H

#include "../tela/texto.h"
#include "../tela/icones.h"

// Barra e rodapé são medidos pela MIÚDA.
#define BARRA_A   23   // a barra de título

// O ícone do sinal por força (0-100), o mesmo na barra e na lista.
icone_id chrome_icone_wifi(int forca);
#define RODAPE_A  20

typedef struct {
    const char *titulo;   // à esquerda: o que é esta tela
    const char *hora;     // "09:14" — nunca some (UI.md §HUD)
    int         bateria;  // 0-100

    // -1 esconde o ícone (o normal, offline); 0-100 desenha os arcos. Os
    // voláteis ficam à ESQUERDA da hora, para hora e bateria não dançarem.
    int         wifi;
    bool        carregando;   // a BATERIA está carregando (o raio)

    // O que a sincronização faz, como forma, à esquerda do Wi-Fi. Não gira:
    // em e-ink coisa que gira vira borrão.
    icone_id    sinc;
} barra_t;

typedef enum { FILETE_FINO = 0, FILETE_GROSSO } filete_t;

// Toda função devolve o y de baixo: nenhuma tela conhece a altura de nada.
int  chrome_barra (bitmap_t *bm, const barra_t *b);
int  chrome_filete(bitmap_t *bm, int y, filete_t f);
// O contador de páginas do PRÓXIMO rodapé, só para um quadro. `total <= 1`
// não desenha.
void chrome_rodape_pagina(int atual, int total);

// Para onde ainda há página: ▼ enquanto houver, ▲ na última.
// `ICO_NENHUM` sem paginação. A seta inverte em vez de sumir: o rodapé não
// pode mudar de largura.
icone_id chrome_seta_da_pagina(int atual, int total);

// A ESPERA do próximo rodapé: 0 a 3 pontinhos, andando com o relógio. Vale
// um quadro só. Onde há carregamento, há pontinhos; nunca reticências.
void chrome_rodape_espera(int pontos);

// `n` quadrados de 5 px, de 8 em 8 (em 1 bit, círculo de 5 px é quadrado
// comido). Devolve a largura.
int  chrome_pontinhos(bitmap_t *bm, int x, int y, int n);

// A espera de quem ENVIA: três posições fixas, uma acesa. Anda o estado,
// não o tamanho.
void chrome_pontinhos_um_aceso(bitmap_t *bm, int x, int y, int aceso);

// Pixels acesos no rodapé, para o teste medir que a espera anda.
int  chrome_pixels_do_rodape(const bitmap_t *bm);

void chrome_rodape(bitmap_t *bm, const char *esq, const char *dir, bool negativo);

// A moldura do cursor extravasa a linha: encostar no texto pareceria erro
// de leiaute.
void chrome_cursor(bitmap_t *bm, int y, int altura);
void chrome_cursor_em(bitmap_t *bm, int x, int y, int larg, int altura);

// Selo em negativo ("seg", "3 d"). Devolve o x final.
int  chrome_selo(bitmap_t *bm, int x, int y, const char *texto, bool negativo);

int  chrome_caixa(bitmap_t *bm, int x, int y, bool marcada);
void chrome_risco(bitmap_t *bm, int x, int y, int largura, fonte_t f);

// Filete grosso à esquerda = veio de fora (Google).
void chrome_origem(bitmap_t *bm, int y, int altura);

#endif
