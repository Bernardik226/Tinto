// nucleo/data.h — aritmética de datas. PURO.
// As pastas do cartão são datas: data ↔ texto precisa ser exato nos dois sentidos.
#ifndef NUCLEO_DATA_H
#define NUCLEO_DATA_H

#include <stdbool.h>
#include <stddef.h>
#include "tipos.h"

#define DATA_TEXTO 11   // "2026-08-14" + terminador

// "2026-08-14" — o nome do diretório, e o formato do contrato com o backend
void   data_para_texto(data_t d, char *out, size_t max);
erro_t data_de_texto(const char *s, data_t *out);

data_t data_soma_dias(data_t d, int dias);
int    data_dias_entre(data_t a, data_t b);   // b - a

// 0 = domingo … 6 = sábado. É o que põe o dia 1 na coluna certa.
int data_dia_da_semana(data_t d);

// "qua", "12 ago" — o vocabulário curto que as telas usam
const char *data_semana_curta(data_t d);
const char *data_mes_curto(data_t d);

// "Agosto" — o mês por extenso, que só o calendário usa
const char *data_mes_longo(data_t d);

// "terça-feira": para o cabeçalho da Agenda, onde o dia é o assunto. A curta
// ("ter") é para rótulo de canto.
const char *data_semana_longa(data_t d);

// Quantos dias o mês tem, com bissexto. É o que fecha a grade.
int data_dias_no_mes(int ano, int mes);

// ── a hora, no formato que a pessoa escolheu ─────────────────────────
// A regra mora aqui, não em cada vista. 24 h: "09:14" (o zero alinha a
// coluna). 12 h: "9:14 am". O destino quer 9 bytes ("12:00 am" + terminador).
#define HORA_TEXTO 9
void hora_texto(int h, int m, bool h24, char *out, size_t max);

// O mesmo para a hora pronta do servidor ("14:00"). O cartão guarda sempre
// `hh:mm`; quem converte é a tela. O que não for `hh:mm` passa inteiro.
void hora_texto_hhmm(const char *hhmm, bool h24, char *out, size_t max);

// "14:30" → 870. -1 para vazio ou malformado.
int data_minutos(const char *hhmm);

#endif
