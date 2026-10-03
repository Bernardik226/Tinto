#include "teste.h"
#include "jogos/xadrez_maquina.h"
#include <string.h>

static xadrez_maquina_estado_t termina(xadrez_maquina_t *m, xadrez_mov_t *lance)
{
    xadrez_maquina_estado_t estado = XZM_BUSCANDO;
    for (int i = 0; i < 2000 && estado == XZM_BUSCANDO; i++)
        estado = xadrez_maquina_passo(m, 8, lance);
    return estado;
}

void t_xadrez_maquina_e_legal_deterministica_e_fatiada(void)
{
    COMECA("xadrez · máquina é legal, determinística e respeita a fatia");
    xadrez_pos_t p;
    xadrez_nova(&p);
    xadrez_maquina_t a, b;
    xadrez_mov_t ma = {0}, mb = {0};
    xadrez_maquina_inicia(&a, &p, XZM_DIFICIL);
    xadrez_maquina_inicia(&b, &p, XZM_DIFICIL);

    uint32_t antes = a.nos;
    xadrez_maquina_estado_t estado = xadrez_maquina_passo(&a, 3, &ma);
    ESPERA(a.nos - antes <= 3);
    while (estado == XZM_BUSCANDO)
        estado = xadrez_maquina_passo(&a, 8, &ma);
    ESPERA_IGUAL(estado, XZM_PRONTO);
    ESPERA_IGUAL(termina(&b, &mb), XZM_PRONTO);
    ESPERA_IGUAL(memcmp(&ma, &mb, sizeof ma), 0);

    xadrez_pos_t depois = p;
    ESPERA(xadrez_joga(&depois, ma));
    TERMINA();
}

void t_xadrez_maquina_media_aproveita_captura_livre(void)
{
    COMECA("xadrez · máquina média aproveita uma dama livre");
    xadrez_pos_t p;
    memset(&p, 0, sizeof p);
    p.casa[XZ_CASA('a', 1)] = XZ_REI;
    p.casa[XZ_CASA('h', 8)] = XZ_REI | 8;
    p.casa[XZ_CASA('d', 8)] = XZ_TORRE | 8;
    p.casa[XZ_CASA('d', 1)] = XZ_DAMA;
    p.turno = XZ_PRETAS;
    p.en_passant = -1;

    xadrez_maquina_t maquina;
    xadrez_mov_t lance = {0};
    xadrez_maquina_inicia(&maquina, &p, XZM_MEDIA);
    ESPERA_IGUAL(termina(&maquina, &lance), XZM_PRONTO);
    ESPERA_IGUAL(lance.de, XZ_CASA('d', 8));
    ESPERA_IGUAL(lance.para, XZ_CASA('d', 1));
    TERMINA();
}

void t_xadrez_maquina_facil_nao_desfaz_o_ultimo_lance(void)
{
    COMECA("xadrez · máquina fácil não oscila a mesma torre");
    xadrez_pos_t p;
    memset(&p, 0, sizeof p);
    p.casa[XZ_CASA('h', 1)] = XZ_REI;
    p.casa[XZ_CASA('h', 8)] = XZ_REI | 8;
    p.casa[XZ_CASA('a', 8)] = XZ_TORRE | 8;
    p.turno = XZ_PRETAS;
    p.en_passant = -1;
    p.ultimo = (xadrez_mov_t){XZ_CASA('b', 8), XZ_CASA('a', 8), 0, 0};

    xadrez_maquina_t maquina;
    xadrez_mov_t lance = {0};
    xadrez_maquina_inicia(&maquina, &p, XZM_FACIL);
    ESPERA_IGUAL(termina(&maquina, &lance), XZM_PRONTO);
    ESPERA(!(lance.de == XZ_CASA('a', 8) &&
             lance.para == XZ_CASA('b', 8)));
    ESPERA(maquina.nos > 1);
    TERMINA();
}
