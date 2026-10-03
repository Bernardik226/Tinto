// firmware/testes/t_armazenamento.c — Armazenamento e Sobre.
// Cartão cheio é aviso PASSIVO: nada é apagado sozinho.
#include "teste.h"
#include "vista/cartao.h"
#include "vista/armazenamento.h"
#include "vista/sobre.h"
#include "vista/inicializacao.h"
#include "vista/confirma.h"
#include "vista/menu.h"
#include "vista/armazenamento.h"

static estado_t e;

static void com(uint32_t usado_kb, uint32_t total_kb,
                uint32_t itens_kb, uint32_t acervo_kb, uint32_t sistema_kb)
{
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 8, 21};
    e.hora = 9; e.minuto = 14;
    e.espaco_usado_kb = usado_kb;
    e.espaco_total_kb = total_kb;
    e.uso.itens_kb   = itens_kb;
    e.uso.acervo_kb  = acervo_kb;
    e.uso.sistema_kb = sistema_kb;
}

void t_o_armazenamento_diz_o_que_ocupa(void)
{
    COMECA("Armazenamento · o que ocupa, não só quanto sobra");

    com(2202009u, 7561376u, 2100000u, 100000u, 2009u);

    vista_cartao_t v;
    vista_armazenamento(&e, &v);

    ESPERA_TEXTO(v.titulo, "Armazenamento");
    ESPERA_CONTEM(v.nome, "5,1 GB");

    // O que ocupa, nas linhas passivas do card.
    bool notas = false, sistema = false;
    for (int i = 0; i < v.n_info; i++) {
        if (strstr(v.info[i].rotulo, "Notas"))   notas = strstr(v.info[i].valor, "2,0 GB") != NULL;
        if (strstr(v.info[i].rotulo, "Sistema")) sistema = v.info[i].valor[0] != 0;
    }
    ESPERA(notas);
    ESPERA(sistema);

    TERMINA();
}

// O aviso aparece quando ainda dá tempo, e diz que nada some sozinho.
void t_cartao_quase_cheio_avisa_sem_apagar_nada(void)
{
    COMECA("Armazenamento · cheio avisa, e promete não apagar nada");

    com(7300000u, 7561376u, 7000000u, 0u, 2009u);

    vista_cartao_t v;
    vista_armazenamento(&e, &v);

    ESPERA_CONTEM(v.corpo, "Nada é apagado");

    TERMINA();
}

// Sem medida não inventa: "0 de 0" parece cartão cheio.
void t_armazenamento_sem_medida_nao_inventa(void)
{
    COMECA("Armazenamento · sem medida, a tela não inventa número");

    com(0, 0, 0, 0, 0);

    vista_cartao_t v;
    vista_armazenamento(&e, &v);

    // O card diz que não conseguiu medir.
    ESPERA_CONTEM(v.nome, "medir");

    TERMINA();
}

// Entrar na tela manda MEDIR.
void t_entrar_no_armazenamento_mede_o_cartao(void)
{
    COMECA("Armazenamento · entrar mede o cartão, e a medida chega");

    static app_t ap;
    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 21}, 9, 14);
    app_liga(&ap, hal);
    app_passo(&ap);

    ENTRA_NOS_AJUSTES(&ap);
    DESCE_ATE(&ap, vista_ajustes, "Sobre o Tinto");
    pc_botao(IN_OK);
    app_passo(&ap);                       // o hub
    ap.estado.cursor = 0;                 // Armazenamento, o primeiro
    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_ARMAZENAMENTO);
    ESPERA(ap.estado.espaco_total_kb > 0);

    TERMINA();
}

// ── recomeçar de propósito ──────────────────────────────────────────
// O caminho destrutivo só era alcançável quando o cartão quebrava.
static void entra_no_armazenamento(app_t *ap)
{
    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 21}, 9, 14);
    app_liga(ap, hal);
    app_passo(ap);

    ENTRA_NOS_AJUSTES(ap);
    DESCE_ATE(ap, vista_ajustes, "Sobre o Tinto");
    pc_botao(IN_OK);
    app_passo(ap);                        // o hub
    ap->estado.cursor = 0;                // Armazenamento, o primeiro
    pc_botao(IN_OK);
    app_passo(ap);

    // O cursor no Restaurar (o Sobre é o primeiro destino).
    ap->estado.cursor = 1;
}

void t_apagar_pergunta_antes_e_nasce_no_nao(void)
{
    COMECA("Armazenamento · apagar abre a pergunta, e ela nasce no 'não'");

    static app_t ap;
    entra_no_armazenamento(&ap);

    pc_botao(IN_OK);                 // "Apagar e preparar"
    app_passo(&ap);

    // RN-6C: escolher formatar abre a pergunta, com o cursor no "não".
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_CONFIRMAR_FORMATAR);
    ESPERA_IGUAL(ap.estado.inicio.cursor, 0);
    ESPERA(!ap.estado.inicio.formatar_pendente);

    TERMINA();
}

void t_desistir_de_apagar_volta_pra_vida_normal(void)
{
    COMECA("Armazenamento · desistir não cai numa tela de cartão quebrado");

    static app_t ap;
    entra_no_armazenamento(&ap);

    pc_botao(IN_OK);                 // abre a pergunta
    app_passo(&ap);
    pc_botao(IN_VOLTAR);             // e desiste
    app_passo(&ap);

    // Desistir não cai na tela de cartão danificado.
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_HOME);
    ESPERA(!ap.estado.inicio.formatar_pendente);

    TERMINA();
}

void t_so_o_sim_deliberado_apaga(void)
{
    COMECA("Armazenamento · só o 'sim' movido de propósito apaga");

    static app_t ap;
    entra_no_armazenamento(&ap);

    pc_botao(IN_OK);
    app_passo(&ap);

    // O OK no "não" desiste.
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA(!ap.estado.inicio.formatar_pendente);

    // Desistir volta para Armazenamento.
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_ARMAZENAMENTO);
    ap.estado.cursor = 1;
    pc_botao(IN_OK);                 // abre de novo
    app_passo(&ap);
    pc_botao(IN_BAIXO);              // move para "sim, apagar tudo"
    pc_botao(IN_OK);
    app_passo(&ap);

    // E o aparelho volta ao PRIMEIRO USO.
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_BOAS_VINDAS);

    TERMINA();
}

// ── Armazenamento ───────────────────────────────────────────────────
// O LIVRE é o número grande; só Restaurar recebe seleção.
void t_armazenamento_no_desenho_da_fase_3(void)
{
    COMECA("Armazenamento · o livre é o número, e só Restaurar se aperta");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 9, 3};
    e.espaco_total_kb = 33554432u;          // 32 GB
    e.espaco_usado_kb = 3774873u;           // 3,6 GB
    e.uso.itens_kb  = 430080u;              // 420 MB
    e.uso.acervo_kb = 3145728u;             // 3,0 GB

    vista_cartao_t v;
    vista_armazenamento(&e, &v);

    ESPERA_CONTEM(v.kicker, "memória");
    ESPERA_CONTEM(v.nome, "livres");
    ESPERA_CONTEM(v.corpo, "usados");

    // O que ocupa: passivo, sem cursor.
    ESPERA(v.n_info >= 3);
    ESPERA_CONTEM(v.secao_info, "OCUPA");
    ESPERA_CONTEM(v.info[0].rotulo, "Notas");

    // Uma coisa só a apertar.
    ESPERA_IGUAL(v.n_dest, 1);
    ESPERA_CONTEM(v.dest[0].titulo, "Restaurar");

    // Sem medida não se oferece apagar.
    e.espaco_total_kb = 0;
    vista_armazenamento(&e, &v);
    ESPERA_IGUAL(v.n_dest, 0);
    ESPERA_CONTEM(v.nome, "medir");

    TERMINA();
}

// ── Sobre ───────────────────────────────────────────────────────────
// Não repete bateria, rede nem conta.
void t_sobre_no_desenho_da_fase_3(void)
{
    COMECA("Sobre · produto, documentos e suporte, e nada repetido");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 9, 3};
    e.bateria = 81;
    e.rede = REDE_LIGADA;
    snprintf(e.nome, sizeof e.nome, "%s", "usuario@exemplo.com");

    vista_cartao_t v;
    vista_sobre(&e, &v);

    // A marca: o assunto é o aparelho.
    ESPERA(v.logo);
    ESPERA_TEXTO(v.kicker, "");
    ESPERA_TEXTO(v.nome, "");

    bool tem_versao = false;
    for (int i = 0; i < v.n_fatos; i++)
        if (strstr(v.fatos[i].rotulo, "Versão")) tem_versao = true;
    ESPERA(tem_versao);

    // Nada de bateria, rede ou conta.
    for (int i = 0; i < v.n_fatos; i++) {
        ESPERA(strstr(v.fatos[i].rotulo, "Bateria") == NULL);
        ESPERA(strstr(v.fatos[i].rotulo, "Rede") == NULL);
        ESPERA(strstr(v.fatos[i].rotulo, "Conta") == NULL);
    }

    // Um destino que leva a algum lugar.
    ESPERA_IGUAL(v.n_dest, 1);
    ESPERA_CONTEM(v.dest[0].titulo, "Armazenamento");

    TERMINA();
}

// ── Restaurar: o que sai e o que NÃO sai ────────────────────────────
// A agenda do Google não vai junto. A opção segura nasce selecionada.
void t_restaurar_diz_o_que_sai_e_o_que_fica(void)
{
    COMECA("Restaurar · diz o que sai daqui, e que o Google não é tocado");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 9, 3};
    e.inicio.fase   = INICIO_CONFIRMAR_FORMATAR;
    e.inicio.cursor = 0;

    vista_inicializacao_t v;
    vista_inicializacao(&e, &v);

    ESPERA_CONTEM(v.titulo, "Apagar");

    // Os três grupos que saem.
    ESPERA_CONTEM(v.corpo, "notas");
    ESPERA_CONTEM(v.corpo, "Wi-Fi");

    // E o que não sai.
    ESPERA_CONTEM(v.alerta, "Google");

    // A saída segura nasce escolhida.
    ESPERA_IGUAL(v.selecionada, 0);
    ESPERA_CONTEM(v.opcoes[0], "voltar");

    TERMINA();
}

// ── a tela bloqueante mostra as ETAPAS, sem rodapé ──────────────────
void t_preparando_mostra_as_etapas(void)
{
    COMECA("Preparando · as três etapas, e nenhuma ação falsa");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 9, 3};
    e.inicio.fase = INICIO_PREPARANDO;
    e.inicio.etapa = 1;                    // apagando já passou

    vista_inicializacao_t v;
    vista_inicializacao(&e, &v);

    ESPERA_IGUAL(v.n_etapas, 3);
    ESPERA_CONTEM(v.etapas[0], "Apagando");
    ESPERA_CONTEM(v.etapas[1], "Preparando");
    ESPERA_CONTEM(v.etapas[2], "Reiniciando");
    ESPERA_IGUAL(v.etapa_atual, 1);

    // Nenhum rodapé: nenhum botão faz nada.
    ESPERA_TEXTO(v.rodape_esq, "");
    ESPERA_TEXTO(v.rodape_dir, "");

    TERMINA();
}

// Restaurar usa a confirmação do SISTEMA, como as outras destrutivas.
void t_restaurar_usa_a_confirmacao_do_sistema(void)
{
    COMECA("Restaurar · a mesma confirmação de desconectar e esquecer");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 9, 3};
    e.inicio.fase   = INICIO_CONFIRMAR_FORMATAR;
    e.inicio.cursor = 0;

    vista_confirma_t v;
    vista_restaurar(&e, &v);

    ESPERA_CONTEM(v.pergunta, "Apagar");
    ESPERA_CONTEM(v.explica, "Google");     // o que NÃO sai
    ESPERA_CONTEM(v.nao, "Não");
    ESPERA_CONTEM(v.sim, "restaurar");
    ESPERA_IGUAL(v.cursor, 0);              // a segura nasce escolhida

    // O cursor segue o campo do fluxo de inicialização.
    e.inicio.cursor = 1;
    vista_restaurar(&e, &v);
    ESPERA_IGUAL(v.cursor, 1);

    TERMINA();
}

// O caminho: Ajustes › Sobre › Armazenamento (o Sobre é o hub).
void t_o_sobre_tem_porta_no_armazenamento(void)
{
    COMECA("Sobre · é o hub do aparelho, e leva ao espaço");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 9, 3};

    vista_cartao_t v;
    vista_sobre(&e, &v);

    bool espaco = false, atualizar = false;
    for (int i = 0; i < v.n_dest; i++) {
        if (strstr(v.dest[i].titulo, "Armazenamento")) espaco = true;
        if (strstr(v.dest[i].titulo, "Atualizar"))     atualizar = true;
    }
    ESPERA(espaco);
    ESPERA(!atualizar);     // OTA é da v2: nenhuma porta para o que não existe

    TERMINA();
}

