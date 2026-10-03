// vista/cartao.h — o card que informa, e abaixo os destinos que agem.
// O padrão visual do sistema (Minha conta, Conexão, Sincronização...):
//
//     o card INFORMA e o cursor nunca para nele; os destinos AGEM.
#ifndef VISTA_CARTAO_H
#define VISTA_CARTAO_H

#include "../nucleo/estado.h"
#include "../tela/icones.h"

#define CARTAO_FATOS_MAX    3
// Doze: a Sincronização lista agendas aqui, e doze é comum.
#define CARTAO_DESTINOS_MAX 12

typedef struct {
    char titulo[20];
    char hora[9];
    int  bateria;
    int  wifi;
    icone_id sinc;

    // Em que situação a tela está; cada tela dá o significado. O tipo garante
    // que ele existe.
    int estado;

    // ── o card do topo: só informa ──────────────────────────────────────
    // O kicker diz a situação ("conta conectada"); o nome grande é o assunto.
    char kicker[28];
    char nome[40];

    // O ícone do assunto. `ICO_NENHUM` quando não há (Minha conta: o assunto é
    // uma pessoa).
    icone_id icone;

    // O LOGOTIPO no lugar do kicker e do nome, onde o assunto é o aparelho.
    bool logo;
    struct { char rotulo[18]; char valor[40]; } fatos[CARTAO_FATOS_MAX];
    int  n_fatos;

    // Os pontinhos que andam ao lado do kicker, 0 a 3, ou -1. Regra do
    // sistema: toda espera os tem — tela que espera e tela travada têm a mesma
    // cara, e apertar de novo já duplicou evento. Nunca "…" no texto.
    int  pontos;

    // A barra de progresso, 0 a 100, ou -1. Só onde há FIM conhecido
    // (download); espera sem fim tem pontinhos.
    int  barra_pct;

    // A explicação, quando o estado precisa: nunca um código nem uma rota.
    char corpo[140];

    // ── as linhas que INFORMAM ──────────────────────────────────────────
    // Entre o card e os destinos, e o cursor pula: Armazenamento diz o que come
    // o cartão sem oferecer botão.
    struct {
        icone_id ico;
        char rotulo[24];
        char sub[28];
        char valor[16];
    } info[CARTAO_DESTINOS_MAX];
    int  n_info;
    char secao_info[24];

    // ── o que se pode fazer ──────────────────────────────────────────────
    // Título, o que se faz lá, e o valor atual à direita (evita entrar só para
    // conferir). 32 bytes: "AGENDAS VISÍVEIS" saía cortado.
    char secao[32];
    struct {
        icone_id ico;
        char titulo[24];
        char sub[28];
        char valor[16];
    } dest[CARTAO_DESTINOS_MAX];
    int  n_dest;

    // ── a ação do canto ──────────────────────────────────────────────────
    // Um ícone no card, não uma linha no fim (que fazia a tela rolar).
    // Alcançado pelo ▲ a partir do primeiro destino; cursor -1 é ela.
    bool tem_sair;

    // -1 na ação do canto, 0..n_dest-1 nos destinos, -2 em nenhum (o primeiro
    // quadro de toda entrada, EINK §5.5).
    int  cursor;

    // Página do contador no rodapé ("2/3"); `paginas <= 1` não pagina.
    int  pagina, paginas;
    char rodape_esq[22], rodape_dir[22];
} vista_cartao_t;

// O começo de todo card: zera, título e barra do sistema, sem espera nem
// progresso, com o cursor da tela.
void cartao_comeca(const estado_t *e, vista_cartao_t *o, const char *titulo);

void cartao_fato(vista_cartao_t *o, const char *rotulo, const char *valor);
void cartao_destino(vista_cartao_t *o, icone_id ico, const char *titulo,
                    const char *sub, const char *valor);

#endif
