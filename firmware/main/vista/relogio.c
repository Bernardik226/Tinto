#include "relogio.h"
#include "campos.h"
#include "../nucleo/data.h"
#include <stdio.h>
#include <string.h>

void vista_relogio(const estado_t *e, vista_relogio_t *out)
{
    memset(out, 0, sizeof *out);
    snprintf(out->titulo, sizeof out->titulo, "%s", "Data e hora");
    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    out->bateria = e->bateria;
    out->wifi = vista_wifi_da_barra(e);
    out->sinc = vista_sinc_da_barra(e);

    out->pela_rede = e->config.valor[AJUSTE_HORA_REDE] != 0;

    // ── o fuso é do GOOGLE quando há conta ──────────────────────────────
    // O NTP dá o INSTANTE (UTC); o fuso diz que horas ele é aqui, e desce no
    // pull com horário de verão. Editável só sem conta: depois, o próximo pull
    // o sobrescreveria.
    out->fuso_do_google = e->nome[0] != '\0';

    // Quantas paradas o cursor tem (a mesma conta do app):
    //
    //   7  à mão            interruptor · fuso · cinco campos
    //   2  NTP, sem conta   interruptor · fuso
    //   1  NTP, com conta   interruptor — nada mais aqui é nosso
    int linhas = 3;
    if (out->pela_rede) linhas = out->fuso_do_google ? 1 : 2;
    int c = e->cursor;
    if (c < 0) c = 0;
    if (c >= linhas) c = linhas - 1;

    out->no_interruptor = c == 0;
    out->no_fuso        = c == 1;
    out->editavel = !out->pela_rede && c == 2;
    out->campo    = out->editavel ? e->inicio.campo : -1;

    int fm = e->config.valor[AJUSTE_FUSO_MIN];
    snprintf(out->fuso, sizeof out->fuso, "%c%02d:%02d",
             fm < 0 ? '-' : '+', (fm < 0 ? -fm : fm) / 60,
             (fm < 0 ? -fm : fm) % 60);
    snprintf(out->interruptor, sizeof out->interruptor, "%s",
             out->pela_rede ? "ligada" : "desligada");

    // Manual: os campos que a pessoa edita, carregados da hora atual ao abrir
    // (quase sempre se corrige minutos). Pela rede: o relógio.
    bool manual = !out->pela_rede;
    int dia    = manual ? e->inicio.dia    : e->hoje.dia;
    int mes    = manual ? e->inicio.mes    : e->hoje.mes;
    int ano    = manual ? e->inicio.ano    : e->hoje.ano;
    int hora   = manual ? e->inicio.hora   : e->hora;
    int minuto = manual ? e->inicio.minuto : e->minuto;

    snprintf(out->valor[0], sizeof out->valor[0], "%02d", dia);
    snprintf(out->valor[1], sizeof out->valor[1], "%02d", mes);
    snprintf(out->valor[2], sizeof out->valor[2], "%04d", ano);
    vista_hora_do_campo(e, hora, out->valor[3], sizeof out->valor[3]);
    snprintf(out->valor[4], sizeof out->valor[4], "%02d", minuto);

    if (out->pela_rede && out->fuso_do_google && out->no_interruptor) {
        // Nada aqui é da pessoa: o instante vem do NTP e o fuso do calendário.
        // Dizer isso separa "travou" de "está sendo cuidado".
        snprintf(out->nota, sizeof out->nota, "%s",
                 "A hora vem da rede e o fuso vem do seu calendário. Nada "
                 "aqui se ajusta à mão enquanto isto valer.");
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s",
                 "OK desligar");
    } else if (out->pela_rede && out->no_interruptor) {
        snprintf(out->nota, sizeof out->nota, "%s",
                 "A hora vem da rede quando houver rede. Até lá vale a "
                 "última que entrou.");
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s",
                 "OK desligar");
    } else if (out->no_fuso) {
        snprintf(out->nota, sizeof out->nota, "%s",
                 "O fuso do seu calendário. Ao conectar a conta, ele vem "
                 "do Google e este ajuste some de cena.");
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s",
                 "◀▶ muda o fuso");
    } else if (out->no_interruptor) {
        snprintf(out->nota, sizeof out->nota, "%s",
                 "Ligada, o aparelho pergunta a hora à rede e para de "
                 "aceitar ajuste à mão.");
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s",
                 "OK ligar");
    } else {
        snprintf(out->nota, sizeof out->nota, "%s",
                 "◀ ▶ escolhe o campo, ▲ ▼ muda o valor.");
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK salvar");
    }
    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK voltar");
}
