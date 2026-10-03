// tela/icones.h — o vocabulário de ícones.
// Em tela/ porque ícone é bitmap, como glifo, e porque vista/ precisa dizer
// QUAL ícone uma linha leva. Gerado em parte por firmware/ferramentas/icones.py
// a partir do Phosphor (MIT), em 1 bit no tamanho de uso. Os de LINHA
// (13/15 px) e os ICO_AREA_ (44 px, cartões da Home) não se misturam.
#ifndef TELA_ICONES_H
#define TELA_ICONES_H

#include "bitmap.h"

typedef enum {
    ICO_NENHUM = 0,   // a linha não leva ícone
    ICO_MIC,
    ICO_PESSOA,
    ICO_CAIXA,
    ICO_CAIXA_ON,
    ICO_NOTA,
    ICO_LISTA,
    ICO_LEITOR_TAMANHO,
    ICO_LEITOR_FONTE,
    ICO_LEITOR_ALINHA,
    ICO_LIVRO_PEQUENO,
    ICO_CONTINUAR_LENDO,
    ICO_BIBLIOTECA,
    ICO_TEMPO_LEITURA,
    ICO_DOCUMENTO_PEQUENO,
    ICO_EVENTO,
    ICO_RELOGIO,
    ICO_RELOGIO_24,
    ICO_LIXO,
    ICO_AJUSTES,
    ICO_ENTRA,
    ICO_RENOMEAR,
    ICO_WIFI,
    ICO_SEM_REDE,
    ICO_WIFI_1,
    ICO_WIFI_2,
    ICO_WIFI_3,
    ICO_CADEADO,
    ICO_SAIR,
    ICO_CAT_CONEXAO2,
    ICO_SUBINDO,
    ICO_DESCENDO,
    ICO_SINCRONIZA,
    ICO_SYNC_ERRO,
    ICO_VISTO,          // o visto — feito, concluído, confirmado
    ICO_X,              // o xis — descartar, o par do visto
    ICO_TINTO,          // a marca, no cabeçalho da Home
    ICO_CAT_CONTA,      // os cinco destinos de Ajustes, 26 px
    ICO_CAT_CONEXAO,
    ICO_CAT_TELA,
    ICO_CAT_CAMERA,
    ICO_CAT_CARTAO,
    ICO_MARCA_TAREFA,   // quadrado cheio — o dia tem tarefa
    ICO_MARCA_EVENTO,   // bolinha cheia  — o dia tem evento
    ICO_AREA_AGENDA,    // cartões da Home, 44 px — calendário
    ICO_AREA_ACERVO,    // livro aberto
    ICO_AREA_JOGOS,     // cavalo de xadrez, silhueta cheia
    ICO_XADREZ,         // o mesmo cavalo, rasterizado para a linha
    ICO_ACERVO_GRANDE,  // o livro do Acervo vazio, 76 px
    ICO_AREA_AJUSTES,   // quadrado com ponto central
    ICO_QUANTOS
} icone_id;

typedef struct {
    int8_t l, a;
    const uint8_t *bits;
} icone_t;

extern const icone_t ICONES[ICO_QUANTOS];

// Pinta com o canto superior esquerdo em (x, y). Devolve o x de depois.
int gfx_icone(bitmap_t *bm, int x, int y, icone_id id);

#endif
