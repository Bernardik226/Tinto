// vista/wifi.h — T-28a, as redes ao alcance.
// Usa a mesma vista do menu. O nome da rede nunca se digita, vem da lista; e
// sem rede o aparelho funciona inteiro, e a tela diz isso.
#ifndef VISTA_WIFI_H
#define VISTA_WIFI_H

#include "menu.h"


// O ícone do sinal por força (0-100), na mesma régua do texto.
icone_id vista_icone_wifi(int forca);

void vista_wifi(const estado_t *e, int cabem, vista_menu_t *out);

#endif
