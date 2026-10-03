// firmware/testes/t_voz.c — a voz, entrando pelo BOTÃO.
// Nenhum teste chama uso_ direto: o defeito que interessa é o botão chegar
// na função.
#include "teste.h"
#include "ui/voz.h"
#include "uso/nuvem.h"
#include "vista/gravador.h"
#include "uso/uso.h"
#include "dado/cartao.h"
#include "vista/gravador.h"
#include "vista/conferir.h"
#include "vista/conta.h"

static app_t        ap;
static const hal_t *hal;

#define HOJE ((data_t){2026, 8, 18})

// Falar exige rede (o ● recusa sem ela; a recusa tem teste próprio).
static void liga(void)
{
    hal = pc_liga();
    pc_relogio(HOJE, 14, 22);
    app_liga(&ap, hal);
    ap.estado.rede = REDE_LIGADA;
    app_passo(&ap);

    // Mede o ciclo da fala dentro da Agenda.
    ENTRA_NA_AGENDA(&ap);
}

// A resposta da captura com UMA ação: sem ela o ciclo não termina.
static void o_servidor_entendeu(void)
{
    pc_nuvem_responde(
        "{\"falou\":\"marca dentista quinta as tres\",\"nota\":\"n1\","
        "\"acoes\":[{\"v\":\"criou\",\"id\":\"n:1\",\"tp\":4,"
        "\"t\":\"Dentista\",\"d\":\"2026-08-18\",\"h\":\"15:00\"}]}");
}

// Pela CONSTANTE do prazo, nunca um número à mão (subir o prazo faria o
// teste passar sem chegar lá).
static void ate_o_prazo_estourar(void);

// Um passo por tique, como na placa (o passo tem teto de eventos).
static void segundos(int n)
{
    for (int i = 0; i < n; i++) {
        pc_tick();
        pc_avanca_ms(1000);
        app_passo(&ap);
    }
}

static void ate_o_prazo_estourar(void)
{
    segundos((int)(ESTRUTURA_PRAZO_MS / 1000u) + 3);
}

// Sair do Conferir é decidir: descartando, quando ele não é o assunto.
static void sai_do_conferir_descartando(void)
{
    for (int i = 0; i < 6; i++) pc_botao(IN_BAIXO);   // até "Descartar"
    app_passo(&ap);
    pc_botao(IN_OK);
    app_passo(&ap);
}

void t_voz_bloqueada_pede_desbloqueio(void)
{
    COMECA("RN-3H · bloqueado, voz só pede para desbloquear");

    liga();
    pc_botao(IN_POWER);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_BLOQUEADA);
    ESPERA(ap.estado.travado);

    int l = 0, a = 0;
    const uint8_t *tela = pc_tela(&l, &a);
    size_t bytes = (size_t)((l + 7) / 8) * (size_t)a;
    uint8_t repouso[(TELA_L + 7) / 8 * TELA_A];
    memcpy(repouso, tela, bytes);

    pc_botao(IN_VOZ);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_PARADA);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_BLOQUEADA);
    ESPERA(ap.estado.travado);
    ESPERA(memcmp(repouso, pc_tela(NULL, NULL), bytes) != 0);

    // Direção e laterais podem acordar o chip, mas não desbloqueiam nem agem.
    int cursor = ap.estado.cursor;
    pc_botao(IN_CIMA);
    pc_botao(IN_DIR);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_BLOQUEADA);
    ESPERA_IGUAL(ap.estado.cursor, cursor);

    pc_avanca_ms(2100);
    pc_tick();
    app_passo(&ap);
    ESPERA(memcmp(repouso, pc_tela(NULL, NULL), bytes) == 0);

    TERMINA();
}

// ── o ciclo inteiro ─────────────────────────────────────────────────
void t_falar_pausar_retomar_e_confirmar(void)
{
    COMECA("o ciclo da fala: grava, pausa, retoma e confirma");

    liga();

    pc_botao(IN_VOZ);            // ● começa
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_GRAVANDO);
    ESPERA(ap.estado.travado);     // RN-14: o direcional trava

    // Gravar NÃO abre tela nem empilha: a faixa sobe sobre a tela.
    ESPERA_IGUAL(ap.estado.profundidade, 1);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_NADA);

    segundos(9);
    ESPERA_IGUAL(ap.estado.gravacao.ms, 9000);

    pc_botao(IN_VOZ);            // solta: PAUSA, não termina (RN-12)
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_PAUSADA);

    pc_botao(IN_VOZ);            // retoma
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_GRAVANDO);
    // RN-13: dois trechos provam que a retomada entrou no MESMO arquivo.
    ESPERA_IGUAL(ap.estado.gravacao.trechos, 2);
    ESPERA_IGUAL(pc_gravando(), true);

    segundos(5);
    o_servidor_entendeu();
    pc_botao(IN_OK);             // só o OK finaliza
    app_passo(&ap);
    app_passo(&ap);              // a resposta chega no passo seguinte

    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_PARADA);
    ESPERA(!ap.estado.travado);

    // Confirmar abre o Conferir em tela cheia, não pop-over.
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_NADA);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_CONFERIR);

    // E não empilha: o resultado guarda a pilha inteira.
    ESPERA_IGUAL(ap.estado.profundidade, 0);
    ESPERA(ap.estado.veio_da_voz);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, HOJE, "1422-fala", &lido), OK);
    ESPERA_IGUAL(lido.dur_s, 14);
    ESPERA_IGUAL(lido.tipo, TIPO_NADA);        // RN-11: nasce SEM TIPO
    ESPERA_IGUAL(lido.origem, ORIGEM_AQUI);

    TERMINA();
}

void t_o_gesto_fisico_da_voz_respeita_o_modo_escolhido(void)
{
    COMECA("botão físico de voz distingue segurar de um toque");

    liga();

    // PTT: apertar começa; soltar pausa.
    ap.estado.config.valor[AJUSTE_VOZ_SEGURAR] = 1;
    pc_empurra((evento_t){ .tipo = EV_BOTAO_APERTO, .botao = IN_VOZ });
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_GRAVANDO);
    pc_botao(IN_VOZ);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_PAUSADA);

    // Um toque: apertar não muda nada; cada soltura alterna.
    ap.estado.config.valor[AJUSTE_VOZ_SEGURAR] = 0;
    pc_empurra((evento_t){ .tipo = EV_BOTAO_APERTO, .botao = IN_VOZ });
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_PAUSADA);
    pc_botao(IN_VOZ);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_GRAVANDO);

    TERMINA();
}

// RN-11: gravar nunca exige escolha; o item nasce cru.
void t_capturar_nao_pergunta_nada(void)
{
    COMECA("RN-11 · capturar não pede tipo, destino nem data");

    liga();
    pc_botao(IN_VOZ);
    app_passo(&ap);
    segundos(3);
    o_servidor_entendeu();
    pc_botao(IN_OK);
    app_passo(&ap);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, HOJE, "1422-fala", &lido), OK);
    ESPERA_IGUAL(lido.tipo, TIPO_NADA);
    ESPERA_TEXTO(lido.titulo, "");
    ESPERA_IGUAL(lido.vence.ano, 0);

    TERMINA();
}

// ● funciona de QUALQUER tela.
void t_grava_de_qualquer_tela(void)
{
    COMECA("o ● grava de qualquer tela, sem voltar pra home antes");

    liga();

    // Com a gaveta aberta, gravar FECHA a gaveta.
    pc_botao(IN_MENU);
    pc_botao(IN_VOZ);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_GRAVANDO);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_NADA);

    o_servidor_entendeu();
    pc_botao(IN_OK);             // salva, e a resposta fecha a faixa
    app_passo(&ap);
    app_passo(&ap);

    sai_do_conferir_descartando();

    // Dentro de uma tela, o gravador vem por cima. A tarefa existe para haver o
    // que abrir.
    item_t tar;
    memset(&tar, 0, sizeof tar);
    snprintf(tar.id,     sizeof tar.id,     "%s", "0900-ipva");
    snprintf(tar.titulo, sizeof tar.titulo, "%s", "Pagar IPVA");
    tar.tipo = TIPO_TAREFA;
    tar.dia  = HOJE;
    ESPERA_IGUAL(cartao_grava_item(hal, HOJE, &tar), OK);

    // Escrever por fora do caso de uso derruba as bandeiras à mão.
    ap.estado.itens_validos    = false;
    ap.estado.pendentes_validas = false;

    // E desenha: quem relê o cartão é a montagem do quadro.
    ap.precisa_desenhar = true;
    app_desenha(&ap);

    pc_botao(IN_DIR);            // ▶ abre a tarefa sob o cursor
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.profundidade, 2);   // Home → Agenda → tarefa

    pc_botao(IN_VOZ);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_GRAVANDO);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_GRAVANDO);
    ESPERA_IGUAL(ap.estado.profundidade, 2);   // a pilha não mexeu

    TERMINA();
}

// RN-A2: confirmação só para o que destrói, com o "não" pré-selecionado.
void t_descartar_pergunta_com_o_nao_selecionado(void)
{
    COMECA("RN-A2 · descartar pergunta, e o \"não\" vem selecionado");

    liga();
    pc_botao(IN_VOZ);
    app_passo(&ap);
    segundos(8);

    pc_botao(IN_VOLTAR);         // ◀ gravando = descartar
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_DESCARTAR);
    ESPERA_IGUAL(ap.estado.cursor_overlay, 0);   // o "não"

    vista_grav_t v;
    vista_gravador(&ap.estado, &v);
    ESPERA(v.confirmando);
    ESPERA(!v.sim_selecionado);
    // O que se perde EM NÚMERO.
    ESPERA_TEXTO(v.perde, "8 segundos.");

    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_NADA);
    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_GRAVANDO);

    TERMINA();
}

void t_descartar_confirmado_nao_deixa_rastro(void)
{
    COMECA("descartado, não sobra item, nem WAV, nem linha na fila");

    liga();
    pc_botao(IN_VOZ);
    app_passo(&ap);
    segundos(8);

    pc_botao(IN_VOLTAR);
    pc_botao(IN_BAIXO);          // vai pro "sim"
    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_PARADA);
    ESPERA(!ap.estado.travado);

    item_t lido;
    ESPERA(cartao_le_item(hal, HOJE, "1422-fala", &lido) != OK);
    ESPERA(!pc_tem_arquivo("/TINTO/itens/2026-08-18/1422-fala/audio.wav"));

    // Nada sobe: o servidor nunca soube desta fala.

    TERMINA();
}

// Pausado, a onda para e a faixa fica vazada em vez de sumir. A onda não
// mede a voz.
void t_pausado_o_medidor_zera_mas_a_tela_fica(void)
{
    COMECA("T-12 · pausado, a onda para e os trechos aparecem");

    liga();
    pc_botao(IN_VOZ);
    app_passo(&ap);
    segundos(20);

    vista_grav_t v;
    vista_gravador(&ap.estado, &v);
    ESPERA_TEXTO(v.estado, "Gravando");
    ESPERA_TEXTO(v.tempo,  "0:20");
    ESPERA(!v.pausado);
    ESPERA_TEXTO(v.trecho_txt, "");        // um trecho só: não anuncia

    pc_botao(IN_VOZ);
    app_passo(&ap);
    pc_botao(IN_VOZ);                     // retoma: agora são dois
    app_passo(&ap);
    segundos(18);

    vista_gravador(&ap.estado, &v);
    ESPERA_TEXTO(v.estado, "Gravando");
    ESPERA_TEXTO(v.trecho_txt, "2 trechos · 0:38");

    pc_botao(IN_VOZ);                     // pausa de novo
    app_passo(&ap);
    vista_gravador(&ap.estado, &v);
    ESPERA_TEXTO(v.estado, "Pausado");
    ESPERA(v.pausado);
    ESPERA_IGUAL(v.onda, 0);                // parada, não sumida

    TERMINA();
}

// RN-95: o log registra duração, NUNCA conteúdo.
void t_o_log_registra_duracao_e_nunca_conteudo(void)
{
    COMECA("RN-95 · o log tem duração e código, nunca o que foi falado");

    liga();
    pc_botao(IN_VOZ);
    app_passo(&ap);
    segundos(37);
    pc_botao(IN_OK);
    app_passo(&ap);

    char log[512];
    ESPERA_IGUAL(hal->ler("/TINTO/sistema/log.txt", log, sizeof log), OK);

    ESPERA(strstr(log, "ligou")  != NULL);
    ESPERA(strstr(log, "gravou") != NULL);
    ESPERA(strstr(log, "37")     != NULL);   // a duração
    ESPERA(strstr(log, "14:22")  != NULL);   // a hora do gesto

    ESPERA(strstr(log, "fala") == NULL);
    ESPERA(strstr(log, ".wav") == NULL);

    TERMINA();
}

// Depois do OK a faixa estrutura e o aparelho trava.

void t_depois_do_ok_a_faixa_estrutura_e_o_aparelho_trava(void)
{
    COMECA("T-30 · o OK põe a faixa em Estruturando, e trava o aparelho");

    liga();
    ap.estado.rede = REDE_LIGADA;
    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", "Convidado");

    // A resposta DEMORA, senão a espera não dura um quadro.
    pc_nuvem_demora(true);

    pc_botao(IN_VOZ);
    app_passo(&ap);
    segundos(41);

    // A linha está livre ao soltar o botão (o registro, de propósito, nunca é
    // respondido aqui).
    ap.estado.nuvem_esperando = NUVEM_NADA;
    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/captura");
    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_ESTRUTURANDO);

    // A faixa continua: nenhuma tela cheia para dizer "aguarde".
    ESPERA(ap.estado.pilha[ap.estado.profundidade] != TELA_CONFERIR);

    vista_grav_t v;
    vista_gravador(&ap.estado, &v);
    ESPERA(v.estruturando);
    ESPERA_TEXTO(v.titulo, "Estruturando");
    ESPERA_CONTEM(v.aviso, "Estruturando");
    ESPERA_TEXTO(v.rodape_dir, "aguarde");

    // O tempo continua na tela durante a espera.
    ESPERA_TEXTO(v.tempo, "0:41");

    // Travado, e sem reverter: descartar é no Conferir.
    ESPERA(ap.estado.travado);

    pc_botao(IN_VOLTAR);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_NADA);   // nada de "descartar?"
    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_ESTRUTURANDO);

    pc_botao(IN_VOZ);                                 // nem grava por cima
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_ESTRUTURANDO);

    TERMINA();
}

// A resposta fecha a faixa e abre o Conferir.
void t_a_resposta_fecha_a_faixa_e_abre_o_recibo(void)
{
    COMECA("a resposta chegou: a faixa some e Conferir ocupa a tela");

    liga();
    ap.estado.rede = REDE_LIGADA;
    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", "Convidado");

    pc_botao(IN_VOZ);
    app_passo(&ap);
    segundos(41);

    pc_nuvem_responde(
        "{\"falou\":\"marca dentista quinta as tres\",\"nota\":\"n1\","
        "\"acoes\":[{\"v\":\"criou\",\"id\":\"n:1\",\"tp\":4,"
        "\"t\":\"Dentista\",\"d\":\"2026-08-18\",\"h\":\"15:00\"}]}");

    pc_botao(IN_OK);
    app_passo(&ap);
    app_passo(&ap);          // o evento de rede chega no passo seguinte

    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_PARADA);
    ESPERA(!ap.estado.travado);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_CONFERIR);

    vista_conferir_t v;
    vista_conferir(&ap.estado, &v);
    ESPERA_TEXTO(v.titulo, "Conferir");
    ESPERA_IGUAL(v.n_res, 1);
    ESPERA_TEXTO(v.res[0].titulo, "Dentista");

    // "pegou o quê, e quanto?" não depende da IA.
    ESPERA_CONTEM(v.trechos, "0:41");

    TERMINA();
}

// O prazo: sem resposta, a faixa destrava, diz onde a fala está e a
// gravação sai do cartão.
void t_sem_resposta_a_faixa_destrava_e_diz_onde_a_fala_esta(void)
{
    COMECA("a resposta não veio: a faixa destrava e descarta a gravação");

    liga();
    ap.estado.rede = REDE_LIGADA;
    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", "Convidado");

    pc_nuvem_demora(true);

    pc_botao(IN_VOZ);
    app_passo(&ap);
    segundos(10);
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA(ap.estado.travado);

    segundos((int)(ESTRUTURA_PRAZO_MS / 1000u) - 3);
    ESPERA(ap.estado.travado);          // ainda dentro do prazo

    segundos(6);
    ESPERA(!ap.estado.travado);
    ESPERA(ap.estado.gravacao.sem_resposta);

    vista_grav_t v;
    vista_gravador(&ap.estado, &v);
    ESPERA_TEXTO(v.titulo, "Sem resposta");
    ESPERA_CONTEM(v.aviso, "descartada");
    ESPERA_TEXTO(v.rodape_dir, "OK fechar");

    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_PARADA);
    ESPERA(!ap.estado.travado);

    TERMINA();
}

// Hal sem microfone não derruba o aparelho: na placa um papel faltando
// reiniciava a cada ●.
void t_hal_sem_microfone_nao_derruba_o_aparelho(void)
{
    COMECA("RN-A1 · sem microfone o aparelho diz, e não reinicia");

    hal_t mudo;
    memset(&mudo, 0, sizeof mudo);   // exatamente o hal da placa antes disto

    estado_t e;
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 8, 21};
    e.hora = 9; e.minuto = 14;

    ESPERA_IGUAL(uso_gravar_comeca(&mudo, &e), ERR_INTERNO);
    ESPERA_IGUAL(e.gravacao.fase, GRAV_PARADA);

    ESPERA_IGUAL(uso_gravar_pausa(&mudo, &e), ERR_INTERNO);
    ESPERA_IGUAL(uso_gravar_retoma(&mudo, &e), ERR_INTERNO);

    TERMINA();
}

// Falar do fundo da navegação e voltar exatamente para lá (o resultado
// guarda a pilha, não empilha).
void t_a_voz_devolve_a_tela_de_onde_se_falou(void)
{
    COMECA("falar de dentro de uma tela devolve exatamente ela no fim");

    liga();

    pc_botao(IN_MENU);
    pc_botao(IN_OK);
    app_passo(&ap);
    int fundo    = ap.estado.profundidade;
    tela_id onde = ap.estado.pilha[fundo];
    ESPERA(fundo > 0);

    pc_botao(IN_VOZ);
    app_passo(&ap);
    o_servidor_entendeu();
    pc_botao(IN_OK);                 // finaliza: o resultado toma a tela
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_CONFERIR);

    sai_do_conferir_descartando();     // e fecha, decidindo

    ESPERA_IGUAL(ap.estado.profundidade, fundo);
    ESPERA_IGUAL(ap.estado.pilha[fundo], onde);
    ESPERA(!ap.estado.veio_da_voz);
    TERMINA();
}

// O BACK segurado (pânico, RN-38) vence a volta da voz.
void t_back_segurado_encerra_a_volta_da_voz(void)
{
    COMECA("RN-38 · BACK segurado leva pra Home, mesmo vindo da voz");

    liga();
    pc_botao(IN_MENU);
    pc_botao(IN_OK);
    app_passo(&ap);

    pc_botao(IN_VOZ);
    app_passo(&ap);
    pc_botao(IN_OK);
    app_passo(&ap);

    pc_segura(IN_VOLTAR, 900);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.profundidade, 0);

    // O pânico leva à Home.
    ESPERA_IGUAL(ap.estado.pilha[0], TELA_HOME);
    ESPERA(!ap.estado.veio_da_voz);
    TERMINA();
}


// Sem rede o ● recusa NO GESTO, antes de a ideia ser dita.
void t_sem_rede_o_botao_de_voz_recusa_na_hora(void)
{
    COMECA("sem rede o ● não grava, e a faixa diz por quê na hora");

    hal = pc_liga();
    pc_relogio(HOJE, 14, 22);
    app_liga(&ap, hal);
    ap.estado.rede = REDE_DESLIGADA;
    app_passo(&ap);

    pc_botao(IN_VOZ);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_PARADA);
    ESPERA(ap.estado.precisa_rede[0]);
    ESPERA(!pc_gravando());          // e o microfone nem foi aberto

    // O aviso some no botão seguinte.
    pc_botao(IN_BAIXO);
    app_passo(&ap);
    ESPERA(!ap.estado.precisa_rede[0]);
    TERMINA();
}

// Com a rede de volta, o mesmo botão grava.
void t_com_a_rede_de_volta_o_mesmo_botao_grava(void)
{
    COMECA("a rede voltou: o mesmo ● passa a gravar");

    hal = pc_liga();
    pc_relogio(HOJE, 14, 22);
    app_liga(&ap, hal);
    ap.estado.rede = REDE_DESLIGADA;
    app_passo(&ap);

    pc_botao(IN_VOZ);
    app_passo(&ap);
    ESPERA(ap.estado.precisa_rede[0]);

    ap.estado.rede = REDE_LIGADA;
    pc_botao(IN_VOZ);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_GRAVANDO);
    ESPERA(!ap.estado.precisa_rede[0]);
    TERMINA();
}

// Pausar não fala com o servidor (transcrever por pausa cobrava o áudio
// várias vezes).
void t_pausar_nao_gasta_whisper(void)
{
    COMECA("pausar não pede nada ao servidor — o áudio sobe UMA vez");

    liga();
    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", "Convidado");

    pc_botao(IN_VOZ);
    app_passo(&ap);
    segundos(6);

    pc_nuvem_rota_zera();
    pc_botao(IN_VOZ);            // pausa
    app_passo(&ap);
    ESPERA_TEXTO(pc_nuvem_rota(), "");

    pc_botao(IN_VOZ);            // retoma
    app_passo(&ap);
    segundos(4);
    pc_botao(IN_VOZ);            // pausa de novo
    app_passo(&ap);
    ESPERA_TEXTO(pc_nuvem_rota(), "");

    // E o OK manda o áudio UMA vez.
    o_servidor_entendeu();
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/captura");

    TERMINA();
}

// A faixa sem resposta é aviso: sai com qualquer botão, senão o ● seguinte
// seria engolido.
void t_a_faixa_sem_resposta_sai_com_qualquer_botao(void)
{
    COMECA("a faixa de sem resposta sai no próximo botão, e ele vale");

    liga();
    ap.estado.rede = REDE_LIGADA;
    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", "Convidado");
    pc_nuvem_demora(true);

    pc_botao(IN_VOZ);
    app_passo(&ap);
    segundos(5);
    pc_botao(IN_OK);
    app_passo(&ap);
    ate_o_prazo_estourar();
    ESPERA(ap.estado.gravacao.sem_resposta);

    // O ● dispensa a faixa E grava.
    pc_botao(IN_VOZ);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_GRAVANDO);
    ESPERA(!ap.estado.gravacao.sem_resposta);

    TERMINA();
}

// O FIFO da faixa: as primeiras palavras saem, o fim nunca é cortado.
void t_a_transcricao_corre_como_fifo(void)
{
    COMECA("a transcrição mostra o FIM: as antigas saem, as novas entram");

    const int larg = 220, linhas = 2;

    // Curta: cabe inteira.
    const char *curta = "marca dentista";
    ESPERA(ui_voz_visivel(curta, larg, linhas) == curta);

    // Longa: as primeiras palavras saíram.
    const char *longa =
        "marca dentista quinta as tres da tarde e lembra de comprar pasta "
        "termica e fita kapton no caminho de volta pra casa depois do "
        "trabalho antes que eu esqueca de novo como sempre";
    const char *vis = ui_voz_visivel(longa, larg, linhas);
    ESPERA(vis > longa);

    // O fim nunca é cortado.
    ESPERA_TEXTO(vis + strlen(vis) - 6, "sempre");

    // Começa em palavra.
    ESPERA(vis == longa || *(vis - 1) == ' ');

    char maior[400];
    snprintf(maior, sizeof maior, "%s e mais um punhado de palavras", longa);
    ESPERA(ui_voz_visivel(maior, larg, linhas) - maior > vis - longa);

    // Uma palavra maior que a janela não trava o laço.
    const char *monstro = "supercalifragilisticexpialidocious"
                          "supercalifragilisticexpialidocious";
    ESPERA(ui_voz_visivel(monstro, 40, 1) != NULL);
    TERMINA();
}

// As recusas do servidor: antes viravam "sem contato", que manda esperar.
void t_sem_minutos_recusa_e_leva_a_voz(void)
{
    COMECA("recusa · sem minutos leva à tela de Voz, não ao Wi-Fi");

    liga();
    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", "Convidado");
    pc_nuvem_codigo(402);
    pc_nuvem_responde("{}");

    ESPERA_IGUAL(uso_sincronizar(hal, &ap.estado), OK);
    (void)uso_nuvem_resposta(hal, &ap.estado);

    ESPERA_IGUAL(ap.estado.recusa, RECUSA_MINUTOS);

    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_FALA);
}

void t_conta_caida_manda_reconectar_e_nao_esperar(void)
{
    COMECA("recusa · sem vínculo leva ao novo pareamento");

    liga();
    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", "Convidado");
    pc_nuvem_codigo(409);
    pc_nuvem_responde("{}");

    ESPERA_IGUAL(uso_sincronizar(hal, &ap.estado), OK);
    (void)uso_nuvem_resposta(hal, &ap.estado);

    ESPERA_IGUAL(ap.estado.recusa, RECUSA_SEM_CONTA);

    // Mostrar o e-mail afirmaria um vínculo que o servidor negou.
    ESPERA_TEXTO(ap.estado.nome, "");

    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_CONTA);

    // O servidor respondeu: não é falta de internet.
    ESPERA_IGUAL(ap.estado.sinc, SINC_OCIOSO);
    vista_cartao_t conta;
    vista_conta(&ap.estado, &conta);
    ESPERA_IGUAL(conta.estado, CONTA_SEM_GOOGLE);
    ESPERA_TEXTO(conta.dest[0].titulo, "Conectar conta Google");
}

void t_google_revogado_mantem_conta_e_pede_novo_login(void)
{
    COMECA("recusa · concessão revogada mantém o vínculo visível");

    liga();
    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", "eu@x.com");
    pc_nuvem_codigo(428);
    pc_nuvem_responde("{}");

    ESPERA_IGUAL(uso_sincronizar(hal, &ap.estado), OK);
    (void)uso_nuvem_resposta(hal, &ap.estado);

    ESPERA_IGUAL(ap.estado.recusa, RECUSA_CONTA);
    ESPERA_TEXTO(ap.estado.nome, "eu@x.com");
    ESPERA(ap.estado.conta_reconectar);
    ESPERA_IGUAL(ap.estado.sinc, SINC_OCIOSO);

    vista_cartao_t conta;
    vista_conta(&ap.estado, &conta);
    ESPERA_IGUAL(conta.estado, CONTA_REAUTORIZAR);
    ESPERA_TEXTO(conta.dest[0].titulo, "Aplicativo Tinto");
}

// O lembrete de reconectar não insiste: o 428 vem em toda resposta e a
// faixa voltava por cima de quem foi resolver.
void t_o_aviso_de_reconectar_nao_insiste(void)
{
    COMECA("recusa · o lembrete de reconectar se dá uma vez");

    liga();
    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", "eu@x.com");
    pc_nuvem_codigo(428);
    pc_nuvem_responde("{}");

    ESPERA_IGUAL(uso_sincronizar(hal, &ap.estado), OK);
    (void)uso_nuvem_resposta(hal, &ap.estado);
    ESPERA_IGUAL(ap.estado.recusa, RECUSA_CONTA);

    ap.estado.recusa = RECUSA_NADA;

    // O 428 seguinte não traz a faixa de volta.
    for (int i = 0; i < 5; i++) {
        ap.estado.agora_ms = 5000u * (uint32_t)(i + 1);
        ap.estado.pull_esperando = false;
        ESPERA_IGUAL(uso_sincronizar(hal, &ap.estado), OK);
        pc_nuvem_codigo(428);
        pc_nuvem_responde("{}");
        (void)uso_nuvem_resposta(hal, &ap.estado);
        ESPERA_IGUAL(ap.estado.recusa, RECUSA_NADA);
    }

    // O ESTADO continua; some o aviso, não o problema.
    ESPERA(ap.estado.conta_reconectar);

    // ENTRAR em Minha Conta avisa, uma vez.
    ap.estado.pilha[++ap.estado.profundidade] = TELA_CONTA;
    pc_tick();
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.recusa, RECUSA_CONTA);

    // O gatilho é a ENTRADA, não estar aqui.
    ap.estado.recusa = RECUSA_NADA;
    for (int i = 0; i < 5; i++) {
        pc_tick();
        app_passo(&ap);
        ESPERA_IGUAL(ap.estado.recusa, RECUSA_NADA);
    }

    ap.estado.pull_esperando = false;
    ESPERA_IGUAL(uso_sincronizar(hal, &ap.estado), OK);
    pc_nuvem_codigo(428);
    pc_nuvem_responde("{}");
    (void)uso_nuvem_resposta(hal, &ap.estado);
    ESPERA_IGUAL(ap.estado.recusa, RECUSA_NADA);

    // O aparelho que LIGA com a concessão vencida avisa uma vez, na primeira
    // resposta (`/v1/parear/estado`).
    estado_conta_ok(&ap.estado);          // como se tivesse acabado de ligar
    ap.estado.recusa = RECUSA_NADA;
    ap.estado.nuvem_esperando = NUVEM_PAREADO;
    pc_nuvem_codigo(200);
    pc_nuvem_responde("{\"pareado\":true,\"reconectar\":true,"
                      "\"conta\":\"eu@x.com\"}");
    (void)uso_nuvem_resposta(hal, &ap.estado);

    ESPERA(ap.estado.conta_reconectar);
    ESPERA_IGUAL(ap.estado.recusa, RECUSA_CONTA);

    ap.estado.recusa = RECUSA_NADA;
    ap.estado.nuvem_esperando = NUVEM_PAREADO;
    pc_nuvem_responde("{\"pareado\":true,\"reconectar\":true,"
                      "\"conta\":\"eu@x.com\"}");
    (void)uso_nuvem_resposta(hal, &ap.estado);
    ESPERA_IGUAL(ap.estado.recusa, RECUSA_NADA);

    TERMINA();
}



// O Resultado só entra depois da resposta do servidor.
void t_o_resultado_so_aparece_depois_da_resposta(void)
{
    COMECA("confirmar espera a resposta; só então o Resultado entra");

    liga();
    pc_botao(IN_VOZ);
    app_passo(&ap);
    o_servidor_entendeu();
    pc_botao(IN_OK);              // termina a fala
    app_passo(&ap);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_CONFERIR);

    // O cursor desce até a decisão: as primeiras paradas são os resultados.
    for (int i = 0; i < ap.estado.n_resultados; i++) {
        pc_botao(IN_BAIXO);
        app_passo(&ap);
    }
    pc_botao(IN_OK);
    app_passo(&ap);
    // O harness responde no mesmo passo: prova-se que a tela chegou pelo
    // caminho da resposta.
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_RESULTADO);
    ESPERA(!ap.estado.esperando_resultado);

    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA(ap.estado.pilha[ap.estado.profundidade] != TELA_RESULTADO);
    ESPERA(!ap.estado.veio_da_voz);
    TERMINA();
}

// O envio que NÃO saiu (rede caiu antes do áudio sair) descarta a gravação
// e não promete que ficou: não há resultado a esperar.
void t_envio_que_nao_saiu_descarta_e_diz_a_verdade(void)
{
    COMECA("o envio que não saiu descarta a fala e diz que não enviou");

    liga();
    ap.estado.rede = REDE_LIGADA;

    pc_botao(IN_VOZ);
    app_passo(&ap);
    segundos(10);

    ap.estado.rede = REDE_DESLIGADA;
    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA(!ap.estado.travado);

    vista_grav_t v;
    vista_gravador(&ap.estado, &v);
    ESPERA_TEXTO(v.titulo, "Não enviei");
    ESPERA_CONTEM(v.aviso, "descartada");
    ESPERA_TEXTO(v.rodape_dir, "OK fechar");

    ESPERA_IGUAL(ap.estado.ultimo.id[0], '\0');

    TERMINA();
}

// ESTRUTURANDO: a frase em negrito e três pontos em posição fixa, um aceso
// por vez (reticências mudavam a largura da frase).
void t_estruturando_diz_e_pisca(void)
{
    COMECA("estruturando: mensagem em negrito e os três pontos girando");

    liga();
    ap.estado.rede = REDE_LIGADA;
    pc_nuvem_demora(true);

    pc_botao(IN_VOZ);
    app_passo(&ap);
    segundos(10);
    pc_botao(IN_OK);
    app_passo(&ap);

    vista_grav_t v;
    vista_gravador(&ap.estado, &v);
    ESPERA(v.estruturando);
    ESPERA_TEXTO(v.aviso, "Estruturando");

    int visto[3] = { 0, 0, 0 };
    for (int i = 0; i < 3; i++) {
        vista_gravador(&ap.estado, &v);
        ESPERA(v.pontos >= 0 && v.pontos < 3);
        visto[v.pontos] = 1;
        segundos(1);
    }
    ESPERA(visto[0] && visto[1] && visto[2]);
    TERMINA();
}

// O ● que não grava diz por quê (o erro ia só para o Sobre).
void t_gravar_que_nao_comeca_diz_por_que(void)
{
    COMECA("o ● que não grava diz por que não gravou");

    liga();
    ap.estado.rede = REDE_LIGADA;
    pc_sem_cartao(true);

    pc_botao(IN_VOZ);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_PARADA);

    vista_grav_t v;
    vista_gravador(&ap.estado, &v);
    ESPERA_TEXTO(v.titulo, "Não gravei");
    ESPERA_CONTEM(v.aviso, "cartão");
    ESPERA_TEXTO(v.rodape_dir, "OK fechar");

    pc_sem_cartao(false);
    TERMINA();
}

// O Conferir não tem saída lateral: só decidir. O BACK segurado passa (é o
// pânico).
void t_conferir_so_sai_decidindo(void)
{
    COMECA("do Conferir só se sai descartando ou confirmando");

    liga();
    ap.estado.rede = REDE_LIGADA;

    pc_botao(IN_VOZ);
    app_passo(&ap);
    segundos(10);
    o_servidor_entendeu();
    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_CONFERIR);
    ESPERA(ap.estado.n_resultados > 0);

    const entrada_t FORA[] = { IN_VOLTAR, IN_MENU, IN_VOZ, IN_ESQ, IN_DIR };
    for (size_t i = 0; i < sizeof FORA / sizeof FORA[0]; i++) {
        pc_botao(FORA[i]);
        app_passo(&ap);
        ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_CONFERIR);
        ESPERA_IGUAL(ap.estado.overlay, OVERLAY_NADA);
    }

    pc_segura(IN_VOLTAR, 900);
    app_passo(&ap);
    ESPERA(ap.estado.pilha[ap.estado.profundidade] != TELA_CONFERIR);
    TERMINA();
}

// O id da fala é o do COMEÇO: onze segundos cruzam um minuto, e o envio
// procurava um WAV que não existia.
void t_o_id_da_fala_e_o_do_comeco(void)
{
    COMECA("o id da fala é o da hora em que ela começou, não a de agora");

    liga();
    ap.estado.rede = REDE_LIGADA;

    pc_botao(IN_VOZ);
    app_passo(&ap);

    char do_comeco[40];
    snprintf(do_comeco, sizeof do_comeco, "%s", ap.estado.gravacao.id);
    ESPERA(do_comeco[0] != '\0');

    segundos(50);
    pc_relogio(HOJE, ap.estado.hora, ap.estado.minuto + 1);
    segundos(10);

    o_servidor_entendeu();
    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA_TEXTO(ap.estado.ultimo.id, do_comeco);
    TERMINA();
}

// Nenhuma falha deixa gravação no cartão: reprocessar é falar de novo.
void t_nenhuma_falha_deixa_gravacao_no_cartao(void)
{
    COMECA("toda falha da voz descarta a gravação");

    // ── subiu e não voltou ──
    liga();
    ap.estado.rede = REDE_LIGADA;
    pc_nuvem_demora(true);

    pc_botao(IN_VOZ);
    app_passo(&ap);
    segundos(10);
    pc_botao(IN_OK);
    app_passo(&ap);
    ate_o_prazo_estourar();

    ESPERA(ap.estado.gravacao.sem_resposta);
    ESPERA_IGUAL(ap.estado.ultimo.id[0], '\0');

    vista_grav_t v;
    vista_gravador(&ap.estado, &v);
    ESPERA(strstr(v.aviso, "guardada") == NULL);
    ESPERA_CONTEM(v.aviso, "descartada");

    // ── voltou sem achar comando ──
    liga();
    ap.estado.rede = REDE_LIGADA;
    pc_nuvem_responde("{\"falou\":\"bom dia\",\"acoes\":[]}");

    pc_botao(IN_VOZ);
    app_passo(&ap);
    segundos(10);
    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA(ap.estado.gravacao.nada_entendido);
    ESPERA_IGUAL(ap.estado.ultimo.id[0], '\0');
    TERMINA();
}

// O prazo da TELA vem depois do da CONEXÃO: ao contrário, a tela dizia
// "Sem resposta" (e apagava o áudio) com a requisição viva.
void t_prazo_da_tela_e_maior_que_o_da_conexao(void)
{
    COMECA("a tela só desiste depois de a conexão ter desistido");

    ESPERA(ESTRUTURA_PRAZO_MS > HTTP_PRAZO_MS);

    // Cobrem o pior caso real do backend (fala longa passa de um minuto).
    ESPERA(HTTP_PRAZO_MS >= 90u * 1000u);
    TERMINA();
}

// O botão de pânico vale durante a espera: sem ele, servidor fora do ar
// seria um minuto e meio de tijolo.
void t_o_panico_funciona_durante_a_estruturacao(void)
{
    COMECA("o BACK segurado sai da espera da estruturação");

    liga();
    ap.estado.rede = REDE_LIGADA;
    pc_nuvem_demora(true);

    pc_botao(IN_VOZ);
    app_passo(&ap);
    segundos(10);
    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA(ap.estado.travado);
    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_ESTRUTURANDO);

    pc_segura(IN_VOLTAR, 900);
    app_passo(&ap);

    ESPERA(!ap.estado.travado);
    ESPERA_IGUAL(ap.estado.profundidade, 0);
    TERMINA();
}

// Três ações de uma fala sobem todas (com um lugar só, ação sumia e ação
// repetia).
void t_tres_acoes_de_uma_fala_sobem_todas(void)
{
    COMECA("voz · três ações de uma fala sobem as três, e cada uma uma vez");

    liga();
    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", "Convidado");
    ap.estado.rede = REDE_LIGADA;
    ap.estado.tem_token = true;

    const char *nomes[] = { "Dentista", "Comprar pão", "Ligar pro João" };
    for (int i = 0; i < 3; i++) {
        item_t *it = &ap.estado.resultados[i].item;
        memset(it, 0, sizeof *it);
        snprintf(it->id, sizeof it->id, "n:%d", i);
        snprintf(it->titulo, sizeof it->titulo, "%s", nomes[i]);
        it->tipo = TIPO_TAREFA;
        it->dia  = ap.estado.hoje;
        ap.estado.resultados[i].verbo = RES_CRIOU;
    }
    ap.estado.n_resultados = 3;

    ESPERA_IGUAL(uso_confirmar_resultado(hal, &ap.estado), OK);

    // A prova é o HISTÓRICO: a resposta de uma puxa a próxima no mesmo passo.
    for (int volta = 0; volta < 12; volta++) {
        pc_nuvem_responde("{\"ok\":true}");
        pc_avanca_ms(1000);
        pc_tick();
        app_passo(&ap);
    }

    int vistas = 0;
    for (int i = 0; i < 3; i++)
        if (strstr(pc_nuvem_historico(), nomes[i])) vistas++;

    ESPERA_IGUAL(vistas, 3);
    ESPERA_IGUAL(ap.estado.gesto_n, 0);      // a fila do cartão esvaziou

    TERMINA();
}

// Gravação NÃO entra na fila: a única espera é pela LINHA. Se o Wi-Fi cai
// nessa janela, a fala desiste.
void t_a_fala_nao_espera_a_rede_voltar(void)
{
    COMECA("voz · a fala desiste se a rede cai, e não vira fila");

    liga();
    ap.estado.tem_token = true;

    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,     sizeof it.id,     "%s", "t:1");
    snprintf(it.titulo, sizeof it.titulo, "%s", "Pagar o IPVA");
    it.tipo = TIPO_TAREFA; it.dia = ap.estado.hoje;
    ESPERA_IGUAL(cartao_grava_item(hal, ap.estado.hoje, &it), OK);

    pc_nuvem_demora(true);
    ESPERA_IGUAL(uso_enviar_gesto(hal, &ap.estado, &it, "editou"), OK);
    ESPERA_IGUAL(uso_enviar_captura(hal, &ap.estado,
                                    "/TINTO/itens/2026-09-03/1422/a.wav"), OK);
    ESPERA_CONTEM(ap.estado.captura_pendente, "a.wav");

    ap.estado.rede = REDE_DESLIGADA;
    pc_avanca_ms(1000);
    pc_tick();
    app_passo(&ap);

    ESPERA(!ap.estado.captura_pendente[0]);
    ESPERA(ap.estado.gravacao.nao_enviou);

    // O GESTO continua na fila: ele é de mão.
    ESPERA_IGUAL(ap.estado.gesto_n, 1);
    TERMINA();
}

// "Concluído" só quando a fila esvazia: com três ações a linha vaga entre
// uma e outra.
void t_o_resultado_so_entra_quando_a_fila_esvazia(void)
{
    COMECA("voz · 'pronto' só depois que a última ação subiu");

    liga();
    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", "Convidado");
    ap.estado.rede = REDE_LIGADA;
    ap.estado.tem_token = true;

    for (int i = 0; i < 2; i++) {
        item_t *it = &ap.estado.resultados[i].item;
        memset(it, 0, sizeof *it);
        snprintf(it->id, sizeof it->id, "n:%d", i);
        snprintf(it->titulo, sizeof it->titulo, "acao %d", i);
        it->tipo = TIPO_TAREFA;
        it->dia  = ap.estado.hoje;
        ap.estado.resultados[i].verbo = RES_CRIOU;
    }
    ap.estado.n_resultados = 2;
    ap.estado.profundidade = 1;
    ap.estado.pilha[1] = TELA_CONFERIR;

    // A segunda fica pendurada: é o único jeito de haver um instante com uma
    // subida e outra não.
    ESPERA_IGUAL(uso_confirmar_resultado(hal, &ap.estado), OK);

    pc_nuvem_demora(true);
    pc_nuvem_responde("{\"ok\":true}");
    app_passo(&ap);

    ESPERA(ap.estado.esperando_resultado);
    ESPERA(ap.estado.pilha[ap.estado.profundidade] != TELA_RESULTADO);
    pc_nuvem_demora(false);

    TERMINA();
}

// Sem rede não se GRAVA: áudio que não pode ser transcrito não é nota.
void t_sem_rede_nao_se_grava(void)
{
    COMECA("voz · sem rede o botão de falar recusa, e explica");

    liga();
    ap.estado.rede = REDE_DESLIGADA;

    ESPERA(uso_gravar_comeca(hal, &ap.estado) != OK);
    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_PARADA);

    // E a tela diz o que fazer.
    ESPERA(ap.estado.precisa_rede[0]);

    ap.estado.rede = REDE_LIGADA;
    ESPERA_IGUAL(uso_gravar_comeca(hal, &ap.estado), OK);

    TERMINA();
}

// A rede que cai no meio da fala descarta o áudio na hora.
void t_rede_que_cai_no_meio_da_fala_descarta_o_audio(void)
{
    COMECA("voz · a rede caiu falando: o áudio vai embora, e a tela diz");

    liga();
    ap.estado.rede = REDE_LIGADA;
    ESPERA_IGUAL(uso_gravar_comeca(hal, &ap.estado), OK);
    segundos(3);

    ESPERA(ap.estado.gravacao.fase != GRAV_PARADA);

    // O teste mexe no estado que o evento do rádio mexeria.
    ap.estado.rede = REDE_DESLIGADA;
    pc_avanca_ms(1000);
    pc_tick();
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_PARADA);
    ESPERA(ap.estado.precisa_rede[0]);

    TERMINA();
}

// E a rede que cai com o áudio SUBINDO também descarta na hora.
void t_rede_que_cai_estruturando_descarta_na_hora(void)
{
    COMECA("voz · a rede caiu enviando: descarta na hora, sem esperar");

    liga();
    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", "Convidado");
    ap.estado.rede = REDE_LIGADA;
    ESPERA_IGUAL(uso_gravar_comeca(hal, &ap.estado), OK);
    segundos(3);
    pc_botao(IN_OK);                       // sobe
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_ESTRUTURANDO);

    ap.estado.rede = REDE_DESLIGADA;
    pc_avanca_ms(1000);
    pc_tick();
    app_passo(&ap);

    ESPERA(ap.estado.gravacao.fase != GRAV_ESTRUTURANDO);
    ESPERA(!ap.estado.travado);

    TERMINA();
}

// E a rede que cai com o Conferir na tela descarta a proposta: o OK não
// teria para onde ir.
void t_rede_que_cai_no_conferir_descarta_a_proposta(void)
{
    COMECA("voz · a rede caiu decidindo: a proposta vai junto");

    liga();
    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", "Convidado");
    ap.estado.rede = REDE_LIGADA;

    resultado_t r;
    memset(&r, 0, sizeof r);
    snprintf(r.item.id, sizeof r.item.id, "%s", "n:1");
    snprintf(r.item.titulo, sizeof r.item.titulo, "%s", "Dentista");
    r.item.tipo = TIPO_TAREFA;
    r.item.dia  = ap.estado.hoje;
    r.verbo = RES_CRIOU;
    ESPERA_IGUAL(uso_propor_resultado(&ap.estado, &r, 1, 0), OK);

    ap.estado.profundidade = 1;
    ap.estado.pilha[1] = TELA_CONFERIR;

    ap.estado.rede = REDE_DESLIGADA;
    pc_avanca_ms(1000);
    pc_tick();
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.n_resultados, 0);
    ESPERA(ap.estado.pilha[ap.estado.profundidade] != TELA_CONFERIR);
    ESPERA(ap.estado.precisa_rede[0]);

    TERMINA();
}
