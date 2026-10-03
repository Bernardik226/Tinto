// hal/memoria_hal.h — de onde vem cada byte.
//
//   RAM interna  512 KB. A única que o DMA alcança: driver do cartão, Wi-Fi,
//                TLS, pilhas. Faltando, quebra quem não tem nada a ver.
//   PSRAM        8 MB, octal. Sem DMA nem ISR, ~3× mais lenta. Para o resto.
//   flash        16 MB, dois apps de 3 MB. Código e constantes.
//   cartão       O dado da pessoa.
//
// Regra: **buffer grande e temporário mora na PSRAM, e é devolvido.** Um
// static de 16 KB para o upload já tirou o buffer de DMA do cartão e abriu a
// tela de reparo sem nada quebrado. Quem precisa de DMA usa
// `heap_caps_malloc` com o flag.
#ifndef HAL_MEMORIA_HAL_H
#define HAL_MEMORIA_HAL_H

#include <stddef.h>

// Um buffer grande e temporário: PSRAM primeiro; sem ela, a interna, e aí
// vale o `minimo`. NULL só quando nem o mínimo cabe; `tamanho_real` pode ser
// menos do que se pediu.
void *mem_emprestada(size_t desejado, size_t minimo, size_t *tamanho_real);

// Devolve o que `mem_emprestada` deu. NULL é aceito e não faz nada.
void  mem_devolve(void *p);

// O que sobra da memória sem substituto: decide se o cartão monta.
size_t mem_interna_livre(void);

// Mostrada ao lado da interna: juntas dizem se a PSRAM subiu.
size_t mem_psram_livre(void);

#endif
