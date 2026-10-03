#include "uso.h"
#include "../nucleo/rotina.h"
#include "../dado/indice.h"
#include "nuvem.h"
#include "../dado/cartao.h"
#include "../dado/log.h"
#include "../nucleo/data.h"
#include <stdio.h>
#include <string.h>

// O índice é montado no boot; quem chega sem ele (teste que não liga o
// aparelho, cartão que voltou) monta agora, uma vez.
static erro_t garante_indice(const hal_t *hal)
{
    return indice_pronto() ? OK : indice_monta(hal);
}

// ── gestos manuais funcionam offline; voz exige rede ─────────────────
// O que se faz com a mão acontece e espera no cartão até a rede voltar. A
// recusa fica para o que depende da rede para acontecer: a fala precisa de
// Whisper e LLM.
static bool exige_rede(estado_t *e, const char *gesto)
{
    if (e->rede == REDE_LIGADA) return true;
    snprintf(e->precisa_rede, sizeof e->precisa_rede, "%s", gesto);
    return false;
}

// A ordem das três metades é sempre esta:
//
//   1. grava no cartão   — se falhar, NADA aconteceu e a função sai
//   2. enfileira         — só depois que o cartão confirmou
//   3. invalida o cache  — o que está na tela deixou de valer
//
// Enfileirar antes subiria um estado que o aparelho não tem; invalidar antes
// redesenharia lendo o cartão velho.

// ── o item do gesto, e ONDE ele mora ─────────────────────────────────
// O cartão é a fonte de tudo menos do dia CONSULTADO, que veio do servidor
// e vive na RAM — sem isto os gestos sobre ele saíam calados. `no_cartao`
// diz de onde veio: gravar um dia distante no cartão inflaria a janela.
static erro_t le_do_gesto(const hal_t *hal, const estado_t *e, data_t dia,
                          const char *id, item_t *out, bool *no_cartao)
{
    *no_cartao = cartao_le_item(hal, dia, id, out) == OK;
    if (*no_cartao) return OK;

    if (e->dia_consultado.ano && data_igual(e->dia_consultado, dia)) {
        for (int i = 0; i < e->n_itens; i++)
            if (strcmp(e->itens[i].id, id) == 0) {
                *out = e->itens[i];
                return OK;
            }
    }
    return ERR_ARQUIVO;
}

// E o gesto volta para onde o item mora: o cartão, ou o cache da consulta
// (otimista por um ciclo; `uso_reconsultar_o_dia` pede a verdade depois).
static erro_t grava_do_gesto(const hal_t *hal, estado_t *e, bool no_cartao,
                             data_t dia, const item_t *novo)
{
    if (no_cartao) return cartao_grava_item(hal, dia, novo);

    for (int i = 0; i < e->n_itens; i++)
        if (strcmp(e->itens[i].id, novo->id) == 0) {
            e->itens[i] = *novo;
            break;
        }
    return OK;
}

erro_t uso_marcar(const hal_t *hal, estado_t *e, const item_t *it, bool feita)
{
    // Copia antes: `it` costuma apontar para o cache, que o passo 3 mexe.
    data_t dia = it->dia;
    char   id[sizeof it->id];
    snprintf(id, sizeof id, "%s", it->id);

    item_t atual;
    bool   no_cartao = false;
    erro_t err = le_do_gesto(hal, e, dia, id, &atual, &no_cartao);
    if (err != OK) return err;

    atual.feita = feita;

    // Quando foi feita: a Agenda e o calendário mostram a concluída no dia em
    // que foi FEITA. Carimbo local otimista; o pull traz o `completed` do Google
    // por cima. Desmarcar limpa.
    if (feita) atual.feita_em = e->hoje;
    else       memset(&atual.feita_em, 0, sizeof atual.feita_em);

    err = grava_do_gesto(hal, e, no_cartao, dia, &atual);
    if (err != OK) return err;


    // E sobe: o modelo é o do Google (`title` do Tasks, `summary` do Calendar).
    // Anotação não sobe aqui: feito e data dela são só do aparelho.
    if (atual.tipo != TIPO_ANOTACAO)
        (void)uso_enviar_gesto(hal, e, &atual, "editou");

    estado_invalida_cartao(e);
    e->marcas_validas = false;
    return OK;
}

erro_t uso_renomear(const hal_t *hal, estado_t *e, const item_t *it,
                    const char *titulo)
{
    data_t dia = it->dia;
    char   id[sizeof it->id];
    snprintf(id, sizeof id, "%s", it->id);

    item_t atual;
    bool   no_cartao = false;
    erro_t err = le_do_gesto(hal, e, dia, id, &atual, &no_cartao);
    if (err != OK) return err;

    // RN-B7: trunca, não recusa.
    snprintf(atual.titulo, sizeof atual.titulo, "%s", titulo ? titulo : "");

    // RN-24: o que a pessoa escreveu à mão, a IA não sobrescreve.
    atual.titulo_manual = true;

    err = grava_do_gesto(hal, e, no_cartao, dia, &atual);
    if (err != OK) return err;

    // E sobe. Anotação também: o servidor a guarda sem tocar o Google (RN-25).
    (void)uso_enviar_gesto(hal, e, &atual, "editou");

    estado_invalida_cartao(e);
    e->marcas_validas = false;
    return OK;
}

erro_t uso_apagar_item(const hal_t *hal, estado_t *e, const item_t *it)
{
    data_t dia = it->dia;
    char   id[sizeof it->id];
    snprintf(id, sizeof id, "%s", it->id);

    // Lido ANTES de sumir: o gesto leva o tipo, e o backend precisa dele para
    // saber se apaga no Calendar ou no Tasks.
    item_t indo = *it;

    erro_t err = cartao_apaga_item(hal, dia, id);
    if (err != OK) return err;

    // E some LÁ também, senão volta no próximo pull. Anotação também, entre o
    // aparelho e o nosso servidor (RN-25).
    (void)uso_enviar_gesto(hal, e, &indo, "apagou");

    estado_invalida_cartao(e);
    e->marcas_validas = false;
    return OK;
}

erro_t uso_mudar_data(const hal_t *hal, estado_t *e, const item_t *it,
                      data_t nova, const char *hora)
{
    data_t dia = it->dia;
    char   id[sizeof it->id];
    snprintf(id, sizeof id, "%s", it->id);

    item_t atual;
    bool   no_cartao = false;
    erro_t err = le_do_gesto(hal, e, dia, id, &atual, &no_cartao);
    if (err != OK) return err;

    // O tipo ANTES vem do cartão, nunca do cache.
    tipo_t tipo_antes = atual.tipo;

    // RN-26: muda o VENCIMENTO, nunca a pasta.
    atual.vence = nova;

    // O fim anda junto, com a mesma duração (a conta que o servidor faz no
    // Google). Sem fim continua sem.
    if (atual.fim[0]) {
        int de = data_minutos(atual.hora), ate = data_minutos(atual.fim);
        int novo = data_minutos(hora);
        int dura = de >= 0 && ate > de ? ate - de : 60;
        if (novo >= 0) {
            int f = (novo + dura) % (24 * 60);
            snprintf(atual.fim, sizeof atual.fim, "%02d:%02d", f / 60, f % 60);
        } else {
            atual.fim[0] = '\0';
        }
    }
    snprintf(atual.hora, sizeof atual.hora, "%s", hora ? hora : "");

    // Tarefa que ganha hora continua TAREFA: um híbrido, na régua do dia e
    // marcável como feita. Convertê-la em evento criava duas coisas brigando
    // pelo mesmo compromisso. A hora vive no cartão (o Tasks não a guarda), e o
    // pull a preserva.
    (void)tipo_antes;

    err = grava_do_gesto(hal, e, no_cartao, dia, &atual);
    if (err != OK) return err;

    // E sobe. Anotação não: feito e data dela são só do aparelho.
    if (atual.tipo != TIPO_ANOTACAO)
        (void)uso_enviar_gesto(hal, e, &atual, "editou");

    estado_invalida_cartao(e);
    e->marcas_validas = false;
    return OK;
}

void uso_ir_para_dia(estado_t *e, data_t dia)
{
    e->dia_visto = dia;

    // A tira do calendário precisa saber disto antes de qualquer leitura:
    // andar o cursor não monta o cache do dia.
    e->dia_fora_da_janela =
        uso_dia_fora_da_janela(e) &&
        !(e->dia_consultado.ano && data_igual(e->dia_consultado, dia));

    // O pedido de OUTRO dia morre aqui: a resposta seria descartada e o pedido
    // reenviado para sempre.
    if (e->dia_pedido[0] && !data_igual(e->dia_pedido_data, dia))
        e->dia_pedido[0] = '\0';

    // Só o DIA cai: as pendentes são de um intervalo em torno de hoje, e as
    // marcas se refazem quando o mês vira. Invalidar tudo custava meio segundo
    // de cartão por seta.
    estado_invalida_dia(e);
}

// A mais nova primeiro (dia, hora, id para desempatar). Positivo quando `a`
// vem antes.
static int antes_na_lista(const item_t *a, const item_t *b)
{
    int d = data_compara(a->dia, b->dia);
    if (d) return d;
    d = strcmp(a->hora, b->hora);
    if (d) return d;
    return strcmp(a->id, b->id);
}

erro_t uso_carregar_anotacoes(const hal_t *hal, estado_t *e)
{
    e->n_anotacoes = 0;
    e->anotacoes_total = 0;
    e->anotacoes_validas = true;

    erro_t err = garante_indice(hal);
    if (err != OK) return err;

    // Do índice, sem janela: só a PÁGINA aberta vem para o estado; contar as
    // outras dá o "2 / 5".
    for (int i = 0; i < indice_n(); i++) {
        const item_t *it = indice_em(i);
        if (it && it->tipo == TIPO_ANOTACAO) e->anotacoes_total++;
    }

    int paginas = (e->anotacoes_total + ANOTACOES_MAX - 1) / ANOTACOES_MAX;
    if (e->anotacoes_pagina >= paginas) e->anotacoes_pagina = (int16_t)(paginas ? paginas - 1 : 0);
    if (e->anotacoes_pagina < 0) e->anotacoes_pagina = 0;
    int pula = e->anotacoes_pagina * ANOTACOES_MAX;

    // O índice não está em ordem de tempo: a página sai selecionando as N mais
    // novas depois das que ficam nas páginas anteriores.
    const item_t *ultimo = NULL;
    for (int pos = 0; pos < pula + ANOTACOES_MAX; pos++) {
        const item_t *melhor = NULL;
        for (int i = 0; i < indice_n(); i++) {
            const item_t *it = indice_em(i);
            if (!it || it->tipo != TIPO_ANOTACAO) continue;
            if (ultimo && antes_na_lista(it, ultimo) >= 0) continue;
            if (!melhor || antes_na_lista(it, melhor) > 0) melhor = it;
        }
        if (!melhor) break;
        ultimo = melhor;
        if (pos >= pula) e->anotacoes[e->n_anotacoes++] = *melhor;
    }

    if (e->cursor >= e->n_anotacoes) e->cursor = (int16_t)(e->n_anotacoes ? e->n_anotacoes - 1 : 0);
    return OK;
}


erro_t uso_carregar_dia(const hal_t *hal, estado_t *e)
{
    // Só o DIA (ver `uso_ir_para_dia`): quem muda o cartão é quem escreve
    // nele. A bandeira cai aqui e volta no fim: falhar no meio não deixa meio
    // cache marcado como bom.
    estado_invalida_dia(e);
    e->n_fora = 0;

    // O ÍNDICE responde, não o cartão.
    erro_t err = garante_indice(hal);
    if (err != OK) return err;

    // A consulta de um dia distante manda no cache enquanto for daquele dia:
    // refazer do índice apagaria o que o servidor mandou.
    bool consultado = e->dia_consultado.ano &&
                      data_igual(e->dia_consultado, e->dia_visto);

    if (!consultado)
        e->n_itens = (int16_t)indice_do_dia(e->dia_visto, e->itens, ITENS_MAX);

    // ── um dia FORA da janela ────────────────────────────────────────────
    // Abrir o 27 é consulta: o que volta entra aqui, sem passar pelo cartão. Sem
    // rede a tela diz isso em vez de afirmar um dia vazio.
    int fora_da_janela =
        data_compara(e->dia_visto, data_soma_dias(e->hoje, -1)) < 0 ||
        data_compara(e->dia_visto, data_soma_dias(e->hoje,  1)) > 0;

    e->dia_fora_da_janela = fora_da_janela && !consultado;
    (void)fora_da_janela;

    // Não se pede aqui: andar o cursor recarrega o dia a cada seta. Quem pede
    // é o OK (`uso_pedir_o_dia`).
    if (!fora_da_janela) e->dia_pedido[0] = '\0';

    // A consulta de OUTRO dia não vale mais.
    if (!consultado) e->dia_consultado = (data_t){ 0, 0, 0 };

    // RN-34: tarefa aberta aparece até ser feita, venha do dia que vier. As
    // atrasadas saem das pendentes (um filtro na RAM), não de outra varredura.
    // Se as pendentes ainda não rodaram, rodam aqui, uma vez. Vale para qualquer
    // dia: é delas também que sai o evento que começou antes.
    if (!e->pendentes_validas) {
        erro_t p = uso_carregar_pendentes(hal, e);
        if (p != OK) return p;
    }

    // ── o que começou antes e ainda não acabou ───────────────────────────
    // A viagem de 14 a 17 mora na pasta do 14; o 15 precisa mostrá-la.
    // ── a ROTINA, no dia em que ela cai ─────────────────────────────────
    // Uma linha no cartão; `nucleo/rotina.h` responde se o dia é dela.
    for (int i = 0; i < e->n_pendentes; i++) {
        const item_t *r = &e->pendentes[i];
        if (!r->regra[0]) continue;
        if (data_igual(r->vence, e->dia_visto)) continue;   // já veio da pasta
        if (!rotina_no_dia(r->regra, r->vence, e->dia_visto)) continue;

        if (e->n_itens < ITENS_MAX) {
            item_t hoje = *r;
            // O item ganha o dia em que é mostrado: contagem e detalhe usam `dia`, e
            // a âncora diria "há 12 dias" na ocorrência de hoje.
            hoje.dia = e->dia_visto;
            hoje.vence = e->dia_visto;
            e->itens[e->n_itens++] = hoje;
        } else {
            e->n_fora++;
        }
    }

    for (int i = 0; i < e->n_pendentes; i++) {
        const item_t *v = &e->pendentes[i];
        if (v->tipo != TIPO_EVENTO || !v->prazo.ano) continue;
        // O primeiro dia já veio da pasta.
        if (data_igual(v->vence, e->dia_visto)) continue;
        if (data_compara(v->vence, e->dia_visto) > 0 ||
            data_compara(e->dia_visto, v->prazo) > 0) continue;

        if (e->n_itens < ITENS_MAX) e->itens[e->n_itens++] = *v;
        else                        e->n_fora++;
    }

    if (data_igual(e->dia_visto, e->hoje)) {
        for (int i = 0; i < e->n_pendentes; i++) {
            const item_t *t = &e->pendentes[i];
            if (t->feita) continue;
            if (data_igual(t->dia, e->hoje)) continue;
            if (t->tipo != TIPO_TAREFA && t->tipo != TIPO_LISTA) continue;

            if (e->n_itens < ITENS_MAX) e->itens[e->n_itens++] = *t;
            else                        e->n_fora++;
        }

        // O que as pendentes deixaram de fora conta também: o "+ N" soma as duas
        // perdas.
        e->n_fora += e->n_pendentes_fora;
    }

    e->itens_validos = true;
    return OK;
}

erro_t uso_abrir_item(const hal_t *hal, estado_t *e, const item_t *it)
{
    data_t dia = it->dia;
    char   id[sizeof it->id];
    snprintf(id, sizeof id, "%s", it->id);

    // Relê do cartão: o cache pode estar velho, e abrir é olhar de perto.
    bool   no_cartao = false;
    erro_t err = le_do_gesto(hal, e, dia, id, &e->aberto, &no_cartao);
    if (err != OK) return err;

    // O dia CONSULTADO não tem texto nem transcrição. Zerar é obrigatório: sem
    // isso o detalhe abria com o texto do último item aberto.
    if (!no_cartao) {
        memset(&e->texto, 0, sizeof e->texto);
        e->aberto_valido = true;
        return OK;
    }

    (void)cartao_le_texto(hal, dia, id, &e->texto);

    // A transcrição vem da NOTA, não da pasta do item: uma fala que virou duas
    // ações tem uma nota só. Sem nota, o item veio do Google.
    // Só sobrescreve se a nota tiver algo: nota vazia não apaga o `texto.txt`.
    if (e->aberto.nota[0]) {
        char da_nota[sizeof e->texto.transcricao];
        da_nota[0] = '\0';
        (void)cartao_le_transcricao(hal, e->aberto.nota,
                                    da_nota, sizeof da_nota);
        if (da_nota[0])
            snprintf(e->texto.transcricao, sizeof e->texto.transcricao,
                     "%s", da_nota);
    }

    e->aberto_valido = true;
    return OK;
}

// O intervalo de cada ajuste, num lugar só.
static int apara(ajuste_t chave, int v)
{
    switch (chave) {
    case AJUSTE_BLOQUEAR_MIN:  return v < 0 ? 0 : v > 60 ? 60 : v;   // 0 = nunca
    case AJUSTE_HORA24:
    case AJUSTE_VOZ_SEGURAR: return v ? 1 : 0;
    case AJUSTE_HORA_REDE:   return v ? 1 : 0;

    // Fusos vão de -12:00 a +14:00. RN-B7: aparar, não recusar.
    case AJUSTE_FUSO_MIN:
        if (v < -720) return -720;
        if (v >  840) return  840;
        return v;
    default:                 return v;
    }
}

erro_t uso_salvar_ajuste(const hal_t *hal, estado_t *e,
                         ajuste_t chave, int valor)
{
    if (chave < 0 || chave >= AJUSTE_QUANTOS) return ERR_INTERNO;

    config_t novo = e->config;
    novo.valor[chave] = (int16_t)apara(chave, valor);

    // Nada mudou: não escreve (segurar uma tecla mandaria uma escrita por
    // repetição).
    if (novo.valor[chave] == e->config.valor[chave]) return OK;

    erro_t err = cartao_grava_config(hal, &novo);
    if (err != OK) return err;

    e->config = novo;

    // O fuso vale NA HORA, não no próximo boot.
    if (chave == AJUSTE_FUSO_MIN && hal->fuso)
        hal->fuso(novo.valor[chave]);

    // Ajuste não sobe nem invalida itens.
    return OK;
}

erro_t uso_carregar_config(const hal_t *hal, estado_t *e)
{
    return cartao_le_config(hal, &e->config);
}

// ── a fala ───────────────────────────────────────────────────────────
// O id é a HORA em que ela começou ("1422-fala"): ordena, é legível e é
// único no dia. Durante a gravação vale o id guardado no começo; sem
// gravação, o do relógio.
static void id_da_fala(const estado_t *e, char *out, size_t max)
{
    if (e->gravacao.id[0]) { snprintf(out, max, "%s", e->gravacao.id); return; }
    snprintf(out, max, "%02d%02d-fala", e->hora, e->minuto);
}

static void caminho_wav(const estado_t *e, const char *id,
                        char *out, size_t max)
{
    cartao_caminho_wav(e->hoje, id, out, max);
}

// O hal é um contrato de ponteiros: buraco só aparece em execução (o ● sem
// áudio no hal_esp reiniciava a placa). Confere todos de uma vez: meia
// gravação é pior que nenhuma.
static bool tem_microfone(const hal_t *hal)
{
    return hal && hal->audio_inicia && hal->audio_pausa && hal->audio_retoma
        && hal->audio_fecha && hal->audio_descarta;
}

erro_t uso_gravar_comeca(const hal_t *hal, estado_t *e)
{
    if (!tem_microfone(hal)) return ERR_INTERNO;

    // Sem rede não se grava: áudio que não pode ser transcrito não é nota.
    // Recusar antes, quando a pessoa ainda não falou nada.
    if (!exige_rede(e, "falar")) return ERR_REDE;
    if (e->gravacao.fase != GRAV_PARADA) return ERR_INTERNO;

    char id[40], wav[128];
    id_da_fala(e, id, sizeof id);
    caminho_wav(e, id, wav, sizeof wav);

    erro_t err = hal->audio_inicia(wav);
    if (err != OK) return err;

    // O id fica guardado: daqui em diante é ele, não o relógio.
    snprintf(e->gravacao.id, sizeof e->gravacao.id, "%s", id);
    e->gravacao.fase    = GRAV_GRAVANDO;
    e->gravacao.ms      = 0;
    e->gravacao.trechos = 1;
    memset(e->gravacao.trecho_s, 0, sizeof e->gravacao.trecho_s);

    // RN-14: gravando, o direcional trava e o aparelho não dorme.
    e->travado = true;
    return OK;
}

erro_t uso_gravar_pausa(const hal_t *hal, estado_t *e)
{
    if (!tem_microfone(hal)) return ERR_INTERNO;
    if (e->gravacao.fase != GRAV_GRAVANDO) return ERR_INTERNO;
    erro_t err = hal->audio_pausa();
    if (err != OK) return err;
    e->gravacao.fase = GRAV_PAUSADA;

    // Pausar não fala com o servidor: transcrever por pausa cobrava o mesmo
    // áudio várias vezes da quota. "Entendeu o quê?" é o Conferir que responde.
    return OK;
}

erro_t uso_gravar_retoma(const hal_t *hal, estado_t *e)
{
    if (!tem_microfone(hal)) return ERR_INTERNO;
    if (e->gravacao.fase != GRAV_PAUSADA) return ERR_INTERNO;

    // RN-13: retomar entra no MESMO arquivo; um segundo `inicia` faria duas
    // notas.
    erro_t err = hal->audio_retoma();
    if (err != OK) return err;
    e->gravacao.fase = GRAV_GRAVANDO;
    if (e->gravacao.trechos < GRAV_MAX_TRECHOS) e->gravacao.trechos++;
    return OK;
}

erro_t uso_salvar_captura(const hal_t *hal, estado_t *e)
{
    if (!tem_microfone(hal)) return ERR_INTERNO;
    if (e->gravacao.fase == GRAV_PARADA) return ERR_INTERNO;

    int dur = 0, trechos = 0;
    erro_t err = hal->audio_fecha(&dur, &trechos);
    if (err != OK) return err;

    char id[40];
    id_da_fala(e, id, sizeof id);

    // RN-11: o item nasce SEM TIPO (TIPO_NADA é legítimo); quem classifica é a
    // IA depois.
    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,   sizeof it.id,   "%s", id);
    snprintf(it.hora, sizeof it.hora, "%02d:%02d", e->hora, e->minuto);
    it.tipo   = TIPO_NADA;
    it.origem = ORIGEM_AQUI;
    it.dia    = e->hoje;
    it.dur_s  = (int16_t)dur;

    err = cartao_grava_item(hal, e->hoje, &it);
    if (err != OK) return err;

    // A captura não entra na fila: sobe agora, pelo POST /v1/captura, ou não
    // acontece.

    // RN-95: a duração entra no log, o conteúdo NUNCA.
    (void)dado_log(hal, LOG_GRAVOU, dur, e->hora, e->minuto);

    // Guarda o resultado para o recibo.
    e->ultimo         = it;
    e->ultimo_trechos = (int8_t)trechos;

    memset(&e->gravacao, 0, sizeof e->gravacao);
    e->travado       = false;
    estado_invalida_cartao(e);
    e->marcas_validas = false;

    // E a fala SOBE. Sem rede não tenta: só chega aqui sem conexão se ela cair
    // no meio.
    if (e->rede == REDE_LIGADA) {
        char wav[128];
        caminho_wav(e, it.id, wav, sizeof wav);
        (void)uso_enviar_captura(hal, e, wav);
    }

    return OK;
}

erro_t uso_descartar_fala_nao_enviada(const hal_t *hal, estado_t *e)
{
    if (!hal || !e) return ERR_INTERNO;
    if (!e->ultimo.id[0]) return ERR_INTERNO;

    char wav[128];
    caminho_wav(e, e->ultimo.id, wav, sizeof wav);
    (void)hal->apagar(wav);
    (void)cartao_apaga_item(hal, e->ultimo.dia, e->ultimo.id);

    // Não enfileira: o servidor nunca soube desta fala.
    (void)dado_log(hal, LOG_DESCARTOU, 0, e->hora, e->minuto);

    memset(&e->ultimo, 0, sizeof e->ultimo);
    e->ultimo_trechos = 0;

    estado_invalida_cartao(e);
    e->marcas_validas = false;
    return OK;
}

erro_t uso_descartar_captura(const hal_t *hal, estado_t *e)
{
    if (!tem_microfone(hal)) return ERR_INTERNO;
    if (e->gravacao.fase == GRAV_PARADA) return ERR_INTERNO;

    (void)hal->audio_descarta();

    // O dia de hoje é o certo: a gravação foi aberta com o `e->hoje` de agora.
    char id[40], wav[128];
    id_da_fala(e, id, sizeof id);
    caminho_wav(e, id, wav, sizeof wav);
    (void)hal->apagar(wav);
    (void)cartao_apaga_item(hal, e->hoje, id);

    // Não enfileira: o servidor nunca viu este item.
    (void)dado_log(hal, LOG_DESCARTOU, 0, e->hora, e->minuto);

    memset(&e->gravacao, 0, sizeof e->gravacao);
    e->travado = false;

    // Descartar apaga um item do cartão: o cache precisa saber.
    estado_invalida_cartao(e);
    e->marcas_validas = false;
    return OK;
}

// ── as marcas do mês ─────────────────────────────────────────────────
// A grade é a união do que o aparelho TEM (a janela, do índice, inclui a
// rotina; vale sem rede) com o que o SERVIDOR disse do mês (um bit por dia).
// Não toca o cartão.
//
// Este dia está fora da janela? Aritmética pura: não depende de o cache do
// dia ter sido montado, senão o OK do calendário nunca pedia o dia.
bool uso_dia_fora_da_janela(const estado_t *e)
{
    if (!e) return false;
    return data_compara(e->dia_visto, data_soma_dias(e->hoje, -1)) < 0 ||
           data_compara(e->dia_visto, data_soma_dias(e->hoje,  1)) > 0;
}

void uso_pedir_o_dia(estado_t *e)
{
    if (!e || !uso_dia_fora_da_janela(e)) return;

    // Já consultado: é o dia que está na tela.
    if (e->dia_consultado.ano && data_igual(e->dia_consultado, e->dia_visto))
        return;

    snprintf(e->dia_pedido, sizeof e->dia_pedido, "%04d-%02d-%02d",
             e->dia_visto.ano, e->dia_visto.mes, e->dia_visto.dia);
    e->dia_pedido_data = e->dia_visto;

    // Sai no PRÓXIMO tick: o relógio espaça reenvios, não atrasa pedido novo.
    e->olhar_ms = 0;
}

// A consulta envelheceu (um gesto mudou o Google). Repede sem apagar o que
// está na tela.
void uso_reconsultar_o_dia(estado_t *e)
{
    if (!e || !e->dia_consultado.ano) return;
    if (!data_igual(e->dia_consultado, e->dia_visto)) return;

    snprintf(e->dia_pedido, sizeof e->dia_pedido, "%04d-%02d-%02d",
             e->dia_visto.ano, e->dia_visto.mes, e->dia_visto.dia);
    e->dia_pedido_data = e->dia_visto;
    e->olhar_ms = 0;
}

erro_t uso_poda_a_janela(const hal_t *hal, estado_t *e)
{
    if (!hal || !e) return ERR_INTERNO;

    // Só com hora de verdade: no boot o relógio nasce zerado, e podar com esse
    // "hoje" apagava todo evento do Google sem ele voltar.
    if (!e->hora_confiavel || e->hoje.ano < 2025) return OK;

    erro_t err = garante_indice(hal);
    if (err != OK) return err;

    data_t de  = data_soma_dias(e->hoje, -1);
    data_t ate = data_soma_dias(e->hoje,  1);

    int podados = 0;
    // De trás para a frente: `indice_tira` troca o último com o buraco.
    for (int i = indice_n() - 1; i >= 0; i--) {
        const item_t *it = indice_em(i);
        if (!it) continue;

        // Só o que o Google devolve. `n:` é proposta em voo, e anotação não tem de
        // onde voltar.
        bool de_fora = it->id[1] == ':' &&
                       (it->id[0] == 'g' || it->id[0] == 't' ||
                        it->id[0] == 'l');
        if (!de_fora) continue;

        // O que está ABERTO não envelhece. Tarefa aberta é pendência (RN-34), e
        // LISTA não pertence a dia nenhum. O Tasks só reenvia o que mudou: podadas,
        // não voltariam nunca.
        bool pendencia = (it->tipo == TIPO_TAREFA || it->tipo == TIPO_LISTA) &&
                         !it->feita;
        if (pendencia) continue;

        if (data_compara(it->dia, de) >= 0 && data_compara(it->dia, ate) <= 0)
            continue;

        data_t onde = it->dia;
        char id[sizeof it->id];
        snprintf(id, sizeof id, "%s", it->id);
        if (cartao_apaga_item(hal, onde, id) == OK) podados++;
    }

    if (podados) {
        estado_invalida_cartao(e);
        if (hal->registrar) {
            char msg[48];
            snprintf(msg, sizeof msg, "poda: %d fora da janela", podados);
            hal->registrar("uso", msg);
        }
    }
    return OK;
}

erro_t uso_carregar_marcas(const hal_t *hal, estado_t *e)
{
    data_t d = e->dia_visto;

    e->marcas_validas = false;
    e->marcas_tarefa  = 0;
    e->marcas_evento  = 0;
    e->marcas_ano     = d.ano;
    e->marcas_mes     = d.mes;

    erro_t err = garante_indice(hal);
    if (err != OK) return err;

    for (int i = 0; i < indice_n(); i++) {
        const item_t *it = indice_em(i);

        if (it->tipo == TIPO_EVENTO) {
            // A ROTINA responde por qualquer dia do mês, por aritmética.
            if (it->regra[0]) {
                for (int dia = 1; dia <= 31; dia++) {
                    data_t k = { d.ano, d.mes, (int8_t)dia };
                    if (!data_valida(k)) continue;
                    if (rotina_no_dia(it->regra, it->vence, k))
                        e->marcas_evento |= 1u << (dia - 1);
                }
                continue;
            }

            // O que dura mais de um dia ocupa todos eles.
            data_t k = it->vence.ano ? it->vence : it->dia;
            data_t ate = it->prazo.ano ? it->prazo : k;
            for (int passos = 0; passos < 62 && data_compara(k, ate) <= 0;
                 k = data_soma_dias(k, 1), passos++)
                if (k.ano == d.ano && k.mes == d.mes &&
                    k.dia >= 1 && k.dia <= 31)
                    e->marcas_evento |= 1u << (k.dia - 1);

        } else if (it->tipo == TIPO_TAREFA || it->tipo == TIPO_LISTA) {
            // O HÍBRIDO marca os dois: ocupa um horário e é coisa a fazer.
            if (it->tipo == TIPO_TAREFA && it->hora[0] &&
                it->vence.ano == d.ano && it->vence.mes == d.mes &&
                it->vence.dia >= 1 && it->vence.dia <= 31)
                e->marcas_evento |= 1u << (it->vence.dia - 1);

            // Duas datas põem uma tarefa num dia: o PRAZO e a CONCLUSÃO.
            data_t D[2] = { it->vence, it->feita_em };
            for (int k = 0; k < 2; k++) {
                if (D[k].ano != d.ano || D[k].mes != d.mes) continue;
                if (D[k].dia < 1 || D[k].dia > 31)           continue;
                e->marcas_tarefa |= 1u << (D[k].dia - 1);
            }
        }
    }

    // E o mês que o servidor respondeu, por cima: cobre o que está fora da
    // janela.
    if (e->marcas_srv_ano == d.ano && e->marcas_srv_mes == d.mes) {
        e->marcas_evento |= e->marcas_srv_evento;
        e->marcas_tarefa |= e->marcas_srv_tarefa;
    }

    // E o pedido para o próximo pull: só muda quando o mês visto muda.
    snprintf(e->mes_pedido, sizeof e->mes_pedido, "%04d-%02d", d.ano, d.mes);
    e->olhar_ms = 0;

    e->marcas_validas = true;
    return OK;
}

erro_t uso_propor_resultado(estado_t *e, const resultado_t *rs, int n,
                            const char *falou)
{
    if (n < 0) n = 0;
    if (n > RESULTADOS_MAX) n = RESULTADOS_MAX;

    for (int i = 0; i < n; i++) e->resultados[i] = rs[i];
    e->n_resultados = (int8_t)n;

    // O cursor começa na primeira decisão, depois dos resultados (que são
    // paradas só para rolar). E ela é a não-destrutiva, como no T-13.
    e->cursor = (int16_t)n;
    snprintf(e->falou, sizeof e->falou, "%s", falou ? falou : "");
    return OK;
}

// A mesma proposta, com o cartão na mão: ações sobre o que existe ganham o
// ANTES. Separada porque `uso_propor_resultado` é chamada sem hal.
erro_t uso_propor_com_antes(const hal_t *hal, estado_t *e,
                            const resultado_t *rs, int n, const char *falou)
{
    erro_t err = uso_propor_resultado(e, rs, n, falou);
    if (err != OK || !hal) return err;

    for (int i = 0; i < e->n_resultados; i++) {
        resultado_t *r = &e->resultados[i];
        r->tem_antes = false;

        // `n:` é coisa nova: não há antes.
        if (r->item.id[0] == '\0' || r->item.id[0] == 'n') continue;

        item_t antes;
        data_t dia;
        if (cartao_acha_item(hal, r->item.id, &antes, &dia) != OK) continue;

        r->tem_antes   = true;
        r->antes_dia   = dia;
        r->antes_vence = antes.vence;
        snprintf(r->antes_hora,   sizeof r->antes_hora,   "%s", antes.hora);
        snprintf(r->antes_titulo, sizeof r->antes_titulo, "%s", antes.titulo);

        // O que a fala não mudou continua: `d` vazio numa edição é "não falei de
        // data", não "tire a data".
        if (!r->item.vence.ano)  r->item.vence = antes.vence;
        if (!r->item.titulo[0])
            snprintf(r->item.titulo, sizeof r->item.titulo, "%s",
                     antes.titulo);
        if (r->item.tipo == TIPO_NADA) r->item.tipo = antes.tipo;
        r->item.origem = antes.origem;
        snprintf(r->item.nota, sizeof r->item.nota, "%s", antes.nota);
    }
    return OK;
}

// ── a captura crua sai, com áudio e tudo ─────────────────────────────
// Depois da IA, confirmada ou descartada, ela não é mais nada: o que virou
// está gravado ao lado e o que foi dito está na nota. O WAV é o mais pesado
// do sistema, e o cartão não é acervo de gravação.
//
// No dia em que ela NASCEU (`dia`, RN-26), não no do relógio: falar às 23:58
// e confirmar às 00:01 deixava o WAV para sempre.
static void apaga_a_captura(const hal_t *hal, estado_t *e)
{
    if (!e->ultimo.id[0]) return;

    data_t dia = data_valida(e->ultimo.dia) ? e->ultimo.dia : e->hoje;

    char wav[128];
    cartao_caminho_wav(dia, e->ultimo.id, wav, sizeof wav);
    (void)hal->apagar(wav);
    (void)cartao_apaga_item(hal, dia, e->ultimo.id);

    e->ultimo.id[0]   = '\0';
    estado_invalida_cartao(e);
    e->marcas_validas = false;
}

// O id de uma anotação: o da nota (`nt:3fa9…` → `a:3fa9…`). Sem nota, a
// hora, com sufixo se já houver outra igual no dia.
static void id_da_anotacao(const hal_t *hal, const estado_t *e, item_t *it)
{
    const char *nota = it->nota;
    if (strncmp(nota, "nt:", 3) == 0) nota += 3;
    if (nota[0]) {
        snprintf(it->id, sizeof it->id, "a:%s", nota);
    } else {
        snprintf(it->id, sizeof it->id, "a:%02d%02d%02u", e->hora, e->minuto,
                 (unsigned)(e->agora_ms / 1000u % 60u));
    }

    item_t ja;
    char base[sizeof it->id];
    snprintf(base, sizeof base, "%s", it->id);
    for (int n = 2; n < 100 && cartao_le_item(hal, it->dia, it->id, &ja) == OK; n++)
        snprintf(it->id, sizeof it->id, "%.30s-%d", base, n);
}

erro_t uso_confirmar_resultado(const hal_t *hal, estado_t *e)
{
    if (e->n_resultados <= 0) return ERR_INTERNO;

    // Já está indo: o segundo OK não manda de novo. A trava fica no caso de
    // uso, onde a duplicata nasceria.
    if (e->esperando_resultado) return ERR_INTERNO;

    // A tela de Resultado só entra depois da resposta: os `resultados` ficam no
    // estado para ela listar.
    e->esperando_resultado = true;

    for (int i = 0; i < e->n_resultados; i++) {
        const item_t *it = &e->resultados[i].item;

        // Grava e só depois manda.
        const resultado_t *r = &e->resultados[i];

        // APAGAR: some do cartão agora e sobe como "apagou". O dia é o da PASTA
        // (RN-26), não o vencimento.
        if (r->verbo == RES_APAGOU) {
            item_t indo = *it;
            if (r->tem_antes) {
                indo.dia = r->antes_dia;
                (void)cartao_apaga_item(hal, r->antes_dia, indo.id);
            } else {
                (void)cartao_apaga_em_qualquer_dia(hal, indo.id);
            }
            (void)uso_enviar_gesto(hal, e, &indo, "apagou");
            continue;
        }

        // Editar escreve na pasta em que o item MORA; no dia do vencimento novo
        // seriam duas cópias.
        item_t gravando = *it;
        if (r->tem_antes) gravando.dia = r->antes_dia;

        // A anotação nunca troca o `n:1` provisório; gravada com ele, a segunda do
        // dia apagaria a primeira.
        if (gravando.tipo == TIPO_ANOTACAO && gravando.id[0] == 'n')
            id_da_anotacao(hal, e, &gravando);
        e->resultados[i].item = gravando;

        erro_t err = cartao_grava_item(hal, gravando.dia, &gravando);
        if (err != OK) return err;
        it = &gravando;

        // E sobe agora, anotação inclusive (fica no servidor, nunca no Google; o
        // `an:` que volta substitui o provisório). No máximo três chamadas
        // seguidas.
        {
            const char *verbo = r->verbo == RES_EDITOU ? "editou" : "criou";
            (void)uso_enviar_gesto(hal, e, it, verbo);
        }
    }

    // A transcrição fica, uma vez, na NOTA: é o que responde "de onde isto
    // veio?" depois que o áudio some.
    if (e->falou[0] && e->n_resultados > 0 && e->resultados[0].item.nota[0])
        (void)cartao_grava_transcricao(hal, e->resultados[0].item.nota,
                                       e->falou);

    // E o ÁUDIO some, depois de CONFIRMAR. O cartão existe para não ter
    // minutagem máxima e deixar pausar, não para guardar gravação; a prova do
    // que foi dito é a transcrição.
    apaga_a_captura(hal, e);

    // Nada subiu (só anotações): não há resposta a esperar, senão o aparelho
    // ficava preso e a confirmação seguinte era recusada.
    if (e->nuvem_esperando == NUVEM_NADA && e->gesto_n == 0)
        e->esperando_resultado = false;

    // Os RESULTADOS ficam para a tela de Resultado, que entra em seguida;
    // quem os apaga é a saída dela.
    e->falou[0] = '\0';

    // RN-3A: o que caiu em hoje aparece agora, e o que caiu em outro dia marca
    // o calendário.
    estado_invalida_cartao(e);
    e->marcas_validas = false;
    return OK;
}

erro_t uso_descartar_resultado(const hal_t *hal, estado_t *e)
{
    e->n_resultados = 0;
    e->falou[0]     = '\0';
    e->cursor       = 0;

    // Some com a gravação junto ("Descartar a gravação"). `uso_descartar_captura`
    // não serve aqui: é para abortar uma gravação EM CURSO, e esta já fechou.
    apaga_a_captura(hal, e);
    return OK;
}

erro_t uso_carregar_pendentes(const hal_t *hal, estado_t *e)
{
    e->n_pendentes       = 0;
    e->n_pendentes_fora  = 0;
    e->pendentes_validas = true;

    // Do ÍNDICE, sem varrer: a mesma fonte da tela do dia.
    erro_t err = garante_indice(hal);
    if (err != OK) return err;

    {
        {
        int n = indice_n();

        for (int i = 0; i < n; i++) {
            const item_t *it = indice_em(i);

            // O evento de vários dias entra também: os dias do meio só o acham aqui.
            bool atravessa = it->tipo == TIPO_EVENTO && it->prazo.ano;

            // E a ROTINA, que mora na pasta da âncora.
            bool rotina = it->regra[0] != '\0';

            if (it->tipo != TIPO_TAREFA && it->tipo != TIPO_LISTA &&
                !atravessa && !rotina)
                continue;

            // A FEITA entra também (RN-35): fica riscada onde estava, em vez de sumir
            // debaixo do dedo. Estourar o teto: conta, avisa e segue.
            if (e->n_pendentes >= PENDENTES_MAX) { e->n_pendentes_fora++; continue; }
            e->pendentes[e->n_pendentes++] = *it;
        }
        }
    }

    // Itens no índice e tarefas que saíram: separa cartão sem tarefa, índice
    // que não montou e teto estourado.
    if (hal->registrar) {
        char msg[64];
        snprintf(msg, sizeof msg, "pendentes: %d no indice, %d tarefas, %d fora",
                 indice_n(), (int)e->n_pendentes, (int)e->n_pendentes_fora);
        hal->registrar("uso", msg);
    }
    return OK;
}
