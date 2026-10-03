#include "indice.h"
#include "cartao.h"
#include <stdio.h>
#include <string.h>

// O índice é do armazenamento: quem grava no cartão o mantém, e nenhum caso
// de uso precisa lembrar. O buffer vem da PSRAM pelo hal.
static item_t *itens;
static int     n;
static int     fora;
static bool    pronto;

bool indice_pronto(void) { return pronto; }
int  indice_n(void)      { return pronto ? n : 0; }

const item_t *indice_em(int i)
{
    if (!pronto || i < 0 || i >= n) return NULL;
    return &itens[i];
}

// Onde este id está, ou -1.
static int posicao(const char *id)
{
    if (!pronto || !id || !id[0]) return -1;
    for (int i = 0; i < n; i++)
        if (strcmp(itens[i].id, id) == 0) return i;
    return -1;
}

void indice_poe(const item_t *it, data_t dia)
{
    if (!pronto || !it || !it->id[0]) return;

    int onde = posicao(it->id);
    if (onde < 0) {
        if (n >= INDICE_MAX) { fora++; return; }
        onde = n++;
    }

    itens[onde] = *it;
    // O `dia` guardado é a PASTA, que nem sempre é o vencimento (RN-26).
    itens[onde].dia = dia;
}

void indice_tira(const char *id)
{
    int onde = posicao(id);
    if (onde < 0) return;

    // Troca com o último: a ordem do array não significa nada.
    itens[onde] = itens[--n];
}

int indice_do_dia(data_t dia, item_t *out, int max)
{
    if (!pronto || !out || max <= 0) return 0;

    int achados = 0;
    for (int i = 0; i < n && achados < max; i++)
        if (data_igual(itens[i].dia, dia)) out[achados++] = itens[i];
    return achados;
}

const item_t *indice_acha(const char *id, data_t *dia)
{
    int onde = posicao(id);
    if (onde < 0) return NULL;
    if (dia) *dia = itens[onde].dia;
    return &itens[onde];
}

// ── a montagem ───────────────────────────────────────────────────────
// A única varredura do sistema, uma vez, no boot.
erro_t indice_monta(const hal_t *hal)
{
    if (!hal || !hal->emprestar) return ERR_INTERNO;

    indice_solta(hal);

    size_t real = 0;
    size_t quer = (size_t)INDICE_MAX * sizeof(item_t);

    // O mínimo é um quarto: um índice menor, que conta o que não coube, é
    // melhor que nenhum.
    itens = hal->emprestar(quer, quer / 4, &real);
    if (!itens) return ERR_INTERNO;

    int cabe = (int)(real / sizeof(item_t));
    if (cabe <= 0) { hal->devolver(itens); itens = NULL; return ERR_INTERNO; }

    n = fora = 0;
    pronto = true;

    data_t dias[CARTAO_DIAS_MAX];
    int nd = 0;
    erro_t err = cartao_lista_dias(hal, dias, CARTAO_DIAS_MAX, &nd);
    if (err != OK) {
        // Cartão com defeito NÃO é agenda vazia.
        pronto = false;
        hal->devolver(itens);
        itens = NULL;
        return err;
    }

    for (int d = 0; d < nd; d++) {
        for (int desde = 0; ; desde += CARTAO_BLOCO) {
            item_t bloco[CARTAO_BLOCO];
            int quantos = 0;
            if (cartao_lista_itens(hal, dias[d], desde, bloco,
                                   CARTAO_BLOCO, &quantos) != OK ||
                quantos == 0)
                break;

            for (int i = 0; i < quantos; i++) {
                if (n >= cabe) { fora++; continue; }
                itens[n] = bloco[i];
                itens[n].dia = dias[d];
                n++;
            }

            if (quantos < CARTAO_BLOCO) break;
        }
    }

    if (hal->registrar) {
        char msg[64];
        snprintf(msg, sizeof msg, "indice: %d itens de %d dias, %d fora",
                 n, nd, fora);
        hal->registrar("indice", msg);
    }
    return OK;
}

void indice_solta(const hal_t *hal)
{
    if (itens && hal && hal->devolver) hal->devolver(itens);
    itens = NULL;
    n = fora = 0;
    pronto = false;
}
