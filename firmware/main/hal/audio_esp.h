// hal/audio_esp.h — o microfone da placa, atrás do papel `audio_*` do hal.
#ifndef HAL_AUDIO_ESP_H
#define HAL_AUDIO_ESP_H

#include "../nucleo/tipos.h"

erro_t hal_audio_liga(void);

erro_t hal_audio_inicia(const char *caminho);
erro_t hal_audio_pausa(void);
erro_t hal_audio_retoma(void);
erro_t hal_audio_fecha(int *dur_s, int *trechos);
erro_t hal_audio_descarta(void);

#endif
