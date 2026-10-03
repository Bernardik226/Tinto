// firmware/testes/t_acervo.c — o acervo do lado do aparelho.
// Uma obra online e uma cópia que abre sem rede não são a mesma coisa.
#include "teste.h"
#include "dado/acervo.h"
#include "uso/acervo.h"

static app_t ap;
static const hal_t *hal;

#define HOJE ((data_t){2026, 9, 4})

static void liga(void)
{
    hal = pc_liga();
    pc_relogio(HOJE, 9, 0);
    app_liga(&ap, hal);
}

static obra_t obra_de(const char *id, const char *titulo)
{
    obra_t o;
    memset(&o, 0, sizeof o);
    snprintf(o.id,     sizeof o.id,     "%s", id);
    snprintf(o.titulo, sizeof o.titulo, "%s", titulo);
    o.tipo = OBRA_LIVRO;
    o.estado = OBRA_AQUI;
    o.tamanho = 1000;
    return o;
}

// ── os três estados, cada um com um ícone ───────────────────────────
void t_acervo_guarda_os_tres_estados(void)
{
    COMECA("acervo · só online, baixando e aqui são estados distintos");

    liga();

    obra_t o = obra_de("ob:abc", "Dom Casmurro");
    o.estado = OBRA_SO_ONLINE;
    ESPERA_IGUAL(acervo_grava_meta(hal, &o), OK);

    obra_t lida;
    ESPERA_IGUAL(acervo_le_meta(hal, "ob:abc", &lida), OK);
    ESPERA_IGUAL(lida.estado, OBRA_SO_ONLINE);
    ESPERA_TEXTO(lida.titulo, "Dom Casmurro");

    o.estado = OBRA_BAIXANDO;
    o.baixado = 400;
    ESPERA_IGUAL(acervo_grava_meta(hal, &o), OK);
    ESPERA_IGUAL(acervo_le_meta(hal, "ob:abc", &lida), OK);
    ESPERA_IGUAL(lida.estado, OBRA_BAIXANDO);
    ESPERA_IGUAL(lida.baixado, 400);
    TERMINA();
}

// ── os dois progressos não se misturam ──────────────────────────────
void t_acervo_separa_transferencia_de_leitura(void)
{
    COMECA("acervo · quanto baixou e onde parou de ler são dois números");

    liga();

    obra_t o = obra_de("ob:abc", "Dom Casmurro");
    o.baixado = 1000;          // inteira no cartão
    o.offset_texto = 0;        // e nunca aberta
    ESPERA_IGUAL(acervo_grava_meta(hal, &o), OK);

    obra_t lida;
    ESPERA_IGUAL(acervo_le_meta(hal, "ob:abc", &lida), OK);
    ESPERA_IGUAL(lida.baixado, 1000);
    ESPERA_IGUAL(lida.offset_texto, 0);

    // Ler não mexe no que foi baixado.
    lida.offset_texto = 350;
    ESPERA_IGUAL(acervo_grava_meta(hal, &lida), OK);
    ESPERA_IGUAL(acervo_le_meta(hal, "ob:abc", &lida), OK);
    ESPERA_IGUAL(lida.baixado, 1000);
    ESPERA_IGUAL(lida.offset_texto, 350);
    TERMINA();
}

// ── da abertura mais recente para a mais antiga ─────────────────────
void t_acervo_lista_da_mais_recente_para_a_mais_antiga(void)
{
    COMECA("acervo · a lista vem da aberta mais recente para trás");

    liga();

    obra_t a = obra_de("ob:a", "Primeira");
    a.aberta_em = (data_t){2026, 9, 1};
    obra_t b = obra_de("ob:b", "Segunda");
    b.aberta_em = (data_t){2026, 9, 4};
    obra_t c = obra_de("ob:c", "Terceira");
    c.aberta_em = (data_t){2026, 8, 20};

    ESPERA_IGUAL(acervo_grava_meta(hal, &a), OK);
    ESPERA_IGUAL(acervo_grava_meta(hal, &b), OK);
    ESPERA_IGUAL(acervo_grava_meta(hal, &c), OK);

    obra_t lista[OBRAS_MAX];
    int n = 0;
    ESPERA_IGUAL(acervo_lista(hal, lista, OBRAS_MAX, &n), OK);
    ESPERA_IGUAL(n, 3);
    ESPERA_TEXTO(lista[0].titulo, "Segunda");
    ESPERA_TEXTO(lista[1].titulo, "Primeira");
    ESPERA_TEXTO(lista[2].titulo, "Terceira");
    TERMINA();
}

// ── o `.part` NUNCA é obra ──────────────────────────────────────────
void t_acervo_ignora_o_que_esta_pela_metade(void)
{
    COMECA("acervo · um download pela metade não aparece na estante");

    liga();

    obra_t o = obra_de("ob:abc", "Dom Casmurro");
    ESPERA_IGUAL(acervo_grava_meta(hal, &o), OK);

    // Um `.part` na pasta, como durante uma transferência.
    pc_poe_arquivo("/TINTO/acervo/ob%abc/texto.part", "meio livro");

    obra_t lista[OBRAS_MAX];
    int n = 0;
    ESPERA_IGUAL(acervo_lista(hal, lista, OBRAS_MAX, &n), OK);
    ESPERA_IGUAL(n, 1);
    ESPERA_TEXTO(lista[0].id, "ob:abc");
    TERMINA();
}

// ── remover é LOCAL, e o caminho é construído aqui ──────────────────
void t_acervo_remove_so_a_copia_local(void)
{
    COMECA("acervo · remover a cópia não tira a obra do acervo");

    liga();

    obra_t o = obra_de("ob:abc", "Dom Casmurro");
    ESPERA_IGUAL(acervo_grava_meta(hal, &o), OK);
    ESPERA_IGUAL(acervo_remove_local(hal, "ob:abc"), OK);

    obra_t lida;
    ESPERA(acervo_le_meta(hal, "ob:abc", &lida) != OK);

    // Id com travessia não apaga nada fora de `/TINTO/acervo`.
    ESPERA(acervo_remove_local(hal, "../../sistema") != OK);
    TERMINA();
}

void t_acervo_descarta_local_sem_apagar_a_origem_online(void)
{
    COMECA("acervo · descartar tira só a cópia deste Tinto");
    liga();

    obra_t o = obra_de("ob:abc", "Dom Casmurro");
    o.baixado = o.tamanho;
    o.no_catalogo = true;
    ESPERA_IGUAL(acervo_grava_meta(hal, &o), OK);
    ESPERA_IGUAL(acervo_grava_texto(hal, o.id, "livro", true), OK);

    ESPERA_IGUAL(uso_acervo_descarta_local(hal, &ap.estado, &o), OK);

    obra_t lida;
    ESPERA_IGUAL(acervo_le_meta(hal, o.id, &lida), OK);
    ESPERA_IGUAL(lida.estado, OBRA_SO_ONLINE);
    ESPERA(lida.no_catalogo);
    ESPERA(acervo_le_texto(hal, o.id, (char[16]){0}, 16) != OK);
    TERMINA();
}
