#include "nota.h"
#include "campos.h"
#include "../nucleo/data.h"
#include <stdio.h>
#include <string.h>

static const char *nome_do_tipo(tipo_t t)
{
    switch (t) {
    case TIPO_ANOTACAO: return "Anotação";
    case TIPO_TAREFA:   return "Tarefa";
    case TIPO_LISTA:    return "Lista";
    case TIPO_EVENTO:   return "Evento";
    default:            return "Captura";
    }
}

// Rótulo e valor, se o valor existir: rótulo sozinho é pergunta sem
// resposta.
static void campo_f(vista_nota_t *out, const char *rotulo, const char *valor,
                    bool forte)
{
    if (!valor || !*valor) return;
    if (out->n_campos >= (int)(sizeof out->campos / sizeof out->campos[0])) return;

    snprintf(out->campos[out->n_campos].rotulo,
             sizeof out->campos[0].rotulo, "%s", rotulo);
    snprintf(out->campos[out->n_campos].valor,
             sizeof out->campos[0].valor, "%s", valor);
    out->campos[out->n_campos].forte = forte;
    out->n_campos++;
}

static void campo(vista_nota_t *out, const char *rotulo, const char *valor)
{
    campo_f(out, rotulo, valor, false);
}

// Cada tipo aparece com o que ELE é: evento fala de quando e onde, tarefa
// de estado e lista, anotação de quando nasceu.
// "14:00–15:00" sem espaços: nos 162 px depois do rótulo, eles cortavam a
// faixa.
static void faixa_compacta(const char *de, char *out, size_t n)
{
    // Troca " – " por "–", os dois espaços.
    const char *TR = "\u2013";
    size_t k = 0;
    for (size_t i = 0; de[i] && k + 1 < n; ) {
        if (de[i] == ' ' && strncmp(de + i + 1, TR, 3) == 0) {
            if (k + 4 >= n) break;
            memcpy(out + k, TR, 3); k += 3;
            i += 4;
            while (de[i] == ' ') i++;
            continue;
        }
        out[k++] = de[i++];
    }
    out[k] = '\0';
}

// "12 ago · 07:12", sem a duração (que é da gravação).
static const char *criada_em(const item_t *it)
{
    static char s[24];
    if (it->hora[0])
        snprintf(s, sizeof s, "%d %s · %s", it->dia.dia,
                 data_mes_curto(it->dia), it->hora);
    else
        snprintf(s, sizeof s, "%d %s", it->dia.dia, data_mes_curto(it->dia));
    return s;
}

static void monta_campos(const estado_t *e, const item_t *it, vista_nota_t *out)
{
    char quando[48];

    if (it->tipo == TIPO_EVENTO) {
        // "amanhã · 14:00–15:00": o dia relativo responde mais rápido.
        int d = data_dias_entre(e->hoje, it->dia);
        const char *dia = d == 0 ? "hoje" : d == 1 ? "amanhã"
                        : d == -1 ? "ontem" : NULL;

        char fx[24];
        faixa_compacta(out->faixa, fx, sizeof fx);

        if (dia && fx[0])
            snprintf(quando, sizeof quando, "%s · %s", dia, fx);
        else if (dia)
            snprintf(quando, sizeof quando, "%s", dia);
        else if (fx[0])
            snprintf(quando, sizeof quando, "%s %d · %s",
                     data_semana_curta(it->dia), it->dia.dia, fx);
        else
            snprintf(quando, sizeof quando, "%s %d",
                     data_semana_curta(it->dia), it->dia.dia);

        campo_f(out, "QUANDO", quando, true);
        campo(out, "ONDE",   it->local);

        // EM QUAL AGENDA ("cadê isso no meu celular?"); some se o nome não veio.
        campo(out, "AGENDA", it->agenda);

        // Que se repete, e como ("todo dia", "seg, qua, sex").
        char rotina[40];
        vista_rotina_em_palavras(it->regra, rotina, sizeof rotina);
        if (rotina[0])       campo(out, "REPETE", rotina);
        else if (it->repete) campo(out, "REPETE", "este dia, não a série");

        return;
    }

    if (it->tipo == TIPO_TAREFA || it->tipo == TIPO_LISTA) {
        campo_f(out, "ESTADO", it->feita ? "Concluída" : "Pendente", true);

        // A LISTA de destino. Repete o kicker: o kicker se lê de relance, o campo
        // quando a pergunta é onde está.
        campo(out, "LISTA", it->tipo == TIPO_LISTA ? "Nas Tarefas"
                                                  : "Minhas tarefas");

        // ── PRAZO e QUANDO são duas coisas ──────────────────────────────────
        // "Até sexta" é prazo; "academia às 7h" é quando. A HÍBRIDA fica aqui em
        // cima com a faixa inteira, como o evento: está na régua do dia.
        if (it->vence.ano && it->hora[0]) {
            int d = data_dias_entre(e->hoje, it->vence);
            if      (d == 0)  snprintf(quando, sizeof quando, "%s", "hoje");
            else if (d == 1)  snprintf(quando, sizeof quando, "%s", "amanhã");
            else if (d == -1) snprintf(quando, sizeof quando, "%s", "ontem");
            else              snprintf(quando, sizeof quando, "%s %d %s",
                                       data_semana_curta(it->vence), it->vence.dia,
                                       data_mes_curto(it->vence));

            char faixa[24];
            vista_faixa(it, e->config.valor[AJUSTE_HORA24] != 0,
                        faixa, sizeof faixa);
            size_t tem = strlen(quando);
            snprintf(quando + tem, sizeof quando - tem, " · %s", faixa);

            campo(out, "QUANDO", quando);
        }
        // Quando foi concluída, como o Google mostra: é o que diz em que dia a
        // Agenda a põe.
        if (it->feita && it->vence.ano != 0) { /* ordem: vence antes */ }
        if (it->feita && it->feita_em.ano) {
            snprintf(quando, sizeof quando, "%s %d %s",
                     data_semana_curta(it->feita_em), it->feita_em.dia,
                     data_mes_curto(it->feita_em));
            campo_f(out, "CONCLUÍDA", quando, true);
        }

        campo_f(out, "CRIADA", criada_em(it), true);

        // ── o PRAZO DE ENTREGA, abaixo de CRIADA ────────────────────────────
        // O `due` do Tasks. Depois do prazo o texto diz que VENCEU, e quando: uma
        // data solta num item atrasado lê-se como plano.
        if (it->vence.ano && !it->hora[0]) {
            int d = data_dias_entre(e->hoje, it->vence);

            if      (d == 0)  snprintf(quando, sizeof quando, "%s", "vence hoje");
            else if (d == 1)  snprintf(quando, sizeof quando, "%s", "vence amanhã");
            else if (d > 1)   snprintf(quando, sizeof quando, "%s %d %s · em %d dias",
                                       data_semana_curta(it->vence), it->vence.dia,
                                       data_mes_curto(it->vence), d);
            else if (d == -1) snprintf(quando, sizeof quando, "%s", "venceu ontem");
            else              snprintf(quando, sizeof quando, "venceu %s %d %s · há %d dias",
                                       data_semana_curta(it->vence), it->vence.dia,
                                       data_mes_curto(it->vence), -d);

            // O INTERVALO ("de 14 a 17") vem junto, sem campo próprio: a tela cabe
            // seis.
            if (it->prazo.ano) {
                size_t tem = strlen(quando);
                snprintf(quando + tem, sizeof quando - tem, " · até %d %s",
                         it->prazo.dia, data_mes_curto(it->prazo));
            }

            campo_f(out, "PRAZO", quando, d <= 0);
        }
        return;
    }

    // Anotação: quando nasceu, e mais nada. As datas em destaque.
    campo_f(out, "CRIADA", criada_em(it), true);
}

void vista_nota(const estado_t *e, vista_nota_t *out)
{
    memset(out, 0, sizeof *out);
    const item_t *it = &e->aberto;

    snprintf(out->titulo, sizeof out->titulo, "%s", "Nota");
    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    out->bateria = e->bateria;
    out->wifi = vista_wifi_da_barra(e);
    out->sinc = vista_sinc_da_barra(e);

    // RN-B8: título vazio é normal antes de processar (`vista_titulo_do_item`).
    vista_titulo_do_item(e, it, out->tl, sizeof out->tl);


    // A barra nomeia o tipo do item ("Nota" era nome interno).
    snprintf(out->titulo, sizeof out->titulo, "%s", nome_do_tipo(it->tipo));

    // ── de onde ISTO veio ───────────────────────────────────────────────
    // O `nota` diz se há gravação, e sobrevive ao sync (o pull o preserva).
    out->tem_fala = it->nota[0] != '\0';

    // "Criada" na anotação, "Criado" no resto. Anotação nunca vem do Google.
    // Duas informações: `nota` (há gravação?) e `origem` (nasceu aqui?). Só a
    // primeira chamava de "do Google" um item da agenda do Tinto.
    snprintf(out->origem, sizeof out->origem, "%s",
             it->tipo == TIPO_ANOTACAO   ? "Criada por voz no Tinto"
           : out->tem_fala               ? "Criado por voz no Tinto"
           : it->origem == ORIGEM_GOOGLE ? "Adicionado no Google"
                                         : "Criado no Tinto");

    out->origem_google = !out->tem_fala && it->tipo != TIPO_ANOTACAO &&
                         it->origem == ORIGEM_GOOGLE;

    // O destino em linha própria (RN-4D): colado no tipo passava
    // despercebido.
    vista_destino(it, out->onde, sizeof out->onde);

    // ── o kicker: o que é, e onde está ───────────────────────────────────
    // O destino já diz o tipo; repetir daria "Evento · EVENTO".
    snprintf(out->kicker, sizeof out->kicker, "%s", out->onde);

    // Antes de montar os campos: `monta_campos` lê `faixa`, `local` e
    // `quando`. Faixa e local só existem em evento.
    if (it->tipo == TIPO_EVENTO) {
        vista_faixa(it, e->config.valor[AJUSTE_HORA24] != 0, out->faixa, sizeof out->faixa);
        snprintf(out->local, sizeof out->local, "%s", it->local);
    }

    char dia[16];
    snprintf(dia, sizeof dia, "%d %s", it->dia.dia, data_mes_curto(it->dia));

    char hq[HORA_TEXTO];
    vista_hora_do_item(e, it->hora, hq, sizeof hq);

    if (it->tipo == TIPO_EVENTO || !it->hora[0])
        snprintf(out->quando, sizeof out->quando, "%s", dia);
    else if (it->dur_s > 0)
        snprintf(out->quando, sizeof out->quando, "%s · %s · %d:%02d",
                 dia, hq, it->dur_s / 60, it->dur_s % 60);
    else
        snprintf(out->quando, sizeof out->quando, "%s · %s", dia, hq);

    // ── os campos, e eles dependem do TIPO ───────────────────────────
    monta_campos(e, it, out);


    // O `quando` fala da GRAVAÇÃO; num evento repetiria a faixa.
    out->tem_resumo = e->texto.tem_resumo;
    snprintf(out->resumo, sizeof out->resumo, "%s", e->texto.resumo);
    snprintf(out->transcricao, sizeof out->transcricao, "%s",
             e->texto.transcricao);

    // ── tudo o que esta fala criou ──────────────────────────────────────
    // Os itens da mesma gravação apontam todos para a mesma NOTA.
    if (it->nota[0]) {
        for (int i = 0; i < e->n_itens && out->n_criou < NOTA_CRIOU_MAX; i++) {
            const item_t *o = &e->itens[i];
            if (strcmp(o->nota, it->nota) != 0) continue;

            snprintf(out->criou[out->n_criou].oque,
                     sizeof out->criou[0].oque, "%s · %s",
                     o->tipo == TIPO_EVENTO   ? "Evento"
                   : o->tipo == TIPO_TAREFA   ? "Tarefa"
                   : o->tipo == TIPO_LISTA    ? "Lista"
                   : o->tipo == TIPO_ANOTACAO ? "Anotação"
                                              : "Captura",
                     o->titulo);

            // O estado CONCRETO de cada um: sucesso genérico esconde o que falhou.
            snprintf(out->criou[out->n_criou].estado,
                     sizeof out->criou[0].estado, "%s",
                     o->tipo == TIPO_TAREFA || o->tipo == TIPO_LISTA
                         ? (o->feita ? "concluída" : "pendente")
                         : "marcado");
            out->n_criou++;
        }
    }

    // RN-A1. Só para quem NASCEU aqui (o do Google nunca teve fala) e nunca na
    // anotação, que é a própria transcrição.
    if (!out->tem_resumo && it->origem == ORIGEM_AQUI &&
        it->tipo != TIPO_ANOTACAO)
        snprintf(out->aviso, sizeof out->aviso, "%s",
                 out->transcricao[0] ? "por estruturar"
                                     : "por transcrever · sem rede");

    // Não há "Ouvir": o WAV é apagado depois de confirmado; a prova do que foi
    // dito é a transcrição, na tela.
    // ── o que se pode fazer DEPENDE do que a coisa é ───────────────────
    // O rótulo diz o estado: "Pôr data" sem data, "Mudar a data" com.
    struct { icone_id ico; const char *txt; } A[4];
    int n_a = 0;

    A[n_a].ico = ICO_RENOMEAR; A[n_a].txt = "Renomear"; n_a++;

    // Adiar vem antes de Apagar: faz-se cem vezes mais.
    if (it->tipo == TIPO_EVENTO) {
        A[n_a].ico = ICO_EVENTO;  A[n_a].txt = "Mudar a data"; n_a++;
        A[n_a].ico = ICO_RELOGIO; A[n_a].txt = "Mudar a hora"; n_a++;
    } else if (it->tipo == TIPO_TAREFA) {
        // "Mudar a data", não "prazo": prazo é outra coisa.
        A[n_a].ico = ICO_EVENTO;
        A[n_a].txt = it->vence.ano ? "Mudar a data" : "Pôr data";
        n_a++;

        // ── mudar a hora, só de quem JÁ TEM hora ────────────────────────────
        // A hora da tarefa mora no evento gêmeo do Google Agenda, que nasce lá
        // quando a pessoa marca o horário no celular. Daqui não se cria sem
        // inventar um segundo compromisso. Quem já tem hora pode mudá-la.
        if (it->hora[0]) {
            A[n_a].ico = ICO_RELOGIO;
            A[n_a].txt = "Mudar a hora";
            n_a++;
        }
    }

    A[n_a].ico = ICO_LIXO; A[n_a].txt = "Apagar"; n_a++;
    // TODAS as ações, para todo item: o escopo `calendar` alcança qualquer
    // evento, como já alcançava qualquer tarefa.
    for (int i = 0; i < n_a; i++) {
        if (A[i].ico == ICO_MIC && it->dur_s == 0) continue;
        out->acoes[out->n_acoes].icone = A[i].ico;
        snprintf(out->acoes[out->n_acoes].texto,
                 sizeof out->acoes[0].texto, "%s", A[i].txt);
        out->n_acoes++;
    }

    if (out->n_acoes == 0)
        snprintf(out->sem_acoes, sizeof out->sem_acoes, "%s",
                 "Só leitura: é da sua agenda");

    // ── os botões da tela, na ordem do desenho ─────────────────────────
    if (it->tipo == TIPO_TAREFA || it->tipo == TIPO_LISTA) {
        // Concluída, o botão vira DESMARCAR: senão marcar sem querer só se
        // desfazia no celular.
        snprintf(out->botoes[out->n_botoes].texto,
                 sizeof out->botoes[0].texto, "%s",
                 it->feita ? "Desmarcar" : "Marcar como concluída");
        out->botoes[out->n_botoes].principal = true;
        out->botoes[out->n_botoes].faz = it->feita ? BOTAO_REABRIR
                                                   : BOTAO_CONCLUIR;
        out->n_botoes++;
    }
    // A transcrição desce no próprio corpo, que já rola (era uma tela à parte).

    out->cursor_bruto = e->cursor;
    out->botao_foco   = e->cursor;
    if (out->botao_foco < 0)              out->botao_foco = 0;
    if (out->botao_foco >= out->n_botoes) out->botao_foco = out->n_botoes - 1;

    out->acoes_abertas = e->overlay == OVERLAY_ACOES;
    out->cursor_acao   = e->cursor_overlay;

    // O rodapé NOMEIA para onde o BACK volta.
    snprintf(out->rodape_esq, sizeof out->rodape_esq, "BACK %s",
             out->acoes_abertas          ? "fechar"
           : it->tipo == TIPO_ANOTACAO   ? "anotações"
                                         : "agenda");
    // O OK abre a fala quando ela existe; sem fala, não promete nada. Diz o que
    // o OK faz na linha em foco.
    const char *ok = "";
    if (out->acoes_abertas)          ok = "OK escolher";
    else if (out->n_botoes > 0)
        switch (out->botoes[out->botao_foco].faz) {
        case BOTAO_CONCLUIR: ok = "OK concluir";   break;
        case BOTAO_REABRIR:  ok = "OK reabrir";    break;
        default:             ok = "OK abrir fala"; break;
        }
    // Sem botão, o rodapé direito fica VAZIO; o ▶ continua abrindo a gaveta.
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", ok);
}
