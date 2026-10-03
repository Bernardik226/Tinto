// hal/hora_esp.h — o relógio pela rede (SNTP).
//
// Arquivo próprio porque o lwip declara `ERR_TIMEOUT`, como o `erro_t`: este
// não inclui os tipos do sistema e avisa o app por uma função.
#ifndef HAL_HORA_ESP_H
#define HAL_HORA_ESP_H

#include <stdbool.h>

void hal_hora_da_rede(void);

// Derruba o cliente NTP quando a pessoa desliga a hora pela rede. Um
// cliente vivo faria o próximo `init` abortar.
void hal_hora_da_rede_para(void);

// Implementada no hal_esp.c: empurra EV_HORA_DA_REDE na fila do app.
void hal_esp_empurra_hora(void);

// Implementada no wifi_esp.c: sem endereço não há a quem perguntar a hora.
bool hal_wifi_tem_ip(void);

#endif
