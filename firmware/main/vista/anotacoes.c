#include "anotacoes.h"
#include "campos.h"
#include "../nucleo/data.h"
#include <stdio.h>
#include <string.h>

// "HOJE", "ONTEM", ou a data: ninguém procura o que falou "em 24 de
// agosto", procura o que falou ontem.
static void rotulo_do_dia(data_t d, data_t hoje, char *out, size_t n)
{
    if (data_compara(d, hoje) == 0) { snprintf(out, n, "%s", "HOJE"); return; }
    if (data_compara(d, data_soma_dias(hoje, -1)) == 0) {
        snprintf(out, n, "%s", "ONTEM");
        return;
    }
    snprintf(out, n, "%s %d %s", data_semana_curta(d), d.dia, data_mes_curto(d));
}

void vista_anotacoes(const estado_t *e, vista_anotacoes_t *out)
{
    memset(out, 0, sizeof *out);
    snprintf(out->titulo, sizeof out->titulo, "%s", "Anotações");
    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    out->bateria = e->bateria;
    out->wifi = vista_wifi_da_barra(e);
    out->sinc = vista_sinc_da_barra(e);
    out->cursor = e->travado ? -1 : e->cursor;
    out->alvo   = -1;

    data_t dia_anterior = {0, 0, 0};

    for (int i = 0; i < e->n_anotacoes && i < ANOTACOES_MAX; i++) {
        const item_t *it = &e->anotacoes[i];
        linha_captura_t *l = &out->linhas[out->n];

        // O rótulo só na primeira do dia.
        if (data_compara(it->dia, dia_anterior) != 0) {
            rotulo_do_dia(it->dia, e->hoje, out->dia[out->n],
                          sizeof out->dia[0]);
            dia_anterior = it->dia;
        }

        vista_hora_do_item(e, it->hora, l->hora, sizeof l->hora);
        snprintf(l->titulo, sizeof l->titulo, "%s", it->titulo);

        // O subtítulo é a duração falada. Sem áudio fica vazio, sem inventar
        // "0:00".
        if (it->dur_s > 0)
            snprintf(l->sub, sizeof l->sub, "%d:%02d",
                     it->dur_s / 60, it->dur_s % 60);

        // Sem título a linha sai fraca e diz "sem título".
        l->indice = (int16_t)i;

        if (out->n == out->cursor) out->alvo = (int16_t)i;
        out->n++;
    }

    if (!out->n)
        snprintf(out->vazio, sizeof out->vazio, "%s",
                 "Nada anotado ainda. Fale algo que não seja "
                 "compromisso nem tarefa, e o que você disser fica aqui.");

    out->paginas = (e->anotacoes_total + ANOTACOES_MAX - 1) / ANOTACOES_MAX;
    if (out->paginas < 1) out->paginas = 1;
    out->pagina = e->anotacoes_pagina + 1;

    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK acervo");
    if (out->n && out->cursor >= 0)
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK abrir");
}

int vista_anotacoes_linhas(const estado_t *e)
{
    return e->n_anotacoes > 0 ? e->n_anotacoes : 1;
}
