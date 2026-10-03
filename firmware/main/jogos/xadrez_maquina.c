#include "xadrez_maquina.h"
#include <limits.h>
#include <string.h>

static int avalia(const xadrez_pos_t *p, xadrez_cor_t cor)
{
    static const int valor[] = {0, 100, 320, 330, 500, 900, 20000};
    int total = 0;
    for (int i = 0; i < 64; i++) {
        uint8_t peca = p->casa[i];
        if (!peca) continue;
        int v = valor[xadrez_tipo(peca)];
        total += xadrez_cor(peca) == cor ? v : -v;
    }
    return total;
}

static int distancia_centro(int casa)
{
    int c = casa & 7, l = casa >> 3;
    int dc = c * 2 - 7, dl = l * 2 - 7;
    return (dc < 0 ? -dc : dc) + (dl < 0 ? -dl : dl);
}

static int avalia_facil(const xadrez_maquina_t *m, xadrez_mov_t lance)
{
    static const int valor[] = {0, 100, 320, 330, 500, 900, 20000};
    uint8_t peca = m->raiz.casa[lance.de];
    uint8_t alvo = m->raiz.casa[lance.para];
    int pontos = valor[xadrez_tipo(alvo)] * 10;
    pontos += distancia_centro(lance.de) - distancia_centro(lance.para);
    if (xadrez_tipo(peca) == XZ_PEAO || xadrez_tipo(peca) == XZ_CAVALO ||
        xadrez_tipo(peca) == XZ_BISPO) pontos += 8;
    if (m->raiz.numero_lance <= 6 &&
        (xadrez_tipo(peca) == XZ_TORRE || xadrez_tipo(peca) == XZ_DAMA))
        pontos -= 24;
    if (lance.de == m->raiz.ultimo.para && lance.para == m->raiz.ultimo.de)
        pontos -= 10000;
    return pontos;
}

static void considera(xadrez_maquina_t *m, int valor)
{
    if (m->candidato == 0 || valor > m->melhor_valor) {
        m->melhor_valor = valor;
        m->melhor = m->candidatos[m->candidato];
    }
}

static void termina(xadrez_maquina_t *m, xadrez_mov_t *out)
{
    if (m->candidato == 0 && m->n_candidatos)
        m->melhor = m->candidatos[0];
    m->estado = m->n_candidatos ? XZM_PRONTO : XZM_SEM_LANCE;
    if (out && m->estado == XZM_PRONTO) *out = m->melhor;
}

void xadrez_maquina_inicia(xadrez_maquina_t *m, const xadrez_pos_t *p,
                           xadrez_dificuldade_t dificuldade)
{
    if (!m) return;
    memset(m, 0, sizeof *m);
    if (!p) { m->estado = XZM_SEM_LANCE; return; }
    m->raiz = *p;
    m->cor = p->turno;
    m->dificuldade = dificuldade > XZM_DIFICIL ? XZM_DIFICIL : dificuldade;
    m->n_candidatos = (uint16_t)xadrez_movimentos(
        p, -1, m->candidatos, XADREZ_MOV_MAX);
    m->teto = m->dificuldade == XZM_FACIL ? m->n_candidatos
            : m->dificuldade == XZM_MEDIA ? m->n_candidatos
            : 4096u;
    m->melhor_valor = INT_MIN;
    m->estado = m->n_candidatos ? XZM_BUSCANDO : XZM_SEM_LANCE;
}

xadrez_maquina_estado_t xadrez_maquina_passo(xadrez_maquina_t *m,
                                              uint16_t fatia_nos,
                                              xadrez_mov_t *out)
{
    if (!m) return XZM_SEM_LANCE;
    if (m->estado != XZM_BUSCANDO) {
        if (out && m->estado == XZM_PRONTO) *out = m->melhor;
        return m->estado;
    }
    if (!fatia_nos) return m->estado;

    uint16_t usados = 0;
    while (usados < fatia_nos && m->candidato < m->n_candidatos &&
           m->nos < m->teto) {
        if (m->dificuldade == XZM_FACIL) {
            considera(m, avalia_facil(m, m->candidatos[m->candidato]));
            m->candidato++;
            m->nos++;
            usados++;
            continue;
        }

        if (m->dificuldade == XZM_MEDIA) {
            xadrez_pos_t depois = m->raiz;
            (void)xadrez_joga(&depois, m->candidatos[m->candidato]);
            considera(m, avalia(&depois, (xadrez_cor_t)m->cor));
            m->candidato++;
            m->nos++;
            usados++;
            continue;
        }

        if (!m->raiz_aberta) {
            m->apos_raiz = m->raiz;
            (void)xadrez_joga(&m->apos_raiz, m->candidatos[m->candidato]);
            m->n_respostas = (uint16_t)xadrez_movimentos(
                &m->apos_raiz, -1, m->respostas, XADREZ_MOV_MAX);
            m->resposta = 0;
            m->pior_resposta = INT_MAX;
            m->raiz_aberta = true;
            m->nos++;
            usados++;
            if (!m->n_respostas) {
                int valor = xadrez_em_xeque(
                    &m->apos_raiz, (xadrez_cor_t)m->apos_raiz.turno)
                    ? 1000000 : 0;
                considera(m, valor);
                m->candidato++;
                m->raiz_aberta = false;
            }
            continue;
        }

        xadrez_pos_t depois = m->apos_raiz;
        (void)xadrez_joga(&depois, m->respostas[m->resposta++]);
        int valor = avalia(&depois, (xadrez_cor_t)m->cor);
        if (valor < m->pior_resposta) m->pior_resposta = valor;
        m->nos++;
        usados++;
        if (m->resposta == m->n_respostas) {
            considera(m, m->pior_resposta);
            m->candidato++;
            m->raiz_aberta = false;
        }
    }

    if (m->candidato == m->n_candidatos || m->nos == m->teto)
        termina(m, out);
    return m->estado;
}
