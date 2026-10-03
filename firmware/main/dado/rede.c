#include "rede.h"
#include "json.h"

#include <stdio.h>
#include <string.h>

#define CAMINHO "/TINTO/sistema/wifi.json"

// Curta: a NVS limita o nome a 15 caracteres, e truncar calado colidiria
// com outra chave.
#define CHAVE_SENHA "wifi_senha"

// Só o NOME vai para o cartão (aparece na tela, ajuda a diagnosticar).
static erro_t grava_o_nome(const hal_t *hal, const char *nome)
{
    char json[192];
    snprintf(json, sizeof json, "{\"ssid\":\"%s\"}", nome);

    // RN-64: .tmp + rename, como todo JSON do cartão.
    erro_t e = hal->escrever(CAMINHO ".tmp", json);
    if (e != OK) return e;

    e = hal->renomear(CAMINHO ".tmp", CAMINHO);
    if (e != OK) { hal->apagar(CAMINHO ".tmp"); return e; }
    return OK;
}

erro_t rede_carrega(const hal_t *hal, rede_salva_t *out)
{
    if (!hal || !out) return ERR_INTERNO;
    memset(out, 0, sizeof *out);

    char json[192];
    erro_t e = hal->ler(CAMINHO, json, sizeof json);
    if (e != OK) return e;

    if (!json_str(json, "ssid", out->nome, sizeof out->nome)) return ERR_FORMATO;
    if (!out->nome[0]) return ERR_FORMATO;

    // ── migração do cartão antigo ───────────────────────────────────────
    // A primeira leitura leva a senha antiga do cartão para o cofre e reescreve
    // o cartão sem ela. Ignorar perderia o Wi-Fi na atualização.
    char antiga[sizeof out->senha];
    antiga[0] = '\0';
    if (json_str(json, "senha", antiga, sizeof antiga) && antiga[0]) {
        if (hal->segredo_grava) (void)hal->segredo_grava(CHAVE_SENHA, antiga);
        (void)grava_o_nome(hal, out->nome);
    }

    if (hal->segredo_le)
        (void)hal->segredo_le(CHAVE_SENHA, out->senha, sizeof out->senha);

    // Sem senha no cofre mas com a antiga: vale a antiga (o cofre falhou ao
    // gravar).
    if (!out->senha[0] && antiga[0])
        snprintf(out->senha, sizeof out->senha, "%s", antiga);

    return OK;
}

erro_t rede_grava(const hal_t *hal, const rede_salva_t *rede)
{
    if (!hal || !rede || !rede->nome[0]) return ERR_INTERNO;

    // O cofre PRIMEIRO, e obrigatório: nome sem senha faria a tela dizer "senha
    // errada" sobre uma senha certa. Sem cofre, recusa em vez de dizer que salvou.
    if (!hal->segredo_grava) return ERR_INTERNO;

    erro_t e = hal->segredo_grava(CHAVE_SENHA, rede->senha);
    if (e != OK) return e;

    return grava_o_nome(hal, rede->nome);
}

erro_t rede_esquece(const hal_t *hal)
{
    if (!hal) return ERR_INTERNO;

    // A senha vai junto: o aparelho passado adiante não leva a senha da casa.
    if (hal->segredo_apaga) (void)hal->segredo_apaga(CHAVE_SENHA);
    return hal->apagar(CAMINHO);
}
