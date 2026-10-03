#include "calendario.h"
#include "campos.h"
#include "../nucleo/data.h"
#include <stdio.h>
#include <string.h>

// ── a tira do dia sob o cursor ──────────────────────────────────────
// Lê `e->itens`, o cache do dia visto, o mesmo que o Dia abre. Três linhas:
// é uma espiada; o resto vira contagem, que diz que há mais atrás do OK.
static void monta_tira(const estado_t *e, vista_cal_t *out)
{
    data_t d = e->dia_visto;

    bool e_hoje = (e->hoje.ano == d.ano && e->hoje.mes == d.mes &&
                   e->hoje.dia == d.dia);
    snprintf(out->tira_rot, sizeof out->tira_rot, "%s %d%s",
             data_semana_curta(d), d.dia, e_hoje ? " · hoje" : "");

    // ── um dia FORA da janela ────────────────────────────────────────────
    // A grade sabe que há algo (marcas do servidor) e a prévia não tem o quê.
    // Tira vazia diria "não tem nada", o contrário do pontinho.
    if (e->dia_fora_da_janela) {
        snprintf(out->tira[out->n_tira++], sizeof out->tira[0], "%s",
                 "OK abre este dia");
        return;
    }

    int por_fazer = 0, n_eventos = 0, feitas = 0;
    for (int i = 0; i < e->n_itens; i++) {
        const item_t *it = &e->itens[i];
        if (it->tipo == TIPO_EVENTO) n_eventos++;

        // Compromisso primeiro e com hora: decide o dia. Tarefa vira contagem.
        if (it->tipo == TIPO_EVENTO && out->n_tira < 3) {
            char *l = out->tira[out->n_tira++];
            // "dia" para o que ocupa o dia inteiro, como no Dia: sem nada na frente
            // lia-se "não sei a hora".
            char h[HORA_TEXTO];
            vista_hora_do_item(e, it->dia_inteiro ? "dia" : it->hora,
                               h, sizeof h);
            if (h[0])
                snprintf(l, sizeof out->tira[0], "%s %s", h, it->titulo);
            else
                snprintf(l, sizeof out->tira[0], "%s", it->titulo);
            continue;
        }
        // A tarefa conta pela mesma regra da marca e do Dia. Sem prazo, não
        // pertence a dia nenhum: vive na Agenda.
    }

    // As tarefas vêm do cache de pendentes: `itens` é a pasta do dia, e a
    // concluída hoje pode ter nascido em agosto.
    for (int i = 0; i < e->n_pendentes; i++) {
        const item_t *it = &e->pendentes[i];
        if (it->tipo != TIPO_TAREFA) continue;

        // Concluída conta pela CONCLUSÃO, pendente pelo PRAZO ("o que fiz" e "o
        // que tinha de fazer"). Quem decide é `vista_tarefa_no_dia`.
        if (!vista_tarefa_no_dia(e, it, e->dia_visto, false)) continue;
        if (it->feita) feitas++;
        else           por_fazer++;
    }

    // A palavra muda com o que as tarefas são: "+ 3 por fazer" sobre três
    // fechadas é contagem certa com palavra errada.
    if (por_fazer && feitas)
        snprintf(out->tira_mais, sizeof out->tira_mais, "+ %d tarefas",
                 por_fazer + feitas);
    else if (feitas)
        snprintf(out->tira_mais, sizeof out->tira_mais, "+ %d concluída%s",
                 feitas, feitas > 1 ? "s" : "");
    else if (por_fazer)
        snprintf(out->tira_mais, sizeof out->tira_mais, "+ %d por fazer",
                 por_fazer);

    // ── a contagem, em PALAVRAS ──────────────────────────────────────────
    // Ensina as marcas no contexto (o dia sob o cursor) e some quando não há o
    // que ensinar. Zero não aparece.
    {
        char *c = out->contagem;
        size_t n = sizeof out->contagem;
        int usado = 0;

        if (n_eventos)
            usado = snprintf(c, n, "%d evento%s", n_eventos,
                             n_eventos > 1 ? "s" : "");
        if (por_fazer && usado >= 0 && (size_t)usado < n)
            snprintf(c + usado, n - (size_t)usado, "%s%d tarefa%s",
                     usado ? " · " : "", por_fazer, por_fazer > 1 ? "s" : "");
    }

    // ── o MÊS vazio ──────────────────────────────────────────────────────
    // Grade limpa pergunta "não tem nada ou não carregou?": a tela responde.
    for (int dia = 1; dia <= out->n_dias; dia++)
        if (out->tem_tarefa[dia] || out->tem_evento[dia]) return;

    snprintf(out->vazio, sizeof out->vazio, "Nada marcado em %s.",
             data_mes_longo(d));
}

void vista_calendario(const estado_t *e, vista_cal_t *out)
{
    memset(out, 0, sizeof *out);

    data_t d = e->dia_visto;

    // A barra nomeia a TELA aberta; o mês é o cabeçalho.
    snprintf(out->titulo, sizeof out->titulo, "%s", "Calendário");
    snprintf(out->mes, sizeof out->mes, "%s %d", data_mes_longo(d), d.ano);
    vista_maiuscula1(out->mes);

    // "◀▶ dia", não "◀ mês ▶": os botões andam de DIA e o mês vira sozinho na
    // borda.
    snprintf(out->nav_mes, sizeof out->nav_mes, "%s", "◀▶ dia");
    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    out->bateria = e->bateria;
    out->wifi = vista_wifi_da_barra(e);
    out->sinc = vista_sinc_da_barra(e);

    data_t primeiro = { d.ano, d.mes, 1 };
    out->primeiro_dw = data_dia_da_semana(primeiro);
    out->n_dias      = data_dias_no_mes(d.ano, d.mes);
    out->cursor_dia  = d.dia;
    out->hoje = (e->hoje.ano == d.ano && e->hoje.mes == d.mes) ? e->hoje.dia : 0;

    // As marcas vêm do mês inteiro, guardadas no estado.
    for (int dia = 1; dia <= out->n_dias; dia++) {
        uint32_t bit = 1u << (dia - 1);
        out->tem_tarefa[dia] = (e->marcas_tarefa & bit) != 0;
        out->tem_evento[dia]  = (e->marcas_evento  & bit) != 0;
    }

    monta_tira(e, out);

    // A esquerda é a SAÍDA, como em toda tela; a legenda virou a contagem em
    // palavras (ver `monta_tira`).
    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK hoje");
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK abrir o dia");
}


// ── ESCOLHER um dia ──────────────────────────────────────────────────
// A mesma grade; muda o que está em volta. O cabeçalho é o ITEM que vai
// mudar, e a tira responde "vai ficar em que dia?". Rota própria
// (`TELA_ESCOLHER_DIA`): o OK carimba e volta.
void vista_escolher_dia(const estado_t *e, vista_cal_t *out)
{
    vista_calendario(e, out);

    const item_t *it = &e->aberto;
    data_t d = e->dia_visto;

    // O item no lugar do nome da área: quem está aqui precisa saber o que vai
    // ser remarcado.
    snprintf(out->titulo, sizeof out->titulo, "%s",
             it->titulo[0] ? it->titulo : "Escolher");

    // A tira vira CONFIRMAÇÃO.
    memset(out->tira, 0, sizeof out->tira);
    out->n_tira = 0;
    out->tira_mais[0] = '\0';
    out->vazio[0] = '\0';

    snprintf(out->tira_rot, sizeof out->tira_rot, "%s %d %s",
             data_semana_curta(d), d.dia, data_mes_curto(d));

    snprintf(out->tira[0], sizeof out->tira[0], "%s",
             it->tipo == TIPO_TAREFA ? "novo prazo" : "nova data");
    out->n_tira = 1;

    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK volta");
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK marcar aqui");
}
