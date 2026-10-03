#ifndef REFRESCO_H
#define REFRESCO_H

// Como o painel pinta cada quadro. Decisão de experiência, fora do driver,
// para ter teste no PC.
//
//   trocar de tela → flash branco + página, ambos parciais (~700 ms)
//   todo o resto   → parcial
//
// Sem completo periódico: contaria parciais, e a maioria é o cursor andando —
// piscaria no meio da navegação. O rastro do cursor se conserta no gesto,
// pela janela do cursor.
//
// Já testado e rejeitado no vidro (não voltar sem olhar o vidro):
// - full RÁPIDO (TSFIX 0x5A) na troca de tela: pouco contraste e piscando.
// - assentamento 600 ms depois da última interação: uma piscada periódica é
//   pior que o rastro que ela limpa.

#include <stdbool.h>
#include <stddef.h>

// ── a intenção do quadro ─────────────────────────────────────────────
// O que o gesto significa, não quanto pixel mudou. A regra do 1/3 é para a
// tela que ganha linhas sozinha; no FOCO ela erraria (mover o cartão da Home
// mede 30,8–33,0% contra um corte de 33,3%).
typedef enum {
    // A tela mudou: a HAL faz flash branco curto + página. `REFRESCO_COMPLETO`
    // aqui não é a waveform longa.
    PINTURA_TELA_NOVA = 0,

    // A mesma tela mudando sozinha: aqui a medida decide (regra do 1/3).
    PINTURA_MESMA_TELA,

    // Movimento de foco ou cursor: parcial garantido.
    PINTURA_FOCO,

} pintura_t;

typedef enum {
    // ~350 ms. O padrão do sistema, e o caminho de quase toda interação.
    REFRESCO_PARCIAL,

    // Transição. Com quadro anterior: flash branco + parcial (~700 ms); sem
    // referência, a waveform nativa (~2,5 s).
    REFRESCO_COMPLETO,
} refresco_t;

// ── quando MUITO muda, o parcial deixa de servir ─────────────────────
// Na mesma tela (Wi-Fi achando redes), um parcial sobre dois terços do painel
// deixa o quadro velho aparecendo por baixo. Acima de 1/3 da tela ele custa
// quase o mesmo que o completo e sai pior — o mesmo teto que a janela de faixa
// usa para desistir de recortar.
#define REFRESCO_MUITO_NUM 1
#define REFRESCO_MUITO_DEN 3


refresco_t refresco_por_intencao(pintura_t intencao, bool tem_anterior,
                                 size_t bytes_mudados, size_t bytes_total);

refresco_t refresco_escolhe_medindo(bool trocou_de_tela, bool tem_anterior,
                                    size_t bytes_mudados, size_t bytes_total);

// ── a dívida ─────────────────────────────────────────────────────────
// Contador de parciais desde o último completo, só para a serial: não decide
// nada. Se o vidro pedir completo periódico, o gatilho é tempo de tinta
// parada ou trocas de tela, nunca o cursor.
typedef struct {
    int parciais_seguidos;
} refresco_estado_t;

#endif
