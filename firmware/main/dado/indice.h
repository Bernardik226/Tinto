// dado/indice.h — todos os itens do cartão, na RAM.
//
// A árvore por data é ótima para gravar e péssima para perguntar ("o que tem
// no dia X?", "que dias têm marca?", "que tarefas estão abertas?"). Cada
// `f_opendir` custa dezenas de ms, dentro do laço que desenha: com 96 pastas
// o passo chegou a 6,5 s e o joystick morreu. O custo crescia com o cartão.
//
//   · montado UMA vez, no boot
//   · toda escrita passa por `cartao_grava_item`/`cartao_apaga_item`, que o
//     atualizam — nenhum chamador precisa lembrar
//   · as telas leem daqui, nunca do cartão
//
// Na PSRAM (~210 KB para 512 itens). Não é persistido de propósito: seria um
// segundo arquivo para ficar velho e discordar do cartão.
#ifndef DADO_INDICE_H
#define DADO_INDICE_H

#include "../hal/hal.h"
#include "../nucleo/estado.h"

// Quantos itens o aparelho comporta. Estourar: conta, avisa e mostra o que
// coube.
#define INDICE_MAX 512

// Monta do zero, no boot. Falha de cartão NÃO é índice vazio: quem pergunta
// recebe erro, não "agenda vazia".
erro_t indice_monta(const hal_t *hal);

// Devolve a memória emprestada. Desligar sem isto é vazamento.
void indice_solta(const hal_t *hal);

// Está montado e utilizável?
bool indice_pronto(void);

// Quantos itens ele guarda.
int indice_n(void);

// ── escrita ──────────────────────────────────────────────────────────
// Só `cartao_grava_item` e `cartao_apaga_item` chamam.
void indice_poe(const item_t *it, data_t dia);
void indice_tira(const char *id);

// Os itens cuja PASTA é este dia: o mesmo que listar o diretório.
int indice_do_dia(data_t dia, item_t *out, int max);

// Todos, em ordem de array, para quem filtra por conta própria.
const item_t *indice_em(int i);

// O item, onde quer que ele esteja. Devolve NULL se não existe.
const item_t *indice_acha(const char *id, data_t *dia);

#endif
