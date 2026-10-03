// dado/cartao.h — o cartão. O ÚNICO que conhece caminho de arquivo e JSON.
// Lê e escreve; não decide regra de negócio.
//
//   RN-61  fora de /TINTO/ o firmware não enxerga nada
//   RN-62  sem meta.json, o diretório NÃO EXISTE
#ifndef DADO_CARTAO_H
#define DADO_CARTAO_H

#include "../nucleo/tipos.h"
#include "../nucleo/data.h"
#include "../nucleo/estado.h"
#include "../hal/hal.h"

#define CARTAO_RAIZ    "/TINTO"
#define CARTAO_FORMATO 1        // o "v" do meta. RN-63

erro_t cartao_prepara(const hal_t *hal);   // RN-68: cria a árvore e segue

erro_t cartao_le_item   (const hal_t *hal, data_t dia, const char *id, item_t *out);
erro_t cartao_grava_item(const hal_t *hal, data_t dia, const item_t *it);
erro_t cartao_apaga_item(const hal_t *hal, data_t dia, const char *id);

// Os ajustes, em /TINTO/sistema/config.json (escrita atômica, RN-64).
erro_t cartao_le_config   (const hal_t *hal, config_t *out);
erro_t cartao_grava_config(const hal_t *hal, const config_t *c);

// A transcrição e o resumo, que moram fora do meta.
erro_t cartao_le_texto(const hal_t *hal, data_t dia, const char *id,
                       texto_t *out);

// Quantos dias com conteúdo a varredura devolve de uma vez; só dias que
// existem ocupam lugar.
#define CARTAO_DIAS_MAX 96

erro_t cartao_lista_dias (const hal_t *hal, data_t *out, int max, int *quantos);

// O id como volta do nome da pasta. O FAT não aceita `:`, e todo id do
// servidor tem um: desfazer a troca antes de usar o nome como id.
void cartao_id_do_nome(const char *nome, char *out, size_t max);

// Onde mora o áudio da fala. Aqui, que é quem conhece o desenho das pastas
// (e o `:` que não cabe).
void cartao_caminho_wav(data_t dia, const char *id, char *out, size_t max);

// Há algo do Google no cartão, em qualquer dia? Decide se pede a colheita
// inteira; é sobre o cartão, nunca sobre o cache do dia.
bool cartao_tem_do_google(const hal_t *hal);

// Apaga o item onde estiver: o `removidos` do pull manda só o id.
erro_t cartao_apaga_em_qualquer_dia(const hal_t *hal, const char *id);

// O item pelo id, em qualquer dia, e o dia da pasta: a voz manda só o id,
// e a pasta não sai do vencimento (RN-26).
erro_t cartao_acha_item(const hal_t *hal, const char *id, item_t *out,
                        data_t *dia);

// Esquece o conteúdo de uma conta (itens, áudios, transcrições). Desconectar
// NÃO chama isto: só conectar uma conta DIFERENTE.
erro_t cartao_esquece_a_conta(const hal_t *hal);

// Itens por varredura: cabe na pilha (~3,5 KB) e o dia comum sai numa
// leitura.
#define CARTAO_BLOCO 8

// `desde` é o cursor, como no `listar` do hal. Varre em blocos: um
// `item_t[32]` na pilha seria demais.
erro_t cartao_lista_itens(const hal_t *hal, data_t dia, int desde,
                          item_t *out, int max, int *quantos);

// Todo ajuste do enum tem chave no config.json? Inicializador designado
// completa com NULL, e NULL num `%s` derruba o ESP32. O teste reprova o build.
bool cartao_chaves_completas(void);

// ── a transcrição mora na NOTA, não no item ──────────────────────────
// Uma fala vira até três ações e tem uma transcrição só, em
// `/TINTO/notas/<id>.txt`. Cada item guarda o id da nota de onde veio.
erro_t cartao_grava_transcricao(const hal_t *hal, const char *nota,
                                const char *texto);
erro_t cartao_le_transcricao(const hal_t *hal, const char *nota,
                             char *out, size_t max);

// ── a fila de gestos, que sobrevive a ficar sem rede e a desligar ────
// O gesto acontece NA HORA e sobe DEPOIS. Em `/TINTO/gestos/<seq>`, um
// arquivo por gesto; o nome (seis dígitos com zero à esquerda) é a ordem, e
// a ordem alfabética do `listar` é a ordem em que a pessoa fez as coisas.
// Conteúdo: `operacao\ncorpo` — o id da operação (retry seguro) e o JSON.
#define GESTOS_MAX 32

erro_t cartao_grava_gesto(const hal_t *hal, unsigned seq, const char *corpo,
                          const char *operacao, data_t dia);
erro_t cartao_lista_gestos(const hal_t *hal, char nomes[][40], int max,
                           int *quantos);
erro_t cartao_le_gesto(const hal_t *hal, const char *nome,
                       char *corpo, size_t max_corpo,
                       char *operacao, size_t max_op, data_t *dia);
erro_t cartao_apaga_gesto(const hal_t *hal, const char *nome);

// A maior sequência já usada: recomeçar do zero poria o gesto novo ATRÁS
// dos que esperam.
unsigned cartao_ultimo_gesto(const hal_t *hal);

#endif
