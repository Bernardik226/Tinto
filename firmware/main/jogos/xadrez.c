#include "xadrez.h"
#include <string.h>

#define COR_BIT 8u
#define PECA(tipo, cor) ((uint8_t)((tipo) | ((cor) == XZ_PRETAS ? COR_BIT : 0u)))
#define MOV_DUPLO 1u
#define MOV_EN_PASSANT 2u
#define MOV_ROQUE 4u

static uint64_t hash_pos(const xadrez_pos_t *p);
static bool atacada(const xadrez_pos_t *p, int casa, xadrez_cor_t por);

static int coluna(int casa) { return casa & 7; }
static int linha(int casa) { return casa >> 3; }
static bool dentro(int c, int l) { return c >= 0 && c < 8 && l >= 0 && l < 8; }

xadrez_peca_t xadrez_tipo(uint8_t peca)
{
    return (xadrez_peca_t)(peca & 7u);
}

xadrez_cor_t xadrez_cor(uint8_t peca)
{
    return (peca & COR_BIT) ? XZ_PRETAS : XZ_BRANCAS;
}

uint8_t xadrez_peca_em(const xadrez_pos_t *p, int casa)
{
    return p && casa >= 0 && casa < 64 ? p->casa[casa] : XZ_NENHUMA;
}

void xadrez_nova(xadrez_pos_t *p)
{
    static const uint8_t primeira[8] = {
        XZ_TORRE, XZ_CAVALO, XZ_BISPO, XZ_DAMA,
        XZ_REI, XZ_BISPO, XZ_CAVALO, XZ_TORRE
    };
    if (!p) return;
    memset(p, 0, sizeof *p);
    for (int c = 0; c < 8; c++) {
        p->casa[c] = PECA(primeira[c], XZ_BRANCAS);
        p->casa[8 + c] = PECA(XZ_PEAO, XZ_BRANCAS);
        p->casa[48 + c] = PECA(XZ_PEAO, XZ_PRETAS);
        p->casa[56 + c] = PECA(primeira[c], XZ_PRETAS);
    }
    p->turno = XZ_BRANCAS;
    p->roques = 0x0f;
    p->en_passant = -1;
    p->numero_lance = 1;
    p->repeticao[0] = hash_pos(p);
    p->n_repeticao = 1;
}

static void inclui(xadrez_mov_t *out, int max, int *n,
                   int de, int para, int promocao, int flags)
{
    if (*n < max)
        out[*n] = (xadrez_mov_t){ (uint8_t)de, (uint8_t)para,
                                 (uint8_t)promocao, (uint8_t)flags };
    (*n)++;
}

static bool adversaria(uint8_t peca, xadrez_cor_t cor)
{
    return peca && xadrez_cor(peca) != cor;
}

static void gera_deslizante(const xadrez_pos_t *p, int de,
                            const int direcoes[][2], int nd,
                            xadrez_mov_t *out, int max, int *n)
{
    xadrez_cor_t cor = xadrez_cor(p->casa[de]);
    for (int d = 0; d < nd; d++) {
        int c = coluna(de) + direcoes[d][0];
        int l = linha(de) + direcoes[d][1];
        while (dentro(c, l)) {
            int para = l * 8 + c;
            uint8_t alvo = p->casa[para];
            if (!alvo) inclui(out, max, n, de, para, XZ_NENHUMA, 0);
            else {
                if (adversaria(alvo, cor))
                    inclui(out, max, n, de, para, XZ_NENHUMA, 0);
                break;
            }
            c += direcoes[d][0];
            l += direcoes[d][1];
        }
    }
}

static int pseudo(const xadrez_pos_t *p, int origem,
                  xadrez_mov_t *out, int max)
{
    static const int cavalo[8][2] = {
        {1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}
    };
    static const int diagonal[4][2] = {{1,1},{1,-1},{-1,-1},{-1,1}};
    static const int reta[4][2] = {{1,0},{0,-1},{-1,0},{0,1}};
    int n = 0;

    for (int de = 0; de < 64; de++) {
        uint8_t peca = p->casa[de];
        if (!peca || xadrez_cor(peca) != (xadrez_cor_t)p->turno ||
            (origem >= 0 && origem != de)) continue;

        int c0 = coluna(de), l0 = linha(de);
        switch (xadrez_tipo(peca)) {
        case XZ_PEAO: {
            int passo = p->turno == XZ_BRANCAS ? 1 : -1;
            int inicial = p->turno == XZ_BRANCAS ? 1 : 6;
            int l = l0 + passo;
            if (dentro(c0, l) && !p->casa[l * 8 + c0]) {
                int para = l * 8 + c0;
                if (l == 0 || l == 7) {
                    for (int pr = XZ_CAVALO; pr <= XZ_DAMA; pr++)
                        inclui(out, max, &n, de, para, pr, 0);
                } else {
                    inclui(out, max, &n, de, para, XZ_NENHUMA, 0);
                }
                int l2 = l0 + passo * 2;
                if (l0 == inicial && !p->casa[l2 * 8 + c0])
                    inclui(out, max, &n, de, l2 * 8 + c0,
                           XZ_NENHUMA, MOV_DUPLO);
            }
            for (int dc = -1; dc <= 1; dc += 2) {
                int c = c0 + dc;
                if (!dentro(c, l)) continue;
                int para = l * 8 + c;
                if (adversaria(p->casa[para], (xadrez_cor_t)p->turno)) {
                    if (l == 0 || l == 7) {
                        for (int pr = XZ_CAVALO; pr <= XZ_DAMA; pr++)
                            inclui(out, max, &n, de, para, pr, 0);
                    } else {
                        inclui(out, max, &n, de, para, XZ_NENHUMA, 0);
                    }
                } else if (para == p->en_passant) {
                    inclui(out, max, &n, de, para, XZ_NENHUMA,
                           MOV_EN_PASSANT);
                }
            }
            break;
        }
        case XZ_CAVALO:
            for (int i = 0; i < 8; i++) {
                int c = c0 + cavalo[i][0], l = l0 + cavalo[i][1];
                if (!dentro(c, l)) continue;
                int para = l * 8 + c;
                if (!p->casa[para] || adversaria(p->casa[para], xadrez_cor(peca)))
                    inclui(out, max, &n, de, para, XZ_NENHUMA, 0);
            }
            break;
        case XZ_BISPO:
            gera_deslizante(p, de, diagonal, 4, out, max, &n);
            break;
        case XZ_TORRE:
            gera_deslizante(p, de, reta, 4, out, max, &n);
            break;
        case XZ_DAMA:
            gera_deslizante(p, de, diagonal, 4, out, max, &n);
            gera_deslizante(p, de, reta, 4, out, max, &n);
            break;
        case XZ_REI:
            for (int dl = -1; dl <= 1; dl++)
                for (int dc = -1; dc <= 1; dc++) {
                    if ((!dc && !dl) || !dentro(c0 + dc, l0 + dl)) continue;
                    int para = (l0 + dl) * 8 + c0 + dc;
                    if (!p->casa[para] || adversaria(p->casa[para], xadrez_cor(peca)))
                        inclui(out, max, &n, de, para, XZ_NENHUMA, 0);
                }
            if (p->turno == XZ_BRANCAS && de == XZ_CASA('e',1) &&
                !atacada(p, de, XZ_PRETAS)) {
                if ((p->roques & 1u) && !p->casa[XZ_CASA('f',1)] &&
                    !p->casa[XZ_CASA('g',1)] &&
                    !atacada(p, XZ_CASA('f',1), XZ_PRETAS) &&
                    !atacada(p, XZ_CASA('g',1), XZ_PRETAS))
                    inclui(out, max, &n, de, XZ_CASA('g',1),
                           XZ_NENHUMA, MOV_ROQUE);
                if ((p->roques & 2u) && !p->casa[XZ_CASA('b',1)] &&
                    !p->casa[XZ_CASA('c',1)] && !p->casa[XZ_CASA('d',1)] &&
                    !atacada(p, XZ_CASA('d',1), XZ_PRETAS) &&
                    !atacada(p, XZ_CASA('c',1), XZ_PRETAS))
                    inclui(out, max, &n, de, XZ_CASA('c',1),
                           XZ_NENHUMA, MOV_ROQUE);
            }
            if (p->turno == XZ_PRETAS && de == XZ_CASA('e',8) &&
                !atacada(p, de, XZ_BRANCAS)) {
                if ((p->roques & 4u) && !p->casa[XZ_CASA('f',8)] &&
                    !p->casa[XZ_CASA('g',8)] &&
                    !atacada(p, XZ_CASA('f',8), XZ_BRANCAS) &&
                    !atacada(p, XZ_CASA('g',8), XZ_BRANCAS))
                    inclui(out, max, &n, de, XZ_CASA('g',8),
                           XZ_NENHUMA, MOV_ROQUE);
                if ((p->roques & 8u) && !p->casa[XZ_CASA('b',8)] &&
                    !p->casa[XZ_CASA('c',8)] && !p->casa[XZ_CASA('d',8)] &&
                    !atacada(p, XZ_CASA('d',8), XZ_BRANCAS) &&
                    !atacada(p, XZ_CASA('c',8), XZ_BRANCAS))
                    inclui(out, max, &n, de, XZ_CASA('c',8),
                           XZ_NENHUMA, MOV_ROQUE);
            }
            break;
        case XZ_NENHUMA:
            break;
        }
    }
    return n;
}

static void aplica(xadrez_pos_t *p, xadrez_mov_t m)
{
    uint8_t peca = p->casa[m.de];
    bool captura = p->casa[m.para] != 0 || (m.flags & MOV_EN_PASSANT);
    uint8_t roques_antes = p->roques;
    if (m.de == XZ_CASA('e',1)) p->roques &= (uint8_t)~3u;
    if (m.de == XZ_CASA('e',8)) p->roques &= (uint8_t)~12u;
    if (m.de == XZ_CASA('h',1) || m.para == XZ_CASA('h',1)) p->roques &= (uint8_t)~1u;
    if (m.de == XZ_CASA('a',1) || m.para == XZ_CASA('a',1)) p->roques &= (uint8_t)~2u;
    if (m.de == XZ_CASA('h',8) || m.para == XZ_CASA('h',8)) p->roques &= (uint8_t)~4u;
    if (m.de == XZ_CASA('a',8) || m.para == XZ_CASA('a',8)) p->roques &= (uint8_t)~8u;
    if (m.flags & MOV_EN_PASSANT)
        p->casa[m.para + (p->turno == XZ_BRANCAS ? -8 : 8)] = 0;
    p->casa[m.para] = peca;
    p->casa[m.de] = 0;
    if (m.promocao) p->casa[m.para] = PECA(m.promocao, p->turno);
    if (m.flags & MOV_ROQUE) {
        int torre_de = coluna(m.para) == 6 ? m.de + 3 : m.de - 4;
        int torre_para = coluna(m.para) == 6 ? m.de + 1 : m.de - 1;
        p->casa[torre_para] = p->casa[torre_de];
        p->casa[torre_de] = 0;
    }
    p->en_passant = m.flags & MOV_DUPLO ? (int8_t)((m.de + m.para) / 2) : -1;
    p->meio_lances = (xadrez_tipo(peca) == XZ_PEAO || captura)
                     ? 0 : (uint16_t)(p->meio_lances + 1);
    p->ultimo = m;
    if (p->turno == XZ_PRETAS) p->numero_lance++;
    p->turno ^= 1u;

    bool irreversivel = xadrez_tipo(peca) == XZ_PEAO || captura ||
                        p->roques != roques_antes;
    if (irreversivel) p->n_repeticao = 0;
    if (p->n_repeticao >= 101) {
        memmove(p->repeticao, p->repeticao + 1,
                100 * sizeof p->repeticao[0]);
        p->n_repeticao = 100;
    }
    p->repeticao[p->n_repeticao++] = hash_pos(p);
}

static bool atacada(const xadrez_pos_t *p, int casa, xadrez_cor_t por)
{
    int c0 = coluna(casa), l0 = linha(casa);
    int peao_l = l0 + (por == XZ_BRANCAS ? -1 : 1);
    for (int dc = -1; dc <= 1; dc += 2) {
        int c = c0 + dc;
        if (dentro(c, peao_l) &&
            p->casa[peao_l * 8 + c] == PECA(XZ_PEAO, por)) return true;
    }
    static const int cavalo[8][2] = {
        {1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}
    };
    for (int i = 0; i < 8; i++) {
        int c = c0 + cavalo[i][0], l = l0 + cavalo[i][1];
        if (dentro(c, l) && p->casa[l * 8 + c] == PECA(XZ_CAVALO, por))
            return true;
    }
    static const int dirs[8][2] = {
        {1,0},{0,1},{-1,0},{0,-1},{1,1},{1,-1},{-1,-1},{-1,1}
    };
    for (int d = 0; d < 8; d++) {
        int c = c0 + dirs[d][0], l = l0 + dirs[d][1], distancia = 1;
        while (dentro(c, l)) {
            uint8_t alvo = p->casa[l * 8 + c];
            if (alvo) {
                if (xadrez_cor(alvo) == por) {
                    xadrez_peca_t tipo = xadrez_tipo(alvo);
                    bool reto = d < 4;
                    if (tipo == XZ_DAMA || (reto && tipo == XZ_TORRE) ||
                        (!reto && tipo == XZ_BISPO) ||
                        (distancia == 1 && tipo == XZ_REI)) return true;
                }
                break;
            }
            c += dirs[d][0];
            l += dirs[d][1];
            distancia++;
        }
    }
    return false;
}

bool xadrez_em_xeque(const xadrez_pos_t *p, xadrez_cor_t cor)
{
    if (!p) return false;
    int rei = -1;
    for (int i = 0; i < 64; i++)
        if (p->casa[i] == PECA(XZ_REI, cor)) { rei = i; break; }
    return rei >= 0 && atacada(p, rei, (xadrez_cor_t)(cor ^ 1));
}

int xadrez_movimentos(const xadrez_pos_t *p, int origem,
                      xadrez_mov_t *out, int max)
{
    if (!p || !out || max <= 0) return 0;
    xadrez_mov_t candidatos[XADREZ_MOV_MAX];
    int todos = pseudo(p, origem, candidatos, XADREZ_MOV_MAX);
    int n = 0;
    for (int i = 0; i < todos && i < XADREZ_MOV_MAX; i++) {
        xadrez_pos_t depois = *p;
        aplica(&depois, candidatos[i]);
        if (!xadrez_em_xeque(&depois, (xadrez_cor_t)p->turno) && n < max)
            out[n++] = candidatos[i];
    }
    return n;
}

bool xadrez_joga(xadrez_pos_t *p, xadrez_mov_t m)
{
    if (!p) return false;
    xadrez_mov_t legais[XADREZ_MOV_MAX];
    int n = xadrez_movimentos(p, m.de, legais, XADREZ_MOV_MAX);
    for (int i = 0; i < n; i++) {
        if (legais[i].de == m.de && legais[i].para == m.para &&
            legais[i].promocao == m.promocao) {
            aplica(p, legais[i]);
            return true;
        }
    }
    return false;
}

static uint64_t hash_pos(const xadrez_pos_t *p)
{
    uint64_t h = UINT64_C(1469598103934665603);
    for (int i = 0; i < 64; i++) { h ^= p->casa[i]; h *= UINT64_C(1099511628211); }
    h ^= p->turno; h *= UINT64_C(1099511628211);
    h ^= p->roques; h *= UINT64_C(1099511628211);
    h ^= (uint8_t)(p->en_passant + 1); h *= UINT64_C(1099511628211);
    return h;
}

static bool material_insuficiente(const xadrez_pos_t *p)
{
    int menores = 0, bispos = 0, cavalos = 0, cor_bispo = -1;
    for (int i = 0; i < 64; i++) {
        switch (xadrez_tipo(p->casa[i])) {
        case XZ_NENHUMA:
        case XZ_REI:
            break;
        case XZ_BISPO:
            menores++; bispos++;
            if (cor_bispo < 0) cor_bispo = (coluna(i) + linha(i)) & 1;
            else if (cor_bispo != ((coluna(i) + linha(i)) & 1)) return false;
            break;
        case XZ_CAVALO:
            menores++; cavalos++;
            break;
        default:
            return false;
        }
    }
    if (menores <= 1) return true;
    if (menores == 2 && cavalos == 2) return true;
    return cavalos == 0 && bispos == menores;
}

xadrez_estado_t xadrez_estado(const xadrez_pos_t *p)
{
    if (!p) return XZ_EM_CURSO;
    xadrez_mov_t m[XADREZ_MOV_MAX];
    int n = xadrez_movimentos(p, -1, m, XADREZ_MOV_MAX);
    if (n == 0) {
        if (!xadrez_em_xeque(p, (xadrez_cor_t)p->turno))
            return XZ_EMPATE_AFOGAMENTO;
        return p->turno == XZ_BRANCAS ? XZ_MATE_PRETAS : XZ_MATE_BRANCAS;
    }
    if (p->meio_lances >= 100) return XZ_EMPATE_50_LANCES;
    uint64_t atual = hash_pos(p);
    int iguais = 0;
    for (int i = 0; i < p->n_repeticao; i++)
        if (p->repeticao[i] == atual) iguais++;
    if (iguais >= 3) return XZ_EMPATE_REPETICAO;
    if (material_insuficiente(p)) return XZ_EMPATE_MATERIAL;
    return XZ_EM_CURSO;
}

static int navega(const bool candidato[64], int atual, int dx, int dy)
{
    if (atual < 0 || atual >= 64 || (!dx && !dy)) return atual;
    int melhor = atual, melhor_lado = 99, melhor_frente = 99;
    int ca = coluna(atual), la = linha(atual);
    for (int i = 0; i < 64; i++) {
        if (!candidato[i] || i == atual) continue;
        int dc = coluna(i) - ca, dl = linha(i) - la;
        int frente = dc * dx + dl * dy;
        if (frente <= 0) continue;
        int lado = dc * dy - dl * dx;
        if (lado < 0) lado = -lado;
        if (lado < melhor_lado ||
            (lado == melhor_lado && frente < melhor_frente) ||
            (lado == melhor_lado && frente == melhor_frente && i < melhor)) {
            melhor = i;
            melhor_lado = lado;
            melhor_frente = frente;
        }
    }
    return melhor;
}

int xadrez_navega_peca(const xadrez_pos_t *p, int atual, int dx, int dy)
{
    if (!p) return atual;
    bool candidato[64] = {0};
    for (int i = 0; i < 64; i++)
        candidato[i] = p->casa[i] &&
                       xadrez_cor(p->casa[i]) == (xadrez_cor_t)p->turno;
    return navega(candidato, atual, dx, dy);
}

int xadrez_navega_destino(const xadrez_pos_t *p, int origem, int atual,
                          int dx, int dy)
{
    if (!p) return atual;
    bool candidato[64] = {0};
    xadrez_mov_t m[XADREZ_MOV_MAX];
    int n = xadrez_movimentos(p, origem, m, XADREZ_MOV_MAX);
    for (int i = 0; i < n; i++) candidato[m[i].para] = true;
    return navega(candidato, atual, dx, dy);
}
