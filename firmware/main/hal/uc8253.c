#include "uc8253.h"

#define PAINEL_LARGURA 240
#define PAINEL_ALTURA  416

// ── a orientação é do PAINEL, não do framebuffer ─────────────────────
// O PSR (R00H) tem UD (bit 3, linhas sobem ou descem) e SHL (bit 2, colunas
// para a direita ou esquerda). Girar a imagem é escolher esses bits, não
// reescrever o quadro. Com o resto igual (KW, LUT do OTP, booster):
//
//   0x1F  natural do controlador — este. Único em que o texto se lê no vidro.
//   0x1B  espelhado na horizontal
//   0x17  de cabeça para baixo
//   0x13  girado 180°
//
// Se o painel mudar de lado na placa, só este número muda (a bancada em
// bancada/eink.h varre as opções). O segundo byte (0x0D) são as tensões, do
// demo da Good Display.
#define PSR_ORIENTACAO 0x1F

// Orientação e TRES ajustáveis em runtime para a bancada varrê-los; o
// default é o de cima.
static uint8_t psr_orientacao = PSR_ORIENTACAO;

// Se o RAM do painel usa 0 = tinta. Medido varrendo as oito combinações:
// sem inversão sai negativo. O framebuffer usa 1 = tinta, então todo byte é
// invertido no caminho.
static bool ram_invertido = true;

void uc8253_configura(uint8_t psr, bool inverte)
{
    psr_orientacao = psr;
    ram_invertido  = inverte;
}

static uint8_t ram_preto(void)  { return ram_invertido ? 0x00 : 0xFF; }
static uint8_t ram_branco(void) { return ram_invertido ? 0xFF : 0x00; }

static bool io_valida(const uc8253_io_t *io)
{
    return io && io->comando && io->dados && io->espera;
}

static bool registro(const uc8253_io_t *io, uint8_t comando, uint8_t valor)
{
    return io->comando(io->contexto, comando) &&
           io->dados(io->contexto, &valor, 1);
}

static bool quadro(const uc8253_io_t *io, const uint8_t *bits, size_t bytes)
{
    // Sem inversão, uma transferência só. O alinhamento para DMA é garantido
    // pelo hal, por onde todo byte passa.
    if (!ram_invertido) return io->dados(io->contexto, bits, bytes);

    // O DMA do SPI do S3 exige endereço alinhado a 4; uint8_t na pilha é 1.
    _Alignas(4) uint8_t bloco[240];
    size_t escrito = 0;
    while (escrito < bytes) {
        size_t n = bytes - escrito;
        if (n > sizeof bloco) n = sizeof bloco;
        for (size_t i = 0; i < n; i++)
            bloco[i] = (uint8_t)~bits[escrito + i];
        if (!io->dados(io->contexto, bloco, n)) return false;
        escrito += n;
    }
    return true;
}

// O mesmo registrador com RST_N=0 é soft reset: registradores ao default.
// Uma vez, na inicialização. Fora dali apaga resolução e direção de
// varredura, e o quadro seguinte sai espelhado ou em blocos.
#define PSR_RST_N 0x01u

// ── a configuração, mandada junto e nesta ordem ──────────────────────
static bool painel_config(const uint8_t *psr, size_t psr_bytes,
                          const uc8253_io_t *io)
{
    return io->comando(io->contexto, 0x00) &&
           io->dados(io->contexto, psr, psr_bytes);
}

// Tudo o que o painel precisa antes de mover tinta, junto: separar PSR de
// resolução foi como a resolução deixou de ser mandada.
static bool painel_base(const uc8253_io_t *io)
{
    const uint8_t psr[] = { psr_orientacao, 0x0D };

    // TRES (R61H): quantos pixels o controlador varre. Sem isto, o default do
    // silício desloca cada linha (o "rasgo").
    const uint8_t tres[] = {
        (uint8_t)PAINEL_LARGURA,
        (uint8_t)(PAINEL_ALTURA >> 8), (uint8_t)(PAINEL_ALTURA & 0xFF),
    };

    return painel_config(psr, sizeof psr, io) &&
           io->comando(io->contexto, 0x61) &&
           io->dados(io->contexto, tres, sizeof tres);
}

bool uc8253_inicia(const uc8253_io_t *io)
{
    if (!io_valida(io)) return false;

    // O soft reset não é instantâneo: configurar em cima dele perdia PSR e
    // resolução. Espera o BUSY no meio.
    const uint8_t reset[] = { (uint8_t)(psr_orientacao & ~PSR_RST_N), 0x0D };
    return painel_config(reset, sizeof reset, io) &&
           io->espera(io->contexto) &&
           painel_base(io);
}

bool uc8253_desliga(const uc8253_io_t *io)
{
    // POF (0x02), só aqui: entre refreshes o painel fica ligado, segurando o
    // booster e o plano velho.
    if (!io_valida(io)) return false;
    return io->comando(io->contexto, 0x02) && io->espera(io->contexto);
}

// Um plano inteiro de um valor, em bytes CRUS do painel.
static bool plano_solido(const uc8253_io_t *io, uint8_t valor, size_t bytes)
{
    _Alignas(4) uint8_t bloco[240];
    for (size_t i = 0; i < sizeof bloco; i++) bloco[i] = valor;
    while (bytes > 0) {
        size_t n = bytes < sizeof bloco ? bytes : sizeof bloco;
        if (!io->dados(io->contexto, bloco, n)) return false;
        bytes -= n;
    }
    return true;
}

// O plano velho sem quadro anterior: branco, o que o vidro tem depois da
// faxina.
static bool plano_branco(const uc8253_io_t *io, size_t bytes)
{
    return plano_solido(io, ram_branco(), bytes);
}

// ── o modo parcial, como o painel pede ───────────────────────────────
// O UC8253 só faz parcial DENTRO do modo parcial: PTIN (0x91) entra, PTL
// (0x90) dá o retângulo, PTOUT (0x92) sai. Fora dele o 0x12 roda o ciclo
// normal. Receita do driver oficial (GxEPD2_370_GDEY037T03, `_Update_Part`).
// As tentativas por waveform que falharam estão no EINK.md §8.

// A janela é a tela inteira: quem decide o que se move é a comparação entre
// os planos; o recorte só diz quais gates varrer.
static bool janela_inteira(const uc8253_io_t *io)
{
    static const uint8_t janela[] = {
        0x00,                                     // HRST: banco inicial
        (uint8_t)((PAINEL_LARGURA - 1) | 0x07),   // HRED: final, inclusive
        0x00, 0x00,                               // VRST[8:0]
        (uint8_t)((PAINEL_ALTURA - 1) >> 8),
        (uint8_t)((PAINEL_ALTURA - 1) & 0xFF),    // VRED[8:0]
        0x01,                                     // PT_SCAN: varre tudo
    };
    return io->comando(io->contexto, 0x90) &&
           io->dados(io->contexto, janela, sizeof janela);
}

static bool entra_parcial(const uc8253_io_t *io)
{
    return io->comando(io->contexto, 0x91) && janela_inteira(io);
}

static bool sai_parcial(const uc8253_io_t *io)
{
    return io->comando(io->contexto, 0x92);
}

// ── toda escrita de plano mora dentro de uma janela ──────────────────
// Como o `_writeImage` oficial: 0x91, 0x90, plano, 0x92 — sempre, mesmo antes
// de um completo. A janela diz ONDE no SRAM os bytes caem; sem ela, vale a
// última janela definida. O refresh é outra sessão (docs/EINK.md §3.3).
static bool escreve_plano(const uc8253_io_t *io, uint8_t plano,
                          const uint8_t *bits, size_t bytes)
{
    return entra_parcial(io) &&
           io->comando(io->contexto, plano) &&
           quadro(io, bits, bytes) &&
           sai_parcial(io);
}

static bool escreve_plano_branco(const uc8253_io_t *io, size_t bytes)
{
    return entra_parcial(io) &&
           io->comando(io->contexto, 0x10) &&
           plano_branco(io, bytes) &&
           sai_parcial(io);
}

// ── a temperatura forçada não pode sobrar para o ciclo seguinte ──────
// O TSFIX (CCSET 0x02 + TSSET) troca o sensor pela faixa do registrador, cada
// uma com sua waveform no OTP. Pendurado, o ciclo seguinte roda a waveform do
// anterior. Como o `_InitDisplay()` oficial: soft reset e a base inteira de
// novo (inclusive a resolução, que o reset apaga).
//
// [HIPÓTESE, não medida] O SRAM sobrevive ao soft reset (o datasheet só fala
// de perda no SHD_N). Se o parcial somar quadros logo depois de outro
// parcial, olhar aqui primeiro (docs/EINK.md §3.4).
static bool desfaz_temperatura(const uc8253_io_t *io)
{
    const uint8_t reset[] = { (uint8_t)(psr_orientacao & ~PSR_RST_N), 0x0D };
    return painel_config(reset, sizeof reset, io) &&
           io->espera(io->contexto) &&
           painel_base(io);
}

// O ciclo que move a tinta, comum aos três modos; muda só a waveform pedida.
static bool dispara(const uc8253_io_t *io, uc8253_modo_t modo)
{
    // TSFIX: 0x6E é a tabela curta do parcial, 0x5A a do full rápido (números do
    // driver oficial). O COMPLETO escreve CCSET=0x00, que é como se DESLIGA o
    // TSFIX — sem soft reset, que levava junto resolução e orientação.
    if (modo == UC8253_PARCIAL || modo == UC8253_RAPIDO) {
        uint8_t temperatura = (modo == UC8253_PARCIAL) ? 0x6E : 0x5A;
        if (!registro(io, 0xE0, 0x02) || !registro(io, 0xE5, temperatura))
            return false;
    } else if (!registro(io, 0xE0, 0x00)) {
        return false;
    }

    uint8_t vcom = (modo == UC8253_PARCIAL) ? 0xD7 : 0x97;

    // PON (0x04), DRF (0x12) e POF (0x02), sempre. A tinta só assenta quando o
    // booster desliga no fim do ciclo; sem POF, o quadro aparecia um comando
    // atrasado (EINK.md §8). Não repetir.
    if (!registro(io, 0x50, vcom) ||
        !io->comando(io->contexto, 0x04) || !io->espera(io->contexto) ||
        !io->comando(io->contexto, 0x12) || !io->espera(io->contexto) ||
        !io->comando(io->contexto, 0x02) || !io->espera(io->contexto))
        return false;

    // Só quem acendeu o TSFIX o apaga; o completo já roda com o sensor.
    if (modo == UC8253_PARCIAL || modo == UC8253_RAPIDO)
        return desfaz_temperatura(io);

    return true;
}

// A janela de uma faixa: a largura inteira, e só as linhas pedidas.
static bool janela_da_faixa(const uc8253_io_t *io, const uc8253_faixa_t *f)
{
    const uint8_t janela[] = {
        0x00,                                     // HRST: do primeiro banco
        (uint8_t)((PAINEL_LARGURA - 1) | 0x07),   // HRED: até o último
        (uint8_t)(f->y0 >> 8), (uint8_t)(f->y0 & 0xFF),
        (uint8_t)(f->y1 >> 8), (uint8_t)(f->y1 & 0xFF),
        0x01,                                     // PT_SCAN
    };
    return io->comando(io->contexto, 0x90) &&
           io->dados(io->contexto, janela, sizeof janela);
}

// Só as linhas da faixa atravessam o SPI. O controlador escreve os bytes NA
// JANELA: o quadro inteiro numa janela pequena a encheria com o começo do
// buffer.
static bool linhas_da_faixa(const uc8253_io_t *io, const uint8_t *bits,
                            size_t passo, const uc8253_faixa_t *f)
{
    for (int y = f->y0; y <= f->y1; y++) {
        if (!quadro(io, bits + (size_t)y * passo, passo)) return false;
    }
    return true;
}

static bool plano_da_faixa(const uc8253_io_t *io, uint8_t plano,
                           const uint8_t *bits, size_t passo,
                           const uc8253_faixa_t *f)
{
    return io->comando(io->contexto, 0x91) &&
           janela_da_faixa(io, f) &&
           io->comando(io->contexto, plano) &&
           linhas_da_faixa(io, bits, passo, f) &&
           io->comando(io->contexto, 0x92);
}

bool uc8253_atualiza_faixa(const uc8253_io_t *io, const uint8_t *bits,
                           size_t bytes, size_t passo,
                           const uc8253_faixa_t *faixa)
{
    if (!io_valida(io) || !bits || !faixa || passo == 0) return false;
    if (faixa->y0 < 0 || faixa->y1 < faixa->y0) return false;
    if ((size_t)(faixa->y1 + 1) * passo > bytes) return false;

    // A mesma ordem do quadro inteiro: 0x13, refresh, 0x10. O plano velho fecha
    // o ciclo porque é ele que o parcial seguinte compara.
    if (!plano_da_faixa(io, 0x13, bits, passo, faixa)) return false;

    if (!io->comando(io->contexto, 0x91)) return false;
    if (!janela_da_faixa(io, faixa)) return false;
    if (!dispara(io, UC8253_PARCIAL)) return false;
    if (!io->comando(io->contexto, 0x92)) return false;

    return plano_da_faixa(io, 0x10, bits, passo, faixa);
}

bool uc8253_limpa(const uc8253_io_t *io, size_t bytes, int passagens)
{
    if (!io_valida(io) || bytes == 0) return false;

    // Preto e depois branco, os dois planos com o MESMO valor: não é
    // diferencial, é levar o vidro a um extremo pelas fases de inversão (como o
    // clearScreen oficial).
    for (int i = 0; i < passagens; i++) {
        const uint8_t extremos[] = { ram_preto(), ram_branco() };
        for (size_t e = 0; e < sizeof extremos; e++) {
            if (!io->comando(io->contexto, 0x10) ||
                !plano_solido(io, extremos[e], bytes) ||
                !io->comando(io->contexto, 0x13) ||
                !plano_solido(io, extremos[e], bytes) ||
                !dispara(io, UC8253_COMPLETO))
                return false;
        }
    }
    return true;
}

bool uc8253_atualiza(const uc8253_io_t *io, const uint8_t *bits,
                     bool tem_referencia, size_t bytes,
                     uc8253_modo_t modo)
{
    if (!io_valida(io) || !bits || bytes == 0) return false;

    // ── quem é dono do plano velho ───────────────────────────────────────
    // O controlador compara o velho (0x10) com o novo (0x13) e o par escolhe a LUT
    // de cada pixel: diferentes empurram, iguais seguram. Vale nos dois modos —
    // o completo também é diferencial, só mais longo (EINK.md §5.5). Regra:
    //
    //     o plano velho tem de ser SEMPRE o quadro que está no vidro.
    //
    // Nunca branco "para forçar", nunca igual ao novo "para neutralizar": as duas
    // mancharam a tela (EINK.md §8). O controlador não copia o novo sobre o velho
    // sozinho; a última linha desta função faz isso (o `writeImageAgain` oficial).
    bool parcial = (modo == UC8253_PARCIAL);

    // A ordem do `nextPage`: 0x13, refresh, 0x10. O velho escrito antes valeria
    // para este ciclo, que precisa comparar contra o quadro anterior.
    if (!tem_referencia) {
        // Só no primeiro quadro, sem referência: o vidro está branco depois da
        // faxina.
        if (!escreve_plano_branco(io, bytes)) return false;
    }

    if (!escreve_plano(io, 0x13, bits, bytes)) return false;

    // O refresh parcial é a sua própria sessão (PTIN, janela, ciclo, PTOUT). O
    // completo não tem sessão: repinta o vidro inteiro.
    if (parcial && !entra_parcial(io)) return false;
    if (!dispara(io, modo)) return false;
    if (parcial && !sai_parcial(io)) return false;

    // O fechamento: o plano velho passa a ser este quadro, o que está no vidro.
    return escreve_plano(io, 0x10, bits, bytes);
}
