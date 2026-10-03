// vista/lancador.h — a Home 2x2, dados crus → cartões prontos. PURO.
// A struct é plana e já formatada: a tela pergunta "este campo existe?".
#ifndef VISTA_LANCADOR_H
#define VISTA_LANCADOR_H

#include "../nucleo/estado.h"
#include "../tela/icones.h"

// Quatro: Agenda, Acervo, Jogos e Ajustes. Um quinto não cabe na grade.
#define LANCADOR_CARTOES 4

// O que sobra do cabeçalho depois da margem e do logo: é contra isto que
// o nome do dono é medido.
#define LANCADOR_CABECA_UTIL 203

typedef struct {
    char titulo[12];    // "Agenda"
    char legenda[18];   // "Hoje" — o que se encontra lá dentro

    // Quem diz qual ícone uma linha leva é a vista (`tela/icones.h`).
    icone_id icone;
} cartao_lancador_t;

typedef struct {
    // ── a barra ──
    // Na Home a esquerda é a DATA: "home" não informa nada.
    char data_curta[20];
    char hora[9];
    int  bateria;
    int  wifi;
    icone_id sinc;   // o que a sincronização faz, como forma

    // ── o cabeçalho, acima da grade ──
    // "TINTO DE USUÁRIO". 40 bytes: 24 caracteres com acento passam de 24 bytes.
    char sobre[40];

    // O nome do dono sem a moldura, para quando "TINTO DE <nome>" não cabe:
    // corta-se a moldura, não o nome.
    char dono[40];
    char saudacao[24];   // "Boa tarde."

    cartao_lancador_t cartoes[LANCADOR_CARTOES];
    int  foco;           // 0..3 — qual cartão está preenchido de preto

    char rodape_esq[24], rodape_dir[24];
} vista_lancador_t;

void vista_lancador(const estado_t *e, vista_lancador_t *out);

#endif
