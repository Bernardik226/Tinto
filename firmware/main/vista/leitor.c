#include "leitor.h"
#include "campos.h"
#include <stdio.h>
#include <string.h>

fonte_t vista_fonte_leitor(leitor_tamanho_t tam, leitor_familia_t fam)
{
    static const fonte_t fontes[14][3] = {
        {F_LEITOR_SERIF_P, F_LEITOR_SERIF_M, F_LEITOR_SERIF_G},
        {F_LEITOR_SANS_P, F_LEITOR_SANS_M, F_LEITOR_SANS_G},
        {F_LEITOR_MONO_P, F_LEITOR_MONO_M, F_LEITOR_MONO_G},
        {F_LEITOR_LITERATA_P, F_LEITOR_LITERATA_M, F_LEITOR_LITERATA_G},
        {F_LEITOR_ATKINSON_P, F_LEITOR_ATKINSON_M, F_LEITOR_ATKINSON_G},
        {F_LEITOR_INTER_P, F_LEITOR_INTER_M, F_LEITOR_INTER_G},
        {F_LEITOR_SOURCE_P, F_LEITOR_SOURCE_M, F_LEITOR_SOURCE_G},
        {F_LEITOR_SERIF_FORTE_P, F_LEITOR_SERIF_FORTE_M, F_LEITOR_SERIF_FORTE_G},
        {F_LEITOR_SANS_FORTE_P, F_LEITOR_SANS_FORTE_M, F_LEITOR_SANS_FORTE_G},
        {F_LEITOR_MONO_FORTE_P, F_LEITOR_MONO_FORTE_M, F_LEITOR_MONO_FORTE_G},
        {F_LEITOR_LITERATA_FORTE_P, F_LEITOR_LITERATA_FORTE_M, F_LEITOR_LITERATA_FORTE_G},
        {F_LEITOR_ATKINSON_FORTE_P, F_LEITOR_ATKINSON_FORTE_M, F_LEITOR_ATKINSON_FORTE_G},
        {F_LEITOR_INTER_FORTE_P, F_LEITOR_INTER_FORTE_M, F_LEITOR_INTER_FORTE_G},
        {F_LEITOR_SOURCE_FORTE_P, F_LEITOR_SOURCE_FORTE_M, F_LEITOR_SOURCE_FORTE_G},
    };
    return fontes[(fam >= 0 && fam < 14) ? fam : 0]
                  [(tam >= 0 && tam < 3) ? tam : 1];
}

void vista_leitor(const estado_t *e, vista_leitor_t *out)
{
    memset(out, 0, sizeof *out);

    // O título da OBRA na barra: quem lê já sabe que está lendo.
    snprintf(out->titulo, sizeof out->titulo, "%s", e->obra_aberta.titulo);
    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    out->bateria = e->bateria;
    out->wifi = vista_wifi_da_barra(e);
    out->sinc = vista_sinc_da_barra(e);
    out->capa = e->leitor_na_capa;
    out->fonte = vista_fonte_leitor(e->leitor.letra, e->leitor.familia);
    out->alinhamento = e->leitor.alinhamento;
    out->barra_progresso = e->obra_aberta.rodape == 1;
    out->progresso_pct = e->leitura_pct;

    snprintf(out->texto, sizeof out->texto, "%s", e->pagina);

    // ── a régua ──────────────────────────────────────────────────────────
    // "◀ 38% · 46/121 ▶": as setas ensinam que ◀▶ viram página.
    if (out->capa) snprintf(out->regua, sizeof out->regua, "%s", "Capa  ▶");
    else if (e->obra_aberta.rodape == 2 && e->paginas_total > 0)
        snprintf(out->regua, sizeof out->regua, "◀ %d/%d ▶",
                 e->pagina_atual, e->paginas_total);
    else if (e->obra_aberta.rodape == 1)
        snprintf(out->regua, sizeof out->regua, "◀ %d%% ▶", e->leitura_pct);
    else if (e->paginas_total > 0)
        snprintf(out->regua, sizeof out->regua, "◀ %d%% · %d/%d ▶",
                 e->leitura_pct, e->pagina_atual, e->paginas_total);
    else
        snprintf(out->regua, sizeof out->regua, "◀ %d%% · %d/… ▶",
                 e->leitura_pct, e->pagina_atual);

    if (e->leitor_erro[0]) {
        snprintf(out->erro, sizeof out->erro, "%s", e->leitor_erro);
        snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK Acervo");
        return;
    }

    // O FIM pergunta: concluir sozinho marcaria como lida a obra folheada.
    if (e->leitor_no_fim) {
        out->no_fim = true;
        snprintf(out->pergunta, sizeof out->pergunta, "%s",
                 "Você chegou ao fim");
        snprintf(out->sim, sizeof out->sim, "%s", "Marcar como concluída");
        snprintf(out->nao, sizeof out->nao, "%s", "Manter em leitura");
    }

    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK Acervo");
}
