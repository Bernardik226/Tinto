#include "vincular.h"
#include "campos.h"
#include <stdio.h>
#include <string.h>

void vista_vincular(const estado_t *e, vista_vincular_t *out)
{
    memset(out, 0, sizeof *out);
    snprintf(out->titulo, sizeof out->titulo, "%s", "Conectar");
    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    out->bateria = e->bateria;
    out->wifi = vista_wifi_da_barra(e);
    out->sinc = vista_sinc_da_barra(e);

    // ── ainda sem token ──────────────────────────────────────────────────
    // O código só existe depois do registro, que acontece sozinho logo depois do
    // Wi-Fi. A tela diz isso.
    if (!e->tem_token) {
        out->sem_token = true;
        snprintf(out->id, sizeof out->id, "%s", e->meu_id);
        snprintf(out->titulo, sizeof out->titulo, "%s", "Conectando");

        snprintf(out->passo[0], sizeof out->passo[0], "%s",
                 "O Tinto está se apresentando");
        snprintf(out->passo[1], sizeof out->passo[1], "%s",
                 "ao seu servidor.");
        snprintf(out->passo[2], sizeof out->passo[2], "%s",
                 "Esta tela continua sozinha.");

        snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK voltar");
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "tentando…");
        return;
    }

    // Vinculado, o QR segue útil (o app gerencia livros, aparelhos e voz); o
    // que perde sentido é gerar outro código.
    if (e->nome[0]) {
        out->somente_app = true;
        snprintf(out->id, sizeof out->id, "%s", e->meu_id);
        snprintf(out->titulo, sizeof out->titulo, "%s", "Aplicativo");
        snprintf(out->passo[0], sizeof out->passo[0], "%s",
                 "Aponte a câmera e abra o aplicativo.");
        snprintf(out->passo[1], sizeof out->passo[1], "%s",
                 "Livros, dispositivos e uso de voz.");
        snprintf(out->passo[2], sizeof out->passo[2], "%s",
                 "");
        snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK voltar");
        return;
    }

    // ── três estados, e a pessoa manda em dois ──────────────────────────
    //   pode_gerar   nada pedido ainda, ou expirou
    //   esperando    pedido, sem resposta
    //   com código   as seis letras e quanto ainda valem
    // Gerar é um GESTO: código de uso único não se gasta a cada olhada.
    uint32_t agora = e->agora_ms;
    bool expirou = e->codigo_ate_ms == 0 ||
                   (int32_t)(agora - e->codigo_ate_ms) >= 0;

    out->esperando = e->codigo[0] == '\0' && e->nuvem_esperando != 0;

    if (out->esperando) {
        snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK voltar");
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "pedindo…");
        return;
    }

    if (e->codigo[0] == '\0' || expirou) {
        out->pode_gerar = true;

        // O código expirado fica na tela, e a linha embaixo diz que morreu:
        // sumir faria a pessoa achar que apertou errado.
        if (e->codigo[0]) {
            snprintf(out->codigo, sizeof out->codigo, "%s", e->codigo);
            snprintf(out->prazo, sizeof out->prazo, "%s", "expirou");
        }

        snprintf(out->passo[0], sizeof out->passo[0], "%s",
                 "1. Aponte a câmera e abra o aplicativo.");
        snprintf(out->passo[1], sizeof out->passo[1], "%s",
                 "2. Entre com a sua conta Google lá.");
        snprintf(out->passo[2], sizeof out->passo[2], "%s",
                 "3. Peça o código aqui e digite lá.");

        snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK voltar");
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s",
                 e->codigo[0] ? "OK gerar outro" : "OK gerar código");
        return;
    }

    snprintf(out->codigo, sizeof out->codigo, "%s", e->codigo);

    // Arredonda PARA CIMA: com 10 s, "1 min" (zero lê como "acabou").
    uint32_t falta_ms = e->codigo_ate_ms - agora;
    int min = (int)((falta_ms + 59999u) / 60000u);
    snprintf(out->prazo, sizeof out->prazo, "vale %d min", min);

    // Os três passos na ordem em que acontecem: o login vem antes do código,
    // porque o consentimento é da PESSOA.
    snprintf(out->passo[0], sizeof out->passo[0], "%s",
             "1. Aponte a câmera e abra o aplicativo.");
    snprintf(out->passo[1], sizeof out->passo[1], "%s",
             "2. Entre com a sua conta Google lá.");
    snprintf(out->passo[2], sizeof out->passo[2], "%s",
             "3. Digite estas seis letras.");

    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK voltar");
    // "esperando" sem reticências: os pontinhos ao lado é que andam.
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "esperando");
    out->pontos = (int)((e->agora_ms / 1000u) % 4u);
}
