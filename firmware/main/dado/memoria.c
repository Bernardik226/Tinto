#include "memoria.h"
#include "cartao.h"
#include "json.h"
#include <stdio.h>
#include <string.h>

#define FORMATO      CARTAO_RAIZ "/sistema/formato.json"
#define FORMATO_TMP  CARTAO_RAIZ "/sistema/formato.json.tmp"
#define PROVA        CARTAO_RAIZ "/sistema/prova"
#define PROVA_TMP    CARTAO_RAIZ "/sistema/prova.tmp"
#define PROVA_TEXTO  "tinto-prova-v1"

// A árvore inteira. /TINTO primeiro: o FatFs não cria filho de diretório
// inexistente.
static const char *const PASTAS[] = {
    CARTAO_RAIZ,
    CARTAO_RAIZ "/itens",
    CARTAO_RAIZ "/acervo",
    CARTAO_RAIZ "/entrada",
    CARTAO_RAIZ "/sistema",
};
#define N_PASTAS (int)(sizeof PASTAS / sizeof PASTAS[0])

static erro_t existe(const hal_t *hal, const char *caminho, caminho_tipo_t *out)
{
    return hal->tipo_caminho(caminho, out);
}

// Vazia = nada dentro, ou só pastas vazias da própria árvore. Qualquer
// arquivo alheio é conteúdo de alguém.
static bool pasta_da_arvore(const char *nome)
{
    return strcmp(nome, "itens")   == 0 || strcmp(nome, "acervo") == 0 ||
           strcmp(nome, "entrada") == 0 || strcmp(nome, "sistema") == 0;
}

static bool arvore_esta_vazia(const hal_t *hal)
{
    char nomes[8][40];
    int  quantos = 0;
    if (hal->listar(CARTAO_RAIZ, 0, nomes, 8, &quantos) != OK) return false;
    if (quantos > 4) return false;

    for (int i = 0; i < quantos; i++) {
        if (!pasta_da_arvore(nomes[i])) return false;

        char caminho[64];
        snprintf(caminho, sizeof caminho, CARTAO_RAIZ "/%s", nomes[i]);
        char dentro[2][40];
        int  n = 0;
        if (hal->listar(caminho, 0, dentro, 2, &n) != OK) return false;
        if (n > 0) return false;
    }
    return true;
}

erro_t memoria_inspeciona(const hal_t *hal, arvore_estado_t *out)
{
    if (!hal || !out) return ERR_INTERNO;

    caminho_tipo_t tipo;
    erro_t err = existe(hal, CARTAO_RAIZ, &tipo);
    if (err != OK) return err;

    // RN-6B: mídia saudável sem /TINTO/ é cartão novo.
    if (tipo == CAMINHO_AUSENTE) { *out = ARVORE_VIRGEM; return OK; }

    // /TINTO como ARQUIVO é de outra pessoa: provisionar por cima o apagaria.
    if (tipo != CAMINHO_DIRETORIO) { *out = ARVORE_DANIFICADA; return OK; }

    err = existe(hal, FORMATO, &tipo);
    if (err != OK) return err;
    if (tipo != CAMINHO_ARQUIVO) {
        // /TINTO/ sem formato.json é estrutura quebrada — mas VAZIA é só um
        // provisionamento interrompido, e não pede formatar.
        *out = arvore_esta_vazia(hal) ? ARVORE_VIRGEM : ARVORE_DANIFICADA;
        return OK;
    }

    char json[64];
    err = hal->ler(FORMATO, json, sizeof json);
    if (err != OK) return err;

    int v = 0;
    if (!json_int(json, "v", &v)) { *out = ARVORE_DANIFICADA; return OK; }

    // RN-63: mais nova que este firmware é intocável; outra divergência é
    // estrutura que não sabemos ler.
    if (v > CARTAO_FORMATO) { *out = ARVORE_FORMATO_FUTURO; return OK; }
    if (v != CARTAO_FORMATO) { *out = ARVORE_DANIFICADA; return OK; }

    for (int i = 0; i < N_PASTAS; i++) {
        err = existe(hal, PASTAS[i], &tipo);
        if (err != OK) return err;
        if (tipo != CAMINHO_DIRETORIO) { *out = ARVORE_PARCIAL; return OK; }
    }

    *out = ARVORE_PRONTA;
    return OK;
}

erro_t memoria_prepara(const hal_t *hal)
{
    if (!hal) return ERR_INTERNO;

    arvore_estado_t estado;
    erro_t err = memoria_inspeciona(hal, &estado);
    if (err != OK) return err;

    // Já pronta: não gasta uma escrita para reafirmar.
    if (estado == ARVORE_PRONTA) return OK;

    // Estas viram tela de recuperação; só o gesto destrutivo explícito passa.
    if (estado == ARVORE_DANIFICADA || estado == ARVORE_FORMATO_FUTURO)
        return ERR_FORMATO;

    for (int i = 0; i < N_PASTAS; i++) {
        err = hal->criar_diretorio(PASTAS[i]);
        if (err != OK) return err;
    }

    // RN-64: .tmp + rename; um corte antes deixa o formato anterior valendo.
    char json[32];
    snprintf(json, sizeof json, "{\"v\":%d}", CARTAO_FORMATO);
    err = hal->escrever(FORMATO_TMP, json);
    if (err != OK) return err;

    err = hal->renomear(FORMATO_TMP, FORMATO);
    if (err != OK) return err;

    // Reler separa "o driver aceitou" de "o cartão guardou".
    char lido[64];
    err = hal->ler(FORMATO, lido, sizeof lido);
    if (err != OK) return err;

    int v = 0;
    if (!json_int(lido, "v", &v) || v != CARTAO_FORMATO) return ERR_FORMATO;
    return OK;
}

erro_t memoria_prova_escrita(const hal_t *hal)
{
    if (!hal) return ERR_INTERNO;

    // Resíduo de um corte anterior: não é erro encontrar, é erro deixar.
    caminho_tipo_t tipo;
    erro_t err = existe(hal, PROVA_TMP, &tipo);
    if (err != OK) return err;
    if (tipo != CAMINHO_AUSENTE) {
        err = hal->apagar(PROVA_TMP);
        if (err != OK) return err;
    }

    err = hal->escrever(PROVA_TMP, PROVA_TEXTO);
    if (err != OK) return err;

    err = hal->renomear(PROVA_TMP, PROVA);
    if (err != OK) return err;

    char lido[32];
    err = hal->ler(PROVA, lido, sizeof lido);
    if (err != OK) return err;
    if (strcmp(lido, PROVA_TEXTO) != 0) return ERR_ARQUIVO;

    return hal->apagar(PROVA);
}
