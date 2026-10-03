#include "log.h"
#include <stdio.h>
#include <string.h>

#define CAMINHO   "/TINTO/sistema/log.txt"
#define TETO      3000     // bytes; ~60 linhas, que é o que serve pra depurar

static const char *NOME[] = {
    [LOG_LIGOU]      = "ligou",
    [LOG_GRAVOU]     = "gravou",
    [LOG_DESCARTOU]  = "descartou",
    [LOG_MARCOU]     = "marcou",
    [LOG_ERRO]       = "erro",
    [LOG_FILA_CHEIA] = "fila-cheia",
    [LOG_DORMIU]     = "dormiu",
};

erro_t dado_log(const hal_t *hal, log_evento_t ev, int valor,
                int hora, int minuto)
{
    if (ev < 0 || ev > LOG_DORMIU) return ERR_INTERNO;

    char antes[TETO + 128];
    if (hal->ler(CAMINHO, antes, sizeof antes) != OK) antes[0] = '\0';

    // Passando do teto, corta a primeira metade: apagar tudo perderia o começo
    // do problema.
    size_t n = strlen(antes);
    if (n > TETO) {
        char *meio = strchr(antes + n / 2, '\n');
        if (meio) memmove(antes, meio + 1, strlen(meio + 1) + 1);
        else      antes[0] = '\0';
    }

    char linha[64];
    snprintf(linha, sizeof linha, "%02d:%02d %s %d\n",
             hora, minuto, NOME[ev], valor);

    char saida[TETO + 192];
    snprintf(saida, sizeof saida, "%s%s", antes, linha);

    // Sem .tmp + rename: log truncado por queda é uma linha a menos, e duas
    // escritas por linha gastariam o cartão à toa.
    return hal->escrever(CAMINHO, saida);
}
