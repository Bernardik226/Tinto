#include "resultado.h"
#include "campos.h"

#include <stdio.h>
#include <string.h>

void vista_resultado(const estado_t *e, vista_resultado_t *out)
{
    memset(out, 0, sizeof *out);

    snprintf(out->titulo, sizeof out->titulo, "%s", "Resultado");
    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    out->bateria = e->bateria;
    out->wifi    = vista_wifi_da_barra(e);
    out->sinc    = vista_sinc_da_barra(e);

    out->tudo_certo = true;

    for (int i = 0; i < e->n_resultados && out->n < RESULTADOS_MAX; i++) {
        const item_t *it = &e->resultados[i].item;

        snprintf(out->linhas[out->n].oque, sizeof out->linhas[0].oque,
                 "%s", it->titulo);
        vista_destino(it, out->linhas[out->n].destino,
                      sizeof out->linhas[0].destino);

        // O estado CONCRETO de cada linha ("criado", "guardada" ou a falha):
        // sucesso parcial como total faz a pessoa parar de conferir.
        bool ok = it->id[0] != '\0';
        out->linhas[out->n].ok = ok;

        // O verbo da AÇÃO vence o do tipo: "apagada", "alterado".
        const res_verbo_t v = e->resultados[i].verbo;
        bool fem = it->tipo == TIPO_TAREFA || it->tipo == TIPO_LISTA
                || it->tipo == TIPO_ANOTACAO;

        snprintf(out->linhas[out->n].estado, sizeof out->linhas[0].estado, "%s",
                 !ok                          ? "falhou"
               : v == RES_APAGOU              ? (fem ? "apagada" : "apagado")
               : v == RES_EDITOU              ? (fem ? "alterada" : "alterado")
               : it->tipo == TIPO_ANOTACAO    ? "guardada"
               : it->tipo == TIPO_TAREFA      ? "criada"
               : it->tipo == TIPO_LISTA       ? "criada"
                                              : "criado");
        if (!ok) out->tudo_certo = false;
        out->n++;
    }

    // A frase conta o que ACONTECEU, não o que foi pedido.
    if (out->n == 1)
        snprintf(out->frase, sizeof out->frase, "%s",
                 out->tudo_certo ? "Pronto" : "Não deu certo");
    else
        snprintf(out->frase, sizeof out->frase, "%d ações %s", out->n,
                 out->tudo_certo ? "concluídas" : "com um problema");

    // Aponta para a LISTA, que é a prova.
    if (!out->tudo_certo)
        snprintf(out->sub, sizeof out->sub, "%s",
                 "Uma delas não subiu. O resto está feito.");
    else if (out->n == 1)
        snprintf(out->sub, sizeof out->sub, "%s",
                 "A gravação criou o item abaixo.");
    else if (out->n == 2)
        snprintf(out->sub, sizeof out->sub, "%s",
                 "A gravação criou os dois itens abaixo.");
    else
        snprintf(out->sub, sizeof out->sub, "A gravação criou os %d itens "
                 "abaixo.", out->n);

    out->cursor = e->cursor < 0 ? 0 : e->cursor;

    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK voltar");
}
