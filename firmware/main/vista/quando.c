#include "quando.h"
#include "campos.h"
#include "../nucleo/data.h"
#include <stdio.h>
#include <string.h>

// "sex 4 set": como se fala do dia.
static void dia_escrito(data_t d, char *out, size_t max)
{
    snprintf(out, max, "%s %d %s", data_semana_curta(d), d.dia,
             data_mes_curto(d));
}

// O rótulo diz a RELAÇÃO ("Amanhã") e o valor a data.
static void poe_dia(vista_quando_t *out, const char *rotulo, data_t quando)
{
    vista_cartao_t *c = &out->cartao;
    if (c->n_dest >= CARTAO_DESTINOS_MAX) return;

    out->acao[c->n_dest]  = QUANDO_DIA;
    out->datas[c->n_dest] = quando;

    // SEM ícone: três marcas iguais não distinguem nada; quem informa é a data.
    c->dest[c->n_dest].ico = ICO_NENHUM;
    snprintf(c->dest[c->n_dest].titulo, sizeof c->dest[0].titulo, "%s",
             rotulo);
    dia_escrito(quando, c->dest[c->n_dest].valor,
                sizeof c->dest[0].valor);
    c->n_dest++;
}

static void poe_acao(vista_quando_t *out, quando_acao_t acao, icone_id ico,
                     const char *titulo, const char *sub)
{
    vista_cartao_t *c = &out->cartao;
    if (c->n_dest >= CARTAO_DESTINOS_MAX) return;

    out->acao[c->n_dest] = acao;
    memset(&out->datas[c->n_dest], 0, sizeof out->datas[0]);

    c->dest[c->n_dest].ico = ico;
    snprintf(c->dest[c->n_dest].titulo, sizeof c->dest[0].titulo, "%s",
             titulo);
    snprintf(c->dest[c->n_dest].sub, sizeof c->dest[0].sub, "%s", sub);
    c->n_dest++;
}

void vista_quando(const estado_t *e, vista_quando_t *out)
{
    memset(out, 0, sizeof *out);
    vista_cartao_t *c = &out->cartao;

    snprintf(c->titulo, sizeof c->titulo, "%s", "Agenda");
    vista_hora_da_barra(e, c->hora, sizeof c->hora);
    c->bateria = e->bateria;
    c->wifi    = vista_wifi_da_barra(e);
    c->sinc    = vista_sinc_da_barra(e);
    c->pontos    = -1;
    c->barra_pct = -1;

    const item_t *it = &e->aberto;

    // O card mostra de onde se sai: adiar é comparar duas datas.
    snprintf(c->kicker, sizeof c->kicker, "%s", "MUDAR A DATA");
    snprintf(c->nome, sizeof c->nome, "%s", it->titulo);
    // O ícone do card (15 px, à esquerda do kicker).
    c->icone = it->tipo == TIPO_TAREFA ? ICO_CAIXA : ICO_EVENTO;

    snprintf(c->fatos[c->n_fatos].rotulo, sizeof c->fatos[0].rotulo, "%s",
             "está em");
    if (it->vence.ano) {
        char quando[24];
        dia_escrito(it->vence, quando, sizeof quando);
        if (it->hora[0]) {
            char hora[12];
            vista_hora_do_item(e, it->hora, hora, sizeof hora);
            snprintf(c->fatos[c->n_fatos].valor, sizeof c->fatos[0].valor,
                     "%s · %s", quando, hora);
        }
        else
            snprintf(c->fatos[c->n_fatos].valor, sizeof c->fatos[0].valor,
                     "%s", quando);

    } else {
        snprintf(c->fatos[c->n_fatos].valor, sizeof c->fatos[0].valor, "%s",
                 "sem data");
    }
    c->n_fatos++;

    // ── os dias prontos ──────────────────────────────────────────────────
    // Amanhã, depois e a semana que vem. Hoje não entra: adiar para hoje não é
    // adiar.
    poe_dia(out, "Amanhã",         data_soma_dias(e->hoje, 1));
    poe_dia(out, "Depois",         data_soma_dias(e->hoje, 2));
    poe_dia(out, "Semana que vem", data_soma_dias(e->hoje, 7));

    // E o resto do mês, pela mesma grade da Agenda. `ICO_ENTRA`, não
    // `ICO_AREA_AGENDA` (44 px, dos cartões da Home; numa lista de 15 px sai por
    // cima do texto).
    poe_acao(out, QUANDO_CALENDARIO, ICO_ENTRA, "Escolher no mês",
             "qualquer dia");

    // TIRAR a data: só tarefa, e só com data (evento sem data não existe no
    // Calendar).
    if (it->tipo == TIPO_TAREFA && it->vence.ano)
        poe_acao(out, QUANDO_SEM_DATA, ICO_NENHUM, "Tirar a data",
                 "volta a ser sem prazo");

    snprintf(c->rodape_esq, sizeof c->rodape_esq, "%s", "BACK item");
    snprintf(c->rodape_dir, sizeof c->rodape_dir, "%s", "OK mudar");
}


// ── a HORA ───────────────────────────────────────────────────────────
// A mesma gramática da data, uma escala abaixo: o relativo primeiro, depois
// as âncoras. O valor que sobe é sempre "15:00"; o desenho passa por
// `vista_hora_do_item` (12 h ou 24 h).
static void poe_hora(const estado_t *e, vista_quando_t *out,
                     const char *rotulo, int h, int m)
{
    vista_cartao_t *c = &out->cartao;
    if (c->n_dest >= CARTAO_DESTINOS_MAX) return;
    if (h < 0 || h > 23) return;

    out->acao[c->n_dest] = QUANDO_HORA;
    char hhmm[6];
    hhmm[0] = (char)('0' + h / 10); hhmm[1] = (char)('0' + h % 10);
    hhmm[2] = ':';
    hhmm[3] = (char)('0' + m / 10); hhmm[4] = (char)('0' + m % 10);
    hhmm[5] = '\0';
    snprintf(out->horas[c->n_dest], sizeof out->horas[0], "%s", hhmm);

    c->dest[c->n_dest].ico = ICO_NENHUM;
    snprintf(c->dest[c->n_dest].titulo, sizeof c->dest[0].titulo, "%s",
             rotulo);
    vista_hora_do_item(e, hhmm, c->dest[c->n_dest].valor,
                       sizeof c->dest[0].valor);
    c->n_dest++;
}

void vista_horario(const estado_t *e, vista_quando_t *out)
{
    memset(out, 0, sizeof *out);
    vista_cartao_t *c = &out->cartao;

    snprintf(c->titulo, sizeof c->titulo, "%s", "Agenda");
    vista_hora_da_barra(e, c->hora, sizeof c->hora);
    c->bateria = e->bateria;
    c->wifi    = vista_wifi_da_barra(e);
    c->sinc    = vista_sinc_da_barra(e);
    c->pontos    = -1;
    c->barra_pct = -1;

    const item_t *it = &e->aberto;

    snprintf(c->kicker, sizeof c->kicker, "%s", "MUDAR A HORA");
    snprintf(c->nome, sizeof c->nome, "%s", it->titulo);
    c->icone = ICO_RELOGIO;

    snprintf(c->fatos[0].rotulo, sizeof c->fatos[0].rotulo, "%s", "está às");
    if (it->hora[0])
        vista_hora_do_item(e, it->hora, c->fatos[0].valor,
                           sizeof c->fatos[0].valor);
    else
        snprintf(c->fatos[0].valor, sizeof c->fatos[0].valor, "%s",
                 "dia inteiro");
    c->n_fatos = 1;

    // Uma hora para lá, uma para cá: só quando há hora de onde partir.
    int h = 0, m = 0;
    bool tem_hora = it->hora[0] && sscanf(it->hora, "%d:%d", &h, &m) == 2;

    if (tem_hora) {
        if (h < 23) poe_hora(e, out, "Uma hora depois", h + 1, m);
        if (h > 0)  poe_hora(e, out, "Uma hora antes",  h - 1, m);
    }

    // E o mostrador, para qualquer hora: exato, sem âncoras.
    poe_acao(out, QUANDO_RELOGIO, ICO_ENTRA, "Escolher a hora",
             "qualquer horário");

    // As âncoras do dia ("de manhã", "depois do almoço") só para quem NÃO tem
    // hora. Com hora saem: o card não rola, e as últimas sumiam sob o rodapé.
    if (!tem_hora) {
        poe_hora(e, out, "De manhã", 9, 0);
        poe_hora(e, out, "À tarde", 14, 0);
    }

    // DIA INTEIRO é tirar a hora (`start.date` do Calendar).
    if (tem_hora) {
        vista_cartao_t *cc = c;
        if (cc->n_dest < CARTAO_DESTINOS_MAX) {
            out->acao[cc->n_dest] = QUANDO_DIA_INTEIRO;
            cc->dest[cc->n_dest].ico = ICO_NENHUM;
            snprintf(cc->dest[cc->n_dest].titulo, sizeof cc->dest[0].titulo,
                     "%s", "Dia inteiro");
            snprintf(cc->dest[cc->n_dest].sub, sizeof cc->dest[0].sub, "%s",
                     "sem horário");
            cc->n_dest++;
        }
    }

    // O `due` do Google Tasks guarda só a data; a tela diz isso para não
    // parecer defeito de sincronização.
    if (it->tipo == TIPO_TAREFA)
        snprintf(c->corpo, sizeof c->corpo, "%s",
                 "A hora fica no Tinto: o Google Tasks guarda só a data "
                 "do prazo.");

    snprintf(c->rodape_esq, sizeof c->rodape_esq, "%s", "BACK item");
    snprintf(c->rodape_dir, sizeof c->rodape_dir, "%s", "OK mudar");
}


// ── o MOSTRADOR: qualquer hora ───────────────────────────────────────
// Duas casas: ◀▶ anda, ▲▼ muda (eixos diferentes num direcional). Não são
// os cinco campos do relógio: aqui se marca um compromisso.
void vista_escolher_hora(const estado_t *e, vista_mostrador_t *out)
{
    memset(out, 0, sizeof *out);

    snprintf(out->titulo, sizeof out->titulo, "%s", "Agenda");
    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    out->bateria = e->bateria;
    out->wifi    = vista_wifi_da_barra(e);
    out->sinc    = vista_sinc_da_barra(e);

    const item_t *it = &e->aberto;
    snprintf(out->kicker, sizeof out->kicker, "%s", "MUDAR A HORA");
    snprintf(out->nome, sizeof out->nome, "%s", it->titulo);

    int h = e->escolha_h, m = e->escolha_m;
    if (h < 0 || h > 23) h = 0;
    if (m < 0 || m > 59) m = 0;

    out->casa[0][0] = (char)('0' + h / 10);
    out->casa[0][1] = (char)('0' + h % 10);
    out->casa[0][2] = '\0';
    out->casa[1][0] = (char)('0' + m / 10);
    out->casa[1][1] = (char)('0' + m % 10);
    out->casa[1][2] = '\0';

    out->campo = e->escolha_campo ? 1 : 0;

    // O rodapé diz a HORA que o OK vai marcar: a última chance de ler.
    char hhmm[6];
    snprintf(hhmm, sizeof hhmm, "%s:%s", out->casa[0], out->casa[1]);

    char lido[12];
    vista_hora_do_item(e, hhmm, lido, sizeof lido);

    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK volta");
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "OK marcar %s", lido);
}
