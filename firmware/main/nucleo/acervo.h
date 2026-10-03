// nucleo/acervo.h — o que é uma OBRA para o aparelho.
//
// O catálogo é da conta e mora no servidor; aqui mora o que este cartão tem.
// Três estados: só online (ícone de baixar), aqui (abre sem rede) e baixando.
// Dois progressos que não se misturam: quanto desceu e onde a pessoa parou.
#ifndef NUCLEO_ACERVO_H
#define NUCLEO_ACERVO_H

#include "tipos.h"

// Um id do servidor tem 12 caracteres com o prefixo `ob:`; 40 dá folga.
#define OBRA_ID       40
#define OBRA_TITULO  128
#define OBRA_AUTOR    64
#define OBRA_SINOPSE 241

// Teto de uma leitura da estante; quem pagina é a Biblioteca.
#define OBRAS_MAX     32

typedef enum {
    OBRA_SO_ONLINE = 0,   // existe no acervo; não está neste aparelho
    OBRA_BAIXANDO,        // a cópia está vindo
    OBRA_AQUI,            // a cópia está no cartão, inteira
} obra_estado_t;

typedef enum {
    OBRA_LIVRO = 0,
    OBRA_DOCUMENTO,
} obra_tipo_t;

typedef struct {
    char id[OBRA_ID];
    char titulo[OBRA_TITULO];
    char autor[OBRA_AUTOR];

    obra_tipo_t   tipo;
    obra_estado_t estado;

    // Tamanho em bytes do texto convertido, como o servidor declarou. A cópia
    // que não bate não vira obra.
    int32_t tamanho;
    int32_t palavras;
    int16_t minutos_leitura;
    data_t  criada_em;

    // `baixado` é quanto está no cartão; `offset_texto`, onde a pessoa parou.
    // 100% baixada e 0% lida é o normal.
    int32_t baixado;
    int32_t offset_texto;

    bool concluida;      // a pessoa marcou que terminou
    bool tem_capa;       // o catálogo online oferece uma capa preparada
    bool capa_aqui;      // o bitmap de 42×75 já está no cartão
    // Persistido: a origem ainda existe no Acervo da conta. Durante uma rodada,
    // marca também que a obra apareceu no catálogo atual.
    bool no_catalogo;

    // Última abertura: escolhe o "Continuar lendo" e ordena "Em leitura".
    data_t aberta_em;

    // O tamanho de fonte da última paginação, por obra. Repaginar preserva a
    // posição pelo offset, nunca pelo número da página.
    int8_t fonte;
    int8_t alinhamento;    // leitor: justificado, esquerda, centro, direita
    int8_t rodape;         // 0 porcentagem+páginas, 1 barra+%, 2 só páginas
} obra_t;

#endif
