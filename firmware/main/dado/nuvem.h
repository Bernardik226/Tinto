// dado/nuvem.h — a credencial deste aparelho (/TINTO/sistema/nuvem.json).
// Lê e escreve, não decide. RN-64: .tmp, rename, releitura — credencial pela
// metade faz o aparelho falhar para sempre, calado.
#ifndef DADO_NUVEM_H
#define DADO_NUVEM_H

#include "../hal/hal.h"
#include "../nucleo/tipos.h"

// ── o endereço de fábrica ────────────────────────────────────────────
// Vem de `main/servidor.h`, fora do repositório: o servidor é de quem monta o
// aparelho (`servidor.exemplo.h` é o modelo). Sem ele o padrão fica vazio e a
// tela diz isso. O endereço de verdade mora no cartão; trocar de servidor não
// exige regravar.
#if defined(__has_include)
#  if __has_include("servidor.h")
#    include "servidor.h"
#  endif
#endif

#ifndef TINTO_SERVIDOR
#  define TINTO_SERVIDOR ""
#endif

#define NUVEM_SERVIDOR_PADRAO TINTO_SERVIDOR

// Carrega o que houver e completa o resto. Ausente devolve ERR_ARQUIVO com a
// struct PREENCHIDA: primeiro uso não é erro (`nuvem_registrado` distingue).
// `servidor` cai no padrão, `device_id` vem do eFuse, `prova` nasce do hal;
// só o `token` fica vazio.
erro_t nuvem_carrega(const hal_t *hal, nuvem_cred_t *out);

erro_t nuvem_grava(const hal_t *hal, const nuvem_cred_t *cred);

// Este aparelho já tem token?
bool nuvem_registrado(const nuvem_cred_t *cred);

// Apaga o TOKEN e guarda o resto. Chamada no 401: o servidor não reconhece
// mais o aparelho. `device_id` e prova ficam — é com eles que ele se
// reapresenta sozinho.
erro_t nuvem_esquece_token(const hal_t *hal);

#endif
