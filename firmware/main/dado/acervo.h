// dado/acervo.h — o acervo no CARTÃO: `/TINTO/acervo/<id>/meta.json` e o
// texto ao lado. Duas regras:
//
//   1. O `.part` nunca é obra: o texto só vira o arquivo final inteiro.
//   2. O id não vira caminho sem passar por aqui: um `../..` vindo do
//      servidor apagaria a pasta errada.
#ifndef DADO_ACERVO_H
#define DADO_ACERVO_H

#include "../hal/hal.h"
#include "../nucleo/acervo.h"

// A obra do cartão, pelo id. `ERR_ARQUIVO` quando ela não está aqui.
erro_t acervo_le_meta(const hal_t *hal, const char *id, obra_t *out);

// Meta atômico (`.tmp` + rename): desligar no meio deixa o velho inteiro.
erro_t acervo_grava_meta(const hal_t *hal, const obra_t *o);

// Qual obra foi aberta por último. A data do meta ordena dias diferentes;
// este marcador desempata o mesmo dia.
erro_t acervo_marca_aberta(const hal_t *hal, const char *id);

// Da aberta mais recente para a mais antiga ("Em leitura"). Sem data de
// abertura vai para o fim.
erro_t acervo_lista(const hal_t *hal, obra_t *out, int max, int *quantos);

// Apaga a CÓPIA local; a obra continua no acervo online.
erro_t acervo_remove_local(const hal_t *hal, const char *id);

// Cabe? O tamanho é declarado antes da transferência: recusa o download
// que não terminaria.
bool acervo_tem_espaco(const hal_t *hal, int32_t bytes);

// `inteiro` decide o nome: `texto.part` enquanto vem, `texto.txt` só
// completo.
erro_t acervo_grava_texto(const hal_t *hal, const char *id,
                          const char *texto, bool inteiro);
erro_t acervo_anexa_texto(const hal_t *hal, const char *id,
                          const char *texto, bool primeiro, bool inteiro);
bool acervo_tem_parcial(const hal_t *hal, const char *id);
erro_t acervo_descarta_parcial(const hal_t *hal, const char *id);
erro_t acervo_le_texto(const hal_t *hal, const char *id,
                       char *out, size_t max);
erro_t acervo_grava_sinopse(const hal_t *hal, const char *id, const char *texto);
erro_t acervo_le_sinopse(const hal_t *hal, const char *id, char *out, size_t max);

// A capa vem pronta do backend para cada lugar (página, miniatura,
// destaque), 1 bit por pixel em hex: o ESP32 não redimensiona.
erro_t acervo_anexa_capa(const hal_t *hal, const char *id, const char *hex,
                         bool primeiro, bool inteira);
erro_t acervo_le_capa(const hal_t *hal, const char *id,
                      uint8_t *out, size_t max);
erro_t acervo_grava_capa_mini(const hal_t *hal, const char *id,
                              const char *hex);
erro_t acervo_le_capa_mini(const hal_t *hal, const char *id,
                           uint8_t *out, size_t max);
erro_t acervo_grava_capa_destaque(const hal_t *hal, const char *id,
                                  const char *hex);
erro_t acervo_le_capa_destaque(const hal_t *hal, const char *id,
                               uint8_t *out, size_t max);

#endif
