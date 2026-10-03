// vista/conta.h — Minha Conta e Sincronização (T-32).
// O aparelho funciona offline, e nenhuma das duas finge saber o que só o
// backend sabe: código, papel e agendas CHEGAM.
#ifndef VISTA_CONTA_H
#define VISTA_CONTA_H

#include "cartao.h"

// As linhas que se apertam, na ordem. Enum e não índice: a lista muda de
// tamanho com o estado.
typedef enum {
    CONTA_NADA = 0,
    CONTA_WIFI,          // conectar o rádio — a única saída quando não há rede
    CONTA_VINCULAR,      // abre o QR; sem conta, também o código
    CONTA_VOZ,           // abre T-26, quanto ainda dá pra falar

    // O gesto de falar: PTT ou um toque. Alterna na própria linha.
    CONTA_DONO,          // abre o teclado, renomeia o aparelho
    CONTA_SINCRONIZACAO, // T-32 — quais agendas da conta entram
    CONTA_DESCONECTAR,
} conta_acao_t;

// O que a linha sob o cursor faz. Fora da lista devolve CONTA_NADA.
conta_acao_t vista_conta_acao(const estado_t *e);

// Sincronização: o card responde "como está"; a lista tem só escolhas
// reais.
void vista_sincronizacao(const estado_t *e, vista_cartao_t *out);

#define AGENDAS_POR_PAGINA 4

// Quantas paradas o cursor tem: uma por agenda.
int  vista_sincronizacao_paradas(const estado_t *e);

// ── Minha conta ──────────────────────────────────────────────────────
// Cada estado é uma situação, não um layout:
//
//   SEM REDE    o dono continua visível; a saída é a Conexão
//   SEM GOOGLE  com rede e sem conta: oferece conectar
//   CONECTADA   dono, conta, voz, agendas e última sincronia
typedef enum {
    CONTA_SEM_REDE = 0,
    CONTA_SEM_GOOGLE,
    CONTA_REAUTORIZAR,
    CONTA_CONECTADA,
} conta_estado_t;

void vista_conta(const estado_t *e, vista_cartao_t *out);

// Quantas paradas o cursor tem nesta tela.
int  vista_conta_paradas(const estado_t *e);

#endif
