// vista/teclado.h — T-28, o teclado virtual.
// Contextos TIPADOS (título, senha de Wi-Fi, dono): o "ok" não adivinha o que
// está editando. Conteúdo nunca se digita: transcrição ruim se regrava.
// Alfabético 6×6, não QWERTY (aqui se digita com um direcional), com
// wrap-around e repetição ao segurar.
#ifndef VISTA_TECLADO_H
#define VISTA_TECLADO_H

#include "../nucleo/estado.h"
#include "../tela/icones.h"

typedef enum {
    TECLADO_RENOMEAR = 0,
    TECLADO_WIFI,          // a senha da rede escolhida
    TECLADO_WIFI_SSID,     // o nome, e só pra rede oculta
    TECLADO_PROPRIETARIO,

    // Renomear o DONO depois do primeiro uso: grava e volta. O PROPRIETARIO do
    // onboarding segue para confirmar.
    TECLADO_DONO,
} teclado_contexto_t;

#define TEC_COLS 6
#define TEC_LINS 6

typedef struct {
    char titulo[24];       // "Senha · casa" · "Renomear"
    char hora[9];
    int  bateria;
    // -1 esconde o ícone do rádio: num aparelho que funciona offline, ícone
    // permanente seria lembrete do que não faz falta.
    int  wifi;
    icone_id sinc;   // o que a sincronização faz, como forma

    // De qual rede é a senha: a barra não tem espaço para isso.
    char kicker[40];

    // Quantos caracteres já foram: a senha vai mascarada.
    char contagem[20];

    char texto[64];        // o que já foi digitado (mascarado, na senha)
    int  restam;           // quantos caracteres ainda cabem
    char teclas[TEC_LINS][TEC_COLS][6];   // o mapa, já no modo certo
    int  cur_lin, cur_col;

    char nota[48];
    char rodape_esq[22], rodape_dir[22];
} vista_teclado_t;

void vista_teclado(const estado_t *e, vista_teclado_t *out);

#endif
