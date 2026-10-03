// ui/cartao.h — o card do sistema, desenhado (Minha conta, Conexão,
// Sincronização, Armazenamento, Sobre, Aparência).
#ifndef UI_CARTAO_H
#define UI_CARTAO_H

#include "../vista/cartao.h"
#include "../tela/bitmap.h"

void tela_cartao(bitmap_t *bm, const vista_cartao_t *v);

// Quanto o corpo mede e quanto cabe. A tela não rola: o teste quebra se
// algum estado passar do quadro.
int  tela_cartao_altura(const bitmap_t *bm, const vista_cartao_t *v);
int  tela_cartao_area(const bitmap_t *bm);

#endif
