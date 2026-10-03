#include "xadrez.h"
#include "json.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARQUIVO "/TINTO/jogos/xadrez.json"
#define TMP     "/TINTO/jogos/xadrez.json.tmp"
#define BUFFER  (64u * 1024u)

static uint32_t checksum(const char *s)
{
    uint32_t h = 2166136261u;
    while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
    return h;
}

static char promocao_char(uint8_t p)
{
    static const char letras[] = "--nbrq";
    return p <= XZ_DAMA ? letras[p] : '-';
}

static xadrez_peca_t promocao_peca(char c)
{
    if (c == 'n') return XZ_CAVALO;
    if (c == 'b') return XZ_BISPO;
    if (c == 'r') return XZ_TORRE;
    if (c == 'q') return XZ_DAMA;
    return XZ_NENHUMA;
}

static bool decodifica(const char *s, xadrez_mov_t *m)
{
    if (!s || strlen(s) < 4 || s[0] < 'a' || s[0] > 'h' ||
        s[2] < 'a' || s[2] > 'h' || s[1] < '1' || s[1] > '8' ||
        s[3] < '1' || s[3] > '8') return false;
    *m = (xadrez_mov_t){
        (uint8_t)XZ_CASA(s[0], s[1] - '0'),
        (uint8_t)XZ_CASA(s[2], s[3] - '0'),
        (uint8_t)promocao_peca(s[4]), 0
    };
    return s[4] == '\0' || m->promocao != XZ_NENHUMA;
}

static erro_t le(const hal_t *hal, xadrez_salvo_t *meta,
                 char *lances, size_t lances_max)
{
    size_t real = 0;
    char *json = hal->emprestar(BUFFER, 2048, &real);
    if (!json) return ERR_CHEIO;
    erro_t e = hal->ler(ARQUIVO, json, real);
    if (e == ERR_SEM_PASTA) e = ERR_ARQUIVO;
    if (e != OK) { hal->devolver(json); return e; }

    char *marca = strstr(json, ",\"checksum\":\"");
    if (!marca || strlen(marca) != 23 || marca[22] != '}') {
        hal->devolver(json); return ERR_FORMATO;
    }
    char hex[9];
    memcpy(hex, marca + 13, 8); hex[8] = '\0';
    *marca = '\0';
    char *fim = NULL;
    unsigned long guardado = strtoul(hex, &fim, 16);
    if (!fim || *fim || guardado != checksum(json)) {
        hal->devolver(json); return ERR_FORMATO;
    }

    int v, modo, cor, baixo, dificuldade, orientacao, cursor, origem;
    int resultado = 0, ajuda = 0, placar_a2 = 0, placar_b2 = 0;
    bool certo = json_int(json, "v", &v) && (v >= 1 && v <= 5) &&
        json_int(json, "modo", &modo) && json_int(json, "cor", &cor) &&
        json_int(json, "dificuldade", &dificuldade) &&
        json_int(json, "orientacao", &orientacao) &&
        json_int(json, "cursor", &cursor) && json_int(json, "origem", &origem) &&
        json_str(json, "lances", lances, lances_max);
    baixo = cor;
    if (certo && v >= 2) certo = json_int(json, "baixo", &baixo);
    if (certo && v >= 3) certo = json_int(json, "resultado", &resultado);
    if (certo && v >= 4) certo = json_int(json, "ajuda", &ajuda);
    if (certo && v >= 5)
        certo = json_int(json, "placar_a2", &placar_a2) &&
                json_int(json, "placar_b2", &placar_b2);
    if (!certo || modo < 0 || modo > 1 || cor < 0 || cor > 1 ||
        baixo < 0 || baixo > 1 ||
        dificuldade < 0 || dificuldade > 2 || orientacao < 0 ||
        orientacao > 2 || cursor < 0 || cursor > 63 || origem < -1 || origem > 63 ||
        resultado < XZ_EM_CURSO || resultado > XZ_EMPATE_ACORDO ||
        ajuda < 0 || ajuda > 1 || placar_a2 < 0 || placar_a2 > UINT16_MAX ||
        placar_b2 < 0 || placar_b2 > UINT16_MAX) {
        hal->devolver(json); return ERR_FORMATO;
    }
    memset(meta, 0, sizeof *meta);
    meta->modo = (uint8_t)modo; meta->cor_humana = (uint8_t)cor;
    meta->cor_baixo = (uint8_t)baixo;
    meta->dificuldade = (uint8_t)dificuldade;
    meta->orientacao = (uint8_t)orientacao; meta->cursor = (uint8_t)cursor;
    meta->origem = (int8_t)origem;
    meta->resultado = (xadrez_estado_t)resultado;
    meta->mostrar_ajuda = ajuda != 0;
    meta->placar_a2 = (uint16_t)placar_a2;
    meta->placar_b2 = (uint16_t)placar_b2;
    hal->devolver(json);
    return OK;
}

erro_t xadrez_carrega(const hal_t *hal, xadrez_salvo_t *out)
{
    if (!hal || !out) return ERR_INTERNO;
    size_t real = 0;
    char *lances = hal->emprestar(BUFFER, 2048, &real);
    if (!lances) return ERR_CHEIO;
    erro_t e = le(hal, out, lances, real);
    if (e != OK) { memset(out, 0, sizeof *out); hal->devolver(lances); return e; }

    xadrez_nova(&out->posicao);
    char *p = lances;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        char *fim = strchr(p, ' ');
        if (fim) *fim = '\0';
        xadrez_mov_t m;
        if (!decodifica(p, &m) || !xadrez_joga(&out->posicao, m)) {
            hal->devolver(lances); memset(out, 0, sizeof *out); return ERR_FORMATO;
        }
        out->n_lances++;
        if (!fim) break;
        p = fim + 1;
    }
    hal->devolver(lances);
    return OK;
}

static erro_t grava(const hal_t *hal, const xadrez_salvo_t *jogo,
                    const char *lances, char *json, size_t jm)
{
    int n = snprintf(json, jm,
        "{\"v\":5,\"modo\":%u,\"cor\":%u,\"baixo\":%u,\"dificuldade\":%u,"
        "\"orientacao\":%u,\"cursor\":%u,\"origem\":%d,\"resultado\":%u,"
        "\"ajuda\":%u,\"placar_a2\":%u,\"placar_b2\":%u,\"lances\":\"%s\"",
        jogo->modo, jogo->cor_humana, jogo->cor_baixo, jogo->dificuldade,
        jogo->orientacao, jogo->cursor, jogo->origem, jogo->resultado,
        jogo->mostrar_ajuda, jogo->placar_a2, jogo->placar_b2, lances);
    if (n < 0 || (size_t)n + 24 >= jm) return ERR_CHEIO;
    uint32_t soma = checksum(json);
    snprintf(json + n, jm - (size_t)n, ",\"checksum\":\"%08" PRIx32 "\"}", soma);

    erro_t e = hal->criar_diretorio("/TINTO/jogos");
    if (e == OK) e = hal->escrever(TMP, json);
    if (e == OK) e = hal->renomear(TMP, ARQUIVO);
    if (e == OK) {
        xadrez_salvo_t conferido;
        e = xadrez_carrega(hal, &conferido);
        if (e == OK && memcmp(conferido.posicao.casa, jogo->posicao.casa, 64) != 0)
            e = ERR_FORMATO;
    }
    return e;
}

erro_t xadrez_salva_lance(const hal_t *hal, const xadrez_salvo_t *jogo,
                          xadrez_mov_t lance)
{
    if (!hal || !jogo) return ERR_INTERNO;
    size_t lm = 0, jm = 0;
    char *lances = hal->emprestar(BUFFER, 2048, &lm);
    char *json = hal->emprestar(BUFFER, 2048, &jm);
    if (!lances || !json) {
        if (lances) hal->devolver(lances);
        if (json) hal->devolver(json);
        return ERR_CHEIO;
    }
    lances[0] = '\0';
    xadrez_salvo_t anterior;
    erro_t e = le(hal, &anterior, lances, lm);
    if (e == ERR_ARQUIVO) e = OK;
    if (e != OK) goto fim;

    char mov[6] = {
        (char)('a' + (lance.de & 7)), (char)('1' + (lance.de >> 3)),
        (char)('a' + (lance.para & 7)), (char)('1' + (lance.para >> 3)),
        lance.promocao ? promocao_char(lance.promocao) : '\0', '\0'
    };
    size_t usado = strlen(lances), precisa = usado + (usado ? 1 : 0) + strlen(mov) + 1;
    if (precisa >= lm) { e = ERR_CHEIO; goto fim; }
    if (usado) strcat(lances, " ");
    strcat(lances, mov);

    e = grava(hal, jogo, lances, json, jm);
fim:
    hal->devolver(json); hal->devolver(lances);
    return e;
}

erro_t xadrez_salva_opcoes(const hal_t *hal, const xadrez_salvo_t *jogo)
{
    if (!hal || !jogo) return ERR_INTERNO;
    size_t lm = 0, jm = 0;
    char *lances = hal->emprestar(BUFFER, 2048, &lm);
    char *json = hal->emprestar(BUFFER, 2048, &jm);
    if (!lances || !json) {
        if (lances) hal->devolver(lances);
        if (json) hal->devolver(json);
        return ERR_CHEIO;
    }
    xadrez_salvo_t anterior;
    erro_t e = le(hal, &anterior, lances, lm);
    if (e == ERR_ARQUIVO) { lances[0] = '\0'; e = OK; }
    if (e == OK) e = grava(hal, jogo, lances, json, jm);
    hal->devolver(json); hal->devolver(lances);
    return e;
}

erro_t xadrez_historico(const hal_t *hal, uint16_t deslocamento,
                        xadrez_hist_item_t *out, int max, uint16_t *total)
{
    if (!hal || !out || max <= 0 || !total) return ERR_INTERNO;
    memset(out, 0, (size_t)max * sizeof *out);
    *total = 0;
    size_t lm = 0;
    char *lances = hal->emprestar(BUFFER, 2048, &lm);
    if (!lances) return ERR_CHEIO;
    xadrez_salvo_t meta;
    erro_t e = le(hal, &meta, lances, lm);
    if (e == ERR_ARQUIVO) { hal->devolver(lances); return OK; }
    if (e != OK) { hal->devolver(lances); return e; }

    for (char *p = lances; *p;) {
        while (*p == ' ') p++;
        if (!*p) break;
        (*total)++;
        p = strchr(p, ' ');
        if (!p) break;
    }

    xadrez_pos_t pos; xadrez_nova(&pos);
    char *p = lances;
    uint16_t indice = 0;
    while (*p) {
        while (*p == ' ') p++;
        if (!*p) break;
        char *fim = strchr(p, ' ');
        if (fim) *fim = '\0';
        xadrez_mov_t m;
        if (!decodifica(p, &m)) { e = ERR_FORMATO; break; }
        uint16_t recente = (uint16_t)(*total - 1 - indice);
        xadrez_hist_item_t item = {
            .lance = m, .numero = pos.numero_lance,
            .peca = (uint8_t)xadrez_tipo(pos.casa[m.de]),
            .capturada = (uint8_t)xadrez_tipo(pos.casa[m.para]),
            .cor = pos.turno,
        };
        int dc = (m.para & 7) - (m.de & 7);
        if (dc < 0) dc = -dc;
        if (item.capturada) item.marcas |= XZ_HIST_CAPTURA;
        if (item.peca == XZ_REI && dc == 2) item.marcas |= XZ_HIST_ROQUE;
        if (item.peca == XZ_PEAO && dc == 1 && !item.capturada) {
            item.marcas |= XZ_HIST_CAPTURA | XZ_HIST_EN_PASSANT;
            item.capturada = XZ_PEAO;
        }
        if (m.promocao) item.marcas |= XZ_HIST_PROMOCAO;
        if (!xadrez_joga(&pos, m)) { e = ERR_FORMATO; break; }
        xadrez_estado_t estado = xadrez_estado(&pos);
        if (xadrez_em_xeque(&pos, (xadrez_cor_t)pos.turno)) item.marcas |= XZ_HIST_XEQUE;
        if (estado == XZ_MATE_BRANCAS || estado == XZ_MATE_PRETAS)
            item.marcas |= XZ_HIST_MATE;
        if (recente >= deslocamento && recente < deslocamento + max)
            out[recente - deslocamento] = item;
        indice++;
        if (!fim) break;
        p = fim + 1;
    }
    hal->devolver(lances);
    return e;
}

erro_t xadrez_apaga(const hal_t *hal)
{
    return hal ? hal->apagar(ARQUIVO) : ERR_INTERNO;
}
