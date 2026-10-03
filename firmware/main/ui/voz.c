// ui/voz.c — ver voz.h.
#include "voz.h"
#include "chrome.h"
#include "../tela/texto.h"
#include "../tela/icones.h"
#include <string.h>
#include <stdio.h>

// A altura é FIXA nos três assuntos (grava, pausa, estrutura): retângulo
// que muda de tamanho vira parcial de tela cheia. As linhas de baixo ficam
// reservadas, vazias.
#define ALTURA        FAIXA_VOZ_A
#define BARRA_TRECHOS 9

// A onda: oito alturas lidas com deslocamento, andando com o quadro. NÃO
// mede a voz (o nível do microfone era instável); responde "está me
// ouvindo?". Quanto foi falado é a barra de trechos.
static void onda(bitmap_t *bm, int x, int y, int quadro)
{
    static const int ALTURAS[8] = { 3, 7, 12, 16, 12, 7, 4, 9 };
    const int N = 14, LARG = 3, VAO = 2;

    for (int i = 0; i < N; i++) {
        int a = ALTURAS[(i + quadro) & 7];
        gfx_ret(bm, x + i * (LARG + VAO), y + (18 - a) / 2, LARG, a, true);
    }
}

const char *ui_voz_visivel(const char *texto, int larg, int linhas)
{
    if (!texto || larg <= 0 || linhas <= 0) return texto;

    int i = 0;
    while (texto[i]) {
        // O que sobra daqui já cabe? Então é daqui que se desenha.
        int n = gfx_cabe(F_MIUDA, texto + i, larg * linhas);
        if (!texto[i + n]) break;

        // Não cabe: pula uma palavra inteira e tenta de novo.
        int passo = 0;
        while (texto[i + passo] && texto[i + passo] != ' ') passo++;
        while (texto[i + passo] == ' ') passo++;

        // Palavra única maior que a janela: sem isto o laço travaria.
        if (!passo || !texto[i + passo]) break;
        i += passo;
    }
    return texto + i;
}

void ui_voz_area(int *x, int *y, int *l, int *a)
{
    ret_t r = grid_faixa_de(ALTURA);
    *x = r.x; *y = r.y; *l = r.l; *a = r.a;
}

void ui_voz(bitmap_t *bm, const vista_grav_t *v)
{
    // Negativo enquanto GRAVA, papel quando pausado: a pele diz "parou" antes
    // de qualquer palavra.
    faixa_t f = faixa_abre(bm, ALTURA, !v->pausado);
    int y = f.y;

    // ── quando deu errado, a faixa muda de assunto ──────────────────────
    // As duas linhas de tempo e trechos viram a explicação, com a mesma altura.
    if (v->nao_comecou != OK || v->nao_enviou || v->nada_entendido ||
        v->sem_resposta) {
        gfx_texto(bm, f.x, y, F_TITULO, v->titulo);
        y += gfx_altura_linha(F_TITULO) + 5;
        gfx_paragrafo(bm, f.x, y, f.util, 2, F_MIUDA, v->aviso);
        faixa_botoes(bm, &f, v->rodape_esq, v->rodape_dir);
        faixa_fecha(bm, &f);
        return;
    }

    // ── linha 1: o tempo, e a onda ao lado ──────────────────────────────
    // O tempo correndo também é o que um aparelho travado faz; a onda responde
    // se está ouvindo.
    gfx_texto(bm, f.x, y, F_TITULO, v->tempo);
    int wx = f.x + gfx_largura(F_TITULO, "00:00") + 12;
    if (!v->pausado) {
        onda(bm, wx, y, v->onda);
    } else if (v->fase_gravando) {
        // PAUSADO no lugar da onda, no corpo do tempo.
        gfx_texto(bm, wx, y, F_TITULO, "Pausado");
    }
    y += gfx_altura_linha(F_TITULO) + 4;

    // ── linha 2: a barra de trechos, em fatias proporcionais ────────────
    // A prova de que a retomada entrou no mesmo arquivo (RN-13).
    gfx_ret(bm, f.x, y, f.util, BARRA_TRECHOS, false);

    int total = 0;
    for (int i = 0; i < v->n_fatias; i++) total += v->fatias[i];
    if (total < 1) total = 1;

    int px = f.x + 2;
    for (int i = 0; i < v->n_fatias; i++) {
        int w = (f.util - 4) * v->fatias[i] / total;
        // O vão de 1 px separa as fatias.
        if (w > 1) gfx_ret(bm, px, y + 2, w - 1, BARRA_TRECHOS - 4, true);
        px += w;
    }
    y += BARRA_TRECHOS + 5;

    // ── linha 3: o estado ───────────────────────────────────────────────
    // Duas linhas sempre reservadas. Gravando, vazias; quem escreve é a espera
    // (e a frase quando o prazo estoura). Não cabendo, mostra o FIM.
    if (v->estruturando) {
        // ── a espera, em negrito, com os pontos ao lado ─────────────────────
        gfx_texto(bm, f.x, y, F_TITULO, v->aviso);

        // Pontos em POSIÇÃO FIXA, um aceso por vez: o que se move é o estado, não
        // a largura da frase.
        chrome_pontinhos_um_aceso(bm, f.x + gfx_largura(F_TITULO, v->aviso) + 8,
                                  y + gfx_altura_linha(F_TITULO) / 2 - 4,
                                  v->pontos);
    } else if (v->aviso[0]) {
        gfx_paragrafo(bm, f.x, y, f.util, 2, F_MIUDA,
                      ui_voz_visivel(v->aviso, f.util, 2));
    }

    faixa_botoes(bm, &f, v->rodape_esq, v->rodape_dir);
    faixa_fecha(bm, &f);
}

// ── a recusa ─────────────────────────────────────────────────────────
// O leque cortado diz o estado antes da frase, e a frase diz o REMÉDIO.
// Sem atalho: a faixa informa e fecha. Cada motivo tem um remédio diferente.
void ui_faixa_recusa(bitmap_t *bm, const char *gesto, int motivo)
{
    faixa_t f = faixa_abre(bm, FAIXA_AVISO_A, false);
    int y = f.y;

    gfx_icone(bm, f.x, y + 2, ICO_SEM_REDE);
    int tx = f.x + ICONES[ICO_SEM_REDE].l + 11;

    const char *titulo, *botao;
    char frase[96];

    switch (motivo) {
    case RECUSA_MINUTOS:
        titulo = "Seus minutos acabaram";
        botao  = "OK ver voz";
        snprintf(frase, sizeof frase, "%s",
                 "Você usou toda a fala deste ciclo. A tela de Voz diz "
                 "quando ele vira.");
        break;

    case RECUSA_CONTA:
        // Diferente de "sem contato": uma manda esperar, a outra manda fazer.
        titulo = "Reconecte sua conta";
        botao  = "OK Minha conta";
        snprintf(frase, sizeof frase, "%s",
                 "O Google deixou de reconhecer este aparelho. Conecte a "
                 "conta de novo para voltar a sincronizar.");
        break;

    case RECUSA_SEM_CONTA:
        titulo = "Conecte sua conta";
        botao  = "OK Minha conta";
        snprintf(frase, sizeof frase, "%s",
                 "Este Tinto ainda não tem uma conta Google vinculada. "
                 "Abra Minha conta para ver o QR e o código.");
        break;

    case RECUSA_TOKEN:
        // Sem botão: o aparelho se reapresenta sozinho.
        titulo = "Reconectando ao servidor";
        botao  = "";
        snprintf(frase, sizeof frase, "%s",
                 "O servidor não reconhece mais este Tinto. Ele está "
                 "tentando se apresentar de novo.");
        break;

    case RECUSA_ACAO:
        // Sem botão: refazer o gesto é o que resta, e a pessoa não pode achar que
        // a ação aconteceu.
        titulo = "Uma ação não subiu";
        botao  = "";
        snprintf(frase, sizeof frase, "%s",
                 "O servidor recusou o último gesto. O que você fez aqui "
                 "não chegou ao Google — refaça quando puder.");
        break;

    default:
        titulo = "Dispositivo offline";
        botao  = "OK conectar";
        // Diz O QUE não deu ("para marcar feita"), não só que a rede caiu.
        snprintf(frase, sizeof frase, "Conecte a uma rede Wi-Fi para %s.",
                 gesto && *gesto ? gesto : "esta ação");
        break;
    }

    gfx_texto(bm, tx, y, F_CORPO, titulo);
    y += gfx_altura_linha(F_CORPO) + 3;

    // Quantas linhas CABEM, não quantas eu gostaria (a segunda caía sobre os
    // botões).
    int sobra  = faixa_conteudo_a(&f) - (y - f.y);
    int linhas = sobra / gfx_altura_linha(F_MIUDA);
    if (linhas > 2) linhas = 2;
    if (linhas > 0)
        gfx_paragrafo(bm, tx, y, f.util - (tx - f.x), linhas, F_MIUDA, frase);

    faixa_botoes(bm, &f, "BACK voltar", botao);
    faixa_fecha(bm, &f);
}

// A confirmação do gesto (ver voz.h). Mais baixa que a recusa: confirmar
// não explica, e uma faixa alta cobriria a data nova.
void ui_faixa_feito(bitmap_t *bm, const char *o_que, const char *detalhe)
{
    faixa_t f = faixa_abre(bm, FAIXA_AVISO_A / 2, false);

    gfx_icone(bm, f.x, f.y + 2, ICO_CAIXA_ON);
    int tx = f.x + ICONES[ICO_CAIXA_ON].l + 10;

    gfx_texto_ate(bm, tx, f.y, F_CORPO, o_que, f.util - (tx - f.x));

    if (detalhe && detalhe[0])
        gfx_texto_ate(bm, tx, f.y + gfx_altura_linha(F_CORPO) + 1, F_MIUDA,
                      detalhe, f.util - (tx - f.x));

    faixa_fecha(bm, &f);
}
