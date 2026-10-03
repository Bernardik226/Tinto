// dado/json.h — leitor de JSON raso. PURO.
// O contrato tem um nível e chaves curtas, então não precisa de biblioteca:
// procura a chave e extrai o valor, sem árvore nem alocação. RN-B8: campo
// faltando devolve false, e quem chamou põe o padrão.
#ifndef DADO_JSON_H
#define DADO_JSON_H

#include "../nucleo/tipos.h"

// RN-B6/B7: copia com limite e TRUNCA. Rejeitar por tamanho sumiria com o
// dia da pessoa por causa de um nome comprido.
bool json_str (const char *json, const char *chave, char *out, size_t max);
bool json_int (const char *json, const char *chave, int *out);
bool json_bool(const char *json, const char *chave, bool *out);

#endif
