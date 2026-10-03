// firmware/testes/t_conta.c — Minha Conta e Sincronização (T-32).
// O aparelho funciona offline, e nenhuma das telas finge saber o que só o
// backend sabe.
#include "teste.h"
#include "ui/cartao.h"
#include "ui/voz.h"
#include "vista/inicializacao.h"
#include "vista/conta.h"
#include "vista/vincular.h"
#include "vista/confirma.h"

static estado_t e;

// O card tem FATOS (informam) e DESTINOS (se apertam).
static bool tem_dest(const vista_cartao_t *v, const char *t)
{
    for (int i = 0; i < v->n_dest; i++)
        if (strstr(v->dest[i].titulo, t)) return true;
    return false;
}

static const char *fato_de(const vista_cartao_t *v, const char *rotulo)
{
    for (int i = 0; i < v->n_fatos; i++)
        if (strstr(v->fatos[i].rotulo, rotulo)) return v->fatos[i].valor;
    return "";
}

static void base(void)
{
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 8, 21};
    e.hora = 10; e.minuto = 2;
    e.hora_confiavel = true;
    e.tem_token = true;      // registrado: é o estado normal
}

// Sem rede, nada do servidor aparece (seria velho com cara de certo).
void t_sem_rede_a_conexao_manda_ligar_o_wifi(void)
{
    COMECA("Minha Conta · sem rede, só o dono e o caminho da rede");

    base();
    e.rede = REDE_DESLIGADA;
    snprintf(e.inicio.nome_pendente, sizeof e.inicio.nome_pendente,
             "%s", "Usuário");
    snprintf(e.nome, sizeof e.nome, "%s", "eu@x.com");   // conta de antes

    vista_cartao_t v;
    vista_conta(&e, &v);

    ESPERA(tem_dest(&v, "Conexão"));
    ESPERA_IGUAL(vista_conta_acao(&e), CONTA_WIFI);

    ESPERA_TEXTO(fato_de(&v, "Google"), "");
    ESPERA(!tem_dest(&v, "Uso de voz"));

    // O DONO fica: mora no cartão.
    ESPERA_TEXTO(v.nome, "Usuário");

    TERMINA();
}

// Com rede e sem conta: uma linha, conectar. O QR mora atrás dela.
void t_com_rede_e_sem_conta_a_tela_espera_o_codigo(void)
{
    COMECA("Minha Conta · com rede e sem conta, conectar é uma linha");

    base();
    e.rede = REDE_LIGADA;

    vista_cartao_t v;
    vista_conta(&e, &v);

    ESPERA(tem_dest(&v, "Google"));
    ESPERA_IGUAL(vista_conta_acao(&e), CONTA_VINCULAR);

    // Voz não aparece sem conta: "0 de 30" prometeria um limite inexistente.
    ESPERA(!tem_dest(&v, "Uso de voz"));

    TERMINA();
}

// Conectada: conta, voz e dono, e o código de vincular some.
void t_pareado_a_tela_diz_a_conta_e_a_fila(void)
{
    COMECA("Minha Conta · conectada, o código some e a voz aparece");

    base();
    e.rede = REDE_LIGADA;
    snprintf(e.nome, sizeof e.nome, "%s", "eu@x.com");
    snprintf(e.meu_id, sizeof e.meu_id, "%s", "a1:b2:c3:d4:e5:f6");
    e.quota.usados_s = 8 * 60;
    e.quota.limite_s = 30 * 60;

    vista_cartao_t v;
    vista_conta(&e, &v);

    ESPERA_TEXTO(fato_de(&v, "Google"), "eu@x.com");

    // O consumo é o VALOR do destino, em minutos usados.
    ESPERA(tem_dest(&v, "Uso de voz"));

    ESPERA(!tem_dest(&v, "Código"));
    ESPERA(v.tem_sair);

    TERMINA();
}

void t_conta_conectada_da_acesso_ao_aplicativo(void)
{
    COMECA("Minha Conta · o aplicativo continua acessível depois de conectar");

    base();
    e.rede = REDE_LIGADA;
    snprintf(e.nome, sizeof e.nome, "%s", "eu@x.com");
    snprintf(e.meu_id, sizeof e.meu_id, "%s", "a1:b2:c3:d4:e5:f6");

    vista_cartao_t v;
    vista_conta(&e, &v);

    ESPERA(tem_dest(&v, "Aplicativo Tinto"));
    bool icone_tinto = false;
    for (int i = 0; i < v.n_dest; i++)
        if (strstr(v.dest[i].titulo, "Aplicativo Tinto"))
            icone_tinto = v.dest[i].ico == ICO_TINTO;
    ESPERA(icone_tinto);

    vista_vincular_t app;
    vista_vincular(&e, &app);
    ESPERA_TEXTO(app.titulo, "Aplicativo");
    ESPERA_TEXTO(app.id, e.meu_id);
    ESPERA(!app.pode_gerar);
    ESPERA(!app.esperando);

    TERMINA();
}


// T-32 sem conta explica, em vez de lista vazia.
void t_sincronizacao_sem_conta_explica_em_vez_de_ficar_vazia(void)
{
    COMECA("T-32 · sem conta, a sincronização explica em vez de esvaziar");

    base();

    vista_cartao_t v;
    vista_sincronizacao(&e, &v);

    ESPERA_TEXTO(v.titulo, "Sincronização");
    // O card diz o que falta; o corpo, onde resolver.
    ESPERA_CONTEM(v.nome, "Sem conta");
    ESPERA_CONTEM(v.corpo, "Minha conta");

    TERMINA();
}

// Padrão de fábrica: só a agenda principal ligada, com a contagem que
// explica as outras desligadas.
void t_sincronizacao_lista_agendas_e_o_que_esta_ligado(void)
{
    COMECA("T-32 · as agendas, e por que a de aniversários vem desligada");

    base();
    // Com rede: escolher agenda é um gesto que sobe.
    e.rede = REDE_LIGADA;
    snprintf(e.nome, sizeof e.nome, "%s", "Convidado");
    snprintf(e.agendas[0].nome, sizeof e.agendas[0].nome, "%s", "principal");
    e.agendas[0].ligada = true;
    snprintf(e.agendas[1].nome, sizeof e.agendas[1].nome, "%s", "Aniversários");
    e.agendas[1].por_ano = 213;
    e.n_agendas = 2;

    vista_cartao_t v;
    vista_sincronizacao(&e, &v);

    // A marcação é a caixa.
    ESPERA_IGUAL(v.dest[0].ico, ICO_CAIXA_ON);
    ESPERA_TEXTO(v.dest[0].valor, "");
    ESPERA_IGUAL(v.dest[1].ico, ICO_CAIXA);
    ESPERA_TEXTO(v.dest[1].valor, "");

    // O número explica por que vem desligada, na legenda.
    ESPERA_CONTEM(v.dest[1].sub, "213");

    TERMINA();
}

// ── vincular ────────────────────────────────────────────────────────
// O código VEM do servidor.
void t_vincular_espera_o_codigo_em_vez_de_inventar(void)
{
    COMECA("Vincular · a pessoa pede o código, e ele não nasce sozinho");

    base();
    e.rede = REDE_LIGADA;
    e.agora_ms = 10000;

    vista_vincular_t v;
    vista_vincular(&e, &v);

    // Sem código nem pedido: a tela OFERECE gerar (pedir ao abrir gastava um
    // código de uso único a cada olhada).
    ESPERA(v.pode_gerar);
    ESPERA(!v.esperando);
    ESPERA_TEXTO(v.codigo, "");
    ESPERA_CONTEM(v.rodape_dir, "gerar");

    // Pedido em voo: ela espera, e diz.
    e.nuvem_esperando = 1;
    vista_vincular(&e, &v);
    ESPERA(v.esperando);
    ESPERA(!v.pode_gerar);

    // O código chegou, com prazo do servidor.
    e.nuvem_esperando = 0;
    snprintf(e.codigo, sizeof e.codigo, "%s", "KXMPQR");
    e.codigo_ate_ms = e.agora_ms + 300000u;      // cinco minutos

    vista_vincular(&e, &v);
    ESPERA(!v.esperando);
    ESPERA(!v.pode_gerar);
    ESPERA_TEXTO(v.codigo, "KXMPQR");
    // Em MINUTOS: segundos seriam trezentos redesenhos.
    ESPERA_TEXTO(v.prazo, "vale 5 min");

    // Arredonda PARA CIMA: com dez segundos, "1 min".
    e.agora_ms += 240000u;
    vista_vincular(&e, &v);
    ESPERA_TEXTO(v.prazo, "vale 1 min");

    // Expirado, o código fica e a linha diz que morreu.
    e.agora_ms += 61000u;
    vista_vincular(&e, &v);
    ESPERA(v.pode_gerar);
    ESPERA_TEXTO(v.codigo, "KXMPQR");
    ESPERA_TEXTO(v.prazo, "expirou");
    ESPERA_CONTEM(v.rodape_dir, "outro");

    TERMINA();
}

// A ordem dos passos: login antes do código (o consentimento é da pessoa).
void t_vincular_manda_logar_antes_de_digitar(void)
{
    COMECA("Vincular · loga primeiro, digita o código depois");

    base();
    e.rede = REDE_LIGADA;

    vista_vincular_t v;
    vista_vincular(&e, &v);

    ESPERA_CONTEM(v.passo[0], "aplicativo");
    ESPERA_CONTEM(v.passo[1], "Google");
    ESPERA_CONTEM(v.passo[2], "digite");
}

// ── desconectar ─────────────────────────────────────────────────────
// Nasce no NÃO (RN-6C) e diz o que NÃO se perde.
void t_desconectar_nasce_no_nao_e_diz_o_que_fica(void)
{
    COMECA("Desconectar · nasce no 'não' e promete o que não se perde");

    base();
    snprintf(e.nome, sizeof e.nome, "%s", "eu@x.com");
    e.cursor_overlay = 0;

    vista_confirma_t v;
    vista_desconectar(&e, &v);

    ESPERA_IGUAL(v.cursor, 0);
    ESPERA_CONTEM(v.nao, "não");
    ESPERA_CONTEM(v.explica, "cartão fica");
    ESPERA_CONTEM(v.explica, "anotações");
}

// Ajustes abre Minha Conta (o roteamento casava pelo rótulo renomeado).
void t_ajustes_abre_minha_conta(void)
{
    COMECA("Ajustes · a linha de Minha Conta abre Minha Conta");

    base();

    vista_menu_t v;
    vista_ajustes(&e, 12, &v);

    bool achou = false;
    for (int i = 0; i < v.n; i++)
        if (strcmp(v.linhas[i].texto, "Minha conta") == 0) {
            achou = true;
            // E o ícone é uma PESSOA, de 26 px na raiz.
            ESPERA_IGUAL(v.linhas[i].icone, ICO_CAT_CONTA);
        }

    ESPERA(achou);
}

// O caminho inteiro, do repouso ao QR: havia teste da vista e da tela, não
// do CAMINHO, e era o caminho que estava quebrado (`empilha` desistia no
// quarto nível).
void t_do_repouso_ate_o_qr_sem_perder_nenhum_gesto(void)
{
    COMECA("e2e · da home ao QR: Ajustes › Minha Conta › Conectar");

    static app_t ap;
    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 26}, 9, 41);
    app_liga(&ap, hal);
    app_passo(&ap);

    // Com rede e sem conta: o Tinto recém-saído da caixa.
    ap.estado.rede = REDE_LIGADA;
    ap.estado.nome[0] = '\0';
    ap.estado.tem_token = true;

    ENTRA_NOS_AJUSTES(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_AJUSTES);

    DESCE_ATE(&ap, vista_ajustes, "Minha conta");
    pc_botao(IN_OK);                     // Minha Conta — nível 2
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_CONTA);

    // Sem conta, um destino: conectar.
    ap.estado.cursor = 0;
    pc_botao(IN_OK);                     // Conectar — nível 3, o que sumia
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_VINCULAR);

    // A tela abre OFERECENDO gerar.
    vista_vincular_t v;
    vista_vincular(&ap.estado, &v);
    ESPERA(v.pode_gerar);
    ESPERA_CONTEM(v.passo[1], "Google");

    // E o OK gera.
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_TEXTO(pc_nuvem_rota(), "/v1/parear/iniciar");

    // O BACK devolve Minha Conta: o degrau existe de verdade.
    pc_botao(IN_VOLTAR);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_CONTA);

    TERMINA();
}

// Pilha cheia troca o topo em vez de sumir com o gesto.
void t_pilha_cheia_troca_o_topo_em_vez_de_sumir(void)
{
    COMECA("navegação · pilha cheia troca o topo, nunca engole o gesto");

    static app_t ap;
    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 26}, 9, 41);
    app_liga(&ap, hal);
    app_passo(&ap);

    ap.estado.rede = REDE_LIGADA;
    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", "eu@x.com");
    ap.estado.quota.limite_s = 1800;

    ENTRA_NOS_AJUSTES(&ap);
    DESCE_ATE(&ap, vista_ajustes, "Minha conta");
    pc_botao(IN_OK);
    app_passo(&ap);
    // "Uso de voz" é o segundo destino.
    ap.estado.cursor = 1;
    pc_botao(IN_OK);
    app_passo(&ap);

    // Fundo da pilha: quatro níveis.
    ESPERA_IGUAL(ap.estado.profundidade, PILHA_MAX - 1);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_FALA);

    TERMINA();
}

// O ▼ nunca dá toque morto: a informação mora no card, fora do caminho do
// cursor.
void t_o_cursor_pula_as_linhas_que_nao_se_apertam(void)
{
    COMECA("Minha conta · o cursor só anda no que se aperta");

    estado_t e;
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.rede = REDE_LIGADA;
    snprintf(e.nome, sizeof e.nome, "%s", "usuario@exemplo.com");
    snprintf(e.inicio.nome_pendente, sizeof e.inicio.nome_pendente,
             "%s", "Usuário");
    e.n_agendas = 5;

    vista_cartao_t v;
    vista_conta(&e, &v);

    // Quatro destinos; os fatos ficam fora da conta.
    ESPERA_IGUAL(v.n_dest, 4);
    ESPERA(v.n_fatos > 0);

    // Quatro paradas: sair é ícone no card (cursor -1). Como linha no fim, a
    // tela rolava.
    ESPERA_IGUAL(vista_conta_paradas(&e), 4);
    ESPERA(v.tem_sair);
    TERMINA();
}

// Nenhum estado do vínculo fica mudo: cobertura, exige que haja texto e que
// ele diga o que falta fazer.
void t_todo_estado_do_vinculo_diz_alguma_coisa(void)
{
    COMECA("cobertura · todo estado do vínculo tem tela e mensagem");

    base();
    snprintf(e.meu_id, sizeof e.meu_id, "%s", "AA:BB:CC:11:22:33");

    vista_cartao_t v;

    // ── 1. sem rede ──
    e.rede = REDE_DESLIGADA;
    vista_conta(&e, &v);
    ESPERA(tem_dest(&v, "Conexão"));
    ESPERA(v.corpo[0]);

    // ── 2. com rede, ainda SEM TOKEN: registrando ──
    e.rede = REDE_LIGADA;
    e.tem_token = false;
    vista_vincular_t vv;
    vista_vincular(&e, &vv);
    ESPERA(vv.sem_token);
    ESPERA_TEXTO(vv.id, "AA:BB:CC:11:22:33");
    ESPERA(vv.passo[1][0]);

    // E o primeiro uso também.
    e.inicio.fase = INICIO_CONTA;
    vista_inicializacao_t vi;
    vista_inicializacao(&e, &vi);
    ESPERA_CONTEM(vi.titulo, "Conectando");
    e.inicio.fase = INICIO_HOME;

    // ── 3. registrado, SEM CONTA ──
    e.tem_token = true;
    vista_conta(&e, &v);
    ESPERA(tem_dest(&v, "Google"));
    ESPERA(v.corpo[0]);
    // Sem conta, sem voz.
    ESPERA(!tem_dest(&v, "Uso de voz"));

    // A tela de Conectar espera o código, e diz.
    vista_vincular(&e, &vv);
    ESPERA(!vv.sem_token);
    ESPERA(vv.pode_gerar);
    ESPERA(vv.passo[0][0] && vv.passo[1][0] && vv.passo[2][0]);
    ESPERA_CONTEM(vv.rodape_dir, "gerar");

    // Com o código, ele aparece com prazo.
    snprintf(e.codigo, sizeof e.codigo, "%s", "KXMPQR");
    e.codigo_ate_ms = e.agora_ms + 300000u;
    vista_vincular(&e, &vv);
    ESPERA(!vv.esperando);
    ESPERA(!vv.pode_gerar);
    ESPERA_TEXTO(vv.codigo, "KXMPQR");
    ESPERA(vv.prazo[0]);
    e.codigo[0] = '\0';
    e.codigo_ate_ms = 0;

    // ── 4. com conta ──
    snprintf(e.nome, sizeof e.nome, "%s", "eu@x.com");
    e.quota.limite_s = 1800;
    vista_conta(&e, &v);
    ESPERA(fato_de(&v, "Google")[0]);
    ESPERA(tem_dest(&v, "Uso de voz"));
    ESPERA(tem_dest(&v, "Sincronizar"));
    ESPERA(v.tem_sair);
    // O código sumiu.
    ESPERA(!tem_dest(&v, "Conectar conta"));

    TERMINA();
}

// As recusas do gesto dizem o remédio certo (Wi-Fi não resolve falta de
// minutos).
void t_as_recusas_do_gesto_dizem_o_remedio_certo(void)
{
    COMECA("cobertura · cada recusa do gesto tem o seu próprio remédio");

    static uint8_t mem[(TELA_L + 7) / 8 * TELA_A];
    bitmap_t bm;
    bitmap_liga(&bm, mem, TELA_L, TELA_A);

    const int motivos[] = { RECUSA_REDE, RECUSA_MINUTOS, RECUSA_SEM_CONTA,
                            RECUSA_CONTA, RECUSA_TOKEN };

    for (size_t i = 0; i < sizeof motivos / sizeof motivos[0]; i++) {
        gfx_limpa(&bm, false);
        gfx_zera_faltantes();

        ui_faixa_recusa(&bm, "falar", motivos[i]);

        // Desenhou alguma coisa.
        int tinta = 0;
        for (int y = TELA_A - 120; y < TELA_A; y++)
            for (int x = 0; x < TELA_L; x++)
                if (gfx_le(&bm, x, y)) tinta++;

        if (!tinta) printf("      motivo %d não desenha nada\n", motivos[i]);
        ESPERA(tinta > 0);
        ESPERA_IGUAL(gfx_faltantes(), 0);
    }

    TERMINA();
}

// Minha conta tem quatro estados, cada um com UMA saída; o card informa e o
// cursor nunca para nele.
void t_minha_conta_tem_quatro_estados(void)
{
    COMECA("Minha conta · quatro estados, cada um com a sua saída");

    estado_t e;
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){ 2026, 9, 2 };
    e.hora = 9; e.minuto = 14; e.hora_confiavel = true;
    snprintf(e.inicio.nome_pendente, sizeof e.inicio.nome_pendente,
             "%s", "Usuário");

    vista_cartao_t v;

    // ── 1. sem rede: o dono visível, a saída é a Conexão ──
    e.rede = REDE_DESLIGADA;
    vista_conta(&e, &v);
    ESPERA_IGUAL(v.estado, CONTA_SEM_REDE);
    ESPERA_TEXTO(v.nome, "Usuário");
    // Dois: a Conexão e o Proprietário (que existe nos quatro).
    ESPERA_IGUAL(v.n_dest, 2);
    ESPERA_CONTEM(v.dest[0].titulo, "Conexão");
    ESPERA_CONTEM(v.dest[1].titulo, "Proprietário");
    ESPERA(!v.tem_sair);

    e.rede = REDE_LIGADA;

    // ── 2. com rede, sem Google: oferece conectar ──
    vista_conta(&e, &v);
    ESPERA_IGUAL(v.estado, CONTA_SEM_GOOGLE);
    ESPERA_IGUAL(v.n_dest, 2);
    ESPERA_CONTEM(v.dest[0].titulo, "Google");
    ESPERA_CONTEM(v.dest[1].titulo, "Proprietário");
    // Sem dizer que o aparelho está inutilizável.
    ESPERA_CONTEM(v.corpo, "já funciona");

    // ── 4. conectada ──
    snprintf(e.nome, sizeof e.nome, "%s", "usuario@exemplo.com");
    e.quota.usados_s = 18 * 60;
    e.quota.limite_s = 60 * 60;
    e.n_agendas = 5;
    for (int i = 0; i < 3; i++) e.agendas[i].ligada = true;
    e.agora_ms = 600000u;
    e.sinc_ultima_ms = 600000u - 120000u;   // dois minutos atrás

    vista_conta(&e, &v);
    ESPERA_IGUAL(v.estado, CONTA_CONECTADA);

    // Quatro: proprietário, uso de voz, sincronizar agendas e aplicativo.
    ESPERA_IGUAL(v.n_dest, 4);
    ESPERA_TEXTO(v.dest[1].valor, "18 min");
    ESPERA_TEXTO(v.dest[2].valor, "3 de 5");
    ESPERA_CONTEM(v.dest[2].titulo, "Sincronizar agendas");
    ESPERA(v.tem_sair);

    bool tem_sinc = false;
    for (int i = 0; i < v.n_fatos; i++)
        if (strstr(v.fatos[i].valor, "há 2 min")) tem_sinc = true;
    ESPERA(tem_sinc);

    // Só os destinos são parada; sair é ícone no card.
    ESPERA_IGUAL(vista_conta_paradas(&e), v.n_dest);
    ESPERA(v.tem_sair);
    TERMINA();
}

// Sincronização separa os dois erros: sem internet (saída: Conexão) e
// servidor que não respondeu (saída: tentar de novo). Nunca as duas.
void t_sincronizacao_separa_os_dois_erros(void)
{
    COMECA("sincronização · sem internet e servidor mudo são telas diferentes");

    estado_t e;
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    snprintf(e.nome, sizeof e.nome, "%s", "usuario@exemplo.com");

    vista_cartao_t v;

    // ── sem internet ──
    e.rede = REDE_DESLIGADA;
    vista_sincronizacao(&e, &v);
    ESPERA_CONTEM(v.nome, "Sem internet");
    ESPERA_IGUAL(v.n_dest, 1);
    ESPERA_CONTEM(v.dest[0].titulo, "Conexão");

    // ── internet de pé, e o outro lado não respondeu ──
    e.rede = REDE_LIGADA;
    e.agendas_pedidas  = true;
    e.agendas_buscando = false;
    vista_sincronizacao(&e, &v);
    ESPERA_CONTEM(v.nome, "Não consegui atualizar");
    ESPERA_IGUAL(v.n_dest, 1);
    ESPERA_CONTEM(v.dest[0].titulo, "Tentar novamente");

    // Nunca as duas.
    for (int i = 0; i < v.n_dest; i++)
        ESPERA(strstr(v.dest[i].titulo, "Conexão") == NULL);

    // ── com agendas ──
    e.n_agendas = 2;
    snprintf(e.agendas[0].nome, sizeof e.agendas[0].nome, "%s", "Pessoal");
    snprintf(e.agendas[1].nome, sizeof e.agendas[1].nome, "%s", "Feriados");
    e.agendas[0].ligada = true;
    e.agendas[1].por_ano = 40;
    e.agora_ms = 600000u;
    e.sinc_ultima_ms = 600000u - 120000u;

    vista_sincronizacao(&e, &v);

    bool marcada = false, vazia = false;
    for (int i = 0; i < v.n_dest; i++) {
        if (v.dest[i].ico == ICO_CAIXA_ON) marcada = true;
        if (v.dest[i].ico == ICO_CAIXA)    vazia   = true;
    }
    ESPERA(marcada);
    ESPERA(vazia);

    // A última sincronia é fato do card.
    bool tem_sinc = false;
    for (int i = 0; i < v.n_fatos; i++)
        if (strstr(v.fatos[i].valor, "há 2 min")) tem_sinc = true;
    ESPERA(tem_sinc);
    TERMINA();
}

// Minha conta NÃO ROLA em nenhum estado (rolando, o card ficava impresso
// sobre a lista). Estado novo que passar do quadro quebra aqui.
void t_minha_conta_nao_rola_em_nenhum_estado(void)
{
    COMECA("Minha conta · cabe no quadro, e por isso não rola");

    static uint8_t bits[(TELA_L + 7) / 8 * TELA_A];
    bitmap_t bm;
    bitmap_liga(&bm, bits, TELA_L, TELA_A);

    for (int caso = 0; caso < 4; caso++) {
        memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
        e.hoje = (data_t){2026, 9, 2};
        e.hora = 9; e.minuto = 14;

        if (caso >= 1) e.rede = REDE_LIGADA;
        if (caso >= 2) snprintf(e.nome, sizeof e.nome, "%s",
                                "usuario.nome.longo@exemplo.com");
        if (caso >= 3) {
            snprintf(e.inicio.nome_pendente, sizeof e.inicio.nome_pendente,
                     "%s", "Usuário");
            snprintf(e.wifi_atual, sizeof e.wifi_atual, "%s", "Casa 5G");
            e.quota.limite_s = 3600; e.quota.usados_s = 1080;
            e.n_agendas = 5;
            for (int i = 0; i < 3; i++) e.agendas[i].ligada = true;
            e.sinc_ultima_ms = 1;
        }

        vista_cartao_t v;
        vista_conta(&e, &v);
        ESPERA(tela_cartao_altura(&bm, &v) <= tela_cartao_area(&bm));
    }

    TERMINA();
}

// Sincronização no desenho: o card responde "como está", e abaixo só
// escolhas.
void t_sincronizacao_mostra_o_card_e_as_agendas(void)
{
    COMECA("Sincronização · o card diz como está, e a lista só escolhe");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 9, 2};
    e.rede = REDE_LIGADA;
    e.sinc_ultima_ms = 1;
    snprintf(e.nome, sizeof e.nome, "%s", "usuario@exemplo.com");

    const char *nomes[] = { "Agenda principal", "Trabalho", "Aniversários",
                            "Feriados no Brasil", "Rotina antiga" };
    for (int i = 0; i < 5; i++) {
        snprintf(e.agendas[i].nome, sizeof e.agendas[0].nome, "%s", nomes[i]);
        e.agendas[i].ligada = i < 3;
        e.n_agendas++;
    }
    e.agendas[3].por_ano = 18;

    vista_cartao_t v;
    vista_sincronizacao(&e, &v);

    // ── o card ──
    ESPERA_CONTEM(v.kicker, "estado atual");
    ESPERA_TEXTO(v.nome, "Tudo atualizado");
    ESPERA_IGUAL(v.n_fatos, 3);
    ESPERA_TEXTO(v.fatos[0].rotulo, "Conta");
    ESPERA_TEXTO(v.fatos[1].rotulo, "Última sincronia");
    ESPERA_TEXTO(v.fatos[2].rotulo, "Agendas ativas");
    ESPERA_TEXTO(v.fatos[2].valor,  "3 de 5");

    // ── a lista: só escolhas, quatro por página ──
    ESPERA_IGUAL(v.n_dest, 4);
    ESPERA_IGUAL(v.pagina, 1);
    ESPERA_IGUAL(v.paginas, 2);
    ESPERA_CONTEM(v.secao, "AGENDAS");
    ESPERA_TEXTO(v.dest[0].titulo, "Agenda principal");
    ESPERA_IGUAL(v.dest[0].ico, ICO_CAIXA_ON);
    ESPERA_TEXTO(v.dest[0].valor, "");

    // O número que explica a agenda desligada, na legenda.
    ESPERA_CONTEM(v.dest[3].sub, "18");

    // O cursor na quinta leva à página 2.
    e.cursor = 4;
    vista_sincronizacao(&e, &v);
    ESPERA_IGUAL(v.pagina, 2);
    ESPERA_IGUAL(v.n_dest, 1);
    ESPERA_IGUAL(v.cursor, 0);
    ESPERA_TEXTO(v.dest[0].titulo, "Rotina antiga");
    ESPERA_IGUAL(v.dest[0].ico, ICO_CAIXA);
    ESPERA_IGUAL(vista_sincronizacao_paradas(&e), 5);

    TERMINA();
}

// Doze agendas e três de fora: três páginas, nada passa do quadro, e a
// última diz quantas não couberam.
void t_doze_agendas_paginam_e_as_de_fora_sao_ditas(void)
{
    COMECA("Sincronização · doze em três páginas, e as que sobram são ditas");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;
    e.hoje = (data_t){2026, 9, 2};
    e.rede = REDE_LIGADA;
    e.sinc_ultima_ms = 1;
    snprintf(e.nome, sizeof e.nome, "%s", "usuario@exemplo.com");
    for (int i = 0; i < AGENDAS_MAX; i++) {
        // 31 letras, o teto do servidor.
        snprintf(e.agendas[i].nome, sizeof e.agendas[0].nome,
                 "Aniversários da família %02d xyz", i);
        e.agendas[i].por_ano = 120;
        e.n_agendas++;
    }
    e.agendas_fora = 3;

    static uint8_t mem[TELA_L / 8 * TELA_A];
    bitmap_t bm;
    bitmap_liga(&bm, mem, TELA_L, TELA_A);

    for (int c = 0; c < AGENDAS_MAX; c++) {
        e.cursor = c;
        vista_cartao_t v;
        vista_sincronizacao(&e, &v);
        ESPERA_IGUAL(v.paginas, 3);
        ESPERA_IGUAL(v.pagina, c / 4 + 1);
        ESPERA_IGUAL(v.cursor, c % 4);
        ESPERA(tela_cartao_altura(&bm, &v) <= tela_cartao_area(&bm));
        if (v.pagina == 3) ESPERA_TEXTO(v.secao, "+3 AGENDAS NÃO COUBERAM");
        else               ESPERA_TEXTO(v.secao, "AGENDAS VISÍVEIS");
    }
    TERMINA();
}

// Buscando é o MESMO card, com os pontinhos.
void t_buscando_agendas_e_o_mesmo_card_com_pontinhos(void)
{
    COMECA("Sincronização · buscando é o mesmo card, com os pontinhos");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 9, 2};
    e.rede = REDE_LIGADA;
    snprintf(e.nome, sizeof e.nome, "%s", "usuario@exemplo.com");
    snprintf(e.wifi_atual, sizeof e.wifi_atual, "%s", "Casa");
    e.agendas_buscando = true;

    vista_cartao_t v;
    int visto[4] = {0};
    for (int s = 0; s < 4; s++) {
        e.agora_ms = (uint32_t)s * 1000u;
        vista_sincronizacao(&e, &v);
        ESPERA(v.pontos >= 0 && v.pontos <= 3);
        visto[v.pontos] = 1;
    }
    ESPERA(visto[0] && visto[1] && visto[2] && visto[3]);

    ESPERA_TEXTO(v.nome, "Buscando agendas");
    ESPERA_IGUAL(v.n_dest, 0);       // nada a escolher enquanto não chega

    TERMINA();
}

// O nome do dono se troca SEMPRE: é do aparelho, não da conta.
void t_o_proprietario_se_troca_em_qualquer_estado(void)
{
    COMECA("Minha conta · o nome do dono se troca sem rede e sem conta");

    for (int caso = 0; caso < 3; caso++) {
        memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
        e.hoje = (data_t){2026, 9, 3};
        snprintf(e.inicio.nome_pendente, sizeof e.inicio.nome_pendente,
                 "%s", "Usuário");

        if (caso >= 1) e.rede = REDE_LIGADA;          // rede, sem conta
        if (caso >= 2) e.tem_token = true;            // registrado

        vista_cartao_t v;
        vista_conta(&e, &v);

        bool tem = false;
        for (int i = 0; i < v.n_dest; i++)
            if (strstr(v.dest[i].titulo, "Proprietário")) tem = true;
        ESPERA(tem);
    }

    TERMINA();
}

// Wi-Fi conectado sem internet (portal de café): não manda conectar a
// conta. O sinal é o X da barra (SINC_ERRO).
void t_minha_conta_reconhece_wifi_sem_internet(void)
{
    estado_t e;
    COMECA("Minha Conta · com Wi-Fi e sem internet, não manda conectar conta");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;

    e.rede = REDE_LIGADA;
    e.sinc = SINC_ERRO;
    e.recusa = RECUSA_NADA;

    vista_cartao_t v;
    vista_conta(&e, &v);

    ESPERA_TEXTO(v.kicker, "sem internet");
    ESPERA_CONTEM(v.corpo, "não alcança o servidor");

    // Não oferece conectar a conta.
    for (int i = 0; i < v.n_dest; i++)
        ESPERA_SEM(v.dest[i].titulo, "Conectar conta");

    // Recusa (401, 403, 409) é o servidor respondendo: tem tela própria.
    e.recusa = RECUSA_CONTA;
    vista_conta(&e, &v);
    ESPERA_SEM(v.kicker, "sem internet");

    TERMINA();
}
