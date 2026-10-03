// firmware/testes/t_wifi.c — Conexão e a lista de redes (T-28a).
// O nome da rede nunca se digita (vem da lista); sem rede o aparelho
// funciona inteiro, e a tela diz isso.
#include "teste.h"
#include "vista/menu.h"
#include "vista/wifi.h"
#include "vista/conexao.h"
#include "vista/campos.h"
#include "vista/confirma.h"
#include "vista/teclado.h"
#include "dado/rede.h"
#include "tela/texto.h"
#include "ui/grid.h"
#include "ui/blocos.h"
#include "ui/menu.h"

static estado_t e;
static app_t    ap;

static void poe_rede(const char *nome, int forca, bool salva)
{
    rede_wifi_t *r = &e.redes[e.n_redes++];
    memset(r, 0, sizeof *r);
    snprintf(r->nome, sizeof r->nome, "%s", nome);
    r->forca = (int8_t)forca;
    r->salva = salva;
}

// O índice de uma linha pelo texto; -1 quando ela sumiu (o que pode ser o
// esperado).
static int linha_de(const vista_menu_t *v, const char *texto)
{
    for (int i = 0; i < v->n; i++)
        if (strcmp(v->linhas[i].texto, texto) == 0) return i;
    return -1;
}

// A navegação até o Wi-Fi num lugar só.
static const hal_t *hal;

// A tela de ESTADO do Wi-Fi, com uma rede conectada.
static void liga_no_wifi_estado(void);

static void liga_no_wifi(void)
{
    hal = pc_liga();
    pc_relogio((data_t){2026, 8, 12}, 9, 19);
    pc_redes("casa-2g", 82, "VIVO-A24C", 54);
    app_liga(&ap, hal);
    app_passo(&ap);

    ENTRA_NOS_AJUSTES(&ap);
    DESCE_ATE(&ap, vista_ajustes, "Conexão");
    pc_botao(IN_OK);            // Wi-Fi
    app_passo(&ap);

    // A LISTA é outra tela: o card de Conexão fica parado.
    vista_cartao_t c;
    vista_conexao(&ap.estado, &c);
    for (int i = 0; i < c.n_dest; i++)
        if (strstr(c.dest[i].titulo, "Procurar")) ap.estado.cursor = i;
    pc_botao(IN_OK);
    app_passo(&ap);
    app_passo(&ap);
}

static void liga_no_wifi_estado(void)
{
    liga_no_wifi();
    ap.estado.wifi_lista = false;
    ap.estado.rede = REDE_LIGADA;
    snprintf(ap.estado.wifi_atual, sizeof ap.estado.wifi_atual, "%s", "casa-2g");
    snprintf(ap.estado.wifi_salva, sizeof ap.estado.wifi_salva, "%s", "casa-2g");
    snprintf(ap.estado.wifi_ip,    sizeof ap.estado.wifi_ip,    "%s", "192.168.0.31");
}


void t_a_lista_de_redes_diz_forca_e_quais_estao_salvas(void)
{
    COMECA("T-28a · a lista diz a força de cada rede, e qual está salva");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 8, 12};
    e.hora = 9; e.minuto = 19;
    poe_rede("casa",         82, true);
    poe_rede("VIVO-A24C",    54, false);
    poe_rede("NET_2G_9BC1",  18, false);
    e.wifi_lista = true;        // a LISTA é outra tela desde 25/08

    vista_menu_t v;
    vista_wifi(&e, 12, &v);

    ESPERA_TEXTO(v.titulo, "Redes");
    // Força e "salva" na LEGENDA (na coluna do valor, cortavam o nome).
    ESPERA_TEXTO(v.sub[linha_de(&v, "casa")],        "forte · salva");
    ESPERA_TEXTO(v.sub[linha_de(&v, "VIVO-A24C")],   "média");
    ESPERA_TEXTO(v.sub[linha_de(&v, "NET_2G_9BC1")], "fraca");

    // As duas portas do fim; "Rede oculta" é a única que digita nome.
    ESPERA_TEXTO(v.sub[linha_de(&v, "Rede oculta")], "Digitar o nome");
    // A nota diz o que FAZER.
    ESPERA_CONTEM(v.nota, "Selecione uma rede");
    ESPERA_CONTEM(v.nota, "servidor");

    TERMINA();
}

// Procurando não é lista vazia: diz que procura.
void t_procurando_diz_que_esta_procurando(void)
{
    COMECA("T-28a · procurando, a tela diz isso em vez de ficar vazia");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 8, 12};
    e.wifi_procurando = true;
    e.wifi_lista      = true;

    vista_menu_t v;
    vista_wifi(&e, 12, &v);

// Sem rede ainda, a manchete é a espera, com os pontinhos.
    ESPERA_CONTEM(v.vazio, "Procurando");
    ESPERA(v.pontos >= 0);

    // Com redes e varrendo, o rótulo da seção diz que continua.
    poe_rede("casa", 80, false);
    vista_wifi(&e, 12, &v);
    ESPERA_TEXTO(v.secao[0], "REDES · PROCURANDO");

    TERMINA();
}

// Entrar na tela MANDA procurar, e o resultado chega ao estado.
void t_entrar_no_wifi_manda_procurar(void)
{
    COMECA("T-28a · entrar em Wi-Fi dispara a varredura, e ela chega");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 12}, 9, 19);
    pc_redes("casa", 82, "VIVO-A24C", 54);
    app_liga(&ap, hal);
    app_passo(&ap);

    ENTRA_NOS_AJUSTES(&ap);
    DESCE_ATE(&ap, vista_ajustes, "Conexão");
    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_WIFI);

    app_passo(&ap);   // o rádio responde no passo seguinte
    ESPERA_IGUAL(ap.estado.n_redes, 2);
    ESPERA_TEXTO(ap.estado.redes[0].nome, "casa");
    ESPERA(!ap.estado.wifi_procurando);

    TERMINA();
}

// Rede nova pede a senha, no teclado; a salva conecta direto.
void t_escolher_uma_rede_nova_pede_a_senha(void)
{
    COMECA("T-28a · rede nova pede senha, com o nome dela no título");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 21}, 9, 19);
    pc_redes("casa", 82, "VIVO-A24C", 54);
    app_liga(&ap, hal);
    app_passo(&ap);

    ENTRA_NOS_AJUSTES(&ap);
    DESCE_ATE(&ap, vista_ajustes, "Conexão");
    pc_botao(IN_OK);
    app_passo(&ap);

    // "Procurar redes" é destino do card de Conexão.
    vista_cartao_t vc;
    vista_conexao(&ap.estado, &vc);
    for (int i = 0; i < vc.n_dest; i++)
        if (strstr(vc.dest[i].titulo, "Procurar")) ap.estado.cursor = i;
    pc_botao(IN_OK);
    app_passo(&ap);
    app_passo(&ap);                                   // a varredura responde

    pc_botao(IN_OK);                                  // a primeira rede
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_TECLADO);
    ESPERA_TEXTO(ap.estado.wifi_alvo, "casa");

    // O nome da rede vai no KICKER (na barra saía cortado).
    vista_teclado_t v;
    vista_teclado(&ap.estado, &v);
    ESPERA_CONTEM(v.kicker, "casa");

    TERMINA();
}

// Rede oculta: nome e depois senha, dois teclados em sequência.
void t_rede_oculta_pede_o_nome_e_depois_a_senha(void)
{
    COMECA("T-28a · rede oculta pede o nome, e só então a senha");

    memset(&ap, 0, sizeof ap);
    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 21}, 9, 19);
    pc_redes(NULL, 0, NULL, 0);
    app_liga(&ap, hal);
    app_passo(&ap);

    ap.estado.pilha[++ap.estado.profundidade] = TELA_WIFI;
    ap.estado.wifi_lista = true;      // "rede oculta" mora na LISTA

    vista_menu_t vw;
    vista_wifi(&ap.estado, 8, &vw);
    ap.estado.cursor = linha_de(&vw, "Rede oculta");

    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_TECLADO);
    ESPERA_IGUAL(ap.estado.teclado_contexto, TECLADO_WIFI_SSID);

    snprintf(ap.estado.digitando, sizeof ap.estado.digitando, "%s", "ap");
    ap.estado.teclado_lin = TEC_LINS - 1;
    ap.estado.teclado_col = 4;   // a tecla "ok"
    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA_TEXTO(ap.estado.wifi_alvo, "ap");
    ESPERA_IGUAL(ap.estado.teclado_contexto, TECLADO_WIFI);
    ESPERA_TEXTO(ap.estado.digitando, "");

    TERMINA();
}

// ── rede aberta: sem senha, sem teclado ─────────────────────────────
void t_rede_aberta_conecta_sem_pedir_senha(void)
{
    COMECA("T-28a · rede aberta conecta direto, sem passar pelo teclado");

    liga_no_wifi();

    pc_redes(NULL, 0, NULL, 0);
    pc_rede_aberta("cafe-livre", 70);

    vista_menu_t v;
    vista_wifi(&ap.estado, 8, &v);
    ap.estado.cursor = linha_de(&v, "Procurar de novo");
    pc_botao(IN_OK);
    app_passo(&ap);
    app_passo(&ap);                  // o rádio responde no passo seguinte

    vista_wifi(&ap.estado, 8, &v);

    // A lista diz que ela é aberta.
    int i = linha_de(&v, "cafe-livre");
    ESPERA(i >= 0);
    ESPERA(strstr(v.sub[i], "aberta") != NULL);

    ap.estado.cursor = i;
    pc_botao(IN_OK);
    app_passo(&ap);

    // Conectou, e o teclado nunca apareceu.
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_WIFI);
    ESPERA_IGUAL(ap.estado.rede, REDE_CONECTANDO);
    ESPERA_TEXTO(ap.estado.wifi_salva, "cafe-livre");
    TERMINA();
}

// ── esquecer ────────────────────────────────────────────────────────
// A única linha que desfaz, por isso a última.
void t_esquecer_a_rede_apaga_a_senha_e_desconecta(void)
{
    COMECA("T-28a · esquecer apaga a senha do cartão e desconecta");

    liga_no_wifi();

    rede_salva_t nova;
    memset(&nova, 0, sizeof nova);
    snprintf(nova.nome, sizeof nova.nome, "%s", "casa-2g");
    ESPERA_IGUAL(rede_grava(hal, &nova), OK);
    snprintf(ap.estado.wifi_salva, sizeof ap.estado.wifi_salva, "%s", "casa-2g");
    ap.estado.wifi_lista = false;   // esquecer mora na tela de ESTADO

    vista_cartao_t v;
    vista_conexao(&ap.estado, &v);
    int i = -1;
    for (int k = 0; k < v.n_dest; k++)
        if (strstr(v.dest[k].titulo, "Esquecer")) i = k;
    ESPERA(i >= 0);

    // Esquecer pergunta antes: o primeiro OK abre a confirmação.
    ap.estado.cursor = i;
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_ESQUECER);

    ap.estado.cursor_overlay = 1;
    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA_TEXTO(ap.estado.wifi_salva, "");
    ESPERA_IGUAL(ap.estado.rede, REDE_DESLIGADA);

    // O cartão não tem mais senha.
    rede_salva_t lida;
    ESPERA(rede_carrega(hal, &lida) != OK);

    // Sem rede salva, a linha some.
    vista_conexao(&ap.estado, &v);
    for (int k = 0; k < v.n_dest; k++)
        ESPERA(strstr(v.dest[k].titulo, "Esquecer") == NULL);
    TERMINA();
}

// ── o HUD mostra que o aparelho espera ──────────────────────────────
// Varrer leva segundos: sem indicador a pessoa aperta de novo. Uma forma,
// sem girar, à esquerda de hora e bateria.
void t_o_hud_diz_quando_o_aparelho_esta_esperando(void)
{
    COMECA("o HUD mostra a forma do ciclo enquanto o aparelho espera");

    liga_no_wifi();

    ap.estado.wifi_procurando = true;
    vista_menu_t v;
    vista_wifi(&ap.estado, 8, &v);
    ESPERA_IGUAL(v.sinc, ICO_SINCRONIZA);

    ap.estado.wifi_procurando = false;
    ap.estado.rede = REDE_CONECTANDO;      // conectar também é esperar
    vista_wifi(&ap.estado, 8, &v);
    ESPERA_IGUAL(v.sinc, ICO_SINCRONIZA);

    // A sincronização tem forma própria.
    ap.estado.rede = REDE_LIGADA;
    ap.estado.sinc = SINC_RECEBENDO;
    vista_wifi(&ap.estado, 8, &v);
    ESPERA_IGUAL(v.sinc, ICO_DESCENDO);

    // Parado, ele some.
    ap.estado.sinc = SINC_OCIOSO;
    vista_wifi(&ap.estado, 8, &v);
    ESPERA_IGUAL(v.sinc, ICO_NENHUM);
    TERMINA();
}

// ── o cursor pula o que só informa ──────────────────────────────────
void t_o_cursor_pula_as_linhas_que_so_informam(void)
{
    COMECA("Conexão · o cursor começa na ação, e não no card");

    liga_no_wifi_estado();

    // No card não há linha informativa a pular: a informação mora dentro dele.
    vista_cartao_t v;
    vista_conexao(&ap.estado, &v);

    ESPERA(v.n_dest >= 1);
    ESPERA(v.n_fatos >= 1);
    ESPERA(!v.tem_sair);

    // Descer e subir não tiram o cursor das ações.
    ap.estado.cursor = 0;
    pc_botao(IN_BAIXO);
    app_passo(&ap);
    ESPERA(ap.estado.cursor >= 0 && ap.estado.cursor < v.n_dest);

    pc_botao(IN_CIMA);
    app_passo(&ap);
    ESPERA(ap.estado.cursor >= 0 && ap.estado.cursor < v.n_dest);
    TERMINA();
}

// "Esquecer esta rede" cabe inteiro: a rede está no card, e a legenda diz
// o custo.
void t_esquecer_cabe_inteiro_na_linha(void)
{
    COMECA("a linha de esquecer cabe inteira, sem reticências");

    liga_no_wifi_estado();

    vista_cartao_t v;
    vista_conexao(&ap.estado, &v);
    int i = -1;
    for (int k = 0; k < v.n_dest; k++)
        if (strstr(v.dest[k].titulo, "Esquecer")) i = k;
    ESPERA(i >= 0);

    int larg = GRID_UTIL - GRID_DESTINO_COL - GRID_DESTINO_PAD * 2;
    ESPERA(gfx_largura(F_CORPO, v.dest[i].titulo) <= larg);
    ESPERA(gfx_largura(F_MIUDA, v.dest[i].sub) <= larg);
    ESPERA_TEXTO(v.dest[i].valor, "");
    TERMINA();
}

// Conexão conectada: a rede é INFORMAÇÃO no card; procurar e esquecer são
// ações abaixo.
void t_conexao_conectada_mostra_o_card_e_as_acoes(void)
{
    COMECA("Conexão · a rede atual informa, e as ações ficam abaixo");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 9, 2};
    e.hora = 11; e.minuto = 17;
    e.rede = REDE_LIGADA;
    e.wifi_forca = 82;
    snprintf(e.wifi_atual, sizeof e.wifi_atual, "%s", "Casa");
    snprintf(e.wifi_salva, sizeof e.wifi_salva, "%s", "Casa");
    snprintf(e.wifi_ip,    sizeof e.wifi_ip,    "%s", "192.168.1.42");

    vista_cartao_t v;
    vista_conexao(&e, &v);

    ESPERA_TEXTO(v.kicker, "rede atual");
    ESPERA_TEXTO(v.nome, "Casa");

    // Três fatos; o endereço é a prova.
    ESPERA_IGUAL(v.n_fatos, 3);
    ESPERA_TEXTO(v.fatos[0].rotulo, "Estado");
    ESPERA_TEXTO(v.fatos[0].valor,  "conectada");
    ESPERA_TEXTO(v.fatos[1].rotulo, "Sinal");
    ESPERA_TEXTO(v.fatos[1].valor,  "forte");
    ESPERA_TEXTO(v.fatos[2].rotulo, "Endereço");
    ESPERA_TEXTO(v.fatos[2].valor,  "192.168.1.42");

    // Duas ações, e esquecer diz o custo antes.
    ESPERA_IGUAL(v.n_dest, 2);
    ESPERA_CONTEM(v.dest[0].titulo, "Procurar redes");
    ESPERA_CONTEM(v.dest[1].titulo, "Esquecer esta rede");
    ESPERA_CONTEM(v.dest[1].sub, "senha");

    // O card não recebe foco.
    ESPERA(!v.tem_sair);

    TERMINA();
}

// Sem rede salva não há o que esquecer.
void t_conexao_sem_rede_salva_nao_oferece_esquecer(void)
{
    COMECA("Conexão · sem rede salva, não há o que esquecer");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 9, 2};

    vista_cartao_t v;
    vista_conexao(&e, &v);

    ESPERA_TEXTO(v.kicker, "sem rede");
    ESPERA_IGUAL(v.n_dest, 1);
    ESPERA_CONTEM(v.dest[0].titulo, "Procurar redes");

    TERMINA();
}

// A lista de redes: o nome com a linha inteira, o resto na legenda.
void t_a_lista_de_redes_poe_o_que_se_sabe_na_legenda(void)
{
    COMECA("Redes · o nome fica inteiro, e o resto vai para a legenda");

    liga_no_wifi();

    vista_menu_t v;
    vista_wifi(&ap.estado, 12, &v);

    int i = linha_de(&v, "casa-2g");
    ESPERA(i >= 0);
    ESPERA_TEXTO(v.linhas[i].valor, "");
    ESPERA_CONTEM(v.sub[i], "forte");

    // As duas ações do fim.
    ESPERA(linha_de(&v, "Procurar de novo") >= 0);
    int oculta = linha_de(&v, "Rede oculta");
    ESPERA(oculta >= 0);
    ESPERA_CONTEM(v.sub[oculta], "nome");

    // O rodapé diz de onde se veio.
    ESPERA_TEXTO(v.rodape_esq, "BACK conexão");

    TERMINA();
}

// Enquanto varre, os pontinhos andam no rótulo da seção.
void t_a_busca_de_redes_anima_os_pontinhos(void)
{
    COMECA("Redes · procurando, os pontinhos andam no rótulo");

    liga_no_wifi();
    ap.estado.wifi_procurando = true;

    vista_menu_t v;
    int visto[4] = {0};
    for (int s = 0; s < 4; s++) {
        ap.estado.agora_ms = (uint32_t)s * 1000u;
        vista_wifi(&ap.estado, 12, &v);
        ESPERA(v.pontos >= 0 && v.pontos <= 3);
        visto[v.pontos] = 1;
    }
    ESPERA(visto[0] && visto[1] && visto[2] && visto[3]);

    // Terminada a varredura, nada anima.
    ap.estado.wifi_procurando = false;
    vista_wifi(&ap.estado, 12, &v);
    ESPERA_IGUAL(v.pontos, -1);

    TERMINA();
}

// Senha recusada e sem resposta são telas diferentes: uma se resolve
// digitando, a outra não.
void t_senha_recusada_e_sem_resposta_sao_telas_diferentes(void)
{
    COMECA("Conexão · senha recusada e rede muda são erros diferentes");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 9, 2};
    snprintf(e.wifi_alvo, sizeof e.wifi_alvo, "%s", "Escritório");

    // ── a senha ──
    e.rede = REDE_DESLIGADA;
    e.wifi_falha = WIFI_FALHA_SENHA;

    vista_cartao_t v;
    vista_conexao(&e, &v);

    ESPERA_CONTEM(v.kicker, "senha");
    ESPERA_TEXTO(v.nome, "Escritório");
    ESPERA(v.n_dest >= 1);
    ESPERA_CONTEM(v.dest[0].titulo, "Digitar");

    // ── a rede que não responde ──
    e.wifi_falha = WIFI_FALHA_SEM_RESPOSTA;
    vista_conexao(&e, &v);

    // Sem oferecer digitar de novo.
    ESPERA(v.dest[0].titulo[0]);
    ESPERA(strstr(v.dest[0].titulo, "Digitar") == NULL);
    ESPERA_CONTEM(v.corpo, "respondeu");

    TERMINA();
}

// Senha recusada PARA de tentar sozinha (senão a tela piscava entre
// conectando e recusada).
void t_senha_recusada_nao_fica_tentando_sozinha(void)
{
    COMECA("Conexão · senha recusada para de tentar, e espera a pessoa");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 9, 2};
    snprintf(e.wifi_alvo, sizeof e.wifi_alvo, "%s", "Escritório");
    e.rede = REDE_DESLIGADA;
    e.wifi_falha = WIFI_FALHA_SENHA;

    vista_cartao_t v;
    vista_conexao(&e, &v);

    // O corpo diz o que aconteceu e o que fazer.
    ESPERA(v.corpo[0]);
    ESPERA(strstr(v.corpo, "aceita") || strstr(v.corpo, "aceitou"));

    TERMINA();
}

// Conectando anima os pontinhos (reticências paradas pareciam travado).
void t_conectando_anima_os_pontinhos(void)
{
    COMECA("Conexão · conectando anima os pontinhos, sem reticências");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 9, 2};
    e.rede = REDE_CONECTANDO;
    snprintf(e.wifi_alvo, sizeof e.wifi_alvo, "%s", "Escritório");

    vista_cartao_t v;
    int visto[4] = {0};
    for (int s = 0; s < 4; s++) {
        e.agora_ms = (uint32_t)s * 1000u;
        vista_conexao(&e, &v);
        ESPERA(v.pontos >= 0 && v.pontos <= 3);
        visto[v.pontos] = 1;
    }
    ESPERA(visto[0] && visto[1] && visto[2] && visto[3]);

    // Sem reticências no texto.
    for (int i = 0; i < v.n_fatos; i++)
        ESPERA(strstr(v.fatos[i].valor, "…") == NULL);

    // Conectada, nada anima.
    e.rede = REDE_LIGADA;
    snprintf(e.wifi_atual, sizeof e.wifi_atual, "%s", "Escritório");
    vista_conexao(&e, &v);
    ESPERA_IGUAL(v.pontos, -1);

    TERMINA();
}

// Esquecer uma rede PERGUNTA antes, e o "sim" repete o nome da rede.
void t_esquecer_rede_pergunta_antes(void)
{
    COMECA("Conexão · esquecer a rede pergunta, e repete o nome dela");

    liga_no_wifi_estado();

    rede_salva_t nova;
    memset(&nova, 0, sizeof nova);
    snprintf(nova.nome, sizeof nova.nome, "%s", "casa-2g");
    ESPERA_IGUAL(rede_grava(hal, &nova), OK);

    vista_cartao_t v;
    vista_conexao(&ap.estado, &v);
    int i = -1;
    for (int k = 0; k < v.n_dest; k++)
        if (strstr(v.dest[k].titulo, "Esquecer")) i = k;
    ESPERA(i >= 0);

    ap.estado.cursor = i;
    pc_botao(IN_OK);
    app_passo(&ap);

    // Nada foi apagado ainda.
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_ESQUECER);
    ESPERA_TEXTO(ap.estado.wifi_salva, "casa-2g");

    // O direcional anda entre as duas opções.
    ap.estado.cursor_overlay = 0;
    pc_botao(IN_BAIXO);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.cursor_overlay, 1);
    pc_botao(IN_CIMA);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.cursor_overlay, 0);

    vista_confirma_t c;
    vista_esquecer_rede(&ap.estado, &c);
    ESPERA_CONTEM(c.pergunta, "casa-2g");
    ESPERA_CONTEM(c.sim, "casa-2g");        // a destrutiva repete o objeto
    ESPERA_CONTEM(c.nao, "manter");
    ESPERA_IGUAL(c.cursor, 0);              // a segura nasce escolhida

    // O "não" mantém tudo.
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_TEXTO(ap.estado.wifi_salva, "casa-2g");
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_NADA);

    // O "sim" apaga.
    ap.estado.cursor = i;
    pc_botao(IN_OK);
    app_passo(&ap);
    ap.estado.cursor_overlay = 1;
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_TEXTO(ap.estado.wifi_salva, "");

    TERMINA();
}

// Lista vazia explica e oferece saída: não achar nada é resultado.
void t_lista_de_redes_vazia_explica_e_oferece_saida(void)
{
    COMECA("Redes · busca sem resultado explica, e oferece o que fazer");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 9, 3};
    e.wifi_lista = true;
    e.n_redes = 0;
    e.wifi_procurando = false;

    vista_menu_t v;
    vista_wifi(&e, 12, &v);

    ESPERA_CONTEM(v.vazio, "Nenhuma rede");

    // As duas ações continuam.
    bool procurar = false, oculta = false;
    for (int i = 0; i < v.n; i++) {
        if (strstr(v.linhas[i].texto, "Procurar")) procurar = true;
        if (strstr(v.linhas[i].texto, "Rede oculta")) oculta = true;
    }
    ESPERA(procurar);
    ESPERA(oculta);

    // A explicação diz o que fazer no MUNDO.
    ESPERA_CONTEM(v.nota, "roteador");

    // A manchete quebra em vez de passar da margem: numa linha só, o vidro
    // mostrava "Nenhuma rede encon".
    static uint8_t mem[TELA_L / 8 * TELA_A];
    bitmap_t bm;
    bitmap_liga(&bm, mem, TELA_L, TELA_A);
    tela_menu(&bm, &v);
    bool na_margem = false;
    const int topo = BARRA_A + 8;   // onde a manchete começa
    for (int y = topo; y < topo + 2 * gfx_altura_linha(F_EDITORIAL); y++)
        for (int x = TELA_L - GRID_MARGEM + 2; x < TELA_L; x++)
            if (gfx_le(&bm, x, y)) na_margem = true;
    ESPERA(!na_margem);

    TERMINA();
}

// A tela de Senha: kicker com a rede e contador de caracteres.
void t_a_tela_de_senha_diz_a_rede_e_conta_os_caracteres(void)
{
    COMECA("Senha · de qual rede é, e quantos caracteres já foram");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 9, 3};
    e.teclado_contexto = TECLADO_WIFI;
    snprintf(e.wifi_alvo, sizeof e.wifi_alvo, "%s", "Escritório");
    snprintf(e.digitando, sizeof e.digitando, "%s", "abc123");

    vista_teclado_t v;
    vista_teclado(&e, &v);

    // De qual rede.
    ESPERA_CONTEM(v.kicker, "Escritório");
    ESPERA_CONTEM(v.kicker, "protegida");

    // Quantos caracteres.
    ESPERA_CONTEM(v.contagem, "6");

    // A senha aparece mascarada.
    ESPERA(strstr(v.texto, "abc") == NULL);

    TERMINA();
}

// O sinal não congela desde a conexão: o evento do rádio só vem quando o
// estado muda; o tick relê. A barra não mostra nível; esta tela mostra.
void t_o_sinal_do_wifi_nao_congela_na_conexao(void)
{
    static app_t ap;
    COMECA("o sinal é relido enquanto conectado, e não só ao conectar");

    const hal_t *hal = pc_liga();
    app_liga(&ap, hal);
    app_passo(&ap);

    ap.estado.rede = REDE_LIGADA;
    ap.estado.wifi_forca = 90;

    // A pessoa anda: sinal fraco.
    pc_wifi_forca(20);

    // O tick relê.
    for (int i = 0; i < 3; i++) {
        pc_avanca_ms(11u * 1000u);
        pc_tick();
        app_passo(&ap);
    }

    ESPERA_IGUAL(ap.estado.wifi_forca, 20);
    ESPERA_TEXTO(vista_forca_texto(ap.estado.wifi_forca), "fraca");

    TERMINA();
}

// E é a PALAVRA que decide o redesenho, não cada oscilação.
void t_a_oscilacao_do_sinal_nao_gasta_tinta(void)
{
    static app_t ap;
    COMECA("sinal que oscila dentro da mesma palavra não redesenha");

    const hal_t *hal = pc_liga();
    app_liga(&ap, hal);
    app_passo(&ap);

    ap.estado.rede = REDE_LIGADA;
    pc_wifi_forca(90);
    pc_avanca_ms(11u * 1000u);
    pc_tick();
    app_passo(&ap);

    // O observável é o QUADRO, não a bandeira.
    int quadros = pc_quadros();

    // 90 → 71: a palavra não muda.
    pc_wifi_forca(71);
    pc_avanca_ms(11u * 1000u);
    pc_tick();
    app_passo(&ap);
    ESPERA_IGUAL(pc_quadros(), quadros);
    ESPERA_IGUAL(ap.estado.wifi_forca, 71);   // o valor acompanha mesmo assim

    // 71 → 40: "forte" vira "média".
    pc_wifi_forca(40);
    pc_avanca_ms(11u * 1000u);
    pc_tick();
    app_passo(&ap);
    ESPERA(pc_quadros() > quadros);

    TERMINA();
}
