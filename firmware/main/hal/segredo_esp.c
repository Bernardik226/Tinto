// hal/segredo_esp.c — o cofre do aparelho: NVS, na flash soldada.
//
// Senha do Wi-Fi, prova e token não ficam no microSD, que sai com a unha. O
// que isto é e não é:
//   · resolve o ataque real, que é puxar o cartão;
//   · NÃO é cofre: sem `CONFIG_NVS_ENCRYPTION` (que exige criptografia de flash
//     e partição `nvs_keys`), um dump lê. Subir esse degrau é decisão do dono.
// Arquivo próprio: `nvs_flash.h` puxa cabeçalhos do IDF.
#include "segredo_esp.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <string.h>

static const char *TAG = "segredo";

// Namespace próprio para não disputar chave com o Wi-Fi do IDF, que usa a
// mesma partição.
#define COFRE "tinto"

// As credenciais são lidas no boot, antes do Wi-Fi iniciar a NVS. Iniciar
// aqui também é idempotente e tira a dependência da ordem do boot.
static esp_err_t garante(void)
{
    static bool pronta;
    if (pronta) return ESP_OK;

    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Partição cheia ou de outra versão: apagar é a única saída. O aparelho
        // volta a ser de fábrica e se registra de novo.
        ESP_LOGW(TAG, "NVS ilegível (%s): apagando", esp_err_to_name(e));
        nvs_flash_erase();
        e = nvs_flash_init();
    }
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "NVS não subiu: %s", esp_err_to_name(e));
        return e;
    }

    pronta = true;
    return ESP_OK;
}

int hal_segredo_grava(const char *chave, const char *valor)
{
    if (!chave || !valor) return -1;
    if (garante() != ESP_OK) return -1;

    nvs_handle_t h;
    if (nvs_open(COFRE, NVS_READWRITE, &h) != ESP_OK) return -1;

    // Valor vazio APAGA: "sem token" e "token em branco" não podem ser dois
    // estados.
    esp_err_t e = valor[0] ? nvs_set_str(h, chave, valor)
                           : nvs_erase_key(h, chave);
    if (e == ESP_ERR_NVS_NOT_FOUND) e = ESP_OK;   // apagar o que não há

    if (e == ESP_OK) e = nvs_commit(h);
    nvs_close(h);

    // Nunca o conteúdo no log: só a chave e o resultado.
    if (e != ESP_OK) ESP_LOGW(TAG, "%s: %s", chave, esp_err_to_name(e));
    return e == ESP_OK ? 0 : -1;
}

int hal_segredo_le(const char *chave, char *out, size_t max)
{
    if (!chave || !out || !max) return -1;
    out[0] = '\0';
    if (garante() != ESP_OK) return -1;

    nvs_handle_t h;
    if (nvs_open(COFRE, NVS_READONLY, &h) != ESP_OK) return -1;

    size_t n = max;
    esp_err_t e = nvs_get_str(h, chave, out, &n);
    nvs_close(h);

    if (e != ESP_OK) { out[0] = '\0'; return -1; }
    return 0;
}

int hal_segredo_apaga(const char *chave)
{
    if (!chave) return -1;
    if (garante() != ESP_OK) return -1;

    nvs_handle_t h;
    if (nvs_open(COFRE, NVS_READWRITE, &h) != ESP_OK) return -1;

    esp_err_t e = nvs_erase_key(h, chave);
    if (e == ESP_ERR_NVS_NOT_FOUND) e = ESP_OK;
    if (e == ESP_OK) e = nvs_commit(h);
    nvs_close(h);

    return e == ESP_OK ? 0 : -1;
}
