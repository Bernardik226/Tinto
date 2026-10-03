#include "cartao.h"
#include "indice.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define META_MAX 512

static void caminho_dia(char *out, size_t max, data_t d)
{
    char t[DATA_TEXTO];
    data_para_texto(d, t, sizeof t);
    snprintf(out, max, CARTAO_RAIZ "/itens/%s", t);
}

// ── o id não cabe num nome de arquivo ───────────────────────────────
// Todo id do servidor tem `:` (`n:1`, `e:`, `t:`), que o FAT recusa — e a
// falha era silenciosa: a ação confirmada nunca chegava ao Google. A troca é
// REVERSÍVEL porque o nome da pasta É o id que volta no gesto. `%` é legal no
// FAT e não aparece em id do Google (base32hex e base64 de URL).
#define ID_DOIS_PONTOS '%'

static void nome_do_id(const char *id, char *out, size_t max)
{
    size_t i = 0;
    for (; id[i] && i < max - 1; i++)
        out[i] = id[i] == ':' ? ID_DOIS_PONTOS : id[i];
    out[i] = '\0';
}

void cartao_id_do_nome(const char *nome, char *out, size_t max)
{
    size_t i = 0;
    for (; nome[i] && i < max - 1; i++)
        out[i] = nome[i] == ID_DOIS_PONTOS ? ':' : nome[i];
    out[i] = '\0';
}

static void caminho_item(char *out, size_t max, data_t d, const char *id,
                         const char *arquivo)
{
    char t[DATA_TEXTO];
    data_para_texto(d, t, sizeof t);

    char nome[40];
    nome_do_id(id, nome, sizeof nome);
    snprintf(out, max, CARTAO_RAIZ "/itens/%s/%s/%s", t, nome, arquivo);
}

static void caminho_meta(char *out, size_t max, data_t d, const char *id)
{
    caminho_item(out, max, d, id, "meta.json");
}

erro_t cartao_prepara(const hal_t *hal)
{
    // RN-68: cartão sem /TINTO/ cria a árvore e segue, sem tocar no que existe
    // fora dela.
    char buf[64];
    if (hal->ler(CARTAO_RAIZ "/sistema/formato.json", buf, sizeof buf) == OK)
        return OK;

    char j[64];
    snprintf(j, sizeof j, "{\"v\":%d}", CARTAO_FORMATO);
    return hal->escrever(CARTAO_RAIZ "/sistema/formato.json", j);
}

// ── ler ──────────────────────────────────────────────────────────────
static erro_t meta_para_item(const char *json, data_t dia, const char *id, item_t *out)
{
    // RN-63: "v" desconhecido → o item é IGNORADO, nunca lido torto.
    int v = 0;
    if (!json_int(json, "v", &v) || v != CARTAO_FORMATO)
        return ERR_FORMATO;

    memset(out, 0, sizeof *out);
    snprintf(out->id, sizeof out->id, "%s", id);
    out->dia = dia;

    // RN-B8: campo faltando → valor padrão. É o normal de um formato que evolui.
    if (!json_str(json, "t", out->titulo, sizeof out->titulo))
        out->titulo[0] = '\0';
    if (!json_str(json, "h", out->hora, sizeof out->hora))
        out->hora[0] = '\0';
    if (!json_str(json, "f", out->fim, sizeof out->fim))
        out->fim[0] = '\0';
    if (!json_str(json, "l", out->local, sizeof out->local))
        out->local[0] = '\0';
    json_bool(json, "di", &out->dia_inteiro);
    json_bool(json, "r",  &out->repete);
    (void)json_str(json, "rr", out->regra, sizeof out->regra);

    int tipo = TIPO_NADA;
    json_int(json, "tp", &tipo);
    // RN-B9: valor fora do enum vira TIPO_NADA, um estado legítimo.
    out->tipo = (tipo >= TIPO_NADA && tipo <= TIPO_EVENTO)
              ? (tipo_t)tipo : TIPO_NADA;

    int origem = ORIGEM_AQUI;
    json_int(json, "o", &origem);
    out->origem = origem == ORIGEM_GOOGLE ? ORIGEM_GOOGLE : ORIGEM_AQUI;

    json_bool(json, "ok", &out->feita);
    json_bool(json, "m", &out->titulo_manual);

    int dur = 0;
    json_int(json, "s", &dur);
    out->dur_s = (int16_t)(dur > 0 ? dur : 0);

    int ln = 0, lk = 0;
    json_int(json, "n", &ln);
    json_int(json, "k", &lk);
    out->lista_n = (int16_t)(ln > 0 ? ln : 0);
    out->lista_k = (int16_t)(lk > 0 ? lk : 0);

    (void)json_str(json, "nt", out->nota, sizeof out->nota);
    (void)json_str(json, "ag", out->agenda, sizeof out->agenda);

    char venc[DATA_TEXTO];
    if (json_str(json, "d", venc, sizeof venc))
        data_de_texto(venc, &out->vence);   // inválida → fica ano 0

    char prazo[DATA_TEXTO];
    if (json_str(json, "p", prazo, sizeof prazo))
        data_de_texto(prazo, &out->prazo);

    // Meta antigo sem o campo: ano 0, que é "não concluída".
    char fdia[DATA_TEXTO];
    if (json_str(json, "c", fdia, sizeof fdia))
        data_de_texto(fdia, &out->feita_em);

    return OK;
}

void cartao_caminho_wav(data_t dia, const char *id, char *out, size_t max)
{
    caminho_item(out, max, dia, id, "audio.wav");
}

erro_t cartao_le_item(const hal_t *hal, data_t dia, const char *id, item_t *out)
{
    char caminho[128], json[META_MAX];
    caminho_meta(caminho, sizeof caminho, dia, id);

    erro_t e = hal->ler(caminho, json, sizeof json);
    if (e != OK) return e;

    return meta_para_item(json, dia, id, out);
}

// ── escrever, atomicamente ───────────────────────────────────────────
// O FatFs não cria caminho pelo meio: /TINTO/itens/<dia>/<id>/ precisa dos
// dois níveis. O provisionamento só vai até /TINTO/itens; dia e item nascem
// aqui. O hal_pc, com caminhos planos, não pegava isso.
static erro_t garante_pasta_do_item(const hal_t *hal, data_t dia,
                                    const char *id)
{
    char t[DATA_TEXTO];
    data_para_texto(dia, t, sizeof t);

    char pasta[128];
    snprintf(pasta, sizeof pasta, CARTAO_RAIZ "/itens/%s", t);
    erro_t e = hal->criar_diretorio(pasta);
    if (e != OK) return e;

    char nome[40];
    nome_do_id(id, nome, sizeof nome);
    snprintf(pasta, sizeof pasta, CARTAO_RAIZ "/itens/%s/%s", t, nome);
    return hal->criar_diretorio(pasta);
}

erro_t cartao_grava_item(const hal_t *hal, data_t dia, const item_t *it)
{
    erro_t pasta = garante_pasta_do_item(hal, dia, it->id);
    if (pasta != OK) return pasta;

    char json[META_MAX];
    char venc[DATA_TEXTO] = "";
    if (it->vence.ano) data_para_texto(it->vence, venc, sizeof venc);
    char prazo[DATA_TEXTO] = "";
    if (it->prazo.ano) data_para_texto(it->prazo, prazo, sizeof prazo);

    char fdia[DATA_TEXTO] = "";
    if (it->feita_em.ano) data_para_texto(it->feita_em, fdia, sizeof fdia);

    snprintf(json, sizeof json,
             "{\"v\":%d,\"t\":\"%s\",\"h\":\"%s\",\"f\":\"%s\","
             "\"l\":\"%s\",\"di\":%s,\"r\":%s,\"rr\":\"%s\","
             "\"tp\":%d,\"o\":%d,"
             "\"ok\":%s,\"m\":%s,\"s\":%d,\"d\":\"%s\",\"p\":\"%s\","
             "\"c\":\"%s\",\"n\":%d,\"k\":%d,\"nt\":\"%s\","
             "\"ag\":\"%s\"}",
             CARTAO_FORMATO, it->titulo, it->hora, it->fim, it->local,
             it->dia_inteiro ? "true" : "false",
             it->repete ? "true" : "false", it->regra, (int)it->tipo,
             (int)it->origem, it->feita ? "true" : "false",
             it->titulo_manual ? "true" : "false", (int)it->dur_s, venc, prazo,
             fdia, (int)it->lista_n, (int)it->lista_k, it->nota,
             it->agenda);

    char alvo[128], tmp[136];
    caminho_meta(alvo, sizeof alvo, dia, it->id);
    snprintf(tmp, sizeof tmp, "%s.tmp", alvo);

    // RN-64: .tmp + rename. Bateria acaba no meio da escrita, e nunca existe
    // meta.json pela metade: ou o antigo, ou o novo.
    erro_t e = hal->escrever(tmp, json);
    if (e != OK) return e;

    e = hal->renomear(tmp, alvo);
    if (e != OK) {
        hal->apagar(tmp);   // não deixa lixo se o rename falhou
        return e;
    }

    // O índice anda junto, aqui e não em quem chama, e só DEPOIS de o arquivo
    // estar no lugar: a RAM nunca afirma mais do que o cartão guarda.
    indice_poe(it, dia);
    return OK;
}

// RN-28: a transcrição crua fica sempre visível — é a única coisa
// literalmente da pessoa. Lida junto com o resumo.
erro_t cartao_le_texto(const hal_t *hal, data_t dia, const char *id,
                       texto_t *out)
{
    memset(out, 0, sizeof *out);

    char caminho[128];
    caminho_item(caminho, sizeof caminho, dia, id, "texto.txt");
    // RN-B7: trunca. Transcrição comprida não pode fazer a nota sumir.
    (void)hal->ler(caminho, out->transcricao, sizeof out->transcricao);

    char proc[640];
    caminho_item(caminho, sizeof caminho, dia, id, "proc.json");
    if (hal->ler(caminho, proc, sizeof proc) == OK)
        out->tem_resumo = json_str(proc, "r", out->resumo, sizeof out->resumo);

    return OK;
}

erro_t cartao_apaga_item(const hal_t *hal, data_t dia, const char *id)
{
    char caminho[128];
    caminho_meta(caminho, sizeof caminho, dia, id);
    erro_t e = hal->apagar(caminho);

    // A pasta do item vai junto: só o meta deixaria um diretório vazio por
    // item apagado, lido a cada boot.
    if (e == OK) {
        for (const char *arquivo[] = { "texto.txt", "proc.json", "audio.wav" },
             **p = arquivo; p < arquivo + 3; p++) {
            char resto[128];
            caminho_item(resto, sizeof resto, dia, id, *p);
            (void)hal->apagar(resto);
        }

        char pasta[128];
        caminho_item(pasta, sizeof pasta, dia, id, "");
        // `caminho_item` termina com barra quando o arquivo é vazio; o FAT recusa a
        // barra final no `unlink`.
        size_t n = strlen(pasta);
        if (n && pasta[n - 1] == '/') pasta[n - 1] = '\0';
        (void)hal->apagar(pasta);

        // E o DIA, se ficou vazio. `f_unlink` recusa diretório com gente dentro.
        char dir[96];
        caminho_dia(dir, sizeof dir, dia);
        (void)hal->apagar(dir);
    }

    // Sai do índice quando saiu do cartão, não antes. E só se o índice apontar
    // para ESTE dia: mudar de data grava a cópia nova antes de apagar a velha.
    if (e == OK) {
        data_t onde;
        if (indice_acha(id, &onde) && data_igual(onde, dia)) indice_tira(id);
    }
    return e;
}

// ── listar ───────────────────────────────────────────────────────────
// Nomes por volta (buffer de pilha); o cursor do hal busca a página seguinte
// até o diretório acabar.
#define PAGINA_NOMES 32

// Pagina até o fim: o FAT não devolve ordenado, e parar em 32 deixava
// quais dias sobreviviam ao acaso (uma rotina cria uma pasta por dia). É a
// única varredura do sistema, chamada pelo índice no boot.
static erro_t varre_dias(const hal_t *hal, data_t *out, int max, int *quantos)
{
    if (!quantos) return ERR_ARQUIVO;
    *quantos = 0;
    if (!hal || !hal->listar || !out || max <= 0) return ERR_ARQUIVO;

    // Os nomes dos diretórios SÃO as datas: sem catálogo para ficar velho.
    char nomes[PAGINA_NOMES][40];
    int achados = 0;

    for (int desde = 0; achados < max; desde += PAGINA_NOMES) {
        int n = 0;
        erro_t err = hal->listar(CARTAO_RAIZ "/itens", desde, nomes,
                                 PAGINA_NOMES, &n);
        if (err != OK) return err;
        if (n <= 0) break;

        for (int i = 0; i < n && achados < max; i++) {
            data_t d;
            if (data_de_texto(nomes[i], &d) == OK) out[achados++] = d;
        }

        // Página incompleta é fim de diretório.
        if (n < PAGINA_NOMES) break;
    }

    *quantos = achados;
    return OK;
}

erro_t cartao_lista_dias(const hal_t *hal, data_t *out, int max, int *quantos)
{
    return varre_dias(hal, out, max, quantos);
}


// RN-63 e RN-65, mesmo erro, regras diferentes:
//   · "v" desconhecido → versão FUTURA: ignora, não lê torto.
//   · sem "v" legível → ilegível: aparece com marcador de defeito; sumir
//     seria nota perdida sem aviso.
static bool versao_conhecida(const hal_t *hal, data_t dia, const char *id)
{
    char caminho[128], json[META_MAX];
    caminho_meta(caminho, sizeof caminho, dia, id);
    if (hal->ler(caminho, json, sizeof json) != OK) return false;

    int v = 0;
    return json_int(json, "v", &v);   // achou um número: é versão, e é outra
}

erro_t cartao_lista_itens(const hal_t *hal, data_t dia, int desde,
                          item_t *out, int max, int *quantos)
{
    if (!quantos) return ERR_ARQUIVO;
    *quantos = 0;
    if (desde < 0) return ERR_ARQUIVO;

    char dir[96];
    caminho_dia(dir, sizeof dir, dia);

    char nomes[32][40];
    int n = 0;
    erro_t err = hal->listar(dir, desde, nomes, 32, &n);

    // Dia sem pasta é dia VAZIO, não falha: aqui se resolve a diferença entre
    // armazenamento e significado. Deixar subir manteve os itens de hoje na RAM
    // ao abrir um amanhã vazio. ERR_SEM_CARTAO continua subindo (RN-A3).
    if (err == ERR_SEM_PASTA) return OK;
    if (err != OK) return err;

    int achados = 0;
    for (int i = 0; i < n && achados < max; i++) {
        // RN-62: sem meta.json o diretório não existe — cobre .Trash, System Volume
        // Information, .DS_Store sem lista negativa. O nome da pasta é o id com o
        // `:` trocado: desfazer antes de usar.
        char id[40];
        cartao_id_do_nome(nomes[i], id, sizeof id);

        item_t it;
        erro_t e = cartao_le_item(hal, dia, id, &it);

        if (e == OK) {
            out[achados++] = it;
        } else if (e == ERR_FORMATO && !versao_conhecida(hal, dia, id)) {
            // O meta existe e não se interpreta: entra com marcador, sem título.
            memset(&it, 0, sizeof it);
            snprintf(it.id, sizeof it.id, "%s", id);
            it.dia     = dia;
            it.tipo    = TIPO_NADA;
            it.defeito = true;
            out[achados++] = it;
        }
        // ERR_ARQUIVO é o normal: diretório sem meta.json não é item (RN-62).
    }
    *quantos = achados;
    return OK;
}

// Há algo do Google no cartão, em qualquer dia? Perguntar ao cache do dia
// fazia um hoje sem compromisso pedir a colheita inteira a cada minuto — e
// ela apaga o que veio de fora antes de reescrever: o vidro esvaziava e
// enchia sozinho.
bool cartao_tem_do_google(const hal_t *hal)
{
    (void)hal;

    // Do índice: era uma varredura completa a cada sincronização.
    for (int i = 0; i < indice_n(); i++) {
        const item_t *it = indice_em(i);
        if (it && it->id[1] == ':' &&
            (it->id[0] == 'g' || it->id[0] == 't' || it->id[0] == 'l'))
            return true;
    }
    return false;
}


// O item, em qualquer dia. A voz manda só o id, e a pasta não sai do
// vencimento (RN-26).
erro_t cartao_acha_item(const hal_t *hal, const char *id, item_t *out,
                        data_t *dia)
{
    if (!hal || !id || !out) return ERR_INTERNO;

    // O índice sabe onde cada item mora.
    data_t onde;
    const item_t *achado = indice_acha(id, &onde);
    if (!achado) return ERR_ARQUIVO;

    // Do CARTÃO: quem pede o item quer o gravado, com transcrição. O índice diz
    // ONDE; o cartão diz O QUÊ.
    erro_t e = cartao_le_item(hal, onde, id, out);
    if (e == OK && dia) *dia = onde;
    return e;
}

erro_t cartao_apaga_em_qualquer_dia(const hal_t *hal, const char *id)
{
    data_t onde;
    if (indice_acha(id, &onde)) (void)cartao_apaga_item(hal, onde, id);

    // Não achar não é falha: o que se queria era que ele não estivesse lá.
    return OK;
}

// Esquece o conteúdo de uma conta. Desconectar NÃO chama: só conectar uma
// conta DIFERENTE (a agenda da anterior seria vazamento). Ajustes, rede e
// credencial ficam: são do aparelho.
erro_t cartao_esquece_a_conta(const hal_t *hal)
{
    // De trás para a frente: `indice_tira` troca o último com o buraco.
    for (int i = indice_n() - 1; i >= 0; i--) {
        const item_t *it = indice_em(i);
        if (!it) continue;

        data_t dia = it->dia;
        char id[sizeof it->id];
        snprintf(id, sizeof id, "%s", it->id);

        char wav[128];
        cartao_caminho_wav(dia, id, wav, sizeof wav);
        (void)hal->apagar(wav);
        (void)cartao_apaga_item(hal, dia, id);
    }

    char nomes[32][40];
    int n = 0;
    if (hal->listar("/TINTO/notas", 0, nomes, 32, &n) == OK) {
        for (int i = 0; i < n; i++) {
            char caminho[96];
            snprintf(caminho, sizeof caminho, "/TINTO/notas/%s", nomes[i]);
            (void)hal->apagar(caminho);
        }
    }
    return OK;
}

// ── os ajustes ───────────────────────────────────────────────────────
#define CONFIG_CAMINHO CARTAO_RAIZ "/sistema/config.json"

// RN-B8: chave faltando → padrão (o arquivo é de uma versão anterior).
static const int16_t PADRAO[AJUSTE_QUANTOS] = {
    [AJUSTE_HORA24]      = 1,
    [AJUSTE_BLOQUEAR_MIN] = 3,
    [AJUSTE_VOZ_SEGURAR] = 1,
    [AJUSTE_HORA_REDE]   = 1,   // a hora vem da rede, que é o normal

    // UTC: fingir Brasília daria hora errada com cara de certa. O backend
    // resolve na primeira conversa.
    [AJUSTE_FUSO_MIN]    = 0,
};

// A chave de cada ajuste no config.json. TODO ajuste do enum precisa de
// linha aqui: o inicializador designado deixa NULL no que falta, e NULL num
// `%s` derruba o ESP32 (LoadProhibited). `cartao_chaves_completas()` faz o
// teste reprovar o build, e gravar pula a chave ausente.
static const char *CHAVE[AJUSTE_QUANTOS] = {
    [AJUSTE_HORA24]      = "h24",
    [AJUSTE_BLOQUEAR_MIN] = "blq",
    [AJUSTE_VOZ_SEGURAR] = "voz",
    [AJUSTE_HORA_REDE]   = "ntp",
    [AJUSTE_FUSO_MIN]    = "tz",
};

bool cartao_chaves_completas(void)
{
    for (int i = 0; i < AJUSTE_QUANTOS; i++)
        if (!CHAVE[i]) return false;
    return true;
}

erro_t cartao_le_config(const hal_t *hal, config_t *out)
{
    for (int i = 0; i < AJUSTE_QUANTOS; i++) out->valor[i] = PADRAO[i];

    char json[256];
    if (hal->ler(CONFIG_CAMINHO, json, sizeof json) != OK)
        return OK;   // sem arquivo é o primeiro boot, não é erro

    for (int i = 0; i < AJUSTE_QUANTOS; i++) {
        int v = 0;
        if (CHAVE[i] && json_int(json, CHAVE[i], &v)) out->valor[i] = (int16_t)v;
    }
    return OK;
}

erro_t cartao_grava_config(const hal_t *hal, const config_t *c)
{
    char json[256];
    int n = snprintf(json, sizeof json, "{");
    bool primeiro = true;
    for (int i = 0; i < AJUSTE_QUANTOS; i++) {
        // Chave ausente pula (ver CHAVE[]).
        if (!CHAVE[i]) continue;
        n += snprintf(json + n, sizeof json - (size_t)n, "%s\"%s\":%d",
                      primeiro ? "" : ",", CHAVE[i], (int)c->valor[i]);
        primeiro = false;
    }
    snprintf(json + n, sizeof json - (size_t)n, "}");

    // RN-64: .tmp + rename, como todo JSON do cartão.
    erro_t e = hal->escrever(CONFIG_CAMINHO ".tmp", json);
    if (e != OK) return e;

    e = hal->renomear(CONFIG_CAMINHO ".tmp", CONFIG_CAMINHO);
    if (e != OK) { hal->apagar(CONFIG_CAMINHO ".tmp"); return e; }
    return OK;
}

// ── a transcrição da nota ────────────────────────────────────────────
static void caminho_da_nota(char *out, size_t max, const char *nota)
{
    // A mesma troca do `:` dos itens: uma regra só para o mesmo problema.
    char nome[40];
    nome_do_id(nota, nome, sizeof nome);
    snprintf(out, max, "/TINTO/notas/%s.txt", nome);
}

erro_t cartao_grava_transcricao(const hal_t *hal, const char *nota,
                                const char *texto)
{
    if (!hal || !nota || !nota[0] || !texto) return ERR_INTERNO;

    (void)hal->criar_diretorio("/TINTO/notas");

    char caminho[64];
    caminho_da_nota(caminho, sizeof caminho, nota);
    return hal->escrever(caminho, texto);
}

// ── a fila de gestos ────────────────────────────────────────────────
// `/TINTO/gestos/000007`: o nome É a ordem (ver `cartao.h`).
#define GESTOS_DIR CARTAO_RAIZ "/gestos"

static void caminho_gesto(char *out, size_t max, const char *nome)
{
    snprintf(out, max, GESTOS_DIR "/%s", nome);
}

erro_t cartao_grava_gesto(const hal_t *hal, unsigned seq, const char *corpo,
                          const char *operacao, data_t dia)
{
    if (!hal || !hal->escrever || !corpo || !operacao) return ERR_INTERNO;

    (void)hal->criar_diretorio(GESTOS_DIR);

    char nome[16];
    snprintf(nome, sizeof nome, "%06u", seq);

    char caminho[64];
    caminho_gesto(caminho, sizeof caminho, nome);

    // Três linhas: a operação, a PASTA do item e o corpo. A pasta não sai do
    // corpo — o `d` é o vencimento, e o item mora no dia em que foi falado
    // (RN-26); sem ela, trocar o id provisório procuraria no dia errado.
    char t[DATA_TEXTO] = "";
    if (dia.ano) data_para_texto(dia, t, sizeof t);

    char tudo[512];
    snprintf(tudo, sizeof tudo, "%s\n%s\n%s", operacao, t, corpo);
    return hal->escrever(caminho, tudo);
}

erro_t cartao_lista_gestos(const hal_t *hal, char nomes[][40], int max,
                           int *quantos)
{
    if (quantos) *quantos = 0;
    if (!hal || !hal->listar || !nomes || max <= 0) return ERR_INTERNO;

    int n = 0;
    erro_t err = hal->listar(GESTOS_DIR, 0, nomes, max, &n);
    // Pasta que não existe é fila VAZIA.
    if (err != OK) return OK;

    // Ordena por nome: o FAT devolve em ordem de criação até reusar um buraco.
    // `memmove`, não `snprintf`: origem e destino se sobrepõem, e o compilador
    // da placa recusa (o do PC deixa passar).
    for (int i = 1; i < n; i++) {
        char chave[40];
        memcpy(chave, nomes[i], sizeof chave);
        int j = i - 1;
        while (j >= 0 && strcmp(nomes[j], chave) > 0) {
            memmove(nomes[j + 1], nomes[j], 40);
            j--;
        }
        memcpy(nomes[j + 1], chave, sizeof chave);
    }

    if (quantos) *quantos = n;
    return OK;
}

erro_t cartao_le_gesto(const hal_t *hal, const char *nome,
                       char *corpo, size_t max_corpo,
                       char *operacao, size_t max_op, data_t *dia)
{
    if (corpo && max_corpo) corpo[0] = '\0';
    if (operacao && max_op) operacao[0] = '\0';
    if (dia) memset(dia, 0, sizeof *dia);
    if (!hal || !hal->ler || !nome || !corpo) return ERR_INTERNO;

    char caminho[64];
    caminho_gesto(caminho, sizeof caminho, nome);

    char tudo[512];
    erro_t err = hal->ler(caminho, tudo, sizeof tudo);
    if (err != OK) return err;

    char *fim_op = strchr(tudo, '\n');
    if (!fim_op) return ERR_FORMATO;
    *fim_op = '\0';

    char *fim_dia = strchr(fim_op + 1, '\n');
    if (!fim_dia) return ERR_FORMATO;
    *fim_dia = '\0';

    if (operacao && max_op) snprintf(operacao, max_op, "%s", tudo);
    if (dia && fim_op[1]) (void)data_de_texto(fim_op + 1, dia);
    snprintf(corpo, max_corpo, "%s", fim_dia + 1);
    return OK;
}

erro_t cartao_apaga_gesto(const hal_t *hal, const char *nome)
{
    if (!hal || !hal->apagar || !nome) return ERR_INTERNO;

    char caminho[64];
    caminho_gesto(caminho, sizeof caminho, nome);
    return hal->apagar(caminho);
}

unsigned cartao_ultimo_gesto(const hal_t *hal)
{
    char nomes[GESTOS_MAX][40];
    int n = 0;
    if (cartao_lista_gestos(hal, nomes, GESTOS_MAX, &n) != OK || n <= 0)
        return 0;

    // Continuar do último, não do zero: o gesto novo não entra atrás dos que
    // esperam.
    unsigned maior = 0;
    for (int i = 0; i < n; i++) {
        unsigned v = (unsigned)strtoul(nomes[i], NULL, 10);
        if (v > maior) maior = v;
    }
    return maior;
}

erro_t cartao_le_transcricao(const hal_t *hal, const char *nota,
                             char *out, size_t max)
{
    if (out && max) out[0] = '\0';
    if (!hal || !nota || !nota[0] || !out) return ERR_INTERNO;

    char caminho[64];
    caminho_da_nota(caminho, sizeof caminho, nota);

    return hal->ler(caminho, out, max);
}
