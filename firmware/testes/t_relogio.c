// firmware/testes/t_relogio.c — Ajustes → Data e hora.
// RN-6G: aqui o contador vira hora, à mão ou pela rede, nunca os dois.
#include "teste.h"
#include "vista/relogio.h"
#include "vista/campos.h"
#include "nucleo/data.h"
#include "uso/inicializacao.h"
#include "uso/uso.h"

static estado_t e;
static app_t    ap;

static void base(void)
{
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 8, 21};
    e.hora = 10; e.minuto = 2;
    e.hora_confiavel = true;
    e.inicio.ano = 2026; e.inicio.mes = 8; e.inicio.dia = 21;
    e.inicio.hora = 10;  e.inicio.minuto = 2;
}

void t_a_hora_pela_internet_desliga_o_ajuste_a_mao(void)
{
    COMECA("Data e hora · com a rede mandando, o ajuste à mão sai de cena");

    base();
    e.config.valor[AJUSTE_HORA_REDE] = 1;

    vista_relogio_t v;
    vista_relogio(&e, &v);

    ESPERA(v.pela_rede);
    ESPERA(!v.editavel);
    ESPERA_CONTEM(v.nota, "rede");

    TERMINA();
}

void t_desligada_a_rede_os_campos_ficam_editaveis(void)
{
    COMECA("Data e hora · sem a rede, os cinco campos são seus");

    base();
    e.config.valor[AJUSTE_HORA_REDE] = 0;

    // Três linhas: interruptor (0), fuso (1), campos (2); o campo ativo é
    // `inicio.campo`.
    e.cursor = 2;
    e.inicio.campo = 3;   // a hora é o quarto campo

    vista_relogio_t v;
    vista_relogio(&e, &v);

    ESPERA(!v.pela_rede);
    ESPERA(!v.no_interruptor);
    ESPERA(v.editavel);
    ESPERA_IGUAL(v.campo, 3);
    ESPERA_TEXTO(v.valor[0], "21");
    ESPERA_TEXTO(v.valor[2], "2026");
    ESPERA_TEXTO(v.valor[3], "10");

    TERMINA();
}

// O OK salva, e a hora do aparelho passa a ser a ajustada.
void t_ajustar_a_hora_a_mao_muda_o_relogio_do_aparelho(void)
{
    COMECA("Data e hora · o OK salva, e o aparelho passa a saber a hora");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 21}, 10, 2);
    app_liga(&ap, hal);
    app_passo(&ap);

    ap.estado.pilha[++ap.estado.profundidade] = TELA_DATA_HORA;
    ap.estado.inicio.ano = 2026; ap.estado.inicio.mes = 8;
    ap.estado.inicio.dia = 22;
    ap.estado.inicio.hora = 7;   ap.estado.inicio.minuto = 30;
    // A hora pela rede vem ligada de fábrica; ajustar à mão exige desligar.
    ap.estado.config.valor[AJUSTE_HORA_REDE] = 0;
    ap.estado.cursor = 2;        // o primeiro campo, o dia

    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.hoje.dia, 22);
    ESPERA_IGUAL(ap.estado.hora, 7);
    ESPERA_IGUAL(ap.estado.minuto, 30);
    ESPERA(ap.estado.hora_confiavel);

    TERMINA();
}


// ── o interruptor liga E desliga ────────────────────────────────────
void t_o_interruptor_da_hora_pela_rede_liga_e_desliga(void)
{
    COMECA("Data e hora · o interruptor alterna nos dois sentidos");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 21}, 10, 2);
    app_liga(&ap, hal);
    app_passo(&ap);

    ap.estado.pilha[++ap.estado.profundidade] = TELA_DATA_HORA;
    ap.estado.cursor = 0;                       // o interruptor
    ap.estado.config.valor[AJUSTE_HORA_REDE] = 1;

    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.config.valor[AJUSTE_HORA_REDE], 0);

    pc_botao(IN_OK);                            // e volta
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.config.valor[AJUSTE_HORA_REDE], 1);

    // Ligado, os campos não se editam e o cursor fica no interruptor.
    vista_relogio_t v;
    vista_relogio(&ap.estado, &v);
    ESPERA(v.pela_rede);
    ESPERA(v.no_interruptor);
    ESPERA(!v.editavel);
    TERMINA();
}

// Ligar com rede pergunta a hora AGORA.
void t_ligar_a_hora_pela_rede_pergunta_na_hora(void)
{
    COMECA("Data e hora · ligar com rede na mão consulta o NTP na hora");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 21}, 10, 2);
    app_liga(&ap, hal);
    app_passo(&ap);

    ap.estado.pilha[++ap.estado.profundidade] = TELA_DATA_HORA;
    ap.estado.cursor = 0;
    ap.estado.config.valor[AJUSTE_HORA_REDE] = 0;
    ap.estado.rede = REDE_LIGADA;

    int antes = pc_ntp_pedidos();
    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.config.valor[AJUSTE_HORA_REDE], 1);
    ESPERA_IGUAL(pc_ntp_pedidos(), antes + 1);
    TERMINA();
}

// ── o fuso, ajustável até a conta existir ───────────────────────────
// Vale mesmo com o NTP ligado: o NTP dá o instante, o fuso diz que horas é
// aqui.
void t_o_fuso_se_ajusta_e_vale_com_o_ntp_ligado(void)
{
    COMECA("Data e hora · o fuso se ajusta de meia em meia hora");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 25}, 10, 2);
    app_liga(&ap, hal);
    app_passo(&ap);

    ap.estado.pilha[++ap.estado.profundidade] = TELA_DATA_HORA;
    ap.estado.config.valor[AJUSTE_HORA_REDE] = 1;   // a rede manda
    ap.estado.config.valor[AJUSTE_FUSO_MIN]  = 0;   // e o aparelho está em UTC
    ap.estado.cursor = 1;                           // o fuso

    // ◀▶ mudam o valor; ▲▼ andam entre as paradas.
    for (int i = 0; i < 6; i++) pc_botao(IN_ESQ);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.config.valor[AJUSTE_FUSO_MIN], -180);

    // Aplicado no hal na hora.
    ESPERA_IGUAL(pc_fuso(), -180);

    vista_relogio_t v;
    vista_relogio(&ap.estado, &v);
    ESPERA(v.no_fuso);
    ESPERA_TEXTO(v.fuso, "-03:00");

    // Com a rede mandando, o cursor não desce aos campos.
    for (int i = 0; i < 5; i++) pc_botao(IN_BAIXO);
    app_passo(&ap);
    vista_relogio(&ap.estado, &v);
    ESPERA(!v.editavel);
    ESPERA(v.no_fuso);          // parou no fuso, que é a última parada
    TERMINA();
}

// ── um dono por número ──────────────────────────────────────────────
// Com conta, o fuso vem do Google: mexer à mão seria sobrescrito no pull.
void t_com_ntp_e_conta_nada_no_relogio_se_ajusta_a_mao(void)
{
    COMECA("relógio · com rede e conta, o aparelho é o dono dos dois");

    static app_t ap;
    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 27}, 10, 2);
    app_liga(&ap, hal);
    app_passo(&ap);

    ap.estado.pilha[0] = TELA_DATA_HORA;
    ap.estado.profundidade = 0;
    ap.estado.cursor = 0;

    // ── à mão: sete paradas ──
    ap.estado.config.valor[AJUSTE_HORA_REDE] = 0;
    ap.estado.nome[0] = '\0';

    vista_relogio_t v;
    vista_relogio(&ap.estado, &v);
    ESPERA(!v.pela_rede);
    ESPERA(!v.fuso_do_google);

    // ── NTP ligado, sem conta: o fuso continua editável ──
    ap.estado.config.valor[AJUSTE_HORA_REDE] = 1;
    ap.estado.cursor = 1;
    vista_relogio(&ap.estado, &v);
    ESPERA(v.pela_rede);
    ESPERA(!v.fuso_do_google);
    ESPERA(v.no_fuso);

    int antes = ap.estado.config.valor[AJUSTE_FUSO_MIN];
    pc_botao(IN_DIR);
    app_passo(&ap);
    ESPERA(ap.estado.config.valor[AJUSTE_FUSO_MIN] != antes);

    // ── com CONTA: o cursor nem chega no fuso ──
    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", "eu@x.com");
    ap.estado.cursor = 0;

    vista_relogio(&ap.estado, &v);
    ESPERA(v.fuso_do_google);
    ESPERA(v.no_interruptor);
    ESPERA(!v.editavel);
    ESPERA_CONTEM(v.nota, "seu calendário");

    pc_botao(IN_BAIXO);
    app_passo(&ap);
    vista_relogio(&ap.estado, &v);
    ESPERA(v.no_interruptor);
    ESPERA(!v.no_fuso);

    antes = ap.estado.config.valor[AJUSTE_FUSO_MIN];
    ap.estado.cursor = 1;
    pc_botao(IN_DIR);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.config.valor[AJUSTE_FUSO_MIN], antes);

    TERMINA();
}

// ── a hora se corrige sozinha com rede ──────────────────────────────
// O NTP era pedido só ao conectar, e falhava com o TLS subindo.
void t_a_hora_se_corrige_sozinha_com_rede(void)
{
    COMECA("relógio · com rede, a hora se acerta sozinha se não confia");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 9, 3}, 11, 17);
    static app_t ap2;
    app_liga(&ap2, hal);
    app_passo(&ap2);

    ap2.estado.rede = REDE_LIGADA;
    ap2.estado.config.valor[AJUSTE_HORA_REDE] = 1;
    ap2.estado.hora_confiavel = false;
    int base = pc_ntp_pedidos();

    pc_avanca_ms(31u * 1000u);
    pc_tick();
    app_passo(&ap2);

    ESPERA(pc_ntp_pedidos() > base);

    // Com hora confiável, não pede a cada tique.
    ap2.estado.hora_confiavel = true;
    int antes = pc_ntp_pedidos();
    pc_avanca_ms(31u * 1000u);
    pc_tick();
    app_passo(&ap2);
    ESPERA_IGUAL(pc_ntp_pedidos(), antes);

    TERMINA();
}

void t_eventos_wifi_seguidos_nao_reiniciam_o_ntp(void)
{
    COMECA("relógio · eventos Wi-Fi seguidos pedem NTP uma vez");
    const hal_t *hal = pc_liga();
    static app_t ap3;
    app_liga(&ap3, hal);
    app_passo(&ap3);
    ap3.estado.config.valor[AJUSTE_HORA_REDE] = 1;
    ap3.estado.hora_confiavel = false;
    evento_t ev = { .tipo = EV_WIFI_ESTADO };
    int antes = pc_ntp_pedidos();
    app_evento(&ap3, &ev);
    app_evento(&ap3, &ev);
    ESPERA_IGUAL(pc_ntp_pedidos(), antes + 1);
    TERMINA();
}

// ── 12 h e 24 h ─────────────────────────────────────────────────────
// Um formatador só, em `nucleo/`, lido por toda tela.
void t_a_hora_de_12_horas_diz_am_e_pm(void)
{
    COMECA("12 h · meia-noite é 12 am, meio-dia é 12 pm");

    char s[9];

    hora_texto(0,  0,  false, s, sizeof s);  ESPERA_TEXTO(s, "12:00 am");
    hora_texto(9,  14, false, s, sizeof s);  ESPERA_TEXTO(s, "9:14 am");
    hora_texto(12, 0,  false, s, sizeof s);  ESPERA_TEXTO(s, "12:00 pm");
    hora_texto(13, 5,  false, s, sizeof s);  ESPERA_TEXTO(s, "1:05 pm");
    hora_texto(23, 59, false, s, sizeof s);  ESPERA_TEXTO(s, "11:59 pm");

    // Em 24 h, com o zero à esquerda que alinha a coluna.
    hora_texto(0,  0,  true, s, sizeof s);   ESPERA_TEXTO(s, "00:00");
    hora_texto(9,  14, true, s, sizeof s);   ESPERA_TEXTO(s, "09:14");
    hora_texto(23, 59, true, s, sizeof s);   ESPERA_TEXTO(s, "23:59");

    TERMINA();
}

// A hora do ITEM chega como "14:00" e é guardada assim; quem converte é a
// tela.
void t_a_hora_do_item_tambem_obedece_ao_ajuste(void)
{
    COMECA("12 h · a hora do item vira 2:00 pm, e o cartão não muda");

    char s[9];

    hora_texto_hhmm("14:00", false, s, sizeof s);  ESPERA_TEXTO(s, "2:00 pm");
    hora_texto_hhmm("00:30", false, s, sizeof s);  ESPERA_TEXTO(s, "12:30 am");
    hora_texto_hhmm("14:00", true,  s, sizeof s);  ESPERA_TEXTO(s, "14:00");

    // Vazio continua vazio.
    hora_texto_hhmm("", false, s, sizeof s);       ESPERA_TEXTO(s, "");

    // "dia" é rótulo, não hora.
    hora_texto_hhmm("dia", false, s, sizeof s);    ESPERA_TEXTO(s, "dia");

    TERMINA();
}

// A barra segue o ajuste; sem hora confiável, vazia (RN-6G).
void t_a_barra_segue_o_ajuste_de_hora(void)
{
    COMECA("a barra do sistema mostra a hora no formato escolhido");

    base();
    e.hora = 14; e.minuto = 5;

    char s[9];

    e.config.valor[AJUSTE_HORA24] = 1;
    vista_hora_da_barra(&e, s, sizeof s);
    ESPERA_TEXTO(s, "14:05");

    e.config.valor[AJUSTE_HORA24] = 0;
    vista_hora_da_barra(&e, s, sizeof s);
    ESPERA_TEXTO(s, "2:05 pm");

    e.hora_confiavel = false;
    vista_hora_da_barra(&e, s, sizeof s);
    ESPERA_TEXTO(s, "");

    TERMINA();
}

// ── o campo editável segue o formato ───────────────────────────────
// Anda de 0 a 23 e o meridiano vira sozinho: sem seletor am/pm a mais.
void t_o_campo_de_hora_segue_o_formato_escolhido(void)
{
    COMECA("Data e hora · o campo mostra 2 pm quando o ajuste é 12 h");

    base();

    char s[7];

    e.config.valor[AJUSTE_HORA24] = 1;
    vista_hora_do_campo(&e, 14, s, sizeof s);   ESPERA_TEXTO(s, "14");
    vista_hora_do_campo(&e, 0,  s, sizeof s);   ESPERA_TEXTO(s, "00");

    e.config.valor[AJUSTE_HORA24] = 0;
    vista_hora_do_campo(&e, 14, s, sizeof s);   ESPERA_TEXTO(s, "2 pm");
    vista_hora_do_campo(&e, 0,  s, sizeof s);   ESPERA_TEXTO(s, "12 am");
    vista_hora_do_campo(&e, 12, s, sizeof s);   ESPERA_TEXTO(s, "12 pm");
    vista_hora_do_campo(&e, 9,  s, sizeof s);   ESPERA_TEXTO(s, "9 am");

    TERMINA();
}

// ── ◀▶ escolhe o campo, ▲▼ muda o valor, partindo da hora atual ─────
void t_o_ajuste_a_mao_parte_da_hora_atual_e_usa_os_dois_eixos(void)
{
    COMECA("Data e hora · ◀▶ campo, ▲▼ valor, partindo da hora atual");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 21}, 10, 2);
    app_liga(&ap, hal);
    app_passo(&ap);

    ap.estado.pilha[++ap.estado.profundidade] = TELA_DATA_HORA;
    ap.estado.config.valor[AJUSTE_HORA_REDE] = 1;
    ap.estado.cursor = 0;

    pc_botao(IN_OK);                 // desliga a rede: passa a ser à mão
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.config.valor[AJUSTE_HORA_REDE], 0);
    ESPERA_IGUAL(ap.estado.inicio.dia, 21);
    ESPERA_IGUAL(ap.estado.inicio.mes, 8);
    ESPERA_IGUAL(ap.estado.inicio.ano, 2026);
    ESPERA_IGUAL(ap.estado.inicio.hora, 10);
    ESPERA_IGUAL(ap.estado.inicio.minuto, 2);

    pc_botao(IN_BAIXO); app_passo(&ap);    // fuso
    pc_botao(IN_BAIXO); app_passo(&ap);    // campos, no dia
    ESPERA_IGUAL(ap.estado.cursor, 2);
    ESPERA_IGUAL(ap.estado.inicio.campo, 0);

    pc_botao(IN_CIMA); app_passo(&ap);     // ▲ muda o valor, não sobe
    ESPERA_IGUAL(ap.estado.cursor, 2);
    ESPERA_IGUAL(ap.estado.inicio.dia, 22);

    pc_botao(IN_DIR); app_passo(&ap);      // ▶ mês
    pc_botao(IN_DIR); app_passo(&ap);      // ▶ ano
    ESPERA_IGUAL(ap.estado.inicio.campo, 2);
    pc_botao(IN_BAIXO); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.ano, 2025);
    ESPERA_IGUAL(ap.estado.inicio.mes, 8);   // o resto não mexeu

    // ◀ até o dia, e mais um sobe para o fuso.
    for (int i = 0; i < 3; i++) { pc_botao(IN_ESQ); app_passo(&ap); }
    ESPERA_IGUAL(ap.estado.cursor, 1);

    TERMINA();
}

void t_o_dia_respeita_o_tamanho_do_mes(void)
{
    COMECA("Data e hora · o dia nunca passa do fim do mês");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 1, 31}, 9, 0);
    app_liga(&ap, hal);
    app_passo(&ap);

    ap.estado.pilha[++ap.estado.profundidade] = TELA_DATA_HORA;
    ap.estado.config.valor[AJUSTE_HORA_REDE] = 0;
    uso_campos_do_relogio(&ap.estado);
    ap.estado.cursor = 2;

    pc_botao(IN_DIR); app_passo(&ap);      // mês
    pc_botao(IN_CIMA); app_passo(&ap);     // fevereiro
    ESPERA_IGUAL(ap.estado.inicio.mes, 2);
    ESPERA_IGUAL(ap.estado.inicio.dia, 28);

    pc_botao(IN_ESQ); app_passo(&ap);      // dia
    pc_botao(IN_CIMA); app_passo(&ap);     // 28 → 1, e não 29
    ESPERA_IGUAL(ap.estado.inicio.dia, 1);

    TERMINA();
}

// Sem hora ainda, os campos começam numa data sensata.
void t_sem_hora_valida_os_campos_comecam_numa_data_sensata(void)
{
    COMECA("Data e hora · sem hora válida, os campos começam em 01/01/2026");

    base();
    e.hoje = (data_t){1970, 1, 1};
    uso_campos_do_relogio(&e);
    ESPERA_IGUAL(e.inicio.ano, 2026);
    ESPERA_IGUAL(e.inicio.mes, 1);
    ESPERA_IGUAL(e.inicio.dia, 1);
    ESPERA_IGUAL(e.inicio.hora, 12);

    TERMINA();
}

void t_no_primeiro_uso_o_direcional_escolhe_o_campo(void)
{
    COMECA("Primeiro uso · ◀▶ escolhe o campo da data, ▲▼ muda o valor");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 21}, 10, 2);
    app_liga(&ap, hal);
    app_passo(&ap);

    ap.estado.inicio.fase = INICIO_DATA_HORA;
    uso_campos_do_relogio(&ap.estado);

    pc_botao(IN_DIR); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.campo, 1);
    pc_botao(IN_CIMA); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.mes, 9);
    ESPERA_IGUAL(ap.estado.inicio.dia, 21);

    for (int i = 0; i < 6; i++) { pc_botao(IN_DIR); app_passo(&ap); }
    ESPERA_IGUAL(ap.estado.inicio.campo, 4);   // para no minuto
    pc_botao(IN_BAIXO); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.minuto, 1);

    TERMINA();
}

// O OK no fuso não grava o relógio.
void t_ok_no_fuso_nao_grava_o_relogio(void)
{
    COMECA("Data e hora · OK na linha do fuso não mexe no relógio");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 21}, 10, 2);
    app_liga(&ap, hal);
    app_passo(&ap);

    ap.estado.pilha[++ap.estado.profundidade] = TELA_DATA_HORA;
    ap.estado.config.valor[AJUSTE_HORA_REDE] = 0;
    ap.estado.inicio.ano = 2020; ap.estado.inicio.mes = 1;   // campos velhos
    ap.estado.inicio.dia = 1;
    ap.estado.cursor = 1;                                     // o fuso

    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.hoje.ano, 2026);
    ESPERA_IGUAL(ap.estado.hoje.dia, 21);
    TERMINA();
}

// ── o bloqueio por tempo ────────────────────────────────────────────
static void liga_na_home(void)
{
    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 21}, 10, 2);
    app_liga(&ap, hal);
    app_passo(&ap);
    ap.estado.config.valor[AJUSTE_BLOQUEAR_MIN] = 3;
}

static void passa_minutos(int n)
{
    for (int i = 0; i < n * 60; i++) {
        pc_avanca_ms(1000);
        pc_tick();
        app_passo(&ap);
    }
}

void t_sem_tocar_em_nada_a_tela_trava_sozinha(void)
{
    COMECA("bloqueio · sem tocar em nada, a tela trava depois do prazo");
    liga_na_home();

    passa_minutos(2);
    ESPERA(ap.estado.pilha[ap.estado.profundidade] != TELA_BLOQUEADA);

    // Um toque reinicia a contagem.
    pc_botao(IN_BAIXO);
    app_passo(&ap);
    passa_minutos(2);
    ESPERA(ap.estado.pilha[ap.estado.profundidade] != TELA_BLOQUEADA);

    passa_minutos(2);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_BLOQUEADA);

    // O power destrava, de volta para onde estava.
    pc_botao(IN_POWER);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[0], TELA_HOME);
    TERMINA();
}

void t_gravando_a_tela_nao_trava_por_tempo(void)
{
    COMECA("bloqueio · com uma fala aberta, o tempo não trava a tela");
    liga_na_home();

    ap.estado.rede = REDE_LIGADA;            // falar exige rede
    ap.estado.gravacao.fase = GRAV_PAUSADA;
    passa_minutos(5);
    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_PAUSADA);
    ESPERA(ap.estado.pilha[ap.estado.profundidade] != TELA_BLOQUEADA);
    TERMINA();
}

void t_no_primeiro_uso_a_tela_nao_trava_por_tempo(void)
{
    COMECA("bloqueio · o primeiro uso não trava por tempo");
    liga_na_home();

    ap.estado.inicio.fase = INICIO_BOAS_VINDAS;
    passa_minutos(5);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_BOAS_VINDAS);
    ESPERA(ap.estado.pilha[ap.estado.profundidade] != TELA_BLOQUEADA);
    TERMINA();
}

// "nunca" desliga o bloqueio automático.
void t_bloqueio_automatico_pode_ser_desligado(void)
{
    COMECA("bloqueio · em \"nunca\", a tela não trava sozinha");
    liga_na_home();
    ESPERA_IGUAL(uso_salvar_ajuste(ap.hal, &ap.estado,
                                   AJUSTE_BLOQUEAR_MIN, 0), OK);
    ESPERA_IGUAL(ap.estado.config.valor[AJUSTE_BLOQUEAR_MIN], 0);
    passa_minutos(60);
    ESPERA(ap.estado.pilha[ap.estado.profundidade] != TELA_BLOQUEADA);
    TERMINA();
}
