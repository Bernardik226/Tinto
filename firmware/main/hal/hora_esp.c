#include "hora_esp.h"

#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_netif.h"

// RN-6G: a hora vem do NTP só quando o ajuste da pessoa manda.
//
// Defensivo porque ligar a hora pela rede já reiniciou o aparelho. Três
// caminhos abortam sem devolver erro:
//   1. `esp_netif_sntp_init` duas vezes sem `deinit` no meio;
//   2. `esp_netif_sntp_start` sem init;
//   3. init antes de haver IP (REDE_LIGADA chega antes dele).
// Cada passo loga ANTES de agir: a última linha da serial diz onde parou.
static const char *TAG = "hora";
static bool de_pe;

static void hora_chegou(struct timeval *tv)
{
    (void)tv;
    ESP_LOGI(TAG, "relógio acertado pela rede");
    hal_esp_empurra_hora();
}

void hal_hora_da_rede(void)
{
    if (!hal_wifi_tem_ip()) {
        ESP_LOGI(TAG, "NTP adiado: sem IP ainda");
        return;
    }

    if (de_pe) {
        // Já de pé: só pede de novo. `restart` não aborta com consulta em voo.
        ESP_LOGI(TAG, "NTP: pedindo de novo");
        esp_err_t err = esp_netif_sntp_start();
        if (err != ESP_OK)
            ESP_LOGW(TAG, "NTP start devolveu %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "NTP: subindo o cliente");
    esp_sntp_config_t conf = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    conf.start    = true;
    conf.sync_cb  = hora_chegou;

    esp_err_t err = esp_netif_sntp_init(&conf);
    if (err != ESP_OK) {
        // ESP_ERR_INVALID_STATE = já havia cliente deste boot. Aceitar como "de
        // pé" é o que impede o caso 1.
        ESP_LOGW(TAG, "NTP init devolveu %s", esp_err_to_name(err));
        de_pe = (err == ESP_ERR_INVALID_STATE);
        return;
    }

    de_pe = true;
    ESP_LOGI(TAG, "NTP de pé");
}

// Desligar o interruptor derruba o cliente; vivo, o próximo `init`
// cairia no caso 1.
void hal_hora_da_rede_para(void)
{
    if (!de_pe) return;
    ESP_LOGI(TAG, "NTP: descendo o cliente");
    esp_netif_sntp_deinit();
    de_pe = false;
}
