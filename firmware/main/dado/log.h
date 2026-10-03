// dado/log.h — o log de campo, no cartão: sem serial ligado, é a única
// história que sobra.
//
// RN-95: id, tamanho, duração e custo. NUNCA conteúdo: log que registra o que
// a pessoa falou é vazamento esperando alguém pedir o arquivo.
#ifndef DADO_LOG_H
#define DADO_LOG_H

#include "../nucleo/tipos.h"
#include "../hal/hal.h"

typedef enum {
    LOG_LIGOU = 0,
    LOG_GRAVOU,        // valor: duração em segundos
    LOG_DESCARTOU,
    LOG_MARCOU,
    LOG_ERRO,          // valor: o erro_t
    LOG_FILA_CHEIA,    // valor: quantas se perderam
    LOG_DORMIU,
} log_evento_t;

// Rotativo: cartão cheio de log não recebe mais fala.
erro_t dado_log(const hal_t *hal, log_evento_t ev, int valor,
                int hora, int minuto);

#endif
