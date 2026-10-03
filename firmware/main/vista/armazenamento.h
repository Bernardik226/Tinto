// vista/armazenamento.h — o que o cartão tem dentro.
// Informa a capacidade; a única linha que se aperta é Restaurar, que abre a
// mesma pergunta destrutiva do cartão danificado (nasce no "não").
#ifndef VISTA_ARMAZENAMENTO_H
#define VISTA_ARMAZENAMENTO_H

#include "menu.h"
#include "cartao.h"


void vista_armazenamento(const estado_t *e, vista_cartao_t *out);

#endif
