#include "motor.h"

// A tela como ela fica depois do gesto. Todo plano termina aqui; senão o
// vidro fica num estado de passagem.
static quadro_t o_quadro_que_fica(void)
{
    return (quadro_t){ .sem_seletor  = false,
                       .sem_overlay  = false,
                       .limpa_o_chao = false,
                       .trocou_de_tela = false };
}

plano_t motor_plano(const motor_fatos_t *f)
{
    plano_t p = {0};

    if (f->trocou_de_tela) {
        // §5.5: o completo pinta só o que FICA. O seletor anda no gesto seguinte,
        // e tinta assentada pelo completo o parcial não apaga. Sem cursor, os dois
        // quadros seriam iguais: um só.
        p.quadro[p.n] = o_quadro_que_fica();
        p.quadro[p.n].trocou_de_tela = true;
        p.quadro[p.n].sem_seletor    = f->tem_cursor;
        p.n++;

        if (!f->tem_cursor) return p;

        // Troca de tela não soma o ritual da caixa: o completo já entregou chão
        // limpo.
    } else if (f->abriu_caixa && f->tem_caixa) {
        // §5.6: a caixa branca sobre texto assentado pelo completo saía
        // transparente no parcial; primeiro se limpa o chão.
        p.quadro[p.n] = o_quadro_que_fica();
        p.quadro[p.n].sem_overlay  = true;
        p.quadro[p.n].limpa_o_chao = true;
        p.n++;
    }

    p.quadro[p.n++] = o_quadro_que_fica();
    return p;
}
