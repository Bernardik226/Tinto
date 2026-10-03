#include "conferir.h"
#include "campos.h"
#include "../nucleo/data.h"
#include <stdio.h>
#include <string.h>


// O verbo diz o que o botão FAZ ("salvar" não descreve marcar nem riscar).
static const char *verbo_da_decisao(const estado_t *e)
{
    // Com mais de um, o verbo genérico. "As duas coisas" com três seria contar
    // errado logo na tela de conferir.
    if (e->n_resultados == 2) return "Fazer as duas coisas";
    if (e->n_resultados  > 2) return "Fazer as três coisas";

    const resultado_t *r = &e->resultados[0];
    // Apagar primeiro, e o botão DIZ "Apagar": é o único que tira algo do
    // mundo.
    if (r->verbo == RES_APAGOU) return "Apagar";
    if (r->verbo == RES_EDITOU) return "Confirmar a mudança";
    if (r->verbo == RES_ANOTOU) return "Guardar a anotação";

    switch (r->item.tipo) {
    case TIPO_EVENTO: return "Marcar na agenda";
    case TIPO_LISTA:  return "Criar a lista";
    case TIPO_TAREFA: return "Criar a tarefa";
    default:          return "Guardar";
    }
}

static icone_id icone_de(const item_t *it)
{
    switch (it->tipo) {
    case TIPO_EVENTO:   return ICO_EVENTO;
    case TIPO_LISTA:    return ICO_LISTA;
    case TIPO_TAREFA:   return ICO_CAIXA;
    case TIPO_ANOTACAO: return ICO_NOTA;
    default:            return ICO_MIC;
    }
}

static void monta_resultados(const estado_t *e, vista_conferir_t *out)
{
    out->n_res = e->n_resultados > RESULTADOS_MAX ? RESULTADOS_MAX
                                                  : e->n_resultados;
    for (int i = 0; i < out->n_res; i++) {
        const item_t      *it = &e->resultados[i].item;
        linha_resultado_t *l  = &out->res[i];

        l->icone = icone_de(it);
        // Tipo e destino na mesma linha, a mesma frase do kicker do detalhe.
        vista_destino(it, l->tipo, sizeof l->tipo);
        snprintf(l->titulo, sizeof l->titulo, "%s", it->titulo);

        // Dia e hora quando existem; "sem data" quando não (tarefa sem prazo é o
        // normal). A faixa INTEIRA: o fim decide se o resto do dia cabe.
        char faixa[20];
        vista_faixa(it, e->config.valor[AJUSTE_HORA24] != 0, faixa, sizeof faixa);

        if (it->vence.ano && faixa[0])
            snprintf(l->quando, sizeof l->quando, "%s %d %s · %s",
                     data_semana_curta(it->vence), it->vence.dia,
                     data_mes_curto(it->vence), faixa);
        else if (it->vence.ano)
            snprintf(l->quando, sizeof l->quando, "%s %d %s",
                     data_semana_curta(it->vence), it->vence.dia,
                     data_mes_curto(it->vence));
        else if (faixa[0])
            snprintf(l->quando, sizeof l->quando, "%s", faixa);
        else
            snprintf(l->quando, sizeof l->quando, "%s", "sem data");

        snprintf(l->local, sizeof l->local, "%s", it->local);

        // ── o que estava lá ANTES ────────────────────────────────────────────
        // Só quando a ação alcança algo que existe, na mesma régua do `quando`, para
        // as duas linhas se lerem em par.
        const resultado_t *r = &e->resultados[i];
        l->apagando = r->verbo == RES_APAGOU;

        if (r->tem_antes) {
            char faixa_antes[20] = "";
            if (r->antes_hora[0])
                snprintf(faixa_antes, sizeof faixa_antes, "%s",
                         r->antes_hora);

            if (r->antes_vence.ano && faixa_antes[0])
                snprintf(l->antes, sizeof l->antes, "%s %d %s · %s",
                         data_semana_curta(r->antes_vence),
                         r->antes_vence.dia,
                         data_mes_curto(r->antes_vence), faixa_antes);
            else if (r->antes_vence.ano)
                snprintf(l->antes, sizeof l->antes, "%s %d %s",
                         data_semana_curta(r->antes_vence),
                         r->antes_vence.dia,
                         data_mes_curto(r->antes_vence));
            else
                snprintf(l->antes, sizeof l->antes, "%s", "sem data");

            // Título trocado também é mudança, e a que mais assusta.
            if (strcmp(r->antes_titulo, it->titulo) != 0) {
                size_t tem = strlen(l->antes);
                snprintf(l->antes + tem, sizeof l->antes - tem, " · %s",
                         r->antes_titulo);
            }
        }
    }
}

void vista_conferir(const estado_t *e, vista_conferir_t *out)
{
    memset(out, 0, sizeof *out);
    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    out->bateria = e->bateria;
    out->wifi = vista_wifi_da_barra(e);
    out->sinc = vista_sinc_da_barra(e);

    const item_t *it = &e->ultimo;

    // O que foi captado: não depende de a IA ter acertado.

    if (e->ultimo_trechos > 1)
        snprintf(out->trechos, sizeof out->trechos, "%d trechos · %d:%02d",
                 e->ultimo_trechos, it->dur_s / 60, it->dur_s % 60);
    else
        snprintf(out->trechos, sizeof out->trechos, "%d:%02d de fala",
                 it->dur_s / 60, it->dur_s % 60);

    // "Conferir": a barra nomeia o gesto de quem olha, não o que o aparelho
    // fez.
    snprintf(out->titulo, sizeof out->titulo, "%s", "Conferir");
    monta_resultados(e, out);

    out->enviando = e->esperando_resultado;
    out->pontos   = (int)((e->agora_ms / 1000u) % 3u);

    // Conta como gente: "1 AÇÃO", não "1 ações".
    snprintf(out->kicker, sizeof out->kicker, "%d %s NESTA GRAVAÇÃO",
             out->n_res, out->n_res == 1 ? "AÇÃO" : "AÇÕES");

    snprintf(out->falou, sizeof out->falou, "%s", e->falou);

    snprintf(out->decisao[0].texto, sizeof out->decisao[0].texto, "%s",
             verbo_da_decisao(e));
    out->decisao[0].icone = ICO_VISTO;

    // "Descartar" diz que apaga a gravação junto.
    snprintf(out->decisao[1].texto, sizeof out->decisao[1].texto, "%s",
             "Descartar a gravação");
    out->decisao[1].icone = ICO_X;

    // O cursor percorre os resultados (só para rolar e reler) e depois as
    // decisões, e COMEÇA na primeira decisão.
    out->cursor = e->cursor - out->n_res;
    if (out->cursor < 0)  out->cursor = -1;   // está lendo, não decidindo
    if (out->cursor > 1)  out->cursor = 1;
    out->cursor_res = e->cursor < out->n_res ? e->cursor : -1;

    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "▲▼ escolher");
    // Lendo um resultado o OK não faz nada, e o rodapé não promete (RN-3G).
    if (out->cursor < 0)
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "▼ decidir");
    else
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "OK %s",
                 out->cursor == 0 ? "confirmar" : "descartar");

    out->pontos = out->enviando
                ? (int)((e->agora_ms / 1000u) % 4u) : -1;

    if (out->enviando) {
        // Enviando: nem saída nem ação no rodapé, só "aguarde".
        snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "");
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "aguarde");
    }
}

// Os resultados e as duas decisões; sem resultado, zero.
int vista_recibo_linhas(const estado_t *e)
{
    return e->n_resultados ? e->n_resultados + 2 : 0;
}
