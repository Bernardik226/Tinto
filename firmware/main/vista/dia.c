#include "dia.h"
#include "campos.h"
#include "../nucleo/data.h"
#include <stdio.h>
#include <string.h>

// Onde a linha cai na régua, em minutos. Dia inteiro antes de tudo (-1);
// sem hora vai para o fim.
static int quando_na_regua(const linha_captura_t *l)
{
    // "dia" é o rótulo do dia inteiro: sem lugar na régua, vem antes.
    if (strcmp(l->hora, "dia") == 0) return -1;
    int m = data_minutos(l->hora);
    return m < 0 ? 24 * 60 + 1 : m;
}

void vista_dia(const estado_t *e, vista_dia_t *out)
{
    memset(out, 0, sizeof *out);

    data_t d = e->dia_visto;
    snprintf(out->titulo, sizeof out->titulo, "%s %d %s",
             data_semana_curta(d), d.dia, data_mes_curto(d));
    vista_maiuscula1(out->titulo);
    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    out->bateria = e->bateria;
    out->wifi = vista_wifi_da_barra(e);
    out->sinc = vista_sinc_da_barra(e);

    // Quantos dias este está atrás de hoje (negativo = futuro): decide o que
    // entra e o rótulo.
    int atras = data_dias_entre(d, e->hoje);

    for (int i = 0; i < e->n_itens; i++) {
        const item_t *it = &e->itens[i];

        // ── compromissos ─────────────────────────────────────────────────────
        // Com ou sem hora (feriado ocupa o dia). O que passa do teto é contado.
        // A GRAVAÇÃO não é compromisso: tem nota e não tem título, e já está dentro
        // do item que criou. Evento do Google sem título não tem nota e aparece.
        if (it->nota[0] && !it->titulo[0]) continue;

        // Na RÉGUA: evento sempre, e a tarefa com hora (o híbrido).
        bool na_regua = it->tipo == TIPO_EVENTO ||
                        (it->tipo == TIPO_TAREFA && it->hora[0]);

        if (na_regua && out->n_compromissos >= DIA_MAX) {
            out->mais_compromissos++;
            continue;
        }
        if (na_regua) {
            linha_captura_t *l = &out->compromissos[out->n_compromissos++];
            memset(l, 0, sizeof *l);
            l->indice = (int16_t)i;
            l->caixa  = it->tipo == TIPO_TAREFA;
            l->feita  = it->feita;
            vista_hora_do_item(e, it->dia_inteiro ? "dia" : it->hora,
                               l->hora, sizeof l->hora);
            vista_titulo_do_item(e, it, l->titulo, sizeof l->titulo);
            // O local como subtítulo, como no desenho.
            snprintf(l->sub,    sizeof l->sub,    "%s", it->local);
            continue;
        }

    }

    // Em ORDEM: dia inteiro primeiro, depois por hora, os sem hora no fim.
    // Insertion sort: n <= DIA_MAX.
    for (int i = 1; i < out->n_compromissos; i++) {
        linha_captura_t chave = out->compromissos[i];
        int peso = quando_na_regua(&chave);
        int j = i - 1;
        while (j >= 0 && quando_na_regua(&out->compromissos[j]) > peso) {
            out->compromissos[j + 1] = out->compromissos[j];
            j--;
        }
        out->compromissos[j + 1] = chave;
    }

    // ── tarefas: do cache de PENDENTES ───────────────────────────────────
    // Não de `itens` (a pasta do dia): a concluída hoje pode morar em agosto. É a
    // mesma fonte da Agenda. Exceto num dia CONSULTADO: ele veio inteiro do
    // servidor em `itens`, e as pendentes seriam as de hoje.
    bool consultado = e->dia_consultado.ano &&
                      data_igual(e->dia_consultado, d);

    const item_t *fonte = consultado ? e->itens : e->pendentes;
    int quantos = consultado ? e->n_itens : e->n_pendentes;

    for (int i = 0; i < quantos; i++) {
        const item_t *it = &fonte[i];
        if (it->tipo != TIPO_TAREFA) continue;

        // Pelo PRAZO ou pela CONCLUSÃO ("o que eu tinha de fazer" e "o que fiz"),
        // decidido por `vista_tarefa_no_dia`.
        if (!vista_tarefa_no_dia(e, it, d, false)) continue;

        if (out->n_tarefas >= DIA_MAX) { out->mais_tarefas++; continue; }

        linha_trabalho_t *l = &out->tarefas[out->n_tarefas++];
        memset(l, 0, sizeof *l);
        l->indice  = (int16_t)i;
        l->feita   = it->feita;
        l->de_fora = it->origem == ORIGEM_GOOGLE;
        l->grupo   = (int8_t)vista_grupo(out->grupos, &out->n_grupos,
                                             vista_lista_da_tarefa(it));
        snprintf(l->titulo, sizeof l->titulo, "%s", it->titulo);
    }

    // O rótulo do dia só existe quando o dia tem algo: título de seção sem
    // conteúdo promete o que não há.
    if (out->n_compromissos || out->n_tarefas) {
        if (atras > 0) {
            snprintf(out->rotulo, sizeof out->rotulo, "%s", "Aconteceu");
            if (atras == 1)
                snprintf(out->quando, sizeof out->quando, "ontem");
            else
                snprintf(out->quando, sizeof out->quando, "há %d dias", atras);
        } else if (atras < 0) {
            snprintf(out->rotulo, sizeof out->rotulo, "%s", "Vai acontecer");
            // O quanto falta, como no passado o quanto faz.
            if (atras == -1)
                snprintf(out->quando, sizeof out->quando, "amanhã");
            else
                snprintf(out->quando, sizeof out->quando, "em %d dias", -atras);
        } else {
            snprintf(out->rotulo, sizeof out->rotulo, "%s", "Hoje");
        }
    } else if (!consultado &&
               (e->dia_fora_da_janela ||
                (e->dia_pedido[0] && data_igual(e->dia_pedido_data, d)))) {
        // ── o dia que ainda está VINDO ───────────────────────────────────────
        // Fora da janela, até a resposta o aparelho não sabe o que ele tem: dizer
        // "Nada marcado" contrariava o pontinho do calendário.
        // ponytail: texto, não os pontinhos animados — exigiriam campo na vista e
        // laço no ui/dia.c, e a política de refresh é do vidro.
        snprintf(out->vazio, sizeof out->vazio, "%s", "Buscando este dia…");
    } else {
        // A mesma família de frase da Agenda: a mesma pergunta, a mesma resposta.
        snprintf(out->vazio, sizeof out->vazio, "%s",
                 "Nada marcado neste dia.");
    }

    out->cursor = e->travado ? -1 : e->cursor;
    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK voltar");
    if (out->n_compromissos || out->n_tarefas)
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK abrir");
}

// Compromissos E tarefas são paradas de cursor. Dia vazio devolve 0: um
// seletor invisível parece travado.
int vista_dia_linhas(const estado_t *e)
{
    // STATIC, não na pilha: na pilha já estourou a task APP (boot loop). Na
    // placa, PSRAM (VISTA_TRANSITORIA).
    static VISTA_TRANSITORIA vista_dia_t v;
    vista_dia(e, &v);
    return v.n_compromissos + v.n_tarefas;
}
