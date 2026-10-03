#include "fala.h"
#include "campos.h"
#include "../nucleo/data.h"
#include <stdio.h>
#include <string.h>

void vista_fala(const estado_t *e, vista_fala_t *out)
{
    memset(out, 0, sizeof *out);
    // "Uso de voz": o nome do destino em Minha conta.
    snprintf(out->titulo, sizeof out->titulo, "%s", "Uso de voz");
    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    out->bateria = e->bateria;
    out->wifi    = vista_wifi_da_barra(e);
    out->sinc    = vista_sinc_da_barra(e);
    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK conta");
    snprintf(out->mes, sizeof out->mes, "%s", data_mes_longo(e->hoje));
    snprintf(out->explica, sizeof out->explica, "%s",
             "Só conta o que é enviado. Gravar, pausar ou descartar não "
             "gasta.");

    int limite_s = (int)e->quota.limite_s;

    // Sem limite conhecido não há número: "0 de 0" pareceria esgotado.
    if (limite_s <= 0) {
        snprintf(out->usados, sizeof out->usados, "%s", "–");
        snprintf(out->de, sizeof out->de, "%s", "sem conta pareada");
        return;
    }

    // O usado em grande: segundos até um minuto (se já começou), depois
    // minutos inteiros. O que resta arredonda para baixo: nunca promete o que
    // não há.
    int usados_s = (int)e->quota.usados_s;
    if (usados_s > limite_s) usados_s = limite_s;
    int sobra_s = limite_s - usados_s;
    if (usados_s > 0 && usados_s < 60) {
        snprintf(out->usados, sizeof out->usados, "%d", usados_s);
        snprintf(out->unidade, sizeof out->unidade, "%s", "s");
    } else {
        snprintf(out->usados, sizeof out->usados, "%d", usados_s / 60);
        snprintf(out->unidade, sizeof out->unidade, "%s", "min");
    }
    snprintf(out->de, sizeof out->de, "usados de %d min", limite_s / 60);
    snprintf(out->restam, sizeof out->restam, "restam %d min", sobra_s / 60);
    out->pct = sobra_s * 100 / limite_s;

    if (e->quota.dias_pra_virar > 0)
        snprintf(out->renova, sizeof out->renova, "renova em %d %s",
                 e->quota.dias_pra_virar,
                 e->quota.dias_pra_virar == 1 ? "dia" : "dias");
}
