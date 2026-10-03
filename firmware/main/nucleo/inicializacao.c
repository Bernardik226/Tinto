#include "inicializacao.h"

// A ordem do primeiro uso, num lugar só; o "voltar" sai dela.
static const inicio_fase_t ORDEM[] = {
    INICIO_BOAS_VINDAS,
    INICIO_NOME,
    INICIO_CONFIRMAR_DONO,
    INICIO_WIFI,
    INICIO_CONTA,
    INICIO_DATA_HORA,
    INICIO_CONCLUSAO,
};
#define N_ORDEM (int)(sizeof ORDEM / sizeof ORDEM[0])

inicio_fase_t inicio_fase_anterior(inicio_fase_t fase)
{
    for (int i = 1; i < N_ORDEM; i++)
        if (ORDEM[i] == fase) return ORDEM[i - 1];

    // Primeira etapa, ou fora do primeiro uso: fica. As telas de mídia são
    // diagnóstico e não têm "anterior".
    return fase;
}

bool inicio_pode_voltar(inicio_fase_t fase)
{
    return inicio_fase_anterior(fase) != fase;
}
