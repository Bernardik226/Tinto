# Tinto — build de placa. O de PC é o Makefile, e os dois compilam O MESMO
# código: só muda quem preenche o hal_t (CAMADAS §5).
cmake_minimum_required(VERSION 3.16)

# ENGENHARIA §5: a versão do ESP-IDF é fixada aqui e no CI. Atualizar IDF é
# um commit próprio, nunca de carona — "funciona na minha máquina" com
# firmware em campo é caro demais.
set(TINTO_IDF_ESPERADO "5.3")

set(PROJECT_VER "1.0.0")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tinto)
