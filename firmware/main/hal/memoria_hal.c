#include "memoria_hal.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

static const char *TAG = "mem";

void *mem_emprestada(size_t desejado, size_t minimo, size_t *tamanho_real)
{
    if (minimo > desejado) minimo = desejado;

    // PSRAM primeiro: 8 MB contra 512 KB, e nada daqui precisa de DMA.
    for (size_t t = desejado; t >= minimo; t /= 2) {
        void *p = heap_caps_malloc(t, MALLOC_CAP_SPIRAM);
        if (p) {
            // Dizer de onde veio: é como se sabe, sem sonda, que a política está de pé.
            ESP_LOGI(TAG, "%u KB da PSRAM · interna livre %u KB",
                     (unsigned)(t / 1024),
                     (unsigned)(mem_interna_livre() / 1024));
            if (tamanho_real) *tamanho_real = t;
            return p;
        }
        if (t == minimo) break;
    }

    // Sem PSRAM (não subiu, ou a placa não tem): vale a interna, e o mínimo
    // protege o resto.
    for (size_t t = desejado; t >= minimo; t /= 2) {
        void *p = heap_caps_malloc(t, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (p) {
            ESP_LOGW(TAG, "%u KB da RAM interna: a PSRAM não deu",
                     (unsigned)(t / 1024));
            if (tamanho_real) *tamanho_real = t;
            return p;
        }
        if (t == minimo) break;
    }

    if (tamanho_real) *tamanho_real = 0;
    ESP_LOGE(TAG, "sem memória para %u KB", (unsigned)(desejado / 1024));
    return NULL;
}

void mem_devolve(void *p)
{
    if (p) heap_caps_free(p);
}

size_t mem_interna_livre(void)
{
    return heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
}

size_t mem_psram_livre(void)
{
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
}
