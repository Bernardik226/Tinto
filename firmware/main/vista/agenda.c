#include "agenda.h"
#include "campos.h"
#include "../tela/texto.h"
#include "../nucleo/data.h"
#include <stdio.h>
#include <string.h>

static bool tem_hora(const item_t *it) { return it->hora[0] != '\0'; }

// Entra na régua: evento (com hora ou de dia inteiro) e tarefa com hora —
// o híbrido, que ocupa um horário e se marca como tarefa.
static bool eh_agenda(const item_t *it)
{
    if (it->tipo == TIPO_TAREFA) return tem_hora(it);
    return it->tipo == TIPO_EVENTO && (tem_hora(it) || it->dia_inteiro);
}

// Ordem do dia; dia inteiro vale -1 e vem antes de tudo.
static int comeca_em(const item_t *it)
{
    return it->dia_inteiro ? -1 : data_minutos(it->hora);
}

// ── o dia vazio ──────────────────────────────────────────────────────
static void monta_vazio(const estado_t *e, vista_agenda_t *v)
{
    if (v->n_agenda || v->n_trabalho) return;

    // Fala só do dia na tela, que é o que o aparelho sabe. "A semana também"
    // afirmava o que as marcas do mês não cobrem.
    snprintf(v->vazio, sizeof v->vazio, "%s", "Seu dia está livre.");

    // O convite só aparece enquanto o aparelho nunca recebeu nem criou nada
    // (as marcas do mês dizem isso; `n_itens == 0` é só o dia vazio). Instrução
    // que fica vira aviso que ninguém lê.
    bool o_aparelho_tem_coisas =
        e->marcas_validas && (e->marcas_evento | e->marcas_tarefa);

    if (e->n_itens == 0 && e->nome[0] && !o_aparelho_tem_coisas)
        // Sem o "●": a F_MIUDA não tem o glifo.
        snprintf(v->convite, sizeof v->convite, "%s",
                 "Segure o botão de voz e diga o que precisa fazer. "
                 "O que você falar aparece aqui.");
}

// ── a régua ──────────────────────────────────────────────────────────
static void monta_agenda(const estado_t *e, vista_agenda_t *v)
{
    // O dia inteiro, inclusive o que já passou: é dado real, e o calendário
    // mostra. `agora` serve só para a contagem regressiva.
    bool eh_hoje = data_igual(e->dia_visto, e->hoje);
    int agora = e->hora * 60 + e->minuto;

    // Filtra por item, não cortando um prefixo da lista: o evento de dia
    // inteiro fica na frente e quebraria o corte.
    int idx[ITENS_MAX], n = 0, total = 0;
    for (int i = 0; i < e->n_itens; i++) {
        const item_t *it = &e->itens[i];
        if (!eh_agenda(it)) continue;
        total++;
        if (n < ITENS_MAX) idx[n++] = i;
    }

    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && comeca_em(&e->itens[idx[j]])
                              < comeca_em(&e->itens[idx[j-1]]); j--) {
            int t = idx[j]; idx[j] = idx[j-1]; idx[j-1] = t;
        }

    // Sem compromisso, a zona não existe: quem fala do dia vazio é `monta_vazio`.
    if (n == 0) return;

    // O destaque é o primeiro que ainda vai chegar, e só hoje. Dia inteiro e o
    // que já passou ficam de fora: "agora" num evento que terminou é mentira.
    int destaque = -1;
    if (eh_hoje)
        for (int i = 0; i < n; i++) {
            const item_t *c = &e->itens[idx[i]];
            if (c->dia_inteiro) continue;
            if (data_minutos(c->hora) < agora) continue;
            destaque = i;
            break;
        }

    for (int i = 0; i < n && v->n_agenda < AGENDA_MAX_COMPROMISSOS; i++) {
        const item_t *it = &e->itens[idx[i]];
        linha_agenda_t *l = &v->agenda[v->n_agenda++];
        memset(l, 0, sizeof *l);
        l->indice = (int16_t)idx[i];
        l->caixa  = it->tipo == TIPO_TAREFA;
        l->feita  = it->feita;

        // "dia" na coluna da hora: cabe na largura de "00:00".
        vista_hora_do_item(e, it->dia_inteiro ? "dia" : it->hora,
                           l->hora, sizeof l->hora);
        // RN-65: item ilegível aparece dizendo isso; sumir seria igual a nunca ter
        // existido.
        if (it->defeito)
            snprintf(l->oque, sizeof l->oque, "%s", "não consegui ler");
        else
            vista_titulo_do_item(e, it, l->oque, sizeof l->oque);

        // O local vai em todo card: endereço que aparece num e some no outro confunde.
        snprintf(l->onde, sizeof l->onde, "%s", it->local);

        // A contagem é só do próximo: duas contagens na tela é nenhuma (RN-32).
        l->proximo = (i == destaque);

        // A contagem só existe hoje: fora de hoje não há "agora" (e a conta virava
        // "em 14h01" num evento de amanhã às 14:00).
        if (l->proximo && eh_hoje)
            vista_falta(data_minutos(it->hora) - agora, l->quando, sizeof l->quando);
    }

}

// ── de que lista é esta tarefa ───────────────────────────────────────
//
// O nome da lista do Google viaja no `local` (em tarefa o campo de lugar está
// livre). Vazio na tarefa recém-falada até o pull trazê-lo: o grupo existe,
// sem título, para não repetir "TAREFAS" embaixo de "POR FAZER".
const char *vista_lista_da_tarefa(const item_t *it)
{
    return it->local;
}

// ── o Por fazer ──────────────────────────────────────────────────────
// RN-34: tarefa aberta aparece até ser feita. RN-35: a feita fica riscada
// onde está e não reordena.
static int prioridade(const item_t *it, const estado_t *e)
{
    if (it->vence.ano && data_compara(it->vence, e->hoje) < 0) return 0;
    if (it->vence.ano) return 1;
    return 2;
}

static bool vem_antes(const item_t *a, const item_t *b, const estado_t *e)
{
    int pa = prioridade(a, e), pb = prioridade(b, e);
    if (pa != pb) return pa < pb;
    if (a->vence.ano && b->vence.ano) return data_compara(a->vence, b->vence) < 0;
    return false;
}

// O selo diz que a linha é de OUTRO dia, e qual: a tarefa com prazo aparece
// hoje antes e depois de vencer. Negativo quando já venceu.
static void selo_de(const item_t *it, const estado_t *e, data_t dia,
                    linha_trabalho_t *l)
{
    if (it->feita || !it->vence.ano) return;

    if (data_igual(it->vence, dia)) return;

    int d = data_dias_entre(it->vence, e->hoje);   // > 0 = já venceu
    if (d == 0) return;

    l->selo_negativo = d > 0;
    int falta = d > 0 ? d : -d;
    if (falta < 7)
        snprintf(l->selo, sizeof l->selo, "%s", data_semana_curta(it->vence));
    else
        snprintf(l->selo, sizeof l->selo, "%d d", falta);
}

// O grupo da lista, criado se ainda não existe. Nasce da tarefa: lista sem
// nada pendente seria um título vazio.
int vista_grupo(grupo_trabalho_t *grupos, int *n, const char *nome)
{
    for (int i = 0; i < *n; i++)
        if (strcmp(grupos[i].titulo, nome) == 0) return i;

    if (*n >= AGENDA_MAX_GRUPOS) return -1;

    int i = (*n)++;
    memset(&grupos[i], 0, sizeof grupos[i]);
    snprintf(grupos[i].titulo, sizeof grupos[i].titulo, "%s", nome);
    return i;
}

// A contagem do grupo ("3 de 12") vem do item LISTA e é sobre o todo: diz
// que há mais lá dentro do que a tela mostra.
static void contagens(const estado_t *e, vista_agenda_t *v)
{
    for (int i = 0; i < e->n_itens; i++) {
        const item_t *it = &e->itens[i];
        if (it->tipo != TIPO_LISTA || it->lista_n <= 0) continue;

        for (int g = 0; g < v->n_grupos; g++)
            if (strcmp(v->grupos[g].titulo, it->titulo) == 0)
                snprintf(v->grupos[g].contagem, sizeof v->grupos[g].contagem,
                         "%d de %d", (int)it->lista_k, (int)it->lista_n);
    }
}

static void monta_trabalho(const estado_t *e, vista_agenda_t *v)
{
    // A lista não é linha: é o cabeçalho do grupo. A fonte é o cache de
    // PENDENTES, não o do dia: tarefa aberta continua aberta amanhã, e o bloco
    // fica parado quando a pessoa anda entre ontem, hoje e amanhã.
    const item_t *fonte = e->pendentes;

    int ordem[ITENS_MAX], n = 0;
    for (int i = 0; i < e->n_pendentes; i++) {
        if (fonte[i].tipo != TIPO_TAREFA) continue;

        // Em que dia a tarefa aparece é decidido num lugar só: `vista_tarefa_no_dia`.
        if (!vista_tarefa_no_dia(e, &fonte[i], e->dia_visto, true)) continue;
        ordem[n++] = i;
    }

    for (int i = 1; i < n; i++) {
        int chave = ordem[i], j = i - 1;
        while (j >= 0 && vem_antes(&fonte[chave], &fonte[ordem[j]], e)) {
            ordem[j + 1] = ordem[j]; j--;
        }
        ordem[j + 1] = chave;
    }

    for (int i = 0; i < n; i++)
        if (!fonte[ordem[i]].feita) v->n_abertas++;

    // Agrupa sem perder a urgência: os grupos saem na ordem da tarefa mais
    // urgente de cada um, e dentro do grupo a ordem continua.
    int agrupada[ITENS_MAX], na = 0;
    for (int i = 0; i < n; i++) {
        if (ordem[i] < 0) continue;       // já entrou no grupo de outra
        int g = vista_grupo(v->grupos, &v->n_grupos, vista_lista_da_tarefa(&fonte[ordem[i]]));
        if (g < 0) continue;              // grupos demais: o resto vira "+N"
        for (int j = i; j < n; j++) {
            if (ordem[j] < 0) continue;
            if (strcmp(vista_lista_da_tarefa(&fonte[ordem[j]]), v->grupos[g].titulo) != 0)
                continue;
            agrupada[na++] = ordem[j];
            ordem[j] = -1;
        }
    }

    // Todas entram: quem decide o que cabe é a página (ui/agenda.c). Só o teto
    // de memória corta, e o corte é contado no rótulo.
    for (int i = 0; i < na && v->n_trabalho < AGENDA_MAX_TRABALHO; i++) {
        const item_t *it = &fonte[agrupada[i]];
        linha_trabalho_t *l = &v->trabalho[v->n_trabalho++];
        memset(l, 0, sizeof *l);
        vista_titulo_do_item(e, it, l->titulo, sizeof l->titulo);
        selo_de(it, e, e->dia_visto, l);
        l->indice  = (int16_t)agrupada[i];
        l->feita   = it->feita;
        l->de_fora = it->origem == ORIGEM_GOOGLE;
        l->grupo   = (int8_t)vista_grupo(v->grupos, &v->n_grupos, vista_lista_da_tarefa(it));
    }

    contagens(e, v);

    int fora = (n - v->n_trabalho) + e->n_pendentes_fora;
    if (fora > 0 && !v->aviso[0])
        snprintf(v->aviso, sizeof v->aviso, "%d não couberam", fora);
}

// ── a montagem ──────────────────────────────────────────────────────
void vista_agenda(const estado_t *e, vista_agenda_t *out)
{
    memset(out, 0, sizeof *out);

    // A barra nomeia a área; a data vira o cabeçalho.
    snprintf(out->titulo, sizeof out->titulo, "%s", "Agenda");

    // O dia VISTO, não hoje: a Agenda anda entre ontem, hoje e amanhã.
    snprintf(out->dia_semana, sizeof out->dia_semana, "%s",
             data_semana_longa(e->dia_visto));
    vista_maiuscula(out->dia_semana);

    // Mês no meio da frase é minúsculo (`data_mes_longo` capitaliza).
    {
        char mes[16];
        snprintf(mes, sizeof mes, "%s", data_mes_longo(e->dia_visto));
        if (mes[0] >= 'A' && mes[0] <= 'Z') mes[0] = (char)(mes[0] + 32);
        snprintf(out->dia_longo, sizeof out->dia_longo, "%d de %s",
                 e->dia_visto.dia, mes);
    }

    // As setas ficam junto do dia que mudam, e dizem onde se está ("◀ hoje ▶").
    // A seta da borda continua desenhada: o rótulo não dança de largura.
    {
        int d = data_dias_entre(e->hoje, e->dia_visto);
        const char *onde = d < 0 ? "ontem" : d > 0 ? "amanhã" : "hoje";
        snprintf(out->nav_dia, sizeof out->nav_dia, "◀ %s ▶", onde);

        // Cursor solto = o direcional é do dia; o app decide com a mesma condição.
        out->nav_focado = e->cursor < 0 && !e->travado &&
                          e->overlay == OVERLAY_NADA;
    }
    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    out->bateria = e->bateria;
    out->wifi = vista_wifi_da_barra(e);
    out->sinc = vista_sinc_da_barra(e);

    // Um aviso só, o mais grave: o que não coube, depois o que o mês não vai
    // transcrever.
    if (e->n_fora) {
        // RN-4B: o que não coube é contado; lista que esconde em silêncio parece completa.
        snprintf(out->aviso, sizeof out->aviso, "%d não couberam", e->n_fora);
    } else if (e->quota.limite_s > 0 &&
               e->quota.usados_s >= e->quota.limite_s) {
        // T-08: estourou o limite, mas continua gravando.
        snprintf(out->aviso, sizeof out->aviso, "%s",
                 "sem transcrever até virar");
    }

    // Estado do aparelho, colado no rodapé, e só quando nada cobre a tela: a
    // recusa no gesto não pega ninguém de surpresa.
    out->sem_rede = (e->rede == REDE_DESLIGADA || e->rede == REDE_SEM_SINAL)
                 && e->overlay == OVERLAY_NADA
                 && e->gravacao.fase == GRAV_PARADA
                 && e->precisa_rede[0] == '\0';

    monta_trabalho(e, out);
    monta_agenda(e, out);
    monta_vazio(e, out);

    out->cursor          = e->travado ? -1 : e->cursor;
    out->foco            = e->cursor > 0 ? e->cursor : 0;
    out->cursor_agenda   = -1;
    out->cursor_trabalho = -1;
    out->alvo            = -1;

    // As linhas do cursor, na ordem de leitura: compromissos, depois tarefas.
    int linha = 0, c = out->cursor;

    for (int i = 0; i < out->n_agenda; i++, linha++)
        if (c == linha) {
            out->cursor_agenda = i;
            if (out->agenda[i].indice >= 0) out->alvo = out->agenda[i].indice;
        }

    for (int i = 0; i < out->n_trabalho; i++, linha++)
        if (c == linha) {
            out->cursor_trabalho = i;
            out->alvo          = out->trabalho[i].indice;
            out->alvo_pendente = true;
        }

    // Cabeçalho de grupo não é parada de cursor: é nome, não ação.

    // ── o rodapé ────────────────────────────────────────────────────────
    // Diz o que os botões fazem agora. Travado (gravando, na dock), não promete
    // gesto nenhum.
    if (e->travado) {
        out->rodape_esq[0] = out->rodape_dir[0] = '\0';
        return;
    }

    // Dia vazio: nada para selecionar, e o rodapé não promete o que não existe.
    if (linha == 0) out->cursor = -1;

    // A esquerda é a saída (UI.md). O ▶ abre o item mas não é anunciado: não
    // cabe com a saída, e rótulo que aparece e some ensina errado.
    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK início");

    if (out->cursor < 0) {
        // Em repouso o OK não faz nada, e o rodapé cala à direita.
        return;
    }

    if (out->cursor_trabalho >= 0)
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s",
                 out->trabalho[out->cursor_trabalho].feita ? "OK desmarcar"
                                                           : "OK marcar");
    else if (out->cursor_agenda >= 0)
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s",
                 out->agenda[out->cursor_agenda].caixa
                     ? (out->agenda[out->cursor_agenda].feita ? "OK desmarcar"
                                                              : "OK marcar")
                     : "OK ver");
}

int vista_agenda_linhas(const estado_t *e)
{
    static VISTA_TRANSITORIA vista_agenda_t v;
    vista_agenda(e, &v);
    return v.n_agenda + v.n_trabalho;
}
