#include "acervo.h"
#include "json.h"
#include "cartao.h"
#include "../nucleo/data.h"
#include <stdio.h>
#include <string.h>

#define ACERVO_RAIZ CARTAO_RAIZ "/acervo"

// ── o id não vira caminho ────────────────────────────────────────────
// Todo id tem `:`, que o FAT recusa: vira `%` (como em `cartao.c`), e a troca
// é reversível porque o nome da pasta É o id. Um id com `/` ou `..` não passa
// da conferência.
static bool nome_do_id(const char *id, char *out, size_t max)
{
    if (!id || !*id) return false;

    size_t i = 0;
    for (; id[i] && i < max - 1; i++) {
        char c = id[i];
        if (c == '/' || c == '\\' || c == '.') return false;
        out[i] = c == ':' ? '%' : c;
    }
    out[i] = '\0';
    return i > 0;
}

static void id_do_nome(const char *nome, char *out, size_t max)
{
    size_t i = 0;
    for (; nome[i] && i < max - 1; i++)
        out[i] = nome[i] == '%' ? ':' : nome[i];
    out[i] = '\0';
}

static bool caminho_meta(char *out, size_t max, const char *id)
{
    char nome[OBRA_ID];
    if (!nome_do_id(id, nome, sizeof nome)) return false;
    snprintf(out, max, ACERVO_RAIZ "/%s/meta.json", nome);
    return true;
}

// ── o meta ───────────────────────────────────────────────────────────
// JSON de um nível, lido pelo mesmo `json.c` do contrato.
erro_t acervo_grava_meta(const hal_t *hal, const obra_t *o)
{
    if (!hal || !hal->escrever || !o) return ERR_INTERNO;

    char nome[OBRA_ID];
    if (!nome_do_id(o->id, nome, sizeof nome)) return ERR_FORMATO;

    char pasta[80];
    snprintf(pasta, sizeof pasta, ACERVO_RAIZ "/%s", nome);
    (void)hal->criar_diretorio(ACERVO_RAIZ);
    (void)hal->criar_diretorio(pasta);

    char aberta[DATA_TEXTO] = "";
    char criada[DATA_TEXTO] = "";
    if (o->aberta_em.ano) data_para_texto(o->aberta_em, aberta, sizeof aberta);
    if (o->criada_em.ano) data_para_texto(o->criada_em, criada, sizeof criada);

    char corpo[1024];
    snprintf(corpo, sizeof corpo,
             "{\"id\":\"%s\",\"t\":\"%s\",\"a\":\"%s\",\"tp\":%d,"
             "\"e\":%d,\"n\":%ld,\"b\":\"%ld\",\"o\":%ld,"
             "\"pl\":%ld,\"ml\":%d,\"c\":%s,\"cp\":%s,\"ca\":%s,\"on\":%s,"
             "\"cr\":\"%s\",\"ab\":\"%s\",\"f\":%d,\"al\":%d,\"r\":%d}",
             o->id, o->titulo, o->autor,
             (int)o->tipo, (int)o->estado,
             (long)o->tamanho, (long)o->baixado, (long)o->offset_texto,
             (long)o->palavras, (int)o->minutos_leitura,
             o->concluida ? "true" : "false",
             o->tem_capa ? "true" : "false", o->capa_aqui ? "true" : "false",
             o->no_catalogo ? "true" : "false",
             criada, aberta, (int)o->fonte, (int)o->alinhamento, (int)o->rodape);

    char caminho[100];
    if (!caminho_meta(caminho, sizeof caminho, o->id)) return ERR_FORMATO;
    return hal->escrever(caminho, corpo);
}

erro_t acervo_le_meta(const hal_t *hal, const char *id, obra_t *out)
{
    if (!hal || !hal->ler || !id || !out) return ERR_INTERNO;
    memset(out, 0, sizeof *out);

    char caminho[100];
    if (!caminho_meta(caminho, sizeof caminho, id)) return ERR_FORMATO;

    char corpo[1024];
    erro_t err = hal->ler(caminho, corpo, sizeof corpo);
    if (err != OK) return err;

    if (!json_str(corpo, "id", out->id, sizeof out->id)) return ERR_FORMATO;
    (void)json_str(corpo, "t", out->titulo, sizeof out->titulo);
    (void)json_str(corpo, "a", out->autor,  sizeof out->autor);

    int v = 0;
    if (json_int(corpo, "tp", &v)) out->tipo   = (obra_tipo_t)v;
    if (json_int(corpo, "e",  &v)) out->estado = (obra_estado_t)v;
    if (json_int(corpo, "n",  &v)) out->tamanho = v;
    if (json_int(corpo, "pl", &v)) out->palavras = v;
    if (json_int(corpo, "ml", &v)) out->minutos_leitura = (int16_t)v;
    if (json_int(corpo, "o",  &v)) out->offset_texto = v;
    if (json_int(corpo, "f",  &v)) out->fonte = (int8_t)v;
    if (json_int(corpo, "al", &v)) out->alinhamento = (int8_t)v;
    if (json_int(corpo, "r",  &v)) out->rodape = (int8_t)v;

    // `baixado` vai como TEXTO: é o único número que cresce durante a escrita,
    // e como número o `json_int` lia o valor de antes no meio da regravação.
    char b[16] = "";
    if (json_str(corpo, "b", b, sizeof b)) {
        long n = 0;
        for (const char *p = b; *p >= '0' && *p <= '9'; p++)
            n = n * 10 + (*p - '0');
        out->baixado = (int32_t)n;
    }

    bool feita = false;
    if (json_bool(corpo, "c", &feita)) out->concluida = feita;
    if (json_bool(corpo, "cp", &feita)) out->tem_capa = feita;
    if (json_bool(corpo, "ca", &feita)) out->capa_aqui = feita;
    if (json_bool(corpo, "on", &feita)) out->no_catalogo = feita;

    char aberta[DATA_TEXTO] = "";
    if (json_str(corpo, "ab", aberta, sizeof aberta) && aberta[0])
        (void)data_de_texto(aberta, &out->aberta_em);
    char criada[DATA_TEXTO] = "";
    if (json_str(corpo, "cr", criada, sizeof criada) && criada[0])
        (void)data_de_texto(criada, &out->criada_em);

    return OK;
}

erro_t acervo_marca_aberta(const hal_t *hal, const char *id)
{
    char nome[OBRA_ID];
    if (!hal || !hal->escrever) return ERR_INTERNO;
    if (!nome_do_id(id, nome, sizeof nome)) return ERR_FORMATO;
    (void)hal->criar_diretorio(ACERVO_RAIZ);
    return hal->escrever(ACERVO_RAIZ "/ultima.txt", id);
}

// ── a listagem ───────────────────────────────────────────────────────
erro_t acervo_lista(const hal_t *hal, obra_t *out, int max, int *quantos)
{
    if (quantos) *quantos = 0;
    if (!hal || !hal->listar || !out || max <= 0) return ERR_INTERNO;

    char nomes[OBRAS_MAX][40];
    int n = 0;
    // Pasta que não existe é acervo VAZIO: nasce na primeira obra.
    if (hal->listar(ACERVO_RAIZ, 0, nomes, OBRAS_MAX, &n) != OK) return OK;

    int achadas = 0;
    for (int i = 0; i < n && achadas < max; i++) {
        char id[OBRA_ID];
        id_do_nome(nomes[i], id, sizeof id);

        // Só quem tem meta legível. Pasta só com `.part` é transferência
        // interrompida.
        if (acervo_le_meta(hal, id, &out[achadas]) != OK) continue;
        achadas++;
    }

    // Da aberta mais recente para a mais antiga; a nunca aberta vai para o fim.
    for (int i = 1; i < achadas; i++) {
        obra_t chave = out[i];
        int j = i - 1;
        while (j >= 0) {
            data_t a = out[j].aberta_em, b = chave.aberta_em;
            bool antes = (!a.ano && b.ano) ||
                         (a.ano && b.ano && data_compara(a, b) < 0);
            if (!antes) break;
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = chave;
    }

    // `aberta_em` tem resolução de um dia; o marcador resolve alternar dois
    // livros no mesmo dia.
    char ultima[OBRA_ID] = "";
    if (hal->ler &&
        hal->ler(ACERVO_RAIZ "/ultima.txt", ultima, sizeof ultima) == OK) {
        for (int i = 1; i < achadas; i++) {
            if (strcmp(out[i].id, ultima) != 0) continue;
            obra_t aberta = out[i];
            memmove(&out[1], &out[0], (size_t)i * sizeof out[0]);
            out[0] = aberta;
            break;
        }
    }

    if (quantos) *quantos = achadas;
    return OK;
}

erro_t acervo_remove_local(const hal_t *hal, const char *id)
{
    if (!hal || !hal->apagar || !id) return ERR_INTERNO;

    char nome[OBRA_ID];
    if (!nome_do_id(id, nome, sizeof nome)) return ERR_FORMATO;

    // Os arquivos conhecidos, um a um, nunca uma varredura: apagar o resto
    // seria apagar o que alguém pôs lá.
    const char *dentro[] = { "meta.json", "texto.txt", "texto.part", "sinopse.txt",
                             "capa.hex", "capa.part", "capa-mini.hex",
                             "capa-destaque.hex" };
    for (size_t i = 0; i < sizeof dentro / sizeof dentro[0]; i++) {
        char caminho[110];
        snprintf(caminho, sizeof caminho, ACERVO_RAIZ "/%s/%s", nome, dentro[i]);
        (void)hal->apagar(caminho);
    }

    char pasta[80];
    snprintf(pasta, sizeof pasta, ACERVO_RAIZ "/%s", nome);
    (void)hal->apagar(pasta);
    return OK;
}

// ── o TEXTO ──────────────────────────────────────────────────────────
// `texto.part` enquanto vem, `texto.txt` quando chega inteiro. O leitor só
// abre `texto.txt`.
static bool caminho_texto(char *out, size_t max, const char *id, bool inteiro)
{
    char nome[OBRA_ID];
    if (!nome_do_id(id, nome, sizeof nome)) return false;
    snprintf(out, max, ACERVO_RAIZ "/%s/%s", nome,
             inteiro ? "texto.txt" : "texto.part");
    return true;
}

erro_t acervo_grava_texto(const hal_t *hal, const char *id,
                          const char *texto, bool inteiro)
{
    if (!hal || !hal->escrever || !id || !texto) return ERR_INTERNO;

    char nome[OBRA_ID];
    if (!nome_do_id(id, nome, sizeof nome)) return ERR_FORMATO;

    char pasta[80];
    snprintf(pasta, sizeof pasta, ACERVO_RAIZ "/%s", nome);
    (void)hal->criar_diretorio(ACERVO_RAIZ);
    (void)hal->criar_diretorio(pasta);

    char caminho[110];
    if (!caminho_texto(caminho, sizeof caminho, id, inteiro))
        return ERR_FORMATO;

    erro_t err = hal->escrever(caminho, texto);

    // Virou obra: o `.part` não serve mais.
    if (err == OK && inteiro) {
        char part[110];
        if (caminho_texto(part, sizeof part, id, false))
            (void)hal->apagar(part);
    }
    return err;
}

erro_t acervo_anexa_texto(const hal_t *hal, const char *id,
                          const char *texto, bool primeiro, bool inteiro)
{
    if (!hal || !hal->escrever || !hal->anexar || !hal->renomear ||
        !id || !texto) return ERR_INTERNO;
    char nome[OBRA_ID];
    if (!nome_do_id(id, nome, sizeof nome)) return ERR_FORMATO;
    char pasta[80], part[110], final[110];
    snprintf(pasta, sizeof pasta, ACERVO_RAIZ "/%s", nome);
    (void)hal->criar_diretorio(ACERVO_RAIZ);
    (void)hal->criar_diretorio(pasta);
    if (!caminho_texto(part, sizeof part, id, false) ||
        !caminho_texto(final, sizeof final, id, true)) return ERR_FORMATO;
    erro_t err = primeiro ? hal->escrever(part, texto)
                          : hal->anexar(part, texto);
    if (err == OK && inteiro) err = hal->renomear(part, final);
    return err;
}

bool acervo_tem_parcial(const hal_t *hal, const char *id)
{
    if (!hal || !hal->tipo_caminho || !id) return false;
    char caminho[110];
    if (!caminho_texto(caminho, sizeof caminho, id, false)) return false;
    caminho_tipo_t tipo = CAMINHO_AUSENTE;
    return hal->tipo_caminho(caminho, &tipo) == OK && tipo == CAMINHO_ARQUIVO;
}

erro_t acervo_descarta_parcial(const hal_t *hal, const char *id)
{
    if (!hal || !hal->apagar || !id) return ERR_INTERNO;
    char caminho[110];
    if (!caminho_texto(caminho, sizeof caminho, id, false)) return ERR_FORMATO;
    return hal->apagar(caminho);
}

erro_t acervo_le_texto(const hal_t *hal, const char *id,
                       char *out, size_t max)
{
    if (out && max) out[0] = '\0';
    if (!hal || !hal->ler || !id || !out) return ERR_INTERNO;

    // SÓ o inteiro.
    char caminho[110];
    if (!caminho_texto(caminho, sizeof caminho, id, true)) return ERR_FORMATO;
    return hal->ler(caminho, out, max);
}

erro_t acervo_grava_sinopse(const hal_t *hal, const char *id, const char *texto)
{
    if (!hal || !hal->escrever || !id || !texto) return ERR_INTERNO;
    char nome[OBRA_ID], pasta[80], caminho[110];
    if (!nome_do_id(id, nome, sizeof nome)) return ERR_FORMATO;
    snprintf(pasta, sizeof pasta, ACERVO_RAIZ "/%s", nome);
    (void)hal->criar_diretorio(ACERVO_RAIZ); (void)hal->criar_diretorio(pasta);
    snprintf(caminho, sizeof caminho, "%s/sinopse.txt", pasta);
    return hal->escrever(caminho, texto);
}

erro_t acervo_le_sinopse(const hal_t *hal, const char *id, char *out, size_t max)
{
    if (out && max) out[0]='\0';
    if (!hal || !hal->ler || !id || !out) return ERR_INTERNO;
    char nome[OBRA_ID], caminho[110];
    if (!nome_do_id(id, nome, sizeof nome)) return ERR_FORMATO;
    snprintf(caminho, sizeof caminho, ACERVO_RAIZ "/%s/sinopse.txt", nome);
    return hal->ler(caminho, out, max);
}

erro_t acervo_anexa_capa(const hal_t *hal, const char *id, const char *hex,
                         bool primeiro, bool inteira)
{
    if (!hal || !hal->escrever || !hal->anexar || !hal->renomear ||
        !id || !hex) return ERR_INTERNO;
    for (const char *p = hex; *p; p++)
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') ||
              (*p >= 'A' && *p <= 'F'))) return ERR_FORMATO;
    char nome[OBRA_ID], pasta[80], part[110], final[110];
    if (!nome_do_id(id, nome, sizeof nome)) return ERR_FORMATO;
    snprintf(pasta, sizeof pasta, ACERVO_RAIZ "/%s", nome);
    (void)hal->criar_diretorio(ACERVO_RAIZ);
    (void)hal->criar_diretorio(pasta);
    snprintf(part, sizeof part, "%s/capa.part", pasta);
    snprintf(final, sizeof final, "%s/capa.hex", pasta);
    erro_t err = primeiro ? hal->escrever(part, hex) : hal->anexar(part, hex);
    if (err == OK && inteira) err = hal->renomear(part, final);
    return err;
}

erro_t acervo_le_capa(const hal_t *hal, const char *id,
                      uint8_t *out, size_t max)
{
    if (!hal || !hal->ler || !hal->emprestar || !hal->devolver ||
        !out || max < 10800) return ERR_INTERNO;
    char nome[OBRA_ID], caminho[110];
    if (!nome_do_id(id, nome, sizeof nome)) return ERR_FORMATO;
    snprintf(caminho, sizeof caminho, ACERVO_RAIZ "/%s/capa.hex", nome);
    size_t real = 0;
    char *hex = hal->emprestar(21601, 21601, &real);
    if (!hex) return ERR_INTERNO;
    erro_t err = hal->ler(caminho, hex, real);
    if (err != OK || strlen(hex) != 21600) { hal->devolver(hex); return ERR_DADO_INCOMPLETO; }
    for (int i = 0; i < 10800; i++) {
        int a = hex[i * 2], b = hex[i * 2 + 1];
        a = a <= '9' ? a - '0' : (a | 32) - 'a' + 10;
        b = b <= '9' ? b - '0' : (b | 32) - 'a' + 10;
        if (a < 0 || a > 15 || b < 0 || b > 15) {
            hal->devolver(hex);
            return ERR_FORMATO;
        }
        out[i] = (uint8_t)((a << 4) | b);
    }
    hal->devolver(hex);
    return OK;
}

erro_t acervo_le_capa_mini(const hal_t *hal, const char *id,
                           uint8_t *out, size_t max)
{
    if (!hal || !hal->ler || !out || max < 245) return ERR_INTERNO;
    char nome[OBRA_ID], caminho[110], hex[491];
    if (!nome_do_id(id, nome, sizeof nome)) return ERR_FORMATO;
    snprintf(caminho, sizeof caminho, ACERVO_RAIZ "/%s/capa-mini.hex", nome);
    erro_t err = hal->ler(caminho, hex, sizeof hex);
    if (err != OK || strlen(hex) != 490) return ERR_DADO_INCOMPLETO;
    for (int i = 0; i < 245; i++) {
        int a = hex[i * 2], b = hex[i * 2 + 1];
        a = a <= '9' ? a - '0' : (a | 32) - 'a' + 10;
        b = b <= '9' ? b - '0' : (b | 32) - 'a' + 10;
        if (a < 0 || a > 15 || b < 0 || b > 15) return ERR_FORMATO;
        out[i] = (uint8_t)((a << 4) | b);
    }
    return OK;
}

static erro_t grava_capa_pequena(const hal_t *hal, const char *id,
                                 const char *arquivo, const char *hex,
                                 size_t letras)
{
    if (!hal || !hal->escrever || !id || !hex) return ERR_INTERNO;
    if (strlen(hex) != letras) return ERR_DADO_INCOMPLETO;
    for (size_t i = 0; i < letras; i++)
        if (!((hex[i] >= '0' && hex[i] <= '9') ||
              (hex[i] >= 'a' && hex[i] <= 'f') ||
              (hex[i] >= 'A' && hex[i] <= 'F'))) return ERR_FORMATO;
    char nome[OBRA_ID], pasta[80], caminho[110];
    if (!nome_do_id(id, nome, sizeof nome)) return ERR_FORMATO;
    snprintf(pasta, sizeof pasta, ACERVO_RAIZ "/%s", nome);
    (void)hal->criar_diretorio(ACERVO_RAIZ);
    (void)hal->criar_diretorio(pasta);
    snprintf(caminho, sizeof caminho, "%s/%s", pasta, arquivo);
    return hal->escrever(caminho, hex);
}

static erro_t le_capa_pequena(const hal_t *hal, const char *id,
                              const char *arquivo, uint8_t *out,
                              size_t bytes, size_t max)
{
    if (!hal || !hal->ler || !out || max < bytes) return ERR_INTERNO;
    char nome[OBRA_ID], caminho[110], hex[757];
    if (!nome_do_id(id, nome, sizeof nome)) return ERR_FORMATO;
    snprintf(caminho, sizeof caminho, ACERVO_RAIZ "/%s/%s", nome, arquivo);
    erro_t err = hal->ler(caminho, hex, sizeof hex);
    if (err != OK || strlen(hex) != bytes * 2) return ERR_DADO_INCOMPLETO;
    for (size_t i = 0; i < bytes; i++) {
        int a = hex[i * 2], b = hex[i * 2 + 1];
        a = a <= '9' ? a - '0' : (a | 32) - 'a' + 10;
        b = b <= '9' ? b - '0' : (b | 32) - 'a' + 10;
        if (a < 0 || a > 15 || b < 0 || b > 15) return ERR_FORMATO;
        out[i] = (uint8_t)((a << 4) | b);
    }
    return OK;
}

erro_t acervo_grava_capa_mini(const hal_t *hal, const char *id,
                              const char *hex)
{
    return grava_capa_pequena(hal, id, "capa-mini.hex", hex, 490);
}

erro_t acervo_grava_capa_destaque(const hal_t *hal, const char *id,
                                  const char *hex)
{
    return grava_capa_pequena(hal, id, "capa-destaque.hex", hex, 756);
}

erro_t acervo_le_capa_destaque(const hal_t *hal, const char *id,
                               uint8_t *out, size_t max)
{
    return le_capa_pequena(hal, id, "capa-destaque.hex", out, 378, max);
}

bool acervo_tem_espaco(const hal_t *hal, int32_t bytes)
{
    if (!hal || bytes <= 0) return false;
    if (!hal->memoria_estado) return true;

    // Cartão cheio não começa download: o `.part` ocuparia o que já falta.
    return hal->memoria_estado() != MEMORIA_CHEIA;
}
