#include "texto.h"
#include <stdio.h>
#include <string.h>

static int faltantes;

int  gfx_faltantes(void)      { return faltantes; }
void gfx_zera_faltantes(void) { faltantes = 0; }

// ── UTF-8 ────────────────────────────────────────────────────────────
// Um decodificador curto evita tratar byte como caractere ("ã" viraria dois
// tofus).
static const char *proximo_codigo(const char *s, uint32_t *out)
{
    uint8_t c = (uint8_t)*s;
    if (c < 0x80)        { *out = c;                     return s + 1; }
    if ((c & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        *out = (uint32_t)(c & 0x1F) << 6 | (s[1] & 0x3F);
        return s + 2;
    }
    if ((c & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        *out = (uint32_t)(c & 0x0F) << 12 | (uint32_t)(s[1] & 0x3F) << 6
             | (s[2] & 0x3F);
        return s + 3;
    }
    if ((c & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80
                           && (s[3] & 0xC0) == 0x80) {
        *out = (uint32_t)(c & 0x07) << 18 | (uint32_t)(s[1] & 0x3F) << 12
             | (uint32_t)(s[2] & 0x3F) << 6 | (s[3] & 0x3F);
        return s + 4;
    }
    *out = 0xFFFD;    // byte solto: um tofu, e segue
    return s + 1;
}

const glifo_t *fonte_glifo(fonte_t f, uint32_t codigo)
{
    if (f >= F_QUANTAS) return NULL;
    const fonte_dados_t *fd = &FONTES[f];

    int lo = 0, hi = fd->n - 1;          // ordenados pelo gerador
    while (lo <= hi) {
        int meio = (lo + hi) / 2;
        uint32_t c = fd->glifos[meio].codigo;
        if (c == codigo) return &fd->glifos[meio];
        if (c < codigo) lo = meio + 1; else hi = meio - 1;
    }
    return NULL;
}

// ── desenhar ────────────────────────────────────────────────────────
static void tofu(bitmap_t *bm, int x, int y, fonte_t f)
{
    int a = FONTES[f].altura_x;
    gfx_ret(bm, x + 1, y - a, (a * 2) / 3, a, false);
}

int gfx_texto(bitmap_t *bm, int x, int y, fonte_t f, const char *utf8)
{
    if (!utf8 || f >= F_QUANTAS) return x;
    const fonte_dados_t *fd = &FONTES[f];
    int base = y + fd->ascent;           // y é o TOPO da linha

    uint32_t cod;
    for (const char *p = utf8; *p; ) {
        p = proximo_codigo(p, &cod);
        const glifo_t *g = fonte_glifo(f, cod);

        if (!g) {
            faltantes++;
            if (bm) tofu(bm, x, base, f);
            x += fd->altura_x;           // avanço plausível, não zero
            continue;
        }

        if (bm && g->l && g->a) {
            int passo = (g->l + 7) / 8;
            const uint8_t *bits = &fd->bits[g->offset];
            for (int j = 0; j < g->a; j++)
                for (int i = 0; i < g->l; i++)
                    if (bits[j * passo + i / 8] & (0x80 >> (i % 8)))
                        gfx_pixel(bm, x + g->esq + i, base - g->topo + j, true);
        }
        // Anda o AVANÇO, não a largura da tinta. Somar inteiros acumula ±1,5 px em
        // 290 px de texto real: irrelevante em 1 bit.
        x += g->avanco;
    }
    return x;
}

int gfx_largura(fonte_t f, const char *utf8)
{
    return gfx_texto(NULL, 0, 0, f, utf8);
}

int gfx_altura_linha(fonte_t f)
{
    return f < F_QUANTAS ? FONTES[f].altura_linha : 0;
}

int gfx_ascent(fonte_t f)
{
    return f < F_QUANTAS ? FONTES[f].ascent : 0;
}

int gfx_cabe(fonte_t f, const char *utf8, int largura)
{
    if (!utf8) return 0;
    int x = 0, bytes = 0;
    uint32_t cod;
    for (const char *p = utf8; *p; ) {
        const char *ini = p;
        p = proximo_codigo(p, &cod);
        const glifo_t *g = fonte_glifo(f, cod);
        int av = g ? g->avanco : FONTES[f].altura_x;
        if (x + av > largura) break;
        x += av;
        bytes += (int)(p - ini);
    }
    return bytes;
}

// ── uma linha, no máximo esta largura, com reticência ───────────────
// Uma função só, recuando por CARACTERE: cortar um byte no meio de "ã" desenha
// caixa vazada e passa nos testes de vista.
int gfx_texto_ate(bitmap_t *bm, int x, int y, fonte_t f, const char *texto,
                  int largura)
{
    if (!texto || !*texto) return x;
    if (largura <= 0) return x;

    if (gfx_largura(f, texto) <= largura) return gfx_texto(bm, x, y, f, texto);

    // O espaço da reticência é RESERVADO antes do corte.
    int reserva = largura - gfx_largura(f, "…");
    if (reserva < 0) reserva = 0;

    char corte[128];
    int n = gfx_cabe(f, texto, reserva);
    if (n > (int)sizeof corte - 4) n = (int)sizeof corte - 4;
    while (n > 0 && ((unsigned char)texto[n] & 0xC0) == 0x80) n--;
    if (n < 0) n = 0;

    memcpy(corte, texto, (size_t)n);
    corte[n] = '\0';
    snprintf(corte + n, sizeof corte - (size_t)n, "…");
    return gfx_texto(bm, x, y, f, corte);
}

static int paragrafo(bitmap_t *bm, int x, int y, int largura, int max_linhas,
                     fonte_t f, const char *utf8, bool centrado)
{
    if (!utf8) return y;
    int alt = gfx_altura_linha(f);
    int linhas = 0;

    const char *p = utf8;
    while (*p && linhas < max_linhas) {
        // ── a quebra EXPLÍCITA vem antes de tudo ────────────────────────────
        // Sem isto o `\n` vira tofu (nenhuma fonte tem o glifo). Aqui, para nenhuma
        // tela reimplementar a quebra.
        int forcado = -1;
        for (int i = 0; p[i]; i++)
            if (p[i] == '\n') { forcado = i; break; }

        // Quantos bytes cabem, e onde foi a última palavra inteira.
        int cabem = gfx_cabe(f, p, largura);
        int corte = cabem;

        if (p[cabem] != '\0') {          // vai sobrar texto: quebra na palavra
            int ultimo_espaco = -1;
            for (int i = 0; i < cabem; i++)
                if (p[i] == ' ' || p[i] == '\n') ultimo_espaco = i;
            if (ultimo_espaco > 0) corte = ultimo_espaco;
        }
        if (corte <= 0) corte = cabem > 0 ? cabem : 1;

        // O `\n` que cabe nesta linha ganha da quebra automática.
        if (forcado >= 0 && forcado <= corte) corte = forcado;

        // ÚLTIMA linha permitida e ainda sobra texto: termina em reticência.
        // Parar calado transformava "Reunião de alinhamento" em outro título.
        bool corta = linhas == max_linhas - 1 && p[corte] != '\0';
        if (corta) {
            int reserva = largura - gfx_largura(f, "…");
            if (reserva < 0) reserva = 0;
            corte = gfx_cabe(f, p, reserva);
            if (corte < 1) corte = 1;
        }

        char linha[128];
        int n = corte < (int)sizeof linha - 4 ? corte : (int)sizeof linha - 4;
        memcpy(linha, p, (size_t)n);
        linha[n] = '\0';
        if (corta) snprintf(linha + n, sizeof linha - (size_t)n, "…");

        // Centrar por LINHA, não pelo bloco.
        int lx = x;
        if (centrado) {
            int w = gfx_largura(f, linha);
            if (w < largura) lx = x + (largura - w) / 2;
        }
        if (bm) gfx_texto(bm, lx, y, f, linha);
        y += alt;
        linhas++;

        p += corte;
        // O `\n` é consumido com os espaços.
        while (*p == ' ' || *p == '\n') p++;
    }
    return y;
}

int gfx_paragrafo(bitmap_t *bm, int x, int y, int largura, int max_linhas,
                  fonte_t f, const char *utf8)
{
    return paragrafo(bm, x, y, largura, max_linhas, f, utf8, false);
}

int gfx_paragrafo_centro(bitmap_t *bm, int x, int y, int largura,
                         int max_linhas, fonte_t f, const char *utf8)
{
    return paragrafo(bm, x, y, largura, max_linhas, f, utf8, true);
}

static void linha_alinhada(bitmap_t *bm, int x, int y, int largura,
                           fonte_t f, char *texto,
                           texto_alinhamento_t alinhamento,
                           bool pode_justificar)
{
    int w = gfx_largura(f, texto);
    if (alinhamento == TEXTO_CENTRO) x += (largura - w) / 2;
    else if (alinhamento == TEXTO_DIREITA) x += largura - w;

    if (alinhamento != TEXTO_JUSTIFICADO || !pode_justificar || !bm) {
        if (bm) gfx_texto(bm, x, y, f, texto);
        return;
    }

    int espacos = 0;
    for (const char *p = texto; *p; p++) if (*p == ' ') espacos++;
    int sobra = largura - w;
    if (espacos <= 0 || sobra <= 0) {
        gfx_texto(bm, x, y, f, texto);
        return;
    }

    const int espaco_normal = gfx_largura(f, " ");
    char *p = texto;
    int qual = 0;
    while (*p) {
        char *esp = strchr(p, ' ');
        if (!esp) { gfx_texto(bm, x, y, f, p); break; }
        *esp = '\0';
        x = gfx_texto(bm, x, y, f, p);
        int extra = sobra / espacos + (qual < sobra % espacos ? 1 : 0);
        x += espaco_normal + extra;
        p = esp + 1;
        qual++;
    }
}

int gfx_pagina_alinhada(bitmap_t *bm, int x, int y, int largura,
                        int max_linhas, fonte_t f, const char *utf8,
                        int tamanho, texto_alinhamento_t alinhamento)
{
    if (!utf8 || tamanho <= 0 || max_linhas <= 0) return 0;
    const char *p = utf8;
    const char *fim = utf8 + tamanho;
    int alt = gfx_altura_linha(f);

    for (int linha_n = 0; linha_n < max_linhas && p < fim && *p; linha_n++) {
        int restante = (int)(fim - p);
        int cabem = gfx_cabe(f, p, largura);
        if (cabem > restante) cabem = restante;
        if (cabem <= 0) cabem = 1;

        int quebra = cabem;
        int nl = -1, espaco = -1;
        for (int i = 0; i < cabem && i < restante; i++) {
            if (p[i] == '\n') { nl = i; break; }
            if (p[i] == ' ') espaco = i;
        }
        if (nl >= 0) quebra = nl;
        else if (cabem < restante && p[cabem] && espaco > 0) quebra = espaco;
        if (quebra <= 0 && nl < 0) quebra = cabem;

        if (quebra > 0) {
            char texto[160];
            int n = quebra < (int)sizeof texto - 1 ? quebra : (int)sizeof texto - 1;
            memcpy(texto, p, (size_t)n);
            texto[n] = '\0';
            // Justifica só linha quebrada automaticamente; a última fica natural.
            bool automatica = nl < 0 && cabem < restante && p[cabem];
            linha_alinhada(bm, x, y, largura, f, texto, alinhamento,
                           automatica);
        }
        y += alt;
        p += quebra;
        if (p < fim && *p == '\n') p++;
        else while (p < fim && *p == ' ') p++;
    }
    return (int)(p - utf8);
}

int gfx_pagina(bitmap_t *bm, int x, int y, int largura, int max_linhas,
               fonte_t f, const char *utf8, int tamanho)
{
    return gfx_pagina_alinhada(bm, x, y, largura, max_linhas, f, utf8,
                               tamanho, TEXTO_ESQUERDA);
}
