// uso/inicializacao.h — o caso de uso do primeiro uso e da mídia.
// Memória, perfil e recibos decidem se o aparelho PODE abrir, por isso não são
// ajustes. Diagnostica a mídia, provisiona a árvore, persiste perfil e avança
// o primeiro uso; deixa um snapshot coerente em `estado->inicio`.
#ifndef USO_INICIALIZACAO_H
#define USO_INICIALIZACAO_H

#include "../nucleo/estado.h"
#include "../hal/hal.h"

// `texto` só é lido em INICIO_CMD_SALVAR_NOME; nos demais é NULL.
erro_t uso_configurar_dispositivo(const hal_t *hal, estado_t *estado,
                                  inicio_comando_t comando, const char *texto);

// Aplica os cinco campos ao relógio. Onboarding e Ajustes → Data e hora
// são o mesmo gesto, escrito uma vez.
erro_t uso_ajustar_relogio(const hal_t *hal, estado_t *e);

// Carrega os campos com a hora que o aparelho mostra. Relógio sem data
// plausível (contador de 1970) começa em 01/01/2026 12:00.
void uso_campos_do_relogio(estado_t *e);

#endif
