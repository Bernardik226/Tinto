// hal/audio_esp.c — o INMP441 virando WAV no cartão.
//
// I2S0 RX mono 16 kHz, sem filtro nem ganho automático: a IA recebe o mesmo
// áudio que fica no cartão. RN-13: os trechos de uma sessão vão para o MESMO
// arquivo, por isso `retomar` não é um segundo `iniciar`.
#include "audio_esp.h"
#include "hal_sd.h"
#include "../pins.h"

#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "audio";

// 16 kHz mono 16 bits = 32 KB/s: o mínimo que a transcrição aceita sem
// perder consoante.
#define TAXA      16000
#define CANAIS    1
#define BITS      16
#define CABECALHO 44

// 4 KB de amostras de 32 bits = 32 ms: o medidor da tela acompanha sem
// acordar a task mil vezes por segundo.
#define BLOCO_AMOSTRAS 256

// Só entra som: não há saída de áudio no Tinto. Se um dia houver, nasce
// num I2S1 TX com CI externo (o S3 não tem DAC), separado deste RX.
static i2s_chan_handle_t canal;      // RX, o microfone
static TaskHandle_t      task;

static volatile bool gravando;     // a task está copiando I2S → cartão
static volatile bool aberto;       // há arquivo esperando fechar
static volatile int  trechos;

// O cabeçalho do WAV canônico. Os dois tamanhos entram zerados e são
// reescritos no fim.
static void monta_cabecalho(uint8_t c[CABECALHO], uint32_t bytes_pcm)
{
    const uint32_t taxa_bytes = TAXA * CANAIS * (BITS / 8);
    const uint16_t alinhamento = CANAIS * (BITS / 8);

    memcpy(c, "RIFF", 4);
    uint32_t riff = 36 + bytes_pcm;
    memcpy(c + 4, &riff, 4);
    memcpy(c + 8, "WAVEfmt ", 8);

    uint32_t tam_fmt = 16;
    uint16_t formato = 1;   // PCM sem compressão
    uint16_t canais  = CANAIS;
    uint32_t taxa    = TAXA;
    uint16_t bits    = BITS;

    memcpy(c + 16, &tam_fmt, 4);
    memcpy(c + 20, &formato, 2);
    memcpy(c + 22, &canais, 2);
    memcpy(c + 24, &taxa, 4);
    memcpy(c + 28, &taxa_bytes, 4);
    memcpy(c + 32, &alinhamento, 2);
    memcpy(c + 34, &bits, 2);
    memcpy(c + 36, "data", 4);
    memcpy(c + 40, &bytes_pcm, 4);
}

// O INMP441 entrega 24 bits alinhados à ESQUERDA num slot de 32: os 16 mais
// significativos são o sinal. Descer mais seria ganho não medido.
static void converte(const int32_t *dentro, int16_t *fora, int n)
{
    for (int i = 0; i < n; i++) {
        int32_t s = dentro[i] >> 16;
        if (s >  32767) s =  32767;
        if (s < -32768) s = -32768;
        fora[i] = (int16_t)s;
    }
}

static void task_audio(void *arg)
{
    (void)arg;
    static int32_t cru[BLOCO_AMOSTRAS];
    static int16_t pcm[BLOCO_AMOSTRAS];

    for (;;) {
        if (!gravando) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }

        size_t lidos = 0;
        esp_err_t e = i2s_channel_read(canal, cru, sizeof cru, &lidos,
                                       pdMS_TO_TICKS(200));
        if (e != ESP_OK || lidos == 0) continue;

        int n = (int)(lidos / sizeof(int32_t));
        converte(cru, pcm, n);

        if (hal_sd_fluxo_escreve(pcm, (size_t)n * sizeof(int16_t)) != OK) {
            // Cartão cheio ou removido no meio da fala: para de gravar sem fechar o
            // arquivo; `audio_fecha` decide o que fazer com o que entrou.
            ESP_LOGW(TAG, "escrita falhou; a captura para aqui");
            gravando = false;
        }
    }
}

erro_t hal_audio_liga(void)
{
    i2s_chan_config_t conf = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,
                                                        I2S_ROLE_MASTER);
    esp_err_t e = i2s_new_channel(&conf, NULL, &canal);
    if (e != ESP_OK) { ESP_LOGE(TAG, "i2s_new_channel: %s", esp_err_to_name(e));
                       return ERR_INTERNO; }

    i2s_std_config_t std = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(TAXA),
        // Philips, 32 bits por slot, só o esquerdo: o L/R do INMP441 está em GND.
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = PIN_MIC_SCK,
            .ws   = PIN_MIC_WS,
            .dout = I2S_GPIO_UNUSED,
            .din  = PIN_MIC_SD,
            .invert_flags = { false, false, false },
        },
    };
    std.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    e = i2s_channel_init_std_mode(canal, &std);
    if (e != ESP_OK) { ESP_LOGE(TAG, "init_std: %s", esp_err_to_name(e));
                       return ERR_INTERNO; }

    // Prioridade alta: amostra de I2S perdida não se reconstrói.
    if (xTaskCreate(task_audio, "AUDIO", 4096, NULL, 6, &task) != pdPASS)
        return ERR_INTERNO;

    ESP_LOGI(TAG, "I2S0 RX pronto: %d Hz mono 16 bits", TAXA);
    return OK;
}


erro_t hal_audio_inicia(const char *caminho)
{
    if (aberto) return ERR_INTERNO;

    erro_t err = hal_sd_fluxo_abre(caminho);
    if (err != OK) { ESP_LOGW(TAG, "não abri %s", caminho); return err; }

    uint8_t cabecalho[CABECALHO];
    monta_cabecalho(cabecalho, 0);
    err = hal_sd_fluxo_escreve(cabecalho, sizeof cabecalho);
    if (err != OK) { hal_sd_fluxo_fecha(); return err; }

    if (i2s_channel_enable(canal) != ESP_OK) {
        hal_sd_fluxo_fecha();
        return ERR_INTERNO;
    }

    aberto   = true;
    trechos  = 1;
    gravando = true;
    ESP_LOGI(TAG, "gravando em %s", caminho);
    return OK;
}

erro_t hal_audio_pausa(void)
{
    if (!aberto || !gravando) return ERR_INTERNO;

    gravando = false;
    i2s_channel_disable(canal);
    return OK;
}

erro_t hal_audio_retoma(void)
{
    if (!aberto || gravando) return ERR_INTERNO;

    if (i2s_channel_enable(canal) != ESP_OK) return ERR_INTERNO;
    trechos++;          // RN-13: mesmo arquivo, trecho novo
    gravando = true;
    return OK;
}

erro_t hal_audio_fecha(int *dur_s, int *trechos_fora)
{
    if (!aberto) return ERR_INTERNO;

    if (gravando) { gravando = false; i2s_channel_disable(canal); }

    uint32_t bytes = hal_sd_fluxo_bytes();
    uint32_t pcm   = bytes > CABECALHO ? bytes - CABECALHO : 0;

    // Agora se sabe quanto áudio existe. Sem reescrever o cabeçalho, o arquivo
    // toca zero segundo.
    uint8_t cabecalho[CABECALHO];
    monta_cabecalho(cabecalho, pcm);
    (void)hal_sd_fluxo_escreve_em(0, cabecalho, sizeof cabecalho);

    erro_t err = hal_sd_fluxo_fecha();
    aberto = false;

    if (dur_s)        *dur_s = (int)(pcm / (TAXA * CANAIS * (BITS / 8)));
    if (trechos_fora) *trechos_fora = trechos;
    trechos = 0;

    ESP_LOGI(TAG, "fechado: %u bytes de PCM, %d s", (unsigned)pcm,
             dur_s ? *dur_s : 0);
    return err;
}

erro_t hal_audio_descarta(void)
{
    if (!aberto) return OK;

    if (gravando) { gravando = false; i2s_channel_disable(canal); }
    erro_t err = hal_sd_fluxo_fecha();
    aberto  = false;
    trechos = 0;
    // Quem apaga o arquivo é uso/; aqui só se solta o que é da placa.
    return err;
}

