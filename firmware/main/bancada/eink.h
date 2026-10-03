// bancada/eink.h — o padrão que responde de que lado o painel está.
// A orientação (dois bits do PSR) só o vidro responde: as combinações passam
// numeradas, e quem olha diz um número. Não faz parte do aparelho: só roda
// com BANCADA_EINK ligado.
#ifndef BANCADA_EINK_H
#define BANCADA_EINK_H

// Ligue em 1 ao trocar de vidro: o aparelho não roda e a bancada varre as
// orientações. Desligada, não ocupa RAM (o padrão é um framebuffer
// inteiro).
#define BANCADA_EINK 0

void bancada_eink_roda(void);

#endif
