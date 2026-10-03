#include "teste.h"
#include "dado/memoria.h"
#include "dado/perfil.h"
#include "uso/inicializacao.h"
#include "vista/inicializacao.h"
#include "vista/teclado.h"
#include "vista/conexao.h"
#include "vista/bloqueio.h"
#include "dado/rede.h"
#include "tela/logo_tinto.h"
#include <string.h>

// RN-6A e RN-6F: a inicialização é um modo ACIMA da pilha.

static app_t ap;

static void liga_virgem(const hal_t **hal)
{
    *hal = pc_liga();
    pc_memoria_virgem();
    app_liga(&ap, *hal);
}

void t_memoria_virgem_vai_para_boas_vindas(void)
{
    COMECA("RN-6B · memória virgem prepara e abre boas-vindas");
    const hal_t *hal;
    liga_virgem(&hal);

    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_BOAS_VINDAS);
    ESPERA(pc_tem_arquivo("/TINTO/sistema/formato.json"));
    TERMINA();
}

void t_cada_defeito_da_midia_tem_fase_propria(void)
{
    COMECA("RN-6A · cada defeito da mídia abre a fase que corresponde a ele");

    struct { memoria_estado_t midia; inicio_fase_t fase; } casos[] = {
        { MEMORIA_AUSENTE,         INICIO_MEMORIA_AUSENTE },
        { MEMORIA_COMUNICACAO,     INICIO_MEMORIA_COMUNICACAO },
        { MEMORIA_SEM_FILESYSTEM,  INICIO_MEMORIA_REPARO },
        { MEMORIA_CORROMPIDA,      INICIO_MEMORIA_REPARO },
        { MEMORIA_SOMENTE_LEITURA, INICIO_MEMORIA_SOMENTE_LEITURA },
        { MEMORIA_CHEIA,           INICIO_MEMORIA_CHEIA },
    };
    for (size_t i = 0; i < sizeof casos / sizeof casos[0]; i++) {
        const hal_t *hal = pc_liga();
        pc_memoria_estado(casos[i].midia);
        app_liga(&ap, hal);
        ESPERA_IGUAL(ap.estado.inicio.fase, casos[i].fase);
        ESPERA_IGUAL(ap.estado.inicio.diagnostico, casos[i].midia);
    }
    TERMINA();
}

void t_formato_futuro_nao_oferece_reparo(void)
{
    COMECA("RN-63 · cartão de versão futura abre tela própria, não reparo");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    hal->criar_diretorio("/TINTO");
    hal->criar_diretorio("/TINTO/sistema");
    pc_poe_arquivo("/TINTO/sistema/formato.json", "{\"v\":99}");

    app_liga(&ap, hal);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_FORMATO_FUTURO);

    pc_botao(IN_OK);
    pc_botao(IN_BAIXO);
    app_passo(&ap);
    char buf[32];
    ESPERA_IGUAL(hal->ler("/TINTO/sistema/formato.json", buf, sizeof buf), OK);
    ESPERA_TEXTO(buf, "{\"v\":99}");
    TERMINA();
}

void t_arvore_danificada_vai_para_reparo(void)
{
    COMECA("/TINTO/ sem formato abre recuperação, e não provisiona por cima");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    hal->criar_diretorio("/TINTO");
    pc_poe_arquivo("/TINTO/coisa-de-alguem.txt", "conteudo alheio");

    app_liga(&ap, hal);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_MEMORIA_REPARO);
    ESPERA(!pc_tem_arquivo("/TINTO/sistema/formato.json"));
    ESPERA(pc_tem_arquivo("/TINTO/coisa-de-alguem.txt"));
    TERMINA();
}

void t_antes_da_home_nenhum_botao_escapa(void)
{
    COMECA("RN-6F · voz, MENU e direções não escapam do primeiro uso");
    const hal_t *hal;
    liga_virgem(&hal);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_BOAS_VINDAS);

    int escritas_antes = pc_escritas();
    pc_botao(IN_VOZ);
    pc_botao(IN_MENU);
    pc_botao(IN_BAIXO);
    pc_botao(IN_DIR);
    pc_botao(IN_VOLTAR);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_BOAS_VINDAS);
    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_PARADA);
    ESPERA(!pc_gravando());
    ESPERA_IGUAL(ap.estado.profundidade, 0);
    ESPERA_IGUAL(pc_escritas(), escritas_antes);
    TERMINA();
}

void t_boas_vindas_abre_o_nome(void)
{
    COMECA("o OK das boas-vindas abre o nome, sem gravar nada");
    const hal_t *hal;
    liga_virgem(&hal);

    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_NOME);
    ESPERA(!pc_tem_arquivo("/TINTO/sistema/perfil.json"));
    TERMINA();
}

void t_reboot_retoma_na_primeira_pendencia(void)
{
    COMECA("RN-6F · reiniciar no meio volta para a pendência, não para o zero");
    const hal_t *hal;
    liga_virgem(&hal);

    pc_botao(IN_OK);            // boas-vindas
    app_passo(&ap);
    ESPERA_IGUAL(uso_configurar_dispositivo(hal, &ap.estado,
                                            INICIO_CMD_SALVAR_NOME,
                                            "Usuário"), OK);

    // Desliga e liga: a mídia é a mesma, a RAM não.
    app_liga(&ap, hal);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_WIFI);
    TERMINA();
}

void t_desligar_sem_nome_volta_para_as_boas_vindas(void)
{
    COMECA("reiniciar sem nome volta para as boas-vindas");
    const hal_t *hal;
    liga_virgem(&hal);

    app_liga(&ap, hal);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_BOAS_VINDAS);
    ESPERA(!pc_tem_arquivo("/TINTO/sistema/perfil.json"));
    TERMINA();
}

void t_onboarding_concluido_abre_a_home(void)
{
    COMECA("RN-6F · só a conclusão libera a home");
    const hal_t *hal;
    liga_virgem(&hal);

    pc_botao(IN_OK);   // boas-vindas
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_NOME);

    // O nome entra pelo caso de uso (o teclado é da UI).
    ESPERA_IGUAL(uso_configurar_dispositivo(hal, &ap.estado,
                                            INICIO_CMD_SALVAR_NOME,
                                            "Usuário"), OK);
    // Depois do nome vêm Wi-Fi e conta, que se pulam.
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_WIFI);

    // "▶ pular" o Wi-Fi pula a conta junto.
    pc_botao(IN_DIR);
    app_passo(&ap);

    // RN-6G: sem NTP, a hora é pendência antes da conclusão.
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_DATA_HORA);
    ESPERA(!ap.estado.hora_confiavel);
    pc_botao(IN_OK);   // aceita a data sugerida
    app_passo(&ap);
    ESPERA(ap.estado.hora_confiavel);

    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_CONCLUSAO);

    pc_botao(IN_OK);   // conclui
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_HOME);

    // A fase INICIO_HOME libera a pilha; a tela é a Home 2x2.
    ESPERA_IGUAL(ap.estado.pilha[0], TELA_HOME);

    // E continua aberto no boot seguinte.
    app_liga(&ap, hal);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_HOME);
    TERMINA();
}

void t_aparelho_configurado_nao_repete_o_primeiro_uso(void)
{
    COMECA("cartão já configurado abre direto na home, sem reescrever nada");
    const hal_t *hal = pc_liga();     // o padrão do hal do PC é configurado
    int escritas_antes = pc_escritas();

    app_liga(&ap, hal);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_HOME);

    // Não reescreve perfil, recibo nem formato de um cartão que já os tem.
    perfil_local_t p;
    ESPERA_IGUAL(perfil_carrega(hal, &p), OK);
    ESPERA_TEXTO(p.proprietario_id, "0123456789abcdef0123456789abcdef");
    ESPERA_IGUAL(p.onboarding_v, 1);
    ESPERA(pc_escritas() >= escritas_antes);
    TERMINA();
}

// ── apagar exige dois gestos deliberados ────────────────────────────

static void liga_danificado(const hal_t **hal)
{
    *hal = pc_liga();
    pc_memoria_virgem();
    (*hal)->criar_diretorio("/TINTO");
    pc_poe_arquivo("/TINTO/nao-apague.txt", "trabalho de alguem");
    app_liga(&ap, *hal);
}

void t_reparo_nao_destrutivo_nao_apaga_nada(void)
{
    COMECA("RN-6C · a saída segura vem selecionada e não apaga nada");
    const hal_t *hal;
    liga_danificado(&hal);

    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_MEMORIA_REPARO);
    ESPERA_IGUAL(ap.estado.inicio.cursor, 0);   // "verificar no computador"

    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(pc_formatacoes(), 0);
    ESPERA(pc_tem_arquivo("/TINTO/nao-apague.txt"));
    TERMINA();
}

void t_formatar_exige_dois_gestos_e_a_pergunta_nasce_no_nao(void)
{
    COMECA("RN-6C · escolher formatar abre a pergunta, e ela nasce no \"não\"");
    const hal_t *hal;
    liga_danificado(&hal);

    pc_botao(IN_BAIXO);      // "formatar como novo"
    pc_botao(IN_OK);
    app_passo(&ap);

    // A tela mudou, nada foi apagado, o cursor voltou ao seguro.
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_CONFIRMAR_FORMATAR);
    ESPERA_IGUAL(ap.estado.inicio.cursor, 0);
    ESPERA_IGUAL(pc_formatacoes(), 0);
    ESPERA(pc_tem_arquivo("/TINTO/nao-apague.txt"));

    // OK aqui é "não, voltar".
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_MEMORIA_REPARO);
    ESPERA_IGUAL(pc_formatacoes(), 0);
    ESPERA(pc_tem_arquivo("/TINTO/nao-apague.txt"));
    TERMINA();
}

void t_so_o_sim_deliberado_formata(void)
{
    COMECA("RN-6C · só mover para \"sim\" e confirmar chega a formatar");
    const hal_t *hal;
    liga_danificado(&hal);

    pc_botao(IN_BAIXO);      // formatar como novo
    pc_botao(IN_OK);         // abre a confirmação
    pc_botao(IN_BAIXO);      // move para "sim, apagar tudo"
    pc_botao(IN_OK);         // confirma
    app_passo(&ap);

    ESPERA_IGUAL(pc_formatacoes(), 1);
    ESPERA(!pc_tem_arquivo("/TINTO/nao-apague.txt"));

    // Formatou, provisionou e caiu no primeiro uso.
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_BOAS_VINDAS);
    ESPERA(pc_tem_arquivo("/TINTO/sistema/formato.json"));
    TERMINA();
}

void t_voltar_da_confirmacao_nao_apaga(void)
{
    COMECA("BACK na confirmação volta ao reparo sem tocar no cartão");
    const hal_t *hal;
    liga_danificado(&hal);

    pc_botao(IN_BAIXO);
    pc_botao(IN_OK);
    pc_botao(IN_BAIXO);      // já em "sim, apagar tudo"
    pc_botao(IN_VOLTAR);     // desiste
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_MEMORIA_REPARO);
    ESPERA_IGUAL(ap.estado.inicio.cursor, 0);
    ESPERA_IGUAL(pc_formatacoes(), 0);
    TERMINA();
}

void t_ausencia_e_comunicacao_nunca_oferecem_formatar(void)
{
    COMECA("mídia ausente ou mal encaixada não oferece apagar nada");

    memoria_estado_t sem_saida[] = { MEMORIA_AUSENTE, MEMORIA_COMUNICACAO };
    for (size_t i = 0; i < sizeof sem_saida / sizeof sem_saida[0]; i++) {
        const hal_t *hal = pc_liga();
        pc_memoria_estado(sem_saida[i]);
        app_liga(&ap, hal);

        vista_inicializacao_t v;
        vista_inicializacao(&ap.estado, &v);
        ESPERA_IGUAL(v.n_opcoes, 0);

        pc_botao(IN_BAIXO);
        pc_botao(IN_OK);
        pc_botao(IN_OK);
        app_passo(&ap);
        ESPERA_IGUAL(pc_formatacoes(), 0);
    }
    TERMINA();
}

void t_a_tela_de_preparacao_aparece_antes_de_formatar(void)
{
    COMECA("a preparação é desenhada ANTES da operação que bloqueia");
    const hal_t *hal;
    liga_danificado(&hal);

    pc_botao(IN_BAIXO);
    pc_botao(IN_OK);
    pc_botao(IN_BAIXO);
    pc_botao(IN_OK);

    // Consome os eventos sem passar pelo app_passo.
    evento_t ev;
    while (hal->proximo_evento(&ev)) app_evento(&ap, &ev);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_PREPARANDO);
    ESPERA(ap.estado.inicio.formatar_pendente);
    ESPERA_IGUAL(pc_formatacoes(), 0);

    int quadros = pc_quadros();
    app_passo(&ap);
    ESPERA(pc_quadros() > quadros);      // desenhou antes de formatar
    ESPERA_IGUAL(pc_formatacoes(), 1);
    TERMINA();
}

// ── a marca vem do asset, não de reconstrução ───────────────────────

void t_logo_vem_do_asset_oficial(void)
{
    COMECA("a logo é o asset oficial reduzido, com procedência conferível");
    ESPERA_TEXTO(LOGO_TINTO_FONTE_SHA256,
        "7400f8129cd26e55c63e4b1ab469af4fbccb35ce696012d44973057d503939f4");

    ESPERA(LOGO_TINTO_L > 0 && LOGO_TINTO_L <= 224);
    ESPERA(LOGO_TINTO_A > 0 && LOGO_TINTO_A <= 150);

    // Tem branco E tinta.
    int passo = (LOGO_TINTO_L + 7) / 8;
    int tinta = 0;
    for (int i = 0; i < passo * LOGO_TINTO_A; i++)
        for (int b = 0; b < 8; b++)
            if (LOGO_TINTO_BITS[i] & (0x80 >> b)) tinta++;
    ESPERA(tinta > 500);
    ESPERA(tinta < passo * LOGO_TINTO_A * 8 / 2);

    // O quadriculado do export viraria listras na primeira linha.
    int na_primeira = 0;
    for (int b = 0; b < 8 * passo; b++)
        if (LOGO_TINTO_BITS[b / 8] & (0x80 >> (b % 8))) na_primeira++;
    ESPERA(na_primeira > 0);
    TERMINA();
}

// ── o cartão que some depois do boot ────────────────────────────────

void t_cartao_removido_em_uso_cai_na_tela_de_memoria(void)
{
    COMECA("RN-6A · cartão removido em uso abre a tela de memória na hora");
    const hal_t *hal = pc_liga();
    app_liga(&ap, hal);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_HOME);

    // A tampa abriu com o aparelho ligado.
    pc_memoria_estado(MEMORIA_AUSENTE);
    pc_tick();
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_MEMORIA_AUSENTE);

    // Recolocar e reiniciar abre a Home.
    pc_memoria_estado(MEMORIA_PRONTA);
    app_liga(&ap, hal);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_HOME);
    TERMINA();
}

void t_gravacao_nao_continua_sem_onde_escrever(void)
{
    COMECA("RN-A3 · perder o cartão gravando descarta, não finge que gravou");
    const hal_t *hal = pc_liga();
    app_liga(&ap, hal);
    ap.estado.rede = REDE_LIGADA;    // falar exige rede desde 25/08

    pc_botao(IN_VOZ);
    app_passo(&ap);
    ESPERA(pc_gravando());

    pc_memoria_estado(MEMORIA_AUSENTE);
    pc_tick();
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_MEMORIA_AUSENTE);
    ESPERA(!pc_gravando());
    TERMINA();
}

void t_o_relogio_anda_fora_da_home(void)
{
    COMECA("o relógio anda no onboarding, sem esperar um botão");
    const hal_t *hal;
    liga_virgem(&hal);

    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_BOAS_VINDAS);

    pc_relogio(ap.estado.hoje, 9, 30);
    evento_t tique = { .tipo = EV_TICK };
    app_evento(&ap, &tique);
    ESPERA_IGUAL(ap.estado.hora, 9);
    ESPERA_IGUAL(ap.estado.minuto, 30);

    // O relógio anda também no primeiro uso.
    pc_relogio(ap.estado.hoje, 9, 31);
    app_evento(&ap, &tique);
    ESPERA_IGUAL(ap.estado.minuto, 31);
    TERMINA();
}

void t_o_minuto_pede_quadro_o_segundo_nao(void)
{
    COMECA("vira o minuto, redesenha; passa o segundo, não");
    const hal_t *hal;
    liga_virgem(&hal);

    pc_relogio(ap.estado.hoje, 9, 30);
    evento_t tique = { .tipo = EV_TICK };
    app_evento(&ap, &tique);

    // Tick no mesmo minuto não pede quadro.
    app_desenha(&ap);
    ESPERA(!ap.precisa_desenhar);
    app_evento(&ap, &tique);
    ESPERA(!ap.precisa_desenhar);

    // Virou o minuto: aí sim.
    pc_relogio(ap.estado.hoje, 9, 31);
    app_evento(&ap, &tique);
    ESPERA(ap.precisa_desenhar);
    TERMINA();
}

void t_o_voltar_anda_para_tras_no_trecho(void)
{
    COMECA("◀ volta uma etapa dentro do trecho do aparelho");
    const hal_t *hal;
    liga_virgem(&hal);

    pc_botao(IN_OK);            // boas-vindas → nome
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_NOME);

    // Errar o caminho não custa reiniciar.
    pc_botao(IN_VOLTAR);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_BOAS_VINDAS);
    TERMINA();
}

void t_a_primeira_etapa_nao_tem_para_onde_voltar(void)
{
    COMECA("◀ na primeira etapa fica onde está");
    const hal_t *hal;
    liga_virgem(&hal);

    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_BOAS_VINDAS);
    pc_botao(IN_VOLTAR);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_BOAS_VINDAS);
    TERMINA();
}

void t_o_diagnostico_de_midia_nao_volta(void)
{
    COMECA("◀ não anda nas telas de mídia — diagnóstico não é etapa");
    const hal_t *hal = pc_liga();
    pc_memoria_estado(MEMORIA_SEM_FILESYSTEM);
    app_liga(&ap, hal);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_MEMORIA_REPARO);

    // Tela de diagnóstico não tem etapa anterior.
    pc_botao(IN_VOLTAR);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_MEMORIA_REPARO);
    TERMINA();
}

void t_o_voltar_so_anda_dentro_do_primeiro_uso(void)
{
    COMECA("o voltar só anda entre as etapas do primeiro uso");

    ESPERA(!inicio_pode_voltar(INICIO_MEMORIA_REPARO));
    ESPERA(inicio_pode_voltar(INICIO_CONTA));
    ESPERA_IGUAL(inicio_fase_anterior(INICIO_WIFI), INICIO_CONFIRMAR_DONO);

    // Depois da conclusão, voltar não reabre o primeiro uso.
    ESPERA_IGUAL(inicio_fase_anterior(INICIO_BOAS_VINDAS), INICIO_BOAS_VINDAS);
    ESPERA_IGUAL(inicio_fase_anterior(INICIO_CONCLUSAO),   INICIO_DATA_HORA);
    ESPERA(!inicio_pode_voltar(INICIO_HOME));
    TERMINA();
}

// Formatar faz o RÁDIO esquecer a rede: senão reconectava no Wi-Fi do dono
// anterior.
void t_formatar_faz_o_radio_esquecer_a_rede(void)
{
    COMECA("RN-6C · formatar apaga a rede do rádio, não só a do cartão");

    const hal_t *hal;
    liga_virgem(&hal);

    // Conectado, com a senha no cofre.
    hal->segredo_grava("wifi_senha", "senha-da-casa");
    ap.estado.rede = REDE_LIGADA;
    ap.estado.wifi_forca = 3;
    snprintf(ap.estado.wifi_atual, sizeof ap.estado.wifi_atual, "Casa");
    snprintf(ap.estado.wifi_salva, sizeof ap.estado.wifi_salva, "Casa");

    ESPERA(!pc_rede_esquecida());
    ESPERA_IGUAL(uso_configurar_dispositivo(hal, &ap.estado,
                                            INICIO_CMD_FORMATAR, NULL), OK);

    ESPERA(pc_rede_esquecida());

    // A senha da flash vai junto; a RAM, o reinício limpa.
    ESPERA_IGUAL(pc_segredo("wifi_senha")[0], '\0');

    TERMINA();
}

// A conclusão diz o que ficou faltando e onde resolver.
void t_a_conclusao_diz_o_que_ficou_faltando(void)
{
    COMECA("primeiro uso · a conclusão diz o que ficou faltando");

    const hal_t *hal;
    liga_virgem(&hal);
    ap.estado.inicio.fase = INICIO_CONCLUSAO;

    vista_inicializacao_t v;

    // Pulou tudo: uma pendência só.
    ap.estado.rede = REDE_DESLIGADA;
    ap.estado.nome[0] = '\0';
    vista_inicializacao(&ap.estado, &v);
    ESPERA_CONTEM(v.corpo, "conectar à rede");
    ESPERA_CONTEM(v.estados[1], "fazer depois");

    // Rede sim, conta não.
    ap.estado.rede = REDE_LIGADA;
    vista_inicializacao(&ap.estado, &v);
    ESPERA_CONTEM(v.corpo, "conta Google");
    ESPERA_CONTEM(v.estados[1], "conectada");

    // Tudo feito.
    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", "eu@x.com");
    vista_inicializacao(&ap.estado, &v);
    ESPERA_CONTEM(v.titulo, "Tudo pronto");
    ESPERA_CONTEM(v.estados[2], "vinculada");

    TERMINA();
}

// Logo depois do Wi-Fi, "conectando"; vira conectar a conta quando o token
// chega.
void t_o_primeiro_uso_diz_que_esta_conectando_antes_da_conta(void)
{
    COMECA("primeiro uso · sem token ainda, a tela diz que está conectando");

    static app_t ap;
    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 27}, 9, 41);
    app_liga(&ap, hal);
    app_passo(&ap);

    ap.estado.inicio.fase = INICIO_CONTA;
    ap.estado.tem_token   = false;

    vista_inicializacao_t v;
    vista_inicializacao(&ap.estado, &v);
    ESPERA_CONTEM(v.titulo, "Conectando");
    // Tela de espera: nada a apertar.
    ESPERA_IGUAL(v.rodape_esq[0], '\0');
    ESPERA_IGUAL(v.rodape_dir[0], '\0');
    ESPERA(v.pontos >= 0);
    pc_botao(IN_DIR);                      // ▶ não pula daqui
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_CONTA);

    ap.estado.tem_token = true;
    vista_inicializacao(&ap.estado, &v);
    ESPERA_CONTEM(v.titulo, "Conectado ao servidor");   // diz que deu certo
    ap.estado.inicio.servidor_visto = true;
    vista_inicializacao(&ap.estado, &v);
    ESPERA_CONTEM(v.titulo, "conta");
    ESPERA_CONTEM(v.rodape_dir, "OK");

    TERMINA();
}

// As fases de rede e conta desenham alguma coisa (eram um vidro em
// branco).
void t_as_fases_de_rede_e_conta_desenham_alguma_coisa(void)
{
    COMECA("primeiro uso · nenhuma fase sai com a tela em branco");

    static app_t ap;
    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 27}, 9, 41);
    app_liga(&ap, hal);
    app_passo(&ap);
    snprintf(ap.estado.inicio.nome_pendente,
             sizeof ap.estado.inicio.nome_pendente, "%s", "Convidado");

    // De 1 em diante: a fase zero é INICIO_HOME.
    for (int f = 1; f <= INICIO_PREPARANDO; f++) {
        // INICIO_NOME desenha o teclado.
        if (f == INICIO_NOME) continue;

        ap.estado.inicio.fase = (inicio_fase_t)f;

        vista_inicializacao_t v;
        vista_inicializacao(&ap.estado, &v);

        // Alguma coisa tem de dizer o que acontece.
        bool fala = v.titulo[0] || v.corpo[0] || v.alerta[0] || v.nome[0] ||
                    v.n_opcoes > 0 || v.n_campos > 0;
        if (!fala) printf("      fase %d não desenha nada\n", f);
        ESPERA(fala);
    }

    TERMINA();
}

// A capa diz o que vem: preparar o aparelho e como os dados são tratados.
void t_a_capa_diz_o_que_vem(void)
{
    COMECA("Primeiro uso · a capa diz o que vem a seguir");

    static estado_t e;
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.inicio.fase = INICIO_BOAS_VINDAS;

    vista_inicializacao_t v;
    vista_inicializacao(&e, &v);

    ESPERA_CONTEM(v.titulo, "Configure");
    ESPERA_CONTEM(v.corpo, "nome");
    ESPERA(v.logo);

    TERMINA();
}

// A conclusão mostra as TRÊS verificações: memória, internet e Google.
void t_a_conclusao_mostra_as_tres_verificacoes(void)
{
    COMECA("Primeiro uso · a conclusão diz o que ficou pronto, item a item");

    static estado_t e;
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.inicio.fase = INICIO_CONCLUSAO;
    e.rede = REDE_LIGADA;
    snprintf(e.nome, sizeof e.nome, "%s", "usuario@exemplo.com");
    snprintf(e.inicio.nome_pendente, sizeof e.inicio.nome_pendente,
             "%s", "Usuário");

    vista_inicializacao_t v;
    vista_inicializacao(&e, &v);

    ESPERA_CONTEM(v.titulo, "Usuário");
    ESPERA_IGUAL(v.n_estados, 3);
    ESPERA_CONTEM(v.estados[0], "Memória");
    ESPERA_CONTEM(v.estados[1], "Internet");
    ESPERA_CONTEM(v.estados[2], "Google");
    ESPERA_CONTEM(v.estados[2], "vinculada");

    // Sem Google, "Quase lá", dizendo qual faltou.
    e.nome[0] = '\0';
    vista_inicializacao(&e, &v);
    ESPERA_CONTEM(v.titulo, "Quase");
    ESPERA_CONTEM(v.estados[1], "conectada");
    ESPERA(strstr(v.estados[2], "vinculada") == NULL);

    TERMINA();
}

// ◀ na conclusão não cai numa Data e hora vazia.
void t_voltar_da_conclusao_nao_abre_data_e_hora_vazia(void)
{
    COMECA("◀ da conclusão: com rede volta à conta, sem rede à hora preenchida");
    const hal_t *hal;
    liga_virgem(&hal);

    ap.estado.inicio.fase = INICIO_CONCLUSAO;
    ap.estado.inicio.ano  = 0;
    ap.estado.rede = REDE_LIGADA;
    ap.estado.inicio.pulou_conta = true;
    pc_botao(IN_VOLTAR);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_CONTA);   // a pulada reabre
    ESPERA(!ap.estado.inicio.pulou_conta);

    ap.estado.inicio.fase = INICIO_CONCLUSAO;
    ap.estado.rede = REDE_DESLIGADA;
    pc_botao(IN_VOLTAR);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_DATA_HORA);
    ESPERA(ap.estado.inicio.ano >= 2024);
    ESPERA(ap.estado.inicio.mes >= 1);
    ESPERA(ap.estado.inicio.dia >= 1);
    TERMINA();
}

// Pular é deixar para depois: o BACK reabre.
void t_pular_o_wifi_e_voltar_reabre_o_wifi(void)
{
    COMECA("▶ pula o Wi-Fi, e o BACK da etapa seguinte o reabre");
    const hal_t *hal;
    liga_virgem(&hal);

    pc_botao(IN_OK); app_passo(&ap);            // boas-vindas
    ESPERA_IGUAL(uso_configurar_dispositivo(hal, &ap.estado,
                                            INICIO_CMD_SALVAR_NOME,
                                            "Usuário"), OK);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_WIFI);

    pc_botao(IN_DIR); app_passo(&ap);           // ▶ pular
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_DATA_HORA);

    pc_botao(IN_VOLTAR); app_passo(&ap);        // mudou de ideia
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_WIFI);
    ESPERA(!ap.estado.inicio.pulou_wifi);

    pc_botao(IN_VOLTAR); app_passo(&ap);        // e o BACK daqui volta
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_CONFIRMAR_DONO);
    TERMINA();
}

// Voltar ao Wi-Fi já conectado: o ▶ segue para a conta.
void t_voltar_para_o_wifi_conectado_e_seguir_volta_a_conta(void)
{
    COMECA("Wi-Fi já conectado: a tela diz, e ▶ segue para a conta");
    const hal_t *hal;
    liga_virgem(&hal);

    pc_botao(IN_OK); app_passo(&ap);            // boas-vindas
    ESPERA_IGUAL(uso_configurar_dispositivo(hal, &ap.estado,
                                            INICIO_CMD_SALVAR_NOME,
                                            "Usuário"), OK);
    ap.estado.rede = REDE_LIGADA;               // o Wi-Fi conectou
    snprintf(ap.estado.wifi_atual, sizeof ap.estado.wifi_atual, "%s", "Casa");
    ESPERA_IGUAL(uso_configurar_dispositivo(hal, &ap.estado,
                                            INICIO_CMD_BOOT, NULL), OK);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_CONTA);

    pc_botao(IN_VOLTAR); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_WIFI);

    vista_inicializacao_t v;
    vista_inicializacao(&ap.estado, &v);
    ESPERA_CONTEM(v.titulo, "conectado");
    ESPERA_CONTEM(v.corpo, "Casa");
    ESPERA_CONTEM(v.rodape_esq, "continuar");

    pc_botao(IN_DIR); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_CONTA);
    ESPERA(!ap.estado.inicio.pulou_wifi);
    TERMINA();
}

// Conta já vinculada: diz conectada e segue.
void t_voltar_para_a_conta_ja_vinculada_diz_conectada(void)
{
    COMECA("conta já vinculada: a tela diz, e ▶ segue para a conclusão");
    const hal_t *hal;
    liga_virgem(&hal);

    pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(uso_configurar_dispositivo(hal, &ap.estado,
                                            INICIO_CMD_SALVAR_NOME,
                                            "Usuário"), OK);
    ap.estado.rede = REDE_LIGADA;
    ap.estado.tem_token = true;
    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", "eu@x.com");
    ap.estado.hora_confiavel = true;
    ESPERA_IGUAL(uso_configurar_dispositivo(hal, &ap.estado,
                                            INICIO_CMD_BOOT, NULL), OK);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_CONCLUSAO);

    pc_botao(IN_VOLTAR); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_CONTA);

    vista_inicializacao_t v;
    vista_inicializacao(&ap.estado, &v);
    ESPERA_CONTEM(v.titulo, "conectada");
    ESPERA_CONTEM(v.corpo, "eu@x.com");

    pc_botao(IN_DIR); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_CONCLUSAO);
    TERMINA();
}

// Caixa alta redesenha na hora.
void t_caixa_alta_redesenha_na_hora(void)
{
    COMECA("primeiro uso · ABC troca a caixa e redesenha no mesmo toque");
    const hal_t *hal;
    liga_virgem(&hal);
    ap.estado.inicio.fase = INICIO_NOME;

    vista_teclado_t v;
    vista_teclado(&ap.estado, &v);
    bool achou = false;
    for (int l = 0; l < TEC_LINS && !achou; l++)
        for (int c = 0; c < TEC_COLS && !achou; c++)
            if (strcmp(v.teclas[l][c], "ABC") == 0) {
                ap.estado.teclado_lin = (int8_t)l;
                ap.estado.teclado_col = (int8_t)c;
                achou = true;
            }
    ESPERA(achou);

    app_desenha(&ap);
    int8_t antes = ap.estado.teclado_modo;
    evento_t ok = { .tipo = EV_BOTAO, .botao = IN_OK };
    app_evento(&ap, &ok);

    ESPERA(ap.estado.teclado_modo != antes);
    ESPERA(ap.precisa_desenhar);
    TERMINA();
}

// No primeiro uso o power trava como no resto, sem dormir.
void t_power_no_primeiro_uso_trava_e_destrava(void)
{
    COMECA("primeiro uso · power trava e destrava, sem reiniciar");
    const hal_t *hal;
    liga_virgem(&hal);

    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_NOME);

    pc_botao(IN_POWER);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_BLOQUEADA);

    // Travado no primeiro uso: só a bateria e a saída.
    vista_bloqueio_t vb;
    vista_bloqueio(&ap.estado, &vb);
    ESPERA_TEXTO(vb.hora, "");
    ESPERA_TEXTO(vb.data, "");
    ESPERA_TEXTO(vb.proximo, "");

    pc_botao(IN_VOLTAR);              // travado, nada anda
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_NOME);

    pc_botao(IN_POWER);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_INICIO);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_NOME);
    TERMINA();
}

// Restaurar reinicia o chip: a RAM anterior fazia o onboarding pular
// Wi-Fi e conta.
void t_restaurar_reinicia_o_chip(void)
{
    COMECA("restaurar · apaga e reinicia, sem nada da RAM de antes");
    const hal_t *hal;
    liga_virgem(&hal);
    ap.estado.inicio.pulou_wifi = true;
    ap.estado.inicio.fase   = INICIO_CONFIRMAR_FORMATAR;
    ap.estado.inicio.cursor = 1;

    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA(pc_rede_esquecida());
    ESPERA(pc_reiniciou());
    TERMINA();
}

// No primeiro uso o BACK da Conexão volta à etapa.
void t_back_da_conexao_no_primeiro_uso_diz_voltar(void)
{
    COMECA("primeiro uso · a Conexão diz BACK voltar, e volta à etapa");
    const hal_t *hal;
    liga_virgem(&hal);
    pc_botao(IN_OK);   // boas-vindas
    app_passo(&ap);
    ESPERA_IGUAL(uso_configurar_dispositivo(hal, &ap.estado,
                                            INICIO_CMD_SALVAR_NOME,
                                            "Usuário"), OK);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_WIFI);

    pc_botao(IN_OK);                 // "OK escolher rede"
    app_passo(&ap);

    vista_cartao_t v;
    vista_conexao(&ap.estado, &v);
    ESPERA_TEXTO(v.rodape_esq, "BACK voltar");

    pc_botao(IN_VOLTAR);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_WIFI);

    // Configurado, a mesma tela pertence aos Ajustes.
    ap.estado.pilha[0] = TELA_HOME;
    vista_conexao(&ap.estado, &v);
    ESPERA_TEXTO(v.rodape_esq, "BACK ajustes");
    TERMINA();
}

// O primeiro uso escuta o relógio e se apresenta (engolia o NTP: sem hora,
// sem TLS, sem registro).
void t_primeiro_uso_escuta_o_relogio_e_se_apresenta(void)
{
    COMECA("primeiro uso · a hora da rede chega, e o aparelho se registra");
    const hal_t *hal;
    liga_virgem(&hal);
    pc_botao(IN_OK);   // boas-vindas
    app_passo(&ap);
    ESPERA_IGUAL(uso_configurar_dispositivo(hal, &ap.estado,
                                            INICIO_CMD_SALVAR_NOME,
                                            "Usuário"), OK);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_WIFI);

    ap.estado.rede = REDE_LIGADA;
    pc_nuvem_rota_zera();
    pc_empurra((evento_t){ .tipo = EV_HORA_DA_REDE });
    app_passo(&ap);

    ESPERA(ap.estado.hora_confiavel);
    ESPERA_CONTEM(pc_nuvem_rota(), "/v1/registrar");
    TERMINA();
}

// Servidor fora do ar: depois de 20 s, erro com saída.
void t_conta_sem_resposta_tem_saida(void)
{
    COMECA("primeiro uso · 20 s sem servidor viram tentar de novo ou pular");
    const hal_t *hal;
    liga_virgem(&hal);
    pc_botao(IN_OK);   // boas-vindas
    app_passo(&ap);
    ESPERA_IGUAL(uso_configurar_dispositivo(hal, &ap.estado,
                                            INICIO_CMD_SALVAR_NOME,
                                            "Usuário"), OK);
    ap.estado.inicio.fase = INICIO_CONTA;
    ap.estado.tem_token   = false;
    pc_tick(); app_passo(&ap);           // a espera começa

    vista_inicializacao_t v;
    for (int i = 0; i < 21; i++) { pc_avanca_ms(1000); pc_tick(); app_passo(&ap); }
    vista_inicializacao(&ap.estado, &v);
    ESPERA_CONTEM(v.titulo, "Sem resposta");
    ESPERA_CONTEM(v.rodape_dir, "tentar");
    ESPERA_CONTEM(v.rodape_esq, "pular");

    pc_botao(IN_OK);                      // tenta de novo: volta a esperar
    app_passo(&ap);
    vista_inicializacao(&ap.estado, &v);
    ESPERA_CONTEM(v.titulo, "Conectando");

    for (int i = 0; i < 21; i++) { pc_avanca_ms(1000); pc_tick(); app_passo(&ap); }
    pc_botao(IN_DIR);                     // e pular segue em frente
    app_passo(&ap);
    ESPERA(ap.estado.inicio.fase != INICIO_CONTA);
    TERMINA();
}

// No primeiro uso o Wi-Fi pede a hora.
void t_wifi_no_primeiro_uso_pede_a_hora(void)
{
    COMECA("primeiro uso · o Wi-Fi sobe e o aparelho pede a hora da rede");
    const hal_t *hal;
    liga_virgem(&hal);
    pc_botao(IN_OK);   // boas-vindas
    app_passo(&ap);
    ESPERA_IGUAL(uso_configurar_dispositivo(hal, &ap.estado,
                                            INICIO_CMD_SALVAR_NOME,
                                            "Usuário"), OK);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_WIFI);
    pc_botao(IN_OK);                      // OK escolher rede
    app_passo(&ap);

    int antes = pc_ntp_pedidos();
    pc_empurra((evento_t){ .tipo = EV_WIFI_ESTADO });   // o rádio subiu
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_CONTA);
    ESPERA(pc_ntp_pedidos() > antes);
    TERMINA();
}

// O primeiro uso se reapresenta sozinho depois de um 401.
void t_primeiro_uso_se_reapresenta_sozinho(void)
{
    COMECA("primeiro uso · sem token, o tick reapresenta o aparelho");
    const hal_t *hal;
    liga_virgem(&hal);
    pc_botao(IN_OK);   // boas-vindas
    app_passo(&ap);
    ESPERA_IGUAL(uso_configurar_dispositivo(hal, &ap.estado,
                                            INICIO_CMD_SALVAR_NOME,
                                            "Usuário"), OK);
    ap.estado.inicio.fase    = INICIO_CONTA;
    ap.estado.rede           = REDE_LIGADA;
    ap.estado.hora_confiavel = true;
    ap.estado.tem_token      = false;
    ap.estado.registro_ms    = 0;
    pc_nuvem_rota_zera();

    pc_avanca_ms(31000);
    pc_tick();
    app_passo(&ap);
    ESPERA_CONTEM(pc_nuvem_rota(), "/v1/registrar");
    TERMINA();
}

// O fluxo de ponta a ponta: boas-vindas > nome > proprietário > Wi-Fi >
// conectando > servidor (OK) > conta (OK) > QR > conclusão > sistema.
void t_fluxo_do_primeiro_uso_de_ponta_a_ponta(void)
{
    COMECA("primeiro uso · o fluxo inteiro, tela por tela, até o sistema");
    const hal_t *hal;
    liga_virgem(&hal);
    vista_inicializacao_t v;

    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_BOAS_VINDAS);
    pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_NOME);
    snprintf(ap.estado.inicio.nome_pendente, sizeof ap.estado.inicio.nome_pendente,
             "%s", "Usuário");
    ap.estado.inicio.fase = INICIO_CONFIRMAR_DONO;      // o teclado já testado
    pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_WIFI);

    // Wi-Fi: OK abre a Conexão; a etapa segue sozinha.
    pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_WIFI);
    ap.estado.tem_token = false;
    pc_empurra((evento_t){ .tipo = EV_WIFI_ESTADO }); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_CONTA);

    // Conectando: nenhum botão anda.
    vista_inicializacao(&ap.estado, &v);
    ESPERA_CONTEM(v.titulo, "Conectando");
    pc_botao(IN_OK); app_passo(&ap);
    pc_botao(IN_DIR); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_CONTA);

    // O servidor respondeu: só o OK segue.
    ap.estado.tem_token = true;
    vista_inicializacao(&ap.estado, &v);
    ESPERA_CONTEM(v.titulo, "Conectado ao servidor");
    ESPERA_CONTEM(v.rodape_dir, "OK");
    pc_botao(IN_DIR); app_passo(&ap);                   // ▶ não pula daqui
    vista_inicializacao(&ap.estado, &v);
    ESPERA_CONTEM(v.titulo, "Conectado ao servidor");
    pc_botao(IN_OK); app_passo(&ap);

    vista_inicializacao(&ap.estado, &v);
    ESPERA_CONTEM(v.titulo, "Conecte a sua conta");
    ESPERA_CONTEM(v.rodape_esq, "pular");

    // OK: o QR.
    pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_VINCULAR);

    // Vinculou: volta à conclusão.
    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", "usuario@exemplo.com");
    ap.estado.hora_confiavel = true;
    pc_empurra((evento_t){ .tipo = EV_REDE_RESULTADO }); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_CONCLUSAO);
    vista_inicializacao(&ap.estado, &v);
    ESPERA_CONTEM(v.titulo, "Tudo pronto");
    ESPERA_CONTEM(v.estados[2], "vinculada");

    pc_botao(IN_OK); app_passo(&ap);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_HOME);
    TERMINA();
}

// Reiniciar religa a rede e não pede a hora a quem já está configurado.
void t_reiniciar_religa_a_rede_e_nao_pede_a_hora(void)
{
    COMECA("reiniciar · a rede salva volta sozinha, e a data e hora não volta");
    const hal_t *hal;
    liga_virgem(&hal);
    ESPERA_IGUAL(uso_configurar_dispositivo(hal, &ap.estado,
                                            INICIO_CMD_SALVAR_NOME,
                                            "Usuário"), OK);
    rede_salva_t casa = {0};
    snprintf(casa.nome, sizeof casa.nome, "%s", "Casa");
    snprintf(casa.senha, sizeof casa.senha, "%s", "segredo");
    ESPERA_IGUAL(rede_grava(hal, &casa), OK);

    // No meio do primeiro uso: religa a rede mesmo assim.
    app_liga(&ap, hal);
    ESPERA_IGUAL(ap.estado.rede, REDE_CONECTANDO);
    ESPERA_TEXTO(ap.estado.wifi_salva, "Casa");

    // Configurado: liga direto no sistema.
    ESPERA_IGUAL(uso_configurar_dispositivo(hal, &ap.estado,
                                            INICIO_CMD_CONCLUIR, NULL), OK);
    app_liga(&ap, hal);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_HOME);
    ESPERA_IGUAL(ap.estado.rede, REDE_CONECTANDO);
    TERMINA();
}
