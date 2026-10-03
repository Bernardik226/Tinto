// main/pins.h — a ÚNICA fonte de número de pino.
// Sai da tabela do HARDWARE.md §3: são as trilhas da placa. Fora daqui, um
// `grep -rn "GPIO_NUM_"` tem de voltar vazio.
//
// PROIBIDOS:
//   26–37      flash SPI + PSRAM octal do N16R8 (falha intermitente)
//   19, 20     USB nativo (D-/D+)
//   43, 44     UART0, por onde sai o log
//   0,3,45,46  strapping/boot
//
// A câmera está desconectada, o que libera os GPIO 4–18 (DVP).
#ifndef PINS_H
#define PINS_H

// ── e-ink 3.7" 240×416 (WeAct GDEY037T03, UC8253) ───────────────────
// O BUSY ocupa o pino que seria MISO: e-ink é write-only.
#define PIN_EINK_SCK    41
#define PIN_EINK_MOSI   42
#define PIN_EINK_CS      1
#define PIN_EINK_DC     21
#define PIN_EINK_RST    14
#define PIN_EINK_BUSY   47

// ── cartão: SD-MMC 1-bit, no slot da própria placa ─────────────────
#define PIN_SD_CMD      38
#define PIN_SD_CLK      39
#define PIN_SD_D0       40

// ── áudio ────────────────────────────────────────────────────────────
// Só o microfone, no I2S RX. GPIO 7, 10 e 11 estão livres (não há saída de
// áudio).
#define PIN_MIC_SCK      5
#define PIN_MIC_WS       6
#define PIN_MIC_SD       4      // L/R do INMP441 em GND = canal esquerdo

// ── power: o único botão em GPIO direto ─────────────────────────────
// Acorda e desbloqueia. A voz mora no PCF e acorda pelo INT do expansor, mas
// só desbloqueia com o power.
#define PIN_BOTAO_POWER  2

// ── I2C: o expansor dos outros dez botões, e o fuel gauge ───────────
#define PIN_I2C_SDA     12
#define PIN_I2C_SCL     13
#define PIN_PCF_INT      9     // RTC, open-drain e pull-up externo de 10 kΩ

// 0x20 = A0, A1 e A2 em GND. Já respondeu em 0x22 (pino de endereço
// flutuando): o hal procura na família 0x20–0x27.
#define ENDERECO_PCF8575  0x20
#define ENDERECO_GAUGE    0x36

// ── cristal de 32.768 kHz ───────────────────────────────────────────
// Pinos fixos no silício.
#define PIN_XTAL_32K_P  15
#define PIN_XTAL_32K_N  16

// ── as 16 entradas do PCF8575 ───────────────────────────────────────
// Dez botões, o hall reservado e cinco livres. O hal traduz bit em papel
// (IN_CIMA, IN_OK…).
#define PCF_CIMA         0
#define PCF_BAIXO        1
#define PCF_ESQ          2
#define PCF_DIR          3
#define PCF_OK           4
#define PCF_MENU         5
#define PCF_VOLTAR       6
#define PCF_VOZ          8     // P10
#define PCF_HALL        11     // P13, reservado: a dock ainda não existe

#endif
