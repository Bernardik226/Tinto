#include "perfil.h"
#include "cartao.h"
#include "json.h"
#include <stdio.h>
#include <string.h>

#define PERFIL      CARTAO_RAIZ "/sistema/perfil.json"
#define PERFIL_TMP  CARTAO_RAIZ "/sistema/perfil.json.tmp"

#define JSON_MAX 512

// ── escrita atômica ─────────────────────────────────────────────────
static erro_t grava_atomico(const hal_t *hal, const char *tmp,
                            const char *final, const char *json)
{
    erro_t err = hal->escrever(tmp, json);
    if (err != OK) return err;

    err = hal->renomear(tmp, final);
    if (err != OK) return err;

    // Reler é o que separa "o driver aceitou" de "o cartão guardou".
    char lido[JSON_MAX];
    err = hal->ler(final, lido, sizeof lido);
    if (err != OK) return err;
    return strcmp(lido, json) == 0 ? OK : ERR_ARQUIVO;
}

// ── o nome ───────────────────────────────────────────────────────────
int perfil_nome_trunca(const char *entrada, char *out, size_t max)
{
    if (!out || max == 0) return 0;
    out[0] = '\0';
    if (!entrada) return 0;

    size_t bytes = 0;
    int    chars = 0;
    while (entrada[bytes] && chars < NOME_CARACTERES_MAX) {
        // Continuação em UTF-8 é 10xxxxxx: contar só o resto conta CARACTERES, e
        // parar ali nunca parte uma sequência.
        size_t n = 1;
        unsigned char c = (unsigned char)entrada[bytes];
        if      ((c & 0x80u) == 0x00u) n = 1;
        else if ((c & 0xE0u) == 0xC0u) n = 2;
        else if ((c & 0xF0u) == 0xE0u) n = 3;
        else if ((c & 0xF8u) == 0xF0u) n = 4;
        else break;                       // byte solto: para, não adivinha

        for (size_t i = 1; i < n; i++)
            if ((entrada[bytes + i] & 0xC0) != 0x80) return chars;

        if (bytes + n >= max) break;
        memcpy(out + bytes, entrada + bytes, n);
        bytes += n;
        chars++;
    }
    out[bytes] = '\0';
    return chars;
}

void perfil_novo_id(const hal_t *hal, char out[33])
{
    uint8_t bruto[16];
    hal->aleatorio(bruto, sizeof bruto);
    for (int i = 0; i < 16; i++)
        snprintf(out + i * 2, 3, "%02x", bruto[i]);
    out[32] = '\0';
}

// ── perfil.json ──────────────────────────────────────────────────────
erro_t perfil_carrega(const hal_t *hal, perfil_local_t *out)
{
    if (!hal || !out) return ERR_INTERNO;
    memset(out, 0, sizeof *out);

    char json[JSON_MAX];
    erro_t err = hal->ler(PERFIL, json, sizeof json);
    if (err != OK) return err;

    int v = 0;
    if (!json_int(json, "v", &v) || v != CARTAO_FORMATO) return ERR_FORMATO;
    if (!json_str(json, "proprietario_id", out->proprietario_id,
                  sizeof out->proprietario_id))
        return ERR_FORMATO;

    // RN-B8: campo faltando é o normal de um formato que evolui.
    if (!json_str(json, "nome", out->nome, sizeof out->nome))
        out->nome[0] = '\0';

    int concluido = 0;
    json_int(json, "onboarding_v", &concluido);
    out->onboarding_v = (uint8_t)(concluido > 0 ? concluido : 0);

    int relogio = 0;
    json_int(json, "relogio_v", &relogio);
    out->relogio_v = (uint8_t)(relogio > 0 ? relogio : 0);

    int sobe = 0;
    json_int(json, "nome_sobe", &sobe);
    out->nome_sobe = sobe ? 1 : 0;
    return OK;
}

erro_t perfil_grava(const hal_t *hal, const perfil_local_t *perfil)
{
    if (!hal || !perfil) return ERR_INTERNO;

    char json[JSON_MAX];
    snprintf(json, sizeof json,
             "{\"v\":%d,\"proprietario_id\":\"%s\",\"nome\":\"%s\","
             "\"nome_sobe\":%u,\"onboarding_v\":%u,\"relogio_v\":%u}",
             CARTAO_FORMATO, perfil->proprietario_id, perfil->nome,
             (unsigned)perfil->nome_sobe,
             (unsigned)perfil->onboarding_v, (unsigned)perfil->relogio_v);
    return grava_atomico(hal, PERFIL_TMP, PERFIL, json);
}


// O nome entra no JSON sem escape: aspas, barra invertida e controle saem
// aqui, venham da tela ou do servidor.
static void nome_limpo(const char *entrada, char *out, size_t max)
{
    char tmp[NOME_UTF8_MAX];
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)entrada;
         *p && n + 1 < sizeof tmp; p++)
        if (*p != '"' && *p != '\\' && *p >= 0x20) tmp[n++] = (char)*p;
    tmp[n] = '\0';
    perfil_nome_trunca(tmp, out, max);
}

erro_t perfil_renomeia(const hal_t *hal, const char *nome, bool local)
{
    if (!hal || !nome) return ERR_INTERNO;

    perfil_local_t p;
    if (perfil_carrega(hal, &p) != OK) {
        memset(&p, 0, sizeof p);
        // RN-6D: o id nasce UMA vez. Só aqui, e só quando ainda não há dono.
        perfil_novo_id(hal, p.proprietario_id);
    }

    nome_limpo(nome, p.nome, sizeof p.nome);
    p.nome_sobe = local ? 1 : 0;
    return perfil_grava(hal, &p);
}
