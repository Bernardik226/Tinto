// dado/perfil.h — quem é o dono desta memória.
// Lê e escreve, não decide. /TINTO/sistema/, RN-64: .tmp, rename, releitura.
#ifndef DADO_PERFIL_H
#define DADO_PERFIL_H

#include "../nucleo/tipos.h"
#include "../nucleo/inicializacao.h"
#include "../hal/hal.h"

// Ausente devolve ERR_ARQUIVO com a struct zerada: primeiro uso não é erro.
erro_t perfil_carrega(const hal_t *hal, perfil_local_t *out);
erro_t perfil_grava  (const hal_t *hal, const perfil_local_t *perfil);

// 16 bytes do hal viram 32 hex. Só sem dono: regenerar seria trocar de
// pessoa.
void perfil_novo_id(const hal_t *hal, char out[33]);

// Troca o nome e grava. `local` = trocado aqui, marcado para subir; senão
// veio do servidor e a marca sai. Cria o perfil e o id do dono se faltarem.
erro_t perfil_renomeia(const hal_t *hal, const char *nome, bool local);

// RN-B7: trunca em NOME_CARACTERES_MAX caracteres, nunca no meio de uma
// sequência UTF-8. Devolve quantos caracteres sobraram.
int perfil_nome_trunca(const char *entrada, char *out, size_t max);

#endif
