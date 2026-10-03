#include "dock.h"
#include "campos.h"
#include "../nucleo/data.h"
#include <string.h>
#include <stdio.h>

void vista_dock(const estado_t *e, vista_dock_t *out)
{
    memset(out, 0, sizeof *out);

    vista_hora_da_barra(e, out->hora, sizeof out->hora);

    // Mês CURTO: a faixa divide a linha com a carga. A data completa está no
    // bloqueio.
    snprintf(out->faixa, sizeof out->faixa, "na dock · %s, %d %s",
             data_semana_curta(e->hoje), e->hoje.dia, data_mes_curto(e->hoje));

    // O número E o que acontece: 81% parado e 81% subindo significam coisas
    // opostas.
    snprintf(out->carga, sizeof out->carga, "%d%% · %s", e->bateria,
             e->docado ? "carregando" : "na bateria");

    // A mesma saudação da Home.
    int h = e->hora;
    snprintf(out->saudacao, sizeof out->saudacao, "%s",
             h < 12 ? "Bom dia." : h < 18 ? "Boa tarde." : "Boa noite.");

    // ── os DOIS próximos ─────────────────────────────────────────────────
    // Olhada de passagem, a pergunta é "ainda dá tempo?": o segundo
    // compromisso responde.
    const int agora = e->hora * 60 + e->minuto;
    const item_t *prox = NULL, *seg = NULL;
    int t_prox = 0, t_seg = 0;

    for (int i = 0; i < e->n_itens; i++) {
        const item_t *it = &e->itens[i];
        if (it->tipo != TIPO_EVENTO || !it->hora[0]) continue;
        if (!data_igual(it->dia, e->hoje)) continue;

        int m = data_minutos(it->hora);
        if (m < agora) continue;

        if (!prox || m < t_prox)      { seg = prox; t_seg = t_prox;
                                        prox = it;  t_prox = m; }
        else if (!seg || m < t_seg)   { seg = it;   t_seg = m; }
    }

    if (prox) {
        char quando[16];
        vista_falta(t_prox - agora, quando, sizeof quando);
        snprintf(out->kicker, sizeof out->kicker, "próximo · %s", quando);
        char h[HORA_TEXTO];
        vista_hora_do_item(e, prox->hora, h, sizeof h);
        snprintf(out->proximo, sizeof out->proximo, "%s · %s",
                 h, prox->titulo);
        snprintf(out->onde, sizeof out->onde, "%s", prox->local);

        if (seg) {
            char hs[HORA_TEXTO];
            vista_hora_do_item(e, seg->hora, hs, sizeof hs);
            snprintf(out->depois, sizeof out->depois, "Depois: %s · %s",
                     seg->titulo, hs);
        }
    } else {
        // A mesma frase do bloqueio: é a mesma notícia.
        snprintf(out->proximo, sizeof out->proximo, "%s", "Dia livre.");
    }

    snprintf(out->travado, sizeof out->travado, "%s",
             "painel travado · retire para usar");
}
