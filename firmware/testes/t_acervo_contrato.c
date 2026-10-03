// firmware/testes/t_acervo_contrato.c — o contrato do acervo, travado nos
// dois lados. O JSON foi CAPTURADO da suíte do backend
// (`backend/testes/fixtures/acervo/lote.json`): se o formato mudar, quebra
// aqui antes do vidro.
#include "teste.h"
#include "dado/acervo.h"
#include "uso/acervo.h"
#include "uso/nuvem.h"

static app_t ap;
static const hal_t *hal;

// Capturado de `/v1/acervo`: um livro com autor e um documento sem.
static const char *LOTE =
    "{\"obras\":[{\"id\":\"ob:tysYaCTOJgoh\",\"t\":\"Contrato de aluguel\",\"a\":\"\",\"tp"
    "\":\"documento\",\"n\":380,\"aqui\":false},{\"id\":\"ob:-lilLkZOAb_6\",\"t\":\"Dom C"
    "asmurro\",\"a\":\"Machado de Assis\",\"tp\":\"livro\",\"n\":390,\"aqui\":false}],\"c"
    "ursor\":\"\"}";

static const char *VAZIO = "{\"obras\":[],\"cursor\":\"\"}";

static void liga(void)
{
    hal = pc_liga();
    pc_relogio((data_t){2026, 9, 5}, 9, 0);
    app_liga(&ap, hal);
    ap.estado.rede = REDE_LIGADA;
}

void t_contrato_o_lote_do_backend_e_lido_inteiro(void)
{
    COMECA("contrato · o lote capturado do backend é lido campo a campo");

    liga();
    ESPERA_IGUAL(uso_acervo_aplica(hal, &ap.estado, LOTE), OK);

    obra_t livro, documento;
    ESPERA_IGUAL(acervo_le_meta(hal, "ob:-lilLkZOAb_6", &livro), OK);
    ESPERA_TEXTO(livro.titulo, "Dom Casmurro");
    ESPERA_TEXTO(livro.autor, "Machado de Assis");
    ESPERA_IGUAL(livro.tipo, OBRA_LIVRO);
    ESPERA_IGUAL(livro.tamanho, 390);
    ESPERA_IGUAL(livro.estado, OBRA_SO_ONLINE);

    ESPERA_IGUAL(acervo_le_meta(hal, "ob:tysYaCTOJgoh", &documento), OK);
    ESPERA_TEXTO(documento.titulo, "Contrato de aluguel");
    ESPERA_IGUAL(documento.tipo, OBRA_DOCUMENTO);

    // Autor vazio é vazio, não "null".
    ESPERA_TEXTO(documento.autor, "");
    TERMINA();
}

void t_contrato_o_lote_vazio_nao_e_erro(void)
{
    COMECA("contrato · acervo vazio é uma resposta, não uma falha");

    liga();
    ESPERA_IGUAL(uso_acervo_aplica(hal, &ap.estado, VAZIO), OK);

    obra_t lista[OBRAS_MAX];
    int n = 0;
    ESPERA_IGUAL(acervo_lista(hal, lista, OBRAS_MAX, &n), OK);
    ESPERA_IGUAL(n, 0);
    TERMINA();
}

// ── o id atravessa inteiro (o `:` vira `%` e volta) ─────────────────
void t_contrato_o_id_volta_igual_do_cartao(void)
{
    COMECA("contrato · o id do servidor sobrevive à ida e volta do cartão");

    liga();
    ESPERA_IGUAL(uso_acervo_aplica(hal, &ap.estado, LOTE), OK);

    obra_t lista[OBRAS_MAX];
    int n = 0;
    ESPERA_IGUAL(acervo_lista(hal, lista, OBRAS_MAX, &n), OK);
    ESPERA_IGUAL(n, 2);

    bool achou = false;
    for (int i = 0; i < n; i++)
        if (strcmp(lista[i].id, "ob:-lilLkZOAb_6") == 0) achou = true;
    ESPERA(achou);
    TERMINA();
}

// ── o TAMANHO confere a cópia ───────────────────────────────────────
void t_contrato_o_tamanho_do_lote_confere_a_copia(void)
{
    COMECA("contrato · o tamanho do lote é o que valida o download");

    liga();
    ESPERA_IGUAL(uso_acervo_aplica(hal, &ap.estado, LOTE), OK);

    static char texto[400];
    memset(texto, 'a', 389);
    texto[389] = '\0';                 // um byte a menos que o declarado

    ap.estado.nuvem_esperando = NUVEM_OBRA;
    snprintf(ap.estado.obra_baixando, sizeof ap.estado.obra_baixando,
             "%s", "ob:-lilLkZOAb_6");
    ESPERA_IGUAL(uso_acervo_recebe(hal, &ap.estado, texto), OK);

    obra_t o;
    ESPERA_IGUAL(acervo_le_meta(hal, "ob:-lilLkZOAb_6", &o), OK);
    ESPERA(o.estado != OBRA_AQUI);
    ESPERA_IGUAL(o.baixado, 389);
    ESPERA_CONTEM(pc_nuvem_rota(), "offset=389");
    TERMINA();
}
