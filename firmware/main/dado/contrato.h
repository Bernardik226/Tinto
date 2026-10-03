// dado/contrato.h — o JSON da fronteira com o backend. PURO.
//
// O contrato é do Tinto, não do Google: chaves curtas, um nível, nenhum campo
// que mude de tipo — desenhado para o parser.
//
//   { "id":"g:a1b2c3", "t":"Dentista", "h":"14:00", "f":"15:00",
//     "l":"Rua Bahia, 210", "o":"g" }
//
// Não fala com rede nem conhece o hal: traduz texto ↔ item, com teste.
#ifndef DADO_CONTRATO_H
#define DADO_CONTRATO_H

#include "../nucleo/tipos.h"

// Percorre os objetos de um array JSON raso. Devolve false quando acabou.
// `cursor` começa apontando para o texto do array e anda sozinho.
bool contrato_proximo(const char **cursor, char *objeto, size_t max);

// O mesmo, sem passar do fim do array. O pull tem DOIS arrays de item, e
// com o primeiro vazio um cursor solto entrava no segundo.
bool contrato_proximo_ate(const char **cursor, const char *fim,
                          char *objeto, size_t max);

// Onde termina o array que começa em `abre` (aponta para o `[`).
const char *contrato_fim_do_array(const char *abre);

// Objeto → item. RN-B8: campo faltando vira padrão; só falta de `id`
// devolve falso.
bool contrato_item(const char *objeto, item_t *out);

// ── a resposta da captura ───────────────────────────────────────────
// Uma fala vira N ações, cada uma com o VERBO (criou, editou...):
//
//   { "falou":"marca dentista quinta às três",
//     "acoes":[ {"v":"criou","id":"n:1","t":"Dentista","h":"15:00",
//                "d":"2026-08-27","tp":4} ] }
//
// `falou` é a transcrição crua: sem ela, conferir vira confiar. Devolve
// quantas ações leu; `falou` sai preenchido mesmo com zero. `nota` volta com
// o id da fala, que cada ação confirmada guarda.
int contrato_captura(const char *json, resultado_t *out, int max,
                     char *falou, size_t falou_max,
                     char *nota, size_t nota_max);

// Item → corpo do POST /v1/push com a ação. Cada envio conserva o instante
// original, para resolver conflito com mudanças no Google.
int contrato_gesto(const item_t *it, const char *verbo, const char *momento,
                   char *out, size_t max);


#endif
