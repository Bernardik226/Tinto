#include "cartao.h"
#include "campos.h"
#include <stdio.h>
#include <string.h>

void cartao_comeca(const estado_t *e, vista_cartao_t *o, const char *titulo)
{
    memset(o, 0, sizeof *o);
    snprintf(o->titulo, sizeof o->titulo, "%s", titulo);
    vista_hora_da_barra(e, o->hora, sizeof o->hora);
    o->bateria   = e->bateria;
    o->wifi      = vista_wifi_da_barra(e);
    o->sinc      = vista_sinc_da_barra(e);
    o->pontos    = -1;
    o->barra_pct = -1;
    o->cursor    = e->travado ? -2 : e->cursor;
}

// Fato sem valor não entra: uma linha "Conta" vazia diz menos que nenhuma.
void cartao_fato(vista_cartao_t *o, const char *rotulo, const char *valor)
{
    if (o->n_fatos >= CARTAO_FATOS_MAX || !valor || !*valor) return;
    snprintf(o->fatos[o->n_fatos].rotulo, sizeof o->fatos[0].rotulo, "%s", rotulo);
    snprintf(o->fatos[o->n_fatos].valor,  sizeof o->fatos[0].valor,  "%s", valor);
    o->n_fatos++;
}

void cartao_destino(vista_cartao_t *o, icone_id ico, const char *titulo,
                    const char *sub, const char *valor)
{
    if (o->n_dest >= CARTAO_DESTINOS_MAX) return;
    o->dest[o->n_dest].ico = ico;
    snprintf(o->dest[o->n_dest].titulo, sizeof o->dest[0].titulo, "%s", titulo);
    snprintf(o->dest[o->n_dest].sub,    sizeof o->dest[0].sub,    "%s", sub ? sub : "");
    snprintf(o->dest[o->n_dest].valor,  sizeof o->dest[0].valor,  "%s", valor ? valor : "");
    o->n_dest++;
}
