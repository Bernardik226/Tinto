#include "refresco.h"

refresco_t refresco_escolhe_medindo(bool trocou_de_tela, bool tem_anterior,
                                    size_t bytes_mudados, size_t bytes_total)
{
    if (!tem_anterior || trocou_de_tela) return REFRESCO_COMPLETO;

    // Muito mudou: o parcial custaria quase o mesmo e sairia pior (lista que
    // ganha linhas, mês trocando).
    if (bytes_total > 0 &&
        bytes_mudados * REFRESCO_MUITO_DEN > bytes_total * REFRESCO_MUITO_NUM)
        return REFRESCO_COMPLETO;

    return REFRESCO_PARCIAL;
}

refresco_t refresco_por_intencao(pintura_t intencao, bool tem_anterior,
                                 size_t bytes_mudados, size_t bytes_total)
{
    // Sem quadro anterior nada é parcial, nem o foco: o vidro pode estar com
    // qualquer imagem.
    if (!tem_anterior) return REFRESCO_COMPLETO;

    if (intencao == PINTURA_TELA_NOVA)
        return REFRESCO_COMPLETO;

    // O foco não passa pela medida. Exceção estreita: só o gesto de navegar.
    if (intencao == PINTURA_FOCO) return REFRESCO_PARCIAL;

    return refresco_escolhe_medindo(false, true, bytes_mudados, bytes_total);
}
