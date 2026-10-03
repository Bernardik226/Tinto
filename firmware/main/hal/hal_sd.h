// hal/hal_sd.h — memória interna SDMMC + FatFs, confinada ao HAL.
#ifndef HAL_SD_H
#define HAL_SD_H

#include "../nucleo/inicializacao.h"
#include "../nucleo/tipos.h"

void hal_sd_liga(void);
memoria_estado_t hal_sd_memoria_estado(void);
erro_t hal_sd_ler(const char *caminho, char *out, size_t max);
erro_t hal_sd_escrever(const char *caminho, const char *conteudo);
erro_t hal_sd_anexar(const char *caminho, const char *conteudo);
erro_t hal_sd_apagar(const char *caminho);
erro_t hal_sd_renomear(const char *de, const char *para);
erro_t hal_sd_criar_diretorio(const char *caminho);
erro_t hal_sd_tipo_caminho(const char *caminho, caminho_tipo_t *out);
erro_t hal_sd_listar(const char *dir, int desde, char nomes[][40], int max,
                     int *quantos);
erro_t hal_sd_formatar(void);
void hal_sd_aleatorio(uint8_t *out, size_t n);
erro_t hal_sd_espaco(uint32_t *usado_kb, uint32_t *total_kb);
erro_t hal_sd_uso_de(const char *dir, uint32_t *kb);

// ── escrita em fluxo ─────────────────────────────────────────────────
// O `escrever` é atômico e de uma vez; áudio chega a 32 KB/s e nunca está
// inteiro na RAM. Um fluxo por vez: abrir o segundo é erro, não fila.
erro_t hal_sd_fluxo_abre(const char *caminho);
erro_t hal_sd_fluxo_escreve(const void *dados, size_t n);
erro_t hal_sd_fluxo_escreve_em(uint32_t pos, const void *dados, size_t n);
uint32_t hal_sd_fluxo_bytes(void);
erro_t hal_sd_fluxo_fecha(void);

// ── leitura em fluxo ─────────────────────────────────────────────────
// Para ler aos poucos o que não cabe numa string (um WAV de minutos).
erro_t hal_sd_leitura_abre(const char *caminho);

// O tamanho do arquivo ABERTO, para o `Content-Length` do multipart.
uint32_t hal_sd_leitura_tamanho(void);
int    hal_sd_leitura_le(void *dados, size_t n);   // < 0 é erro, 0 é fim
erro_t hal_sd_leitura_fecha(void);

#endif
