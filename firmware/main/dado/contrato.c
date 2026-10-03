#include "contrato.h"
#include "json.h"
#include "../nucleo/data.h"

#include <stdio.h>
#include <string.h>

const char *contrato_fim_do_array(const char *abre)
{
    if (!abre || *abre != '[') return NULL;

    // Um nível de aninhamento, por contrato. Conta colchetes e pula strings
    // inteiras: um `]` num título fecharia cedo.
    int fundo = 0;
    for (const char *p = abre; *p; p++) {
        if (*p == '"') {
            p++;
            while (*p && *p != '"') p += (*p == '\\' && p[1]) ? 2 : 1;
            if (!*p) return NULL;
            continue;
        }
        if (*p == '[') fundo++;
        else if (*p == ']' && --fundo == 0) return p;
    }
    return NULL;
}

bool contrato_proximo_ate(const char **cursor, const char *fim,
                          char *objeto, size_t max)
{
    if (!cursor || !*cursor) return false;
    if (fim && *cursor >= fim) return false;

    const char *antes = *cursor;
    if (!contrato_proximo(cursor, objeto, max)) return false;

    // Passou do fim do array: o objeto era do próximo campo.
    if (fim && *cursor > fim + 1) {
        *cursor = antes;
        return false;
    }
    return true;
}

bool contrato_proximo(const char **cursor, char *objeto, size_t max)
{
    if (!cursor || !*cursor || !objeto || max == 0) return false;

    const char *p = strchr(*cursor, '{');
    if (!p) return false;

    // Um nível só: o primeiro '}' fecha o objeto.
    const char *fim = strchr(p, '}');
    if (!fim) return false;

    size_t n = (size_t)(fim - p) + 1;
    if (n >= max) n = max - 1;
    memcpy(objeto, p, n);
    objeto[n] = '\0';

    *cursor = fim + 1;
    return true;
}

bool contrato_item(const char *objeto, item_t *out)
{
    if (!objeto || !out) return false;
    memset(out, 0, sizeof *out);

    if (!json_str(objeto, "id", out->id, sizeof out->id)) return false;

    (void)json_str(objeto, "t", out->titulo, sizeof out->titulo);
    (void)json_str(objeto, "h", out->hora,   sizeof out->hora);
    (void)json_str(objeto, "f", out->fim,    sizeof out->fim);
    (void)json_str(objeto, "l", out->local,  sizeof out->local);
    (void)json_str(objeto, "a", out->agenda, sizeof out->agenda);

    bool di = false;
    if (json_bool(objeto, "di", &di)) out->dia_inteiro = di;

    bool rep = false;
    if (json_bool(objeto, "r", &rep)) out->repete = rep;

    (void)json_str(objeto, "rr", out->regra, sizeof out->regra);

    // Dia inteiro VENCE a hora: `h` preenchido ali é dado sujo.
    if (out->dia_inteiro) out->hora[0] = '\0';

    bool ok = false;
    if (json_bool(objeto, "ok", &ok)) out->feita = ok;

    // Quando foi concluída, vindo do Google (a verdade). O device carimba ao
    // marcar, otimista, e o pull confirma.
    char feita_em[DATA_TEXTO];
    if (json_str(objeto, "c", feita_em, sizeof feita_em))
        data_de_texto(feita_em, &out->feita_em);

    char data[DATA_TEXTO];
    if (json_str(objeto, "d", data, sizeof data))
        (void)data_de_texto(data, &out->vence);

    char prazo[DATA_TEXTO];
    if (json_str(objeto, "p", prazo, sizeof prazo))
        (void)data_de_texto(prazo, &out->prazo);

    char origem[4] = "";
    (void)json_str(objeto, "o", origem, sizeof origem);
    out->origem = origem[0] == 'g' ? ORIGEM_GOOGLE : ORIGEM_AQUI;

    // O tipo tem duas fontes. No pull, o prefixo do id (`g:` evento, `t:`
    // tarefa, `l:` lista). Na captura ainda não há id do outro lado, e vem `tp`,
    // a ferramenta que a LLM escolheu — ele VENCE o prefixo.
    //
    // De que nota veio (vazio = Google). Atravessa o pull também, para o vínculo
    // com a fala sobreviver a um cartão novo.
    (void)json_str(objeto, "nota", out->nota, sizeof out->nota);

    // Tamanho da lista e quantos feitos: "Compras · 2 de 7" sem abrir a lista.
    int lista = 0;
    if (json_int(objeto, "n", &lista)) out->lista_n = (int16_t)lista;
    if (json_int(objeto, "k", &lista)) out->lista_k = (int16_t)lista;

    int tp = -1;
    if (json_int(objeto, "tp", &tp) && tp >= TIPO_NADA && tp <= TIPO_EVENTO) {
        out->tipo = (tipo_t)tp;
    } else if (out->id[0] == 'g') out->tipo = TIPO_EVENTO;
    else if   (out->id[0] == 't') out->tipo = TIPO_TAREFA;
    else if   (out->id[0] == 'l') out->tipo = TIPO_LISTA;
    else                          out->tipo = TIPO_ANOTACAO;

    return true;
}


// ── a resposta da captura ────────────────────────────────────────────
static res_verbo_t verbo_de(const char *texto)
{
    if (strcmp(texto, "editou") == 0) return RES_EDITOU;
    if (strcmp(texto, "anotou") == 0) return RES_ANOTOU;
    if (strcmp(texto, "apagou") == 0) return RES_APAGOU;
    return RES_CRIOU;
}

int contrato_captura(const char *json, resultado_t *out, int max,
                     char *falou, size_t falou_max,
                     char *nota, size_t nota_max)
{
    if (falou && falou_max) falou[0] = '\0';
    if (nota && nota_max)   nota[0]  = '\0';
    if (!json || !out || max <= 0) return 0;

    if (falou && falou_max) (void)json_str(json, "falou", falou, falou_max);
    if (nota  && nota_max)  (void)json_str(json, "nota",  nota,  nota_max);

    // O cursor começa no texto do array, depois da chave.
    const char *cursor = strstr(json, "\"acoes\"");
    if (!cursor) return 0;

    int n = 0;
    char objeto[400];
    while (n < max && contrato_proximo(&cursor, objeto, sizeof objeto)) {
        resultado_t *r = &out[n];
        memset(r, 0, sizeof *r);

        char v[12] = "criou";
        (void)json_str(objeto, "v", v, sizeof v);
        r->verbo = verbo_de(v);

        // Ação sem item gravável (sem `id`) não entra: linha a menos em Conferir é
        // melhor que uma que o OK não aplica.
        if (!contrato_item(objeto, &r->item)) continue;

        // Criação sem tipo, ou "anotou", vira ANOTAÇÃO: a IA às vezes esquece o
        // `tp`, e o item ia ao Google como compromisso e sumia. Editar e apagar
        // ficam como vieram: apontam algo que já existe.
        bool cria = r->verbo != RES_EDITOU && r->verbo != RES_APAGOU;
        if (r->verbo == RES_ANOTOU || (cria && r->item.tipo == TIPO_NADA)) {
            r->item.tipo = TIPO_ANOTACAO;
            r->verbo     = RES_ANOTOU;
        }
        n++;
    }
    return n;
}

int contrato_gesto(const item_t *it, const char *verbo, const char *momento,
                   char *out, size_t max)
{
    if (!it || !out || !max) return 0;

    char venc[DATA_TEXTO] = "";
    if (it->vence.ano) data_para_texto(it->vence, venc, sizeof venc);
    char prazo[DATA_TEXTO] = "";
    if (it->prazo.ano) data_para_texto(it->prazo, prazo, sizeof prazo);

    // Os mesmos nomes de campo do pull: um contrato só nas duas direções.
    // `nota` diz de que fala veio, e é por ela que o servidor guarda o texto de
    // uma anotação.
    int n = snprintf(out, max,
                     "{\"v\":\"%s\",\"id\":\"%s\",\"t\":\"%s\","
                     "\"h\":\"%s\",\"d\":\"%s\",\"p\":\"%s\",\"tp\":%d,"
                     "\"ok\":%s,\"em\":\"%s\",\"nota\":\"%s\"}",
                     verbo ? verbo : "criou", it->id, it->titulo, it->hora,
                     venc, prazo, (int)it->tipo, it->feita ? "true" : "false",
                     momento ? momento : "", it->nota);
    return (n > 0 && (size_t)n < max) ? n : 0;
}
