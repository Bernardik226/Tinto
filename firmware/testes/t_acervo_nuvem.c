// firmware/testes/t_acervo_nuvem.c — o catálogo que desce e a cópia que
// chega. A LISTA é barata e desce inteira; o TEXTO só ao abrir.
#include "teste.h"
#include "dado/acervo.h"
#include "uso/acervo.h"
#include "uso/nuvem.h"
#include "vista/acervo.h"
#include "nucleo/prazos.h"

static app_t ap;
static const hal_t *hal;

#define HOJE ((data_t){2026, 9, 4})

static void liga_pareado_acervo(void)
{
    hal = pc_liga();
    pc_relogio(HOJE, 9, 0);
    app_liga(&ap, hal);
    ap.estado.rede = REDE_LIGADA;
    ap.estado.tem_token = true;
}

// ── o LOTE desce e vira estante ─────────────────────────────────────
void t_acervo_o_lote_do_servidor_vira_obra_no_cartao(void)
{
    COMECA("acervo · o lote do servidor vira obra no cartão");

    liga_pareado_acervo();

    ESPERA_IGUAL(uso_acervo_sincroniza(hal, &ap.estado), OK);
    ESPERA_CONTEM(pc_nuvem_rota(), "/v1/acervo");

    pc_nuvem_responde(
        "{\"obras\":[{\"id\":\"ob:abc\",\"t\":\"Dom Casmurro\","
        "\"a\":\"Machado\",\"tp\":\"livro\",\"n\":4200,\"aqui\":false},"
        "{\"id\":\"ob:xyz\",\"t\":\"Contrato\",\"tp\":\"documento\","
        "\"n\":900,\"aqui\":false}],\"cursor\":\"\"}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    obra_t lista[OBRAS_MAX];
    int n = 0;
    ESPERA_IGUAL(acervo_lista(hal, lista, OBRAS_MAX, &n), OK);
    ESPERA_IGUAL(n, 2);

    obra_t o;
    ESPERA_IGUAL(acervo_le_meta(hal, "ob:abc", &o), OK);
    ESPERA_TEXTO(o.titulo, "Dom Casmurro");
    ESPERA_IGUAL(o.estado, OBRA_SO_ONLINE);   // existe lá, não está aqui
    ESPERA_IGUAL(o.tamanho, 4200);
    TERMINA();
}

void t_acervo_catalogo_igual_nao_regrava_o_cartao(void)
{
    COMECA("acervo · catálogo igual não regrava o cartão");
    liga_pareado_acervo();
    const char *json = "{\"obras\":[{\"id\":\"ob:igual\",\"t\":\"Livro\","
                       "\"a\":\"Autor\",\"s\":\"Sinopse\",\"tp\":\"livro\","
                       "\"n\":4200,\"aqui\":false}],\"cursor\":\"\"}";
    ESPERA_IGUAL(uso_acervo_aplica(hal, &ap.estado, json), OK);
    int escritas = pc_escritas();
    ESPERA_IGUAL(uso_acervo_aplica(hal, &ap.estado, json), OK);
    ESPERA_IGUAL(pc_escritas(), escritas);
    TERMINA();
}

void t_acervo_a_capa_desce_depois_do_texto(void)
{
    COMECA("acervo · a capa preparada desce depois do texto");
    liga_pareado_acervo();

    ESPERA_IGUAL(uso_acervo_sincroniza(hal, &ap.estado), OK);
    pc_nuvem_responde("{\"obras\":[{\"id\":\"ob:capa\",\"t\":\"Livro\","
                      "\"tp\":\"livro\",\"n\":12,\"c\":true}],\"cursor\":\"\"}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    ESPERA_IGUAL(uso_acervo_baixa(hal, &ap.estado, "ob:capa"), OK);
    pc_nuvem_responde("012345678901");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_CONTEM(pc_nuvem_rota(), "/capa/grande");

    static char hex[14001];
    memset(hex, '0', 14000); hex[14000] = '\0';
    pc_nuvem_responde(hex);
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_IGUAL(ap.estado.capa_baixada, 7000);
    ESPERA_IGUAL(ap.estado.nuvem_esperando, NUVEM_CAPA);
    memset(hex, '0', 7600); hex[7600] = '\0';
    pc_nuvem_responde(hex);
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_CONTEM(pc_nuvem_rota(), "/capa/mini");

    memset(hex, '0', 490); hex[490] = '\0';
    pc_nuvem_responde(hex);
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_CONTEM(pc_nuvem_rota(), "/capa/destaque");

    memset(hex, '0', 756); hex[756] = '\0';
    pc_nuvem_responde(hex);
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_IGUAL(ap.estado.capa_baixada, 0);

    obra_t o;
    ESPERA_IGUAL(acervo_le_meta(hal, "ob:capa", &o), OK);
    ESPERA(o.capa_aqui);
    uint8_t capa[10800];
    ESPERA_IGUAL(acervo_le_capa(hal, "ob:capa", capa, sizeof capa), OK);
    uint8_t mini[245];
    ESPERA_IGUAL(acervo_le_capa_mini(hal, "ob:capa", mini, sizeof mini), OK);
    uint8_t destaque[378];
    ESPERA_IGUAL(acervo_le_capa_destaque(hal, "ob:capa", destaque,
                                         sizeof destaque), OK);
    TERMINA();
}

// ── a obra que saiu do acervo CONTINUA aqui ─────────────────────────
// A cópia é da pessoa; tirar do catálogo não entra no cartão.
void t_acervo_obra_removida_online_continua_local(void)
{
    COMECA("acervo · removida no PWA, a cópia baixada continua aqui");

    liga_pareado_acervo();

    obra_t minha;
    memset(&minha, 0, sizeof minha);
    snprintf(minha.id,     sizeof minha.id,     "%s", "ob:abc");
    snprintf(minha.titulo, sizeof minha.titulo, "%s", "Dom Casmurro");
    minha.estado = OBRA_AQUI;
    minha.tamanho = minha.baixado = 4200;
    ESPERA_IGUAL(acervo_grava_meta(hal, &minha), OK);

    // O lote seguinte não traz mais essa obra.
    ESPERA_IGUAL(uso_acervo_sincroniza(hal, &ap.estado), OK);
    pc_nuvem_responde("{\"obras\":[],\"cursor\":\"\"}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    obra_t lida;
    ESPERA_IGUAL(acervo_le_meta(hal, "ob:abc", &lida), OK);
    ESPERA_IGUAL(lida.estado, OBRA_AQUI);
    TERMINA();
}

void t_acervo_obra_apenas_online_some_com_o_catalogo(void)
{
    COMECA("acervo · removida no PWA, a obra não baixada some do Tinto");

    liga_pareado_acervo();

    obra_t remota;
    memset(&remota, 0, sizeof remota);
    snprintf(remota.id, sizeof remota.id, "%s", "ob:remota");
    snprintf(remota.titulo, sizeof remota.titulo, "%s", "Ainda na nuvem");
    remota.estado = OBRA_SO_ONLINE;
    remota.tamanho = 4200;
    ESPERA_IGUAL(acervo_grava_meta(hal, &remota), OK);

    ESPERA_IGUAL(uso_acervo_sincroniza(hal, &ap.estado), OK);
    pc_nuvem_responde("{\"obras\":[],\"cursor\":\"\"}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    obra_t lida;
    ESPERA(acervo_le_meta(hal, "ob:remota", &lida) != OK);
    TERMINA();
}

void t_acervo_percorre_todos_os_lotes_antes_de_remover(void)
{
    COMECA("acervo · o catálogo percorre os lotes antes de remover ausentes");

    liga_pareado_acervo();
    ESPERA_IGUAL(uso_acervo_sincroniza(hal, &ap.estado), OK);

    pc_nuvem_responde("{\"obras\":[{\"id\":\"ob:primeira\",\"t\":\"Primeira\","
                      "\"tp\":\"livro\",\"n\":100}],\"cursor\":\"ob:segunda\"}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    // O cursor dispara o próximo lote antes de o catálogo terminar.
    ESPERA_CONTEM(pc_nuvem_rota(), "cursor=ob:segunda");
    ESPERA_IGUAL(ap.estado.nuvem_esperando, NUVEM_ACERVO);

    pc_nuvem_responde("{\"obras\":[{\"id\":\"ob:segunda\",\"t\":\"Segunda\","
                      "\"tp\":\"livro\",\"n\":100}],\"cursor\":\"\"}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    obra_t o;
    ESPERA_IGUAL(acervo_le_meta(hal, "ob:primeira", &o), OK);
    ESPERA_IGUAL(acervo_le_meta(hal, "ob:segunda", &o), OK);
    ESPERA_IGUAL(ap.estado.nuvem_esperando, NUVEM_NADA);
    TERMINA();
}

// ── o download só vira obra se chegar INTEIRO ───────────────────────
void t_acervo_o_texto_so_vira_obra_se_chegar_inteiro(void)
{
    COMECA("acervo · o texto só vira obra depois de conferido");

    liga_pareado_acervo();

    obra_t o;
    memset(&o, 0, sizeof o);
    snprintf(o.id,     sizeof o.id,     "%s", "ob:abc");
    snprintf(o.titulo, sizeof o.titulo, "%s", "Dom Casmurro");
    o.estado = OBRA_SO_ONLINE;
    o.tamanho = 12;
    ESPERA_IGUAL(acervo_grava_meta(hal, &o), OK);

    ESPERA_IGUAL(uso_acervo_baixa(hal, &ap.estado, "ob:abc"), OK);
    ESPERA_CONTEM(pc_nuvem_rota(), "/conteudo");

    // Chega menos do que o declarado: não vira obra.
    pc_nuvem_responde("curto");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    obra_t lida;
    ESPERA_IGUAL(acervo_le_meta(hal, "ob:abc", &lida), OK);
    ESPERA(lida.estado != OBRA_AQUI);

    // O segundo bloco continua do offset, sem recomeçar.
    ESPERA_CONTEM(pc_nuvem_rota(), "offset=5");
    ESPERA_IGUAL(uso_acervo_recebe(hal, &ap.estado, "6789012"), OK);

    ESPERA_IGUAL(acervo_le_meta(hal, "ob:abc", &lida), OK);
    ESPERA_IGUAL(lida.estado, OBRA_AQUI);
    ESPERA_IGUAL(lida.baixado, 12);
    TERMINA();
}

void t_acervo_baixa_em_blocos_sem_prender_a_navegacao(void)
{
    COMECA("acervo · texto grande desce em blocos sem prender a navegação");
    liga_pareado_acervo();

    obra_t o;
    memset(&o, 0, sizeof o);
    snprintf(o.id, sizeof o.id, "%s", "ob:grande");
    snprintf(o.titulo, sizeof o.titulo, "%s", "Livro grande");
    o.estado = OBRA_SO_ONLINE;
    o.tamanho = 18;
    ESPERA_IGUAL(acervo_grava_meta(hal, &o), OK);

    ESPERA_IGUAL(uso_acervo_baixa(hal, &ap.estado, o.id), OK);
    ESPERA_CONTEM(pc_nuvem_rota(), "offset=0");

    ESPERA_IGUAL(uso_acervo_recebe(hal, &ap.estado, "primeiro-"), OK);
    ESPERA_CONTEM(pc_nuvem_rota(), "offset=9");
    ESPERA_IGUAL(uso_acervo_recebe(hal, &ap.estado, "segundo--"), OK);

    obra_t lida;
    ESPERA_IGUAL(acervo_le_meta(hal, o.id, &lida), OK);
    ESPERA_IGUAL(lida.estado, OBRA_AQUI);
    ESPERA_IGUAL(lida.baixado, 18);
    char texto[32];
    ESPERA_IGUAL(acervo_le_texto(hal, o.id, texto, sizeof texto), OK);
    ESPERA_TEXTO(texto, "primeiro-segundo--");
    TERMINA();
}

void t_acervo_o_clique_marca_baixando_na_mesma_hora(void)
{
    COMECA("acervo · o clique dá feedback antes da rede responder");
    liga_pareado_acervo();

    obra_t o;
    memset(&o, 0, sizeof o);
    snprintf(o.id, sizeof o.id, "%s", "ob:agora");
    snprintf(o.titulo, sizeof o.titulo, "%s", "Resposta imediata");
    o.estado = OBRA_SO_ONLINE;
    o.tamanho = 32000;
    o.no_catalogo = true;
    ESPERA_IGUAL(acervo_grava_meta(hal, &o), OK);
    ESPERA_IGUAL(uso_carregar_acervo(hal, &ap.estado), OK);

    ESPERA_IGUAL(uso_acervo_baixa(hal, &ap.estado, o.id), OK);
    ESPERA_IGUAL(ap.estado.acervo[0].estado, OBRA_BAIXANDO);
    ESPERA_IGUAL(ap.estado.acervo[0].baixado, 0);
    vista_acervo_t v;
    vista_acervo(&ap.estado, &v);
    ESPERA_CONTEM(v.linhas[0].legenda, "Preparando");
    TERMINA();
}

void t_acervo_retoma_o_part_depois_de_reiniciar(void)
{
    COMECA("acervo · o retry retoma o part salvo em vez de zerar");
    liga_pareado_acervo();
    obra_t o;
    memset(&o, 0, sizeof o);
    snprintf(o.id, sizeof o.id, "%s", "ob:retoma");
    o.estado = OBRA_BAIXANDO; o.tamanho = 12; o.baixado = 5;
    ESPERA_IGUAL(acervo_grava_meta(hal, &o), OK);
    ESPERA_IGUAL(acervo_grava_texto(hal, o.id, "curto", false), OK);

    ESPERA_IGUAL(uso_acervo_baixa(hal, &ap.estado, o.id), OK);
    ESPERA_CONTEM(pc_nuvem_rota(), "offset=5");
    ESPERA_IGUAL(uso_acervo_recebe(hal, &ap.estado, "6789012"), OK);
    char texto[20];
    ESPERA_IGUAL(acervo_le_texto(hal, o.id, texto, sizeof texto), OK);
    ESPERA_TEXTO(texto, "curto6789012");
    TERMINA();
}

void t_acervo_reiniciar_retomara_depois_de_confirmar_catalogo(void)
{
    COMECA("acervo · reboot retoma sozinho depois do catálogo");
    liga_pareado_acervo();

    obra_t o;
    memset(&o, 0, sizeof o);
    snprintf(o.id, sizeof o.id, "%s", "ob:reboot");
    o.estado = OBRA_BAIXANDO; o.tamanho = 12; o.baixado = 5;
    ESPERA_IGUAL(acervo_grava_meta(hal, &o), OK);
    ESPERA_IGUAL(acervo_grava_texto(hal, o.id, "curto", false), OK);

    // O boot limpa a RAM; o cartão guarda a retomada.
    app_liga(&ap, hal);
    ap.estado.rede = REDE_LIGADA;
    ESPERA_IGUAL(uso_acervo_sincroniza(hal, &ap.estado), OK);
    pc_nuvem_responde("{\"obras\":[{\"id\":\"ob:reboot\",\"t\":\"Livro\","
                      "\"tp\":\"livro\",\"n\":12}],\"cursor\":\"\"}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_CONTEM(pc_nuvem_rota(), "offset=5");
    TERMINA();
}

void t_acervo_sync_nao_pisca_o_estado_online(void)
{
    COMECA("acervo · sincronizar mantém a marca confirmada até o fim");
    liga_pareado_acervo();

    obra_t o = {0};
    snprintf(o.id, sizeof o.id, "%s", "ob:estavel");
    snprintf(o.titulo, sizeof o.titulo, "%s", "Estado estável");
    o.estado = OBRA_AQUI;
    o.no_catalogo = true;
    o.tamanho = o.baixado = 10;
    ESPERA_IGUAL(acervo_grava_meta(hal, &o), OK);
    ESPERA_IGUAL(uso_carregar_acervo(hal, &ap.estado), OK);

    ESPERA_IGUAL(uso_acervo_sincroniza(hal, &ap.estado), OK);
    vista_acervo_t v;
    vista_acervo(&ap.estado, &v);
    ESPERA_IGUAL(v.linhas[0].marca, ACERVO_LOCAL);
    TERMINA();
}

// ── sem espaço, não começa ──────────────────────────────────────────
void t_acervo_cartao_cheio_nao_comeca_download(void)
{
    COMECA("acervo · cartão cheio recusa o download antes de tentar");

    liga_pareado_acervo();

    obra_t o;
    memset(&o, 0, sizeof o);
    snprintf(o.id, sizeof o.id, "%s", "ob:abc");
    o.tamanho = 4200;
    ESPERA_IGUAL(acervo_grava_meta(hal, &o), OK);

    pc_memoria_estado(MEMORIA_CHEIA);
    pc_nuvem_rota_zera();
    ESPERA(uso_acervo_baixa(hal, &ap.estado, "ob:abc") != OK);
    ESPERA_TEXTO(pc_nuvem_rota(), "");
    pc_memoria_estado(MEMORIA_PRONTA);
    TERMINA();
}

// ── o `.part` sobrevive ao reboot e continua não sendo obra ─────────
void t_acervo_o_part_sobrevive_ao_reboot_sem_virar_obra(void)
{
    COMECA("acervo · o download interrompido não vira obra depois do boot");

    liga_pareado_acervo();

    obra_t o;
    memset(&o, 0, sizeof o);
    snprintf(o.id,     sizeof o.id,     "%s", "ob:meio");
    snprintf(o.titulo, sizeof o.titulo, "%s", "Livro pela metade");
    o.estado = OBRA_BAIXANDO;
    o.tamanho = 5000;
    o.baixado = 2000;
    ESPERA_IGUAL(acervo_grava_meta(hal, &o), OK);
    ESPERA_IGUAL(acervo_grava_texto(hal, "ob:meio", "metade do livro",
                                    false), OK);

    // Desliga e liga: o cartão é o mesmo.
    app_liga(&ap, hal);
    ESPERA_IGUAL(uso_carregar_acervo(hal, &ap.estado), OK);

    // Aparece na estante, mas NÃO como cópia local.
    obra_t lida;
    ESPERA_IGUAL(acervo_le_meta(hal, "ob:meio", &lida), OK);
    ESPERA(lida.estado != OBRA_AQUI);

    // E o leitor não tem o que abrir.
    char texto[64];
    ESPERA(acervo_le_texto(hal, "ob:meio", texto, sizeof texto) != OK);
    TERMINA();
}

// ── o servidor responde com erro ───────────────────────────────────
void t_acervo_erro_do_servidor_solta_a_obra_na_hora(void)
{
    COMECA("acervo · erro do servidor solta a obra sem esperar o prazo");

    liga_pareado_acervo();

    obra_t o;
    memset(&o, 0, sizeof o);
    snprintf(o.id, sizeof o.id, "%s", "ob:erro");
    snprintf(o.titulo, sizeof o.titulo, "%s", "Quinhentos");
    o.estado = OBRA_SO_ONLINE;
    o.tamanho = 500;
    ESPERA_IGUAL(acervo_grava_meta(hal, &o), OK);

    pc_nuvem_demora(true);
    ESPERA_IGUAL(uso_acervo_baixa(hal, &ap.estado, "ob:erro"), OK);

    // O que volta é vazio, como um 500 chega aqui.
    pc_nuvem_responde_pull(NULL);   // e a outra linha também está muda
    pc_nuvem_responde_ja(NULL);
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), ERR_REDE);
    pc_nuvem_demora(false);

    // O erro já é a resposta: nada de esperar o prazo.
    ESPERA(!ap.estado.obra_baixando[0]);
    ESPERA_IGUAL((int)ap.estado.sinc, (int)SINC_ERRO);
    TERMINA();
}

// ── a rede cai no meio do download ─────────────────────────────────
void t_acervo_a_rede_cair_no_meio_nao_deixa_obra_falsa(void)
{
    COMECA("acervo · a rede cair no meio não deixa uma obra que abre");

    liga_pareado_acervo();

    obra_t o;
    memset(&o, 0, sizeof o);
    snprintf(o.id, sizeof o.id, "%s", "ob:corta");
    snprintf(o.titulo, sizeof o.titulo, "%s", "Interrompido");
    o.estado = OBRA_SO_ONLINE;
    o.tamanho = 500;
    ESPERA_IGUAL(acervo_grava_meta(hal, &o), OK);

    // A resposta nunca é entregue.
    pc_nuvem_demora(true);
    ESPERA_IGUAL(uso_acervo_baixa(hal, &ap.estado, "ob:corta"), OK);

    // O aparelho descobre pelo prazo.
    for (int i = 0; i < (int)(ESTRUTURA_PRAZO_MS / 1000u) + 3; i++) {
        pc_tick();
        pc_avanca_ms(1000);
        app_passo(&ap);
    }
    pc_nuvem_demora(false);

    obra_t lida;
    ESPERA_IGUAL(acervo_le_meta(hal, "ob:corta", &lida), OK);
    ESPERA(lida.estado != OBRA_AQUI);      // meia obra não abre
    ESPERA(!ap.estado.obra_baixando[0]);   // e o próximo texto não cai nela
    TERMINA();
}
