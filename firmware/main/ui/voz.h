// ui/voz.h — a faixa de voz.
// Retângulo FIXO, inclusive enquanto a transcrição chega: parcial de faixa,
// o único que se repete a cada segundo sem gastar a tinta (EINK §5.2).
//
//     0:07  ▁▃▅▂▆▃  → está gravando? há quanto tempo? está me ouvindo?
//     marca dentista quinta às  → entendeu o quê?
//     solte pra pausar          → e o que eu faço agora?
#ifndef UI_VOZ_H
#define UI_VOZ_H

#include "faixa.h"
#include "../vista/gravador.h"

// Por cima da tela. Quem limpa é a faixa, só o retângulo dela.
void ui_voz(bitmap_t *bm, const vista_grav_t *v);

// O gesto que não aconteceu, com o MOTIVO: cada um leva a um lugar
// ("conecte o Wi-Fi" não resolve quem ficou sem minutos). `gesto` no
// infinitivo; a frase é montada aqui, igual em todo o sistema.
void ui_faixa_recusa(bitmap_t *bm, const char *gesto, int motivo);

// ── a faixa do que ACABOU de acontecer ───────────────────────────────
// "Remarcado · sex 12 set", por cima, e some sozinha: sem ela a pessoa
// aperta OK de novo. A irmã positiva da recusa, na mesma peça.
void ui_faixa_feito(bitmap_t *bm, const char *o_que, const char *detalhe);

// ── o FIFO da transcrição ────────────────────────────────────────────
// Onde começar para o FIM do texto caber em `linhas` de largura `larg`
// (ponteiro dentro de `texto`). As primeiras palavras saem conforme as novas
// chegam. Anda por PALAVRA.
const char *ui_voz_visivel(const char *texto, int larg, int linhas);

// O retângulo que ela ocupa: o motor pede parcial de faixa, não de tela.
void ui_voz_area(int *x, int *y, int *l, int *a);

#endif
