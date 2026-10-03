// firmware/testes/t_leitor.c — paginar sem perder o lugar.
// A posição canônica é o OFFSET no texto; página e porcentagem derivam
// dela.
#include "teste.h"
#include "nucleo/leitor.h"

// Acento e palavra comprida: UTF-8 é o caso normal em português.
static const char *TEXTO =
    "Continuei até a esquina. O céu estava límpido e a rua, silenciosa. "
    "Pensei em voltar, mas a inquietação não me deixava parar. "
    "Caminhei mais um quarteirão, contando os passos, até que a "
    "extraordinariamente comprida avenida terminasse. ";

void t_leitor_pagina_sem_cortar_palavra(void)
{
    COMECA("leitor · a página termina em palavra inteira");

    leitor_t l;
    leitor_abre(&l, TEXTO, (int32_t)strlen(TEXTO), 0);

    char pagina[LEITOR_PAGINA];
    int32_t fim = leitor_pagina(&l, pagina, sizeof pagina);

    ESPERA(fim > 0);
    ESPERA(pagina[0] != '\0');

    // O corte cai num espaço, não no meio de uma palavra.
    if (fim < l.tamanho)
        ESPERA(TEXTO[fim] == ' ' || TEXTO[fim - 1] == ' ');
    TERMINA();
}

void t_leitor_nao_corta_caractere_utf8(void)
{
    COMECA("leitor · a página nunca termina no meio de um acento");

    leitor_t l;
    leitor_abre(&l, TEXTO, (int32_t)strlen(TEXTO), 0);

    for (int p = 0; p < 6 && !leitor_no_fim(&l); p++) {
        char pagina[LEITOR_PAGINA];
        (void)leitor_pagina(&l, pagina, sizeof pagina);

        // Nenhum byte de continuação solto no fim.
        size_t n = strlen(pagina);
        if (n) ESPERA((pagina[n - 1] & 0xC0) != 0x80);
        leitor_avanca(&l);
    }
    TERMINA();
}

// ── trocar a fonte NÃO move a pessoa de lugar ──────────────────────
void t_leitor_trocar_a_fonte_preserva_a_posicao(void)
{
    COMECA("leitor · mudar o tamanho da letra não muda onde se estava");

    leitor_t l;
    leitor_abre(&l, TEXTO, (int32_t)strlen(TEXTO), 0);

    leitor_avanca(&l);
    leitor_avanca(&l);
    int32_t onde = leitor_posicao(&l);
    ESPERA(onde > 0);

    leitor_recompoe(&l, LEITOR_GRANDE);
    ESPERA_IGUAL(leitor_posicao(&l), onde);

    leitor_recompoe(&l, LEITOR_PEQUENA);
    ESPERA_IGUAL(leitor_posicao(&l), onde);
    TERMINA();
}

// ── a letra maior cabe MENOS na página ─────────────────────────────
void t_leitor_a_letra_maior_pagina_menos(void)
{
    COMECA("leitor · a letra maior põe menos texto na página");

    // Um texto LONGO: com um curto, as duas fontes cabem numa página só.
    static char longo[6000];
    longo[0] = '\0';
    for (int i = 0; i < 12; i++) strncat(longo, TEXTO, sizeof longo - strlen(longo) - 1);

    leitor_t pequena, grande;
    leitor_abre(&pequena, longo, (int32_t)strlen(longo), 0);
    leitor_abre(&grande,  longo, (int32_t)strlen(longo), 0);

    leitor_recompoe(&pequena, LEITOR_PEQUENA);
    leitor_recompoe(&grande,  LEITOR_GRANDE);

    char a[LEITOR_PAGINA], b[LEITOR_PAGINA];
    int32_t fim_p = leitor_pagina(&pequena, a, sizeof a);
    int32_t fim_g = leitor_pagina(&grande,  b, sizeof b);

    ESPERA(fim_p > fim_g);
    TERMINA();
}

// ── voltar é o inverso exato de avançar ────────────────────────────
void t_leitor_voltar_devolve_a_mesma_pagina(void)
{
    COMECA("leitor · voltar devolve exatamente a página anterior");

    leitor_t l;
    leitor_abre(&l, TEXTO, (int32_t)strlen(TEXTO), 0);

    char primeira[LEITOR_PAGINA];
    (void)leitor_pagina(&l, primeira, sizeof primeira);

    leitor_avanca(&l);
    leitor_volta(&l);

    char devolvida[LEITOR_PAGINA];
    (void)leitor_pagina(&l, devolvida, sizeof devolvida);

    ESPERA_TEXTO(devolvida, primeira);
    ESPERA_IGUAL(leitor_posicao(&l), 0);
    TERMINA();
}

// ── o fim é o FIM ───────────────────────────────────────────────────
void t_leitor_o_fim_nao_passa_do_fim(void)
{
    COMECA("leitor · avançar na última página não sai do texto");

    leitor_t l;
    leitor_abre(&l, TEXTO, (int32_t)strlen(TEXTO), 0);

    for (int i = 0; i < 200 && !leitor_no_fim(&l); i++) leitor_avanca(&l);

    ESPERA(leitor_no_fim(&l));
    int32_t onde = leitor_posicao(&l);
    leitor_avanca(&l);
    ESPERA_IGUAL(leitor_posicao(&l), onde);
    TERMINA();
}

void t_leitor_a_ultima_pagina_nunca_vira_branco(void)
{
    COMECA("leitor · avançar no fim mantém a última página visível");
    leitor_t l;
    leitor_abre(&l, TEXTO, (int32_t)strlen(TEXTO), 0);
    char ultima[LEITOR_PAGINA];
    for (int i = 0; i < 200; i++) {
        (void)leitor_pagina(&l, ultima, sizeof ultima);
        ESPERA(ultima[0] != '\0');
        if (leitor_no_fim(&l)) break;
        leitor_avanca(&l);
    }
    int32_t onde = leitor_posicao(&l);
    leitor_avanca(&l);
    char ainda[LEITOR_PAGINA];
    (void)leitor_pagina(&l, ainda, sizeof ainda);
    ESPERA_IGUAL(leitor_posicao(&l), onde);
    ESPERA_TEXTO(ainda, ultima);
    TERMINA();
}

// ── a porcentagem é do TEXTO ────────────────────────────────────────
void t_leitor_a_porcentagem_vem_do_offset(void)
{
    COMECA("leitor · a porcentagem é do texto, não da contagem de páginas");

    leitor_t l;
    leitor_abre(&l, TEXTO, (int32_t)strlen(TEXTO), 0);
    ESPERA_IGUAL(leitor_pct(&l), 0);

    for (int i = 0; i < 200 && !leitor_no_fim(&l); i++) leitor_avanca(&l);
    ESPERA(leitor_pct(&l) > 50);
    TERMINA();
}

// ── abrir na posição guardada ───────────────────────────────────────
void t_leitor_reabre_onde_parou(void)
{
    COMECA("leitor · reabrir volta ao trecho em que a pessoa parou");

    leitor_t l;
    leitor_abre(&l, TEXTO, (int32_t)strlen(TEXTO), 0);
    leitor_avanca(&l);
    int32_t onde = leitor_posicao(&l);

    leitor_t de_novo;
    leitor_abre(&de_novo, TEXTO, (int32_t)strlen(TEXTO), onde);
    ESPERA_IGUAL(leitor_posicao(&de_novo), onde);

    char a[LEITOR_PAGINA], b[LEITOR_PAGINA];
    (void)leitor_pagina(&l, a, sizeof a);
    (void)leitor_pagina(&de_novo, b, sizeof b);
    ESPERA_TEXTO(b, a);
    TERMINA();
}

// ── a página não é maior que a TELA ────────────────────────────────
// Página maior que o desenho deixava um "…" e o avançar pulava o trecho
// escondido. 14 linhas de ~20 letras; acento custa dois bytes.
void t_leitor_a_pagina_cabe_na_tela(void)
{
    COMECA("leitor · a página nunca guarda mais do que a tela desenha");

    static char longo[8000];
    longo[0] = '\0';
    for (int i = 0; i < 20; i++)
        strncat(longo, TEXTO, sizeof longo - strlen(longo) - 1);

    const int TETO = 360;   // o maior tamanho, com folga de acento

    for (int t = LEITOR_PEQUENA; t <= LEITOR_GRANDE; t++) {
        leitor_t l;
        leitor_abre(&l, longo, (int32_t)strlen(longo), 0);
        leitor_recompoe(&l, (leitor_tamanho_t)t);

        char pagina[LEITOR_PAGINA];
        int32_t fim = leitor_pagina(&l, pagina, sizeof pagina);

        ESPERA(fim - leitor_posicao(&l) <= TETO);
        ESPERA(strlen(pagina) <= (size_t)TETO);
    }
    TERMINA();
}

// ── o total de páginas é o que o rodapé promete ─────────────────────
void t_leitor_conta_as_paginas_da_obra(void)
{
    COMECA("leitor · o rodapé conta as páginas, e a conta fecha");

    static char longo[4000];
    longo[0] = '\0';
    for (int i = 0; i < 10; i++)
        strncat(longo, TEXTO, sizeof longo - strlen(longo) - 1);

    leitor_t l;
    leitor_abre(&l, longo, (int32_t)strlen(longo), 0);

    // A conta aos poucos: quatro páginas por volta.
    int total = 0, numero = 0, voltas = 0;
    while (!leitor_conta_passo(&l, 4, &total, &numero) && voltas++ < 500) {}
    ESPERA(total > 1);
    ESPERA_IGUAL(numero, 1);

    // Andando até o fim, o número chega ao total e não passa.
    voltas = 0;
    while (!leitor_no_fim(&l) && voltas++ < 500) leitor_avanca(&l);
    int32_t onde = leitor_posicao(&l);
    leitor_recompoe(&l, l.letra);              // recomeça a conta daqui
    while (!leitor_conta_passo(&l, 4, &total, &numero)) {}
    ESPERA(numero > 1 && numero <= total);

    // Trocar a fonte muda o TOTAL, não a posição.
    int antes = total;
    leitor_recompoe(&l, LEITOR_GRANDE);
    while (!leitor_conta_passo(&l, 4, &total, &numero)) {}
    ESPERA(total > antes);
    ESPERA_IGUAL(leitor_posicao(&l), onde);
    TERMINA();
}
