#include "wifi.h"
#include "campos.h"
#include <stdio.h>
#include <string.h>

// A régua do sinal, UMA: o texto, o ícone da lista e o da barra dizem a
// mesma coisa sobre a mesma força.
icone_id vista_icone_wifi(int forca)
{
    if (forca >= 66) return ICO_WIFI_3;
    if (forca >= 33) return ICO_WIFI_2;
    return ICO_WIFI_1;
}

static const char *forca_texto(int forca)
{
    if (forca >= 66) return "forte";
    if (forca >= 33) return "média";
    return "fraca";
}

void vista_wifi(const estado_t *e, int cabem, vista_menu_t *out)
{
    memset(out, 0, sizeof *out);
    snprintf(out->titulo, sizeof out->titulo, "%s", "Redes");
    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    out->bateria = e->bateria;
    out->wifi = vista_wifi_da_barra(e);
    out->sinc = vista_sinc_da_barra(e);
    out->cursor  = e->travado ? -1 : e->cursor;
    out->pontos  = -1;

    // Só a LISTA: o resumo é `vista_conexao` (juntas, cada rede que chegava
    // empurrava o resumo). "Procurando" mora no RÓTULO da seção: uma linha que
    // some empurraria a lista e o cursor. Os pontinhos andam ao lado, com o
    // relógio, como no "Buscando" da Sincronização.
    const char *secao = e->wifi_procurando ? "REDES · PROCURANDO" : "REDES";
    out->pontos = e->wifi_procurando ? (int)((e->agora_ms / 1000u) % 4u) : -1;

    int primeira_rede = out->n;
    for (int i = 0; i < e->n_redes && i < REDES_MAX; i++) {
        // A conectada já está no card da Conexão.
        if (e->rede == REDE_LIGADA && strcmp(e->redes[i].nome, e->wifi_atual) == 0)
            continue;

        // O ícone já diz o nível: escolher rede vira varrer, não ler.
        vista_menu_poe(out, out->n == primeira_rede ? secao : "",
                       vista_icone_wifi(e->redes[i].forca),
                       e->redes[i].nome, "");

        // O cadeado: rede fechada vai pedir senha.
        if (!e->redes[i].aberta)
            out->linhas[out->n - 1].icone2 = ICO_CADEADO;

        // Força sempre; "salva" e "aberta" quando informam. Na LEGENDA, não na
        // coluna do valor, que cortava o nome da rede.
        const char *extra = e->redes[i].salva  ? " · salva"
                          : e->redes[i].aberta ? " · aberta" : "";
        snprintf(out->sub[out->n - 1], sizeof out->sub[0], "%s%s",
                 forca_texto(e->redes[i].forca), extra);
    }

    // ── vazio ACIONÁVEL ──────────────────────────────────────────────────
    // Não achar nada é resultado, não erro: a manchete diz o que houve e a nota
    // o que fazer no MUNDO (aproximar, ligar o roteador).
    if (e->n_redes == 0) {
        // Varrendo, a manchete é a espera, com os pontinhos.
        snprintf(out->vazio, sizeof out->vazio, "%s",
                 e->wifi_procurando ? "Procurando redes"
                                    : "Nenhuma rede encontrada.");
    }

    vista_menu_poe(out, "", ICO_SINCRONIZA, "Procurar de novo", "");
    vista_menu_poe(out, "", ICO_LISTA, "Rede oculta", "");
    snprintf(out->sub[out->n - 1], sizeof out->sub[0], "%s", "Digitar o nome");

    // A nota diz o que FAZER: quem está aqui veio conectar.
    if (e->n_redes == 0 && !e->wifi_procurando)
        snprintf(out->nota, sizeof out->nota, "%s",
                 "Aproxime o Tinto do roteador ou verifique se a rede está "
                 "ligada. Redes ocultas podem ser digitadas.");
    else
        snprintf(out->nota, sizeof out->nota, "%s",
                 "Selecione uma rede Wi-Fi disponível para estabelecer "
                 "conexão com o servidor.");
    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK conexão");
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK conectar");
    vista_menu_rola(out, cabem);
}
