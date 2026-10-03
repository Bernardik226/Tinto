#include "pagina.h"
#include "chrome.h"
#include "../tela/texto.h"
#include <stdio.h>

int pagina_total(int alto, int area)
{
    if (area <= 0 || alto <= area) return 1;
    return (alto + area - 1) / area;
}

int pagina_atual(int desloc, int alto, int area)
{
    if (area <= 0) return 1;
    int n = desloc / area + 1;
    int total = pagina_total(alto, area);
    return n > total ? total : n;
}

void pagina_marca(bitmap_t *bm, int atual, int total)
{
    (void)bm;
    // Quem desenha é o rodapé: só ele reserva o centro sem colar num rótulo.
    chrome_rodape_pagina(atual, total);
}
