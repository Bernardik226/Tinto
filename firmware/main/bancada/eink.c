#include "eink.h"

#if BANCADA_EINK

#include "../tela/bitmap.h"
#include "../tela/texto.h"
#include "../hal/uc8253.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// O hal reinicia o painel com a configuração pedida e manda UM quadro
// completo, sem faxina nem política de refresco.
bool hal_esp_eink_bancada(uint8_t psr, bool inverte, const uint8_t *bits);

static const char *TAG = "bancada";

static uint8_t memoria[TELA_L / 8 * TELA_A];

// ── o padrão ─────────────────────────────────────────────────────────
// Tudo TEXTO: palavra escrita se lê, ou está espelhada, ou de ponta-cabeça,
// e os três casos são óbvios de longe. A polaridade já foi decidida (o RAM
// usa 1 = tinta); falta a orientação.
static void desenha(bitmap_t *bm, int indice)
{
    char digito[2] = { (char)('0' + indice), 0 };

    gfx_limpa(bm, false);

    // A moldura: se faltar um lado, a resolução não bate com o vidro.
    gfx_hlin(bm, 0, 0, TELA_L, 3);
    gfx_hlin(bm, 0, TELA_A - 3, TELA_L, 3);
    gfx_vlin(bm, 0, 0, TELA_A, 3);
    gfx_vlin(bm, TELA_L - 3, 0, TELA_A, 3);

    // "TOPO" no alto e "PE" embaixo: nenhuma das duas se lê igual virada.
    gfx_texto(bm, 14, 14, F_TITULO, "TOPO");
    gfx_texto(bm, 14, TELA_A - 14 - gfx_altura_linha(F_TITULO), F_TITULO, "PE");

    // "ESQUERDA" na borda esquerda: à direita ou invertida, é o bit SHL.
    gfx_texto(bm, 14, TELA_A / 2 - 60, F_CORPO, "ESQUERDA");

    // O número da combinação, grande: dígito espelhado se denuncia de longe.
    gfx_texto(bm, TELA_L / 2 - 14, TELA_A / 2 - 20, F_ENORME, digito);
}

void bancada_eink_roda(void)
{
    // As quatro orientações, na ordem do driver.
    static const uint8_t PSRS[] = { 0x1F, 0x1B, 0x17, 0x13 };

    bitmap_t bm;
    bitmap_liga(&bm, memoria, TELA_L, TELA_A);

    ESP_LOGW(TAG, "=== bancada do e-ink: 4 orientações, 8 s cada ===");
    ESP_LOGW(TAG, "a certa é a que se LÊ: TOPO em cima, PE embaixo,");
    ESP_LOGW(TAG, "ESQUERDA à esquerda e o número sem estar espelhado.");

    for (;;) {
        for (int p = 0; p < 4; p++) {
            desenha(&bm, p + 1);
            ESP_LOGW(TAG, "orientação %d · PSR=0x%02X", p + 1, PSRS[p]);
            if (!hal_esp_eink_bancada(PSRS[p], false, memoria))
                ESP_LOGE(TAG, "orientação %d falhou", p + 1);
            vTaskDelay(pdMS_TO_TICKS(8000));
        }
    }
}

#else
void bancada_eink_roda(void) { }
#endif
