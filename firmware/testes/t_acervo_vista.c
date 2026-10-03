// firmware/testes/t_acervo_vista.c — a Biblioteca, em conteúdo puro.
#include "teste.h"
#include "dado/acervo.h"
#include "vista/acervo.h"
#include "uso/acervo.h"

static app_t ap;
static const hal_t *hal;

#define HOJE ((data_t){2026, 9, 4})

static void liga(void)
{
    hal = pc_liga();
    pc_relogio(HOJE, 9, 14);
    app_liga(&ap, hal);
}

static void poe(const char *id, const char *titulo, obra_estado_t estado,
                int32_t tamanho, int32_t lido, data_t aberta)
{
    obra_t o;
    memset(&o, 0, sizeof o);
    snprintf(o.id,     sizeof o.id,     "%s", id);
    snprintf(o.titulo, sizeof o.titulo, "%s", titulo);
    o.tipo = OBRA_LIVRO;
    o.estado = estado;
    o.tamanho = tamanho;
    o.baixado = estado == OBRA_AQUI ? tamanho : 0;
    o.offset_texto = lido;
    o.aberta_em = aberta;
    ESPERA_IGUAL(acervo_grava_meta(hal, &o), OK);
}

// ── CONTINUAR LENDO é a última ABERTA ───────────────────────────────
void t_acervo_vista_continuar_lendo_e_a_ultima_aberta(void)
{
    COMECA("acervo · Continuar lendo é a última obra ABERTA");

    liga();
    poe("ob:a", "Manual de campo", OBRA_AQUI, 1000, 0, (data_t){0,0,0});
    poe("ob:b", "O estrangeiro", OBRA_AQUI, 1000, 380, (data_t){2026, 9, 4});
    poe("ob:c", "Contos escolhidos", OBRA_AQUI, 1000, 120, (data_t){2026, 9, 1});

    ESPERA_IGUAL(uso_carregar_acervo(hal, &ap.estado), OK);

    vista_acervo_t v;
    vista_acervo(&ap.estado, &v);

    ESPERA(v.tem_destaque);
    ESPERA_TEXTO(v.destaque.titulo, "O estrangeiro");

    // A porcentagem é do texto lido; a página vem depois.
    ESPERA_CONTEM(v.destaque.legenda, "38%");
    TERMINA();
}

// ── a legenda diz o tipo e o quanto ─────────────────────────────────
// "Livro" seco para o nunca aberto.
void t_acervo_vista_a_legenda_muda_com_o_progresso(void)
{
    COMECA("acervo · a legenda só mostra porcentagem do que foi começado");

    liga();
    poe("ob:a", "Manual de campo", OBRA_AQUI, 1000, 0, (data_t){0,0,0});
    poe("ob:b", "Contos escolhidos", OBRA_AQUI, 1000, 120, (data_t){2026,9,1});

    ESPERA_IGUAL(uso_carregar_acervo(hal, &ap.estado), OK);

    vista_acervo_t v;
    vista_acervo(&ap.estado, &v);

    ESPERA_IGUAL(v.n, 2);   // sem isto o laço abaixo não assertava nada

    for (int i = 0; i < v.n; i++) {
        if (strcmp(v.linhas[i].titulo, "Manual de campo") == 0)
            ESPERA_TEXTO(v.linhas[i].legenda, "Livro");
        if (strcmp(v.linhas[i].titulo, "Contos escolhidos") == 0)
            ESPERA_TEXTO(v.linhas[i].legenda, "Livro · 12%");
    }
    TERMINA();
}

// ── os três ícones, e o que cada um promete ─────────────────────────
void t_acervo_vista_o_icone_diz_o_que_o_OK_faz(void)
{
    COMECA("acervo · baixar, baixando e aqui têm marcas diferentes");

    liga();
    poe("ob:a", "Só online", OBRA_SO_ONLINE, 1000, 0, (data_t){0,0,0});
    poe("ob:b", "Aqui", OBRA_AQUI, 1000, 0, (data_t){0,0,0});
    obra_t aqui_online;
    ESPERA_IGUAL(acervo_le_meta(hal, "ob:b", &aqui_online), OK);
    aqui_online.no_catalogo = true;
    ESPERA_IGUAL(acervo_grava_meta(hal, &aqui_online), OK);
    poe("ob:d", "Só na memória", OBRA_AQUI, 1000, 0, (data_t){0,0,0});

    obra_t baixando;
    memset(&baixando, 0, sizeof baixando);
    snprintf(baixando.id, sizeof baixando.id, "%s", "ob:c");
    snprintf(baixando.titulo, sizeof baixando.titulo, "%s", "Baixando");
    baixando.estado = OBRA_BAIXANDO;
    baixando.tamanho = 1000;
    baixando.baixado = 640;
    ESPERA_IGUAL(acervo_grava_meta(hal, &baixando), OK);

    ESPERA_IGUAL(uso_carregar_acervo(hal, &ap.estado), OK);

    vista_acervo_t v;
    vista_acervo(&ap.estado, &v);

    ESPERA_IGUAL(v.n, 4);

    for (int i = 0; i < v.n; i++) {
        if (strcmp(v.linhas[i].titulo, "Só online") == 0)
            ESPERA_IGUAL(v.linhas[i].marca, ACERVO_BAIXAR);
        if (strcmp(v.linhas[i].titulo, "Aqui") == 0)
            ESPERA_IGUAL(v.linhas[i].marca, ACERVO_LOCAL);
        if (strcmp(v.linhas[i].titulo, "Só na memória") == 0)
            ESPERA_IGUAL(v.linhas[i].marca, ACERVO_SO_MEMORIA);
        if (strcmp(v.linhas[i].titulo, "Baixando") == 0) {
            ESPERA_IGUAL(v.linhas[i].marca, ACERVO_VINDO);
            // A porcentagem da TRANSFERÊNCIA.
            ESPERA_CONTEM(v.linhas[i].legenda, "64%");
        }
    }
    TERMINA();
}

// ── o acervo vazio diz que está vazio ───────────────────────────────
void t_acervo_vista_vazio_explica(void)
{
    COMECA("acervo · vazio é uma resposta, e não uma tela em branco");

    liga();

    ESPERA_IGUAL(uso_carregar_acervo(hal, &ap.estado), OK);

    vista_acervo_t v;
    vista_acervo(&ap.estado, &v);

    ESPERA_IGUAL(v.n, 0);
    ESPERA(!v.tem_destaque);
    ESPERA_TEXTO(v.vazio, "Seu Acervo está vazio");
    TERMINA();
}

// ── os cinco filtros ────────────────────────────────────────────────
void t_acervo_vista_filtra_sem_perder_o_resto(void)
{
    COMECA("acervo · os cinco filtros olham o acervo inteiro");

    liga();
    poe("ob:a", "Manual de campo", OBRA_AQUI, 1000, 0, (data_t){0,0,0});
    poe("ob:b", "Contos escolhidos", OBRA_AQUI, 1000, 120, (data_t){2026,9,1});

    obra_t doc;
    memset(&doc, 0, sizeof doc);
    snprintf(doc.id, sizeof doc.id, "%s", "ob:c");
    snprintf(doc.titulo, sizeof doc.titulo, "%s", "Notas sobre o tempo");
    doc.tipo = OBRA_DOCUMENTO;
    doc.estado = OBRA_AQUI;
    doc.tamanho = 1000;
    doc.offset_texto = 630;
    ESPERA_IGUAL(acervo_grava_meta(hal, &doc), OK);

    ap.estado.acervo_filtro = ACERVO_SO_DOCUMENTOS;
    ESPERA_IGUAL(uso_carregar_acervo(hal, &ap.estado), OK);

    vista_acervo_t v;
    vista_acervo(&ap.estado, &v);
    ESPERA_IGUAL(v.n, 1);
    ESPERA_TEXTO(v.linhas[0].titulo, "Notas sobre o tempo");

    ap.estado.acervo_filtro = ACERVO_EM_LEITURA;
    vista_acervo(&ap.estado, &v);
    ESPERA_IGUAL(v.n, 2);      // as duas começadas, e não a intocada

    ap.estado.acervo_filtro = ACERVO_TODOS;
    vista_acervo(&ap.estado, &v);
    ESPERA_IGUAL(v.n, 3);
    TERMINA();
}
