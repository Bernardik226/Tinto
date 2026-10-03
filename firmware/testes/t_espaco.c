// firmware/testes/t_espaco.c — o espaço do cartão e os ajustes do aparelho.
#include "teste.h"
#include "vista/armazenamento.h"
#include "vista/conta.h"
#include "vista/cartao.h"
#include "vista/menu.h"
#include "ui/blocos.h"
#include "vista/sobre.h"
#include "tela/texto.h"

static estado_t e;
static app_t    ap;

static void com_espaco(uint32_t usado_kb, uint32_t total_kb)
{
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = e.dia_visto = (data_t){2026, 8, 12};
    e.hora = 9; e.minuto = 14;
    e.espaco_usado_kb = usado_kb;
    e.espaco_total_kb = total_kb;
}

// O LIVRE, como o card diz: o número grande.
static const char *livre_de(const vista_cartao_t *v)
{
    return v->nome;
}

void t_armazenamento_sai_em_gb_com_uma_casa(void)
{
    COMECA("Ajustes · a linha diz o que SOBRA, que é o que se pergunta");

    com_espaco(2202009u, 33554432u);   // 2,1 de 32 GB

    // Na tela de Armazenamento, não na raiz de Ajustes.
    vista_cartao_t v;
    vista_armazenamento(&e, &v);

    ESPERA_CONTEM(livre_de(&v), "29,9 GB");

    TERMINA();
}

// Cartão recém-provisionado sai em MB: "0,0 GB" não informa.
void t_cartao_quase_vazio_sai_em_mb(void)
{
    COMECA("Ajustes · cartão vazio mostra o cartão inteiro livre");

    com_espaco(352u, 7561376u);   // o cartão da bancada, recém-provisionado

    vista_cartao_t v;
    vista_armazenamento(&e, &v);

    ESPERA_CONTEM(livre_de(&v), "7,2 GB");

    TERMINA();
}

void t_espaco_desconhecido_nao_vira_zero(void)
{
    COMECA("Ajustes · espaço que não se sabe continua em branco");

    com_espaco(0, 0);

    vista_cartao_t v;
    vista_armazenamento(&e, &v);

    // Sem medida, o card diz que não mediu.
    ESPERA_CONTEM(v.nome, "medir");

    TERMINA();
}

// Quem mede é o hal ao ligar, e o app guarda.
void t_o_aparelho_mede_a_memoria_ao_ligar(void)
{
    COMECA("Ajustes · o aparelho mede a memória sozinho");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 12}, 9, 14);
    app_liga(&ap, hal);
    app_passo(&ap);

    ESPERA(ap.estado.espaco_total_kb > 0);
    ESPERA(ap.estado.espaco_usado_kb <= ap.estado.espaco_total_kb);

    TERMINA();
}

// O rótulo não invade a coluna do valor.
void t_o_rotulo_nao_invade_a_coluna_do_valor(void)
{
    COMECA("Ajustes · o rótulo cede espaço ao valor, e nenhum invade o outro");

    linha_acao_t l;
    memset(&l, 0, sizeof l);
    snprintf(l.texto, sizeof l.texto, "%s", "Armazenamento");
    snprintf(l.valor, sizeof l.valor, "%s", "0 MB de 7 GB");

    static uint8_t bits[(TELA_L + 7) / 8 * TELA_A];
    bitmap_t bm;
    bitmap_liga(&bm, bits, TELA_L, TELA_A);
    memset(bits, 0, sizeof bits);

    int larg = TELA_L - 20;
    bloco_acao_em(&bm, 10, 20, larg, &l, false);

    int fim_do_rotulo = 10 + bloco_acao_largura_do_texto(&l, larg);
    int comeca_o_valor = 10 + larg - gfx_largura(F_MIUDA, l.valor);

    ESPERA(fim_do_rotulo <= comeca_o_valor);

    TERMINA();
}

// O dono mora em Minha Conta, não em Sobre.
void t_o_sobre_diz_de_quem_e_o_aparelho(void)
{
    COMECA("Sobre · o dono saiu daqui e mora em Minha Conta");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 8, 21};
    snprintf(e.inicio.nome_pendente, sizeof e.inicio.nome_pendente,
             "%s", "Usuário");

    vista_cartao_t v;
    vista_sobre(&e, &v);

    for (int i = 0; i < v.n_fatos; i++)
        ESPERA(strcmp(v.fatos[i].rotulo, "De") != 0);

    bool tem_versao = false;
    for (int i = 0; i < v.n_fatos; i++)
        if (strcmp(v.fatos[i].rotulo, "Versão") == 0) tem_versao = true;
    ESPERA(tem_versao);

    TERMINA();
}

// O gesto de falar (PTT ou um toque) alterna na própria linha, em "Câmera e
// voz" (em Minha conta empurrava a tela além do quadro).
void t_o_botao_de_voz_alterna_na_propria_linha(void)
{
    COMECA("Câmera e voz · o botão de voz alterna entre segurar e um toque");

    static app_t ap2;
    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 21}, 9, 14);
    app_liga(&ap2, hal);

    ap2.estado.rede = REDE_LIGADA;
    snprintf(ap2.estado.nome, sizeof ap2.estado.nome, "%s", "usuario@exemplo.com");
    app_passo(&ap2);

    ENTRA_NOS_AJUSTES(&ap2);

    int antes = ap2.estado.config.valor[AJUSTE_VOZ_SEGURAR];
    DESCE_ATE(&ap2, vista_ajustes, "Câmera e voz");
    pc_botao(IN_OK);
    app_passo(&ap2);
    ESPERA_IGUAL(ap2.estado.pilha[ap2.estado.profundidade], TELA_SOM);

    vista_cartao_t v;
    vista_som(&ap2.estado, &v);
    int onde = -1;
    for (int i = 0; i < v.n_dest; i++)
        if (strcmp(v.dest[i].titulo, "Botão de voz") == 0) onde = i;
    ESPERA(onde >= 0);

    ap2.estado.cursor = onde;
    pc_botao(IN_OK);
    app_passo(&ap2);
    ESPERA(ap2.estado.config.valor[AJUSTE_VOZ_SEGURAR] != antes);

    // E não aparece em Minha conta nem em Aparência.
    vista_cartao_t c;
    vista_conta(&ap2.estado, &c);
    for (int i = 0; i < c.n_dest; i++)
        ESPERA(strcmp(c.dest[i].titulo, "Botão de voz") != 0);

    vista_aparencia(&ap2.estado, &v);
    for (int i = 0; i < v.n_dest; i++)
        ESPERA(strcmp(v.dest[i].titulo, "Botão de voz") != 0);

    TERMINA();
}

// ── Hora e tela ─────────────────────────────────────────────────────
// O card diz "como está"; abaixo, só valores alteráveis.
void t_aparencia_no_desenho_da_fase_3(void)
{
    COMECA("Aparência · o card diz o estado, e a lista só o que muda");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 9, 3};
    e.bateria = 81;
    e.config.valor[AJUSTE_HORA24] = 1;
    e.config.valor[AJUSTE_BLOQUEAR_MIN] = 3;
    e.config.valor[AJUSTE_HORA_REDE] = 1;

    vista_cartao_t v;
    vista_aparencia(&e, &v);

    // Os fatos são os três assuntos do título.
    ESPERA_CONTEM(v.kicker, "relógio e bloqueio");
    ESPERA_CONTEM(v.nome, "pela rede");
    ESPERA_IGUAL(v.n_fatos, 3);
    ESPERA_TEXTO(v.fatos[0].rotulo, "Formato");
    ESPERA_TEXTO(v.fatos[0].valor,  "24 h");
    ESPERA_TEXTO(v.fatos[1].rotulo, "Fonte");
    ESPERA_TEXTO(v.fatos[1].valor,  "pela rede");
    ESPERA_TEXTO(v.fatos[2].rotulo, "Bloqueio");
    ESPERA_CONTEM(v.fatos[2].valor, "3 min");

    // Três destinos, todos alteram alguma coisa.
    ESPERA_IGUAL(v.n_dest, 3);
    ESPERA_CONTEM(v.dest[0].titulo, "Formato");
    ESPERA_TEXTO(v.dest[0].valor, "24 h");
    ESPERA_CONTEM(v.dest[2].titulo, "Bloquear após");
    ESPERA_TEXTO(v.dest[2].valor, "3 min");

    // O ícone segue o valor: 24 h digital, 12 h analógico.
    ESPERA_IGUAL(v.dest[0].ico, ICO_RELOGIO_24);
    e.config.valor[AJUSTE_HORA24] = 0;
    vista_aparencia(&e, &v);
    ESPERA_IGUAL(v.dest[0].ico, ICO_RELOGIO);
    ESPERA_TEXTO(v.dest[0].valor, "12 h");
    ESPERA_TEXTO(v.fatos[0].valor, "12 h");

    // A hora à mão muda o veredito do card.
    e.config.valor[AJUSTE_HORA_REDE] = 0;
    vista_aparencia(&e, &v);
    ESPERA_CONTEM(v.nome, "à mão");

    TERMINA();
}

// ── Câmera e voz ────────────────────────────────────────────────────
void t_som_no_desenho_da_fase_3(void)
{
    COMECA("Câmera e voz · o card diz o que há, e a lista o que se ajusta");

    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 9, 3};
    e.config.valor[AJUSTE_VOZ_SEGURAR] = 1;

    vista_cartao_t v;
    vista_som(&e, &v);

    ESPERA_CONTEM(v.kicker, "captura");
    ESPERA_IGUAL(v.n_fatos, 2);
    ESPERA_TEXTO(v.fatos[0].rotulo, "Microfone");
    ESPERA_TEXTO(v.fatos[0].valor,  "interno");
    ESPERA_TEXTO(v.fatos[1].rotulo, "Câmera");
    ESPERA_TEXTO(v.fatos[1].valor,  "desconectada");

    // O gesto de falar é o único ajuste daqui.
    ESPERA_IGUAL(v.n_dest, 1);
    ESPERA_CONTEM(v.dest[0].titulo, "Botão de voz");
    ESPERA_TEXTO(v.dest[0].valor, "segurar");

    e.config.valor[AJUSTE_VOZ_SEGURAR] = 0;
    vista_som(&e, &v);
    ESPERA_TEXTO(v.dest[0].valor, "um toque");

    TERMINA();
}
