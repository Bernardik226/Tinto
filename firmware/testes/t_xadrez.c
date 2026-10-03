#include "teste.h"
#include "jogos/xadrez.h"

static bool tem(const xadrez_mov_t *m, int n, int de, int para)
{
    for (int i = 0; i < n; i++)
        if (m[i].de == de && m[i].para == para) return true;
    return false;
}

void t_xadrez_posicao_inicial(void)
{
    COMECA("xadrez · posição inicial tem vinte lances legais");
    xadrez_pos_t p;
    xadrez_nova(&p);
    xadrez_mov_t m[XADREZ_MOV_MAX];
    ESPERA_IGUAL(xadrez_movimentos(&p, -1, m, XADREZ_MOV_MAX), 20);
    TERMINA();
}

void t_xadrez_peao_anda_sem_saltar(void)
{
    COMECA("xadrez · peão anda uma ou duas casas, nunca três");
    xadrez_pos_t p;
    xadrez_nova(&p);
    xadrez_mov_t m[XADREZ_MOV_MAX];
    int n = xadrez_movimentos(&p, XZ_E2, m, XADREZ_MOV_MAX);
    ESPERA_IGUAL(n, 2);
    ESPERA(tem(m, n, XZ_E2, XZ_E3));
    ESPERA(tem(m, n, XZ_E2, XZ_E4));
    ESPERA(!tem(m, n, XZ_E2, XZ_E5));
    TERMINA();
}

void t_xadrez_lance_troca_o_turno(void)
{
    COMECA("xadrez · lance aceito move a peça e troca o turno");
    xadrez_pos_t p;
    xadrez_nova(&p);
    ESPERA(xadrez_joga(&p, (xadrez_mov_t){ XZ_E2, XZ_E4, XZ_NENHUMA, 0 }));
    ESPERA_IGUAL(xadrez_peca_em(&p, XZ_E2), XZ_NENHUMA);
    ESPERA_IGUAL(xadrez_tipo(xadrez_peca_em(&p, XZ_E4)), XZ_PEAO);
    ESPERA_IGUAL(p.turno, XZ_PRETAS);
    TERMINA();
}

static bool joga(xadrez_pos_t *p, int de, int para, xadrez_peca_t promocao)
{
    return xadrez_joga(p, (xadrez_mov_t){
        (uint8_t)de, (uint8_t)para, (uint8_t)promocao, 0
    });
}

void t_xadrez_en_passant(void)
{
    COMECA("xadrez · en passant remove o peão ultrapassado");
    xadrez_pos_t p;
    xadrez_nova(&p);
    ESPERA(joga(&p, XZ_E2, XZ_E4, XZ_NENHUMA));
    ESPERA(joga(&p, XZ_CASA('a',7), XZ_CASA('a',6), XZ_NENHUMA));
    ESPERA(joga(&p, XZ_E4, XZ_E5, XZ_NENHUMA));
    ESPERA(joga(&p, XZ_CASA('d',7), XZ_CASA('d',5), XZ_NENHUMA));
    ESPERA(joga(&p, XZ_E5, XZ_CASA('d',6), XZ_NENHUMA));
    ESPERA_IGUAL(xadrez_peca_em(&p, XZ_CASA('d',5)), XZ_NENHUMA);
    TERMINA();
}

void t_xadrez_roque_move_rei_e_torre(void)
{
    COMECA("xadrez · roque move rei e torre no mesmo lance");
    xadrez_pos_t p;
    xadrez_nova(&p);
    ESPERA(joga(&p, XZ_E2, XZ_E4, XZ_NENHUMA));
    ESPERA(joga(&p, XZ_CASA('a',7), XZ_CASA('a',6), XZ_NENHUMA));
    ESPERA(joga(&p, XZ_CASA('g',1), XZ_CASA('f',3), XZ_NENHUMA));
    ESPERA(joga(&p, XZ_CASA('a',6), XZ_CASA('a',5), XZ_NENHUMA));
    ESPERA(joga(&p, XZ_CASA('f',1), XZ_E2, XZ_NENHUMA));
    ESPERA(joga(&p, XZ_CASA('a',5), XZ_CASA('a',4), XZ_NENHUMA));
    ESPERA(joga(&p, XZ_CASA('e',1), XZ_CASA('g',1), XZ_NENHUMA));
    ESPERA_IGUAL(xadrez_tipo(xadrez_peca_em(&p, XZ_CASA('g',1))), XZ_REI);
    ESPERA_IGUAL(xadrez_tipo(xadrez_peca_em(&p, XZ_CASA('f',1))), XZ_TORRE);
    TERMINA();
}

void t_xadrez_promocao_oferece_quatro_pecas(void)
{
    COMECA("xadrez · promoção oferece dama, torre, bispo e cavalo");
    xadrez_pos_t p = {0};
    p.en_passant = -1;
    p.casa[XZ_CASA('a',1)] = XZ_REI;
    p.casa[XZ_CASA('h',8)] = (uint8_t)(XZ_REI | 8);
    p.casa[XZ_CASA('e',7)] = XZ_PEAO;
    xadrez_mov_t m[XADREZ_MOV_MAX];
    int n = xadrez_movimentos(&p, XZ_CASA('e',7), m, XADREZ_MOV_MAX);
    ESPERA_IGUAL(n, 4);
    ESPERA(joga(&p, XZ_CASA('e',7), XZ_CASA('e',8), XZ_CAVALO));
    ESPERA_IGUAL(xadrez_tipo(xadrez_peca_em(&p, XZ_CASA('e',8))), XZ_CAVALO);
    TERMINA();
}

void t_xadrez_reconhece_mate_e_empates(void)
{
    COMECA("xadrez · reconhece mate, afogamento e material insuficiente");
    xadrez_pos_t p;
    xadrez_nova(&p);
    ESPERA(joga(&p, XZ_E2, XZ_E4, XZ_NENHUMA));
    ESPERA(joga(&p, XZ_CASA('e',7), XZ_E5, XZ_NENHUMA));
    ESPERA(joga(&p, XZ_CASA('f',1), XZ_CASA('c',4), XZ_NENHUMA));
    ESPERA(joga(&p, XZ_CASA('b',8), XZ_CASA('c',6), XZ_NENHUMA));
    ESPERA(joga(&p, XZ_CASA('d',1), XZ_CASA('h',5), XZ_NENHUMA));
    ESPERA(joga(&p, XZ_CASA('g',8), XZ_CASA('f',6), XZ_NENHUMA));
    ESPERA(joga(&p, XZ_CASA('h',5), XZ_CASA('f',7), XZ_NENHUMA));
    ESPERA_IGUAL(xadrez_estado(&p), XZ_MATE_BRANCAS);

    memset(&p, 0, sizeof p);
    p.en_passant = -1;
    p.turno = XZ_PRETAS;
    p.casa[XZ_CASA('a',8)] = (uint8_t)(XZ_REI | 8);
    p.casa[XZ_CASA('c',6)] = XZ_REI;
    p.casa[XZ_CASA('c',7)] = XZ_DAMA;
    ESPERA_IGUAL(xadrez_estado(&p), XZ_EMPATE_AFOGAMENTO);

    memset(&p, 0, sizeof p);
    p.en_passant = -1;
    p.casa[XZ_CASA('a',1)] = XZ_REI;
    p.casa[XZ_CASA('h',8)] = (uint8_t)(XZ_REI | 8);
    ESPERA_IGUAL(xadrez_estado(&p), XZ_EMPATE_MATERIAL);
    TERMINA();
}

void t_xadrez_navega_so_por_candidatos_validos(void)
{
    COMECA("xadrez · direcional percorre peças e depois destinos legais");
    xadrez_pos_t p;
    xadrez_nova(&p);
    ESPERA_IGUAL(xadrez_navega_peca(&p, XZ_E2, 1, 0), XZ_CASA('f',2));
    ESPERA_IGUAL(xadrez_navega_peca(&p, XZ_E2, 0, 1), XZ_E2);
    ESPERA_IGUAL(xadrez_navega_destino(&p, XZ_E2, XZ_E3, 0, 1), XZ_E4);
    ESPERA_IGUAL(xadrez_navega_destino(&p, XZ_E2, XZ_E4, 0, 1), XZ_E4);
    TERMINA();
}
