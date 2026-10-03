// dado/memoria.h — a árvore /TINTO/ vista como estado.
// Só diagnóstico e o conserto que não destrói nada; o que fazer com um
// cartão danificado é decisão de uso/.
//
//   RN-61  fora de /TINTO/ o firmware não enxerga nada
//   RN-6B  FAT válido sem /TINTO/ é provisionado sem formatar
//   RN-63  formato que este firmware não conhece NÃO é sobrescrito
#ifndef DADO_MEMORIA_H
#define DADO_MEMORIA_H

#include "../nucleo/tipos.h"
#include "../hal/hal.h"

// O que a mídia tem dentro, depois de montada (montar é do hal).
typedef enum {
    ARVORE_PRONTA = 0,      // /TINTO/ completa, no formato desta versão
    ARVORE_VIRGEM,          // FAT saudável, sem /TINTO/ — provisiona e segue
    ARVORE_PARCIAL,         // formato certo, pasta faltando — repara sem apagar
    ARVORE_FORMATO_FUTURO,  // gravada por versão mais nova — não toca
    ARVORE_DANIFICADA,      // /TINTO/ existe e não faz sentido — recuperação
} arvore_estado_t;

// Só olha. Não cria, não corrige, não escreve.
erro_t memoria_inspeciona(const hal_t *hal, arvore_estado_t *out);

// Cria o que falta de /TINTO/ e grava o formato. Recusa árvore danificada
// ou de versão futura (ERR_FORMATO).
erro_t memoria_prepara(const hal_t *hal);

// Escreve, sincroniza, relê e apaga: separa "montou" de "dá para trabalhar"
// (cartão cheio ou somente-leitura monta perfeito).
erro_t memoria_prova_escrita(const hal_t *hal);

#endif
