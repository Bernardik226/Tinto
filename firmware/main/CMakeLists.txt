# As camadas puras entram INTEIRAS, por glob: elas são o mesmo código que
# roda no PC, e a lista à mão já divergiu do Makefile uma vez — seis
# arquivos novos compilavam no PC e faltavam no link da placa.
#
# O preço do glob é conhecido: arquivo novo não é notado sem um
# `idf.py reconfigure`. Vale, porque o outro preço é descobrir a falta no
# link, que é o lugar mais chato de descobrir qualquer coisa.
#
# hal_esp.c é o único arquivo desta lista que conhece a placa.
file(GLOB TINTO_FONTES
    "nucleo/*.c" "jogos/*.c" "dado/*.c" "tela/*.c" "uso/*.c" "vista/*.c" "ui/*.c"
    "app/*.c")

idf_component_register(
    SRCS "main.c" "hal/hal_esp.c" "hal/hal_sd.c" "hal/audio_esp.c" "hal/wifi_esp.c" "hal/hora_esp.c" "hal/nuvem_esp.c" "hal/segredo_esp.c" "hal/memoria_hal.c" "hal/uc8253.c" "hal/refresco.c" "bancada/eink.c" ${TINTO_FONTES}
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer fatfs sdmmc vfs app_update esp_driver_i2c
             esp_driver_spi esp_driver_gpio esp_driver_sdmmc
             esp_driver_i2s esp_wifi nvs_flash esp_netif esp_event esp_http_client
             # mbedtls entra pelo `esp_crt_bundle.h`: o HTTPS do backend
             # pina a CA RAIZ, e não a folha. Pinar a folha derrubaria
             # todos os aparelhos de campo no dia em que o provedor
             # renovar — e ele renova sozinho, sem avisar.
             mbedtls
)

# -Wno-format-truncation: TRUNCAR É A POLÍTICA (RN-B7), não um acidente.
# "Rejeitar por tamanho faz o dia da pessoa sumir da tela por causa de um
# nome comprido." O snprintf já trunca com segurança e sempre termina a
# string; o aviso existe pra quem NÃO quer truncar, que não é o caso aqui.
#
# A mesma flag e a mesma razão estão no Makefile do build de PC — se ela
# valesse num e não no outro, o mesmo código compilaria de um lado e não do
# outro, que é exatamente o que os dois builds existem pra impedir.
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-format-truncation)
