#include "teste.h"
#include "dado/memoria.h"
#include "dado/xadrez.h"

static const hal_t *cartao(void)
{
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    (void)memoria_prepara(hal);
    return hal;
}

static erro_t le_sem_pasta(const char *caminho, char *out, size_t max)
{
    (void)caminho;
    (void)out;
    (void)max;
    return ERR_SEM_PASTA;
}

void t_xadrez_pasta_ausente_e_partida_ausente(void)
{
    COMECA("xadrez · pasta ausente significa primeira partida");
    hal_t hal = *pc_liga();
    hal.ler = le_sem_pasta;
    xadrez_salvo_t salvo;
    ESPERA_IGUAL(xadrez_carrega(&hal, &salvo), ERR_ARQUIVO);
    TERMINA();
}

void t_xadrez_salva_e_retoma_lance(void)
{
    COMECA("xadrez · salvamento retoma posição e opções");
    const hal_t *hal = cartao();
    int emprestados_antes = pc_emprestados();
    xadrez_salvo_t s = { .modo = XZ_MODO_LOCAL, .orientacao = XZ_VERTICAL,
                         .cursor = XZ_E4, .origem = -1,
                         .placar_a2 = 3, .placar_b2 = 1 };
    xadrez_nova(&s.posicao);
    xadrez_mov_t m = { XZ_E2, XZ_E4, XZ_NENHUMA, 0 };
    ESPERA(xadrez_joga(&s.posicao, m));
    ESPERA_IGUAL(xadrez_salva_lance(hal, &s, m), OK);

    xadrez_salvo_t lido;
    ESPERA_IGUAL(xadrez_carrega(hal, &lido), OK);
    ESPERA_IGUAL(lido.cursor, XZ_E4);
    ESPERA_IGUAL(lido.posicao.turno, XZ_PRETAS);
    ESPERA_IGUAL(lido.placar_a2, 3);
    ESPERA_IGUAL(lido.placar_b2, 1);
    ESPERA_IGUAL(xadrez_tipo(xadrez_peca_em(&lido.posicao, XZ_E4)), XZ_PEAO);
    ESPERA_IGUAL(pc_emprestados(), emprestados_antes);
    TERMINA();
}

void t_xadrez_recusa_salvamento_corrompido(void)
{
    COMECA("xadrez · checksum inválido não vira partida");
    const hal_t *hal = cartao();
    int emprestados_antes = pc_emprestados();
    pc_poe_arquivo("/TINTO/jogos/xadrez.json",
        "{\"v\":1,\"modo\":0,\"cor\":0,\"dificuldade\":1,"
        "\"orientacao\":0,\"cursor\":12,\"origem\":-1,\"lances\":\"e2e4\","
        "\"checksum\":0}");
    xadrez_salvo_t s;
    ESPERA_IGUAL(xadrez_carrega(hal, &s), ERR_FORMATO);
    ESPERA_IGUAL(pc_emprestados(), emprestados_antes);
    TERMINA();
}

void t_xadrez_historico_vem_recente_e_opcoes_nao_duplicam_lance(void)
{
    COMECA("xadrez · histórico vem recente e opções preservam os lances");
    const hal_t *hal = cartao();
    xadrez_salvo_t s = {
        .modo = XZ_MODO_LOCAL, .cor_humana = XZ_BRANCAS,
        .cor_baixo = XZ_BRANCAS, .orientacao = XZ_VERTICAL,
        .cursor = XZ_E2, .origem = -1,
    };
    xadrez_nova(&s.posicao);
    xadrez_mov_t branco = {XZ_E2, XZ_E4, XZ_NENHUMA, 0};
    ESPERA(xadrez_joga(&s.posicao, branco));
    ESPERA_IGUAL(xadrez_salva_lance(hal, &s, branco), OK);
    xadrez_mov_t preto = {XZ_E5, XZ_CASA('e', 7), XZ_NENHUMA, 0};
    preto.de = XZ_CASA('e', 7); preto.para = XZ_E5;
    ESPERA(xadrez_joga(&s.posicao, preto));
    ESPERA_IGUAL(xadrez_salva_lance(hal, &s, preto), OK);

    xadrez_hist_item_t itens[4]; uint16_t total = 0;
    ESPERA_IGUAL(xadrez_historico(hal, 0, itens, 4, &total), OK);
    ESPERA_IGUAL(total, 2);
    ESPERA_IGUAL(itens[0].lance.de, XZ_CASA('e', 7));
    ESPERA_IGUAL(itens[1].lance.de, XZ_E2);
    ESPERA_IGUAL(itens[0].peca, XZ_PEAO);

    s.cor_baixo = XZ_PRETAS;
    s.orientacao = XZ_HORIZONTAL_ESQUERDA;
    ESPERA_IGUAL(xadrez_salva_opcoes(hal, &s), OK);
    xadrez_salvo_t lido;
    ESPERA_IGUAL(xadrez_carrega(hal, &lido), OK);
    ESPERA_IGUAL(lido.n_lances, 2);
    ESPERA_IGUAL(lido.cor_baixo, XZ_PRETAS);
    ESPERA_IGUAL(lido.orientacao, XZ_HORIZONTAL_ESQUERDA);
    TERMINA();
}
