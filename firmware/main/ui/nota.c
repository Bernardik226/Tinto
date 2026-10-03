#include "nota.h"
#include "chrome.h"
#include "grid.h"
#include "blocos.h"
#include "rolagem.h"
#include "faixa.h"
#include "../tela/icones.h"
#include <string.h>

#define MARGEM GRID_MARGEM


// ── o detalhe do item ───────────────────────────────────────────────
// O item com o que ELE é: margem 11, kicker em caixa alta, título em serifa,
// origem entre filetes e campos em coluna.
#define NOTA_MARGEM  11

// O vão entre rótulo e valor: abaixo disso "QUANDO hoje" vira uma palavra.
#define NOTA_VAO     12

int tela_nota_coluna(const vista_nota_t *v)
{
    // A coluna sai do rótulo MAIS LARGO desta tela (depende do tipo).
    int maior = 0;
    for (int i = 0; i < v->n_campos; i++) {
        int w = gfx_largura(F_MIUDA, v->campos[i].rotulo);
        if (w > maior) maior = w;
    }
    return maior + NOTA_VAO;
}

// A barra da fala, ancorada acima do rodapé, invertida: em 1 bit o que
// chama é a inversão.
#define FALA_A       37
#define FALA_FUNDO   11

// Teto alto de propósito: quem corta é a rolagem.
#define RESUMO_MAX_LINHAS 32

// Teto só contra laço infinito: 128 bytes não passam de seis linhas.
#define NOTA_TITULO_LINHAS 8

// O corpo desce até onde precisar; quem corta é a rolagem. A MESMA função
// mede e pinta.
static int corpo(bitmap_t *bm, const vista_nota_t *v, int y)
{
    const int x    = NOTA_MARGEM;
    const int larg = bm->l - NOTA_MARGEM * 2;

    // ── o kicker: o que é, e onde está ──
    gfx_texto(bm, x, y, F_MIUDA, v->kicker);
    y += gfx_altura_linha(F_MIUDA) + 5;

    // ── o título, em serifa, INTEIRO ──
    // O que não couber está na página seguinte, nunca em reticências.
    y = gfx_paragrafo(bm, x, y, larg, NOTA_TITULO_LINHAS, F_EDITORIAL,
                      v->tl) + 7;

    // ── a origem, entre filetes ──
    // Discreta: os filetes a separam, sem tom para recuar em 1 bit.
    gfx_hlin(bm, x, y, larg, 1);
    y += 7;
    gfx_ret(bm, x, y - 1, 14, 14, false);
    if (v->origem_google) {
        // O "G" centrado e medido na caixinha.
        int gw = gfx_largura(F_MIUDA, "G");
        int gh = gfx_altura_linha(F_MIUDA);
        gfx_texto(bm, x + (14 - gw) / 2, y - 1 + (14 - gh) / 2, F_MIUDA, "G");
    } else {
        gfx_ret(bm, x + 5, y + 4, 4, 4, true);     // nasceu de uma fala
    }
    gfx_texto(bm, x + 20, y, F_MIUDA, v->origem);
    y += gfx_altura_linha(F_MIUDA) + 6;
    gfx_hlin(bm, x, y, larg, 1);
    y += 12;

    // ── os campos rotulados ──
    const int col = tela_nota_coluna(v);
    for (int i = 0; i < v->n_campos; i++) {
        // Sem filete antes do primeiro: o da origem já está logo acima.
        if (i > 0) { gfx_hlin(bm, x, y, larg, 1); y += 7; }

        // O valor principal sobe um degrau de corpo — e DESCE quando não cabe: a
        // hora do fim vale mais que a ênfase.
        fonte_t f = F_CORPO_P;
        if (v->campos[i].forte &&
            gfx_largura(F_CORPO, v->campos[i].valor) <= larg - col)
            f = F_CORPO;
        gfx_texto(bm, x, y + (v->campos[i].forte ? 2 : 0), F_MIUDA,
                  v->campos[i].rotulo);

        // QUEBRA, não corta: aqui se quer o dado inteiro. Duas linhas para 64
        // bytes.
        y = gfx_paragrafo(bm, x + col, y, larg - col, 2, f,
                          v->campos[i].valor) + 5;
    }

    // ── a descrição, com filete grosso ──
    if (v->tem_resumo && v->resumo[0]) {
        y += 3;
        gfx_hlin(bm, x, y, larg, 2);
        y += 9;

        // INTEIRA: quem resolve o que não cabe é a rolagem.
        y = gfx_paragrafo(bm, x, y, larg, RESUMO_MAX_LINHAS, F_CORPO_P,
                          v->resumo);
    }

    // ── e o que foi DITO, no fim ──
    // Depois do que o item é e quando é. Em itálico: é fala de alguém.
    if (v->transcricao[0]) {
        y += 8;
        gfx_hlin(bm, x, y, larg, 1);
        y += 8;

        gfx_texto(bm, x, y, F_MIUDA, "O QUE VOCÊ DISSE");
        y += gfx_altura_linha(F_MIUDA) + 5;

        y = gfx_paragrafo(bm, x, y, larg, RESUMO_MAX_LINHAS, F_CITACAO,
                          v->transcricao);
    }

    // ── e o que ela CRIOU ──
    // Só quando foi mais de uma coisa.
    if (v->n_criou > 1) {
        y += 8;
        gfx_texto(bm, x, y, F_MIUDA, "O TINTO CRIOU");
        y += gfx_altura_linha(F_MIUDA) + 4;

        for (int i = 0; i < v->n_criou; i++) {
            int w = gfx_largura(F_MIUDA, v->criou[i].estado);
            gfx_texto_ate(bm, x, y, F_CORPO_P, v->criou[i].oque,
                          larg - w - 8);
            gfx_texto(bm, x + larg - w, y + 2, F_MIUDA, v->criou[i].estado);
            y += gfx_altura_linha(F_CORPO_P) + 3;
        }
    }

    return y;
}


// ── a tela ───────────────────────────────────────────────────────────
// O corpo ROLA; os botões ficam ancorados no rodapé. Paradas: as de LEITURA
// (só com transbordo) e depois os botões.

// Onde o corpo pode começar, e onde ele tem de parar.
static int area_do_corpo(const bitmap_t *bm, const vista_nota_t *v,
                         int *topo, int *fundo)
{
    *topo  = BARRA_A + 8;
    *fundo = bm->a - RODAPE_A - 8
           - (v->n_botoes ? v->n_botoes * FALA_A
                          + (v->n_botoes - 1) * 4 + FALA_FUNDO : 0);
    return *fundo - *topo;
}

// Quanto o corpo mede, num bitmap que ninguém vê.
static int altura_do_corpo(const bitmap_t *bm, const vista_nota_t *v)
{
    bitmap_t *medida = rolagem_rascunho(bm);
    if (!medida) return 0;
    return corpo(medida, v, 0);
}

static int rolagens(const bitmap_t *bm, const vista_nota_t *v)
{
    int topo, fundo;
    int area  = area_do_corpo(bm, v, &topo, &fundo);
    int alto = altura_do_corpo(bm, v);
    if (alto <= area) return 0;
    return rolagem_suave_paradas(alto, area);
}

int tela_nota_paradas(bitmap_t *bm, const vista_nota_t *v)
{
    int n = rolagens(bm, v) + v->n_botoes;
    return n > 0 ? n : 1;
}

bool tela_nota_fim_visivel(bitmap_t *bm, const vista_nota_t *v, int parada)
{
    int topo, fundo;
    int area = area_do_corpo(bm, v, &topo, &fundo);
    int alto = altura_do_corpo(bm, v);
    if (alto <= area) return true;

    return alto - rolagem_suave_desloc(parada, alto, area) <= area;
}

void tela_nota(bitmap_t *bm, const vista_nota_t *v)
{
    gfx_limpa(bm, false);

    barra_t b = { v->titulo, v->hora, v->bateria, v->wifi, false, v->sinc };
    chrome_barra(bm, &b);

    const int x    = NOTA_MARGEM;
    const int larg = bm->l - NOTA_MARGEM * 2;

    int topo, fundo;
    int area = area_do_corpo(bm, v, &topo, &fundo);
    int alto = altura_do_corpo(bm, v);
    int n_rol = rolagens(bm, v);

    // Nas paradas de leitura a rolagem segue o cursor; nos botões vai ao fim,
    // para conferir antes de agir.
    int desloc = alto > area
               ? rolagem_suave_desloc(v->cursor_bruto < n_rol ? v->cursor_bruto
                                                              : n_rol,
                                       alto, area)
               : 0;

    corpo(bm, v, topo - desloc);

    // A barra por cima do que rolou para fora (as primitivas recortam em y < 0,
    // não abaixo), com um respiro: linha cortada colada nela parece falha.
    gfx_limpa_ret(bm, 0, 0, bm->l, BARRA_A + 4);
    chrome_barra(bm, &b);

    // E o que passou do fundo sai: os botões são ancorados.
    if (fundo < bm->a - RODAPE_A)
        gfx_limpa_ret(bm, 0, fundo + 2, bm->l, bm->a - RODAPE_A - fundo - 2);

    // A seta é a pista de que rolar adianta.
    rolagem_setas_laterais(bm, desloc, alto, area, topo, fundo);

    // ── os botões, ancorados acima do rodapé ────────────────────────────
    // Concluir primeiro. O que está EM FOCO inverte.
    if (v->n_botoes > 0) {
        int fy = bm->a - RODAPE_A - FALA_FUNDO - v->n_botoes * FALA_A
               - (v->n_botoes - 1) * 4;

        for (int i = 0; i < v->n_botoes; i++) {
            gfx_ret(bm, x, fy, larg, FALA_A, false);

            int ty = fy + (FALA_A - gfx_altura_linha(F_CORPO_P)) / 2;
            gfx_ret(bm, x + 9, ty + 3, 6, 6, v->botoes[i].principal);
            // 170 px: o que "Marcar como concluída" mede.
            gfx_texto_ate(bm, x + 22, ty, F_CORPO_P, v->botoes[i].texto,
                          larg - 46);
            // ">" e não "›": a fonte não tem U+203A.
            gfx_texto(bm, x + larg - 20, ty, F_CORPO_P, ">");

            // Lendo, nenhum botão em foco.
            int foco = (alto > area && v->cursor_bruto < n_rol)
                     ? -1 : v->botao_foco;
            if (i == foco) gfx_negativo(bm, x, fy, larg, FALA_A);
            fy += FALA_A + 4;
        }
    }

    // ── a gaveta de ações ───────────────────────────────────────────────
    // As MESMAS peças da gaveta do MENU (`faixa_abre`, `bloco_acao_em`,
    // `faixa_fecha`).
    if (v->acoes_abertas) {
        int alt = FAIXA_TOPO + 7 + gfx_altura_linha(F_MIUDA) + 6
                + (v->n_acoes ? v->n_acoes * (gfx_altura_linha(F_CORPO) + 7)
                              : gfx_altura_linha(F_MIUDA) + 7)
                + 4;

        faixa_t g = faixa_abre(bm, alt, false);
        int gy = g.y;

        gfx_texto(bm, g.x, gy, F_MIUDA, "Ações");
        gy += gfx_altura_linha(F_MIUDA) + 6;

        if (v->n_acoes == 0)
            gfx_texto_ate(bm, g.x, gy, F_MIUDA, v->sem_acoes, g.util);
        else
            for (int i = 0; i < v->n_acoes; i++)
                gy = bloco_acao_em(bm, g.x, gy, g.util, &v->acoes[i],
                                   i == v->cursor_acao);

        faixa_fecha(bm, &g);
    }

    // Numa parada de leitura o OK não faz nada, e o rodapé não promete.
    const char *dir = (alto > area && v->cursor_bruto < n_rol)
                    ? "▼ continuar" : v->rodape_dir;
    chrome_rodape(bm, v->rodape_esq, dir, false);
}

// Quantas linhas o título ocupa, para o teste provar que não corta.
int tela_nota_titulo_linhas(bitmap_t *bm, const vista_nota_t *v)
{
    bitmap_t *medida = rolagem_rascunho(bm);
    if (!medida) return 0;

    int alto = gfx_paragrafo(medida, 0, 0, bm->l - NOTA_MARGEM * 2,
                             NOTA_TITULO_LINHAS, F_EDITORIAL, v->tl);
    return alto / gfx_altura_linha(F_EDITORIAL);
}
