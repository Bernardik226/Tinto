#include "json.h"
#include <string.h>

// Acha "chave": comparando a chave inteira entre aspas ("t" não casa com
// "titulo").
static const char *acha_valor(const char *json, const char *chave)
{
    if (!json || !chave) return NULL;
    size_t n = strlen(chave);

    for (const char *p = json; (p = strchr(p, '"')) != NULL; p++) {
        const char *ini = p + 1;
        const char *fim = strchr(ini, '"');
        if (!fim) return NULL;
        if ((size_t)(fim - ini) == n && strncmp(ini, chave, n) == 0) {
            const char *q = fim + 1;
            while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
            if (*q == ':') {
                q++;
                while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') q++;
                return q;
            }
        }
        p = fim;   // pula o valor; não confunde conteúdo com chave
    }
    return NULL;
}

bool json_str(const char *json, const char *chave, char *out, size_t max)
{
    if (!out || max == 0) return false;
    out[0] = '\0';

    const char *v = acha_valor(json, chave);
    if (!v || *v != '"') return false;
    v++;

    size_t i = 0;
    while (*v && *v != '"') {
        if (*v == '\\' && v[1]) {
            v++;
            char c = *v;
            // o contrato só usa estes; o resto passa como veio
            if      (c == 'n') c = '\n';
            else if (c == 't') c = '\t';
            if (i + 1 < max) out[i++] = c;
        } else {
            if (i + 1 < max) out[i++] = *v;
        }
        v++;
        // Truncar, não rejeitar (RN-B7): segue até o fecha-aspas sem copiar.
    }
    out[i] = '\0';
    return true;
}

bool json_int(const char *json, const char *chave, int *out)
{
    const char *v = acha_valor(json, chave);
    if (!v) return false;

    bool negativo = false;
    if (*v == '-') { negativo = true; v++; }
    if (*v < '0' || *v > '9') return false;

    long n = 0;
    while (*v >= '0' && *v <= '9') {
        n = n * 10 + (*v - '0');
        if (n > 2147483647L) return false;   // RN-B9: valor absurdo é recusa
        v++;
    }
    if (out) *out = (int)(negativo ? -n : n);
    return true;
}

bool json_bool(const char *json, const char *chave, bool *out)
{
    const char *v = acha_valor(json, chave);
    if (!v) return false;
    if (strncmp(v, "true", 4) == 0)  { if (out) *out = true;  return true; }
    if (strncmp(v, "false", 5) == 0) { if (out) *out = false; return true; }
    return false;
}
