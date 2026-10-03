// hal/wifi_esp.h — o rádio da placa, atrás do papel `wifi_*` do hal.
// Nada bloqueia o APP: pede-se, e a resposta chega como evento (RN-41).
#ifndef HAL_WIFI_ESP_H
#define HAL_WIFI_ESP_H

#include "../nucleo/tipos.h"

erro_t hal_wifi_liga(void);

void   hal_wifi_procurar(void);
int    hal_wifi_redes(rede_wifi_t *out, int max);
erro_t hal_wifi_conectar(const char *nome, const char *senha);

// 0 nenhuma, 1 senha recusada, 2 sem resposta. Espelha `wifi_falha_t` sem
// incluí-lo.
int    hal_wifi_falha(void);
int    hal_wifi_estado(char *ip, size_t max, int *forca);
bool   hal_wifi_tem_ip(void);

// Apaga SSID e senha da NVS do rádio. Diferente de desconectar: é o que
// "Apagar e preparar" precisa, para o aparelho não levar a senha da casa
// anterior.
void hal_wifi_esquecer(void);

#endif
