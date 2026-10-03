#include "nuvem.h"
#include "cartao.h"
#include "json.h"

#include <stdio.h>
#include <string.h>

#define ARQUIVO     CARTAO_RAIZ "/sistema/nuvem.json"
#define ARQUIVO_TMP CARTAO_RAIZ "/sistema/nuvem.json.tmp"

#define JSON_MAX 384

// ── as chaves do cofre ───────────────────────────────────────────────
// Prova e token vão para a NVS (o cartão sai com a unha). No cartão fica o
// que não é segredo e ajuda a diagnosticar: servidor, MAC, conta. Nomes
// curtos: a NVS limita a 15 caracteres.
#define CHAVE_PROVA "nuvem_prova"
#define CHAVE_TOKEN "nuvem_token"

bool nuvem_registrado(const nuvem_cred_t *cred)
{
    return cred && cred->token[0] != '\0';
}

// A prova nasce UMA vez: regenerá-la seria trocar de aparelho, e o servidor
// recusaria o registro seguinte.
static void nova_prova(const hal_t *hal, char out[33])
{
    uint8_t bytes[16] = {0};
    if (hal->aleatorio) hal->aleatorio(bytes, sizeof bytes);

    for (size_t i = 0; i < sizeof bytes; i++)
        snprintf(out + i * 2, 3, "%02x", bytes[i]);
    out[32] = '\0';
}

erro_t nuvem_carrega(const hal_t *hal, nuvem_cred_t *out)
{
    if (!hal || !out) return ERR_INTERNO;
    memset(out, 0, sizeof *out);

    char json[JSON_MAX];
    erro_t err = hal->ler ? hal->ler(ARQUIVO, json, sizeof json) : ERR_ARQUIVO;

    // Prova vinda do cartão antigo, fonte da migração.
    char prova_velha[sizeof out->prova];
    char token_velho[sizeof out->token];
    prova_velha[0] = token_velho[0] = '\0';

    if (err == OK) {
        (void)json_str(json, "servidor",  out->servidor,  sizeof out->servidor);
        (void)json_str(json, "device_id", out->device_id, sizeof out->device_id);
        (void)json_str(json, "conta",     out->conta,     sizeof out->conta);
        (void)json_str(json, "prova", prova_velha, sizeof prova_velha);
        (void)json_str(json, "token", token_velho, sizeof token_velho);
    }

    // O cofre é a fonte.
    if (hal->segredo_le) {
        (void)hal->segredo_le(CHAVE_PROVA, out->prova, sizeof out->prova);
        (void)hal->segredo_le(CHAVE_TOKEN, out->token, sizeof out->token);
    }

    // ── migração do cartão antigo ───────────────────────────────────────
    // Cofre vazio + prova e token no arquivo = migra uma vez. Ignorá-los faria o
    // aparelho se registrar de novo, e o servidor (com a prova fixada) recusaria.
    // O `nuvem_grava` no fim reescreve o arquivo sem os dois.
    bool migrar = false;
    if (!out->prova[0] && prova_velha[0]) {
        snprintf(out->prova, sizeof out->prova, "%s", prova_velha);
        migrar = true;
    }
    if (!out->token[0] && token_velho[0]) {
        snprintf(out->token, sizeof out->token, "%s", token_velho);
        migrar = true;
    }

    // Arquivo ausente e incompleto caem no mesmo caminho: completar o que este
    // lado sabe preencher.
    //
    // Só HTTPS: o endereço vem do cartão, que qualquer um edita, e um `http://`
    // entregaria o token em claro. Vazio ou inseguro cai no de fábrica.
    if (strncmp(out->servidor, "https://", 8) != 0)
        snprintf(out->servidor, sizeof out->servidor, "%s",
                 NUVEM_SERVIDOR_PADRAO);

    if (out->device_id[0] == '\0' && hal->id_aparelho)
        hal->id_aparelho(out->device_id, sizeof out->device_id);

    if (out->prova[0] == '\0')
        nova_prova(hal, out->prova);

    // A migração grava pelo caminho de sempre (segredos ao cofre, resto ao
    // cartão). Falhar não perde nada: a próxima leitura tenta de novo.
    if (migrar) (void)nuvem_grava(hal, out);

    return err;
}

erro_t nuvem_grava(const hal_t *hal, const nuvem_cred_t *cred)
{
    if (!hal || !cred || !hal->escrever || !hal->renomear || !hal->ler)
        return ERR_INTERNO;

    // O COFRE primeiro, e obrigatório: sem token salvo, o boot seguinte pediria
    // registro com uma prova que o servidor já fixou — Tinto pareado vira morto.
    if (!hal->segredo_grava) return ERR_INTERNO;

    erro_t e = hal->segredo_grava(CHAVE_PROVA, cred->prova);
    if (e != OK) return e;
    e = hal->segredo_grava(CHAVE_TOKEN, cred->token);
    if (e != OK) return e;

    char json[JSON_MAX];
    int n = snprintf(json, sizeof json,
                     "{\"v\":2,\"servidor\":\"%s\",\"device_id\":\"%s\","
                     "\"conta\":\"%s\"}",
                     cred->servidor, cred->device_id, cred->conta);
    if (n <= 0 || (size_t)n >= sizeof json) return ERR_INTERNO;

    erro_t err = hal->escrever(ARQUIVO_TMP, json);
    if (err != OK) return err;

    err = hal->renomear(ARQUIVO_TMP, ARQUIVO);
    if (err != OK) return err;

    // Reler separa "o driver aceitou" de "o cartão guardou".
    char lido[JSON_MAX];
    err = hal->ler(ARQUIVO, lido, sizeof lido);
    if (err != OK) return err;

    return strcmp(lido, json) == 0 ? OK : ERR_ARQUIVO;
}

erro_t nuvem_esquece_token(const hal_t *hal)
{
    nuvem_cred_t cred;
    (void)nuvem_carrega(hal, &cred);

    if (cred.token[0] == '\0') return OK;      // já não havia

    cred.token[0] = '\0';
    return nuvem_grava(hal, &cred);
}
