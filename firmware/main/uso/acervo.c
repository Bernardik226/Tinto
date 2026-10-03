#include "acervo.h"
#include "../nucleo/data.h"
#include "nuvem.h"
#include "../dado/acervo.h"
#include "../dado/contrato.h"
#include "../dado/json.h"
#include <stdio.h>
#include <string.h>

static void texto_meta_seguro(char *s)
{
    if (!s) return;
    for (; *s; s++) {
        if (*s == '"') *s = '\'';
        else if (*s == '\\') *s = '/';
        else if (*s == '\n' || *s == '\r' || *s == '\t') *s = ' ';
    }
}

// ── o cache da estante ───────────────────────────────────────────────
erro_t uso_carregar_acervo(const hal_t *hal, estado_t *e)
{
    if (!hal || !e) return ERR_INTERNO;

    int n = 0;
    erro_t err = acervo_lista(hal, e->acervo, OBRAS_MAX, &n);
    e->n_acervo = (int8_t)(err == OK ? n : 0);
    e->acervo_valido = true;
    return err;
}

// ── o CATÁLOGO ───────────────────────────────────────────────────────
erro_t uso_acervo_sincroniza(const hal_t *hal, estado_t *e)
{
    if (!hal || !e || !hal->nuvem_pede) return ERR_INTERNO;
    if (e->rede != REDE_LIGADA) return ERR_REDE;
    if (e->nuvem_esperando != NUVEM_NADA) return OK;

    // O catálogo chega em lotes. Cada item visto é marcado, e só o último lote
    // remove o ausente: comparar página isolada apagaria a 13ª obra.
    (void)uso_carregar_acervo(hal, e);
    memset(e->acervo_visto, 0, sizeof e->acervo_visto);

    e->nuvem_esperando = NUVEM_ACERVO;
    e->nuvem_desde_ms  = e->agora_ms;
    e->sinc            = SINC_RECEBENDO;
    hal->nuvem_pede("/v1/acervo", NULL, NULL);
    return OK;
}

erro_t uso_acervo_aplica(const hal_t *hal, estado_t *e, const char *json)
{
    if (!hal || !e || !json) return ERR_INTERNO;

    const char *lista = strstr(json, "\"obras\"");
    if (!lista) return ERR_FORMATO;

    const char *p = strchr(lista, '[');
    char objeto[720];
    int entraram = 0;

    if (!e->acervo_valido) {
        (void)uso_carregar_acervo(hal, e);
        memset(e->acervo_visto, 0, sizeof e->acervo_visto);
    }

    while (p && contrato_proximo(&p, objeto, sizeof objeto)) {
        char id[OBRA_ID] = "";
        if (!json_str(objeto, "id", id, sizeof id)) continue;

        // Cópia baixada, posição de leitura e "concluída" são deste aparelho: o
        // catálogo não pode sobrescrevê-los.
        obra_t o;
        bool tinha = acervo_le_meta(hal, id, &o) == OK;
        if (!tinha) {
            memset(&o, 0, sizeof o);
            snprintf(o.id, sizeof o.id, "%s", id);
            o.estado = OBRA_SO_ONLINE;
        }
        obra_t antes = o;
        char sinopse_antes[OBRA_SINOPSE] = "";
        bool tinha_sinopse = tinha &&
            acervo_le_sinopse(hal, id, sinopse_antes,
                              sizeof sinopse_antes) == OK;

        (void)json_str(objeto, "t", o.titulo, sizeof o.titulo);
        (void)json_str(objeto, "a", o.autor,  sizeof o.autor);
        char sinopse[OBRA_SINOPSE] = "";
        (void)json_str(objeto, "s", sinopse, sizeof sinopse);
        texto_meta_seguro(o.titulo);
        texto_meta_seguro(o.autor);
        texto_meta_seguro(sinopse);

        int n = 0;
        if (json_int(objeto, "n", &n)) o.tamanho = n;
        if (json_int(objeto, "pl", &n)) o.palavras = n;
        if (json_int(objeto, "ml", &n)) o.minutos_leitura = (int16_t)n;
        char criada[DATA_TEXTO] = "";
        if (json_str(objeto, "cr", criada, sizeof criada) && criada[0])
            (void)data_de_texto(criada, &o.criada_em);

        char tipo[12] = "";
        if (json_str(objeto, "tp", tipo, sizeof tipo))
            o.tipo = strcmp(tipo, "documento") == 0 ? OBRA_DOCUMENTO
                                                    : OBRA_LIVRO;

        bool tem_capa = false;
        if (json_bool(objeto, "c", &tem_capa)) o.tem_capa = tem_capa;

        o.no_catalogo = true;
        bool mudou_meta = !tinha || memcmp(&antes, &o, sizeof o) != 0;
        bool mudou_sinopse = !tinha_sinopse ||
                             strcmp(sinopse_antes, sinopse) != 0;
        erro_t salvo = OK;
        if (mudou_sinopse) salvo = acervo_grava_sinopse(hal, o.id, sinopse);
        if (salvo == OK && mudou_meta) salvo = acervo_grava_meta(hal, &o);
        if (salvo == OK) entraram++;

        int cache = -1;
        for (int i = 0; i < e->n_acervo; i++)
            if (strcmp(e->acervo[i].id, id) == 0) { cache = i; break; }
        if (cache < 0 && e->n_acervo < OBRAS_MAX) cache = e->n_acervo++;
        if (cache >= 0) {
            e->acervo[cache] = o;
            e->acervo[cache].no_catalogo = true;
            e->acervo_visto[cache] = true;
        }
    }

    char cursor[OBRA_ID] = "";
    (void)json_str(json, "cursor", cursor, sizeof cursor);
    if (cursor[0]) {
        char rota[96];
        snprintf(rota, sizeof rota, "/v1/acervo?cursor=%s", cursor);
        e->nuvem_esperando = NUVEM_ACERVO;
        e->nuvem_desde_ms = e->agora_ms;
        e->sinc = SINC_RECEBENDO;
        hal->nuvem_pede(rota, NULL, NULL);
    } else {
        // O catálogo manda só no que é promessa; a cópia baixada é do cartão.
        for (int i = 0; i < e->n_acervo; i++) {
            obra_t *local = &e->acervo[i];
            if (e->acervo_visto[i]) {
                local->no_catalogo = true;
                continue;
            }
            local->no_catalogo = false;
            if (local->estado == OBRA_AQUI) {
                // A cópia continua legível, mas passa a ser só deste Tinto.
                (void)acervo_grava_meta(hal, local);
                continue;
            }
            if (strcmp(e->obra_baixando, local->id) == 0)
                e->obra_baixando[0] = '\0';
            (void)acervo_remove_local(hal, local->id);
        }
        e->acervo_valido = false;

        // O `.part` sobreviveu ao reboot com o byte onde parou: confirmada a obra
        // online, retoma sozinha.
        for (int i = 0; i < e->n_acervo; i++) {
            obra_t *local = &e->acervo[i];
            if (local->no_catalogo &&
                local->estado == OBRA_BAIXANDO && local->baixado > 0 &&
                acervo_tem_parcial(hal, local->id)) {
                (void)uso_acervo_baixa(hal, e, local->id);
                break;  // só há uma transferência por vez
            }
        }
    }

    if (hal->registrar) {
        char msg[48];
        snprintf(msg, sizeof msg, "acervo: %d obra(s)", entraram);
        hal->registrar("acervo", msg);
    }

    return OK;
}

// ── o TEXTO de uma obra ──────────────────────────────────────────────
erro_t uso_acervo_baixa(const hal_t *hal, estado_t *e, const char *id)
{
    if (!hal || !e || !id || !hal->nuvem_pede) return ERR_INTERNO;
    if (e->rede != REDE_LIGADA) return ERR_REDE;
    if (e->nuvem_esperando != NUVEM_NADA) return OK;

    obra_t o;
    if (acervo_le_meta(hal, id, &o) != OK) return ERR_ARQUIVO;

    // Antes de pedir: um download que não cabe deixa o `.part` ocupando o
    // espaço.
    if (!acervo_tem_espaco(hal, o.tamanho)) return ERR_CHEIO;

    bool retoma = o.estado == OBRA_BAIXANDO && o.baixado > 0 &&
                  acervo_tem_parcial(hal, id);
    o.estado = OBRA_BAIXANDO;
    if (!retoma) o.baixado = 0;
    erro_t salvo = acervo_grava_meta(hal, &o);
    if (salvo != OK) return salvo;

    // O clique aparece antes da primeira conexão.
    for (int i = 0; i < e->n_acervo; i++)
        if (strcmp(e->acervo[i].id, o.id) == 0) {
            e->acervo[i] = o;
            break;
        }

    snprintf(e->obra_baixando, sizeof e->obra_baixando, "%s", id);

    char rota[96];
    snprintf(rota, sizeof rota, "/v1/acervo/%s/conteudo/bloco?offset=%ld", id,
             (long)o.baixado);

    e->nuvem_esperando = NUVEM_OBRA;
    e->nuvem_desde_ms  = e->agora_ms;
    e->sinc            = SINC_RECEBENDO;
    hal->nuvem_pede(rota, NULL, NULL);
    return OK;
}

erro_t uso_acervo_descarta_local(const hal_t *hal, estado_t *e,
                                 const obra_t *obra)
{
    if (!hal || !e || !obra) return ERR_INTERNO;

    obra_t promessa = *obra;
    erro_t err = acervo_remove_local(hal, obra->id);
    if (err != OK) return err;

    // A origem online continua: recria só a ficha, sem texto, capa ou
    // progresso.
    if (promessa.no_catalogo) {
        promessa.estado = OBRA_SO_ONLINE;
        promessa.baixado = 0;
        promessa.offset_texto = 0;
        promessa.concluida = false;
        promessa.capa_aqui = false;
        err = acervo_grava_meta(hal, &promessa);
        if (err != OK) return err;
    }

    e->acervo_valido = false;
    return uso_carregar_acervo(hal, e);
}

erro_t uso_acervo_recebe(const hal_t *hal, estado_t *e, const char *texto)
{
    if (!hal || !e || !texto) return ERR_INTERNO;
    if (!e->obra_baixando[0]) return ERR_INTERNO;

    obra_t o;
    if (acervo_le_meta(hal, e->obra_baixando, &o) != OK) return ERR_ARQUIVO;

    int32_t bloco = (int32_t)strlen(texto);
    int32_t chegou = o.baixado + bloco;
    if (bloco <= 0 || chegou > o.tamanho) return ERR_DADO_INCOMPLETO;

    // O `.part` só vira obra CONFERIDO contra o tamanho declarado. O pedaço
    // fica gravado mesmo assim: a listagem ignora quem não tem meta válido.
    erro_t err = acervo_anexa_texto(hal, o.id, texto, o.baixado == 0,
                                    chegou >= o.tamanho);
    if (err != OK) return err;

    if (chegou < o.tamanho) {
        o.estado = OBRA_BAIXANDO;
        o.baixado = chegou;
        err = acervo_grava_meta(hal, &o);
        if (err != OK) {
            (void)acervo_descarta_parcial(hal, o.id);
            o.estado = OBRA_SO_ONLINE;
            o.baixado = 0;
            (void)acervo_grava_meta(hal, &o);
            e->obra_baixando[0] = '\0';
            return err;
        }
        for (int i = 0; i < e->n_acervo; i++)
            if (strcmp(e->acervo[i].id, o.id) == 0) {
                e->acervo[i] = o;
                break;
            }
        char rota[96];
        snprintf(rota, sizeof rota, "/v1/acervo/%s/conteudo/bloco?offset=%ld",
                 o.id, (long)chegou);
        e->nuvem_esperando = NUVEM_OBRA;
        e->nuvem_desde_ms = e->agora_ms;
        hal->nuvem_pede(rota, NULL, NULL);
        return OK;
    }

    o.estado = OBRA_AQUI;
    o.baixado = chegou;
    err = acervo_grava_meta(hal, &o);
    if (err != OK) return err;

    for (int i = 0; i < e->n_acervo; i++)
        if (strcmp(e->acervo[i].id, o.id) == 0) {
            e->acervo[i] = o;
            break;
        }

    e->acervo_valido = false;
    return OK;
}

erro_t uso_acervo_capa_baixa(const hal_t *hal, estado_t *e, const char *id)
{
    if (!hal || !e || !id || !hal->nuvem_pede) return ERR_INTERNO;
    if (e->rede != REDE_LIGADA || e->nuvem_esperando != NUVEM_NADA)
        return ERR_REDE;
    snprintf(e->obra_baixando, sizeof e->obra_baixando, "%s", id);
    char rota[96];
    e->capa_baixada = 0;
    e->capa_etapa = 0;
    snprintf(rota, sizeof rota, "/v1/acervo/%s/capa/grande?offset=0", id);
    e->nuvem_esperando = NUVEM_CAPA;
    e->nuvem_desde_ms = e->agora_ms;
    hal->nuvem_pede(rota, NULL, NULL);
    return OK;
}

erro_t uso_acervo_capas_pequenas_baixa(const hal_t *hal, estado_t *e,
                                       const char *id)
{
    if (!hal || !e || !id || !hal->nuvem_pede) return ERR_INTERNO;
    if (e->rede != REDE_LIGADA || e->nuvem_esperando != NUVEM_NADA)
        return ERR_REDE;
    snprintf(e->obra_baixando, sizeof e->obra_baixando, "%s", id);
    e->capa_baixada = 0;
    e->capa_etapa = 1;
    char rota[96];
    snprintf(rota, sizeof rota, "/v1/acervo/%s/capa/mini", id);
    e->nuvem_esperando = NUVEM_CAPA;
    e->nuvem_desde_ms = e->agora_ms;
    hal->nuvem_pede(rota, NULL, NULL);
    return OK;
}

erro_t uso_acervo_capa_recebe(const hal_t *hal, estado_t *e, const char *hex)
{
    if (!hal || !e || !hex || !e->obra_baixando[0]) return ERR_INTERNO;
    obra_t o;
    if (acervo_le_meta(hal, e->obra_baixando, &o) != OK) return ERR_ARQUIVO;
    size_t letras = strlen(hex);
    if (!letras || (letras & 1u)) return ERR_DADO_INCOMPLETO;

    if (e->capa_etapa == 1) {
        erro_t err = acervo_grava_capa_mini(hal, o.id, hex);
        if (err != OK) return err;
        e->capa_etapa = 2;
        char rota[104];
        snprintf(rota, sizeof rota, "/v1/acervo/%s/capa/destaque", o.id);
        e->nuvem_esperando = NUVEM_CAPA;
        e->nuvem_desde_ms = e->agora_ms;
        hal->nuvem_pede(rota, NULL, NULL);
        return OK;
    }
    if (e->capa_etapa == 2) {
        erro_t err = acervo_grava_capa_destaque(hal, o.id, hex);
        if (err != OK) return err;
        o.capa_aqui = true;
        err = acervo_grava_meta(hal, &o);
        if (err != OK) return err;
        if (strcmp(e->obra_aberta.id, o.id) == 0)
            e->obra_aberta.capa_aqui = true;
        for (int i = 0; i < e->n_acervo; i++)
            if (strcmp(e->acervo[i].id, o.id) == 0) {
                e->acervo[i].capa_aqui = true;
                break;
            }
        e->acervo_valido = false;
        e->obra_baixando[0] = '\0';
        e->capa_baixada = 0;
        e->capa_etapa = 0;
        return OK;
    }

    int32_t vieram = (int32_t)(letras / 2u);
    bool inteira = e->capa_baixada + vieram >= 10800;
    erro_t err = acervo_anexa_capa(hal, o.id, hex,
                                   e->capa_baixada == 0, inteira);
    if (err == OK) e->capa_baixada += vieram;
    if (err == OK && inteira) {
        e->capa_etapa = 1;
        e->capa_baixada = 0;
        char rota[96];
        snprintf(rota, sizeof rota, "/v1/acervo/%s/capa/mini", o.id);
        e->nuvem_esperando = NUVEM_CAPA;
        e->nuvem_desde_ms = e->agora_ms;
        hal->nuvem_pede(rota, NULL, NULL);
        return OK;
    }
    if (err == OK && !inteira) {
        char rota[112];
        snprintf(rota, sizeof rota, "/v1/acervo/%s/capa/grande?offset=%ld",
                 o.id, (long)e->capa_baixada);
        e->nuvem_esperando = NUVEM_CAPA;
        e->nuvem_desde_ms = e->agora_ms;
        hal->nuvem_pede(rota, NULL, NULL);
    } else {
        e->obra_baixando[0] = '\0';
        e->capa_baixada = 0;
        e->capa_etapa = 0;
    }
    return err;
}
