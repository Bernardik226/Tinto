#ifndef TELA_MAPA_H
#define TELA_MAPA_H

// O que cada tela É: as propriedades que o motor de quadros precisa e não
// se leem do bitmap. A navegação é a pilha do `estado_t`.

#include <stdbool.h>
#include "nucleo/estado.h"

// A tela desenha um seletor? Decide se trocar de tela custa um quadro ou
// dois (EINK.md §5.5). Default `true`: errar para mais custa CPU (o painel
// descarta o quadro idêntico); errar para menos congela o seletor no vidro.
bool tela_tem_seletor(tela_id t);

// Toda tela do enum foi CONSIDERADA aqui? O default de cima engole tela
// nova calado; esta pergunta não tem default, e o teste de cobertura
// reprova o build quando a resposta é não.
bool tela_declarada(tela_id t);

#endif
