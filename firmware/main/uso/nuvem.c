#include "nuvem.h"
#include "acervo.h"
#include "uso.h"

#include "../dado/cartao.h"
#include "../dado/indice.h"
#include "../dado/acervo.h"
#include "../dado/nuvem.h"
#include "../dado/contrato.h"
#include "../dado/json.h"
#include "../dado/perfil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

erro_t uso_nuvem_ligar(const hal_t *hal, estado_t *e)
{
    if (!hal || !e) return ERR_INTERNO;
    if (!hal->nuvem_pede || !hal->nuvem_credencial) return ERR_REDE;
    if (e->rede != REDE_LIGADA) return ERR_REDE;

    // ── o relógio ANTES do TLS ───────────────────────────────────────────
    // Validar certificado compara datas, e o aparelho recém-ligado acha que é
    // 1970: toda conexão falha com `ESP_ERR_HTTP_CONNECT`. O NTP é assíncrono;
    // `EV_HORA_DA_REDE` chama de novo quando a hora chega.
    if (!e->hora_confiavel) return ERR_REDE;

    if (e->registro_tentado &&
        e->agora_ms - e->registro_ms < 30u * 1000u) return OK;
    e->registro_tentado = true;
    e->registro_ms = e->agora_ms;

    nuvem_cred_t cred;
    (void)nuvem_carrega(hal, &cred);

    // O id vai para a tela antes de qualquer atalho: é quando o servidor recusa
    // o aparelho que alguém precisa lê-lo.
    snprintf(e->meu_id, sizeof e->meu_id, "%s", cred.device_id);

    // O endereço vale mesmo sem token: o registro precisa de para onde ir.
    hal->nuvem_credencial(cred.servidor, cred.token);

    e->tem_token = nuvem_registrado(&cred);

    // Já registrado: falta saber DE QUEM. `e->nome` não sobrevive ao desligar,
    // e sem ele `uso_sincronizar` recusa (ERR_NAO_PAREADO).
    if (nuvem_registrado(&cred)) {
        e->nuvem_esperando = NUVEM_PAREADO;
    e->nuvem_desde_ms  = e->agora_ms;
        hal->nuvem_pede("/v1/parear/estado", NULL, NULL);
        return OK;
    }

    // A prova vai para o cartão ANTES de sair pela rede. `nuvem_carrega` a
    // inventa sem gravar: sem esta escrita, o servidor fixaria uma e o cartão
    // guardaria outra, e todo registro seguinte levaria 401 para sempre.
    if (nuvem_grava(hal, &cred) != OK) return ERR_ARQUIVO;

    // A prova separa "sou este aparelho" de "sei o número de série dele": o MAC
    // se lê da etiqueta; a prova nunca saiu daqui.
    char corpo[160];
    snprintf(corpo, sizeof corpo,
             "{\"device_id\":\"%s\",\"prova\":\"%s\"}",
             cred.device_id, cred.prova);

    e->nuvem_esperando = NUVEM_REGISTRAR;
    e->nuvem_desde_ms  = e->agora_ms;
    hal->nuvem_pede("/v1/registrar", corpo, NULL);
    return OK;
}

erro_t uso_desvincular(const hal_t *hal, estado_t *e)
{
    if (!hal || !e) return ERR_INTERNO;
    if (!hal->nuvem_pede) return ERR_REDE;
    if (e->rede != REDE_LIGADA) return ERR_REDE;

    e->nuvem_esperando = NUVEM_DESVINCULAR;
    e->nuvem_desde_ms  = e->agora_ms;
    hal->nuvem_pede("/v1/desparear", "{}", NULL);
    return OK;
}

// Percent-encoding: o que não é letra, dígito ou `-_.~` vira %XX, byte a
// byte (cobre UTF-8).
static void url_codifica(const char *in, char *out, size_t max)
{
    static const char HEX[] = "0123456789ABCDEF";
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p; p++) {
        bool livre = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                     (*p >= '0' && *p <= '9') || strchr("-_.~", *p);
        if (livre) {
            if (n + 1 >= max) break;
            out[n++] = (char)*p;
        } else {
            if (n + 3 >= max) break;
            out[n++] = '%';
            out[n++] = HEX[*p >> 4];
            out[n++] = HEX[*p & 15];
        }
    }
    out[n] = '\0';
}

// O nome que voltou no pull. Com troca local esperando, é a confirmação e a
// marca sai; sem troca local, o aparelho adota o do servidor (trocado no
// app). Servidor sem nome recebe o daqui no próximo pull.
static void aplica_nome(const hal_t *hal, estado_t *e, const char *json)
{
    char dele[NOME_UTF8_MAX];
    if (!json_str(json, "aparelho_nome", dele, sizeof dele)) return;

    const char *meu = e->inicio.nome_pendente;
    if (e->inicio.nome_sobe) {
        if (strcmp(dele, meu) == 0 && perfil_renomeia(hal, meu, false) == OK)
            e->inicio.nome_sobe = false;
        return;
    }
    if (!dele[0]) {
        if (meu[0] && perfil_renomeia(hal, meu, true) == OK)
            e->inicio.nome_sobe = true;
        return;
    }
    if (strcmp(dele, meu) != 0 && perfil_renomeia(hal, dele, false) == OK) {
        perfil_local_t p;
        if (perfil_carrega(hal, &p) == OK)
            snprintf(e->inicio.nome_pendente, sizeof e->inicio.nome_pendente,
                     "%s", p.nome);
    }
}

erro_t uso_sincronizar(const hal_t *hal, estado_t *e)
{
    if (!hal || !e) return ERR_INTERNO;
    if (!hal->nuvem_pede) return ERR_REDE;
    if (e->nome[0] == '\0') return ERR_NAO_PAREADO;
    if (e->rede != REDE_LIGADA) return ERR_REDE;

    // Um pull de cada vez: o segundo chegaria com o cursor já andado.
    if (e->pull_esperando) return OK;

    // Só PULL: os gestos sobem pela fila, na linha do gesto.
    //
    // `esperar=1` é long polling: o servidor segura a resposta até haver o que
    // dizer, e um evento marcado no celular aparece em segundos. Opt-in: um
    // servidor que não conhece o parâmetro responde na hora.
    //
    // `tudo=1` (a colheita inteira) só quando não há NADA do Google no CARTÃO —
    // recém-pareado, ou lote perdido (o `syncToken` nunca reenvia o que já deu).
    // Perguntar ao cache do dia fazia um hoje vazio pedir tudo a cada minuto, e
    // a colheita apaga o que veio de fora antes de reescrever. `pediu_tudo`
    // segura um segundo pedido enquanto o primeiro está em voo.
    if (!e->google_verificado) {
        e->tem_google = cartao_tem_do_google(hal);
        e->google_verificado = true;
    }
    bool vazio = !e->pediu_tudo && !e->tem_google;
    if (vazio) e->pediu_tudo = true;

    // Na linha DELE: o pull é o único que fica pendurado.
    e->pull_esperando = true;
    e->nuvem_desde_ms = e->agora_ms;

    // Mês e dia aberto NÃO vão aqui: são consulta, e vão pela linha do gesto
    // (`uso_olhar`). O nome do aparelho vai de carona quando foi trocado aqui e
    // ainda não subiu.
    char nome[NOME_UTF8_MAX * 3];
    nome[0] = '\0';
    if (e->inicio.nome_sobe)
        url_codifica(e->inicio.nome_pendente, nome, sizeof nome);

    char rota[NUVEM_ROTA_MAX];
    snprintf(rota, sizeof rota, "/v1/pull?esperar=1%s%s%s",
             vazio ? "&tudo=1" : "", nome[0] ? "&nome=" : "", nome);

    hal->nuvem_pede(rota, NULL, NULL);
    return OK;
}

erro_t uso_olhar(const hal_t *hal, estado_t *e)
{
    if (!hal || !e || !hal->nuvem_pede) return ERR_INTERNO;
    if (!e->mes_pedido[0] && !e->dia_pedido[0]) return OK;
    if (e->nome[0] == '\0') return ERR_NAO_PAREADO;
    if (e->rede != REDE_LIGADA) return ERR_REDE;

    // A linha do gesto é uma: um "feita" em voo vem primeiro, e a consulta sai
    // no próximo tick.
    if (e->nuvem_esperando != NUVEM_NADA) return OK;

    // Reenviada até ser respondida (o pedido só morre na resposta), a cada 3 s:
    // sem o relógio, rede fora seria um pedido por passo do laço.
    if (e->olhar_ms && e->agora_ms - e->olhar_ms < 3000u) return OK;
    e->olhar_ms = e->agora_ms ? e->agora_ms : 1;

    // Os dois campos sempre, vazios ou não: o servidor confere cada um.
    char rota[48];
    snprintf(rota, sizeof rota, "/v1/olhar?mes=%s&dia=%s",
             e->mes_pedido, e->dia_pedido);

    e->nuvem_esperando = NUVEM_OLHAR;
    e->nuvem_desde_ms  = e->agora_ms;
    hal->nuvem_pede(rota, NULL, NULL);
    return OK;
}

// ── T-32 · as agendas da conta ───────────────────────────────────────
erro_t uso_agendas(const hal_t *hal, estado_t *e)
{
    if (!hal || !e) return ERR_INTERNO;
    if (!hal->nuvem_pede) return ERR_REDE;
    if (e->nome[0] == '\0') return ERR_NAO_PAREADO;
    if (e->rede != REDE_LIGADA) return ERR_REDE;

    // O prazo da busca marca na TRANSIÇÃO: o pedido tenta a cada tick, e
    // reiniciar em cada um seria um prazo que nunca vence.
    if (!e->agendas_buscando) e->agendas_desde_ms = e->agora_ms;
    e->agendas_pedidas  = true;
    e->agendas_buscando = true;

    // Uma resposta por vez (`nuvem_esperando`). Mas o gesto não some: com o
    // long polling a linha vive ocupada, então o pedido fica GUARDADO e sai no
    // primeiro tick em que ela vagar.
    if (e->nuvem_esperando != NUVEM_NADA) {
        e->agendas_querendo = true;
        return OK;
    }

    e->agendas_querendo = false;
    e->nuvem_esperando  = NUVEM_AGENDAS;
    hal->nuvem_pede("/v1/agendas", NULL, NULL);
    return OK;
}

// A escolha guardada sai. Chamada pelo tick assim que a linha vaga.
erro_t uso_agenda_pendente(const hal_t *hal, estado_t *e)
{
    if (!hal || !e || !hal->nuvem_pede) return ERR_INTERNO;
    if (!e->agenda_querendo) return OK;
    if (e->nuvem_esperando != NUVEM_NADA) return OK;

    int i = e->agenda_querida;
    if (i < 0 || i >= e->n_agendas) { e->agenda_querendo = false; return OK; }

    // O nome como está na tela; o backend casa contra o catálogo dele (404 se
    // não achar).
    char corpo[80];
    snprintf(corpo, sizeof corpo, "{\"t\":\"%s\",\"on\":%s}",
             e->agendas[i].nome, e->agenda_querida_on ? "true" : "false");

    // Guarda QUAL: a resposta devolve o id de lá, que não existe aqui.
    e->agenda_alvo     = (int8_t)i;
    e->agenda_querendo = false;
    e->nuvem_esperando = NUVEM_ESCOLHA;
    e->nuvem_desde_ms  = e->agora_ms;
    e->sinc            = SINC_ENVIANDO;
    hal->nuvem_pede("/v1/agendas", corpo, NULL);
    return OK;
}

erro_t uso_escolher_agenda(const hal_t *hal, estado_t *e, int indice,
                           bool ligada)
{
    if (!hal || !e) return ERR_INTERNO;
    if (!hal->nuvem_pede) return ERR_REDE;
    if (indice < 0 || indice >= e->n_agendas) return ERR_INTERNO;
    if (e->rede != REDE_LIGADA) return ERR_REDE;

    // A linha tem um dono por vez, e o gesto ESPERA em vez de sumir.
    e->agenda_querendo   = true;
    e->agenda_querida    = (int8_t)indice;
    e->agenda_querida_on = ligada;
    e->sinc              = SINC_ENVIANDO;

    return uso_agenda_pendente(hal, e);
}

erro_t uso_enviar_captura(const hal_t *hal, estado_t *e, const char *wav)
{
    if (!hal || !e) return ERR_INTERNO;
    if (!hal->nuvem_pede) return ERR_REDE;
    if (e->rede != REDE_LIGADA) return ERR_REDE;

    if (!hal->nuvem_pede_audio) return ERR_REDE;

    // ── a linha tem UM dono ──────────────────────────────────────────────
    // A captura escrevia `nuvem_esperando` por cima do que estava em voo, e a
    // resposta de um gesto era lida como a da fala. Ela espera a vez; basta
    // lembrar o caminho do WAV.
    if (e->nuvem_esperando != NUVEM_NADA) {
        snprintf(e->captura_pendente, sizeof e->captura_pendente, "%s",
                 wav ? wav : "");
        e->sinc = SINC_ENVIANDO;
        return OK;
    }
    e->captura_pendente[0] = '\0';

    // Sobe o ÁUDIO, montado pelo hal (uso/ não abre arquivo). O id da operação
    // é o caminho do WAV: o mesmo arquivo duas vezes é a MESMA fala, e um OK
    // repetido depois de uma resposta perdida não cria dois eventos.
    e->nuvem_esperando = NUVEM_CAPTURA;
    e->nuvem_desde_ms  = e->agora_ms;
    e->sinc            = SINC_ENVIANDO;
    hal->nuvem_pede_audio("/v1/captura", wav ? wav : "", wav ? wav : "");
    return OK;
}

// A captura que ficou esperando a linha vagar. Chamada pelo tick.
erro_t uso_captura_pendente(const hal_t *hal, estado_t *e)
{
    if (!hal || !e) return ERR_INTERNO;
    if (!e->captura_pendente[0]) return OK;
    if (e->nuvem_esperando != NUVEM_NADA) return OK;
    if (e->rede != REDE_LIGADA) return OK;

    char wav[sizeof e->captura_pendente];
    snprintf(wav, sizeof wav, "%s", e->captura_pendente);
    e->captura_pendente[0] = '\0';
    return uso_enviar_captura(hal, e, wav);
}

// ── as LÁPIDES ───────────────────────────────────────────────────────
// Apagado aqui e ainda não lá: o pull não pode regravá-lo, senão a exclusão
// se desfaz sozinha.
bool uso_esta_apagando(const estado_t *e, const char *id)
{
    if (!e || !id || !id[0]) return false;
    for (int i = 0; i < e->n_apagando; i++)
        if (strcmp(e->apagando[i], id) == 0) return true;
    return false;
}

static void poe_lapide(estado_t *e, const char *id)
{
    if (!id || !id[0] || uso_esta_apagando(e, id)) return;

    int max = (int)(sizeof e->apagando / sizeof e->apagando[0]);
    if (e->n_apagando >= max) {
        // Cheio: a mais velha sai (no pior caso, um item reaparece uma vez).
        // `memmove`: as duas pontas são o mesmo vetor.
        for (int i = 1; i < max; i++)
            memmove(e->apagando[i - 1], e->apagando[i],
                    sizeof e->apagando[0]);
        e->n_apagando--;
    }
    snprintf(e->apagando[e->n_apagando], sizeof e->apagando[0], "%s", id);
    e->n_apagando++;
}

static void tira_lapide(estado_t *e, const char *id)
{
    for (int i = 0; i < e->n_apagando; i++) {
        if (strcmp(e->apagando[i], id) != 0) continue;
        for (int j = i + 1; j < e->n_apagando; j++)
            memmove(e->apagando[j - 1], e->apagando[j],
                    sizeof e->apagando[0]);
        e->n_apagando--;
        return;
    }
}

// A fila do cartão, relida no boot: sem ela, gestos na fila seriam
// esquecidos e o pull traria de volta o que foi apagado.
erro_t uso_gestos_do_cartao(const hal_t *hal, estado_t *e)
{
    if (!hal || !e) return ERR_INTERNO;

    char nomes[GESTOS_MAX][40];
    int n = 0;
    if (cartao_lista_gestos(hal, nomes, GESTOS_MAX, &n) != OK) return OK;

    e->gesto_n     = (int8_t)n;
    e->n_apagando  = 0;
    e->gesto_seq   = cartao_ultimo_gesto(hal);

    for (int i = 0; i < n; i++) {
        char corpo[384], op[80];
        data_t dia;
        if (cartao_le_gesto(hal, nomes[i], corpo, sizeof corpo,
                            op, sizeof op, &dia) != OK)
            continue;

        char verbo[12] = "", id[40] = "";
        (void)json_str(corpo, "v", verbo, sizeof verbo);
        (void)json_str(corpo, "id", id, sizeof id);
        if (strcmp(verbo, "apagou") == 0) poe_lapide(e, id);
    }

    if (n && hal->registrar) {
        char msg[48];
        snprintf(msg, sizeof msg, "fila: %d gesto(s) esperando a rede", n);
        hal->registrar("nuvem", msg);
    }
    return OK;
}

// O gesto mais velho sai, quando a linha vaga E há rede.
erro_t uso_gesto_pendente(const hal_t *hal, estado_t *e)
{
    if (!hal || !e || !hal->nuvem_pede) return ERR_INTERNO;
    if (e->gesto_n <= 0) return OK;
    if (e->nuvem_esperando != NUVEM_NADA) return OK;
    if (e->rede != REDE_LIGADA) return OK;

    char nomes[GESTOS_MAX][40];
    int n = 0;
    if (cartao_lista_gestos(hal, nomes, GESTOS_MAX, &n) != OK) return ERR_ARQUIVO;

    e->gesto_n = (int8_t)n;
    if (n <= 0) return OK;

    // O MAIS VELHO, sempre: renomear e apagar fora de ordem trazem o nome
    // antigo de volta.
    char corpo[384], op[80];
    data_t dia;
    if (cartao_le_gesto(hal, nomes[0], corpo, sizeof corpo,
                        op, sizeof op, &dia) != OK) {
        // Arquivo ilegível sai: a fila inteira parada atrás dele é pior.
        (void)cartao_apaga_gesto(hal, nomes[0]);
        e->gesto_n--;
        return ERR_FORMATO;
    }

    e->nuvem_esperando = NUVEM_GESTO;
    e->nuvem_desde_ms  = e->agora_ms;
    e->sinc            = SINC_ENVIANDO;
    snprintf(e->gesto_em_voo, sizeof e->gesto_em_voo, "%s", nomes[0]);

    // O item deste gesto entra na linha com ele: a resposta é dele.
    (void)json_str(corpo, "id", e->gesto_id, sizeof e->gesto_id);
    e->gesto_dia = dia;

    hal->nuvem_pede("/v1/push", corpo, op);
    return OK;
}

erro_t uso_enviar_gesto(const hal_t *hal, estado_t *e,
                        const item_t *it, const char *verbo)
{
    if (!hal || !e || !it) return ERR_INTERNO;
    if (!hal->nuvem_pede) return ERR_REDE;

    char momento[26] = "";
    if (e->hora_confiavel) {
        int fuso = e->config.valor[AJUSTE_FUSO_MIN];
        char sinal = fuso < 0 ? '-' : '+';
        if (fuso < 0) fuso = -fuso;
        snprintf(momento, sizeof momento,
                 "%04d-%02d-%02dT%02d:%02d:00%c%02d:%02d",
                 e->hoje.ano, e->hoje.mes, e->hoje.dia, e->hora, e->minuto,
                 sinal, fuso / 60, fuso % 60);
    }

    char corpo[384];
    if (!contrato_gesto(it, verbo, momento, corpo, sizeof corpo))
        return ERR_INTERNO;

    // O gesto é este verbo, neste item, neste instante: repetir mais tarde é
    // gesto NOVO, e o carimbo entra no id.
    char operacao[80];
    snprintf(operacao, sizeof operacao, "%s-%s-%lu", verbo ? verbo : "criou",
             it->id, (unsigned long)(hal->agora_ms ? hal->agora_ms() : 0));

    // Vai para o CARTÃO: apagar sem Wi-Fi some do vidro na hora e sobe quando
    // a rede voltar. Em RAM morreria no boot.
    if (e->gesto_n >= GESTOS_MAX) {
        // Fila cheia: recusa o NOVO. Jogar fora o mais velho perderia o que a
        // pessoa já viu acontecer.
        snprintf(e->precisa_rede, sizeof e->precisa_rede, "%s",
                 "fila cheia");
        return ERR_CHEIO;
    }

    erro_t err = cartao_grava_gesto(hal, e->gesto_seq + 1, corpo, operacao,
                                    it->dia);
    if (err != OK) return err;

    e->gesto_seq++;
    e->gesto_n++;
    if (verbo && strcmp(verbo, "apagou") == 0) poe_lapide(e, it->id);

    e->sinc = e->rede == REDE_LIGADA ? SINC_ENVIANDO : SINC_ESPERANDO_REDE;

    return uso_gesto_pendente(hal, e);
}

erro_t uso_parear(const hal_t *hal, estado_t *e)
{
    if (!hal || !e) return ERR_INTERNO;
    if (!hal->nuvem_pede) return ERR_REDE;
    if (e->rede != REDE_LIGADA) return ERR_REDE;

    e->codigo[0]       = '\0';   // some enquanto não chega o novo
    e->nuvem_esperando = NUVEM_PAREAR;
    e->nuvem_desde_ms  = e->agora_ms;
    // `"{}"` e não NULL: o hal escolhe POST quando há corpo. Com NULL, saía GET
    // numa rota só POST (405) — e o harness do PC não simula método.
    hal->nuvem_pede("/v1/parear/iniciar", "{}", NULL);
    return OK;
}

// ── apaga do cartão tudo o que veio de FORA ──────────────────────────
// Só o de fora: o que nasceu aqui não tem de onde voltar. Chamada quando o
// servidor diz `zerar` (resync diário, troca de agenda em T-32); sem ela,
// desligar "Feriados" deixava no cartão os que já desceram.
static void limpa_o_que_veio_de_fora(const hal_t *hal, estado_t *e)
{
    // Pelo índice, de trás para a frente: `indice_tira` troca o último com o
    // buraco.
    for (int i = indice_n() - 1; i >= 0; i--) {
        const item_t *it = indice_em(i);
        if (!it) continue;

        // O prefixo diz quem é a autoridade: criado por voz e confirmado ganha
        // g:/t: e some se o Google não o devolver.
        if (it->id[1] != ':' ||
            (it->id[0] != 'g' && it->id[0] != 't' && it->id[0] != 'l'))
            continue;

        data_t onde = it->dia;
        char id[sizeof it->id];
        snprintf(id, sizeof id, "%s", it->id);
        (void)cartao_apaga_item(hal, onde, id);
    }

    // O cache do dia tem cópias do que saiu: cai junto.
    estado_invalida_cartao(e);
}


// O delta: itens que entram e ids que saem. O cursor só avança depois do
// lote inteiro: travar repete, nunca pula.
//
// ── a resposta da CONSULTA ───────────────────────────────────────────
// Mês da grade e dia aberto. Não vai para o cartão: vive no cache do dia, e
// `uso_carregar_dia` o preserva enquanto `dia_consultado` for este dia.
static erro_t aplica_olhar(const hal_t *hal, estado_t *e, const char *json)
{
    if (!json) return ERR_FORMATO;

    char mes[8] = "";
    if (json_str(json, "mcm", mes, sizeof mes) && mes[0]) {
        int evt = 0, tar = 0;
        (void)json_int(json, "mce", &evt);
        (void)json_int(json, "mct", &tar);

        e->marcas_srv_ano    = (int16_t)atoi(mes);
        e->marcas_srv_mes    = (int8_t)atoi(mes + 5);
        e->marcas_srv_evento = (uint32_t)evt;
        e->marcas_srv_tarefa = (uint32_t)tar;

        // A grade se redesenha com o que chegou.
        e->marcas_validas = false;
        e->mes_pedido[0]  = '\0';
    }

    // Só vale para o dia que a resposta DIZ responder: sem esse eco, resposta
    // sem dia era lida como "dia vazio" e o pedido morria.
    char dd[12] = "";
    if (!json_str(json, "dd", dd, sizeof dd) || !dd[0]) return OK;
    if (strcmp(dd, e->dia_pedido) != 0) {
        if (hal->registrar) hal->registrar("dia", "resposta de outro dia");
        return OK;
    }

    // E só entra se a tela ainda está NAQUELE dia.
    if (!data_igual(e->dia_visto, e->dia_pedido_data)) return OK;

    const char *do_dia = strstr(json, "\"dia\"");
    if (!do_dia) return ERR_FORMATO;

    const char *p = strchr(do_dia, '[');
    const char *fim_dia = contrato_fim_do_array(p);
    char objeto[320];
    e->n_itens = 0;
    e->n_fora  = 0;
    while (p && contrato_proximo_ate(&p, fim_dia, objeto, sizeof objeto)) {
        item_t it;
        if (!contrato_item(objeto, &it)) continue;
        it.dia = e->dia_visto;
        if (e->n_itens < ITENS_MAX) e->itens[e->n_itens++] = it;
        else                        e->n_fora++;
    }

    e->itens_validos      = true;
    e->dia_consultado     = e->dia_visto;
    e->dia_fora_da_janela = false;
    e->dia_pedido[0]      = '\0';

    if (hal->registrar) {
        char msg[48];
        snprintf(msg, sizeof msg, "consulta %s: %d itens", dd, (int)e->n_itens);
        hal->registrar("dia", msg);
    }
    return OK;
}

static erro_t aplica_pull(const hal_t *hal, estado_t *e, const char *json)
{
    // ANTES de aplicar: o lote desta resposta é a verdade nova.
    bool zerar = false;
    if (json_bool(json, "zerar", &zerar) && zerar)
        limpa_o_que_veio_de_fora(hal, e);

    int entraram = 0;

    // ── o lote foi INTEIRO para o cartão? ────────────────────────────────
    // Só então o cursor avança: num FAT não há transação, e repetir o lote até
    // passar é o substituto. Repetir é seguro (gravar é idempotente, apagar o
    // que não existe é no-op); pular perderia o item para sempre.
    bool lote_inteiro = true;

    const char *lista = strstr(json, "\"itens\"");
    if (lista) {
        const char *p = strchr(lista, '[');
        const char *fim_itens = contrato_fim_do_array(p);
        char objeto[320];
        while (p && contrato_proximo_ate(&p, fim_itens, objeto, sizeof objeto)) {
            item_t it;
            if (!contrato_item(objeto, &it)) continue;

            // A LÁPIDE vence o pull: regravar desfaria a exclusão que ainda não subiu.
            if (uso_esta_apagando(e, it.id)) continue;

            // Onde ele mora: o que vem do Google (`g:`, `t:`) mora no dia que o Google
            // diz — mudar a data no celular muda a pasta aqui. O que nasceu aqui não se
            // move (RN-26). Sem destino não muda: tarefa sem prazo trocaria de pasta
            // todo dia.
            data_t quer = it.vence.ano ? it.vence : e->hoje;

            // RN-36B: a concluída mora no dia em que foi concluída (`completed`).
            if (it.tipo == TIPO_TAREFA && it.feita && it.feita_em.ano)
                quer = it.feita_em;

            item_t antes;
            data_t onde_antes;
            bool tinha = !zerar &&
                         cartao_acha_item(hal, it.id, &antes, &onde_antes) == OK;

            bool manda_o_google = it.id[1] == ':' &&
                                  (it.id[0] == 'g' || it.id[0] == 't');
            bool tem_destino = it.vence.ano ||
                               (it.feita && it.feita_em.ano);

            data_t onde = quer;
            if (tinha && (!manda_o_google || !tem_destino))
                onde = onde_antes;
            it.dia = onde;

            // Sincronizar não muda a ORIGEM: o servidor não sabe que o item nasceu de
            // uma fala. Gravar por cima apagava a `nota` e a transcrição sumia. A
            // `origem` vai junto: também decide o que se pode fazer com o item (RN-2A).
            if (tinha && antes.nota[0]) {
                snprintf(it.nota, sizeof it.nota, "%s", antes.nota);
                it.origem = antes.origem;
            }

            // A hora vem do Google e some com ele: o servidor casa a tarefa com o
            // evento gêmeo no Calendar, onde a hora mora. Preservá-la aqui deixava na
            // régua um horário que já não existia.

            // O que já está igual não se reescreve: poupa cartão e redesenho, e deixa
            // o servidor repetir de graça o que não tem certeza de ter entregue.
            if (tinha && data_igual(onde, onde_antes) &&
                memcmp(&it, &antes, sizeof it) == 0)
                continue;

            entraram++;

            if (cartao_grava_item(hal, onde, &it) != OK) lote_inteiro = false;

            // Mudou de dia: a cópia velha sai.
            else if (tinha && !data_igual(onde, onde_antes))
                (void)cartao_apaga_item(hal, onde_antes, it.id);
        }
    }

    // Quantos entraram: um pull vazio e um cheio teriam o mesmo silêncio na
    // serial.
    if (hal->registrar) {
        char msg[48];
        snprintf(msg, sizeof msg, "pull: %d itens", entraram);
        hal->registrar("nuvem", msg);
    }

    // `pediu_tudo` cai quando a colheita chega.
    if (entraram) e->pediu_tudo = false;

    int saíram = 0;
    const char *fora = strstr(json, "\"removidos\"");
    if (fora) {
        const char *p = strchr(fora, '[');
        const char *fim_fora = contrato_fim_do_array(p);
        char objeto[320];
        while (p && contrato_proximo_ate(&p, fim_fora, objeto, sizeof objeto)) {
            char id[40];
            if (!json_str(objeto, "id", id, sizeof id)) continue;
            saíram++;
            if (cartao_apaga_em_qualquer_dia(hal, id) != OK)
                lote_inteiro = false;
        }
    }

    // O cursor, só se o lote passou inteiro: travar repete no próximo pull.
    if (lote_inteiro)
        (void)json_str(json, "cursor", e->cursor_pull, sizeof e->cursor_pull);
    else if (hal->registrar)
        hal->registrar("nuvem", "lote nao gravou inteiro: cursor parado");

    // O cache do dia e as marcas caem SÓ quando algo mudou: invalidar em todo
    // pull vazio relia meio segundo de cartão a cada ciclo.
    //
    // Enquanto o servidor disser `mais`, o vidro espera: invalidar por pedaço
    // repintava a agenda item a item. O cartão recebe tudo; a tela relê no fim.
    bool mais = false;
    (void)json_bool(json, "mais", &mais);
    e->pull_tem_mais = mais;

    // Mês e dia aberto não vêm mais aqui (ver `uso_olhar`).

    e->pull_mudou = e->pull_mudou || entraram || saíram;
    if (!mais && e->pull_mudou) {
        estado_invalida_cartao(e);
        e->pull_mudou = false;
    }
    return OK;
}

static erro_t uso_nuvem_resposta_com(const hal_t *hal, estado_t *e,
                                     char *json, size_t capacidade)
{
    if (!hal || !e || !json || capacidade < 2) return ERR_INTERNO;
    bool veio = hal->nuvem_resposta &&
                hal->nuvem_resposta(json, capacidade) == OK;

    // ── de QUAL linha ────────────────────────────────────────────────────
    // A leitura vem antes da decisão: com duas linhas, delta e confirmação podem
    // chegar no mesmo quadro. E é a leitura que anda a fila de ordem da placa.
    bool do_pull = hal->nuvem_linha && hal->nuvem_linha() == 1;

    nuvem_espera_t o_que;
    if (do_pull) {
        o_que = NUVEM_PULL;
        e->pull_esperando = false;
    } else {
        o_que = (nuvem_espera_t)e->nuvem_esperando;
        e->nuvem_esperando = NUVEM_NADA;
    }

    // E fica registrado DE QUEM era: olhar o pendente devolveria a outra linha.
    e->nuvem_respondeu = (int8_t)o_que;

    if (veio) {
        // Contato: "o servidor está de pé?". Marcado aqui, por onde toda resposta
        // passa.
        e->nuvem_contato_ms = hal->agora_ms ? hal->agora_ms() : 0;
        e->nuvem_ja_falou   = true;
    } else {
        // O código diz qual recusa foi; cada uma leva a um lugar diferente.
        int codigo = hal->nuvem_codigo ? hal->nuvem_codigo() : 0;
        bool recusou = codigo == 401 || codigo == 402 ||
                       codigo == 403 || codigo == 409 || codigo == 428;

        // Recusa HTTP prova que Wi-Fi, internet e servidor responderam: não é "sem
        // internet".
        if (recusou) {
            e->nuvem_contato_ms = hal->agora_ms ? hal->agora_ms() : 0;
            e->nuvem_ja_falou = true;
        }

        if (codigo == 402)      e->recusa = RECUSA_MINUTOS;
        else if (codigo == 409) e->recusa = RECUSA_SEM_CONTA;
        else if (codigo == 428) {
            // O aviso de conta caída é um LEMBRETE: o 428 vem em toda resposta, e a
            // faixa voltava por cima de Minha Conta. Uma vez por episódio: aqui, ao
            // perceber a queda (inclusive no primeiro pull depois do boot); o outro
            // momento, entrar em Minha Conta, é do app.
            if (!e->conta_avisada) {
                e->recusa = RECUSA_CONTA;
                e->conta_avisada = true;
            }
            e->conta_reconectar = true;
        }
        else if (codigo == 401) {
            // O servidor não reconhece mais este token (perdeu o registro). Sem tratar,
            // toda rota dá 401 e o aparelho fica mudo. O remédio é se reapresentar com
            // `device_id` e prova.
            e->recusa         = RECUSA_TOKEN;
            e->tem_token      = false;
            e->registro_ms    = 0;      // tenta já, sem esperar o ciclo
            nuvem_esquece_token(hal);
        }

        // Sem vínculo (409) ou sem identidade (401), a conta local some. No 428
        // ela fica: só a autorização do Google venceu.
        if (codigo == 409 || codigo == 401) {
            e->nome[0] = '\0';
            e->codigo[0] = '\0';
            estado_conta_ok(e);
            e->quota.usados_s = e->quota.limite_s = 0;
        }

        // A busca de agendas morreu com a resposta que não veio: sem isto,
        // "Buscando…" para sempre.
        if (o_que == NUVEM_AGENDAS) {
            e->agendas_buscando = false;
            e->agendas_querendo = false;
        }
        if (o_que == NUVEM_ESCOLHA) e->agenda_alvo = -1;

        // O download que morreu solta a obra: senão o próximo texto seria gravado
        // com o nome dela.
        if (o_que == NUVEM_OBRA) e->obra_baixando[0] = '\0';

        e->sinc = recusou ? SINC_OCIOSO : SINC_ERRO;
        e->ultimo_erro = recusou ? OK : ERR_REDE;
        return ERR_REDE;
    }

    switch (o_que) {
    case NUVEM_DESVINCULAR:
        // O servidor confirmou: só AGORA o aparelho esquece. O cartão fica.
        e->nome[0]  = '\0';
        e->codigo[0] = '\0';
        e->codigo_ate_ms = 0;
        e->quota.usados_s = e->quota.limite_s = 0;
        e->sinc = SINC_OCIOSO;
        return OK;

    case NUVEM_REGISTRAR: {
        // O token vai para o cartão ANTES de entrar em uso.
        nuvem_cred_t cred;
        (void)nuvem_carrega(hal, &cred);

        if (!json_str(json, "device_token", cred.token, sizeof cred.token))
            return ERR_FORMATO;

        // O fato é o ACEITE do servidor: falha ao gravar é outro problema.
        e->tem_token      = true;
        e->recusa         = RECUSA_NADA;

        erro_t err = nuvem_grava(hal, &cred);
        if (err != OK) return err;

        hal->nuvem_credencial(cred.servidor, cred.token);

        // E emenda na próxima pergunta: de quem é este aparelho.
        e->nuvem_esperando = NUVEM_PAREADO;
    e->nuvem_desde_ms  = e->agora_ms;
        hal->nuvem_pede("/v1/parear/estado", NULL, NULL);
        return OK;
    }

    case NUVEM_PAREAR: {
        if (!json_str(json, "codigo", e->codigo, sizeof e->codigo))
            return ERR_FORMATO;

        // O prazo vem do servidor; o device só conta.
        int vale = 0;
        if (!json_int(json, "validade_s", &vale) || vale <= 0) vale = 300;

        uint32_t agora = hal->agora_ms ? hal->agora_ms() : 0;
        e->codigo_ate_ms = agora + (uint32_t)vale * 1000u;

        e->sinc = SINC_OCIOSO;
        return OK;
    }

    case NUVEM_PAREADO: {
        bool pareado = false;
        bool reconectar = false;
        (void)json_bool(json, "pareado", &pareado);
        (void)json_bool(json, "reconectar", &reconectar);
        // O aparelho que LIGA com a concessão vencida descobre aqui, na primeira
        // resposta depois do boot. `conta_avisada` nasce falsa: avisa uma vez por
        // vida ligada.
        if (pareado && reconectar) {
            if (!e->conta_avisada) {
                e->recusa = RECUSA_CONTA;
                e->conta_avisada = true;
            }
            e->conta_reconectar = true;
        }
        else estado_conta_ok(e);

        if (!pareado) {
            e->nome[0] = '\0';

            // Sem dono e com a tela de Conectar ABERTA: pede o código agora. Pedir sem
            // ninguém olhando gastaria um código de uso único.
            if (e->pilha[e->profundidade] == TELA_VINCULAR &&
                e->codigo[0] == '\0')
                (void)uso_parear(hal, e);
        }
        if (pareado) {
            char conta[64] = "";
            (void)json_str(json, "conta", conta, sizeof conta);

            // Outra pessoa, cartão limpo: a agenda da anterior seria vazamento.
            // Reconectar a MESMA conta não apaga nada.
            nuvem_cred_t cred;
            (void)nuvem_carrega(hal, &cred);
            if (conta[0] && cred.conta[0] && strcmp(cred.conta, conta) != 0) {
                (void)cartao_esquece_a_conta(hal);
                estado_invalida_cartao(e);
                e->pediu_tudo     = false;
            }
            if (conta[0] && strcmp(cred.conta, conta) != 0) {
                snprintf(cred.conta, sizeof cred.conta, "%s", conta);
                (void)nuvem_grava(hal, &cred);
            }

            snprintf(e->nome, sizeof e->nome, "%s", conta);
            e->codigo[0] = '\0';

            // E o dia desce AGORA, não no próximo ciclo.
            if (!e->conta_reconectar) (void)uso_sincronizar(hal, e);
        }
        return OK;
    }

    case NUVEM_PUSH: {
        // Só se confirma o que o servidor aceitou.
        bool ok = false;
        (void)json_bool(json, "ok", &ok);
        if (!ok) { e->sinc = SINC_ERRO; return ERR_REDE; }

        e->nuvem_em_voo = 0;
        e->sinc = SINC_OCIOSO;

        // O que sobrou vai no próximo ciclo; quem decide é o app.
        return OK;
    }

    case NUVEM_PULL: {
        erro_t err = aplica_pull(hal, e, json);

        // O carimbo da última sincronia ("há 2 min"), só com o lote APLICADO.
        if (err == OK) {
            e->sinc_ultima_ms = e->agora_ms;

            // Pull que passa prova que a concessão vale: o lembrete recomeça.
            estado_conta_ok(e);
        }
        e->sinc = err == OK ? SINC_OCIOSO : SINC_ERRO;

        // O FUSO vem de carona no pull, do Google Agenda: é o fuso em que os
        // eventos foram marcados, não o de onde o aparelho está. O servidor já
        // resolve horário de verão.
        int fuso = 0;
        if (json_int(json, "tz_min", &fuso))
            (void)uso_salvar_ajuste(hal, e, AJUSTE_FUSO_MIN, fuso);

        // A quota vem de carona no pull (RN-52): o aparelho só exibe.
        int v = 0;
        if (json_int(json, "quota_usados", &v)) e->quota.usados_s = v;
        if (json_int(json, "quota_limite", &v)) e->quota.limite_s = v;
        if (json_int(json, "quota_dias", &v))   e->quota.dias_pra_virar = (int16_t)v;

        aplica_nome(hal, e, json);
        return err;
    }

    case NUVEM_OLHAR: {
        erro_t err = aplica_olhar(hal, e, json);
        e->nuvem_esperando = NUVEM_NADA;
        e->sinc = SINC_OCIOSO;
        return err;
    }

    case NUVEM_CAPTURA: {
        // O que a IA entendeu é PROPOSTA: não toca o cartão até o OK (RN-16).
        resultado_t rs[RESULTADOS_MAX];
        char falou[FALA_MAX], nota[16];
        int n = contrato_captura(json, rs, RESULTADOS_MAX,
                                 falou, sizeof falou, nota, sizeof nota);

        // O item mora no dia em que VENCE, a mesma regra do pull ("amanhã" nascia
        // na pasta de hoje). A fala mora em `/TINTO/notas/`, apontada pelo `nota`.
        for (int i = 0; i < n; i++) {
            rs[i].item.dia = rs[i].item.vence.ano ? rs[i].item.vence : e->hoje;

            // E cada ação guarda de que nota veio.
            snprintf(rs[i].item.nota, sizeof rs[i].item.nota, "%s", nota);
        }

        // Zero ações não é erro: a frase crua vem e a tela diz "não entendi como
        // comando". Com o ANTES, para Conferir mostrar o que muda.
        (void)uso_propor_com_antes(hal, e, rs, n, falou);

        e->sinc            = SINC_OCIOSO;
        e->nuvem_esperando = NUVEM_NADA;
        return OK;
    }

    // ── o CATÁLOGO do acervo ─────────────────────────────────────
    case NUVEM_ACERVO: {
        e->sinc            = SINC_OCIOSO;
        e->nuvem_esperando = NUVEM_NADA;
        if (veio) {
            (void)uso_acervo_aplica(hal, e, json);
            // O cache da estante acompanha o cartão na mesma resposta. Com cursor, o
            // próximo lote já está em voo e recarregar apagaria as marcas da rodada: só
            // o último lote relê.
            if (e->nuvem_esperando != NUVEM_ACERVO)
                (void)uso_carregar_acervo(hal, e);
        }
        return OK;
    }

    // ── o TEXTO de uma obra ──────────────────────────────────────────────
    // Texto cru (tamanho e hash nos cabeçalhos); quem confere é
    // `uso_acervo_recebe`.
    case NUVEM_OBRA: {
        e->sinc            = SINC_OCIOSO;
        e->nuvem_esperando = NUVEM_NADA;
        // Só o sucesso passa aqui; a falha já soltou a obra lá em cima.
        erro_t err = uso_acervo_recebe(hal, e, json);
        if (err == OK && e->obra_baixando[0]) {
            obra_t o;
            if (acervo_le_meta(hal, e->obra_baixando, &o) == OK &&
                o.estado == OBRA_AQUI) {
                if (o.tem_capa)
                    (void)uso_acervo_capa_baixa(hal, e, o.id);
                else
                    e->obra_baixando[0] = '\0';
            }
        }
        return OK;
    }

    case NUVEM_CAPA: {
        e->sinc            = SINC_OCIOSO;
        e->nuvem_esperando = NUVEM_NADA;
        if (veio) return uso_acervo_capa_recebe(hal, e, json);
        e->obra_baixando[0] = '\0';
        e->capa_baixada = 0;
        e->capa_etapa = 0;
        return ERR_REDE;
    }

    case NUVEM_GESTO: {
        e->nuvem_esperando = NUVEM_NADA;

        // ── o item ganha o id de VERDADE ─────────────────────────────────────
        // O `n:1` provisório vira o definitivo (`g:`/`t:`), o mesmo que o pull vai
        // mandar; sem a troca, o compromisso ficava duas vezes no dia. Renomear, não
        // recriar: a pasta guarda duração, nota e edições.
        char id[40] = "";
        bool ok = false;
        bool ignorado = false;
        (void)json_bool(json, "ok", &ok);
        (void)json_bool(json, "ignorado", &ignorado);
        (void)json_str(json, "id", id, sizeof id);

        // O Google tinha uma edição posterior: o gesto fecha (não deve repetir),
        // mas a cópia otimista pode estar velha. O próximo pull pede a verdade
        // inteira uma vez.
        if (ignorado) {
            e->tem_google = false;
            e->pediu_tudo = false;
        }

        // O gesto que não pegou diz por quê na serial (o backend manda o motivo):
        // separa "reconecte a conta" de "servidor fora".
        if (!ok) {
            e->sinc = SINC_ERRO;

            // E o vidro fica sabendo (RECUSA_ACAO). Menos com a fala no ar: o Resultado
            // já diz "falhou" linha a linha.
            if (!e->esperando_resultado) e->recusa = RECUSA_ACAO;

            if (hal->registrar) {
                char motivo[80] = "";
                (void)json_str(json, "motivo", motivo, sizeof motivo);
                char msg[128];
                snprintf(msg, sizeof msg, "gesto recusado: %s",
                         motivo[0] ? motivo : "sem motivo");
                hal->registrar("nuvem", msg);
            }
        }

        // O arquivo sai da fila nos DOIS casos: a resposta do servidor é final, e
        // um gesto recusado para sempre travaria a fila. Resposta que NÃO chega é
        // outra coisa: o arquivo fica e sai de novo (a `operacao` torna seguro).
        if (e->gesto_em_voo[0]) {
            (void)cartao_apaga_gesto(hal, e->gesto_em_voo);
            e->gesto_em_voo[0] = '\0';
            if (e->gesto_n > 0) e->gesto_n--;
        }
        tira_lapide(e, e->gesto_id);

        // A fila continua: o próximo sai assim que este fechar.
        e->sinc = e->gesto_n > 0 ? SINC_ENVIANDO
                : (ok ? SINC_OCIOSO : SINC_ERRO);

        if (ok && id[0] && e->gesto_id[0] && strcmp(id, e->gesto_id) != 0) {
            item_t it;
            if (cartao_le_item(hal, e->gesto_dia, e->gesto_id, &it) == OK) {
                snprintf(it.id, sizeof it.id, "%s", id);
                if (cartao_grava_item(hal, e->gesto_dia, &it) == OK) {
                    (void)cartao_apaga_item(hal, e->gesto_dia, e->gesto_id);
                    estado_invalida_cartao(e);
                }
            }
        }

        e->gesto_id[0] = '\0';

        // O dia distante na tela é uma consulta, e o gesto acabou de mudar o
        // Google: repede.
        if (ok) uso_reconsultar_o_dia(e);

        (void)uso_gesto_pendente(hal, e);
        return OK;
    }

    // ── T-32 · a lista de agendas ────────────────────────────────────────
    case NUVEM_AGENDAS: {
        const char *lista = strstr(json, "\"agendas\"");
        e->n_agendas        = 0;
        e->agendas_buscando = false;
        e->agendas_querendo = false;
        e->nuvem_esperando  = NUVEM_NADA;
        e->sinc             = SINC_OCIOSO;

        // Sem a chave não é lista de agendas: ERR_FORMATO, distinto de vazio.
        if (!lista) return ERR_FORMATO;

        int total = 0;
        e->agendas_fora = 0;

        const char *pos = strchr(lista, '[');
        char objeto[160];
        while (pos && e->n_agendas < AGENDAS_MAX &&
               contrato_proximo(&pos, objeto, sizeof objeto)) {
            agenda_t *a = &e->agendas[e->n_agendas];
            memset(a, 0, sizeof *a);

            // Sem nome não há linha. O backend corta em 31, o que cabe aqui.
            if (!json_str(objeto, "t", a->nome, sizeof a->nome)) continue;

            (void)json_bool(objeto, "on", &a->ligada);

            int py = 0;
            if (json_int(objeto, "py", &py)) a->por_ano = (int16_t)py;

            e->n_agendas++;
        }

        // O servidor corta em AGENDAS_MAX e diz quantas a conta tem.
        if (json_int(json, "total", &total) && total > e->n_agendas)
            e->agendas_fora = (int8_t)(total - e->n_agendas);
        return OK;
    }

    case NUVEM_ESCOLHA: {
        // O interruptor vira AQUI, na confirmação, não no gesto.
        bool ok = false, on = false;
        (void)json_bool(json, "ok", &ok);

        if (ok && e->agenda_alvo >= 0 && e->agenda_alvo < e->n_agendas) {
            // O valor vem do servidor: se ele resolveu diferente, manda ele.
            (void)json_bool(json, "on", &on);
            e->agendas[e->agenda_alvo].ligada = on;
        }
        e->agenda_alvo     = -1;
        e->nuvem_esperando = NUVEM_NADA;
        e->sinc            = SINC_OCIOSO;

        // O backend esqueceu onde parou (`recomecar`) para TRAZER a agenda ligada:
        // pedir o delta agora a faz aparecer em segundos.
        if (ok) (void)uso_sincronizar(hal, e);
        return OK;
    }

    case NUVEM_NADA:
    default:
        return OK;
    }
}

erro_t uso_nuvem_resposta(const hal_t *hal, estado_t *e)
{
    if (!hal || !e || !hal->emprestar || !hal->devolver) return ERR_INTERNO;
    size_t capacidade = 0;
    char *json = hal->emprestar(128 * 1024 + 1, 16 * 1024 + 1, &capacidade);
    if (!json) return ERR_INTERNO;
    erro_t err = uso_nuvem_resposta_com(hal, e, json, capacidade);
    hal->devolver(json);
    return err;
}
