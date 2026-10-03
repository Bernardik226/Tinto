// main.c — o ponto de entrada da PLACA (no PC: firmware/testes/main_teste.c
// ou o simulador). Só a task APP toca o `estado_t`; áudio e rede empurram
// evento na fila. Uma dona só: o estado nunca é visto no meio de uma
// transição.
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "app/app.h"
#include "bancada/eink.h"
#include "hal/hal.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

const hal_t *hal_esp_liga(void);

// Estático: um app_t na pilha de uma task seriam ~13 KB.
static app_t ap;

// RN-72: o firmware novo tem 60 s para se declarar são, ou o bootloader
// volta sozinho. "São" é bootou, LEU O CARTÃO e desenhou.
static void confirma_se_esta_sao(void)
{
    esp_ota_img_states_t estado;
    const esp_partition_t *p = esp_ota_get_running_partition();
    if (esp_ota_get_state_partition(p, &estado) != ESP_OK) return;
    if (estado != ESP_OTA_IMG_PENDING_VERIFY) return;

    if (ap.estado.itens_validos)
        esp_ota_mark_app_valid_cancel_rollback();
}

// Por que reiniciou: separa travamento (watchdog, código) de queda de
// tensão (brownout, capacitor).
static void conta_por_que_reiniciou(void)
{
    const char *causa;
    switch (esp_reset_reason()) {
    case ESP_RST_POWERON:  causa = "ligou na tomada";            break;
    case ESP_RST_SW:       causa = "reinício por software";      break;
    case ESP_RST_PANIC:    causa = "exceção — olhe o backtrace"; break;
    case ESP_RST_INT_WDT:  causa = "watchdog de interrupção";    break;
    case ESP_RST_TASK_WDT: causa = "watchdog de task";           break;
    case ESP_RST_WDT:      causa = "watchdog";                   break;
    case ESP_RST_BROWNOUT: causa = "BROWNOUT — a tensão caiu";   break;
    case ESP_RST_DEEPSLEEP:causa = "acordou do deep sleep";      break;
    default:               causa = "outra";                      break;
    }
    ESP_LOGW("tinto", "reset: %s", causa);
}

// A margem de pilha que sobrou, dita alto: estouro de pilha é reboot que
// parece qualquer outra coisa.
static void conta_recursos_que_sobram(void)
{
    UBaseType_t sobra = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI("tinto", "APP stack min %u · DRAM livre/min/bloco %u/%u/%u · "
             "PSRAM livre/min %u/%u",
             (unsigned)(sobra * sizeof(StackType_t)),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
}

static void task_app(void *arg)
{
    (void)arg;
    conta_por_que_reiniciou();
    app_liga(&ap, hal_esp_liga());

    bool ja_confirmou = false;
    unsigned passos = 0;
    for (;;) {
        // Quanto um passo custa: separa "o vidro é lento" de "o app é lento".
        int64_t t0 = esp_timer_get_time();
        app_passo(&ap);
        int64_t dt = (esp_timer_get_time() - t0) / 1000;
        if (dt > 60) ESP_LOGW("perf", "passo levou %d ms", (int)dt);

        if (!ja_confirmou) { confirma_se_esta_sao(); ja_confirmou = true; }

        // Uma vez por minuto: diagnóstico, não telemetria.
        if (++passos % (60 * 1000 / 20) == 0) conta_recursos_que_sobram();

        // O laço espera evento em vez de girar.
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_main(void)
{
#if BANCADA_EINK
    hal_esp_liga();
    bancada_eink_roda();
    return;
#endif

    // Prioridades: AUDIO alta (amostra perdida não volta), APP no meio, REDE
    // baixa (assíncrona, RN-41).
    //
    // Pilha da APP de 48 KB: carrega na mesma cadeia um vetor de `item_t` e a
    // vista da tela; menos já deu boot loop. Mover tudo para `static` foi
    // tentado e levou o .bss de 108 a 203 KB de RAM interna (que o DMA, o
    // cartão e o Wi-Fi precisam). Buffer de trabalho vive na pilha;
    // `firmware/ferramentas/pilha.py` vigia o quadro de cada função.
    xTaskCreate(task_app, "APP", 49152, NULL, 5, NULL);
}
