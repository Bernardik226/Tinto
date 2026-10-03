#include "conexao.h"
#include "campos.h"
#include <string.h>
#include <stdio.h>

// Três palavras, não um número: "-67 dBm" não decide nada. O número serve
// ao ícone da barra.
void vista_conexao(const estado_t *e, vista_cartao_t *out)
{
    cartao_comeca(e, out, "Conexão");
    // O ícone diz o assunto antes de qualquer palavra: o da barra, na força
    // atual.
    out->icone   = ICO_WIFI;
    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s",
             e->pilha[0] == TELA_INICIO ? "BACK voltar" : "BACK ajustes");
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK procurar");

    // ── as duas falhas, cada uma com a sua saída ─────────────────────────
    // Vêm primeiro: são o que aconteceu por último, e é o que resolve.
    if (e->rede != REDE_LIGADA && e->wifi_falha != WIFI_FALHA_NENHUMA &&
        e->wifi_alvo[0]) {
        snprintf(out->nome, sizeof out->nome, "%s", e->wifi_alvo);
        snprintf(out->secao, sizeof out->secao, "%s", "AÇÕES");

        if (e->wifi_falha == WIFI_FALHA_SENHA) {
            out->estado = CONEXAO_SENHA_RECUSADA;
            snprintf(out->kicker, sizeof out->kicker, "%s", "senha recusada");
            cartao_fato(out, "Estado", "não conectou");
            snprintf(out->corpo, sizeof out->corpo, "%s",
                     "A rede não aceitou a senha. O aparelho parou de "
                     "tentar — digite de novo quando quiser.");
            cartao_destino(out, ICO_RENOMEAR, "Digitar a senha de novo",
                    "Para esta mesma rede", NULL);
        } else {
            out->estado = CONEXAO_SEM_RESPOSTA;
            snprintf(out->kicker, sizeof out->kicker, "%s", "sem resposta");
            cartao_fato(out, "Estado", "não conectou");
            // Sem oferecer o teclado: a senha não é o problema.
            snprintf(out->corpo, sizeof out->corpo, "%s",
                     "A rede não respondeu. Ela pode estar fora de "
                     "alcance ou desligada.");
            cartao_destino(out, ICO_SINCRONIZA, "Tentar de novo",
                    "Nesta mesma rede", NULL);
        }

        cartao_destino(out, ICO_CAT_CONEXAO2, "Procurar redes",
                "Escolher outra conexão", NULL);
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK abrir");
        return;
    }

    if (e->rede == REDE_CONECTANDO && e->wifi_alvo[0]) {
        // O nome da rede fica no card ("estou entrando na certa?"). BACK sai sem
        // cancelar a tentativa, e o rodapé diz isso.
        out->estado = CONEXAO_CONECTANDO;
        snprintf(out->kicker, sizeof out->kicker, "%s", "conectando");
        snprintf(out->nome, sizeof out->nome, "%s", e->wifi_alvo);
        cartao_fato(out, "Estado", "tentando entrar");

        // Os pontinhos andam com o RELÓGIO, não com o número de desenhos.
        out->pontos = (int)((e->agora_ms / 1000u) % 4u);
        snprintf(out->corpo, sizeof out->corpo, "%s",
                 "Sair desta tela não cancela: ele continua tentando.");
        cartao_destino(out, ICO_CAT_CONEXAO2, "Procurar redes",
                "Escolher outra conexão", NULL);
        return;
    }

    if (e->rede == REDE_LIGADA && e->wifi_atual[0]) {
        out->estado = CONEXAO_CONECTADA;
        snprintf(out->kicker, sizeof out->kicker, "%s", "rede atual");
        snprintf(out->nome, sizeof out->nome, "%s", e->wifi_atual);

        cartao_fato(out, "Estado", "conectada");
        cartao_fato(out, "Sinal", vista_forca_texto(e->wifi_forca));
        // O endereço é a PROVA de que está conectada.
        cartao_fato(out, "Endereço", e->wifi_ip[0] ? e->wifi_ip : "—");
    } else {
        out->estado = CONEXAO_SEM_REDE;
        snprintf(out->kicker, sizeof out->kicker, "%s", "sem rede");
        snprintf(out->nome, sizeof out->nome, "%s",
                 e->wifi_salva[0] ? e->wifi_salva : "Não conectado");

        if (e->wifi_salva[0]) {
            cartao_fato(out, "Estado", e->rede == REDE_SEM_SINAL ? "fora de alcance"
                                                          : "desconectado");
            cartao_fato(out, "Salva", "sim");
        } else {
            cartao_fato(out, "Estado", "desconectado");
        }

        // O que o aparelho CONTINUA fazendo sem rede.
        snprintf(out->corpo, sizeof out->corpo, "%s",
                 "Sem rede o aparelho mostra o dia e guarda o que você "
                 "falar. O que sobe e desce espera a conexão voltar.");
    }

    snprintf(out->secao, sizeof out->secao, "%s", "AÇÕES");
    cartao_destino(out, ICO_CAT_CONEXAO2, "Procurar redes", "Escolher outra conexão", NULL);

    // Só quando há o que esquecer, e dizendo o custo antes.
    if (e->wifi_salva[0])
        cartao_destino(out, ICO_LIXO, "Esquecer esta rede",
                "Vai pedir a senha de novo", NULL);
}

int vista_conexao_paradas(const estado_t *e)
{
    vista_cartao_t v;
    vista_conexao(e, &v);
    return v.n_dest > 0 ? v.n_dest : 1;
}
