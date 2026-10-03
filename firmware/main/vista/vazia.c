#include "vazia.h"
#include "campos.h"
#include <stdio.h>
#include <string.h>

static const char *nome_da_tela(tela_id t)
{
    switch (t) {
    case TELA_NOTA:     return "NOTA";
    case TELA_DIA:      return "O DIA";
    case TELA_CALENDARIO: return "CALENDÁRIO";
    case TELA_AJUSTES:  return "AJUSTES";
    default:            return "TINTO";
    }
}

void vista_vazia(const estado_t *e, vista_vazia_t *out)
{
    memset(out, 0, sizeof *out);

    snprintf(out->titulo, sizeof out->titulo, "%s",
             nome_da_tela(e->pilha[e->profundidade]));
    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    out->bateria = e->bateria;
    out->wifi = vista_wifi_da_barra(e);
    out->sinc = vista_sinc_da_barra(e);

    snprintf(out->aviso, sizeof out->aviso, "%s", "em construcao");

    // A esquerda é sempre a saída: sem ela, quem entrou fundo não sai.
    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK voltar");
}
