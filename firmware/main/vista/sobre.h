// vista/sobre.h — Sobre: produto, documentos e suporte.
// RN-A1: estado em palavras, nunca código de erro.
#ifndef VISTA_SOBRE_H
#define VISTA_SOBRE_H

#include "../nucleo/estado.h"
#include "cartao.h"
#include "../tela/icones.h"

void vista_sobre(const estado_t *e, vista_cartao_t *out);

// A versão do firmware, num lugar só (Sobre e Atualizar).
const char *vista_sobre_versao(void);

#endif
