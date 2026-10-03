#include "nuvem_esp.h"
#include <stdlib.h>
#include "memoria_hal.h"
#include "rota.h"
#include "../nucleo/prazos.h"

#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "nuvem";

#define ROTA_MAX     NUVEM_ROTA_MAX
#define CORPO_MAX    2048
#define RESP_PULL_MAX (32 * 1024 + 1)
#define RESP_JA_MAX   (128 * 1024 + 1)
#define SERVIDOR_MAX 80
#define TOKEN_MAX    48
#define OPERACAO_MAX 80
// O pedaço do upload: emprestado da PSRAM, não reservado. Cada volta do laço
// é leitura do cartão + TLS + socket com custo fixo, então pedaço grande é
// upload rápido. Como `static` ele já prendeu RAM interna e tirou o DMA do SD
// (o aparelho abriu a tela de reparo); daqui ele não precisa de DMA.
#define PEDACO_MAX   (32 * 1024)
#define PEDACO_MIN   (2 * 1024)

// A fronteira do multipart: só precisa não aparecer dentro do WAV.
#define LIMITE "----tinto7f3a91"

typedef struct {
    char rota[ROTA_MAX];
    char corpo[CORPO_MAX];
    char audio[128];
    char operacao[OPERACAO_MAX];
    bool tem_corpo;
    bool tem_audio;
} pedido_t;

// ── DUAS linhas, divididas por QUEM ESPERA ───────────────────────────
// O pull fica pendurado no long polling; com uma linha só, todo gesto
// esperava ele fechar. Cada linha tem fila, buffers, trava e task próprias,
// e a resposta diz de onde veio. LINHA_JA: o que nasce de um gesto e alguém
// olha. LINHA_PULL: o delta, que ninguém está esperando ver.
#define LINHA_JA   0
#define LINHA_PULL 1
#define LINHAS     2

typedef struct {
    const char       *nome;
    QueueHandle_t     fila;
    SemaphoreHandle_t trava;

    // Buffers na PSRAM, alocados uma vez no boot. Em `.bss` eles tiravam da RAM
    // interna o que o AES por hardware precisa a cada handshake, e as duas
    // conexões falhavam com `esp-aes: Failed to allocate memory`.
    //
    // O de trabalho: a task escreve nele durante a transferência, com a trava.
    char  *resposta;
    size_t capacidade;
    size_t escrito;
    bool   truncada;
    bool   resposta_vale;
    int    resposta_codigo;

    // O de ENTREGA: copiado uma vez no fim, e o único que a UI lê, sem trava.
    // Lendo o de trabalho, a UI disputava com o pedido seguinte e uma resposta
    // boa voltava vazia — foi assim que o aparelho "perdeu" a conta vinculada.
    char  *entrega;
    int    entrega_codigo;
    bool   entrega_vale;
    bool   entrega_cheia;

    // O conteúdo de uma obra chega em vários pedidos seguidos; o handle fica
    // vivo entre eles para reaproveitar a conexão TLS.
    esp_http_client_handle_t cliente_acervo;
} linha_t;

static linha_t L[LINHAS];

// De qual linha é a resposta que a UI vai ler. As duas podem terminar
// juntas.
static QueueHandle_t ordem;

// O pull é o único que fica pendurado; o resto nasce de um gesto.
static int linha_da_rota(const char *rota)
{
    return (rota && strncmp(rota, "/v1/pull", 8) == 0) ? LINHA_PULL : LINHA_JA;
}


// O endereço e o token, entregues uma vez por `uso_nuvem_ligar`. Quem monta
// a requisição é este arquivo.
static char servidor[SERVIDOR_MAX];
static char token[TOKEN_MAX];

void hal_nuvem_credencial(const char *endereco, const char *segredo)
{
    snprintf(servidor, sizeof servidor, "%s", endereco ? endereco : "");
    snprintf(token,    sizeof token,    "%s", segredo  ? segredo  : "");
}

// O corpo chega em pedaços, acumulados. Sem isso, sobrava o último pedaço:
// JSON pela metade.
static esp_err_t junta(esp_http_client_event_t *ev)
{
    // Qual linha, pelo `user_data`: com duas tasks, um acumulador estático
    // misturaria duas respostas.
    linha_t *l = (linha_t *)ev->user_data;
    if (!l) return ESP_OK;

    if (ev->event_id == HTTP_EVENT_ON_CONNECTED) {
        l->escrito = 0;
        l->truncada = false;
        return ESP_OK;
    }
    if (ev->event_id != HTTP_EVENT_ON_DATA)      return ESP_OK;

    size_t cabe = l->capacidade - 1 - l->escrito;
    size_t n = (size_t)ev->data_len < cabe ? (size_t)ev->data_len : cabe;
    if (n) {
        memcpy(l->resposta + l->escrito, ev->data, n);
        l->escrito += n;
        l->resposta[l->escrito] = '\0';
    }
    if ((size_t)ev->data_len > n) l->truncada = true;
    return ESP_OK;
}

static esp_http_client_handle_t abre(linha_t *l, const pedido_t *p,
                                    char *url, size_t max)
{
    snprintf(url, max, "%s%s", servidor, p->rota);

    // A URL inteira, uma vez por pedido: `ESP_ERR_HTTP_CONNECT` não diz PARA
    // ONDE tentou ir, e o endereço vem do cartão.
    ESP_LOGI(TAG, "→ %s", url);

    esp_http_client_config_t conf = {
        .url = url,
        .event_handler = junta,
        .user_data     = l,
        // O servidor segura um pull vazio por até 25 s; desistir antes transforma a
        // espera em erro de rede. A folga cobre o handshake TLS e a resposta.
        .timeout_ms = (int)HTTP_PRAZO_MS,
        // O bundle de CAs raiz, não a folha: pinar a folha derrubaria todo aparelho
        // no dia em que o provedor renovar o certificado.
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t c = esp_http_client_init(&conf);
    if (!c) return NULL;

    // Toda rota do device exige o token, menos a de registro, onde ele nasce.
    if (token[0]) {
        char cabecalho[TOKEN_MAX + 16];
        snprintf(cabecalho, sizeof cabecalho, "Bearer %s", token);
        esp_http_client_set_header(c, "Authorization", cabecalho);
    }

    // O id da operação torna o retry seguro: a repetição recebe a mesma
    // resposta sem reexecutar. Sem ele, uma resposta perdida vira dois eventos.
    if (p->operacao[0])
        esp_http_client_set_header(c, "operacao", p->operacao);

    return c;
}

// ── o áudio, em pedaços ──────────────────────────────────────────────
// O WAV não passa inteiro pela RAM: vai em pedaços, e o `Content-Length` sai
// do tamanho que o `f_open` já conhece.
static void manda_audio(linha_t *l, const pedido_t *p,
                        esp_http_client_handle_t c)
{
    char cabeca[256], rabo[64];
    int n_cabeca = snprintf(cabeca, sizeof cabeca,
        "--" LIMITE "\r\n"
        "Content-Disposition: form-data; name=\"audio\"; "
        "filename=\"fala.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n");
    int n_rabo = snprintf(rabo, sizeof rabo, "\r\n--" LIMITE "--\r\n");

    if (hal_nuvem_audio_abre(p->audio) != 0) {
        ESP_LOGW(TAG, "%s: não abriu %s", p->rota, p->audio);
        return;
    }

    unsigned bytes = hal_nuvem_audio_tamanho();
    int total = n_cabeca + (int)bytes + n_rabo;

    esp_http_client_set_method(c, HTTP_METHOD_POST);
    esp_http_client_set_header(c, "Content-Type",
                               "multipart/form-data; boundary=" LIMITE);

    if (esp_http_client_open(c, total) != ESP_OK) {
        hal_nuvem_audio_fecha();
        ESP_LOGW(TAG, "%s: não abriu a conexão", p->rota);
        return;
    }

    // Separa o tempo de REDE do tempo de IA (Whisper + Haiku): sem isso "está
    // lento" não diz o que consertar.
    int64_t t0 = esp_timer_get_time();

    esp_http_client_write(c, cabeca, n_cabeca);

    // Do maior para o menor: upload mais lento é melhor que upload que não
    // acontece.
    size_t tam = 0;
    char  *pedaco = mem_emprestada(PEDACO_MAX, PEDACO_MIN, &tam);
    if (!pedaco) {
        hal_nuvem_audio_fecha();
        esp_http_client_close(c);
        ESP_LOGW(TAG, "%s: sem memória para o envio", p->rota);
        return;
    }

    unsigned enviados = 0;
    for (;;) {
        int lidos = hal_nuvem_audio_le(pedaco, tam);
        if (lidos <= 0) break;
        if (esp_http_client_write(c, pedaco, lidos) < 0) break;
        enviados += (unsigned)lidos;
    }
    hal_nuvem_audio_fecha();
    mem_devolve(pedaco);

    esp_http_client_write(c, rabo, n_rabo);

    int64_t t1 = esp_timer_get_time();
    unsigned subida_ms = (unsigned)((t1 - t0) / 1000);
    ESP_LOGI(TAG, "upload: %u KB em %u ms (%u KB/s)",
             enviados / 1024u, subida_ms,
             subida_ms ? (enviados * 1000u) / (subida_ms * 1024u) : 0u);

    // Com `open`/`write` o handler não recebe o corpo: a resposta é montada
    // aqui.
    esp_http_client_fetch_headers(c);
    ESP_LOGI(TAG, "servidor: %u ms pensando",
             (unsigned)((esp_timer_get_time() - t1) / 1000));
    ESP_LOGI(TAG, "REDE stack min %u · DRAM min/bloco %u/%u",
             (unsigned)(uxTaskGetStackHighWaterMark(NULL) *
                        sizeof(StackType_t)),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    int status = esp_http_client_get_status_code(c);

    int lidos = esp_http_client_read(c, l->resposta, l->capacidade - 1);
    l->escrito = lidos > 0 ? (size_t)lidos : 0;
    l->resposta[l->escrito] = '\0';
    int64_t esperado = esp_http_client_get_content_length(c);
    l->truncada = esperado >= (int64_t)l->capacidade;

    // O CÓDIGO, não só o "deu certo": sem ele, a quota estourada (402) das
    // rotas de áudio virava "offline · conecte a uma rede".
    l->resposta_codigo = status;
    l->resposta_vale = status >= 200 && status < 300 && !l->truncada;
    if (!l->resposta_vale)
        ESP_LOGW(TAG, "%s: status %d", p->rota, status);
}

static void faz_o_pedido(linha_t *l, const pedido_t *p)
{
    if (!servidor[0]) {
        ESP_LOGW(TAG, "%s: sem servidor configurado", p->rota);
        return;
    }

    bool bloco_acervo = strstr(p->rota, "/conteudo/bloco?") != NULL;
    char url[160];
    esp_http_client_handle_t c = NULL;

    snprintf(url, sizeof url, "%s%s", servidor, p->rota);
    if (bloco_acervo && l->cliente_acervo) {
        c = l->cliente_acervo;
        esp_http_client_set_url(c, url);
        esp_http_client_set_method(c, HTTP_METHOD_GET);
        esp_http_client_set_post_field(c, NULL, 0);
        ESP_LOGI(TAG, "→ %s (conexao mantida)", url);
    } else {
        // Qualquer outra operação encerra a sessão do livro: não segura socket
        // depois que a pessoa seguiu.
        if (l->cliente_acervo) {
            esp_http_client_cleanup(l->cliente_acervo);
            l->cliente_acervo = NULL;
        }
        c = abre(l, p, url, sizeof url);
        if (bloco_acervo) l->cliente_acervo = c;
    }
    if (!c) return;

    xSemaphoreTake(l->trava, portMAX_DELAY);
    l->resposta[0] = '\0';
    l->escrito = 0;
    l->truncada = false;
    l->resposta_vale = false;
    l->resposta_codigo = 0;

    if (p->tem_audio) {
        manda_audio(l, p, c);
    } else {
        if (p->tem_corpo) {
            esp_http_client_set_method(c, HTTP_METHOD_POST);
            esp_http_client_set_header(c, "Content-Type", "application/json");
            esp_http_client_set_post_field(c, p->corpo, (int)strlen(p->corpo));
        }

        int64_t inicio_http = esp_timer_get_time();
        esp_err_t e = esp_http_client_perform(c);
        if (bloco_acervo) {
            unsigned levou_ms = (unsigned)((esp_timer_get_time() - inicio_http) /
                                           1000);
            ESP_LOGI(TAG, "download: %u KB em %u ms",
                     (unsigned)(l->escrito / 1024u), levou_ms);
        }
        int status = esp_http_client_get_status_code(c);
        l->resposta_codigo = status;
        l->resposta_vale = (e == ESP_OK && status >= 200 && status < 300 &&
                            !l->truncada);

        if (!l->resposta_vale) {
            // A heap livre junto do erro: `ESP_ERR_HTTP_CONNECT` sozinho não separa
            // rede fora do ar de memória curta.
            ESP_LOGW(TAG, "%s: %s (status %d) · DRAM interna %u · stack %u",
                     p->rota, esp_err_to_name(e), status,
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                     (unsigned)uxTaskGetStackHighWaterMark(NULL));
        }
    }
    // A entrega, copiada antes de soltar a trava (ver `entrega`).
    l->entrega_codigo = l->resposta_codigo;
    l->entrega_vale   = l->resposta_vale;
    if (l->entrega_vale)
        memcpy(l->entrega, l->resposta, l->escrito + 1);
    l->entrega_cheia = true;
    xSemaphoreGive(l->trava);

    // O próximo bloco da obra reusa este handle; se o servidor fechou o
    // keep-alive, o cliente reconecta sozinho.
    if (!bloco_acervo) esp_http_client_cleanup(c);

    // A ordem importa: as duas linhas podem terminar juntas.
    int qual = (int)(l - L);
    xQueueSend(ordem, &qual, 0);
    hal_esp_empurra_resposta();
}

static void task_rede(void *arg)
{
    linha_t *l = (linha_t *)arg;
    for (;;) {
        pedido_t p;
        if (xQueueReceive(l->fila, &p, portMAX_DELAY) == pdTRUE)
            faz_o_pedido(l, &p);
    }
}

void hal_nuvem_liga(void)
{
    static const char *NOMES[LINHAS] = { "REDE", "REDE_PULL" };

    ordem = xQueueCreate(LINHAS, sizeof(int));
    if (!ordem) {
        ESP_LOGE(TAG, "sem fila de respostas");
        return;
    }

    for (int i = 0; i < LINHAS; i++) {
        L[i].nome  = NOMES[i];
        L[i].capacidade = i == LINHA_PULL ? RESP_PULL_MAX : RESP_JA_MAX;
        // A fila do PULL tem um lugar: dois pulls seriam dois deltas para o mesmo
        // instante. A de gestos tem quatro (uma fala gera até três).
        L[i].fila  = xQueueCreate(i == LINHA_PULL ? 1 : 4, sizeof(pedido_t));
        L[i].trava = xSemaphoreCreateMutex();

        // PSRAM: a RAM interna é do AES por hardware, e é ela que falta primeiro.
        L[i].resposta = heap_caps_malloc(L[i].capacidade, MALLOC_CAP_SPIRAM);
        L[i].entrega  = heap_caps_malloc(L[i].capacidade, MALLOC_CAP_SPIRAM);

        // Sem PSRAM a linha não existe: melhor que escrever em NULL.
        if (!L[i].fila || !L[i].trava ||
            !L[i].resposta || !L[i].entrega) {
            ESP_LOGE(TAG, "%s: sem recurso para iniciar", NOMES[i]);
            free(L[i].resposta);
            free(L[i].entrega);
            if (L[i].fila) vQueueDelete(L[i].fila);
            if (L[i].trava) vSemaphoreDelete(L[i].trava);
            L[i].fila = NULL;
            L[i].trava = NULL;
            L[i].resposta = NULL;
            L[i].entrega = NULL;
            continue;
        }
        L[i].resposta[0] = '\0';
        L[i].entrega[0]  = '\0';
    }

    // Prioridade BAIXA: tudo da rede é assíncrono, e a tela não espera HTTP.
    //
    // Pilha de 16 KB por causa do HANDSHAKE: verificar a cadeia do bundle de CAs
    // aloca na pilha. Com 8 KB toda conexão falhava com `ESP_ERR_HTTP_CONNECT`,
    // que parece rede, DNS ou certificado. Se voltar, quem decide o número é o
    // `uxTaskGetStackHighWaterMark`, nunca chute.
    //
    // No CPU1: o driver de Wi-Fi está fixo no CPU0, e o APP não tem núcleo fixo.
    // Com o TLS livre para cair no núcleo do APP, ele o preemptava (~700 ms por
    // passo sem outro motivo).
    for (int i = 0; i < LINHAS; i++) {
        if (!L[i].fila) continue;
        if (xTaskCreatePinnedToCore(task_rede, L[i].nome, 16384, &L[i], 3,
                                    NULL, 1) != pdPASS) {
            ESP_LOGE(TAG, "%s: sem task de rede", L[i].nome);
            vQueueDelete(L[i].fila);
            vSemaphoreDelete(L[i].trava);
            free(L[i].resposta);
            free(L[i].entrega);
            L[i].fila = NULL;
            L[i].trava = NULL;
            L[i].resposta = NULL;
            L[i].entrega = NULL;
        }
    }
}

static void enfileira(const pedido_t *p)
{
    linha_t *l = &L[linha_da_rota(p->rota)];
    if (!l->fila) return;

    // Fila cheia: descarta o pedido novo em vez de bloquear a task APP.
    if (xQueueSend(l->fila, p, 0) != pdTRUE)
        ESP_LOGW(TAG, "fila %s cheia: %s ficou pra depois", l->nome, p->rota);
}

void hal_nuvem_pede(const char *rota, const char *corpo, const char *operacao)
{
    if (!rota || !L[linha_da_rota(rota)].fila) return;

    pedido_t p;
    memset(&p, 0, sizeof p);
    snprintf(p.rota, sizeof p.rota, "%s", rota);
    if (operacao) snprintf(p.operacao, sizeof p.operacao, "%s", operacao);
    if (corpo) {
        snprintf(p.corpo, sizeof p.corpo, "%s", corpo);
        p.tem_corpo = true;
    }
    enfileira(&p);
}

void hal_nuvem_pede_audio(const char *rota, const char *caminho,
                          const char *operacao)
{
    if (!L[LINHA_JA].fila || !rota || !caminho) return;

    pedido_t p;
    memset(&p, 0, sizeof p);
    snprintf(p.rota,  sizeof p.rota,  "%s", rota);
    snprintf(p.audio, sizeof p.audio, "%s", caminho);
    if (operacao) snprintf(p.operacao, sizeof p.operacao, "%s", operacao);
    p.tem_audio = true;
    enfileira(&p);
}

// A UI nunca espera a rede: lê a entrega, sem trava. Quando a leitura
// pegava a trava com `portMAX_DELAY`, o app congelava a duração inteira de
// um long poll.
//
// Qual linha a UI está lendo: tirada da fila de ordem ao perguntar, e mantida
// até a próxima leitura — `hal_nuvem_codigo` vem depois de
// `hal_nuvem_resposta` e tem de falar da mesma.
static int lendo = LINHA_JA;

bool hal_nuvem_resposta(char *out, size_t max)
{
    if (!out || !max || !ordem) return false;

    int qual;
    if (xQueueReceive(ordem, &qual, 0) == pdTRUE) lendo = qual;

    linha_t *l = &L[lendo];
    l->entrega_cheia = false;
    if (!l->entrega_vale) return false;
    snprintf(out, max, "%s", l->entrega);
    return true;
}

int hal_nuvem_codigo(void) { return L[lendo].entrega_codigo; }
int hal_nuvem_linha(void)  { return lendo; }
