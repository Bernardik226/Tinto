// firmware/testes/t_nuvem.c — o ciclo com o backend, sem backend.
// Nada bloqueia: o aparelho pede e a resposta chega como evento. O harness
// responde no lugar do servidor.
#include "teste.h"
#include "vista/dia.h"
#include "uso/uso.h"
#include "vista/agenda.h"
#include "uso/nuvem.h"
#include "dado/cartao.h"
#include "dado/nuvem.h"
#include "dado/json.h"
#include "dado/perfil.h"
#include "vista/nota.h"
#include "vista/anotacoes.h"
#include "vista/conta.h"
#include "vista/vincular.h"
#include "vista/campos.h"

static app_t ap;
static const hal_t *hal;

#define HOJE ((data_t){2026, 8, 21})

static void liga_pareado(void)
{
    hal = pc_liga();
    pc_relogio(HOJE, 10, 2);
    app_liga(&ap, hal);
    app_passo(&ap);

    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", "Convidado");
    ap.estado.rede = REDE_LIGADA;
    ap.estado.tem_token = true;
}

// O pull é a única via de entrada: grava no cartão e derruba o cache.
void t_o_pull_grava_no_cartao_e_derruba_o_cache(void)
{
    COMECA("sync · o que o delta traz vai pro cartão, e o cache cai");

    liga_pareado();
    ap.estado.itens_validos = true;

    ESPERA_IGUAL(uso_sincronizar(hal, &ap.estado), OK);
    ESPERA_IGUAL(vista_sinc_da_barra(&ap.estado), ICO_NENHUM);
    pc_nuvem_responde(
        "{\"itens\":[{\"id\":\"g:a1\",\"t\":\"Dentista\",\"h\":\"14:00\","
        "\"d\":\"2026-08-21\",\"o\":\"g\"}],\"removidos\":[],"
        "\"cursor\":\"c-77\",\"quota_usados\":720,\"quota_limite\":1800}");

    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, HOJE, "g:a1", &lido), OK);
    ESPERA_TEXTO(lido.titulo, "Dentista");
    ESPERA(!ap.estado.itens_validos);
    ESPERA_TEXTO(ap.estado.cursor_pull, "c-77");

    // RN-52: a quota vem de carona; o aparelho só exibe.
    ESPERA_IGUAL(ap.estado.quota.usados_s, 720);
    ESPERA_IGUAL(ap.estado.quota.limite_s, 1800);

    TERMINA();
}

// O código de pareamento CHEGA, não se inventa (T-27a).
void t_o_codigo_de_pareamento_chega_do_servidor(void)
{
    COMECA("T-27a · o código vem do servidor, e a conta também");

    liga_pareado();
    ap.estado.nome[0] = '\0';

    ESPERA_IGUAL(uso_parear(hal, &ap.estado), OK);
    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/parear/iniciar");

    pc_nuvem_responde("{\"codigo\":\"KXPT4M\",\"vale_s\":540}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_TEXTO(ap.estado.codigo, "KXPT4M");

    ap.estado.nuvem_esperando = NUVEM_PAREADO;
    pc_nuvem_responde("{\"pareado\":true,\"conta\":\"Convidado\"}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_TEXTO(ap.estado.nome, "Convidado");
    ESPERA_TEXTO(ap.estado.codigo, "");

    TERMINA();
}

// Sem rede não se tenta: a chamada nasceria morta.
void t_sem_rede_nao_se_tenta_sincronizar(void)
{
    COMECA("sync · sem rede o aparelho nem tenta, e diz por quê");

    liga_pareado();
    ap.estado.rede = REDE_DESLIGADA;

    ESPERA_IGUAL(uso_sincronizar(hal, &ap.estado), ERR_REDE);

    TERMINA();
}

// Quem nunca falou com o servidor diz "nunca".
void t_nunca_falou_diz_nunca(void)
{
    COMECA("Conexões · quem nunca falou com o servidor diz nunca");

    liga_pareado();
    ap.estado.nuvem_ja_falou = false;

    vista_cartao_t v;
    vista_conta(&ap.estado, &v);

    // O fato tem de existir: antes o teste não achava a linha e passava sem
    // comparar nada.
    bool achou = false;
    for (int i = 0; i < v.n_fatos; i++)
        if (strstr(v.fatos[i].rotulo, "sincronia")) {
            achou = true;
            ESPERA_TEXTO(v.fatos[i].valor, "nunca");
        }
    ESPERA(achou);

    TERMINA();
}

// O fuso vem do Google Agenda (o fuso da conta, onde os eventos foram
// marcados), não de geolocalização: sem TZ o vidro mostrava UTC.
void t_o_pull_traz_o_fuso_da_conta(void)
{
    COMECA("o fuso vem no pull, é aplicado na hora e sobrevive ao reboot");

    liga_pareado();

    ESPERA_IGUAL(uso_sincronizar(hal, &ap.estado), OK);
    pc_nuvem_responde("{\"itens\":[],\"removidos\":[],\"tz_min\":-180}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    // Guardado no ajuste E aplicado no hal.
    ESPERA_IGUAL(ap.estado.config.valor[AJUSTE_FUSO_MIN], -180);
    ESPERA_IGUAL(pc_fuso(), -180);

    // Valor absurdo é APARADO, não recusado (RN-B7).
    ESPERA_IGUAL(uso_sincronizar(hal, &ap.estado), OK);
    pc_nuvem_responde("{\"itens\":[],\"removidos\":[],\"tz_min\":99999}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_IGUAL(ap.estado.config.valor[AJUSTE_FUSO_MIN], 840);
    TERMINA();
}

// A captura: a IA PROPÕE, e nada toca o cartão até o OK.
void t_a_captura_volta_com_as_acoes_propostas(void)
{
    COMECA("a resposta da captura vira as ações que Conferir mostra");

    liga_pareado();

    ESPERA_IGUAL(uso_enviar_captura(hal, &ap.estado,
                                    "/TINTO/itens/2026-08-25/1422/a.wav"), OK);
    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/captura");

    // Sobe o ÁUDIO, não o caminho: o backend não alcança o cartão.
    ESPERA_CONTEM(pc_nuvem_audio(), "a.wav");
    ESPERA_TEXTO(pc_nuvem_corpo(), "");

    // O id da operação é o arquivo: a mesma fala duas vezes não cria dois
    // eventos.
    ESPERA_CONTEM(pc_nuvem_operacao(), "a.wav");
    ESPERA_IGUAL(ap.estado.sinc, SINC_ENVIANDO);

    pc_nuvem_responde(
        "{\"falou\":\"marca dentista quinta as tres e lembra de comprar pasta\","
        " \"acoes\":["
        "  {\"v\":\"criou\",\"id\":\"n:1\",\"t\":\"Dentista\",\"h\":\"15:00\","
        "   \"d\":\"2026-08-27\",\"tp\":4},"
        "  {\"v\":\"criou\",\"id\":\"n:2\",\"t\":\"Comprar pasta térmica\","
        "   \"tp\":2}"
        " ]}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    ESPERA_IGUAL(ap.estado.n_resultados, 2);
    ESPERA_TEXTO(ap.estado.resultados[0].item.titulo, "Dentista");
    ESPERA_IGUAL(ap.estado.resultados[0].item.tipo, TIPO_EVENTO);
    ESPERA_IGUAL(ap.estado.resultados[1].item.tipo, TIPO_TAREFA);

    // A frase crua vem junto: sem ela, conferir vira confiar.
    ESPERA(strstr(ap.estado.falou, "dentista") != NULL);

    ESPERA_IGUAL(ap.estado.sinc, SINC_OCIOSO);
    TERMINA();
}

// Fala sem ação não é erro: a frase vem para a tela dizer "não entendi".
void t_captura_sem_acao_ainda_traz_a_frase(void)
{
    COMECA("fala que não virou nada traz a frase crua do mesmo jeito");

    liga_pareado();
    ESPERA_IGUAL(uso_enviar_captura(hal, &ap.estado, "/x.wav"), OK);

    pc_nuvem_responde("{\"falou\":\"hmmm deixa pra la\",\"acoes\":[]}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    ESPERA_IGUAL(ap.estado.n_resultados, 0);
    ESPERA(strstr(ap.estado.falou, "deixa pra la") != NULL);
    TERMINA();
}

void t_o_gesto_sobe_um_por_vez(void)
{
    COMECA("marcar feita sobe UM gesto, com o verbo e o item");

    liga_pareado();

    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,     sizeof it.id,     "%s", "g:a1b2");
    snprintf(it.titulo, sizeof it.titulo, "%s", "Pagar IPVA");
    it.tipo  = TIPO_TAREFA;
    it.feita = true;

    ESPERA_IGUAL(uso_enviar_gesto(hal, &ap.estado, &it, "editou"), OK);
    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/push");
    ESPERA_CONTEM(pc_nuvem_corpo(), "g:a1b2");
    ESPERA_CONTEM(pc_nuvem_corpo(), "editou");
    ESPERA_CONTEM(pc_nuvem_corpo(), "true");

    pc_nuvem_responde("{\"ok\":true}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_IGUAL(ap.estado.sinc, SINC_OCIOSO);
    TERMINA();
}

// O ciclo inteiro, da fala à Agenda mudar, atravessando cinco camadas. Até
// o OK nada toca o cartão (RN-16).
void t_da_fala_ate_a_home_mudar(void)
{
    COMECA("falar, conferir, confirmar: e a home e o calendário mudam");

    liga_pareado();
    const data_t hoje = ap.estado.hoje;

    ESPERA_IGUAL(uso_enviar_captura(hal, &ap.estado, "/x.wav"), OK);

    char corpo[420];
    snprintf(corpo, sizeof corpo,
             "{\"falou\":\"compra pasta termica e marca dentista quinta\","
             " \"acoes\":["
             "  {\"v\":\"criou\",\"id\":\"t:1\",\"t\":\"Comprar pasta\","
             "   \"d\":\"%04d-%02d-%02d\",\"tp\":2},"
             "  {\"v\":\"criou\",\"id\":\"g:2\",\"t\":\"Dentista\","
             "   \"h\":\"15:00\",\"d\":\"%04d-%02d-%02d\",\"tp\":4}"
             " ]}",
             hoje.ano, hoje.mes, hoje.dia,
             hoje.ano, hoje.mes, hoje.dia);
    pc_nuvem_responde(corpo);
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_IGUAL(ap.estado.n_resultados, 2);

    // Até aqui nada tocou o cartão.
    item_t lido;
    ESPERA(cartao_le_item(hal, hoje, "t:1", &lido) != OK);

    ESPERA_IGUAL(uso_confirmar_resultado(hal, &ap.estado), OK);

    ESPERA_IGUAL(cartao_le_item(hal, hoje, "t:1", &lido), OK);
    ESPERA_IGUAL(lido.tipo, TIPO_TAREFA);
    ESPERA_IGUAL(cartao_le_item(hal, hoje, "g:2", &lido), OK);
    ESPERA_IGUAL(lido.tipo, TIPO_EVENTO);

    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/push");

    // A Agenda e o calendário sabem que mudou (RN-3A).
    ESPERA(!ap.estado.itens_validos);
    ESPERA(!ap.estado.marcas_validas);

    ESPERA_IGUAL(uso_carregar_dia(hal, &ap.estado), OK);

    // As tarefas vêm do cache de PENDENTES: relê os dois, como o aparelho.
    ESPERA_IGUAL(uso_carregar_pendentes(hal, &ap.estado), OK);

    vista_agenda_t v;
    vista_agenda(&ap.estado, &v);

    bool achou = false;
    for (int i = 0; i < v.n_trabalho; i++)
        if (strstr(v.trabalho[i].titulo, "pasta")) achou = true;
    ESPERA(achou);

    ESPERA(v.n_agenda > 0);
    ESPERA(strstr(v.agenda[0].oque, "Dentista") != NULL);
    TERMINA();
}

// Descartar não deixa rastro, nem no cartão nem no que ia subir.
void t_descartar_a_proposta_nao_sobe_nada(void)
{
    COMECA("descartar a proposta não grava nem manda nada");

    liga_pareado();
    ESPERA_IGUAL(uso_enviar_captura(hal, &ap.estado, "/x.wav"), OK);
    pc_nuvem_responde("{\"falou\":\"x\",\"acoes\":["
                      "{\"v\":\"criou\",\"id\":\"t:9\",\"t\":\"Nada\",\"tp\":2}]}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_IGUAL(ap.estado.n_resultados, 1);

    ESPERA_IGUAL(uso_descartar_resultado(hal, &ap.estado), OK);

    item_t lido;
    ESPERA(cartao_le_item(hal, ap.estado.hoje, "t:9", &lido) != OK);
    ESPERA_IGUAL(ap.estado.n_resultados, 0);
    ESPERA_TEXTO(ap.estado.falou, "");
    TERMINA();
}

// A fala não atropela o gesto em voo: a resposta do gesto era lida como a
// da captura.
void t_a_captura_espera_a_linha_vagar(void)
{
    COMECA("voz · a fala espera a linha, e não atropela o gesto");

    liga_pareado();

    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,     sizeof it.id,     "%s", "t:1");
    snprintf(it.titulo, sizeof it.titulo, "%s", "Pagar o IPVA");
    it.tipo = TIPO_TAREFA; it.dia = HOJE;
    ESPERA_IGUAL(cartao_grava_item(hal, HOJE, &it), OK);

    pc_nuvem_demora(true);
    ESPERA_IGUAL(uso_enviar_gesto(hal, &ap.estado, &it, "editou"), OK);
    ESPERA_IGUAL(ap.estado.nuvem_esperando, NUVEM_GESTO);

    ESPERA_IGUAL(uso_enviar_captura(hal, &ap.estado,
                                    "/TINTO/itens/2026-09-03/1422/a.wav"), OK);

    ESPERA_IGUAL(ap.estado.nuvem_esperando, NUVEM_GESTO);
    ESPERA_CONTEM(ap.estado.captura_pendente, "a.wav");

    pc_nuvem_demora(false);
    pc_nuvem_responde("{\"ok\":true,\"id\":\"t:1\"}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_IGUAL(uso_captura_pendente(hal, &ap.estado), OK);

    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/captura");
    ESPERA_CONTEM(pc_nuvem_audio(), "a.wav");
    ESPERA(!ap.estado.captura_pendente[0]);
    TERMINA();
}

// O gesto recusado de vez sai da fila E aparece no vidro.
void t_gesto_recusado_avisa_no_vidro(void)
{
    COMECA("o gesto que o servidor recusa aparece na tela");

    liga_pareado();

    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,     sizeof it.id,     "%s", "g:sumiu");
    snprintf(it.titulo, sizeof it.titulo, "%s", "Reunião");
    it.tipo = TIPO_EVENTO; it.dia = HOJE;
    ESPERA_IGUAL(cartao_grava_item(hal, HOJE, &it), OK);

    ESPERA_IGUAL(uso_apagar_item(hal, &ap.estado, &it), OK);
    ESPERA_IGUAL(ap.estado.gesto_n, 1);

    pc_nuvem_responde("{\"ok\":false,\"motivo\":\"http 403\"}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    ESPERA_IGUAL(ap.estado.gesto_n, 0);
    ESPERA_IGUAL(ap.estado.recusa, RECUSA_ACAO);
    ESPERA_IGUAL(ap.estado.sinc, SINC_ERRO);

    // A lápide cai junto: senão esconderia o item para sempre.
    ESPERA(!uso_esta_apagando(&ap.estado, "g:sumiu"));
    TERMINA();
}

void t_gesto_antigo_pede_a_verdade_inteira_do_google(void)
{
    COMECA("conflito · gesto antigo força a cópia local a voltar ao Google");

    liga_pareado();
    ap.estado.tem_google = true;
    ap.estado.pediu_tudo = true;

    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id, sizeof it.id, "%s", "g:mais-novo-no-google");
    snprintf(it.titulo, sizeof it.titulo, "%s", "Reunião");
    it.tipo = TIPO_EVENTO;
    it.origem = ORIGEM_GOOGLE;
    it.dia = HOJE;
    ESPERA_IGUAL(cartao_grava_item(hal, HOJE, &it), OK);
    ESPERA_IGUAL(uso_apagar_item(hal, &ap.estado, &it), OK);

    pc_nuvem_responde(
        "{\"ok\":true,\"id\":\"g:mais-novo-no-google\","
        "\"ignorado\":true}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    ESPERA(!ap.estado.tem_google);
    ESPERA(!ap.estado.pediu_tudo);
    ESPERA_IGUAL(uso_sincronizar(hal, &ap.estado), OK);
    ESPERA_CONTEM(pc_nuvem_rota(), "tudo=1");
    TERMINA();
}

// A voz alcança o que já existe pelo id DE VERDADE: "muda o dentista" não
// cria um segundo.
void t_a_voz_apaga_o_que_ja_existe(void)
{
    COMECA("voz · apagar alcança o item que já está na agenda");

    liga_pareado();

    data_t amanha = data_soma_dias(HOJE, 1);
    item_t ev;
    memset(&ev, 0, sizeof ev);
    snprintf(ev.id,     sizeof ev.id,     "%s", "g:abc");
    snprintf(ev.titulo, sizeof ev.titulo, "%s", "Dentista");
    snprintf(ev.hora,   sizeof ev.hora,   "%s", "15:00");
    ev.tipo = TIPO_EVENTO; ev.origem = ORIGEM_GOOGLE;
    ev.dia = amanha; ev.vence = amanha;
    ESPERA_IGUAL(cartao_grava_item(hal, amanha, &ev), OK);

    ap.estado.nuvem_esperando = NUVEM_CAPTURA;
    pc_nuvem_responde(
        "{\"falou\":\"cancela o dentista\",\"acoes\":["
        " {\"v\":\"apagou\",\"id\":\"g:abc\",\"t\":\"Dentista\","
        "  \"tp\":4}]}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    // Conferir mostra o ANTES: sem ele, "qual dentista?" fica sem resposta.
    ESPERA_IGUAL(ap.estado.n_resultados, 1);
    ESPERA_IGUAL(ap.estado.resultados[0].verbo, RES_APAGOU);
    ESPERA(ap.estado.resultados[0].tem_antes);
    ESPERA(data_igual(ap.estado.resultados[0].antes_vence, amanha));
    ESPERA_TEXTO(ap.estado.resultados[0].antes_hora, "15:00");

    pc_nuvem_historico_zera();
    ESPERA_IGUAL(uso_confirmar_resultado(hal, &ap.estado), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, amanha, "g:abc", &lido), ERR_ARQUIVO);
    ESPERA(strstr(pc_nuvem_historico(), "apagou") != NULL);
    ESPERA(strstr(pc_nuvem_historico(), "g:abc") != NULL);
    TERMINA();
}

// Editar por voz grava na PASTA em que o item mora (RN-26): no dia novo
// seriam duas cópias.
void t_a_voz_edita_sem_duplicar(void)
{
    COMECA("voz · editar muda o item que existe, e não cria outro");

    liga_pareado();

    item_t ev;
    memset(&ev, 0, sizeof ev);
    snprintf(ev.id,     sizeof ev.id,     "%s", "g:abc");
    snprintf(ev.titulo, sizeof ev.titulo, "%s", "Dentista");
    snprintf(ev.hora,   sizeof ev.hora,   "%s", "15:00");
    ev.tipo = TIPO_EVENTO; ev.origem = ORIGEM_GOOGLE;
    ev.dia = HOJE; ev.vence = HOJE;
    ESPERA_IGUAL(cartao_grava_item(hal, HOJE, &ev), OK);

    data_t depois = data_soma_dias(HOJE, 2);
    char texto[DATA_TEXTO];
    data_para_texto(depois, texto, sizeof texto);

    char resposta[300];
    snprintf(resposta, sizeof resposta,
             "{\"falou\":\"muda o dentista\",\"acoes\":["
             " {\"v\":\"editou\",\"id\":\"g:abc\",\"t\":\"Dentista\","
             "  \"h\":\"15:00\",\"d\":\"%s\",\"tp\":4}]}", texto);

    ap.estado.nuvem_esperando = NUVEM_CAPTURA;
    pc_nuvem_responde(resposta);
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_IGUAL(ap.estado.resultados[0].verbo, RES_EDITOU);

    ESPERA_IGUAL(uso_confirmar_resultado(hal, &ap.estado), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, HOJE, "g:abc", &lido), OK);
    ESPERA(data_igual(lido.vence, depois));
    ESPERA_IGUAL(cartao_le_item(hal, depois, "g:abc", &lido), ERR_ARQUIVO);
    TERMINA();
}

// OFFLINE: apagar sem Wi-Fi apaga na hora e sobe quando a rede voltar. O
// difícil é o PULL: o item ainda está no Google e desceria de volta; a
// lápide impede.
void t_apagar_offline_e_o_pull_nao_ressuscita(void)
{
    COMECA("apagar sem rede: some agora, e o pull não traz de volta");

    liga_pareado();

    item_t ev;
    memset(&ev, 0, sizeof ev);
    snprintf(ev.id,     sizeof ev.id,     "%s", "g:abc");
    snprintf(ev.titulo, sizeof ev.titulo, "%s", "Dentista");
    ev.tipo = TIPO_EVENTO; ev.origem = ORIGEM_GOOGLE; ev.dia = HOJE;
    ESPERA_IGUAL(cartao_grava_item(hal, HOJE, &ev), OK);

    ap.estado.rede = REDE_DESLIGADA;
    pc_nuvem_historico_zera();
    ESPERA_IGUAL(uso_apagar_item(hal, &ap.estado, &ev), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, HOJE, "g:abc", &lido), ERR_ARQUIVO);
    ESPERA_IGUAL(ap.estado.gesto_n, 1);
    ESPERA(!strstr(pc_nuvem_historico(), "/v1/push"));

    ap.estado.nuvem_esperando = NUVEM_PULL;
    pc_nuvem_responde("{\"itens\":[{\"id\":\"g:abc\",\"t\":\"Dentista\","
                      "\"tp\":4}]}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_IGUAL(cartao_le_item(hal, HOJE, "g:abc", &lido), ERR_ARQUIVO);

    pc_nuvem_responde("{\"ok\":true,\"id\":\"g:abc\"}");
    ap.estado.rede = REDE_LIGADA;
    pc_avanca_ms(1000);
    pc_tick();
    app_passo(&ap);

    ESPERA(strstr(pc_nuvem_historico(), "apagou") != NULL);
    ESPERA_IGUAL(ap.estado.gesto_n, 0);
    ESPERA(!uso_esta_apagando(&ap.estado, "g:abc"));
    TERMINA();
}

// A fila sobrevive a desligar: senão o primeiro pull do dia ressuscitaria
// o que foi apagado ontem.
void t_a_fila_sobrevive_ao_boot(void)
{
    COMECA("a fila e as lápides voltam depois de desligar");

    liga_pareado();

    item_t ev;
    memset(&ev, 0, sizeof ev);
    snprintf(ev.id,     sizeof ev.id,     "%s", "g:xyz");
    snprintf(ev.titulo, sizeof ev.titulo, "%s", "Reunião");
    ev.tipo = TIPO_EVENTO; ev.origem = ORIGEM_GOOGLE; ev.dia = HOJE;
    ESPERA_IGUAL(cartao_grava_item(hal, HOJE, &ev), OK);

    ap.estado.rede = REDE_DESLIGADA;
    ESPERA_IGUAL(uso_apagar_item(hal, &ap.estado, &ev), OK);
    ESPERA_IGUAL(ap.estado.gesto_n, 1);

    app_liga(&ap, hal);

    ESPERA_IGUAL(ap.estado.gesto_n, 1);
    ESPERA(uso_esta_apagando(&ap.estado, "g:xyz"));

    // A numeração continua: o gesto novo não entra atrás dos que esperam.
    item_t outro;
    memset(&outro, 0, sizeof outro);
    snprintf(outro.id,     sizeof outro.id,     "%s", "t:novo");
    snprintf(outro.titulo, sizeof outro.titulo, "%s", "Comprar pão");
    outro.tipo = TIPO_TAREFA; outro.dia = HOJE;
    ESPERA_IGUAL(cartao_grava_item(hal, HOJE, &outro), OK);
    ap.estado.rede = REDE_DESLIGADA;
    ESPERA_IGUAL(uso_enviar_gesto(hal, &ap.estado, &outro, "editou"), OK);
    ESPERA_IGUAL(ap.estado.gesto_n, 2);

    pc_nuvem_historico_zera();
    pc_nuvem_responde("{\"ok\":true,\"id\":\"g:xyz\"}");
    ap.estado.rede = REDE_LIGADA;
    pc_avanca_ms(1000);
    pc_tick();
    app_passo(&ap);

    const char *h = pc_nuvem_historico();
    const char *primeiro = strstr(h, "apagou");
    const char *segundo  = strstr(h, "editou");
    ESPERA(primeiro != NULL);
    ESPERA(segundo == NULL || primeiro < segundo);
    TERMINA();
}

// Renomear e apagar sobem: o modelo é o do Google (`title`, `summary`).
void t_renomear_e_apagar_sobem_pro_google(void)
{
    COMECA("renomear e apagar viram gesto no Google, não só no cartão");

    liga_pareado();
    const data_t hoje = ap.estado.hoje;

    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,     sizeof it.id,     "%s", "t:abc");
    snprintf(it.titulo, sizeof it.titulo, "%s", "Comprar pasta");
    it.tipo = TIPO_TAREFA;
    it.dia  = hoje;
    cartao_grava_item(hal, hoje, &it);

    pc_nuvem_rota_zera();
    ESPERA_IGUAL(uso_renomear(hal, &ap.estado, &it, "Comprar pasta térmica"), OK);
    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/push");
    ESPERA_CONTEM(pc_nuvem_corpo(), "térmica");
    ESPERA_CONTEM(pc_nuvem_corpo(), "editou");

    // A resposta do renomear vem antes: a fila é ordenada, e o arquivo na
    // linha só sai quando o servidor responde.
    pc_nuvem_responde("{\"ok\":true,\"id\":\"t:abc\"}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    pc_nuvem_rota_zera();
    ESPERA_IGUAL(uso_apagar_item(hal, &ap.estado, &it), OK);
    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/push");
    ESPERA_CONTEM(pc_nuvem_corpo(), "apagou");
    TERMINA();
}

// A ANOTAÇÃO não vai ao Google (RN-25), mas sobe ao nosso servidor.
void t_anotacao_nao_vira_gesto_no_google(void)
{
    COMECA("RN-25 · renomear anotação não manda nada pro Google");

    liga_pareado();
    const data_t hoje = ap.estado.hoje;

    item_t nota;
    memset(&nota, 0, sizeof nota);
    snprintf(nota.id,     sizeof nota.id,     "%s", "1422-ideia");
    snprintf(nota.titulo, sizeof nota.titulo, "%s", "Ideia do painel");
    nota.tipo = TIPO_ANOTACAO;
    nota.dia  = hoje;
    cartao_grava_item(hal, hoje, &nota);

    pc_nuvem_rota_zera();
    ESPERA_IGUAL(uso_renomear(hal, &ap.estado, &nota, "Ideia da dock"), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, hoje, "1422-ideia", &lido), OK);
    ESPERA_TEXTO(lido.titulo, "Ideia da dock");

    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/push");
    TERMINA();
}

// O aparelho se apresenta (`/v1/registrar`): sem isso toda rota dava 401.
void t_o_primeiro_boot_registra_o_aparelho(void)
{
    COMECA("nuvem · o primeiro boot apresenta o aparelho ao servidor");

    liga_pareado();
    pc_nuvem_rota_zera();

    ESPERA_IGUAL(uso_nuvem_ligar(hal, &ap.estado), OK);

    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/registrar");
    ESPERA_CONTEM(pc_nuvem_corpo(), "AA:BB:CC:44:55:66");

    // A PROVA acompanha o id: o MAC se lê da etiqueta.
    ESPERA_CONTEM(pc_nuvem_corpo(), "prova");

    ESPERA_CONTEM(pc_nuvem_servidor(), "https://");
}

void t_o_token_fica_no_cofre_e_vale_no_boot_seguinte(void)
{
    COMECA("nuvem · o token chega uma vez e sobrevive ao desligar");

    liga_pareado();
    ESPERA_IGUAL(uso_nuvem_ligar(hal, &ap.estado), OK);

    pc_nuvem_responde("{\"device_token\":\"tk-abc-123\"}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_TEXTO(pc_nuvem_token(), "tk-abc-123");

    nuvem_cred_t cred;
    ESPERA_IGUAL(nuvem_carrega(hal, &cred), OK);
    ESPERA_TEXTO(cred.token, "tk-abc-123");
    ESPERA(nuvem_registrado(&cred));

    // Segundo boot: não se apresenta de novo, mas pergunta DE QUEM é (`nome`
    // não sobrevive ao desligar).
    ap.estado.registro_tentado = false;  // o segundo boot zera a RAM
    pc_nuvem_rota_zera();
    ESPERA_IGUAL(uso_nuvem_ligar(hal, &ap.estado), OK);
    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/parear/estado");
    ESPERA_TEXTO(pc_nuvem_token(), "tk-abc-123");

    pc_nuvem_rota_zera();
    ESPERA_IGUAL(uso_nuvem_ligar(hal, &ap.estado), OK);
    ESPERA_TEXTO(pc_nuvem_rota(), "");
}

void t_a_prova_nao_muda_entre_boots(void)
{
    COMECA("nuvem · a prova nasce uma vez e fica");

    liga_pareado();

    // Ausente devolve ERR_ARQUIVO com a struct preenchida, e a prova já
    // nasceu.
    nuvem_cred_t um, dois;
    (void)nuvem_carrega(hal, &um);
    ESPERA(um.prova[0] != '\0');
    ESPERA_TEXTO(um.device_id, "AA:BB:CC:44:55:66");

    ESPERA_IGUAL(nuvem_grava(hal, &um), OK);
    ESPERA_IGUAL(nuvem_carrega(hal, &dois), OK);

    // A prova não se regenera: seria trocar de aparelho.
    ESPERA_TEXTO(dois.prova, um.prova);
}

void t_o_gesto_carrega_o_id_da_operacao(void)
{
    COMECA("nuvem · todo gesto que muda dado leva id de operação");

    liga_pareado();

    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id, sizeof it.id, "%s", "t:MTk4Nz");
    snprintf(it.titulo, sizeof it.titulo, "%s", "Pagar IPVA");
    it.tipo = TIPO_TAREFA;
    it.feita = true;

    ESPERA_IGUAL(uso_enviar_gesto(hal, &ap.estado, &it, "editou"), OK);

    // O id de operação torna o retry seguro.
    ESPERA_CONTEM(pc_nuvem_operacao(), "editou");
    ESPERA_CONTEM(pc_nuvem_operacao(), "t:MTk4Nz");
}

// Um aparelho pareado que liga precisa descobrir que tem dono.
void t_ao_ligar_ele_descobre_o_dono_e_puxa_o_dia(void)
{
    COMECA("nuvem · ao ligar ele descobre de quem é, e o dia desce");

    liga_pareado();
    ap.estado.nome[0] = '\0';        // como depois de um desligar

    ESPERA_IGUAL(uso_nuvem_ligar(hal, &ap.estado), OK);
    pc_nuvem_responde("{\"device_token\":\"tk-1\"}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/parear/estado");

    pc_nuvem_rota_zera();
    pc_nuvem_responde("{\"pareado\":true,\"conta\":\"eu@x.com\"}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    ESPERA_TEXTO(ap.estado.nome, "eu@x.com");

    // E o dia desce agora, não no próximo ciclo.
    ESPERA_CONTEM(pc_nuvem_rota(), "/v1/pull");
}

// Desconectar avisa o servidor E limpa aqui, nessa ordem.
void t_desvincular_avisa_o_servidor_antes_de_esquecer(void)
{
    COMECA("nuvem · desconectar avisa o servidor antes de esquecer");

    liga_pareado();
    ap.estado.quota.limite_s = 30 * 60;

    ESPERA_IGUAL(uso_desvincular(hal, &ap.estado), OK);
    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/desparear");

    // A resposta é que decide.
    ESPERA_TEXTO(ap.estado.nome, "Convidado");

    pc_nuvem_responde("{\"ok\":true,\"pareado\":false}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    ESPERA_TEXTO(ap.estado.nome, "");
    ESPERA_IGUAL(ap.estado.quota.limite_s, 0);
}

// Restaurar desvincula antes de formatar: senão o próximo dono via a
// conta do anterior.
void t_restaurar_desvincula_antes_de_formatar(void)
{
    COMECA("restaurar · com rede, desvincula a conta antes de formatar");

    liga_pareado();
    ap.estado.inicio.fase   = INICIO_CONFIRMAR_FORMATAR;
    ap.estado.inicio.cursor = 1;
    ap.estado.inicio.formatar_de_ajustes = true;
    pc_nuvem_rota_zera();
    pc_nuvem_demora(true);

    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/desparear");
    ESPERA(!pc_rede_esquecida());          // ainda esperando o servidor

    pc_nuvem_demora(false);
    pc_nuvem_responde("{\"ok\":true,\"pareado\":false}");
    pc_nuvem_entrega();
    app_passo(&ap);
    app_passo(&ap);

    ESPERA(pc_rede_esquecida());
    ESPERA(pc_reiniciou());
    ESPERA_TEXTO(ap.estado.nome, "");
}

// T-32: a lista de agendas chega e enche a tela (antes ninguém chamava
// `/v1/agendas`).
void t_as_agendas_chegam_e_enchem_a_tela(void)
{
    COMECA("T-32 · a lista chega do servidor e vira linhas");

    liga_pareado();

    ESPERA_IGUAL(uso_agendas(hal, &ap.estado), OK);
    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/agendas");

    // Enquanto não volta, a tela diz BUSCANDO, não "nenhuma agenda".
    vista_cartao_t v;
    vista_sincronizacao(&ap.estado, &v);
    ESPERA_CONTEM(v.nome, "Buscando");

    pc_nuvem_responde(
        "{\"agendas\":["
        "{\"id\":\"convidado@exemplo.com\",\"t\":\"Convidado\",\"on\":true,\"py\":0},"
        "{\"id\":\"pt-br#holiday\",\"t\":\"Feriados no Brasil\","
        "\"on\":false,\"py\":14}]}");
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.n_agendas, 2);
    ESPERA_TEXTO(ap.estado.agendas[0].nome, "Convidado");
    ESPERA(ap.estado.agendas[0].ligada);
    ESPERA_TEXTO(ap.estado.agendas[1].nome, "Feriados no Brasil");
    ESPERA(!ap.estado.agendas[1].ligada);

    // "por ano" explica por que ela vem desligada.
    vista_sincronizacao(&ap.estado, &v);
    // Só as agendas são paradas de cursor.
    ESPERA_IGUAL(v.n_dest, 2);
    ESPERA_IGUAL(v.dest[0].ico, ICO_CAIXA_ON);
    // O número mora na legenda, não na coluna do valor.
    ESPERA_CONTEM(v.dest[1].sub, "14 por ano");

    TERMINA();
}

// O interruptor vira quando o SERVIDOR confirma, não no gesto.
void t_o_interruptor_vira_quando_o_servidor_confirma(void)
{
    COMECA("T-32 · o interruptor vira na confirmação, não no gesto");

    liga_pareado();
    (void)uso_agendas(hal, &ap.estado);
    pc_nuvem_responde(
        "{\"agendas\":[{\"id\":\"h\",\"t\":\"Feriados\",\"on\":true,\"py\":14}]}");
    app_passo(&ap);
    ESPERA(ap.estado.agendas[0].ligada);

    ESPERA_IGUAL(uso_escolher_agenda(hal, &ap.estado, 0, false), OK);
    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/agendas");
    // Pelo NOME (o id do Google é um e-mail de 60 caracteres).
    ESPERA_CONTEM(pc_nuvem_corpo(), "\"t\":\"Feriados\"");
    ESPERA_CONTEM(pc_nuvem_corpo(), "\"on\":false");

    // Ainda ligada: o servidor não respondeu.
    ESPERA(ap.estado.agendas[0].ligada);

    pc_nuvem_responde("{\"ok\":true,\"id\":\"h\",\"on\":false}");
    app_passo(&ap);

    ESPERA(!ap.estado.agendas[0].ligada);

    TERMINA();
}

// Desligar uma agenda apaga o que ela já tinha trazido: o servidor manda
// `zerar` e o lote seguinte é a verdade inteira.
void t_desligar_uma_agenda_apaga_o_que_ela_ja_tinha_trazido(void)
{
    COMECA("T-32 · desligar apaga do cartão o que aquela agenda trouxe");

    liga_pareado();

    item_t feriado;
    memset(&feriado, 0, sizeof feriado);
    snprintf(feriado.id, sizeof feriado.id, "%s", "g:natal");
    snprintf(feriado.titulo, sizeof feriado.titulo, "%s", "Natal");
    feriado.tipo = TIPO_EVENTO;
    feriado.origem = ORIGEM_GOOGLE;
    feriado.dia = HOJE;
    ESPERA_IGUAL(cartao_grava_item(hal, HOJE, &feriado), OK);

    item_t minha;
    memset(&minha, 0, sizeof minha);
    snprintf(minha.id, sizeof minha.id, "%s", "n:ideia");
    snprintf(minha.titulo, sizeof minha.titulo, "%s", "ideia do encoder");
    minha.tipo = TIPO_ANOTACAO;
    minha.origem = ORIGEM_AQUI;
    minha.dia = HOJE;
    ESPERA_IGUAL(cartao_grava_item(hal, HOJE, &minha), OK);

    // Nasceu por voz mas ganhou id Google: a autoridade é de lá.
    item_t falado;
    memset(&falado, 0, sizeof falado);
    snprintf(falado.id, sizeof falado.id, "%s", "g:falado");
    snprintf(falado.titulo, sizeof falado.titulo, "%s", "Evento falado");
    falado.tipo = TIPO_EVENTO;
    falado.origem = ORIGEM_AQUI;
    falado.dia = HOJE;
    ESPERA_IGUAL(cartao_grava_item(hal, HOJE, &falado), OK);

    (void)uso_sincronizar(hal, &ap.estado);
    pc_nuvem_responde("{\"itens\":[],\"removidos\":[],\"zerar\":true,"
                      "\"tz_min\":-180}");
    app_passo(&ap);

    item_t volta;
    ESPERA(cartao_le_item(hal, HOJE, "g:natal", &volta) != OK);
    // O confirmado também sai: o Google não o devolveu.
    ESPERA(cartao_le_item(hal, HOJE, "g:falado", &volta) != OK);
    // O que nasceu aqui fica.
    ESPERA_IGUAL(cartao_le_item(hal, HOJE, "n:ideia", &volta), OK);
    ESPERA_TEXTO(volta.titulo, "ideia do encoder");

    TERMINA();
}

// Sem `zerar`, nada é apagado.
void t_um_pull_comum_nao_apaga_nada(void)
{
    COMECA("pull · sem `zerar`, o delta não toca no que já está lá");

    liga_pareado();

    item_t ev;
    memset(&ev, 0, sizeof ev);
    snprintf(ev.id, sizeof ev.id, "%s", "g:dentista");
    snprintf(ev.titulo, sizeof ev.titulo, "%s", "Dentista");
    ev.tipo = TIPO_EVENTO;
    ev.origem = ORIGEM_GOOGLE;
    ev.dia = HOJE;
    ESPERA_IGUAL(cartao_grava_item(hal, HOJE, &ev), OK);

    (void)uso_sincronizar(hal, &ap.estado);
    pc_nuvem_responde("{\"itens\":[],\"removidos\":[],\"tz_min\":-180}");
    app_passo(&ap);

    item_t volta;
    ESPERA_IGUAL(cartao_le_item(hal, HOJE, "g:dentista", &volta), OK);

    TERMINA();
}

// Sem resposta do servidor, ele tenta se registrar de novo sozinho.
void t_o_tinto_sem_token_tenta_de_novo_ate_se_registrar(void)
{
    COMECA("registro · sem resposta, ele tenta de novo até entrar");

    hal = pc_liga();
    pc_relogio(HOJE, 10, 2);
    app_liga(&ap, hal);
    app_passo(&ap);
    ap.estado.rede = REDE_LIGADA;

    (void)uso_nuvem_ligar(hal, &ap.estado);
    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/registrar");
    ESPERA(ap.estado.meu_id[0] != '\0');

    pc_nuvem_codigo(503);
    pc_nuvem_responde("");
    app_passo(&ap);
    ESPERA(!ap.estado.tem_token);

    // A resposta é armada antes do retry: o harness entrega no mesmo passo.
    pc_nuvem_rota_zera();
    pc_nuvem_codigo(200);
    pc_nuvem_responde("{\"device_token\":\"tok-1\"}");

    pc_avanca_ms(31u * 1000u);
    pc_tick();
    app_passo(&ap);

    ESPERA(ap.estado.tem_token);
    ESPERA_IGUAL(ap.estado.recusa, RECUSA_NADA);

    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/parear/estado");

    TERMINA();
}

// A prova que SOBE e a que fica no cofre são a mesma: sorteadas
// separadamente, todo registro seguinte levaria 401.
void t_a_prova_que_sobe_e_a_que_fica_no_cofre_sao_a_mesma(void)
{
    COMECA("registro · a prova que sobe é a que fica no cartão");

    hal = pc_liga();
    pc_relogio(HOJE, 10, 2);
    app_liga(&ap, hal);
    app_passo(&ap);
    ap.estado.rede = REDE_LIGADA;

    (void)uso_nuvem_ligar(hal, &ap.estado);
    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/registrar");

    char subiu[40] = {0};
    const char *corpo = pc_nuvem_corpo();
    ESPERA(json_str(corpo, "prova", subiu, sizeof subiu));
    ESPERA(subiu[0] != '\0');

    nuvem_cred_t cred;
    (void)nuvem_carrega(hal, &cred);
    ESPERA_TEXTO(cred.prova, subiu);

    pc_nuvem_responde("{\"device_token\":\"tok-1\"}");
    app_passo(&ap);

    nuvem_cred_t depois;
    (void)nuvem_carrega(hal, &depois);
    ESPERA_TEXTO(depois.prova, subiu);
    ESPERA_TEXTO(depois.token, "tok-1");

    TERMINA();
}

// O servidor perdeu o registro (401): a conta some da tela e o aparelho
// esquece o token e se reapresenta.
void t_token_recusado_faz_o_aparelho_se_registrar_de_novo(void)
{
    COMECA("401 · a conta sai da tela, e ele se registra de novo sozinho");

    liga_pareado();
    snprintf(ap.estado.meu_id, sizeof ap.estado.meu_id, "%s", "AA:BB:CC");

    (void)uso_sincronizar(hal, &ap.estado);
    pc_nuvem_codigo(401);
    pc_nuvem_responde("");
    app_passo(&ap);

    ESPERA_TEXTO(ap.estado.nome, "");
    ESPERA(!ap.estado.tem_token);
    ESPERA_IGUAL(ap.estado.recusa, RECUSA_TOKEN);

    nuvem_cred_t cred;
    (void)nuvem_carrega(hal, &cred);
    ESPERA_TEXTO(cred.token, "");
    // A prova fica: é a identidade.
    ESPERA(cred.prova[0] != '\0');

    TERMINA();
}

// Sem hora não há TLS (o certificado compara datas): não se apresenta antes
// do NTP.
void t_o_aparelho_nao_fala_https_antes_de_saber_a_hora(void)
{
    COMECA("registro · sem relógio não há TLS, e ele espera");

    hal = pc_liga();
    pc_relogio(HOJE, 10, 2);
    app_liga(&ap, hal);
    app_passo(&ap);

    ap.estado.rede = REDE_LIGADA;
    ap.estado.hora_confiavel = false;
    pc_nuvem_rota_zera();

    ESPERA(uso_nuvem_ligar(hal, &ap.estado) != OK);
    ESPERA_TEXTO(pc_nuvem_rota(), "");

    ap.estado.hora_confiavel = true;
    ESPERA_IGUAL(uso_nuvem_ligar(hal, &ap.estado), OK);
    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/registrar");

    TERMINA();
}

// Cadastrado no painel, o código aparece sozinho: o aparelho ouvia "de
// ninguém" e não pedia o código.
void t_cadastrado_no_painel_o_codigo_aparece_sozinho(void)
{
    COMECA("cadastro → código · as seis letras chegam sem ninguém mexer");

    liga_pareado();
    ap.estado.nome[0] = '\0';          // cadastrado, e ainda sem conta
    ap.estado.hora_confiavel = true;

    ap.estado.pilha[0] = TELA_VINCULAR;
    ap.estado.profundidade = 0;

    ap.estado.nuvem_esperando = NUVEM_PAREADO;
    pc_nuvem_rota_zera();
    pc_nuvem_responde("{\"pareado\":false}");
    (void)uso_nuvem_resposta(hal, &ap.estado);

    // É aqui que o código tem de ser pedido.
    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/parear/iniciar");

    ap.estado.nuvem_esperando = NUVEM_PAREAR;
    pc_nuvem_responde("{\"codigo\":\"KXMPQR\"}");
    (void)uso_nuvem_resposta(hal, &ap.estado);

    vista_vincular_t v;
    vista_vincular(&ap.estado, &v);
    ESPERA(!v.esperando);
    ESPERA_TEXTO(v.codigo, "KXMPQR");
    ESPERA_CONTEM(v.prazo, "vale");

    TERMINA();
}

// Com a tela fechada não se pede código (uso único, cinco minutos).
void t_o_codigo_nao_e_pedido_com_a_tela_fechada(void)
{
    COMECA("cadastro → código · só pede com a tela de Conectar aberta");

    liga_pareado();
    ap.estado.nome[0] = '\0';
    ap.estado.pilha[0] = TELA_AGENDA;       // a home, e não a de Conectar
    ap.estado.profundidade = 0;

    ap.estado.nuvem_esperando = NUVEM_PAREADO;
    pc_nuvem_rota_zera();
    pc_nuvem_responde("{\"pareado\":false}");
    (void)uso_nuvem_resposta(hal, &ap.estado);

    ESPERA_TEXTO(pc_nuvem_rota(), "");
    ESPERA_TEXTO(ap.estado.codigo, "");

    TERMINA();
}

// Cada rota sai com o método que o servidor espera: o hal decide pelo
// corpo, e um NULL virou 405 no vidro (o harness não simulava método).
void t_cada_rota_sai_com_o_metodo_que_o_servidor_espera(void)
{
    COMECA("contrato · cada rota sai com o método que o backend aceita");

    liga_pareado();
    ap.estado.hora_confiavel = true;

    // POST precisa de corpo.
    (void)uso_parear(hal, &ap.estado);
    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/parear/iniciar");
    ESPERA_TEXTO(pc_nuvem_metodo(), "POST");

    ap.estado.nuvem_esperando = 0;
    (void)uso_desvincular(hal, &ap.estado);
    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/desparear");
    ESPERA_TEXTO(pc_nuvem_metodo(), "POST");

    ap.estado.nuvem_esperando = 0;
    (void)uso_escolher_agenda(hal, &ap.estado, 0, false);

    // Leitura sai GET.
    ap.estado.nuvem_esperando = 0;
    (void)uso_sincronizar(hal, &ap.estado);
    ESPERA_CONTEM(pc_nuvem_rota(), "/v1/pull");
    ESPERA_TEXTO(pc_nuvem_metodo(), "GET");

    ap.estado.nuvem_esperando = 0;
    (void)uso_agendas(hal, &ap.estado);
    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/agendas");
    ESPERA_TEXTO(pc_nuvem_metodo(), "GET");

    TERMINA();
}

// O cartão vazio pede a colheita inteira: o `syncToken` nunca reenvia o
// que se perdeu.
void t_o_cartao_vazio_pede_a_colheita_inteira(void)
{
    COMECA("pull · cartão sem nada do Google pede tudo, e uma vez só");

    liga_pareado();

    (void)uso_sincronizar(hal, &ap.estado);
    ESPERA_CONTEM(pc_nuvem_rota(), "tudo=1");

    // Uma vez enquanto a colheita está em voo.
    ap.estado.pull_esperando = false;
    pc_nuvem_rota_zera();
    (void)uso_sincronizar(hal, &ap.estado);
    ESPERA_SEM(pc_nuvem_rota(), "tudo=1");
    ESPERA_CONTEM(pc_nuvem_rota(), "esperar=1");

    // A pergunta é do CARTÃO: um evento de amanhã já responde "tenho agenda"
    // com o dia de hoje vazio.
    data_t amanha = data_soma_dias(HOJE, 1);
    item_t ev;
    memset(&ev, 0, sizeof ev);
    snprintf(ev.id,     sizeof ev.id,     "%s", "g:abc");
    snprintf(ev.titulo, sizeof ev.titulo, "%s", "Dentista");
    ev.tipo   = TIPO_EVENTO;
    ev.origem = ORIGEM_GOOGLE;
    ev.dia    = amanha;
    ESPERA_IGUAL(cartao_grava_item(hal, amanha, &ev), OK);
    estado_invalida_cartao(&ap.estado);

    ap.estado.pediu_tudo = false;
    ap.estado.n_itens = 0;              // o dia aberto continua vazio
    ap.estado.itens_validos = true;
    ap.estado.nuvem_esperando = 0;
    ap.estado.pull_esperando = false;
    pc_nuvem_rota_zera();
    (void)uso_sincronizar(hal, &ap.estado);
    ESPERA_SEM(pc_nuvem_rota(), "tudo=1");

    // Depois de descoberta, o ciclo seguinte não relê os diretórios.
    ap.estado.pull_esperando = false;
    pc_falhar_listar(true);
    pc_nuvem_rota_zera();
    (void)uso_sincronizar(hal, &ap.estado);
    ESPERA_SEM(pc_nuvem_rota(), "tudo=1");
    pc_falhar_listar(false);

    // Só capturas de voz no cartão não contam como agenda.
    ESPERA_IGUAL(cartao_apaga_item(hal, amanha, "g:abc"), OK);
    estado_invalida_cartao(&ap.estado);

    item_t fala;
    memset(&fala, 0, sizeof fala);
    snprintf(fala.id, sizeof fala.id, "%s", "1422-fala");
    fala.tipo   = TIPO_NADA;
    fala.origem = ORIGEM_AQUI;
    fala.dia    = HOJE;
    ESPERA_IGUAL(cartao_grava_item(hal, HOJE, &fala), OK);

    ap.estado.pediu_tudo = false;
    ap.estado.pull_esperando = false;
    pc_nuvem_rota_zera();
    (void)uso_sincronizar(hal, &ap.estado);
    ESPERA_CONTEM(pc_nuvem_rota(), "tudo=1");

    TERMINA();
}


// O item da fala mora no dia em que VENCE ("amanhã" nascia em hoje).
void t_o_que_a_fala_criou_mora_no_dia_do_evento(void)
{
    COMECA("evento falado hoje para amanhã mora na pasta de AMANHÃ");

    liga_pareado();
    ap.estado.nuvem_esperando = NUVEM_CAPTURA;

    data_t amanha = data_soma_dias(HOJE, 1);
    char json[320];
    snprintf(json, sizeof json,
             "{\"falou\":\"marca dentista amanha as tres\",\"nota\":\"nt:1\","
             "\"acoes\":[{\"v\":\"criou\",\"id\":\"n:1\",\"tp\":4,"
             "\"t\":\"Dentista\",\"d\":\"%04d-%02d-%02d\",\"h\":\"15:00\"}]}",
             amanha.ano, amanha.mes, amanha.dia);
    pc_nuvem_responde(json);
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    ESPERA_IGUAL(ap.estado.n_resultados, 1);
    ESPERA(data_igual(ap.estado.resultados[0].item.dia, amanha));

    ESPERA_IGUAL(uso_confirmar_resultado(hal, &ap.estado), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, amanha, "n:1", &lido), OK);
    ESPERA_TEXTO(lido.titulo, "Dentista");
    ESPERA_IGUAL(cartao_le_item(hal, HOJE, "n:1", &lido), ERR_ARQUIVO);

    TERMINA();
}

// Tarefa sem data nasce em hoje.
void t_tarefa_sem_data_continua_no_dia_da_fala(void)
{
    COMECA("tarefa sem data nasce no dia em que foi falada");

    liga_pareado();
    ap.estado.nuvem_esperando = NUVEM_CAPTURA;

    pc_nuvem_responde(
        "{\"falou\":\"comprar pasta termica\",\"nota\":\"nt:2\","
        "\"acoes\":[{\"v\":\"criou\",\"id\":\"n:1\",\"tp\":2,"
        "\"t\":\"Comprar pasta térmica\"}]}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA(data_igual(ap.estado.resultados[0].item.dia, HOJE));

    TERMINA();
}


// O gesto troca o id provisório (`n:1`) pelo do Google: senão o pull
// gravava ao lado e o compromisso aparecia duas vezes.
void t_o_gesto_troca_o_id_provisorio_pelo_do_google(void)
{
    COMECA("o id provisório vira o do Google, e o item não duplica");

    liga_pareado();

    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,     sizeof it.id,     "%s", "n:1");
    snprintf(it.titulo, sizeof it.titulo, "%s", "Dentista");
    snprintf(it.hora,   sizeof it.hora,   "%s", "15:00");
    it.tipo  = TIPO_EVENTO;
    it.dia   = HOJE;
    it.dur_s = 14;
    ESPERA_IGUAL(cartao_grava_item(hal, HOJE, &it), OK);

    ESPERA_IGUAL(uso_enviar_gesto(hal, &ap.estado, &it, "criou"), OK);
    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/push");

    pc_nuvem_responde("{\"ok\":true,\"id\":\"g:5o8p2k1q3r\"}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    // Renomear, não recriar: duração e nota ficam.
    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, HOJE, "n:1", &lido), ERR_ARQUIVO);
    ESPERA_IGUAL(cartao_le_item(hal, HOJE, "g:5o8p2k1q3r", &lido), OK);
    ESPERA_TEXTO(lido.titulo, "Dentista");
    ESPERA_IGUAL(lido.dur_s, 14);

    int n = 0;
    item_t itens[ITENS_MAX];
    ESPERA_IGUAL(cartao_lista_itens(hal, HOJE, 0, itens, ITENS_MAX, &n), OK);
    ESPERA_IGUAL(n, 1);

    TERMINA();
}

// Uma fala com TRÊS ações: cada resposta tem de achar o seu item (com um
// lugar só para `gesto_id`, a troca acontecia no item errado e as tarefas
// duplicavam).
void t_uma_fala_com_tres_acoes_nao_duplica_nenhuma(void)
{
    COMECA("três ações de uma fala: cada resposta troca o id da SUA");

    liga_pareado();

    const char *ids[3]     = { "n:1", "n:2", "n:3" };
    const char *titulos[3] = { "Dentista", "Pasta térmica", "Ligar pro Léo" };

    for (int i = 0; i < 3; i++) {
        item_t it;
        memset(&it, 0, sizeof it);
        snprintf(it.id,     sizeof it.id,     "%s", ids[i]);
        snprintf(it.titulo, sizeof it.titulo, "%s", titulos[i]);
        it.tipo = i == 0 ? TIPO_EVENTO : TIPO_TAREFA;
        it.dia  = HOJE;
        ESPERA_IGUAL(cartao_grava_item(hal, HOJE, &it), OK);
        ESPERA_IGUAL(uso_enviar_gesto(hal, &ap.estado, &it, "criou"), OK);
    }

    // A linha tem um dono: a primeira saiu. São três na conta porque o arquivo
    // do que sobe só sai quando o servidor responde.
    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/push");
    ESPERA_IGUAL(ap.estado.gesto_n, 3);

    const char *definitivos[3] = { "g:aaa", "t:bbb", "t:ccc" };
    for (int i = 0; i < 3; i++) {
        char resposta[80];
        snprintf(resposta, sizeof resposta, "{\"ok\":true,\"id\":\"%s\"}",
                 definitivos[i]);
        pc_nuvem_responde(resposta);
        ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
        ESPERA_IGUAL(uso_gesto_pendente(hal, &ap.estado), OK);
    }

    int n = 0;
    item_t itens[ITENS_MAX];
    ESPERA_IGUAL(cartao_lista_itens(hal, HOJE, 0, itens, ITENS_MAX, &n), OK);
    ESPERA_IGUAL(n, 3);

    for (int i = 0; i < 3; i++) {
        item_t lido;
        ESPERA_IGUAL(cartao_le_item(hal, HOJE, ids[i], &lido), ERR_ARQUIVO);
        ESPERA_IGUAL(cartao_le_item(hal, HOJE, definitivos[i], &lido), OK);
        ESPERA_TEXTO(lido.titulo, titulos[i]);
    }

    TERMINA();
}
// Editar não muda o id: a resposta traz o mesmo que subiu.
void t_gesto_de_edicao_nao_mexe_no_id(void)
{
    COMECA("gesto de edição devolve o mesmo id, e o item fica onde está");

    liga_pareado();

    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,     sizeof it.id,     "%s", "g:abc");
    snprintf(it.titulo, sizeof it.titulo, "%s", "Dentista");
    it.tipo = TIPO_EVENTO;
    it.dia  = HOJE;
    ESPERA_IGUAL(cartao_grava_item(hal, HOJE, &it), OK);

    ESPERA_IGUAL(uso_enviar_gesto(hal, &ap.estado, &it, "editou"), OK);
    pc_nuvem_responde("{\"ok\":true,\"id\":\"g:abc\"}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, HOJE, "g:abc", &lido), OK);

    TERMINA();
}


// O pull derruba as marcas do mês: senão um evento novo não acendia o dia
// até outra coisa invalidar.
void t_o_pull_derruba_as_marcas_do_calendario(void)
{
    COMECA("o que o pull traz acende o pontinho do dia dele");

    liga_pareado();

    ESPERA_IGUAL(uso_carregar_marcas(hal, &ap.estado), OK);
    ESPERA(ap.estado.marcas_validas);
    ESPERA_IGUAL((int)ap.estado.marcas_evento, 0);

    data_t amanha = data_soma_dias(HOJE, 1);
    char json[320];
    snprintf(json, sizeof json,
             "{\"itens\":[{\"id\":\"g:abc\",\"t\":\"Dentista\","
             "\"h\":\"15:00\",\"d\":\"%04d-%02d-%02d\",\"tp\":4,"
             "\"o\":\"g\"}]}",
             amanha.ano, amanha.mes, amanha.dia);

    ap.estado.nuvem_esperando = NUVEM_PULL;
    pc_nuvem_responde(json);
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    ESPERA(!ap.estado.marcas_validas);
    ESPERA_IGUAL(uso_carregar_marcas(hal, &ap.estado), OK);
    ESPERA(ap.estado.marcas_evento & (1u << (amanha.dia - 1)));

    ESPERA(!(ap.estado.marcas_evento & (1u << (HOJE.dia - 1))));

    // Uma edição posterior MOVE o item de pasta, e continua havendo uma cópia
    // só.
    data_t depois = data_soma_dias(HOJE, 2);
    snprintf(json, sizeof json,
             "{\"itens\":[{\"id\":\"g:abc\",\"t\":\"Dentista\","
             "\"h\":\"15:00\",\"d\":\"%04d-%02d-%02d\",\"tp\":4,"
             "\"o\":\"g\"}]}", depois.ano, depois.mes, depois.dia);
    ap.estado.nuvem_esperando = NUVEM_PULL;
    pc_nuvem_responde(json);
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, depois, "g:abc", &lido), OK);
    ESPERA(data_igual(lido.vence, depois));
    ESPERA_IGUAL(cartao_le_item(hal, amanha, "g:abc", &lido), ERR_ARQUIVO);

    // O pontinho vai junto: acende no dia novo, apaga no velho.
    ESPERA_IGUAL(uso_carregar_marcas(hal, &ap.estado), OK);
    ESPERA(ap.estado.marcas_evento & (1u << (depois.dia - 1)));
    ESPERA(!(ap.estado.marcas_evento & (1u << (amanha.dia - 1))));

    TERMINA();
}


// Apagou no Google, some do Tinto em qualquer dia: o `removidos` traz só o
// id, e procurar só em hoje deixava o de amanhã no vidro.
void t_apagado_no_google_some_de_qualquer_dia(void)
{
    COMECA("apagou no Google: some do Tinto, esteja no dia que estiver");

    liga_pareado();

    data_t amanha = data_soma_dias(HOJE, 1);
    item_t ev;
    memset(&ev, 0, sizeof ev);
    snprintf(ev.id,     sizeof ev.id,     "%s", "g:abc");
    snprintf(ev.titulo, sizeof ev.titulo, "%s", "Dentista");
    ev.tipo = TIPO_EVENTO; ev.origem = ORIGEM_GOOGLE; ev.dia = amanha;
    ESPERA_IGUAL(cartao_grava_item(hal, amanha, &ev), OK);

    ap.estado.nuvem_esperando = NUVEM_PULL;
    pc_nuvem_responde("{\"removidos\":[{\"id\":\"g:abc\"}]}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, amanha, "g:abc", &lido), ERR_ARQUIVO);

    // E some de TODOS os caches (dia, marcas, item aberto, pendentes).
    ESPERA(!ap.estado.itens_validos);
    ESPERA(!ap.estado.marcas_validas);
    ESPERA(!ap.estado.pendentes_validas);

    TERMINA();
}

// A tarefa apagada no Google sai da TELA: o teste acima prova as
// bandeiras, este prova o que se vê.
void t_a_tarefa_apagada_no_google_some_da_tela(void)
{
    COMECA("tarefa apagada no Google some da Agenda, e não só do cartão");

    liga_pareado();

    item_t ta;
    memset(&ta, 0, sizeof ta);
    snprintf(ta.id,     sizeof ta.id,     "%s", "g:tarefa");
    snprintf(ta.titulo, sizeof ta.titulo, "%s", "Comprar pasta");
    ta.tipo = TIPO_TAREFA; ta.origem = ORIGEM_GOOGLE; ta.dia = HOJE;
    ESPERA_IGUAL(cartao_grava_item(hal, HOJE, &ta), OK);

    ESPERA_IGUAL(uso_carregar_dia(hal, &ap.estado), OK);
    ESPERA_IGUAL(uso_carregar_pendentes(hal, &ap.estado), OK);

    vista_agenda_t v;
    vista_agenda(&ap.estado, &v);
    ESPERA_IGUAL(v.n_trabalho, 1);

    ap.estado.nuvem_esperando = NUVEM_PULL;
    pc_nuvem_responde("{\"removidos\":[{\"id\":\"g:tarefa\"}]}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    if (!ap.estado.itens_validos)     ESPERA_IGUAL(uso_carregar_dia(hal, &ap.estado), OK);
    if (!ap.estado.pendentes_validas) ESPERA_IGUAL(uso_carregar_pendentes(hal, &ap.estado), OK);

    vista_agenda(&ap.estado, &v);
    ESPERA_IGUAL(v.n_trabalho, 0);

    TERMINA();
}

// Desconectar PRESERVA o cartão; outra conta LIMPA (a agenda da anterior
// seria vazamento).
void t_desconectar_preserva_e_outra_conta_limpa(void)
{
    COMECA("desconectar preserva o cache; outra conta limpa o que era dela");

    liga_pareado();

    nuvem_cred_t cred;
    (void)nuvem_carrega(hal, &cred);
    snprintf(cred.conta, sizeof cred.conta, "%s", "convidado@exemplo.com");
    ESPERA_IGUAL(nuvem_grava(hal, &cred), OK);

    item_t ev;
    memset(&ev, 0, sizeof ev);
    snprintf(ev.id,     sizeof ev.id,     "%s", "g:abc");
    snprintf(ev.titulo, sizeof ev.titulo, "%s", "Dentista");
    ev.tipo = TIPO_EVENTO; ev.origem = ORIGEM_GOOGLE; ev.dia = HOJE;
    ESPERA_IGUAL(cartao_grava_item(hal, HOJE, &ev), OK);

    // Na linha de AGORA: o pull do `liga_pareado` segue pendurado na outra.
    ap.estado.nuvem_esperando = NUVEM_DESVINCULAR;
    pc_nuvem_responde_ja("{\"ok\":true}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_TEXTO(ap.estado.nome, "");

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, HOJE, "g:abc", &lido), OK);

    // A MESMA conta volta: nada se perde.
    ap.estado.nuvem_esperando = NUVEM_PAREADO;
    pc_nuvem_responde_ja("{\"pareado\":true,\"conta\":\"convidado@exemplo.com\"}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_IGUAL(cartao_le_item(hal, HOJE, "g:abc", &lido), OK);

    // OUTRA conta: o que era da anterior sai.
    ap.estado.nuvem_esperando = NUVEM_PAREADO;
    pc_nuvem_responde_ja("{\"pareado\":true,\"conta\":\"outro@exemplo.com\"}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_IGUAL(cartao_le_item(hal, HOJE, "g:abc", &lido), ERR_ARQUIVO);

    // Token, prova e ajustes ficam.
    (void)nuvem_carrega(hal, &cred);
    ESPERA(cred.prova[0] != '\0');
    ESPERA_TEXTO(cred.conta, "outro@exemplo.com");

    TERMINA();
}


// O cursor não avança sobre um lote que não foi gravado: pular perderia o
// item para sempre (o Google não o manda de novo). Repetir é seguro.
void t_o_cursor_nao_passa_por_cima_de_um_lote_que_falhou(void)
{
    COMECA("escrita que falha segura o cursor — o lote volta, não se perde");

    liga_pareado();

    snprintf(ap.estado.cursor_pull, sizeof ap.estado.cursor_pull, "%s", "c1");

    pc_falhar_escrever(true);

    ap.estado.nuvem_esperando = NUVEM_PULL;
    pc_nuvem_responde(
        "{\"itens\":[{\"id\":\"g:novo\",\"t\":\"Dentista\","
        "\"tp\":4,\"d\":\"2026-08-18\"}],\"cursor\":\"c2\"}");
    (void)uso_nuvem_resposta(hal, &ap.estado);

    pc_falhar_escrever(false);

    // O cursor ficou: o lote volta no próximo pull.
    ESPERA_TEXTO(ap.estado.cursor_pull, "c1");

    TERMINA();
}

// E o caminho feliz continua andando.
void t_o_cursor_avanca_quando_o_lote_grava(void)
{
    COMECA("lote gravado inteiro avança o cursor, como sempre");

    liga_pareado();
    snprintf(ap.estado.cursor_pull, sizeof ap.estado.cursor_pull, "%s", "c1");

    ap.estado.nuvem_esperando = NUVEM_PULL;
    pc_nuvem_responde(
        "{\"itens\":[{\"id\":\"g:novo\",\"t\":\"Dentista\","
        "\"tp\":4,\"d\":\"2026-08-18\"}],\"cursor\":\"c2\"}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    ESPERA_TEXTO(ap.estado.cursor_pull, "c2");
    TERMINA();
}


// Sincronizar não muda a ORIGEM: o servidor não conhece o `nota`, e gravar
// por cima perdia a fala que originou o item.
void t_sincronizar_nao_apaga_a_fala_que_originou_o_item(void)
{
    COMECA("o item criado por voz continua ligado à fala depois do sync");

    liga_pareado();

    item_t meu;
    memset(&meu, 0, sizeof meu);
    snprintf(meu.id,     sizeof meu.id,     "%s", "g:abc");
    snprintf(meu.titulo, sizeof meu.titulo, "%s", "Dentista");
    snprintf(meu.hora,   sizeof meu.hora,   "%s", "14:00");
    snprintf(meu.nota,   sizeof meu.nota,   "%s", "0712-fala");
    meu.tipo   = TIPO_EVENTO;
    meu.origem = ORIGEM_AQUI;
    meu.dia    = HOJE;
    meu.vence  = HOJE;
    ESPERA_IGUAL(cartao_grava_item(hal, HOJE, &meu), OK);

    ap.estado.nuvem_esperando = NUVEM_PULL;
    pc_nuvem_responde(
        "{\"itens\":[{\"id\":\"g:abc\",\"t\":\"Dentista (remarcado)\","
        "\"tp\":4,\"d\":\"2026-08-21\",\"h\":\"15:00\"}],"
        "\"cursor\":\"c2\"}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, HOJE, "g:abc", &lido), OK);

    // Os CAMPOS são os do Google.
    ESPERA_TEXTO(lido.titulo, "Dentista (remarcado)");
    ESPERA_TEXTO(lido.hora,   "15:00");

    // A PROCEDÊNCIA é do Tinto.
    ESPERA_TEXTO(lido.nota, "0712-fala");

    TERMINA();
}

// A busca de agendas espera a vez em vez de sumir (com o pull pendurado,
// devolvia OK sem pedir, e a tela ficava "Buscando").
void t_a_busca_de_agendas_espera_a_vez_em_vez_de_sumir(void)
{
    COMECA("T-32 · abrir a tela com o pull em voo não perde a busca");

    liga_pareado();

    pc_nuvem_rota_zera();
    ap.estado.nuvem_esperando = NUVEM_PULL;

    ESPERA_IGUAL(uso_agendas(hal, &ap.estado), OK);

    ESPERA_TEXTO(pc_nuvem_rota(), "");
    ESPERA(ap.estado.agendas_buscando);

    vista_cartao_t v;
    vista_sincronizacao(&ap.estado, &v);
    ESPERA_CONTEM(v.nome, "Buscando");

    ap.estado.nuvem_esperando = NUVEM_NADA;
    pc_avanca_ms(1000);
    pc_tick();
    app_passo(&ap);

    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/agendas");

    TERMINA();
}

// E quando não volta, ela desiste com uma saída.
void t_a_busca_de_agendas_desiste_e_oferece_tentar_de_novo(void)
{
    COMECA("T-32 · busca que não volta vira erro, e não espera eterna");

    liga_pareado();

    pc_nuvem_demora(true);
    ESPERA_IGUAL(uso_agendas(hal, &ap.estado), OK);
    ESPERA(ap.estado.agendas_buscando);

    pc_avanca_ms(60u * 1000u);
    pc_tick();
    app_passo(&ap);

    ESPERA(!ap.estado.agendas_buscando);

    vista_cartao_t v;
    vista_sincronizacao(&ap.estado, &v);
    ESPERA_CONTEM(v.nome, "Não consegui");

    bool tem_saida = false;
    for (int i = 0; i < v.n_dest; i++)
        if (strstr(v.dest[i].titulo, "Tentar novamente")) tem_saida = true;
    ESPERA(tem_saida);

    pc_nuvem_demora(false);
    TERMINA();
}

// Buscando, os pontinhos andam com o relógio.
void t_a_busca_de_agendas_anima_os_pontinhos(void)
{
    COMECA("T-32 · o 'Buscando' anima os três pontinhos");

    liga_pareado();
    (void)uso_agendas(hal, &ap.estado);

    vista_cartao_t v;
    int visto[4] = {0};
    for (int s = 0; s < 4; s++) {
        ap.estado.agora_ms = (uint32_t)s * 1000u;
        vista_sincronizacao(&ap.estado, &v);
        ESPERA(v.pontos >= 0 && v.pontos <= 3);
        visto[v.pontos] = 1;
    }

    // Os quatro quadros: um indicador que alterna dois estados pisca.
    ESPERA(visto[0] && visto[1] && visto[2] && visto[3]);

    // Fora da busca, nada anima.
    ap.estado.agendas_buscando = false;
    ap.estado.n_agendas = 1;
    snprintf(ap.estado.agendas[0].nome, sizeof ap.estado.agendas[0].nome,
             "%s", "Convidado");
    vista_sincronizacao(&ap.estado, &v);
    ESPERA_IGUAL(v.pontos, -1);

    TERMINA();
}

// O gesto não atropela o pull: duas LINHAS de rede, cada resposta lida como
// de quem é (antes a do pull era lida como a do gesto e o delta ia para o
// lixo).
void t_o_gesto_nao_atropela_o_pull_em_voo(void)
{
    COMECA("nuvem · o gesto e o pull não leem a resposta um do outro");

    liga_pareado();

    ap.estado.tem_token = true;
    pc_nuvem_demora(true);
    ESPERA_IGUAL(uso_sincronizar(hal, &ap.estado), OK);
    ESPERA(ap.estado.pull_esperando);
    pc_nuvem_rota_zera();

    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id, sizeof it.id, "%s", "g:1");
    snprintf(it.titulo, sizeof it.titulo, "%s", "dentista");
    it.tipo = TIPO_TAREFA;
    it.dia = ap.estado.hoje;

    ESPERA_IGUAL(uso_enviar_gesto(hal, &ap.estado, &it, "apagou"), OK);

    // O gesto saiu sem esperar, e o pull segue pendurado.
    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/push");
    ESPERA(ap.estado.pull_esperando);
    ESPERA_IGUAL(ap.estado.nuvem_esperando, NUVEM_GESTO);

    pc_nuvem_demora(false);
    pc_nuvem_responde_pull("{\"itens\":[],\"fora\":[]}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA(!ap.estado.pull_esperando);
    ESPERA_IGUAL(ap.estado.nuvem_esperando, NUVEM_GESTO);

    TERMINA();
}

// A linha nunca fica presa: uma resposta perdida deixava o aparelho mudo.
void t_a_linha_da_nuvem_se_solta_sozinha(void)
{
    COMECA("nuvem · resposta que não volta não prende a linha para sempre");

    liga_pareado();

    ap.estado.tem_token = true;
    pc_nuvem_demora(true);
    ESPERA_IGUAL(uso_sincronizar(hal, &ap.estado), OK);
    ESPERA(ap.estado.pull_esperando);

    pc_avanca_ms(120u * 1000u);
    pc_tick();
    app_passo(&ap);

    ESPERA(!ap.estado.pull_esperando);

    pc_nuvem_demora(false);
    TERMINA();
}

// Marcar uma agenda sai na hora, pela linha própria, em vez de sumir.
void t_marcar_agenda_espera_a_vez_em_vez_de_sumir(void)
{
    COMECA("T-32 · ligar uma agenda com o pull em voo sai na hora");

    liga_pareado();
    ap.estado.tem_token = true;
    snprintf(ap.estado.agendas[0].nome, sizeof ap.estado.agendas[0].nome,
             "%s", "Feriados");
    ap.estado.n_agendas = 1;

    pc_nuvem_demora(true);
    ESPERA_IGUAL(uso_sincronizar(hal, &ap.estado), OK);
    pc_nuvem_rota_zera();

    ESPERA_IGUAL(uso_escolher_agenda(hal, &ap.estado, 0, true), OK);

    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/agendas");
    ESPERA_CONTEM(pc_nuvem_corpo(), "Feriados");
    ESPERA(ap.estado.pull_esperando);

    pc_nuvem_demora(false);
    pc_nuvem_responde_pull("{\"itens\":[],\"fora\":[]}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA(!ap.estado.pull_esperando);
    ESPERA_IGUAL(ap.estado.nuvem_esperando, NUVEM_ESCOLHA);

    TERMINA();
}

// Voltar da rede continua pelo DELTA: o syncToken segue válido, e colher
// tudo de novo apagaria e regravaria o cartão na volta do rádio.
void t_voltar_da_rede_continua_pelo_delta(void)
{
    COMECA("pull · a rede voltou: continua pelo delta");

    liga_pareado();
    ap.estado.tem_token = true;

    ap.estado.pediu_tudo = true;

    ap.estado.rede = REDE_LIGADA;

    pc_nuvem_rota_zera();
    ESPERA_IGUAL(uso_sincronizar(hal, &ap.estado), OK);

    ESPERA_SEM(pc_nuvem_rota(), "tudo=1");
    ESPERA_CONTEM(pc_nuvem_rota(), "esperar=1");

    pc_nuvem_responde("{\"itens\":[],\"fora\":[]}");
    app_passo(&ap);
    pc_nuvem_rota_zera();
    ESPERA_IGUAL(uso_sincronizar(hal, &ap.estado), OK);
    ESPERA(strstr(pc_nuvem_rota(), "tudo=1") == NULL);

    TERMINA();
}

// O interruptor diz que está indo (pontinhos) antes de virar na
// confirmação.
void t_o_interruptor_avisa_que_esta_indo_antes_de_virar(void)
{
    COMECA("T-32 · entre o OK e a resposta, a linha mostra os pontinhos");

    liga_pareado();
    (void)uso_agendas(hal, &ap.estado);
    pc_nuvem_responde(
        "{\"agendas\":[{\"id\":\"h\",\"t\":\"Feriados\",\"on\":true,\"py\":14},"
        "{\"id\":\"p\",\"t\":\"Pessoal\",\"on\":true,\"py\":30}]}");
    app_passo(&ap);

    vista_cartao_t v;
    vista_sincronizacao(&ap.estado, &v);
    ESPERA_IGUAL(v.dest[0].ico, ICO_CAIXA_ON);
    ESPERA_TEXTO(v.dest[0].valor, "");

    ESPERA_IGUAL(uso_escolher_agenda(hal, &ap.estado, 0, false), OK);

    // Só a linha tocada para de afirmar.
    vista_sincronizacao(&ap.estado, &v);
    ESPERA_IGUAL(v.dest[0].ico, ICO_CAIXA);
    ESPERA_CONTEM(v.dest[0].valor, "·");
    ESPERA_IGUAL(v.dest[1].ico, ICO_CAIXA_ON);
    ESPERA_TEXTO(v.dest[1].valor, "");

    pc_nuvem_responde("{\"ok\":true,\"id\":\"h\",\"on\":false}");
    app_passo(&ap);

    vista_sincronizacao(&ap.estado, &v);
    ESPERA_IGUAL(v.dest[0].ico, ICO_CAIXA);
    ESPERA_TEXTO(v.dest[0].valor, "");

    TERMINA();
}

// O gesto não espera o pull pendurado: o pull vai na linha lenta, o resto
// na de agora.
void t_o_gesto_nao_espera_o_pull_pendurado(void)
{
    COMECA("o gesto sai na hora, com o pull pendurado na outra linha");

    liga_pareado();

    ESPERA_IGUAL(uso_sincronizar(hal, &ap.estado), OK);
    ESPERA_CONTEM(pc_nuvem_rota(), "/v1/pull?esperar=1");
    ESPERA(ap.estado.pull_esperando);

    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,     sizeof it.id,     "%s", "t:1");
    snprintf(it.titulo, sizeof it.titulo, "%s", "Pagar o IPVA");
    it.tipo = TIPO_TAREFA;
    it.dia  = HOJE;
    it.feita = true;

    ESPERA_IGUAL(uso_enviar_gesto(hal, &ap.estado, &it, "editou"), OK);
    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/push");
    // Um na conta: o arquivo só sai do cartão quando o servidor responde.
    ESPERA_IGUAL(ap.estado.gesto_n, 1);

    pc_nuvem_responde("{\"ok\":true,\"id\":\"t:1\"}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA(ap.estado.pull_esperando);        // o pull continua de pé

    pc_nuvem_responde_pull("{\"itens\":[],\"removidos\":[]}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA(!ap.estado.pull_esperando);

    TERMINA();
}

// Nem a prova nem o token moram no cartão (vão para o cofre); fica o que
// ajuda a diagnosticar.
void t_o_cartao_nao_guarda_mais_prova_nem_token(void)
{
    COMECA("o cartão não guarda mais prova nem token");

    liga_pareado();

    nuvem_cred_t cred;
    (void)nuvem_carrega(hal, &cred);
    snprintf(cred.token, sizeof cred.token, "%s", "tk-secreto-999");
    snprintf(cred.conta, sizeof cred.conta, "%s", "convidado@exemplo.com");
    ESPERA_IGUAL(nuvem_grava(hal, &cred), OK);

    char json[512];
    ESPERA_IGUAL(hal->ler("/TINTO/sistema/nuvem.json", json, sizeof json), OK);

    ESPERA_SEM(json, "tk-secreto-999");
    ESPERA_SEM(json, cred.prova);

    ESPERA_CONTEM(json, "convidado@exemplo.com");
    ESPERA_CONTEM(json, cred.device_id);

    ESPERA_TEXTO(pc_segredo("nuvem_token"), "tk-secreto-999");
    ESPERA_TEXTO(pc_segredo("nuvem_prova"), cred.prova);

    TERMINA();
}

// O cartão antigo migra prova e token: ignorá-los faria o servidor recusar
// o registro.
void t_o_cartao_velho_migra_prova_e_token(void)
{
    COMECA("cartão de antes: prova e token migram e saem do arquivo");

    hal = pc_liga();
    app_liga(&ap, hal);

    pc_poe_arquivo("/TINTO/sistema/nuvem.json",
                   "{\"v\":1,\"servidor\":\"https://x.test\","
                   "\"device_id\":\"AA:BB:CC\",\"prova\":\"prova-velha-32\","
                   "\"token\":\"tk-de-campo\",\"conta\":\"eu@x.com\"}");

    nuvem_cred_t cred;
    ESPERA_IGUAL(nuvem_carrega(hal, &cred), OK);

    ESPERA_TEXTO(cred.prova, "prova-velha-32");
    ESPERA_TEXTO(cred.token, "tk-de-campo");
    ESPERA(nuvem_registrado(&cred));

    char json[512];
    ESPERA_IGUAL(hal->ler("/TINTO/sistema/nuvem.json", json, sizeof json), OK);
    ESPERA_SEM(json, "tk-de-campo");
    ESPERA_SEM(json, "prova-velha-32");

    ESPERA_TEXTO(pc_segredo("nuvem_token"), "tk-de-campo");

    TERMINA();
}

// PONTA A PONTA: uma série diária chega ao mês inteiro, entrando lote a
// lote como o servidor entrega (a varredura parava em 32 pastas na ordem do
// FAT).
void t_e2e_a_serie_diaria_chega_ao_mes_inteiro(void)
{
    COMECA("e2e · série diária marca o mês inteiro e abre no dia distante");

    liga_pareado();

    // O cartão já tem o mês passado: era isso que fazia o teto estourar.
    for (int dia = 1; dia <= 12; dia++) {
        char json[320];
        snprintf(json, sizeof json,
                 "{\"itens\":[{\"id\":\"g:ago%02d\",\"t\":\"Reunião\","
                 "\"h\":\"09:00\",\"d\":\"2026-08-%02d\",\"tp\":4,"
                 "\"o\":\"g\"}]}", dia, dia);
        ap.estado.nuvem_esperando = NUVEM_PULL;
        pc_nuvem_responde(json);
        ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    }

    const int PRIMEIRO = 1, ULTIMO = 30;
    for (int dia = PRIMEIRO; dia <= ULTIMO; dia++) {
        char json[320];
        snprintf(json, sizeof json,
                 "{\"itens\":[{\"id\":\"g:serie_202609%02d\","
                 "\"t\":\"Academia\",\"h\":\"07:00\",\"r\":true,"
                 "\"d\":\"2026-09-%02d\",\"tp\":4,\"o\":\"g\"}]}",
                 dia, dia);

        ap.estado.nuvem_esperando = NUVEM_PULL;
        pc_nuvem_responde(json);
        ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    }

    uso_ir_para_dia(&ap.estado, (data_t){ 2026, 9, 15 });
    ESPERA_IGUAL(uso_carregar_marcas(hal, &ap.estado), OK);

    for (int dia = PRIMEIRO; dia <= ULTIMO; dia++)
        ESPERA(ap.estado.marcas_evento & (1u << (dia - 1)));

    // E o dia distante abre com a coisa dentro.
    uso_ir_para_dia(&ap.estado, (data_t){ 2026, 9, 28 });
    ESPERA_IGUAL(uso_carregar_dia(hal, &ap.estado), OK);

    int achou = 0;
    for (int i = 0; i < ap.estado.n_itens; i++)
        if (strcmp(ap.estado.itens[i].titulo, "Academia") == 0) {
            achou++;
            // Chega sabendo que é série.
            ESPERA(ap.estado.itens[i].repete);
        }
    ESPERA_IGUAL(achou, 1);

    TERMINA();
}

// O vidro espera o lote acabar: invalidar por pedaço repintava item a item.
void t_o_lote_em_pedacos_nao_repinta_a_cada_pedaco(void)
{
    COMECA("pull · enquanto houver mais, o cache não cai");

    liga_pareado();

    ESPERA_IGUAL(uso_carregar_dia(hal, &ap.estado), OK);
    ESPERA(ap.estado.itens_validos);

    ap.estado.nuvem_esperando = NUVEM_PULL;
    pc_nuvem_responde("{\"itens\":[{\"id\":\"g:m1\",\"t\":\"Academia\","
                      "\"h\":\"07:00\",\"d\":\"2026-08-21\",\"tp\":4,"
                      "\"o\":\"g\"}],\"mais\":true}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, HOJE, "g:m1", &lido), OK);
    ESPERA(ap.estado.itens_validos);
    ESPERA(ap.estado.pull_tem_mais);

    ap.estado.nuvem_esperando = NUVEM_PULL;
    pc_nuvem_responde("{\"itens\":[{\"id\":\"g:m2\",\"t\":\"Dentista\","
                      "\"h\":\"15:00\",\"d\":\"2026-08-21\",\"tp\":4,"
                      "\"o\":\"g\"}],\"mais\":false}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    ESPERA(!ap.estado.itens_validos);
    ESPERA(!ap.estado.pull_tem_mais);

    TERMINA();
}

// O lote que termina num pedaço vazio ainda relê a tela (a lista "Compras"
// desceu e a Agenda mostrava as tarefas de antes).
void t_lote_que_termina_vazio_ainda_rele_a_tela(void)
{
    COMECA("pull · o lote que termina num pedaço vazio ainda relê a tela");

    liga_pareado();
    ESPERA_IGUAL(uso_carregar_dia(hal, &ap.estado), OK);
    ESPERA_IGUAL(uso_carregar_pendentes(hal, &ap.estado), OK);

    ap.estado.nuvem_esperando = NUVEM_PULL;
    pc_nuvem_responde("{\"itens\":[{\"id\":\"t:arroz\",\"t\":\"Arroz\","
                      "\"tp\":2,\"l\":\"Compras\",\"o\":\"g\"}],\"mais\":true}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA(ap.estado.pendentes_validas);          // no meio, a tela espera

    ap.estado.nuvem_esperando = NUVEM_PULL;
    pc_nuvem_responde("{\"itens\":[],\"mais\":false}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    ESPERA(!ap.estado.pendentes_validas);
    ESPERA(!ap.estado.itens_validos);

    ESPERA_IGUAL(uso_carregar_pendentes(hal, &ap.estado), OK);
    ap.estado.nuvem_esperando = NUVEM_PULL;
    pc_nuvem_responde("{\"itens\":[],\"mais\":false}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA(ap.estado.pendentes_validas);

    TERMINA();
}

// PONTA A PONTA: a rotina desce como REGRA, uma linha no cartão; o dia
// apagado no celular volta como buraco na regra.
void t_e2e_a_rotina_marca_o_mes_com_uma_linha(void)
{
    COMECA("e2e · a rotina desce como regra e marca o mês inteiro");

    liga_pareado();

    ap.estado.nuvem_esperando = NUVEM_PULL;
    pc_nuvem_responde("{\"itens\":[{\"id\":\"g:academia\",\"t\":\"Academia\","
                      "\"h\":\"07:00\",\"r\":true,\"rr\":\"d:1\","
                      "\"d\":\"2026-08-21\",\"tp\":4,\"o\":\"g\"}]}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    // UMA pasta no cartão, não uma por dia.
    data_t dias[CARTAO_DIAS_MAX];
    int nd = 0;
    ESPERA_IGUAL(cartao_lista_dias(hal, dias, CARTAO_DIAS_MAX, &nd), OK);
    ESPERA_IGUAL(nd, 1);

    ESPERA_IGUAL(uso_carregar_marcas(hal, &ap.estado), OK);
    for (int dia = 21; dia <= 31; dia++)
        ESPERA(ap.estado.marcas_evento & (1u << (dia - 1)));
    ESPERA(!(ap.estado.marcas_evento & (1u << 19)));   // dia 20: antes dela

    uso_ir_para_dia(&ap.estado, (data_t){ 2026, 8, 29 });
    ESPERA_IGUAL(uso_carregar_dia(hal, &ap.estado), OK);

    int achou = 0;
    for (int i = 0; i < ap.estado.n_itens; i++)
        if (strcmp(ap.estado.itens[i].titulo, "Academia") == 0) {
            achou++;
            ESPERA_IGUAL(ap.estado.itens[i].dia.dia, 29);
        }
    ESPERA_IGUAL(achou, 1);

    ap.estado.nuvem_esperando = NUVEM_PULL;
    pc_nuvem_responde("{\"itens\":[{\"id\":\"g:academia\",\"t\":\"Academia\","
                      "\"h\":\"07:00\",\"r\":true,\"rr\":\"d:1|x=0829\","
                      "\"d\":\"2026-08-21\",\"tp\":4,\"o\":\"g\"}]}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    ESPERA_IGUAL(uso_carregar_dia(hal, &ap.estado), OK);
    for (int i = 0; i < ap.estado.n_itens; i++)
        ESPERA(strcmp(ap.estado.itens[i].titulo, "Academia") != 0);

    ESPERA_IGUAL(uso_carregar_marcas(hal, &ap.estado), OK);
    ESPERA(!(ap.estado.marcas_evento & (1u << 28)));   // 29 saiu
    ESPERA(ap.estado.marcas_evento & (1u << 29));      // 30 continua

    TERMINA();
}

// O dia FORA da janela é CONSULTA: aparece na tela e não vai para o cartão.
void t_o_dia_distante_e_consulta_e_nao_vai_para_o_cartao(void)
{
    COMECA("janela · o dia distante aparece na tela e não fica no cartão");

    liga_pareado();

    data_t longe = data_soma_dias(HOJE, 20);
    uso_ir_para_dia(&ap.estado, longe);
    ESPERA_IGUAL(uso_carregar_dia(hal, &ap.estado), OK);

    // Andar o cursor não pede nada; quem pede é o OK.
    ESPERA_IGUAL(ap.estado.n_itens, 0);
    ESPERA(ap.estado.dia_fora_da_janela);
    ESPERA_IGUAL(ap.estado.dia_pedido[0], '\0');

    uso_pedir_o_dia(&ap.estado);
    ESPERA(ap.estado.dia_pedido[0] != '\0');

    // O pull NÃO responde por isto: o `"dia":[]` de toda resposta zerava a
    // consulta.
    ap.estado.nuvem_esperando = NUVEM_PULL;
    pc_nuvem_responde("{\"itens\":[],\"dia\":[]}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA(ap.estado.dia_pedido[0] != '\0');       // o pedido continua de pé
    ESPERA_IGUAL(ap.estado.dia_consultado.ano, 0);  // e nada foi consultado

    pc_nuvem_rota_zera();
    ap.estado.nuvem_esperando = NUVEM_NADA;
    ESPERA_IGUAL(uso_olhar(hal, &ap.estado), OK);
    ESPERA_CONTEM(pc_nuvem_rota(), "/v1/olhar");

    char json[360];
    snprintf(json, sizeof json,
             "{\"dd\":\"%04d-%02d-%02d\",\"dia\":[{\"id\":\"g:casamento\","
             "\"t\":\"Casamento\",\"h\":\"18:00\",\"d\":\"%04d-%02d-%02d\","
             "\"tp\":4,\"o\":\"g\"}]}",
             longe.ano, longe.mes, longe.dia,
             longe.ano, longe.mes, longe.dia);

    pc_nuvem_responde(json);
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    ESPERA_IGUAL(ap.estado.n_itens, 1);
    ESPERA_TEXTO(ap.estado.itens[0].titulo, "Casamento");

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, longe, "g:casamento", &lido), ERR_ARQUIVO);

    ESPERA_IGUAL(ap.estado.dia_pedido[0], '\0');

    TERMINA();
}

// O item do dia consultado ABRE e a tarefa dele MARCA (antes o gesto lia do
// cartão, onde ele não está, e saía calado).
void t_o_item_do_dia_consultado_abre_e_marca(void)
{
    COMECA("janela · o item do dia distante abre e a tarefa dele marca");

    liga_pareado();

    data_t longe = data_soma_dias(HOJE, 20);
    uso_ir_para_dia(&ap.estado, longe);
    ESPERA_IGUAL(uso_carregar_dia(hal, &ap.estado), OK);
    uso_pedir_o_dia(&ap.estado);

    char json[440];
    snprintf(json, sizeof json,
             "{\"dd\":\"%04d-%02d-%02d\",\"dia\":[{\"id\":\"g:casamento\","
             "\"t\":\"Casamento\",\"h\":\"18:00\",\"d\":\"%04d-%02d-%02d\","
             "\"tp\":4,\"o\":\"g\"},{\"id\":\"t:presente\","
             "\"t\":\"Levar presente\",\"d\":\"%04d-%02d-%02d\",\"tp\":2,"
             "\"o\":\"g\"}]}",
             longe.ano, longe.mes, longe.dia,
             longe.ano, longe.mes, longe.dia,
             longe.ano, longe.mes, longe.dia);

    ap.estado.nuvem_esperando = NUVEM_OLHAR;
    pc_nuvem_responde(json);
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_IGUAL(ap.estado.n_itens, 2);

    item_t evento = ap.estado.itens[0];
    ESPERA_IGUAL(uso_abrir_item(hal, &ap.estado, &evento), OK);
    ESPERA_TEXTO(ap.estado.aberto.titulo, "Casamento");

    // O texto do item anterior não vem junto.
    ESPERA_IGUAL(ap.estado.texto.transcricao[0], '\0');

    item_t tarefa = ap.estado.itens[1];
    pc_nuvem_rota_zera();
    ap.estado.nuvem_esperando = NUVEM_NADA;
    ESPERA_IGUAL(uso_marcar(hal, &ap.estado, &tarefa, true), OK);
    ESPERA_CONTEM(pc_nuvem_rota(), "/v1/push");

    ESPERA(ap.estado.itens[1].feita);

    // Não foi para o cartão.
    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, longe, "t:presente", &lido), ERR_ARQUIVO);

    TERMINA();
}

// As MARCAS do mês saem pela mesma rota, e o pedido morre na resposta (de
// carona no pull, desligava o long polling para sempre).
void t_o_mes_pedido_sai_uma_vez_e_morre_na_resposta(void)
{
    COMECA("janela · o mês é pedido uma vez, e a resposta acende a grade");

    liga_pareado();

    data_t d = ap.estado.dia_visto;
    ESPERA_IGUAL(uso_carregar_marcas(hal, &ap.estado), OK);
    ESPERA(ap.estado.mes_pedido[0] != '\0');

    pc_nuvem_rota_zera();
    ap.estado.pull_esperando = false;
    ESPERA_IGUAL(uso_sincronizar(hal, &ap.estado), OK);
    ESPERA(strstr(pc_nuvem_rota(), "mes=") == NULL);

    pc_nuvem_rota_zera();
    ap.estado.nuvem_esperando = NUVEM_NADA;
    ESPERA_IGUAL(uso_olhar(hal, &ap.estado), OK);
    ESPERA_CONTEM(pc_nuvem_rota(), "/v1/olhar?mes=");

    char json[96];
    snprintf(json, sizeof json,
             "{\"mcm\":\"%04d-%02d\",\"mce\":%u,\"mct\":%u,\"dia\":[]}",
             d.ano, d.mes, 1u << 16, 1u << 28);
    pc_nuvem_responde(json);
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);

    ESPERA_IGUAL(uso_carregar_marcas(hal, &ap.estado), OK);
    ESPERA(ap.estado.marcas_evento & (1u << 16));
    ESPERA(ap.estado.marcas_tarefa & (1u << 28));

    ap.estado.mes_pedido[0] = '\0';
    pc_nuvem_rota_zera();
    ap.estado.nuvem_esperando = NUVEM_NADA;
    ESPERA_IGUAL(uso_olhar(hal, &ap.estado), OK);
    ESPERA_IGUAL(pc_nuvem_rota()[0], '\0');

    TERMINA();
}

// A consulta do dia sobrevive ao recarregamento: o índice só tem a janela,
// e `uso_carregar_dia` a apagava.
void t_a_consulta_do_dia_sobrevive_ao_recarregamento(void)
{
    COMECA("janela · o dia consultado não some no próximo quadro");

    liga_pareado();

    data_t longe = data_soma_dias(HOJE, 20);
    uso_ir_para_dia(&ap.estado, longe);
    ESPERA_IGUAL(uso_carregar_dia(hal, &ap.estado), OK);
    uso_pedir_o_dia(&ap.estado);
    ESPERA(ap.estado.dia_pedido[0] != '\0');

    char json[440];
    snprintf(json, sizeof json,
             "{\"dd\":\"%04d-%02d-%02d\",\"dia\":[{\"id\":\"g:casamento\","
             "\"t\":\"Casamento\",\"h\":\"18:00\",\"d\":\"%04d-%02d-%02d\","
             "\"tp\":4,\"o\":\"g\"},{\"id\":\"t:presente\","
             "\"t\":\"Levar presente\",\"d\":\"%04d-%02d-%02d\",\"tp\":2,"
             "\"o\":\"g\"}]}",
             longe.ano, longe.mes, longe.dia,
             longe.ano, longe.mes, longe.dia,
             longe.ano, longe.mes, longe.dia);

    ap.estado.nuvem_esperando = NUVEM_OLHAR;
    pc_nuvem_responde(json);
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_IGUAL(ap.estado.n_itens, 2);

    ESPERA_IGUAL(uso_carregar_dia(hal, &ap.estado), OK);
    ESPERA_IGUAL(ap.estado.n_itens, 2);

    vista_dia_t v;
    vista_dia(&ap.estado, &v);
    ESPERA_IGUAL(v.n_compromissos, 1);
    ESPERA_IGUAL(v.n_tarefas, 1);

    // Andar para outro dia distante descarta a consulta.
    uso_ir_para_dia(&ap.estado, data_soma_dias(longe, 1));
    ESPERA_IGUAL(uso_carregar_dia(hal, &ap.estado), OK);
    ESPERA_IGUAL(ap.estado.n_itens, 0);

    TERMINA();
}

// O CAMINHO inteiro: calendário › dia passado › OK. Os testes de unidade
// passavam e o caminho não estava coberto.
void t_e2e_abrir_um_dia_passado_pede_e_mostra(void)
{
    COMECA("e2e · calendário › dia passado › OK carrega o que o Google tem");

    liga_pareado();
    ap.estado.pilha[++ap.estado.profundidade] = TELA_CALENDARIO;

    data_t passado = data_soma_dias(HOJE, -5);
    uso_ir_para_dia(&ap.estado, passado);
    app_passo(&ap);

    pc_nuvem_rota_zera();
    ESPERA(ap.estado.dia_fora_da_janela);

    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_DIA);

    ESPERA(ap.estado.dia_pedido[0] != '\0');

    // Sai na rota da CONSULTA, na linha do gesto (atrás do long polling, o dia
    // esperava).
    ap.estado.nuvem_esperando = NUVEM_NADA;
    pc_nuvem_rota_zera();
    ESPERA_IGUAL(uso_olhar(hal, &ap.estado), OK);

    char esperado[24];
    snprintf(esperado, sizeof esperado, "dia=%04d-%02d-%02d",
             passado.ano, passado.mes, passado.dia);
    ESPERA(strstr(pc_nuvem_rota(), esperado) != NULL);
    ESPERA_CONTEM(pc_nuvem_rota(), "/v1/olhar");

    char json[440];
    snprintf(json, sizeof json,
             "{\"dd\":\"%04d-%02d-%02d\",\"dia\":[{\"id\":\"g:reuniao\","
             "\"t\":\"Reunião do time\",\"h\":\"09:00\","
             "\"d\":\"%04d-%02d-%02d\",\"tp\":4,\"o\":\"g\"}]}",
             passado.ano, passado.mes, passado.dia,
             passado.ano, passado.mes, passado.dia);

    ap.estado.nuvem_esperando = NUVEM_OLHAR;
    pc_nuvem_responde(json);
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.n_itens, 1);

    vista_dia_t v;
    vista_dia(&ap.estado, &v);
    ESPERA_IGUAL(v.n_compromissos, 1);

    TERMINA();
}

// ── o nome é um só, no aparelho e no aplicativo ──────────────────────
static void responde_pull_com_nome(const char *nome)
{
    char j[160];
    snprintf(j, sizeof j, "{\"itens\":[],\"removidos\":[],\"mais\":false,"
                          "\"aparelho_nome\":\"%s\"}", nome);
    pc_nuvem_responde(j);
    (void)uso_nuvem_resposta(hal, &ap.estado);
}

static void troca_o_nome_aqui(const char *nome)
{
    ESPERA_IGUAL(perfil_renomeia(hal, nome, true), OK);
    ap.estado.inicio.nome_sobe = true;
    snprintf(ap.estado.inicio.nome_pendente,
             sizeof ap.estado.inicio.nome_pendente, "%s", nome);
}

void t_o_nome_trocado_aqui_sobe_e_e_confirmado(void)
{
    COMECA("nome · trocado aqui, sobe no pull e a marca sai ao confirmar");

    liga_pareado();
    troca_o_nome_aqui("Tinto da família");

    (void)uso_sincronizar(hal, &ap.estado);
    ESPERA_CONTEM(pc_nuvem_rota(), "nome=Tinto%20da%20fam%C3%ADlia");

    responde_pull_com_nome("Tinto da família");
    ESPERA(!ap.estado.inicio.nome_sobe);
    perfil_local_t p;
    ESPERA_IGUAL(perfil_carrega(hal, &p), OK);
    ESPERA(!p.nome_sobe);

    (void)uso_sincronizar(hal, &ap.estado);
    ESPERA_SEM(pc_nuvem_rota(), "nome=");
    TERMINA();
}

void t_o_nome_trocado_no_aplicativo_chega_ao_aparelho(void)
{
    COMECA("nome · sem troca local, o aparelho adota o do servidor");

    liga_pareado();
    ESPERA_IGUAL(perfil_renomeia(hal, "Mesa", false), OK);
    snprintf(ap.estado.inicio.nome_pendente,
             sizeof ap.estado.inicio.nome_pendente, "%s", "Mesa");

    (void)uso_sincronizar(hal, &ap.estado);
    responde_pull_com_nome("Cozinha");

    ESPERA_TEXTO(ap.estado.inicio.nome_pendente, "Cozinha");
    perfil_local_t p;
    ESPERA_IGUAL(perfil_carrega(hal, &p), OK);
    ESPERA_TEXTO(p.nome, "Cozinha");
    TERMINA();
}

// Depois de formatar, o nome digitado é troca local e vence o antigo do
// servidor (o token, na flash, mantém o vínculo).
void t_nome_novo_depois_de_formatar_nao_perde_para_o_antigo(void)
{
    COMECA("nome · depois de formatar, o nome novo vence o antigo do servidor");

    liga_pareado();
    troca_o_nome_aqui("Novo");

    (void)uso_sincronizar(hal, &ap.estado);
    // Uma resposta atrasada com o nome antigo não desfaz a troca.
    responde_pull_com_nome("Antigo");
    ESPERA_TEXTO(ap.estado.inicio.nome_pendente, "Novo");
    ESPERA(ap.estado.inicio.nome_sobe);

    (void)uso_sincronizar(hal, &ap.estado);
    ESPERA_CONTEM(pc_nuvem_rota(), "nome=Novo");
    TERMINA();
}

void t_servidor_sem_nome_recebe_o_do_aparelho(void)
{
    COMECA("nome · servidor sem nome recebe o do aparelho no pull seguinte");

    liga_pareado();
    ESPERA_IGUAL(perfil_renomeia(hal, "Mesa", false), OK);
    snprintf(ap.estado.inicio.nome_pendente,
             sizeof ap.estado.inicio.nome_pendente, "%s", "Mesa");

    (void)uso_sincronizar(hal, &ap.estado);
    responde_pull_com_nome("");
    ESPERA(ap.estado.inicio.nome_sobe);

    (void)uso_sincronizar(hal, &ap.estado);
    ESPERA_CONTEM(pc_nuvem_rota(), "nome=Mesa");
    TERMINA();
}

// ── anotações, com a resposta de VERDADE do servidor ────────────────
// Chegam com `n:1` e o servidor dá o `an:`: duas no mesmo dia não viram uma.
static void anota_pela_voz(const char *falou)
{
    ESPERA_IGUAL(uso_enviar_captura(hal, &ap.estado, "/x.wav"), OK);
    char corpo[400];
    snprintf(corpo, sizeof corpo,
             "{\"falou\":\"%s\",\"nota\":\"nota-x\",\"acoes\":["
             "{\"v\":\"anotou\",\"id\":\"n:1\",\"tp\":1,\"t\":\"%s\"}]}",
             falou, falou);
    pc_nuvem_responde(corpo);
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_IGUAL(uso_confirmar_resultado(hal, &ap.estado), OK);
    static int n;
    char resposta[64];
    snprintf(resposta, sizeof resposta, "{\"ok\":true,\"id\":\"an:%d\"}", ++n);
    pc_nuvem_responde(resposta);
    (void)uso_nuvem_resposta(hal, &ap.estado);
    ap.estado.esperando_resultado = false;   // o app fecha o Resultado
}

void t_duas_anotacoes_no_mesmo_dia_sao_duas(void)
{
    COMECA("anotações · duas falas no mesmo dia viram duas anotações");

    liga_pareado();
    anota_pela_voz("pensei no encoder");
    anota_pela_voz("ideia para o case");

    ESPERA_IGUAL(uso_carregar_anotacoes(hal, &ap.estado), OK);
    ESPERA_IGUAL(ap.estado.n_anotacoes, 2);
    TERMINA();
}

// Abrir a anotação mostra o que foi dito, sem "por estruturar".
void t_abrir_a_anotacao_mostra_o_que_foi_dito(void)
{
    COMECA("anotações · abrir mostra a transcrição, sem \"por estruturar\"");

    liga_pareado();
    anota_pela_voz("pensei no encoder no lugar do cinco vias");

    ESPERA_IGUAL(uso_carregar_anotacoes(hal, &ap.estado), OK);
    ESPERA_IGUAL(ap.estado.n_anotacoes, 1);
    ESPERA_IGUAL(uso_abrir_item(hal, &ap.estado, &ap.estado.anotacoes[0]), OK);

    vista_nota_t v;
    vista_nota(&ap.estado, &v);
    ESPERA_CONTEM(v.transcricao, "encoder no lugar do cinco vias");
    ESPERA_TEXTO(v.aviso, "");
    TERMINA();
}

// ── a lista de anotações: página, ordem, e abrir ────────────────────
static void grava_anotacao(int dia, const char *hora, const char *titulo)
{
    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id, sizeof it.id, "a:%02d%c%c%c%c", dia, hora[0], hora[1],
             hora[3], hora[4]);
    snprintf(it.titulo, sizeof it.titulo, "%s", titulo);
    snprintf(it.hora, sizeof it.hora, "%s", hora);
    it.tipo   = TIPO_ANOTACAO;
    it.origem = ORIGEM_AQUI;
    it.dia    = (data_t){2026, 8, (int8_t)dia};
    ESPERA_IGUAL(cartao_grava_item(hal, it.dia, &it), OK);
}

static void abre_as_anotacoes(void)
{
    ap.estado.pilha[++ap.estado.profundidade] = TELA_ANOTACOES;
    ap.estado.anotacoes_pagina = 0;
    ap.estado.cursor = 0;
    ESPERA_IGUAL(uso_carregar_anotacoes(hal, &ap.estado), OK);
}

void t_a_lista_de_anotacoes_pagina_e_abre(void)
{
    COMECA("anotações · seis por página, mais novas primeiro, e OK abre");

    liga_pareado();
    // Oito, gravadas fora de ordem.
    grava_anotacao(10, "09:00", "dez cedo");
    grava_anotacao(12, "18:00", "doze tarde");
    grava_anotacao(11, "08:00", "onze");
    grava_anotacao(12, "07:00", "doze cedo");
    grava_anotacao(9,  "10:00", "nove");
    grava_anotacao(8,  "10:00", "oito");
    grava_anotacao(7,  "10:00", "sete");
    grava_anotacao(6,  "10:00", "seis");
    abre_as_anotacoes();

    ESPERA_IGUAL(ap.estado.anotacoes_total, 8);
    ESPERA_IGUAL(ap.estado.n_anotacoes, 6);
    ESPERA_TEXTO(ap.estado.anotacoes[0].titulo, "doze tarde");
    ESPERA_TEXTO(ap.estado.anotacoes[1].titulo, "doze cedo");
    ESPERA_TEXTO(ap.estado.anotacoes[5].titulo, "oito");

    vista_anotacoes_t v;
    vista_anotacoes(&ap.estado, &v);
    ESPERA_IGUAL(v.pagina, 1);
    ESPERA_IGUAL(v.paginas, 2);

    for (int i = 0; i < 6; i++) { pc_botao(IN_BAIXO); app_passo(&ap); }
    ESPERA_IGUAL(ap.estado.anotacoes_pagina, 1);
    ESPERA_IGUAL(ap.estado.n_anotacoes, 2);
    ESPERA_TEXTO(ap.estado.anotacoes[1].titulo, "seis");
    ESPERA_IGUAL(ap.estado.cursor, 0);

    pc_botao(IN_ESQ); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.anotacoes_pagina, 0);

    pc_botao(IN_BAIXO); app_passo(&ap);
    pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_NOTA);
    ESPERA_TEXTO(ap.estado.aberto.titulo, "doze cedo");
    TERMINA();
}

void t_confirmar_uma_anotacao_abre_o_pronto(void)
{
    COMECA("anotações · confirmar uma fala que vira anotação abre o Pronto");

    liga_pareado();
    ESPERA_IGUAL(uso_enviar_captura(hal, &ap.estado, "/x.wav"), OK);
    pc_nuvem_responde("{\"falou\":\"uma ideia\",\"nota\":\"nt:abc\",\"acoes\":["
                      "{\"v\":\"anotou\",\"id\":\"n:1\",\"tp\":1,"
                      "\"t\":\"Uma ideia\"}]}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ap.estado.pilha[++ap.estado.profundidade] = TELA_CONFERIR;
    ap.estado.cursor = (int16_t)ap.estado.n_resultados;

    pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_RESULTADO);
    ESPERA(!ap.estado.esperando_resultado);
    TERMINA();
}

// "anotou" sem `tp` vira anotação e não vai ao Google (subia como gesto sem
// tipo e sumia).
void t_anotou_sem_tipo_vira_anotacao_e_nao_sobe(void)
{
    COMECA("anotações · \"anotou\" sem tipo vira anotação, e sobe ao servidor");

    liga_pareado();
    ESPERA_IGUAL(uso_enviar_captura(hal, &ap.estado, "/x.wav"), OK);
    pc_nuvem_responde("{\"falou\":\"uma ideia solta\",\"nota\":\"nt:ab12\","
                      "\"acoes\":[{\"v\":\"anotou\",\"id\":\"n:1\","
                      "\"t\":\"Uma ideia solta\"}]}");
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_IGUAL(ap.estado.resultados[0].item.tipo, TIPO_ANOTACAO);

    pc_nuvem_rota_zera();
    ESPERA_IGUAL(uso_confirmar_resultado(hal, &ap.estado), OK);
    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/push");
    ESPERA_CONTEM(pc_nuvem_corpo(), "\"tp\":1");    // vai como ANOTAÇÃO
    ESPERA_CONTEM(pc_nuvem_corpo(), "\"nota\":\"nt:ab12\"");   // e de que fala veio

    ESPERA_IGUAL(uso_carregar_anotacoes(hal, &ap.estado), OK);
    ESPERA_IGUAL(ap.estado.n_anotacoes, 1);
    TERMINA();
}

// O que nasce por voz e ganha id do Google sobrevive ao boot.
static void cria_pela_voz_e_reinicia(int tipo, const char *id_google)
{
    liga_pareado();
    ESPERA_IGUAL(uso_enviar_captura(hal, &ap.estado, "/x.wav"), OK);
    char corpo[400];
    snprintf(corpo, sizeof corpo,
             "{\"falou\":\"ir ao shopping as 8\",\"nota\":\"nt:sh1\",\"acoes\":["
             "{\"v\":\"criou\",\"id\":\"n:1\",\"tp\":%d,\"t\":\"Shopping\","
             "\"h\":\"20:00\",\"d\":\"%04d-%02d-%02d\"}]}", tipo,
             ap.estado.hoje.ano, ap.estado.hoje.mes, ap.estado.hoje.dia);
    pc_nuvem_responde(corpo);
    ESPERA_IGUAL(uso_nuvem_resposta(hal, &ap.estado), OK);
    ESPERA_IGUAL(uso_confirmar_resultado(hal, &ap.estado), OK);
    char resp[80];
    snprintf(resp, sizeof resp, "{\"ok\":true,\"id\":\"%s\"}", id_google);
    pc_nuvem_responde(resp);
    (void)uso_nuvem_resposta(hal, &ap.estado);

    app_liga(&ap, hal);                       // reiniciou
    ap.estado.n_itens = 0;
    ESPERA_IGUAL(uso_carregar_dia(hal, &ap.estado), OK);
    bool achou = false;
    for (int i = 0; i < ap.estado.n_itens; i++)
        if (strstr(ap.estado.itens[i].titulo, "Shopping")) achou = true;
    ESPERA(achou);
}

void t_evento_falado_sobrevive_ao_reinicio(void)
{
    COMECA("voz · evento criado aqui continua no dia depois de reiniciar");
    cria_pela_voz_e_reinicia(4, "g:shop123");
    TERMINA();
}

void t_tarefa_com_hora_falada_sobrevive_ao_reinicio(void)
{
    COMECA("voz · tarefa com hora criada aqui continua no dia depois de reiniciar");
    cria_pela_voz_e_reinicia(2, "t:shop123");
    TERMINA();
}

// Boot sem hora não poda o cartão: com o relógio zerado, a poda apagava os
// eventos do Google.
void t_boot_sem_hora_nao_poda_o_cartao(void)
{
    COMECA("boot · sem hora confiável, a poda não apaga os eventos do Google");
    liga_pareado();
    item_t ev;
    memset(&ev, 0, sizeof ev);
    snprintf(ev.id, sizeof ev.id, "%s", "g:shop1");
    snprintf(ev.titulo, sizeof ev.titulo, "%s", "Shopping");
    snprintf(ev.hora, sizeof ev.hora, "%s", "20:00");
    ev.tipo = TIPO_EVENTO;
    ev.dia  = ap.estado.hoje;
    ESPERA_IGUAL(cartao_grava_item(hal, ev.dia, &ev), OK);
    data_t dia = ev.dia;

    pc_relogio((data_t){1970, 1, 1}, 0, 5);   // ligou na tomada
    app_liga(&ap, hal);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, dia, "g:shop1", &lido), OK);
    TERMINA();
}
