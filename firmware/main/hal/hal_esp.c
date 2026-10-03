// hal/hal_esp.c — o ÚNICO arquivo que conhece a placa.
//
// Entrega papéis (IN_CIMA, `mostrar(bits)`), nunca peças ("bit 0 do PCF",
// "comando 0x24"). Se o aparelho divergir do simulador, o vazamento está aqui
// ou num #include que não devia existir.
#include "hal.h"
#include "memoria_hal.h"
#include "hal_sd.h"
#include "audio_esp.h"
#include "wifi_esp.h"
#include "hora_esp.h"
#include "nuvem_esp.h"
#include "segredo_esp.h"
#include "../pins.h"
#include "uc8253.h"
#include "refresco.h"

// A troca de tela é FLASH BRANCO + página, não a waveform completa de 2,5 s.
// Medido no vidro.

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_memory_utils.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

// ── a fila de eventos ────────────────────────────────────────────────
// Por aqui AUDIO e REDE falam com o APP. Nenhum evento carrega ponteiro para
// memória de outra task: o dado cabe na struct, ou vem um caminho no cartão.
static QueueHandle_t fila_eventos;

void hal_esp_empurra(const evento_t *ev)
{
    if (!fila_eventos) return;
    if (xQueueSend(fila_eventos, ev, 0) != pdTRUE)
        ESP_LOGW("entrada", "fila cheia: evento %d descartado", ev->tipo);
}

// O SNTP não enxerga os tipos do sistema (ERR_TIMEOUT do lwip), então avisa
// por aqui.
void hal_esp_empurra_hora(void)
{
    evento_t ev = { .tipo = EV_HORA_DA_REDE };
    hal_esp_empurra(&ev);
}

// ── entrada ──────────────────────────────────────────────────────────
// Dez botões pelo PCF8575 numa leitura de dois bytes; power por GPIO direto.
// O INT do expansor acorda o chip, e o app decide se a origem desbloqueia.
static i2c_master_dev_handle_t pcf;

// Onde o expansor foi achado. `pins.h` diz onde deveria estar; os dois
// divergem quando um pino de endereço flutua.
static uint8_t pcf_onde = ENDERECO_PCF8575;
static i2c_master_bus_handle_t i2c_bus;
static i2c_master_dev_handle_t fuel;
static int falhas_fuel;

// Fixo: o MAX17048 não tem pinos de seleção.
#define ENDERECO_MAX17048 0x36

// ── a carga, do fuel gauge ───────────────────────────────────────────
// Registrador 0x04 (SOC): o byte alto já é a porcentagem. -1 quando não dá
// para saber, nunca 100.
static int hal_esp_bateria(void)
{
    if (!fuel) return -1;

    uint8_t reg = 0x04, b[2] = {0, 0};
    if (i2c_master_transmit_receive(fuel, &reg, 1, b, 2, 50) != ESP_OK) {
        // Desiste depois de três falhas seguidas: o gauge pode não estar montado,
        // e o driver loga três linhas por tentativa, a cada segundo. Soltar o handle
        // cala o driver; -1 já esconde a barra.
        if (++falhas_fuel >= 3) {
            ESP_LOGW("bateria",
                     "fuel gauge não responde — parando de perguntar");
            i2c_master_bus_rm_device(fuel);
            fuel = NULL;
        }
        return -1;
    }

    falhas_fuel = 0;
    int soc = b[0];
    return soc > 100 ? 100 : soc;
}
static TaskHandle_t task_entrada_handle;

#define ENTRADA_POWER   16
#define ENTRADAS_QUANTAS 17

// Bits já apertados no wake: soltá-los é o fim do gesto que acordou o chip,
// não um clique novo.
static uint32_t ignorar_ate_soltar;

static const entrada_t PAPEL[] = {
    [PCF_CIMA]      = IN_CIMA,
    [PCF_BAIXO]     = IN_BAIXO,
    [PCF_ESQ]       = IN_ESQ,
    [PCF_DIR]       = IN_DIR,
    [PCF_OK]        = IN_OK,
    [PCF_MENU]      = IN_MENU,
    [PCF_VOLTAR]    = IN_VOLTAR,
    [PCF_VOZ]       = IN_VOZ,
    [ENTRADA_POWER] = IN_POWER,
};

// Procura o expansor só na família dele (0x20–0x27), para não falar com o
// gauge nem com fantasma. Devolve true quando trocou de lugar.
static bool pcf_reencontra(void)
{
    if (!i2c_bus) return false;

    // O de `pins.h` primeiro: se ele responder, é ele. Varrer antes fazia o
    // aparelho se mudar para um ACK fantasma em 0x20.
    uint8_t ordem[9];
    int n = 0;
    ordem[n++] = ENDERECO_PCF8575;
    for (uint8_t a = 0x20; a <= 0x27; a++)
        if (a != ENDERECO_PCF8575) ordem[n++] = a;

    for (int i = 0; i < n; i++) {
        uint8_t a = ordem[i];
        if (i2c_master_probe(i2c_bus, a, 20) != ESP_OK) continue;
        if (a == pcf_onde) return false;      // é o mesmo: nada a fazer

        i2c_device_config_t novo = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = a,
            .scl_speed_hz = 100000,
        };
        i2c_master_dev_handle_t alvo = NULL;
        if (i2c_master_bus_add_device(i2c_bus, &novo, &alvo) != ESP_OK)
            return false;

        ESP_LOGW("entrada", "expansor MUDOU de 0x%02x para 0x%02x",
                 pcf_onde, a);
        if (pcf) (void)i2c_master_bus_rm_device(pcf);
        pcf = alvo;
        pcf_onde = a;
        return true;
    }
    return false;
}

static bool pcf_le(uint16_t *out)
{
    static uint16_t ultimo = 0;
    static bool avisou = false;
    static uint8_t falhas = 0;
    // Leituras mortas e por quanto tempo: sem o número, "melhorou" é opinião.
    static uint32_t falhas_seguidas = 0, caiu_em = 0;
    if (out) *out = ultimo;
    // PCF8575 é quase-bidirecional: 1 no pino = entrada com pull-up fraco, e o
    // botão puxa para GND. Apertado = bit ZERO.
    uint8_t b[2] = { 0xFF, 0xFF };
    esp_err_t err = i2c_master_receive(pcf, b, 2, 50);
    // O driver reinicia a máquina de estados quando a transação falha; repetir
    // já aqui impede a amostra perdida de engolir um clique curto.
    if (err != ESP_OK)
        err = i2c_master_receive(pcf, b, 2, 50);
    if (err != ESP_OK) {
        // Uma linha por transição separa "não apertou" de "o expansor não
        // respondeu" — sem ela, tela certa e botões mortos sem explicação.
        if (!avisou) {
            ESP_LOGE("entrada", "PCF8575 não respondeu: %s",
                     esp_err_to_name(err));
            avisou = true;
            caiu_em = (uint32_t)(esp_timer_get_time() / 1000);
            falhas_seguidas = 0;
        }
        falhas_seguidas++;
        // Periférico que ficou energizado enquanto só o ESP reiniciou pode deixar o
        // barramento fora de fase. Recupera de tempos em tempos, sem martelar.
        if (++falhas >= 50) {
            falhas = 0;
            if (i2c_bus) (void)i2c_master_bus_reset(i2c_bus);
            uint8_t tudo_alto[2] = {0xFF, 0xFF};
            (void)i2c_master_transmit(pcf, tudo_alto, 2, 50);

            // Um A0/A1/A2 flutuante muda o endereço com o aparelho ligado. Sem procurar
            // de novo, os botões morriam até tirar da tomada.
            (void)pcf_reencontra();
        }
        return false;
    }

    if (avisou)
        ESP_LOGI("entrada", "PCF8575 voltou após %u falhas em %u ms",
                 (unsigned)falhas_seguidas,
                 (unsigned)((esp_timer_get_time() / 1000) - caiu_em));
    avisou = false;
    falhas = 0;

    uint16_t lido = (uint16_t)(~(b[0] | (b[1] << 8)));
    static uint16_t anterior = 0;
    if (lido != anterior) {
        ESP_LOGI("entrada", "PCF8575 0x%04x", lido);
        anterior = lido;
    }
    if (out) *out = lido;
    ultimo = lido;
    return true;
}

static bool entradas_le(uint32_t *out)
{
    uint16_t botoes = 0;
    bool pcf_ok = pcf_le(&botoes);
    uint32_t entradas = botoes;
    if (gpio_get_level(PIN_BOTAO_POWER) == 0)
        entradas |= 1u << ENTRADA_POWER;
    if (out) *out = entradas;
    return pcf_ok;
}

static void IRAM_ATTR avisa_mudanca_de_entrada(void *arg)
{
    (void)arg;
    BaseType_t acordou = pdFALSE;
    vTaskNotifyGiveFromISR(task_entrada_handle, &acordou);
    if (acordou) portYIELD_FROM_ISR();
}

// Debounce por AMOSTRAGEM ESTÁVEL: dois quadros iguais valem, um pico não.
// Segurar para andar: 500 ms até a primeira repetição e 110 ms entre elas —
// lista longa sem mil cliques, toque comum sem andar duas linhas.
#define REPETE_ESPERA_MS 500
#define REPETE_PASSO_MS  110

#define DEBOUNCE_MS 12

static bool entrada_dispara_no_aperto(entrada_t e)
{
    return e == IN_CIMA || e == IN_BAIXO || e == IN_ESQ || e == IN_DIR ||
           e == IN_OK || e == IN_MENU;
}

static void task_entrada(void *arg)
{
    (void)arg;
    uint32_t estavel = 0, anterior = 0;
    uint32_t desde[ENTRADAS_QUANTAS] = {0};
    uint32_t apertado_em[ENTRADAS_QUANTAS] = {0};
    uint32_t repetido_em[ENTRADAS_QUANTAS] = {0};
    bool     repetiu[ENTRADAS_QUANTAS] = {false};

    for (;;) {
        uint32_t agora = 0;
        bool leitura_ok = entradas_le(&agora);
        uint32_t t = (uint32_t)(esp_timer_get_time() / 1000);

        // O INT pode continuar baixo depois de uma leitura falha. Dormir esperando
        // outra borda deixava joystick/MENU/BACK congelados até o Power.
        for (int i = 0; i < ENTRADAS_QUANTAS; i++) {
            uint32_t bit = 1u << i;
            bool ligado_agora = (agora & bit) != 0;
            bool ligado_antes = (anterior & bit) != 0;

            if (ligado_agora != ligado_antes) { desde[i] = t; continue; }
            if (t - desde[i] < DEBOUNCE_MS)   continue;

            bool era = (estavel & bit) != 0;
            if (ligado_agora == era) continue;

            if (ligado_agora) estavel |= bit; else estavel &= ~bit;

            if (i >= (int)(sizeof PAPEL / sizeof PAPEL[0])) continue;
            entrada_t papel = PAPEL[i];
            if (papel == IN_NADA) continue;

            if (ligado_agora) {
                apertado_em[i] = t;
                repetiu[i] = false;
                if (!(ignorar_ate_soltar & bit)) {
                    evento_t ev = {
                        .tipo = papel == IN_VOZ ? EV_BOTAO_APERTO : EV_BOTAO,
                        .botao = papel,
                    };
                    if (papel == IN_VOZ || entrada_dispara_no_aperto(papel))
                        hal_esp_empurra(&ev);
                }
            } else {
                if (ignorar_ate_soltar & bit) {
                    ignorar_ate_soltar &= ~bit;
                    continue;
                }
                // Navegação, OK e MENU já saíram no apertar; soltar só fecha os
                // gestos que medem duração.
                if (entrada_dispara_no_aperto(papel)) {
                    repetiu[i] = false;
                    continue;
                }
                // O ms do evento é quanto ficou apertado, e separa clique de segurado no
                // app. Se já repetiu, soltar não emite nada: andaria uma linha a mais.
                if (repetiu[i]) { repetiu[i] = false; continue; }

                evento_t ev = { .tipo = EV_BOTAO, .botao = papel,
                                .ms = (int32_t)(t - apertado_em[i]) };
                hal_esp_empurra(&ev);
            }
        }
        anterior = agora;

        // ── repetição ao segurar ─────────────────────────────────────────────
        // Só o DIRECIONAL: OK, BACK e ● segurados têm sentido próprio (BACK volta
        // para a home, ● grava). Mora aqui para ser a mesma regra em toda tela.
        bool segurando = false;
        for (int i = 0; i < ENTRADAS_QUANTAS; i++) {
            if (i >= (int)(sizeof PAPEL / sizeof PAPEL[0])) continue;
            entrada_t papel = PAPEL[i];
            if (papel != IN_CIMA && papel != IN_BAIXO &&
                papel != IN_ESQ  && papel != IN_DIR) continue;
            if (!(estavel & (1u << i))) continue;
            if (ignorar_ate_soltar & (1u << i)) continue;

            segurando = true;
            uint32_t preso = t - apertado_em[i];

            // Antes de meio segundo ainda é clique.
            if (preso < REPETE_ESPERA_MS) continue;

            uint32_t desde = repetiu[i] ? t - repetido_em[i]
                                        : REPETE_PASSO_MS;
            if (desde < REPETE_PASSO_MS) continue;

            evento_t ev = { .tipo = EV_BOTAO, .botao = papel, .ms = 0 };
            hal_esp_empurra(&ev);
            repetiu[i] = true;
            repetido_em[i] = t;
        }

        // Reamostra só enquanto há mudança esperando o debounce; segurando, acorda
        // no passo da repetição; estável e solto, dorme até o INT ou o power.
        TickType_t espera = !leitura_ok      ? pdMS_TO_TICKS(20)
                          : agora != estavel ? pdMS_TO_TICKS(DEBOUNCE_MS)
                          : segurando        ? pdMS_TO_TICKS(REPETE_PASSO_MS / 2)
                          : portMAX_DELAY;
        ulTaskNotifyTake(pdTRUE, espera);
    }
}

static void task_tick(void *arg)
{
    (void)arg;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        evento_t ev = { .tipo = EV_TICK };
        hal_esp_empurra(&ev);
    }
}

static bool esp_proximo_evento(evento_t *out)
{
    return xQueueReceive(fila_eventos, out, 0) == pdTRUE;
}

// ── relógio e tempo ──────────────────────────────────────────────────
static uint32_t esp_agora_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

// ── o fuso ───────────────────────────────────────────────────────────
// Sem TZ, `localtime_r` devolve UTC. A TZ sai do deslocamento em minutos que
// o backend manda (fuso da conta Google). POSIX inverte o sinal: `<-03>3` é
// três horas ATRÁS. Sem regra de horário de verão: o servidor manda o
// deslocamento já resolvido.
void hal_esp_fuso(int minutos)
{
    if (minutos < -720) minutos = -720;
    if (minutos >  840) minutos =  840;

    int h = -minutos / 60;              // POSIX inverte o sinal
    int m = (minutos < 0 ? -minutos : minutos) % 60;

    char tz[24];
    if (m) snprintf(tz, sizeof tz, "<%+03d:%02d>%d:%02d",
                    minutos / 60, m, h, m);
    else   snprintf(tz, sizeof tz, "<%+03d>%d", minutos / 60, h);

    setenv("TZ", tz, 1);
    tzset();
    ESP_LOGI("hora", "fuso: %d min (TZ=%s)", minutos, tz);
}

static void esp_relogio(data_t *d, int *hora, int *minuto)
{
    time_t agora = time(NULL);
    struct tm tm;
    localtime_r(&agora, &tm);
    d->ano  = (int16_t)(tm.tm_year + 1900);
    d->mes  = (int8_t)(tm.tm_mon + 1);
    d->dia  = (int8_t)tm.tm_mday;
    *hora   = tm.tm_hour;
    *minuto = tm.tm_min;
}

static void esp_registrar(const char *assunto, const char *texto)
{
    ESP_LOGI("tinto", "%s: %s", assunto, texto);
}

static void esp_ajustar_relogio(data_t d, int hora, int minuto)
{
    struct tm tm = {
        .tm_year = d.ano - 1900,
        .tm_mon  = d.mes - 1,
        .tm_mday = d.dia,
        .tm_hour = hora,
        .tm_min  = minuto,
        .tm_sec  = 0,
        .tm_isdst = -1,
    };
    time_t quando = mktime(&tm);
    if (quando == (time_t)-1) return;

    struct timeval tv = { .tv_sec = quando, .tv_usec = 0 };
    settimeofday(&tv, NULL);
}

static bool esp_docado(void)
{
    // O hall da dock ainda não existe: o aparelho está sempre na mão.
    return false;
}

// ── e-ink 3.7" 240×416 (UC8253) ──────────────────────────────────────
// Write-only: o BUSY é a única resposta. Todo comando espera o BUSY; sem
// isso o controlador engole metade da sequência.
static spi_device_handle_t eink;
static bool eink_pronto;
static const char *TAG_EINK = "eink";

static uc8253_io_t eink_io(void);

static bool eink_espera(void *contexto)
{
    (void)contexto;
    // Full ~2–4 s, parcial ~300 ms. O teto de 8 s é rede de segurança: se
    // disparar, o painel travou e o caminho é o reset.
    uint32_t inicio = esp_agora_ms();
    uint32_t limite = inicio + 8000;
    while (gpio_get_level(PIN_EINK_BUSY) == 0) {
        if (esp_agora_ms() > limite) {
            ESP_LOGE(TAG_EINK, "timeout BUSY=0 após %lu ms",
                     (unsigned long)(esp_agora_ms() - inicio));
            return false;
        }
        // 10 ms, e NÃO 2: o painel não baixa o BUSY no instante do comando, e
        // amostrar mais fino lia o BUSY ainda alto e voltava na hora — o comando
        // seguinte entrava no meio do ciclo. Não mexer sem antes esperar o BUSY CAIR.
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_LOGI(TAG_EINK, "BUSY liberou em %lu ms",
             (unsigned long)(esp_agora_ms() - inicio));
    return true;
}

static bool eink_comando(void *contexto, uint8_t c)
{
    (void)contexto;
    gpio_set_level(PIN_EINK_DC, 0);
    spi_transaction_t t = { .length = 8, .tx_buffer = &c };
    esp_err_t err = spi_device_polling_transmit(eink, &t);
    if (err != ESP_OK)
        ESP_LOGE(TAG_EINK, "comando 0x%02X falhou: %s", c,
                 esp_err_to_name(err));
    return err == ESP_OK;
}

// O DMA do SPI quer DRAM interna alinhada a 4 bytes. Constante em flash ou
// bloco na pilha chega torto e o vidro sai em blocos deslocados de vez em
// quando. A cópia acontece aqui, por onde todo byte passa.
static WORD_ALIGNED_ATTR uint8_t eink_dma[256];

static bool eink_dados(void *contexto, const uint8_t *d, size_t n)
{
    (void)contexto;
    if (!n) return true;
    if (!esp_ptr_dma_capable(d) || ((uintptr_t)d & 3u)) {
        size_t enviado = 0;
        while (enviado < n) {
            size_t p = n - enviado;
            if (p > sizeof eink_dma) p = sizeof eink_dma;
            memcpy(eink_dma, d + enviado, p);
            if (!eink_dados(contexto, eink_dma, p)) return false;
            enviado += p;
        }
        return true;
    }
    gpio_set_level(PIN_EINK_DC, 1);
    spi_transaction_t t = { .length = n * 8, .tx_buffer = d };
    esp_err_t err = spi_device_polling_transmit(eink, &t);
    if (err != ESP_OK)
        ESP_LOGE(TAG_EINK, "envio de %zu bytes falhou: %s", n,
                 esp_err_to_name(err));
    return err == ESP_OK;
}

// A política de refresco mora em hal/refresco.c: é decisão, e lá tem teste.

static uc8253_io_t eink_io(void)
{
    uc8253_io_t io = {
        .contexto = NULL,
        .comando = eink_comando,
        .dados = eink_dados,
        .espera = eink_espera,
    };
    return io;
}
static uint8_t quadro_anterior[(240 + 7) / 8 * 416];
static bool    tem_anterior = false;
static bool    vidro_branco = false;   // a faxina acabou de rodar

static int      tela_l = 240, tela_a = 416;

// Reset e configuração do painel, do zero: o caminho de volta quando o
// controlador para de responder.
static bool eink_liga_do_zero(void)
{
    // Cada lado do reset por 50 ms, como na referência da WeAct.
    gpio_set_level(PIN_EINK_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(PIN_EINK_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    // Depois de um reset o vidro não tem mais o nosso último quadro: o próximo
    // é completo.
    tem_anterior = false;

    uc8253_io_t io = eink_io();
    if (!eink_espera(NULL) || !uc8253_inicia(&io)) return false;

    // A faxina do vidro: refresh diferencial nunca alcança tinta presa (ela não
    // está em nenhum dos dois planos). Sem isto, um fantasma sobrevive a reset e
    // a qualquer número de fulls. Custa ~6 s no boot.
    ESP_LOGI(TAG_EINK, "limpando o vidro");
    uint32_t t0 = esp_agora_ms();
    bool limpo = uc8253_limpa(&io, sizeof quadro_anterior, 1);
    ESP_LOGI(TAG_EINK, "vidro limpo em %lu ms (%s)",
             (unsigned long)(esp_agora_ms() - t0), limpo ? "ok" : "falhou");

    // Depois da faxina o plano velho é branco.
    tem_anterior = false;
    vidro_branco = limpo;
    return limpo;
}

// ── o painel emprestado à bancada ────────────────────────────────────
// Reinicia com a configuração pedida e manda UM quadro, sem faxina e sem
// política no meio.
bool hal_esp_eink_bancada(uint8_t psr, bool inverte, const uint8_t *bits)
{
    uc8253_configura(psr, inverte);

    gpio_set_level(PIN_EINK_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(PIN_EINK_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    uc8253_io_t io = eink_io();
    if (!eink_espera(NULL) || !uc8253_inicia(&io)) return false;
    return uc8253_atualiza(&io, bits, false, sizeof quadro_anterior,
                           UC8253_COMPLETO);
}

// A identidade de um quadro em oito dígitos: é o que deixa a serial dizer
// DE QUÊ foi o refresh, e comparar quadros entre si.
static uint32_t marca_do_quadro(const uint8_t *bits, size_t bytes)
{
    uint32_t h = 2166136261u;                 // FNV-1a
    for (size_t i = 0; i < bytes; i++) { h ^= bits[i]; h *= 16777619u; }
    return h;
}

static void esp_mostrar(const uint8_t *bits, int l, int a,
                        pintura_t intencao)
{
    if (!eink_pronto) {
        ESP_LOGE(TAG_EINK, "quadro ignorado: painel não inicializou");
        return;
    }

    // A largura vem do parâmetro, nunca escrita à mão.
    size_t bytes = (size_t)((l + 7) / 8) * (size_t)a;

    // O primeiro quadro é full: o vidro pode ter a imagem de fábrica.
    // Quadro IDÊNTICO ao do vidro não move tinta e não sai. A comparação é byte a
    // byte: um byte em 12 480 arredonda para 0% e é real.
    if (tem_anterior && bytes <= sizeof quadro_anterior &&
        memcmp(bits, quadro_anterior, bytes) == 0) {
        ESP_LOGI(TAG_EINK, "quadro %08x idêntico ao do vidro: nada a fazer",
                 (unsigned)marca_do_quadro(bits, bytes));
        return;
    }

    // A dívida de ghosting é do PAINEL, por isso mora aqui. O quadro idêntico é
    // descartado antes: não moveu tinta, não deve dívida.
    static refresco_estado_t divida;
    // Conta o que mudou ANTES de decidir: a escolha depende disso.
    size_t diferentes = 0;
    if (tem_anterior && bytes <= sizeof quadro_anterior)
        for (size_t i = 0; i < bytes; i++)
            if (bits[i] != quadro_anterior[i]) diferentes++;

    refresco_t escolha = refresco_por_intencao(intencao, tem_anterior,
                                               diferentes, bytes);
    if (escolha == REFRESCO_COMPLETO) divida.parciais_seguidos = 0;
    else                              divida.parciais_seguidos++;

    static const uc8253_modo_t MODOS[] = {
        [REFRESCO_PARCIAL]  = UC8253_PARCIAL,
        [REFRESCO_COMPLETO] = UC8253_COMPLETO,
    };
    static const char *NOMES[] = {
        [REFRESCO_PARCIAL]  = "parcial",
        [REFRESCO_COMPLETO] = "completo",
    };
    uc8253_modo_t modo = MODOS[escolha];
    uc8253_io_t io = eink_io();

    // Por que este quadro é completo: "tela nova" é navegação; "dívida vencida"
    // é orçamento. Pedem conserto em lugares opostos.
    const char *motivo = intencao == PINTURA_TELA_NOVA ? "tela nova"
                       : intencao == PINTURA_FOCO      ? "foco"
                       : escolha == REFRESCO_COMPLETO  ? "dívida vencida"
                       : "a mesma tela";
    ESP_LOGI(TAG_EINK, "gesto · %s · %d parciais desde o último completo",
             motivo, divida.parciais_seguidos);

    // Quem entra, quem sai e quantos bytes mudaram: diz depois se o vidro
    // pintou o quadro deste ciclo ou o do anterior.
    ESP_LOGI(TAG_EINK, "quadro %08x sobre %08x · %u bytes mudaram",
             (unsigned)marca_do_quadro(bits, bytes),
             (unsigned)(tem_anterior ? marca_do_quadro(quadro_anterior, bytes) : 0),
             (unsigned)diferentes);

    // ── a faixa ──────────────────────────────────────────────────────────
    // Se o que mudou cabe numa faixa curta de linhas (relógio, rodapé), só ela
    // vai. Teto de um quarto da tela: acima disso o ganho some e aparece resíduo
    // fora da janela.
    size_t passo = (size_t)((l + 7) / 8);
    int y0 = -1, y1 = -1;
    if (escolha == REFRESCO_PARCIAL && tem_anterior &&
        bytes <= sizeof quadro_anterior) {
        for (int y = 0; y < a; y++) {
            if (memcmp(bits + (size_t)y * passo,
                       quadro_anterior + (size_t)y * passo, passo) == 0)
                continue;
            if (y0 < 0) y0 = y;
            y1 = y;
        }
    }

    if (y0 >= 0 && (y1 - y0 + 1) <= a / 4) {
        uc8253_faixa_t faixa = { .y0 = y0, .y1 = y1 };
        ESP_LOGI(TAG_EINK, "refresh de faixa · linhas %d-%d (%d de %d)",
                 y0, y1, y1 - y0 + 1, a);

        if (uc8253_atualiza_faixa(&io, bits, bytes, passo, &faixa)) {
            memcpy(quadro_anterior, bits, bytes);
            tem_anterior = true;
            return;
        }
        ESP_LOGW(TAG_EINK, "faixa falhou; vai o quadro inteiro");
    }

    // ── FLASH BRANCO na troca de tela ────────────────────────────────────
    // A waveform nativa custa 2,5 s e duas piscadas: no vidro parece travado.
    // Aqui o vidro vai a branco num parcial e a página entra por cima em outro,
    // ~700 ms. O segundo parcial parte de branco UNIFORME, o caso em que parcial
    // funciona melhor — diferente do full rápido (TSFIX 0x5A), rejeitado.
    //
    // EINK.md §5.5: nada que um parcial vá desfazer pode ser assentado pela
    // waveform completa. Por isso nenhuma troca de tela firma: firmar ali já
    // deixou seletor de sombra, tela preta e engasgos de 4 s. A waveform longa
    // fica para o boot.
    if (escolha == REFRESCO_COMPLETO && tem_anterior) {
        static uint8_t uniforme[sizeof quadro_anterior];
        memset(uniforme, 0x00, bytes);           // bit 0 = BRANCO

        ESP_LOGI(TAG_EINK, "flash branco");
        if (uc8253_atualiza(&io, uniforme, tem_anterior, bytes,
                            UC8253_PARCIAL)) {
            // A tela agora é uniforme, e o driver a guardou como plano velho.
            modo = UC8253_PARCIAL;
        } else {
            // O flash falhou: segue o completo. Um caminho que some quando a peça nova
            // falha vira aparelho morto.
            ESP_LOGW(TAG_EINK, "flash branco falhou; vai o completo");
        }
    }

    // O primeiro quadro depois da faxina também entra por PARCIAL sobre o
    // branco: pelo completo, ele assentava o seletor (§5.5).
    if (!tem_anterior && vidro_branco) modo = UC8253_PARCIAL;
    vidro_branco = false;

    // O que roda de fato: flash branco e pós-faxina vão por parcial mesmo
    // quando a escolha foi completo.
    ESP_LOGI(TAG_EINK, "refresh %s · %s",
             modo == UC8253_PARCIAL ? "parcial" : NOMES[escolha], motivo);

    // O plano velho quem mantém é o driver; o hal só diz se a referência vale.
    if (!uc8253_atualiza(&io, bits, tem_anterior, bytes, modo)) {
        // Sem reiniciar, o BUSY preso derrubava todo refresh seguinte e a tela
        // congelava até tirar da tomada.
        ESP_LOGE(TAG_EINK, "refresh falhou — reiniciando o painel");
        eink_pronto = eink_liga_do_zero();
        if (!eink_pronto) {
            ESP_LOGE(TAG_EINK, "painel não voltou do reset");
            return;
        }
        // Depois do reset o vidro é desconhecido: este quadro vai completo.
        io = eink_io();
        if (!uc8253_atualiza(&io, bits, false, bytes, UC8253_COMPLETO)) {
            // Não desiste do painel para sempre: brownout e mau contato são
            // transientes. O próximo quadro tenta de novo, do zero.
            ESP_LOGE(TAG_EINK, "refresh falhou de novo, mesmo do zero");
            return;
        }
    }

    if (bytes <= sizeof quadro_anterior) {
        memcpy(quadro_anterior, bits, bytes);
        tem_anterior = true;
    }

    tela_l = l;
    tela_a = a;
    ESP_LOGI(TAG_EINK, "refresh concluído");
}

// A ponte entre a task REDE e a fila do app (o nuvem_esp.c não enxerga os
// tipos do sistema).
void hal_esp_empurra_resposta(void)
{
    evento_t ev = { .tipo = EV_REDE_RESULTADO };
    hal_esp_empurra(&ev);
}

// A credencial do rádio, que não mora no cartão.
static void esp_esquecer_rede(void)
{
    hal_wifi_esquecer();
}

static void esp_id_aparelho(char *out, size_t max)
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(out, max, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void esp_nuvem_pede(const char *rota, const char *corpo,
                           const char *operacao)
{
    hal_nuvem_pede(rota, corpo, operacao);
}

static void esp_nuvem_pede_audio(const char *rota, const char *caminho,
                                 const char *operacao)
{
    hal_nuvem_pede_audio(rota, caminho, operacao);
}

static void esp_nuvem_credencial(const char *servidor, const char *token)
{
    hal_nuvem_credencial(servidor, token);
}

// O cofre atravessa a fronteira de tipos: `int` de lá, `erro_t` daqui.
static erro_t esp_segredo_grava(const char *chave, const char *valor)
{
    return hal_segredo_grava(chave, valor) == 0 ? OK : ERR_ARQUIVO;
}

static erro_t esp_segredo_le(const char *chave, char *out, size_t max)
{
    return hal_segredo_le(chave, out, max) == 0 ? OK : ERR_ARQUIVO;
}

static erro_t esp_segredo_apaga(const char *chave)
{
    return hal_segredo_apaga(chave) == 0 ? OK : ERR_ARQUIVO;
}

static int esp_nuvem_codigo(void) { return hal_nuvem_codigo(); }
static int esp_nuvem_linha(void)  { return hal_nuvem_linha();  }

static erro_t esp_nuvem_resposta(char *out, size_t max)
{
    return hal_nuvem_resposta(out, max) ? OK : ERR_REDE;
}

// Reinício de verdade, como tirar da tomada. É o fim do Restaurar.
static void esp_reiniciar(void)
{
    if (eink_pronto) {
        uc8253_io_t io = eink_io();
        uc8253_desliga(&io);
    }
    esp_restart();
}

// ── a struct que o app recebe ────────────────────────────────────────
static const hal_t HAL = {
    .proximo_evento = esp_proximo_evento,
    .mostrar        = esp_mostrar,
    .ler            = hal_sd_ler,
    .escrever       = hal_sd_escrever,
    .anexar         = hal_sd_anexar,
    .apagar         = hal_sd_apagar,
    .listar         = hal_sd_listar,
    .renomear       = hal_sd_renomear,
    .memoria_estado = hal_sd_memoria_estado,

    // PSRAM primeiro, e devolvido (`memoria_hal.h`).
    .emprestar = mem_emprestada,
    .devolver  = mem_devolve,
    .criar_diretorio = hal_sd_criar_diretorio,
    .tipo_caminho   = hal_sd_tipo_caminho,
    .formatar_memoria = hal_sd_formatar,
    .esquecer_rede    = esp_esquecer_rede,
    .aleatorio      = hal_sd_aleatorio,
    .id_aparelho    = esp_id_aparelho,
    .espaco         = hal_sd_espaco,
    .uso_de         = hal_sd_uso_de,
    .audio_inicia   = hal_audio_inicia,
    .audio_pausa    = hal_audio_pausa,
    .audio_retoma   = hal_audio_retoma,
    .audio_fecha    = hal_audio_fecha,
    .audio_descarta = hal_audio_descarta,
    .wifi_procurar  = hal_wifi_procurar,
    .wifi_redes     = hal_wifi_redes,
    .wifi_conectar  = hal_wifi_conectar,
    .wifi_estado    = hal_wifi_estado,
    .wifi_falha     = hal_wifi_falha,
    .hora_da_rede   = hal_hora_da_rede,
    .hora_da_rede_para = hal_hora_da_rede_para,
    .fuso           = hal_esp_fuso,
    .bateria        = hal_esp_bateria,
    .docado         = esp_docado,
    .relogio        = esp_relogio,
    .ajustar_relogio = esp_ajustar_relogio,
    .registrar      = esp_registrar,
    .agora_ms       = esp_agora_ms,
    .reiniciar      = esp_reiniciar,
    .nuvem_pede       = esp_nuvem_pede,
    .nuvem_pede_audio = esp_nuvem_pede_audio,
    .nuvem_credencial = esp_nuvem_credencial,
    .segredo_grava    = esp_segredo_grava,
    .segredo_le       = esp_segredo_le,
    .segredo_apaga    = esp_segredo_apaga,
    .nuvem_resposta   = esp_nuvem_resposta,
    .nuvem_codigo     = esp_nuvem_codigo,
    .nuvem_linha      = esp_nuvem_linha,
};

// ── montagem ─────────────────────────────────────────────────────────
static void monta_eink(void)
{
    gpio_config_t saidas = {
        .pin_bit_mask = (1ULL << PIN_EINK_DC) | (1ULL << PIN_EINK_RST),
        .mode = GPIO_MODE_OUTPUT,
    };
    esp_err_t err = gpio_config(&saidas);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_EINK, "GPIOs de saída falharam: %s", esp_err_to_name(err));
        return;
    }

    gpio_config_t busy = {
        .pin_bit_mask = 1ULL << PIN_EINK_BUSY,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    err = gpio_config(&busy);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_EINK, "GPIO BUSY falhou: %s", esp_err_to_name(err));
        return;
    }

    spi_bus_config_t bus = {
        .sclk_io_num = PIN_EINK_SCK,
        .mosi_io_num = PIN_EINK_MOSI,
        .miso_io_num = -1,              // e-ink é write-only
        .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = 240 / 8 * 416 + 16,
    };
    err = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_EINK, "barramento SPI falhou: %s", esp_err_to_name(err));
        return;
    }

    spi_device_interface_config_t dev = {
        // 2 MHz: na protoboard os fios do SPI são jumpers de 20 cm sem terra ao
        // lado, e a 10 MHz isso é antena. Bit errado não dá erro, dá tinta no lugar
        // errado. O quadro inteiro leva ~50 ms contra 350 ms de tinta. Na PCB dá para
        // subir, medindo.
        .clock_speed_hz = 2 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_EINK_CS,
        .queue_size = 4,
    };
    err = spi_bus_add_device(SPI2_HOST, &dev, &eink);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_EINK, "dispositivo SPI falhou: %s", esp_err_to_name(err));
        return;
    }

    eink_pronto = eink_liga_do_zero();
    ESP_LOGI(TAG_EINK, "após reset: BUSY=%d", gpio_get_level(PIN_EINK_BUSY));
    ESP_LOGI(TAG_EINK, "inicialização %s", eink_pronto ? "ok" : "falhou");
}

static void monta_i2c(void)
{
    i2c_master_bus_config_t bus = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_new_master_bus(&bus, &i2c_bus);

    // Cala o driver antes da varredura: três linhas por endereço que não
    // responde, 112 endereços.
    esp_log_level_set("i2c.master", ESP_LOG_NONE);

    // ── varredura de endereços ───────────────────────────────────────────
    // Diagnóstico permanente. O expansor já esteve em outro endereço que o
    // esperado, e isso custou sete gravações e quatro teorias erradas, todas
    // mortas por esta varredura em 70 ms. Num barramento, a primeira pergunta é
    // SE alguém responde e EM QUE endereço.
    char achados[96];
    int  n = 0, k = 0;
    for (uint8_t a = 0x08; a <= 0x77; a++) {
        if (i2c_master_probe(i2c_bus, a, 20) != ESP_OK) continue;
        n++;
        if (k < (int)sizeof achados - 8)
            k += snprintf(achados + k, sizeof achados - k, " 0x%02x", a);
    }
    if (n) ESP_LOGW("i2c", "varredura achou %d:%s", n, achados);
    else   ESP_LOGE("i2c", "varredura NAO achou ninguem no barramento");

    // ── o expansor, no endereço em que ele ESTIVER ───────────────────────
    // Já respondeu em 0x22 e em 0x20 em boots diferentes (pino de endereço
    // flutuando). Sem ele, os botões morrem com a tela certa — parece software.
    // O endereço de `pins.h` tem prioridade e insiste cinco vezes: o barramento
    // já deu ACK fantasma em 0x14, 0x20 e 0x5c no mesmo boot. Só então procura na
    // família (0x20–0x27) e diz onde achou. Não conserta o barramento.
    uint8_t onde = ENDERECO_PCF8575;
    bool achou = false;
    for (int tenta = 0; tenta < 5 && !achou; tenta++)
        achou = i2c_master_probe(i2c_bus, onde, 20) == ESP_OK;

    if (!achou) {
        for (uint8_t a = 0x20; a <= 0x27; a++) {
            if (a == ENDERECO_PCF8575) continue;
            if (i2c_master_probe(i2c_bus, a, 20) != ESP_OK) continue;
            ESP_LOGW("entrada", "expansor NAO respondeu em 0x%02x; usando 0x%02x",
                     ENDERECO_PCF8575, a);
            onde = a;
            break;
        }
        if (onde == ENDERECO_PCF8575)
            ESP_LOGE("entrada", "nenhum expansor entre 0x20 e 0x27 — "
                                "os botoes nao vao responder");
    }
    pcf_onde = onde;

    i2c_device_config_t expansor = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = onde,
        .scl_speed_hz = 100000,
    };
    i2c_master_bus_add_device(i2c_bus, &expansor, &pcf);

    // ── o gauge só entra se responder ────────────────────────────────────
    // MAX17048: SOC por quem conhece a curva da célula (tensão de lítio é quase
    // plana entre 80% e 20%). Barramento único: chip ausente chamado sempre é
    // transação falhando junto com os botões. Pergunta uma vez.
    if (i2c_master_probe(i2c_bus, ENDERECO_MAX17048, 20) == ESP_OK) {
        i2c_device_config_t gauge = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = ENDERECO_MAX17048,
            .scl_speed_hz = 100000,
        };
        if (i2c_master_bus_add_device(i2c_bus, &gauge, &fuel) != ESP_OK)
            ESP_LOGW("tinto", "fuel gauge nao entrou no barramento");
    } else {
        ESP_LOGW("tinto", "fuel gauge ausente — barra de bateria vazia");
    }

    // Nenhuma escrita no PCF: no power-on as I/Os já estão HIGH e servem de
    // entrada (datasheet NXP, Quasi-bidirectional I/Os). Transação que não
    // precisa existir não pode falhar.
}

// Power acorda direto; o PCF pelo INT open-drain. Os dois em RTC-GPIO,
// ativos em LOW.
static void monta_fontes_de_wake(void)
{
    gpio_config_t c = {
        .pin_bit_mask = (1ULL << PIN_PCF_INT) | (1ULL << PIN_BOTAO_POWER),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&c);

    // Duas fontes, dois sonos: `ext1` acorda do deep sleep; `gpio_wakeup`, do
    // light sleep do bloqueio, onde ext1 não vale.
    esp_sleep_enable_ext1_wakeup(
        (1ULL << PIN_PCF_INT) | (1ULL << PIN_BOTAO_POWER),
        ESP_EXT1_WAKEUP_ANY_LOW);

    gpio_wakeup_enable(PIN_BOTAO_POWER, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(PIN_PCF_INT,     GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
}

static void trata_wake(void)
{
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT1) return;

    uint64_t origem = esp_sleep_get_ext1_wakeup_status();
    uint32_t apertados = 0;
    (void)entradas_le(&apertados);
    ignorar_ate_soltar = apertados;

    // Power ganha se os dois coincidirem: é o gesto explícito de desbloqueio.
    if (origem & (1ULL << PIN_BOTAO_POWER)) return;

    if (origem & (1ULL << PIN_PCF_INT)) {
        // O PCF diz que ALGO mudou, não qual porta. Se o clique acabou durante o
        // boot, o fail-safe é mostrar o aviso de voz, nunca gravar nem desbloquear.
        bool voz = (apertados & (1u << PCF_VOZ)) != 0 ||
                   (apertados & 0xFFFFu) == 0;
        evento_t ev = {
            .tipo = EV_ACORDOU_BLOQUEADO,
            .botao = voz ? IN_VOZ : IN_NADA,
        };
        hal_esp_empurra(&ev);
    }
}

const hal_t *hal_esp_liga(void)
{
    fila_eventos = xQueueCreate(32, sizeof(evento_t));

    monta_i2c();
    monta_fontes_de_wake();
    trata_wake();
    hal_sd_liga();
    monta_eink();

    // O microfone depois do cartão: sem onde escrever, gravar não faz sentido.
    if (hal_audio_liga() != OK)
        ESP_LOGE("tinto", "microfone não subiu: o ● vai recusar, não morrer");

    // O rádio sobe sem conectar: só varre quando pedem e só conecta quando
    // escolhem.
    if (hal_wifi_liga() != OK)
        ESP_LOGE("tinto", "rádio não subiu: a tela de redes vai sair vazia");

    hal_nuvem_liga();

    // Task própria: o debounce precisa de amostragem regular, e o laço do APP
    // não é. Ela só empurra evento; nunca toca o estado.
    xTaskCreate(task_entrada, "ENTRADA", 3072, NULL, 6,
                &task_entrada_handle);

    gpio_set_intr_type(PIN_PCF_INT, GPIO_INTR_NEGEDGE);
    gpio_set_intr_type(PIN_BOTAO_POWER, GPIO_INTR_ANYEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_PCF_INT, avisa_mudanca_de_entrada, NULL);
    gpio_isr_handler_add(PIN_BOTAO_POWER, avisa_mudanca_de_entrada, NULL);

    xTaskCreate(task_tick, "TICK", 2048, NULL, 4, NULL);

    return &HAL;
}
