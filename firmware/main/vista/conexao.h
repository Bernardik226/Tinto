// vista/conexao.h — Conexão (o Wi-Fi). A rede atual é INFORMAÇÃO e não
// recebe foco; procurar, escolher e esquecer são destinos abaixo do card.
#ifndef VISTA_CONEXAO_H
#define VISTA_CONEXAO_H

#include "cartao.h"

// Em que situação a rede está: cada estado responde a uma pergunta.
typedef enum {
    CONEXAO_SEM_REDE = 0,  // nada conectado, e talvez nada salvo
    CONEXAO_CONECTANDO,    // tentando; o BACK sai sem cancelar
    CONEXAO_CONECTADA,
    CONEXAO_SENHA_RECUSADA,  // o AP recusou a credencial
    CONEXAO_SEM_RESPOSTA,    // a rede não respondeu, ou sumiu
} conexao_estado_t;

void vista_conexao(const estado_t *e, vista_cartao_t *out);

// Quantas paradas o cursor tem: uma por destino.
int  vista_conexao_paradas(const estado_t *e);

#endif
