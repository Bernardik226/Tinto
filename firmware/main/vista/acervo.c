#include "acervo.h"
#include "campos.h"
#include "../dado/acervo.h"
#include <stdio.h>
#include <string.h>

// Porcentagem sobre o TAMANHO do texto, não sobre páginas: a página muda
// com a fonte, e a porcentagem não pode andar sozinha.
static int pct_lido(const obra_t *o)
{
    if (o->tamanho <= 0 || o->offset_texto <= 0) return 0;
    long p = (long)o->offset_texto * 100 / o->tamanho;
    return p > 100 ? 100 : (int)p;
}

static int pct_baixado(const obra_t *o)
{
    if (o->tamanho <= 0 || o->baixado <= 0) return 0;
    long p = (long)o->baixado * 100 / o->tamanho;
    return p > 100 ? 100 : (int)p;
}

static const char *nome_do_tipo(const obra_t *o)
{
    return o->tipo == OBRA_DOCUMENTO ? "Documento" : "Livro";
}

// "Livro", "Livro · 12%", "Livro · Concluído". Porcentagem só no começado.
static void legenda_de(const obra_t *o, char *out, size_t max)
{
    if (o->estado == OBRA_BAIXANDO) {
        if (o->baixado <= 0)
            snprintf(out, max, "%s · Preparando...", nome_do_tipo(o));
        else
            snprintf(out, max, "%s · %d%%", nome_do_tipo(o), pct_baixado(o));
        return;
    }
    if (o->concluida) {
        snprintf(out, max, "%s · Concluído", nome_do_tipo(o));
        return;
    }

    int lido = pct_lido(o);
    if (lido > 0) snprintf(out, max, "%s · %d%%", nome_do_tipo(o), lido);
    else          snprintf(out, max, "%s", nome_do_tipo(o));
}

static bool passa_no_filtro(const obra_t *o, acervo_filtro_t f)
{
    switch (f) {
    case ACERVO_EM_LEITURA:
        // Começada e não terminada: a lista de "onde eu estava".
        return o->offset_texto > 0 && !o->concluida;
    case ACERVO_SO_LIVROS:      return o->tipo == OBRA_LIVRO;
    case ACERVO_SO_DOCUMENTOS:  return o->tipo == OBRA_DOCUMENTO;
    case ACERVO_CONCLUIDOS:     return o->concluida;
    case ACERVO_TODOS:
    default:                    return true;
    }
}

void vista_acervo(const estado_t *e, vista_acervo_t *out)
{
    memset(out, 0, sizeof *out);

    snprintf(out->titulo, sizeof out->titulo, "%s", "Acervo");
    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    out->bateria = e->bateria;
    out->wifi = vista_wifi_da_barra(e);
    out->sinc = vista_sinc_da_barra(e);
    out->cursor = e->cursor;

    // A listagem já vem da mais recentemente aberta (`dado/acervo.c`).
    const obra_t *obras = e->acervo;
    int total = e->n_acervo;

    // ── CONTINUAR LENDO ──────────────────────────────────────────────────
    // A primeira com leitura começada, antes do filtro: filtrar não pode
    // esconder onde a pessoa parou.
    for (int i = 0; i < total; i++) {
        if (obras[i].estado != OBRA_AQUI) continue;
        if (obras[i].offset_texto <= 0 || obras[i].concluida) continue;

        out->tem_destaque = true;
        out->destaque.indice = (int16_t)i;
        snprintf(out->destaque.titulo, sizeof out->destaque.titulo, "%s",
                 obras[i].titulo);
        snprintf(out->destaque.legenda, sizeof out->destaque.legenda,
                 "%d%%", pct_lido(&obras[i]));
        break;
    }

    // O cartaz é parada real, mas `cursor` continua índice da lista para a
    // paginação não misturar cartaz e obra.
    if (out->tem_destaque) {
        out->destaque_focado = e->cursor == 0;
        out->cursor = e->cursor - 1;
    }

    for (int i = 0; i < total; i++) {
        if (!passa_no_filtro(&obras[i], e->acervo_filtro)) continue;

        if (out->n >= ACERVO_LINHAS) { out->mais++; continue; }

        linha_obra_t *l = &out->linhas[out->n++];
        l->indice = (int16_t)i;
        snprintf(l->titulo, sizeof l->titulo, "%s", obras[i].titulo);
        legenda_de(&obras[i], l->legenda, sizeof l->legenda);

        switch (obras[i].estado) {
        case OBRA_AQUI:     l->marca = obras[i].no_catalogo
                                      ? ACERVO_LOCAL : ACERVO_SO_MEMORIA;
                            l->pct = -1; break;
        case OBRA_BAIXANDO: l->marca = ACERVO_VINDO;
                            l->pct = pct_baixado(&obras[i]); break;
        default:            l->marca = ACERVO_BAIXAR; l->pct = -1; break;
        }
    }

    // Vazio é uma RESPOSTA, com o que fazer: quem manda livro é o app (um
    // arquivo de 3 MB precisa de teclado e disco).
    if (out->n == 0 && !out->tem_destaque) {
        snprintf(out->vazio, sizeof out->vazio, "%s",
                 "Seu Acervo está vazio");
        snprintf(out->vazio_como, sizeof out->vazio_como, "%s",
                 "Mande um livro pelo aplicativo");
    }

    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK voltar");
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK abrir");
}
