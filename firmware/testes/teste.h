// firmware/testes/teste.h — o arnês. Sem framework: cada teste é uma função
// chamada à mão no main.
#ifndef TESTE_H
#define TESTE_H

#include <stdio.h>
#include <string.h>
#include "hal_pc.h"
#include "app/app.h"
#include "vista/menu.h"

extern int         teste_falhas;
extern int         teste_total;
extern const char *teste_nome;
extern bool        teste_quebrou;

#define COMECA(nome) \
    do { teste_nome = (nome); teste_total++; teste_quebrou = false; } while (0)

#define TERMINA() \
    do { if (!teste_quebrou) printf("  \033[32m✓\033[0m %s\n", teste_nome); } while (0)

#define ESPERA(cond)                                                       \
    do { if (!(cond)) {                                                    \
        printf("  \033[31m✗\033[0m %s\n      %s:%d — %s\n",                \
               teste_nome, __FILE__, __LINE__, #cond);                     \
        teste_falhas++; teste_quebrou = true; return;                      \
    } } while (0)

#define ESPERA_IGUAL(a, b)                                                 \
    do { long _a = (long)(a), _b = (long)(b);                              \
         if (_a != _b) {                                                   \
        printf("  \033[31m✗\033[0m %s\n      %s:%d — %s == %s  (%ld ≠ %ld)\n", \
               teste_nome, __FILE__, __LINE__, #a, #b, _a, _b);            \
        teste_falhas++; teste_quebrou = true; return;                      \
    } } while (0)

#define ESPERA_TEXTO(a, b)                                                 \
    do { if (strcmp((a), (b)) != 0) {                                      \
        printf("  \033[31m✗\033[0m %s\n      %s:%d — \"%s\" ≠ \"%s\"\n",   \
               teste_nome, __FILE__, __LINE__, (a), (b));                  \
        teste_falhas++; teste_quebrou = true; return;                      \
    } } while (0)

#define ESPERA_CONTEM(palheiro, agulha)                                    \
    do { if (strstr((palheiro), (agulha)) == NULL) {                       \
        printf("  \033[31m✗\033[0m %s\n      %s:%d — não achei \"%s\" em\n      %s\n", \
               teste_nome, __FILE__, __LINE__, (agulha), (palheiro));      \
        teste_falhas++; teste_quebrou = true; return;                      \
    } } while (0)

// ── andar até uma linha PELO NOME ───────────────────────────────────
// Contar ▼ amarrava os testes ao número de linhas do menu. O teto de passos
// transforma um nome errado em falha legível. `ap` é sempre ponteiro.
#define DESCE_ATE(ap, montar, nome)                                        \
    do {                                                                   \
        bool _achou = false;                                               \
        for (int _i = 0; _i < 24 && !_achou; _i++) {                       \
            vista_menu_t _v;                                               \
            montar(&(ap)->estado, 12, &_v);                                \
            if (_v.cursor >= 0 && _v.cursor < _v.n &&                      \
                strcmp(_v.linhas[_v.cursor].texto, (nome)) == 0) {         \
                _achou = true; break;                                      \
            }                                                              \
            pc_botao(IN_BAIXO);                                            \
            app_passo((ap));                                               \
        }                                                                  \
        if (!_achou) {                                                     \
            printf("  \033[31m✗\033[0m %s\n      %s:%d — não achei a linha \"%s\"\n", \
                   teste_nome, __FILE__, __LINE__, (nome));                \
            teste_falhas++; teste_quebrou = true; return;                  \
        }                                                                  \
    } while (0)

// ── entrar na Agenda ────────────────────────────────────────────────
// O aparelho liga na Home; quem testa a Agenda entra nela primeiro (um OK no
// cartão em foco). Conta como evento de entrada.
#define ENTRA_NA_AGENDA(ap) \
    do { pc_botao(IN_OK); app_passo((ap)); } while (0)

// ── entrar nos Ajustes ──────────────────────────────────────────────
// Ajustes é um cartão da Home: ▶ e ▼ chegam nele, e o OK entra.
#define ENTRA_NOS_AJUSTES(ap)                                              \
    do { pc_botao(IN_DIR); pc_botao(IN_BAIXO); pc_botao(IN_OK);            \
         app_passo((ap)); } while (0)

#define ESPERA_SEM(palheiro, agulha)                                       \
    do { if (strstr((palheiro), (agulha)) != NULL) {                       \
        printf("  \033[31m✗\033[0m %s\n      %s:%d — \"%s\" não devia estar em\n      %s\n", \
               teste_nome, __FILE__, __LINE__, (agulha), (palheiro));      \
        teste_falhas++; teste_quebrou = true; return;                      \
    } } while (0)

#endif
