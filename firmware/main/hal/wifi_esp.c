// hal/wifi_esp.c — esp_wifi em modo estação, confinado ao HAL.
// A varredura é assíncrona: pede-se com `procurar`, e a lista chega como
// EV_WIFI_REDES.
#include "wifi_esp.h"
#include "hal.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/ip4_addr.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include <string.h>

void hal_esp_empurra(const evento_t *ev);

static const char *TAG = "wifi";

// Oito redes: o que cabe na tela; o resto é ruído de vizinho.
static rede_wifi_t achadas[REDES_MAX];
static int         n_achadas;
static bool        ligado;

// Associado sem IP é o estado que engana: está na rede e ainda não fala com
// ninguém. Por isso os dois ficam separados.
static bool associado;
static char ip_atual[16];

// 0 = nenhuma, 1 = senha, 2 = sem resposta. Espelha `wifi_falha_t` sem
// incluí-lo.
static int  falha_atual;

// RSSI em dBm (-30 excelente, -90 inútil) vira 0-100; as palavras são da
// vista.
static int8_t forca_de(int rssi)
{
    if (rssi >= -50) return 100;
    if (rssi <= -95) return 0;
    return (int8_t)((rssi + 95) * 100 / 45);
}

static void guarda_o_que_o_ar_tinha(void)
{
    uint16_t quantas = 0;
    esp_wifi_scan_get_ap_num(&quantas);
    if (quantas > REDES_MAX) quantas = REDES_MAX;

    // ESTÁTICO: oito registros são 640 bytes, e quem chama é a task `sys_evt`
    // do IDF, de pilha curta — estourava ao achar oito redes e o aparelho
    // reiniciava ao entrar em Wi-Fi. Uma varredura por vez, na mesma task.
    static wifi_ap_record_t brutas[REDES_MAX];
    uint16_t pedidas = quantas;
    if (esp_wifi_scan_get_ap_records(&pedidas, brutas) != ESP_OK) {
        n_achadas = 0;
        return;
    }

    n_achadas = 0;
    for (int i = 0; i < pedidas && n_achadas < REDES_MAX; i++) {
        // Sem nome é rede oculta: não se conecta pela lista.
        if (brutas[i].ssid[0] == '\0') continue;

        snprintf(achadas[n_achadas].nome, sizeof achadas[0].nome, "%s",
                 (const char *)brutas[i].ssid);
        achadas[n_achadas].forca = forca_de(brutas[i].rssi);
        achadas[n_achadas].salva = false;   // quem sabe da senha é o cartão

        // WIFI_AUTH_OPEN é o único modo sem senha; para esta tela os outros são
        // iguais.
        achadas[n_achadas].aberta = brutas[i].authmode == WIFI_AUTH_OPEN;
        n_achadas++;
    }

    ESP_LOGI(TAG, "varredura: %d redes", n_achadas);
}

static bool conectar_ao_subir;

static void evento_do_radio(void *arg, esp_event_base_t base,
                            int32_t id, void *dados)
{
    (void)arg; (void)dados;
    if (base != WIFI_EVENT) return;

    if (id == WIFI_EVENT_SCAN_DONE) {
        guarda_o_que_o_ar_tinha();
        evento_t ev = { .tipo = EV_WIFI_REDES };
        hal_esp_empurra(&ev);
        return;
    }

    if (id == WIFI_EVENT_STA_CONNECTED)    associado = true;
    // O STA terminou de subir. A conexão pedida antes disso (o boot pede)
    // acontece agora.
    if (id == WIFI_EVENT_STA_START && conectar_ao_subir) {
        conectar_ao_subir = false;
        esp_err_t e = esp_wifi_connect();
        ESP_LOGI(TAG, "STA subiu: conectando (%s)", esp_err_to_name(e));
        return;
    }

    if (id == WIFI_EVENT_STA_DISCONNECTED) {
        associado = false;
        ip_atual[0] = '\0';

        // ── por que caiu ──────────────────────────────────────────────────
        // O `reason` do evento é o único que separa senha errada de roteador
        // desligado.
        int razao = 0;
        if (dados) {
            const wifi_event_sta_disconnected_t *d = dados;
            razao = d->reason;
        }

        bool senha = razao == WIFI_REASON_AUTH_FAIL ||
                     razao == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
                     razao == WIFI_REASON_HANDSHAKE_TIMEOUT ||
                     razao == WIFI_REASON_MIC_FAILURE;

        falha_atual = senha ? 1 : (razao ? 2 : falha_atual);
        ESP_LOGW(TAG, "caiu: reason=%d%s", razao, senha ? " (senha)" : "");

        // Caiu: tenta de novo (o roteador pode ter reiniciado). Com a SENHA
        // recusada, insistir seria um laço infinito na tela: para e espera a pessoa.
        if (!senha) esp_wifi_connect();
    }

    evento_t ev = { .tipo = EV_WIFI_ESTADO };
    hal_esp_empurra(&ev);
}

static void evento_do_ip(void *arg, esp_event_base_t base,
                         int32_t id, void *dados)
{
    (void)arg; (void)base;
    if (id != IP_EVENT_STA_GOT_IP || !dados) return;

    const ip_event_got_ip_t *e = (const ip_event_got_ip_t *)dados;
    snprintf(ip_atual, sizeof ip_atual, IPSTR, IP2STR(&e->ip_info.ip));
    ESP_LOGI(TAG, "endereço %s", ip_atual);

    evento_t ev = { .tipo = EV_WIFI_ESTADO };
    hal_esp_empurra(&ev);
}

int hal_wifi_falha(void) { return falha_atual; }

erro_t hal_wifi_liga(void)
{
    // O esp_wifi guarda calibração e credencial na NVS; sem ela, o init falha
    // com um erro que não parece ser sobre isso.
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        e = nvs_flash_init();
    }
    if (e != ESP_OK) { ESP_LOGE(TAG, "nvs: %s", esp_err_to_name(e));
                       return ERR_INTERNO; }

    if (esp_netif_init() != ESP_OK) return ERR_INTERNO;
    if (esp_event_loop_create_default() != ESP_OK) return ERR_INTERNO;
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t conf = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&conf) != ESP_OK) return ERR_INTERNO;

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        evento_do_radio, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        evento_do_ip, NULL, NULL);

    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) return ERR_INTERNO;
    if (esp_wifi_start() != ESP_OK) return ERR_INTERNO;

    ligado = true;
    ESP_LOGI(TAG, "rádio em modo estação");
    return OK;
}

void hal_wifi_procurar(void)
{
    if (!ligado) return;

    // `false` torna a varredura assíncrona: volta na hora e o SCAN_DONE avisa.
    // Quem chama é o APP.
    esp_err_t e = esp_wifi_scan_start(NULL, false);

    // O rádio RECUSA varrer enquanto tenta conectar, e sem rede salva por perto
    // o driver fica nesse laço para sempre (ESP_ERR_WIFI_STATE). Desconectar
    // libera o rádio; quem abriu a lista veio trocar de rede.
    if (e == ESP_ERR_WIFI_STATE) {
        ESP_LOGI(TAG, "scan recusado (conectando): solto o rádio e repito");
        esp_wifi_disconnect();
        e = esp_wifi_scan_start(NULL, false);
    }

    if (e != ESP_OK) {
        ESP_LOGW(TAG, "scan: %s", esp_err_to_name(e));
        // Avisa mesmo assim: a tela ficaria em "procurando…" para sempre.
        evento_t ev = { .tipo = EV_WIFI_REDES };
        hal_esp_empurra(&ev);
    }
}

int hal_wifi_redes(rede_wifi_t *out, int max)
{
    int n = n_achadas < max ? n_achadas : max;
    for (int i = 0; i < n; i++) out[i] = achadas[i];
    return n;
}

// 0 desligada · 1 conectando · 2 ligada · 3 sem sinal: a ordem do `rede_t`,
// que o hal não conhece.
bool hal_wifi_tem_ip(void) { return ip_atual[0] != '\0'; }

int hal_wifi_estado(char *ip, size_t max, int *forca)
{
    if (ip && max) snprintf(ip, max, "%s", ip_atual);

    if (forca) {
        wifi_ap_record_t ap;
        *forca = (esp_wifi_sta_get_ap_info(&ap) == ESP_OK)
               ? forca_de(ap.rssi) : 0;
    }

    if (!ligado)        return 0;
    if (ip_atual[0])    return 2;
    if (associado)      return 1;
    return 0;
}

erro_t hal_wifi_conectar(const char *nome, const char *senha)
{
    // A falha velha morre com a tentativa nova.
    falha_atual = 0;

    if (!ligado || !nome || !nome[0]) return ERR_REDE;

    wifi_config_t conf;
    memset(&conf, 0, sizeof conf);
    snprintf((char *)conf.sta.ssid, sizeof conf.sta.ssid, "%s", nome);
    if (senha) snprintf((char *)conf.sta.password, sizeof conf.sta.password,
                        "%s", senha);

    if (esp_wifi_set_config(WIFI_IF_STA, &conf) != ESP_OK) return ERR_INTERNO;

    esp_err_t e = esp_wifi_connect();

    // `esp_wifi_start()` volta antes de o STA ficar pronto, e o connect do boot
    // falhava com ESP_ERR_WIFI_NOT_STARTED, sem nova tentativa. A intenção fica
    // guardada e o STA_START a executa.
    if (e == ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGI(TAG, "STA ainda subindo: %s entra na fila do STA_START",
                 nome);
        conectar_ao_subir = true;
        return OK;
    }

    if (e != ESP_OK) { ESP_LOGW(TAG, "connect: %s", esp_err_to_name(e));
                       return ERR_REDE; }

    ESP_LOGI(TAG, "conectando em %s", nome);
    return OK;
}

void hal_wifi_esquecer(void)
{
    // `esp_wifi_restore` devolve o rádio ao de fábrica. Desconectar antes evita
    // reconectar com o que acabou de ser apagado.
    esp_wifi_disconnect();
    esp_wifi_restore();
}
