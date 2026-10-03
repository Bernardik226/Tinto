#include "teclado.h"
#include "campos.h"
#include <stdio.h>
#include <string.h>

// Três modos no mesmo grid; a última linha troca entre eles. Teclado que
// muda de tamanho faria o cursor pular.
static const char *MAPA[3][TEC_LINS] = {
    { "abcdef", "ghijkl", "mnopqr", "stuvwx", "yz-_.@", "" },
    { "ABCDEF", "GHIJKL", "MNOPQR", "STUVWX", "YZ-_.@", "" },
    { "012345", "6789+-", "!?,;:/", "()[]{}", "#$%&*=", "" },
};

// A última linha é sempre a das ações.
static const char *ACOES[TEC_COLS] = { "ABC", "123", "esp", "del", "ok", "" };

void vista_teclado(const estado_t *e, vista_teclado_t *out)
{
    memset(out, 0, sizeof *out);
    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    out->bateria = e->bateria;
    out->wifi = vista_wifi_da_barra(e);
    out->sinc = vista_sinc_da_barra(e);

    // O contexto é tipado: a mesma grade, perguntas diferentes.
    switch ((teclado_contexto_t)e->teclado_contexto) {
    case TECLADO_PROPRIETARIO:
        snprintf(out->titulo, sizeof out->titulo, "%s", "Proprietário");
        break;
    case TECLADO_WIFI:
        // Só "Senha" na barra (a faixa é dividida com hora e bateria); QUAL rede
        // vai no kicker, com a linha inteira.
        snprintf(out->titulo, sizeof out->titulo, "%s", "Senha");
        break;
    case TECLADO_WIFI_SSID:
        snprintf(out->titulo, sizeof out->titulo, "%s", "Nome da rede");
        break;
    case TECLADO_RENOMEAR:
    default:
        snprintf(out->titulo, sizeof out->titulo, "%s", "Renomear");
        break;
    }
    // ── a senha é MASCARADA ──────────────────────────────────────────────
    // O CONTADOR substitui o texto: sem ele, a letra que faltou só aparece
    // quando o roteador recusa.
    if (e->teclado_contexto == TECLADO_WIFI) {
        size_t n = strlen(e->digitando);
        if (n > sizeof out->texto - 1) n = sizeof out->texto - 1;
        for (size_t i = 0; i < n; i++) out->texto[i] = '*';
        out->texto[n] = '\0';

        snprintf(out->contagem, sizeof out->contagem, "%u caractere%s",
                 (unsigned)n, n == 1 ? "" : "s");

        if (e->wifi_alvo[0])
            snprintf(out->kicker, sizeof out->kicker, "%s · rede protegida",
                     e->wifi_alvo);
    } else {
        snprintf(out->texto, sizeof out->texto, "%s", e->digitando);
    }
    out->restam = (int)(sizeof e->digitando - 1 - strlen(e->digitando));

    // No primeiro uso o teto é o do NOME: 24 caracteres.
    if (e->teclado_contexto == TECLADO_PROPRIETARIO) {
        int usados = 0;
        for (const char *p = e->digitando; *p; p++)
            if ((*p & 0xC0) != 0x80) usados++;
        out->restam = NOME_CARACTERES_MAX - usados;
        if (out->restam < 0) out->restam = 0;
    }

    int modo = e->teclado_modo % 3;
    for (int l = 0; l < TEC_LINS - 1; l++)
        for (int c = 0; c < TEC_COLS; c++) {
            const char *linha = MAPA[modo][l];
            if ((int)strlen(linha) > c)
                snprintf(out->teclas[l][c], sizeof out->teclas[0][0], "%c",
                         linha[c]);
        }
    for (int c = 0; c < TEC_COLS; c++)
        snprintf(out->teclas[TEC_LINS - 1][c], sizeof out->teclas[0][0], "%s",
                 ACOES[c]);

    out->cur_lin = e->teclado_lin;
    out->cur_col = e->teclado_col;

    // A nota de rodapé muda com o contexto.
    if (e->teclado_contexto == TECLADO_PROPRIETARIO)
        snprintf(out->nota, sizeof out->nota, "%s",
                 "Este nome identifica o dono desta memória.");
    else
        snprintf(out->nota, sizeof out->nota, "%s",
                 "Segure uma direção pra andar rápido.");
    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK cancelar");
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK digitar");
}
