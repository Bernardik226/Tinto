#include "obra.h"
#include "campos.h"
#include <stdio.h>
#include <string.h>
void vista_obra(const estado_t *e, vista_obra_t *out)
{
    memset(out, 0, sizeof *out);
    const obra_t *o = &e->obra_aberta;
    out->cursor = e->cursor;
    snprintf(out->barra, sizeof out->barra, "Obra");
    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    out->bateria=e->bateria; out->wifi=vista_wifi_da_barra(e); out->sinc=vista_sinc_da_barra(e);
    snprintf(out->titulo, sizeof out->titulo, "%s", o->titulo);
    snprintf(out->autor, sizeof out->autor, "%s", o->autor[0] ? o->autor : "Autor não informado");
    snprintf(out->sinopse, sizeof out->sinopse, "%s", e->obra_sinopse[0] ? e->obra_sinopse : "Sem resumo disponível.");
    snprintf(out->tipo, sizeof out->tipo, "%s", o->tipo == OBRA_DOCUMENTO ? "Documento" : "Livro");
    snprintf(out->tempo, sizeof out->tempo, "Cerca de %d min", o->minutos_leitura > 0 ? o->minutos_leitura : 1);
    if (o->criada_em.ano)
        snprintf(out->adicionada, sizeof out->adicionada, "%02d/%02d/%04d",
                 o->criada_em.dia, o->criada_em.mes, o->criada_em.ano);
    else snprintf(out->adicionada, sizeof out->adicionada, "Data não informada");
    int pct=o->tamanho>0?(int)((long)o->offset_texto*100/o->tamanho):0;
    snprintf(out->progresso, sizeof out->progresso, "%d%% lido", pct>100?100:pct);
}
