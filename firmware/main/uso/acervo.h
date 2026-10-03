// uso/acervo.h — o catálogo que desce e a cópia que chega.
// O CATÁLOGO (metadados) desce inteiro e vira a estante; o TEXTO (megabytes)
// só desce quando alguém abre a obra.
#ifndef USO_ACERVO_H
#define USO_ACERVO_H

#include "../hal/hal.h"
#include "../nucleo/estado.h"

// Pede o catálogo da conta (aplicado em `uso_nuvem_resposta`).
// Traz o que o cartão tem para a RAM: reler a pasta por quadro custa meio
// segundo.
erro_t uso_carregar_acervo(const hal_t *hal, estado_t *e);

erro_t uso_acervo_sincroniza(const hal_t *hal, estado_t *e);

// Pede o TEXTO de uma obra. Cartão cheio recusa antes: um `.part` ocuparia
// o espaço que já falta.
erro_t uso_acervo_baixa(const hal_t *hal, estado_t *e, const char *id);
erro_t uso_acervo_descarta_local(const hal_t *hal, estado_t *e,
                                 const obra_t *obra);

// Aplica o lote do catálogo. Público para o teste provar o parse.
erro_t uso_acervo_aplica(const hal_t *hal, estado_t *e, const char *json);

// Aplica o texto que chegou. Menos do que o declarado NÃO vira obra.
erro_t uso_acervo_recebe(const hal_t *hal, estado_t *e, const char *texto);
erro_t uso_acervo_capa_baixa(const hal_t *hal, estado_t *e, const char *id);
erro_t uso_acervo_capas_pequenas_baixa(const hal_t *hal, estado_t *e,
                                       const char *id);
erro_t uso_acervo_capa_recebe(const hal_t *hal, estado_t *e, const char *hex);

#endif
