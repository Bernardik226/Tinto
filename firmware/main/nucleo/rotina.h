// nucleo/rotina.h — a rotina é uma REGRA, não uma lista.
//
// O Google expande a série em uma ocorrência por dia; no cartão isso eram
// noventa itens e noventa repinturas. Aqui a pergunta é sempre "este dia é
// dela?", aritmética sem ler o cartão. PURO.
#ifndef NUCLEO_ROTINA_H
#define NUCLEO_ROTINA_H

#include "data.h"
#include "tipos.h"

// O formato que desce no contrato (cabe em 48 bytes):
//
//     d:1                 todo dia
//     s:1:12345           toda semana, seg a sex (0=dom … 6=sáb)
//     m:1                 todo mês, no mesmo dia
//     a:1                 todo ano
//     |u=20261130         até essa data, inclusive
//     |x=0915,0922        menos esses dias (MMDD)
//
// O `x` são as exceções do Google: ocorrência apagada ou movida sai da regra.
#define ROTINA_MAX 48

// Este dia é da rotina? `ancora` é a primeira ocorrência que ainda vale.
// Regra vazia: só o dia da âncora.
bool rotina_no_dia(const char *regra, data_t ancora, data_t dia);

#endif
