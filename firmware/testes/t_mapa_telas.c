// firmware/testes/t_mapa_telas.c — o catálogo de telas.
// Toda tela foi considerada no mapa, e a política de seletor é decisão.
#include "teste.h"
#include "tela/mapa.h"

// ── a sentinela alcança a última tela ───────────────────────────────
// Derivada de uma tela nomeada, ela deixou o QR fora de toda varredura.
void t_mapa_a_sentinela_alcanca_a_ultima_tela(void)
{
    COMECA("a sentinela do enum conta todas as telas, inclusive a última");

    // Tela nascida depois do limite ficaria fora dos laços.
    ESPERA((int)TELA_VINCULAR < (int)TELA_QUANTAS);
    TERMINA();
}

// ── nenhuma tela entra sem linha no mapa ────────────────────────────
void t_mapa_toda_tela_esta_declarada(void)
{
    COMECA("nenhuma tela entra no sistema sem linha no mapa de propriedades");

    for (int t = 0; t < TELA_QUANTAS; t++)
        ESPERA(tela_declarada((tela_id)t));
    TERMINA();
}

// ── seletor é DECISÃO, não default ──────────────────────────────────
// Este teste nomeia as que NÃO têm seletor.
void t_mapa_tela_de_leitura_nao_pinta_seletor(void)
{
    COMECA("tela sem lista para andar não paga o quadro do seletor");

    // As telas de leitura pura.
    ESPERA(!tela_tem_seletor(TELA_BLOQUEADA));
    ESPERA(!tela_tem_seletor(TELA_SOBRE));
    ESPERA(!tela_tem_seletor(TELA_FALA));

    // O Leitor: ◀▶ viram página, sem linha selecionada.
    ESPERA(!tela_tem_seletor(TELA_LEITOR));

    // As que têm cursor continuam pagando.
    ESPERA(tela_tem_seletor(TELA_AGENDA));
    ESPERA(tela_tem_seletor(TELA_AJUSTES));
    ESPERA(tela_tem_seletor(TELA_CALENDARIO));

    // As áreas novas pagam o seletor.
    ESPERA(tela_tem_seletor(TELA_HOME));
    ESPERA(tela_tem_seletor(TELA_ACERVO));
    ESPERA(tela_tem_seletor(TELA_LEITURA_AJUSTES));
    ESPERA(tela_tem_seletor(TELA_JOGOS));
    ESPERA(tela_tem_seletor(TELA_XADREZ));
    TERMINA();
}
