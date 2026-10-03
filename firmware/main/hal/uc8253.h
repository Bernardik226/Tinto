#ifndef UC8253_H
#define UC8253_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    void *contexto;
    bool (*comando)(void *contexto, uint8_t valor);
    bool (*dados)(void *contexto, const uint8_t *valores, size_t quantos);
    bool (*espera)(void *contexto);
} uc8253_io_t;

// Como a tinta se move: waveforms diferentes, cada uma faz o que as outras
// não fazem.
typedef enum {
    // A waveform nativa, na temperatura real. Tem fases de inversão: é lenta
    // (~3 s) e é a ÚNICA que limpa resíduo.
    UC8253_COMPLETO,

    // Full rápido, pela faixa de temperatura forçada: repinta tudo de forma
    // diferencial e não limpa fantasma.
    UC8253_RAPIDO,

    // Parcial. O mais curto, e o que acumula resíduo.
    UC8253_PARCIAL,
} uc8253_modo_t;

// O que só o vidro decide: o PSR (modo + bits UD/SHL da orientação) e se o
// RAM do painel usa 0 = tinta. Antes de uc8253_inicia; sem chamar, defaults.
void uc8253_configura(uint8_t psr, bool inverte);

bool uc8253_inicia(const uc8253_io_t *io);

// Desliga o booster (POF), só ao dormir. Entre refreshes ele fica ligado:
// mantém o plano velho com que o controlador compara o quadro seguinte.
// Desligar a cada quadro sobrepunha o anterior ao novo.
bool uc8253_desliga(const uc8253_io_t *io);

// `tem_referencia` diz se o plano velho do controlador corresponde ao vidro.
// Quem mantém o plano velho é o driver, reescrevendo o MESMO quadro no 0x10
// depois do refresh (como o driver oficial). false só depois de ligar,
// resetar ou dormir: o plano vai branco e o refresh é completo
// (docs/EINK.md §3.1).
bool uc8253_atualiza(const uc8253_io_t *io, const uint8_t *bits,
                     bool tem_referencia, size_t bytes,
                     uc8253_modo_t modo);

// ── o refresh de uma FAIXA ───────────────────────────────────────────
// Barra e rodapé mudam sozinhos; repintar 240×416 para dois dígitos gasta
// 600 ms. Faixa de LINHAS inteiras: a janela do UC8253 alinha o horizontal em
// blocos de 8 px. Com PSR 0x1F (sem espelho) a faixa do quadro é a do painel;
// em 0x13 a linha `y` seria a `altura-1-y`.
typedef struct {
    int y0, y1;   // primeira e última linha, inclusivas
} uc8253_faixa_t;

bool uc8253_atualiza_faixa(const uc8253_io_t *io, const uint8_t *bits,
                           size_t bytes, size_t passo,
                           const uc8253_faixa_t *faixa);

bool uc8253_limpa(const uc8253_io_t *io, size_t bytes, int passagens);

#endif
