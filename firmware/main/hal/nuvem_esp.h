// hal/nuvem_esp.h — as chamadas ao backend, numa task própria.
//
// RN-41: nenhuma tela espera rede. A chamada vai para uma fila, a task REDE
// faz o HTTP, e o resultado volta como EV_REDE_RESULTADO. Arquivo próprio
// porque o lwip declara um ERR_TIMEOUT que colide com o `erro_t`.
#ifndef HAL_NUVEM_ESP_H
#define HAL_NUVEM_ESP_H

#include <stdbool.h>
#include <stddef.h>

void hal_nuvem_liga(void);

// ── o áudio, por uma porta estreita ──────────────────────────────────
// Este arquivo não pode incluir `hal_sd.h` (colisão do lwip): o WAV atravessa
// com `int` e `uint32_t`. Implementadas em `hal_sd.c`.
int      hal_nuvem_audio_abre(const char *caminho);   // 0 = abriu
unsigned hal_nuvem_audio_tamanho(void);
int      hal_nuvem_audio_le(void *dados, size_t n);   // 0 = fim, <0 = erro
void     hal_nuvem_audio_fecha(void);

// Enfileira. `corpo` nulo é GET; com corpo, POST. `operacao` torna o retry
// seguro; nulo quando a chamada não muda nada.
void hal_nuvem_pede(const char *rota, const char *corpo,
                    const char *operacao);

// O WAV vira multipart, lido em pedaços de 2 KB: a fala inteira não passa
// pela RAM.
void hal_nuvem_pede_audio(const char *rota, const char *caminho_wav,
                          const char *operacao);

// O endereço do backend e o token, uma vez, depois de ler o cartão.
void hal_nuvem_credencial(const char *servidor, const char *token);

// A última resposta, e se veio inteira. Chamada pela task APP depois do
// evento: o buffer é dela.
bool hal_nuvem_resposta(char *out, size_t max);

// O código HTTP da última resposta, ou 0.
int  hal_nuvem_codigo(void);

// De qual linha veio a resposta lida: 0 = agora (gestos), 1 = o pull. As
// duas podem estar em voo juntas.
int  hal_nuvem_linha(void);

// Implementada no hal_esp.c: empurra EV_REDE_RESULTADO na fila do app.
void hal_esp_empurra_resposta(void);

#endif
