#include "bloqueio.h"
#include "campos.h"
#include "../nucleo/data.h"
#include <stdio.h>
#include <string.h>

void vista_bloqueio(const estado_t *e, vista_bloqueio_t *out)
{
    memset(out, 0, sizeof *out);

    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    // Por extenso: olhada de longe, e aqui sobra espaço.
    snprintf(out->data, sizeof out->data, "%s, %d de %s",
             data_semana_longa(e->hoje), e->hoje.dia, data_mes_longo(e->hoje));

    snprintf(out->saida, sizeof out->saida, "%s", "power para desbloquear");
    out->bateria    = e->bateria;
    out->wifi = vista_wifi_da_barra(e);
    out->sinc = vista_sinc_da_barra(e);
    out->carregando = e->docado;
    if (e->aviso_desbloqueio)
        snprintf(out->aviso, sizeof out->aviso, "%s", "desbloqueie para gravar");

    // No primeiro uso não há dia para mostrar (a hora pode ser um contador,
    // RN-6G): fica a bateria e a saída.
    if (e->inicio.fase != INICIO_HOME) {
        out->hora[0] = '\0';
        out->data[0] = '\0';
        out->wifi    = -1;
        out->sinc    = ICO_NENHUM;
        return;
    }

    // ── o que tem HORA: só hoje, e só o que ainda vem ────────────────────
    // Evento e o híbrido de hoje (a academia às 7h).
    int agora = e->hora * 60 + e->minuto;
    const item_t *hora[ITENS_MAX];
    int nh = 0;
    for (int i = 0; i < e->n_itens && nh < ITENS_MAX; i++) {
        const item_t *it = &e->itens[i];
        if (!data_igual(it->dia, e->hoje) || !it->hora[0]) continue;
        bool evento = it->tipo == TIPO_EVENTO;
        bool tarefa = it->tipo == TIPO_TAREFA && !it->feita;
        if (!evento && !tarefa) continue;
        if (data_minutos(it->hora) < agora) continue;
        hora[nh++] = it;
    }
    for (int a = 1; a < nh; a++)
        for (int b = a; b > 0 && data_minutos(hora[b]->hora) <
                                 data_minutos(hora[b - 1]->hora); b--) {
            const item_t *t = hora[b]; hora[b] = hora[b - 1]; hora[b - 1] = t;
        }

    // ── as TAREFAS: abertas até serem feitas ─────────────────────────────
    // A mesma fonte e regra da Agenda (pendentes + `vista_tarefa_no_dia`). A
    // atrasada primeiro.
    const item_t *tarefas[PENDENTES_MAX];
    int nt = 0;
    for (int i = 0; i < e->n_pendentes && nt < PENDENTES_MAX; i++) {
        const item_t *it = &e->pendentes[i];
        if (it->tipo != TIPO_TAREFA || it->feita) continue;
        if (!vista_tarefa_no_dia(e, it, e->hoje, true)) continue;
        tarefas[nt++] = it;
    }
    for (int a = 1; a < nt; a++)
        for (int b = a; b > 0; b--) {
            const item_t *x = tarefas[b], *y = tarefas[b - 1];
            bool antes = x->vence.ano &&
                         (!y->vence.ano || data_compara(x->vence, y->vence) < 0);
            if (!antes) break;
            tarefas[b] = y; tarefas[b - 1] = x;
        }

    // O PRÓXIMO é o primeiro com hora.
    int ih = 0;
    if (nh > 0) {
        const item_t *prox = hora[ih++];
        char h[HORA_TEXTO];
        vista_hora_do_item(e, prox->hora, h, sizeof h);
        snprintf(out->proximo, sizeof out->proximo, "%s · %s", h, prox->titulo);
        snprintf(out->onde, sizeof out->onde, "%s", prox->local);
        vista_falta(data_minutos(prox->hora) - agora, out->falta,
                    sizeof out->falta);
    } else if (nt == 0) {
        // "Dia livre." é informação, não ausência dela.
        snprintf(out->proximo, sizeof out->proximo, "%s", "Dia livre.");
    }

    // Três linhas: o resto de hoje com hora, depois as tarefas.
    int it_ = 0;
    for (; ih < nh && out->n_resto < 3; ih++) {
        char h[HORA_TEXTO];
        vista_hora_do_item(e, hora[ih]->hora, h, sizeof h);
        snprintf(out->resto[out->n_resto++], sizeof out->resto[0], "%s · %s",
                 h, hora[ih]->titulo);
    }
    for (; it_ < nt && out->n_resto < 3; it_++)
        snprintf(out->resto[out->n_resto++], sizeof out->resto[0], "· %s",
                 tarefas[it_]->titulo);

    // O que não coube, dito pelo nome do que é.
    int sobra_h = nh - ih, sobra_t = nt - it_ + e->n_pendentes_fora;
    if (sobra_h > 0)
        snprintf(out->mais, sizeof out->mais, "+ %d mais", sobra_h + sobra_t);
    else if (sobra_t > 0)
        snprintf(out->mais, sizeof out->mais, "+ %d por fazer", sobra_t);
}
