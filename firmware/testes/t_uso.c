// firmware/testes/t_uso.c — os casos de uso.
// Cada um deixa o sistema COERENTE sozinho: grava no cartão, enfileira o
// gesto e invalida o cache, nunca metade. "Marcar feita" já existiu em três
// cópias, e uma esquecia de enfileirar.
#include "teste.h"
#include "dado/indice.h"
#include "vista/quando.h"
#include "ui/cartao.h"
#include "tela/bitmap.h"
#include "uso/uso.h"
#include "uso/nuvem.h"
#include "dado/cartao.h"
#include "vista/agenda.h"
#include "vista/nota.h"
#include "vista/conferir.h"

static estado_t    e;
static const hal_t *hal;

#define DIA ((data_t){2026, 8, 18})

// O aparelho como fica depois de desenhar a Agenda: cartão, cache quente e
// rede.
static void aparelho_com(const char *id, const char *titulo,
                         tipo_t tipo, bool feita)
{
    e.rede = REDE_LIGADA;
    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,     sizeof it.id,     "%s", id);
    snprintf(it.titulo, sizeof it.titulo, "%s", titulo);
    it.tipo  = tipo;
    it.feita = feita;
    it.dia   = DIA;
    cartao_grava_item(hal, DIA, &it);

    int quantos = 0;
    (void)cartao_lista_itens(hal, DIA, 0, e.itens, 32, &quantos);
    e.n_itens       = (int16_t)quantos;
    e.itens_validos = true;
}

static void aparelho_limpo(void)
{
    hal = pc_liga();
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje      = DIA;
    e.dia_visto = DIA;
    e.rede      = REDE_LIGADA;   // sem ela nenhum gesto passa (25/08)
}

// ── as três metades do gesto ────────────────────────────────────────
void t_marcar_feita_sem_rede_enfileira(void)
{
    COMECA("marcar feita sem rede grava, enfileira e invalida");

    aparelho_limpo();
    aparelho_com("1422-ipva", "Pagar IPVA", TIPO_TAREFA, false);
    ESPERA_IGUAL(e.n_itens, 1);

    item_t alvo = e.itens[0];
    ESPERA_IGUAL(uso_marcar(hal, &e, &alvo, true), OK);

    // 1. gravou no cartão
    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, DIA, "1422-ipva", &lido), OK);
    ESPERA(lido.feita);

    // 2. enfileirou

    // 3. invalidou o cache
    ESPERA(!e.itens_validos);

    TERMINA();
}

void t_desmarcar_enfileira_o_estado_novo(void)
{
    COMECA("desmarcar enfileira o estado novo, não um evento de desfazer");

    aparelho_limpo();
    aparelho_com("1422-ipva", "Pagar IPVA", TIPO_TAREFA, true);

    item_t alvo = e.itens[0];
    ESPERA_IGUAL(uso_marcar(hal, &e, &alvo, false), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, DIA, "1422-ipva", &lido), OK);
    ESPERA(!lido.feita);

    TERMINA();
}

// Se o cartão falha, NADA acontece: enfileirar sem gravar subiria um
// estado que o aparelho não tem.
void t_cartao_falhou_entao_nada_aconteceu(void)
{
    COMECA("cartão falhou: não enfileira, não invalida, não mente");

    aparelho_limpo();
    aparelho_com("1422-ipva", "Pagar IPVA", TIPO_TAREFA, false);

    item_t alvo = e.itens[0];
    pc_falhar_escrever(true);
    ESPERA(uso_marcar(hal, &e, &alvo, true) != OK);
    pc_falhar_escrever(false);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, DIA, "1422-ipva", &lido), OK);
    ESPERA(!lido.feita);                 // o cartão continua como estava
    ESPERA(e.itens_validos);             // o cache não mentiu, então vale

    TERMINA();
}

// RN-45: a ação carrega o horário do GESTO, para o backend resolver
// conflito com o Google.
void t_a_acao_carrega_o_horario_do_gesto(void)
{
    COMECA("RN-45 · a ação carrega o horário do gesto");

    aparelho_limpo();
    aparelho_com("1422-ipva", "Pagar IPVA", TIPO_TAREFA, false);

    e.hora = 14;
    e.minuto = 22;
    e.hora_confiavel = true;
    e.config.valor[AJUSTE_FUSO_MIN] = -180;
    pc_avanca_ms(90000);
    item_t alvo = e.itens[0];
    ESPERA_IGUAL(uso_marcar(hal, &e, &alvo, true), OK);
    ESPERA_CONTEM(pc_nuvem_corpo(),
                  "\"em\":\"2026-08-18T14:22:00-03:00\"");

    TERMINA();
}

// RN-42: cinco toques no OK viram um estado.
void t_marcar_cinco_vezes_e_um_estado(void)
{
    COMECA("RN-42 · cinco toques no OK terminam num estado só");

    aparelho_limpo();
    aparelho_com("1422-ipva", "Pagar IPVA", TIPO_TAREFA, false);

    for (int i = 0; i < 5; i++) {
        item_t alvo = e.itens[0];
        alvo.feita  = (i % 2) != 0;
        ESPERA_IGUAL(uso_marcar(hal, &e, &alvo, i % 2 == 0), OK);
    }

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, DIA, "1422-ipva", &lido), OK);
    ESPERA(lido.feita);             // o quinto toque, e só ele, vale

    TERMINA();
}

// O item mora na pasta do dia em que foi falado (RN-26): o caso de uso
// grava onde ele mora, não no dia visto.
void t_marca_item_de_outro_dia_no_lugar_certo(void)
{
    COMECA("RN-34 · atrasada de outro dia é gravada na pasta dela");

    aparelho_limpo();

    data_t ontem = {2026, 8, 3};
    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,     sizeof it.id,     "%s", "0900-ipva");
    snprintf(it.titulo, sizeof it.titulo, "%s", "Pagar IPVA");
    it.tipo = TIPO_TAREFA;
    it.dia  = ontem;
    cartao_grava_item(hal, ontem, &it);

    ESPERA_IGUAL(uso_marcar(hal, &e, &it, true), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, ontem, "0900-ipva", &lido), OK);
    ESPERA(lido.feita);

    TERMINA();
}

// ── renomear — RN-24 ────────────────────────────────────────────────
void t_renomear_trava_o_titulo(void)
{
    COMECA("RN-24 · título escrito à mão trava, e a trava sobrevive ao cartão");

    aparelho_limpo();
    aparelho_com("0914-nota", "Gravação 09:14", TIPO_ANOTACAO, false);
    ESPERA(!e.itens[0].titulo_manual);

    item_t alvo = e.itens[0];
    ESPERA_IGUAL(uso_renomear(hal, &e, &alvo, "Ideia do painel"), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, DIA, "0914-nota", &lido), OK);
    ESPERA_TEXTO(lido.titulo, "Ideia do painel");
    // A trava persiste no meta: senão o próximo processamento apaga o que a
    // pessoa escreveu.
    ESPERA(lido.titulo_manual);

    TERMINA();
}

// RN-B7: truncar, não rejeitar.
void t_renomear_trunca_em_vez_de_recusar(void)
{
    COMECA("RN-B7 · título comprido é truncado, não recusado");

    aparelho_limpo();
    aparelho_com("0914-nota", "Nota", TIPO_ANOTACAO, false);

    char gigante[200];
    memset(gigante, 'a', sizeof gigante - 1);
    gigante[sizeof gigante - 1] = '\0';

    item_t alvo = e.itens[0];
    ESPERA_IGUAL(uso_renomear(hal, &e, &alvo, gigante), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, DIA, "0914-nota", &lido), OK);
    ESPERA_IGUAL((int)strlen(lido.titulo), (int)sizeof lido.titulo - 1);

    TERMINA();
}

// ── apagar — RN-43 ──────────────────────────────────────────────────
void t_apagar_some_do_cartao_e_engole_a_fila(void)
{
    COMECA("RN-43 · apagar some do cartão");

    aparelho_limpo();
    aparelho_com("0914-nota", "Ideia do painel", TIPO_ANOTACAO, false);

    item_t alvo = e.itens[0];
    ESPERA_IGUAL(uso_renomear(hal, &e, &alvo, "Outro nome"), OK);

    ESPERA_IGUAL(uso_apagar_item(hal, &e, &alvo), OK);

    item_t lido;
    ESPERA(cartao_le_item(hal, DIA, "0914-nota", &lido) != OK);

    TERMINA();
}

// Tarefa com hora CONTINUA tarefa: um híbrido, na régua e marcável.
// Virar evento criava duas coisas brigando pelo mesmo compromisso.
void t_tarefa_com_hora_continua_tarefa(void)
{
    COMECA("tarefa que ganha hora continua tarefa, e não vira duas coisas");

    aparelho_limpo();
    aparelho_com("1422-ipva", "Pagar IPVA", TIPO_TAREFA, false);

    item_t alvo = e.itens[0];
    data_t quinta = {2026, 8, 20};
    ESPERA_IGUAL(uso_mudar_data(hal, &e, &alvo, quinta, "15:00"), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, DIA, "1422-ipva", &lido), OK);
    ESPERA_IGUAL(lido.tipo, TIPO_TAREFA);
    ESPERA_TEXTO(lido.hora, "15:00");
    ESPERA_IGUAL(lido.vence.dia, 20);

    // E continua marcável: é o que a separa de um evento.
    ESPERA_IGUAL(uso_marcar(hal, &e, &lido, true), OK);
    ESPERA_IGUAL(cartao_le_item(hal, DIA, "1422-ipva", &lido), OK);
    ESPERA(lido.feita);
    ESPERA_TEXTO(lido.hora, "15:00");
    TERMINA();
}

// Mudar a hora leva o FIM junto, com a mesma duração.
void t_mudar_a_hora_leva_o_fim_junto(void)
{
    COMECA("mudar a hora leva o fim junto, com a mesma duração");

    aparelho_limpo();
    aparelho_com("ev-dentista", "Dentista", TIPO_EVENTO, false);
    item_t it;
    ESPERA_IGUAL(cartao_le_item(hal, DIA, "ev-dentista", &it), OK);
    snprintf(it.hora, sizeof it.hora, "%s", "13:00");
    snprintf(it.fim,  sizeof it.fim,  "%s", "14:30");
    ESPERA_IGUAL(cartao_grava_item(hal, DIA, &it), OK);

    ESPERA_IGUAL(uso_mudar_data(hal, &e, &it, DIA, "14:10"), OK);
    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, DIA, "ev-dentista", &lido), OK);
    ESPERA_TEXTO(lido.hora, "14:10");
    ESPERA_TEXTO(lido.fim,  "15:40");

    // Tarefa não tem fim, e continua sem.
    aparelho_com("1422-ipva", "Pagar IPVA", TIPO_TAREFA, false);
    item_t t = e.itens[0];
    for (int i = 0; i < e.n_itens; i++)
        if (strcmp(e.itens[i].id, "1422-ipva") == 0) t = e.itens[i];
    ESPERA_IGUAL(uso_mudar_data(hal, &e, &t, DIA, "09:00"), OK);
    ESPERA_IGUAL(cartao_le_item(hal, DIA, "1422-ipva", &lido), OK);
    ESPERA_TEXTO(lido.fim, "");
    TERMINA();
}

void t_mudar_a_data_nao_move_o_item_de_pasta(void)
{
    COMECA("RN-26 · mudar a data não tira o item do dia em que foi falado");

    aparelho_limpo();
    aparelho_com("1422-ipva", "Pagar IPVA", TIPO_TAREFA, false);

    item_t alvo = e.itens[0];
    data_t quinta = {2026, 8, 20};
    ESPERA_IGUAL(uso_mudar_data(hal, &e, &alvo, quinta, ""), OK);

    // Continua morando no dia em que foi falada.
    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, DIA, "1422-ipva", &lido), OK);
    ESPERA_IGUAL(lido.tipo, TIPO_TAREFA);      // sem hora, sem conversão
    ESPERA_IGUAL(lido.vence.dia, 20);
    int quantos = -1;
    ESPERA_IGUAL(cartao_lista_itens(hal, quinta, 0, e.itens, 32, &quantos), OK);
    ESPERA_IGUAL(quantos, 0);

    TERMINA();
}

// ── andar entre dias ────────────────────────────────────────────────
void t_ir_para_dia_troca_e_invalida(void)
{
    COMECA("ir para outro dia troca o dia visto e derruba o cache");

    aparelho_limpo();
    aparelho_com("1422-ipva", "Pagar IPVA", TIPO_TAREFA, false);
    ESPERA(e.itens_validos);

    data_t ontem = {2026, 8, 17};
    uso_ir_para_dia(&e, ontem);

    ESPERA_IGUAL(e.dia_visto.dia, 17);
    ESPERA(!e.itens_validos);   // andar não é gesto de sync

    TERMINA();
}

// A decisão olha o CARTÃO, não o cache velho.
void t_a_conversao_olha_o_cartao_nao_o_cache(void)
{
    COMECA("RN-22 · a conversão de tipo olha o cartão, nunca o cache velho");

    aparelho_limpo();
    aparelho_com("1422-ipva", "Dentista", TIPO_EVENTO, false);

    item_t cache_velho = e.itens[0];
    cache_velho.tipo   = TIPO_TAREFA;      // o cache ficou pra trás

    data_t quinta = {2026, 8, 20};
    ESPERA_IGUAL(uso_mudar_data(hal, &e, &cache_velho, quinta, "15:00"), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, DIA, "1422-ipva", &lido), OK);
    ESPERA_IGUAL(lido.tipo, TIPO_EVENTO);

    TERMINA();
}

// ── os ajustes ──────────────────────────────────────────────────────
void t_ajuste_grava_e_sobrevive_ao_reboot(void)
{
    COMECA("o ajuste vai pro cartão e sobrevive ao reboot");

    aparelho_limpo();
    (void)uso_carregar_config(hal, &e);
    ESPERA_IGUAL(e.config.valor[AJUSTE_BLOQUEAR_MIN], 3);   // padrão de fábrica

    ESPERA_IGUAL(uso_salvar_ajuste(hal, &e, AJUSTE_BLOQUEAR_MIN, 12), OK);
    ESPERA_IGUAL(e.config.valor[AJUSTE_BLOQUEAR_MIN], 12);

    // Reboot: outro estado, mesmo cartão.
    estado_t depois;
    memset(&depois, 0, sizeof depois);
    ESPERA_IGUAL(uso_carregar_config(hal, &depois), OK);
    ESPERA_IGUAL(depois.config.valor[AJUSTE_BLOQUEAR_MIN], 12);

    // Ajuste não sobe.

    TERMINA();
}

// O teto mora num lugar só.
void t_ajuste_apara_em_vez_de_recusar(void)
{
    COMECA("valor fora de faixa é aparado, não recusado");

    aparelho_limpo();
    (void)uso_carregar_config(hal, &e);

    ESPERA_IGUAL(uso_salvar_ajuste(hal, &e, AJUSTE_BLOQUEAR_MIN, 999), OK);
    ESPERA_IGUAL(e.config.valor[AJUSTE_BLOQUEAR_MIN], 60);

    // Abaixo de zero vira 0, "nunca".
    ESPERA_IGUAL(uso_salvar_ajuste(hal, &e, AJUSTE_BLOQUEAR_MIN, -50), OK);
    ESPERA_IGUAL(e.config.valor[AJUSTE_BLOQUEAR_MIN], 0);

    TERMINA();
}

// Valor igual não escreve de novo (segurar a tecla no extremo).
void t_ajuste_igual_nao_escreve_de_novo(void)
{
    COMECA("ajuste que não muda nada não toca o cartão");

    aparelho_limpo();
    (void)uso_carregar_config(hal, &e);
    (void)uso_salvar_ajuste(hal, &e, AJUSTE_BLOQUEAR_MIN, 7);

    int antes = pc_escritas();
    (void)uso_salvar_ajuste(hal, &e, AJUSTE_BLOQUEAR_MIN, 7);
    ESPERA_IGUAL(pc_escritas(), antes);

    TERMINA();
}

// RN-B8 nos dois sentidos: chave nova que o arquivo não tem e chave velha
// que o firmware não lê mais (`"vol"`, real). Persistência por chave, não
// por índice.
void t_config_de_versao_antiga_usa_o_padrao(void)
{
    COMECA("RN-B8 · chave que falta vira padrão, chave que sobra é ignorada");

    aparelho_limpo();
    pc_poe_arquivo("/TINTO/sistema/config.json", "{\"vol\":25,\"h24\":0}");

    ESPERA_IGUAL(uso_carregar_config(hal, &e), OK);
    ESPERA_IGUAL(e.config.valor[AJUSTE_HORA24], 0);       // o que estava lá
    ESPERA_IGUAL(e.config.valor[AJUSTE_BLOQUEAR_MIN], 3);   // o que faltava

    TERMINA();
}

// ── sem cartão: a única condição que impede tudo (RN-A3) ────────────
void t_sem_cartao_o_aparelho_diz_e_nao_finge(void)
{
    COMECA("RN-A3 · sem cartão o aparelho diz, e nenhum botão finge");

    hal = pc_liga();
    pc_sem_cartao(true);

    static app_t ap;
    app_liga(&ap, hal);
    app_passo(&ap);

    // "Vazio" e "quebrado" são estados diferentes. Sem mídia, fase RAIZ
    // (RN-6A).
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_MEMORIA_AUSENTE);
    ESPERA_IGUAL(ap.estado.inicio.diagnostico, MEMORIA_AUSENTE);
    ESPERA_IGUAL(ap.estado.ultimo_erro, ERR_SEM_CARTAO);

    // Nenhum botão faz nada, nem o ●: gravar sem onde escrever é pior.
    pc_botao(IN_VOZ);
    pc_botao(IN_OK);
    pc_botao(IN_BAIXO);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_PARADA);
    ESPERA_IGUAL(ap.estado.inicio.fase, INICIO_MEMORIA_AUSENTE);
    ESPERA_IGUAL(ap.estado.cursor, 0);

    TERMINA();
}

// RN-68: cartão sem /TINTO/ cria e segue.
void t_cartao_virgem_nao_bloqueia(void)
{
    COMECA("RN-68 · cartão sem /TINTO/ cria a árvore e segue");

    hal = pc_liga();          // cartão vazio, mas presente
    static app_t ap;
    app_liga(&ap, hal);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.pilha[0], TELA_HOME);
    ESPERA(pc_tem_arquivo("/TINTO/sistema/formato.json"));

    TERMINA();
}

// Cartão mudo NÃO é agenda vazia: o índice não monta e o erro chega ao
// estado.
void t_falha_de_listagem_chega_ao_estado_do_app(void)
{
    COMECA("falha de listagem chega ao estado e não vira dia vazio");

    const hal_t *memoria = pc_liga();
    pc_falhar_listar(true);

    static app_t ap;
    app_liga(&ap, memoria);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.ultimo_erro, ERR_ARQUIVO);
    ESPERA(!ap.estado.itens_validos);
    ESPERA(!indice_pronto());

    TERMINA();
}

// RN-34: tarefa falada ontem para quinta aparece hoje.
void t_tarefa_futura_de_outro_dia_aparece_hoje(void)
{
    COMECA("RN-34 · tarefa aberta aparece em hoje, venha do dia que vier");

    aparelho_limpo();

    data_t ontem  = {2026, 8, 17};
    data_t amanha = {2026, 8, 19};

    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,     sizeof it.id,     "%s", "0900-passaporte");
    snprintf(it.titulo, sizeof it.titulo, "%s", "renovar o passaporte");
    it.tipo  = TIPO_TAREFA;
    it.dia   = ontem;
    it.vence = amanha;              // falada ontem, vence amanhã
    cartao_grava_item(hal, ontem, &it);

    e.itens_validos = false;
    ESPERA_IGUAL(uso_carregar_dia(hal, &e), OK);

    bool achou = false;
    for (int i = 0; i < e.n_itens; i++)
        if (strcmp(e.itens[i].id, "0900-passaporte") == 0) achou = true;
    ESPERA(achou);

    TERMINA();
}

// RN-36: dia passado é só histórico, não recebe pendência.
void t_dia_passado_nao_recebe_pendencia(void)
{
    COMECA("RN-36 · dia passado não recebe tarefa aberta de outro dia");

    aparelho_limpo();

    data_t ontem = {2026, 8, 17};
    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,     sizeof it.id,     "%s", "1000-ipva");
    snprintf(it.titulo, sizeof it.titulo, "%s", "pagar IPVA");
    it.tipo = TIPO_TAREFA;
    it.dia  = DIA;
    cartao_grava_item(hal, DIA, &it);      // aberta, no dia de hoje

    uso_ir_para_dia(&e, ontem);            // vai pro passado
    ESPERA_IGUAL(uso_carregar_dia(hal, &e), OK);

    ESPERA_IGUAL(e.n_itens, 0);            // o passado está vazio, e é isso

    TERMINA();
}

// O calendário marca o mês inteiro, não só o dia carregado.
void t_o_calendario_marca_o_mes_inteiro(void)
{
    COMECA("RN-69 · o calendário marca o mês inteiro, não só o dia aberto");

    aparelho_limpo();

    // As marcas são EVENTO × TAREFA, o vocabulário do Google.
    struct { int dia; tipo_t tipo; origem_t org; } D[] = {
        {  3, TIPO_EVENTO, ORIGEM_GOOGLE },   // só evento
        {  8, TIPO_TAREFA, ORIGEM_AQUI   },   // só tarefa
        { 12, TIPO_EVENTO, ORIGEM_GOOGLE },   // os dois no mesmo dia
        { 12, TIPO_TAREFA, ORIGEM_AQUI   },
    };
    for (size_t i = 0; i < sizeof D / sizeof D[0]; i++) {
        data_t d = { 2026, 8, (int8_t)D[i].dia };
        item_t it;
        memset(&it, 0, sizeof it);
        snprintf(it.id, sizeof it.id, "item%d-%d", D[i].dia, (int)i);
        it.tipo = D[i].tipo; it.origem = D[i].org; it.dia = d;

        // A tarefa marca o dia do PRAZO; sem prazo, não marca nada.
        if (D[i].tipo == TIPO_TAREFA) it.vence = d;

        cartao_grava_item(hal, d, &it);
    }

    ESPERA_IGUAL(uso_carregar_marcas(hal, &e), OK);

    ESPERA(e.marcas_evento  & (1u << 2));    // dia 3
    ESPERA(e.marcas_tarefa & (1u << 7));    // dia 8
    ESPERA(e.marcas_evento  & (1u << 11));   // dia 12, os dois
    ESPERA(e.marcas_tarefa & (1u << 11));

    // Dia sem nada continua sem marca.
    ESPERA(!(e.marcas_evento  & (1u << 4)));
    ESPERA(!(e.marcas_tarefa & (1u << 4)));

    TERMINA();
}

// Mais dias do que cabem numa listagem: a varredura parava em 32 pastas,
// e quais sumiam era a ordem do FAT.
void t_o_calendario_ve_alem_da_primeira_pagina(void)
{
    COMECA("o calendário marca o mês inteiro com mais de 32 dias no cartão");

    aparelho_limpo();

    // Julho enche a primeira página; agosto vem depois.
    for (int dia = 1; dia <= 25; dia++) {
        data_t d = { 2026, 7, (int8_t)dia };
        item_t it;
        memset(&it, 0, sizeof it);
        snprintf(it.id, sizeof it.id, "jul-%d", dia);
        it.tipo = TIPO_EVENTO; it.origem = ORIGEM_GOOGLE; it.dia = d;
        ESPERA_IGUAL(cartao_grava_item(hal, d, &it), OK);
    }
    for (int dia = 1; dia <= 10; dia++) {
        data_t d = { 2026, 8, (int8_t)dia };
        item_t it;
        memset(&it, 0, sizeof it);
        snprintf(it.id, sizeof it.id, "ago-%d", dia);
        it.tipo = TIPO_EVENTO; it.origem = ORIGEM_GOOGLE; it.dia = d;
        ESPERA_IGUAL(cartao_grava_item(hal, d, &it), OK);
    }

    estado_t e;
    memset(&e, 0, sizeof e);
    e.hoje = (data_t){ 2026, 8, 5 };
    e.dia_visto = e.hoje;

    ESPERA_IGUAL(uso_carregar_marcas(hal, &e), OK);

    for (int dia = 1; dia <= 10; dia++)
        ESPERA(e.marcas_evento & (1u << (dia - 1)));

    ESPERA(!(e.marcas_evento & (1u << 20)));   // dia 21: não existe

    TERMINA();
}


// O evento de vários dias ocupa todos eles, na grade e na tela do dia.
void t_evento_de_varios_dias_ocupa_todos_eles(void)
{
    COMECA("evento de vários dias marca o intervalo e aparece no meio dele");

    aparelho_limpo();

    data_t comeco = { 2026, 8, 14 };
    item_t viagem;
    memset(&viagem, 0, sizeof viagem);
    snprintf(viagem.id,     sizeof viagem.id,     "%s", "g%viagem");
    snprintf(viagem.titulo, sizeof viagem.titulo, "%s", "Viagem");
    viagem.tipo  = TIPO_EVENTO;
    viagem.origem = ORIGEM_GOOGLE;
    viagem.dia   = comeco;
    viagem.vence = comeco;
    viagem.prazo = (data_t){ 2026, 8, 17 };
    viagem.dia_inteiro = true;
    ESPERA_IGUAL(cartao_grava_item(hal, comeco, &viagem), OK);

    estado_t e;
    memset(&e, 0, sizeof e);
    e.hoje = (data_t){ 2026, 8, 14 };
    e.dia_visto = e.hoje;

    ESPERA_IGUAL(uso_carregar_marcas(hal, &e), OK);
    for (int dia = 14; dia <= 17; dia++)
        ESPERA(e.marcas_evento & (1u << (dia - 1)));
    ESPERA(!(e.marcas_evento & (1u << 17)));   // dia 18: acabou

    // O dia do meio ABRE com ele dentro.
    uso_ir_para_dia(&e, (data_t){ 2026, 8, 16 });
    ESPERA_IGUAL(uso_carregar_dia(hal, &e), OK);

    int achou = 0;
    for (int i = 0; i < e.n_itens; i++)
        if (strcmp(e.itens[i].titulo, "Viagem") == 0) achou++;
    ESPERA_IGUAL(achou, 1);

    // E o dia seguinte ao fim continua vazio.
    uso_ir_para_dia(&e, (data_t){ 2026, 8, 18 });
    ESPERA_IGUAL(uso_carregar_dia(hal, &e), OK);
    ESPERA_IGUAL(e.n_itens, 0);

    TERMINA();
}


// Descartar invalida o cache: a captura ficava fantasma na Agenda.
void t_descartar_invalida_o_cache(void)
{
    COMECA("descartar a captura derruba o cache, e ela some da home");

    aparelho_limpo();
    e.itens_validos  = true;
    e.marcas_validas = true;
    e.gravacao.fase  = GRAV_GRAVANDO;

    ESPERA_IGUAL(uso_descartar_captura(hal, &e), OK);
    ESPERA(!e.itens_validos);
    ESPERA(!e.marcas_validas);

    TERMINA();
}

// Estourar o teto é caso a tratar: lista que esconde parece completa.
void t_item_que_nao_cabe_e_contado(void)
{
    COMECA("item que não cabe no cache é contado, não some calado");

    aparelho_limpo();

    // Mais tarefas abertas do que o cache aguenta.
    for (int i = 0; i < 40; i++) {
        data_t d = { 2026, 8, (int8_t)(1 + i % 16) };
        if (data_igual(d, DIA)) continue;
        item_t it;
        memset(&it, 0, sizeof it);
        snprintf(it.id, sizeof it.id, "t%02d", i);
        snprintf(it.titulo, sizeof it.titulo, "tarefa %d", i);
        it.tipo = TIPO_TAREFA;
        it.dia  = d;
        cartao_grava_item(hal, d, &it);
    }

    e.itens_validos = false;
    ESPERA_IGUAL(uso_carregar_dia(hal, &e), OK);

    ESPERA_IGUAL(e.n_itens, ITENS_MAX);
    ESPERA(e.n_fora > 0);          // o que sobrou foi CONTADO

    vista_agenda_t v;
    vista_agenda(&e, &v);
    ESPERA(v.aviso[0] != '\0');    // e a home diz

    TERMINA();
}

// ── o que a tela do item oferece ────────────────────────────────────
// Não oferecer o que o outro lado não aceita: oferecer e falhar mina a
// confiança no resto da tela.
static void abre(const char *id, const char *titulo, tipo_t tipo,
                 origem_t origem)
{
    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,     sizeof it.id,     "%s", id);
    snprintf(it.titulo, sizeof it.titulo, "%s", titulo);
    it.tipo   = tipo;
    it.origem = origem;
    it.dia    = DIA;
    cartao_grava_item(hal, DIA, &it);
    uso_abrir_item(hal, &e, &it);
}

void t_evento_do_google_e_so_leitura(void)
{
    COMECA("evento do Google se renomeia e se apaga, como qualquer outro");

    aparelho_limpo();
    abre("g-dentista", "Dentista", TIPO_EVENTO, ORIGEM_GOOGLE);

    vista_nota_t v;
    vista_nota(&e, &v);

    // Evento: renomear, data, hora e apagar, para qualquer evento (o escopo
    // `calendar` inteiro).
    ESPERA_IGUAL(v.n_acoes, 4);
    ESPERA_TEXTO(v.acoes[0].texto, "Renomear");
    ESPERA_TEXTO(v.acoes[1].texto, "Mudar a data");
    ESPERA_TEXTO(v.acoes[2].texto, "Mudar a hora");
    ESPERA_TEXTO(v.acoes[3].texto, "Apagar");

    // O destino continua aparecendo.
    ESPERA(strstr(v.onde, "Google Agenda") != NULL);

    TERMINA();
}

// QUANDO: a data à mão, com os dias já contados.
void t_quando_oferece_os_dias_prontos(void)
{
    COMECA("mudar a data · os dias vêm prontos, com o dia da semana");

    aparelho_limpo();
    abre("n-dentista", "Dentista", TIPO_EVENTO, ORIGEM_GOOGLE);

    vista_quando_t v;
    vista_quando(&e, &v);

    // O card mostra de onde se sai.
    ESPERA_TEXTO(v.cartao.nome, "Dentista");
    ESPERA_TEXTO(v.cartao.fatos[0].rotulo, "está em");

    // Amanhã, depois, semana que vem, e o mês.
    ESPERA(v.cartao.n_dest >= 4);
    ESPERA_TEXTO(v.cartao.dest[0].titulo, "Amanhã");
    ESPERA_IGUAL(v.acao[0], QUANDO_DIA);
    ESPERA(data_igual(v.datas[0], data_soma_dias(e.hoje, 1)));
    ESPERA(data_igual(v.datas[1], data_soma_dias(e.hoje, 2)));
    ESPERA(data_igual(v.datas[2], data_soma_dias(e.hoje, 7)));

    // A DATA vem escrita ao lado do rótulo.
    ESPERA(v.cartao.dest[0].valor[0] != '\0');
    ESPERA_IGUAL(v.acao[3], QUANDO_CALENDARIO);

    // HOJE não entra.
    for (int i = 0; i < v.cartao.n_dest; i++)
        if (v.acao[i] == QUANDO_DIA)
            ESPERA(!data_igual(v.datas[i], e.hoje));
    TERMINA();
}

// "Tirar a data" só para tarefa, e só com data.
void t_tirar_a_data_e_so_de_tarefa(void)
{
    COMECA("mudar a data · só tarefa pode ficar sem data");

    aparelho_limpo();
    abre("n-dentista", "Dentista", TIPO_EVENTO, ORIGEM_GOOGLE);
    e.aberto.vence = e.hoje;

    vista_quando_t v;
    vista_quando(&e, &v);
    for (int i = 0; i < v.cartao.n_dest; i++)
        ESPERA(v.acao[i] != QUANDO_SEM_DATA);

    aparelho_limpo();
    abre("n-pasta", "Comprar pasta", TIPO_TAREFA, ORIGEM_GOOGLE);
    e.aberto.vence = e.hoje;

    vista_quando(&e, &v);
    bool tem = false;
    for (int i = 0; i < v.cartao.n_dest; i++)
        if (v.acao[i] == QUANDO_SEM_DATA) tem = true;
    ESPERA(tem);

    memset(&e.aberto.vence, 0, sizeof e.aberto.vence);
    vista_quando(&e, &v);
    for (int i = 0; i < v.cartao.n_dest; i++)
        ESPERA(v.acao[i] != QUANDO_SEM_DATA);
    TERMINA();
}

// A hora: evento sempre; tarefa só quando já tem hora (ela mora no evento
// gêmeo do Google Agenda).
void t_hora_e_de_evento_prazo_e_de_tarefa(void)
{
    COMECA("hora é de evento; tarefa põe e tira prazo, e nada de hora");

    // Tarefa COM prazo: a linha diz MUDAR.
    aparelho_limpo();
    abre("n-pasta", "Comprar pasta", TIPO_TAREFA, ORIGEM_GOOGLE);
    e.aberto.vence = e.hoje;

    vista_nota_t v;
    vista_nota(&e, &v);
    ESPERA_TEXTO(v.acoes[1].texto, "Mudar a data");

    // Sem hora não há o que mudar nem como PÔR.
    for (int i = 0; i < v.n_acoes; i++)
        ESPERA(strstr(v.acoes[i].texto, "hora") == NULL);

    // Com hora, é o híbrido e a hora se muda.
    snprintf(e.aberto.hora, sizeof e.aberto.hora, "%s", "07:00");
    vista_nota(&e, &v);
    ESPERA_TEXTO(v.acoes[2].texto, "Mudar a hora");

    // Lá dentro dá para tirar o prazo.
    vista_quando_t q;
    vista_quando(&e, &q);
    bool tira = false;
    for (int i = 0; i < q.cartao.n_dest; i++)
        if (q.acao[i] == QUANDO_SEM_DATA) tira = true;
    ESPERA(tira);

    // Evento: "uma hora depois" nasce da hora que ele tem.
    aparelho_limpo();
    abre("n-dentista", "Dentista", TIPO_EVENTO, ORIGEM_GOOGLE);
    e.aberto.vence = e.hoje;
    snprintf(e.aberto.hora, sizeof e.aberto.hora, "%s", "15:00");

    vista_horario(&e, &q);
    ESPERA_TEXTO(q.cartao.fatos[0].rotulo, "está às");
    ESPERA_TEXTO(q.cartao.dest[0].titulo, "Uma hora depois");
    ESPERA_TEXTO(q.horas[0], "16:00");
    ESPERA_TEXTO(q.horas[1], "14:00");

    // "Dia inteiro" só quando há hora para tirar.
    bool inteiro = false;
    for (int i = 0; i < q.cartao.n_dest; i++)
        if (q.acao[i] == QUANDO_DIA_INTEIRO) inteiro = true;
    ESPERA(inteiro);

    // Evento sem hora: as âncoras do dia e o mostrador.
    e.aberto.hora[0] = '\0';
    vista_horario(&e, &q);
    for (int i = 0; i < q.cartao.n_dest; i++) {
        ESPERA(q.acao[i] == QUANDO_HORA || q.acao[i] == QUANDO_RELOGIO);
        ESPERA(strcmp(q.cartao.dest[i].titulo, "Uma hora depois") != 0);
    }

    // O mostrador está sempre lá.
    bool livre = false;
    for (int i = 0; i < q.cartao.n_dest; i++)
        if (q.acao[i] == QUANDO_RELOGIO) livre = true;
    ESPERA(livre);
    TERMINA();
}

// As telas de QUANDO cabem no quadro: o card não rola, e a resposta é
// tirar linha.
void t_as_telas_de_quando_cabem_no_quadro(void)
{
    COMECA("mudar data e hora cabem no quadro, sem esconder linha");

    aparelho_limpo();
    abre("n-dentista", "Dentista", TIPO_EVENTO, ORIGEM_GOOGLE);
    e.aberto.vence = e.hoje;
    snprintf(e.aberto.hora, sizeof e.aberto.hora, "%s", "15:00");

    static bitmap_t bm;
    static uint8_t px[(240 / 8) * 416];
    bitmap_liga(&bm, px, 240, 416);

    vista_quando_t v;

    // O pior caso da data.
    e.aberto.tipo = TIPO_TAREFA;
    vista_quando(&e, &v);
    ESPERA(tela_cartao_altura(&bm, &v.cartao) <= tela_cartao_area(&bm));

    // O pior caso da hora.
    e.aberto.tipo = TIPO_EVENTO;
    vista_horario(&e, &v);
    ESPERA(tela_cartao_altura(&bm, &v.cartao) <= tela_cartao_area(&bm));

    // E o da tarefa.
    e.aberto.tipo = TIPO_TAREFA;
    vista_horario(&e, &v);
    ESPERA(tela_cartao_altura(&bm, &v.cartao) <= tela_cartao_area(&bm));
    TERMINA();
}

// O híbrido vale em TODOS os lugares: este teste é a lista.
void t_o_hibrido_vale_em_todo_lugar(void)
{
    COMECA("tarefa com hora: detalhe, kicker, calendário e bloqueio");

    aparelho_limpo();
    abre("t:academia", "Academia", TIPO_TAREFA, ORIGEM_GOOGLE);
    e.aberto.vence = e.hoje;
    snprintf(e.aberto.hora, sizeof e.aberto.hora, "%s", "07:00");

    vista_nota_t v;
    vista_nota(&e, &v);

    // 1 · o detalhe mostra a hora, com o rótulo QUANDO (não VENCE).
    bool tem_hora = false;
    for (int i = 0; i < v.n_campos; i++)
        if (strstr(v.campos[i].valor, "07:00") &&
            strcmp(v.campos[i].rotulo, "QUANDO") == 0) tem_hora = true;
    ESPERA(tem_hora);

    // Sem hora ela vira PRAZO, abaixo de CRIADA.
    e.aberto.hora[0] = '\0';
    vista_nota(&e, &v);
    bool prazo = false;
    for (int i = 0; i < v.n_campos; i++)
        if (strcmp(v.campos[i].rotulo, "PRAZO") == 0) prazo = true;
    ESPERA(prazo);

    snprintf(e.aberto.hora, sizeof e.aberto.hora, "%s", "07:00");
    vista_nota(&e, &v);

    // 2 · o kicker se nomeia "TAREFA COM HORA".
    ESPERA(strstr(v.onde, "TAREFA COM HORA") != NULL);

    // 3 · e ela continua se marcando.
    bool marca = false;
    for (int i = 0; i < v.n_botoes; i++)
        if (strstr(v.botoes[i].texto, "conclu")) marca = true;
    ESPERA(marca);
    TERMINA();
}

void t_evento_nao_oferece_marcar_feita(void)
{
    COMECA("RN-2B · evento não tem \"feito\", e a tela não oferece");

    aparelho_limpo();
    abre("n-reuniao", "Reunião com o cliente", TIPO_EVENTO, ORIGEM_AQUI);

    vista_nota_t v;
    vista_nota(&e, &v);

    // Renomear, data, hora, apagar. "Mudar o tipo" não existe: o tipo é o que a
    // LLM decidiu.
    ESPERA_IGUAL(v.n_acoes, 4);
    for (int i = 0; i < v.n_acoes; i++)
        ESPERA(strcmp(v.acoes[i].texto, "marcar feita") != 0);
    // Evento não tem "concluir".
    for (int i = 0; i < v.n_botoes; i++)
        ESPERA(v.botoes[i].texto[0] != 'M');

    // O destino nomeia o APP onde se procura (a agenda "Tinto" está no Google
    // Agenda).
    ESPERA(strstr(v.onde, "Google Agenda") != NULL);

    // A tarefa oferece concluir, como BOTÃO.
    aparelho_limpo();
    abre("n-pasta", "comprar pasta térmica", TIPO_TAREFA, ORIGEM_GOOGLE);
    vista_nota(&e, &v);

    // Tarefa sem prazo: três ações (hora sem dia não põe nada na régua).
    ESPERA_IGUAL(v.n_acoes, 3);
    ESPERA_TEXTO(v.acoes[1].texto, "Pôr data");
    for (int i = 0; i < v.n_acoes; i++)
        ESPERA(strstr(v.acoes[i].texto, "hora") == NULL);
    ESPERA_CONTEM(v.botoes[0].texto, "conclu");

    ESPERA(strstr(v.onde, "TAREFA") != NULL);

    TERMINA();
}

// RN-25: a anotação não leva o G.
void t_anotacao_nao_vai_pro_google(void)
{
    COMECA("RN-25 · anotação diz que fica fora do Google, e não leva o G");

    aparelho_limpo();
    abre("n-ideia", "ideia do painel", TIPO_ANOTACAO, ORIGEM_AQUI);

    vista_nota_t v;
    vista_nota(&e, &v);

    ESPERA_TEXTO(v.onde, "ANOTAÇÃO · fora do Google");

    TERMINA();
}

// ── a confirmação do que a fala virou ───────────────────────────────
// A IA PROPÕE e a pessoa decide: nada entra no Google sem ela ter visto.
static resultado_t proposta(const char *id, const char *titulo, tipo_t tipo)
{
    resultado_t r;
    memset(&r, 0, sizeof r);
    r.verbo = RES_CRIOU;
    snprintf(r.item.id,     sizeof r.item.id,     "%s", id);
    snprintf(r.item.titulo, sizeof r.item.titulo, "%s", titulo);
    r.item.tipo = tipo;
    r.item.dia  = DIA;
    return r;
}

void t_a_proposta_nao_toca_no_cartao(void)
{
    COMECA("a proposta fica na RAM: recusar não deixa rastro no cartão");

    aparelho_limpo();
    resultado_t r = proposta("1500-reuniao", "Reunião com o cliente",
                             TIPO_EVENTO);
    ESPERA_IGUAL(uso_propor_resultado(&e, &r, 1, "marca reunião quinta"), OK);

    // Nada no cartão ainda.
    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, DIA, "1500-reuniao", &lido),
                 ERR_ARQUIVO);

    vista_conferir_t v;
    vista_conferir(&e, &v);
    ESPERA_IGUAL(v.n_res, 1);
    ESPERA_TEXTO(v.res[0].titulo, "Reunião com o cliente");

    // RN-4D: o destino aparece antes da decisão, na linha do tipo.
    ESPERA_TEXTO(v.res[0].tipo, "EVENTO · Google Agenda");

    // O verbo do botão diz o que vai acontecer.
    ESPERA_TEXTO(v.decisao[0].texto, "Marcar na agenda");

    // O cursor começa na primeira decisão, a não-destrutiva.
    ESPERA_IGUAL(v.cursor, 0);
    ESPERA_IGUAL(v.cursor_res, -1);
    ESPERA_IGUAL(vista_recibo_linhas(&e), 3);   // 1 resultado + 2 decisões

    // A frase crua está lá.
    ESPERA_TEXTO(v.falou, "marca reunião quinta");

    TERMINA();
}

void t_confirmar_grava_e_enfileira(void)
{
    COMECA("confirmar grava no cartão, enfileira, e a home já mostra");

    aparelho_limpo();
    resultado_t r = proposta("0741-pasta", "comprar pasta térmica",
                             TIPO_TAREFA);
    (void)uso_propor_resultado(&e, &r, 1, "me lembra de comprar pasta");
    e.itens_validos = true;         // como se a home estivesse desenhada

    ESPERA_IGUAL(uso_confirmar_resultado(hal, &e), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, DIA, "0741-pasta", &lido), OK);
    ESPERA_TEXTO(lido.titulo, "comprar pasta térmica");

    // Invalidar é o que faz a volta cair numa Agenda já atualizada.
    ESPERA(!e.itens_validos);
    ESPERA(!e.marcas_validas);

    // As ações ficam para o Resultado listar.
    ESPERA(e.esperando_resultado);
    ESPERA_IGUAL(e.n_resultados, 1);
    ESPERA_TEXTO(e.falou, "");

    TERMINA();
}

void t_anotacao_confirmada_nao_entra_na_fila(void)
{
    COMECA("RN-25 · anotação confirmada grava no cartão e não vai para o Google");

    aparelho_limpo();
    resultado_t r = proposta("2114-encoder", "o encoder no lugar do 5-vias",
                             TIPO_ANOTACAO);
    r.verbo = RES_ANOTOU;
    (void)uso_propor_resultado(&e, &r, 1, "e se em vez do cinco vias");

    vista_conferir_t v;
    vista_conferir(&e, &v);
    ESPERA_TEXTO(v.decisao[0].texto, "Guardar a anotação");

    ESPERA_IGUAL(uso_confirmar_resultado(hal, &e), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, DIA, "2114-encoder", &lido), OK);

    TERMINA();
}

// O áudio some depois de confirmado; fica a transcrição.
void t_confirmar_apaga_o_audio_e_guarda_a_transcricao(void)
{
    COMECA("confirmar apaga o WAV: o cartão não é acervo de gravação");

    aparelho_limpo();

    snprintf(e.ultimo.id, sizeof e.ultimo.id, "%s", "1422-fala");
    e.ultimo.dia = DIA;
    snprintf(e.falou, sizeof e.falou, "%s", "comprar pasta termica");

    resultado_t r;
    memset(&r, 0, sizeof r);
    r.verbo = RES_CRIOU;
    snprintf(r.item.id,     sizeof r.item.id,     "%s", "t:1");
    snprintf(r.item.titulo, sizeof r.item.titulo, "%s", "Comprar pasta");
    r.item.tipo = TIPO_TAREFA;
    r.item.dia  = DIA;
    ESPERA_IGUAL(uso_propor_resultado(&e, &r, 1, e.falou), OK);

    ESPERA_IGUAL(uso_confirmar_resultado(hal, &e), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, DIA, "t:1", &lido), OK);

    ESPERA(!pc_tem_arquivo("/TINTO/itens/2026-08-18/1422-fala/audio.wav"));
    TERMINA();
}


// Depois da decisão, a captura crua não fica (o WAV é o arquivo mais
// pesado).
static void uma_fala_no_cartao(void)
{
    item_t fala;
    memset(&fala, 0, sizeof fala);
    snprintf(fala.id, sizeof fala.id, "%s", "1422-fala");
    fala.tipo   = TIPO_NADA;
    fala.origem = ORIGEM_AQUI;
    fala.dia    = DIA;
    fala.dur_s  = 14;
    ESPERA_IGUAL(cartao_grava_item(hal, DIA, &fala), OK);

    char wav[128];
    cartao_caminho_wav(DIA, "1422-fala", wav, sizeof wav);
    pc_poe_arquivo(wav, "RIFF");

    e.ultimo = fala;
    e.hoje   = DIA;
}

void t_confirmar_apaga_a_captura_crua(void)
{
    COMECA("confirmada a fala, a captura crua e o WAV saem do cartão");

    aparelho_limpo();
    uma_fala_no_cartao();

    resultado_t r = proposta("n:1", "Dentista", TIPO_EVENTO);
    ESPERA_IGUAL(uso_propor_resultado(&e, &r, 1, "marca dentista"), OK);
    ESPERA_IGUAL(uso_confirmar_resultado(hal, &e), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, DIA, "1422-fala", &lido), ERR_ARQUIVO);

    char wav[128];
    cartao_caminho_wav(DIA, "1422-fala", wav, sizeof wav);
    ESPERA(!pc_tem_arquivo(wav));

    // O que ela virou continua.
    ESPERA_IGUAL(cartao_le_item(hal, DIA, "n:1", &lido), OK);

    TERMINA();
}

// "Descartar a gravação" apaga mesmo: chamava a função de abortar gravação
// EM CURSO, que recusava.
void t_descartar_apaga_mesmo_a_gravacao(void)
{
    COMECA("descartar apaga a gravação de verdade, e não só a sugestão");

    aparelho_limpo();
    uma_fala_no_cartao();

    resultado_t r = proposta("n:1", "Dentista", TIPO_EVENTO);
    ESPERA_IGUAL(uso_propor_resultado(&e, &r, 1, "marca dentista"), OK);
    ESPERA_IGUAL(uso_descartar_resultado(hal, &e), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, DIA, "1422-fala", &lido), ERR_ARQUIVO);

    char wav[128];
    cartao_caminho_wav(DIA, "1422-fala", wav, sizeof wav);
    ESPERA(!pc_tem_arquivo(wav));

    ESPERA_IGUAL(cartao_le_item(hal, DIA, "n:1", &lido), ERR_ARQUIVO);
    ESPERA_IGUAL(e.n_resultados, 0);

    TERMINA();
}


// Todo gesto que mexe no cartão derruba TODOS os caches (dia, marcas,
// pendentes, item aberto): esquecer um deixou tarefa feita aparecendo aberta.
// Mudar o dia visto derruba só o do dia.
void t_marcar_derruba_todos_os_caches(void)
{
    COMECA("marcar feita invalida o cache que a Agenda desenha, não só o do dia");

    const hal_t *hal = pc_liga();
    static estado_t e;
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = e.dia_visto = (data_t){2026, 8, 18};

    e.rede = REDE_LIGADA;

    item_t ta;
    memset(&ta, 0, sizeof ta);
    snprintf(ta.id,     sizeof ta.id,     "%s", "0900-ipva");
    snprintf(ta.titulo, sizeof ta.titulo, "%s", "Pagar IPVA");
    ta.tipo = TIPO_TAREFA;
    ta.dia  = e.hoje;
    ESPERA_IGUAL(cartao_grava_item(hal, e.hoje, &ta), OK);

    ESPERA_IGUAL(uso_carregar_dia(hal, &e), OK);
    ESPERA_IGUAL(uso_carregar_pendentes(hal, &e), OK);
    ESPERA(e.itens_validos && e.pendentes_validas);

    ESPERA_IGUAL(uso_marcar(hal, &e, &ta, true), OK);

    ESPERA(!e.itens_validos);
    ESPERA(!e.pendentes_validas);
    ESPERA(!e.marcas_validas);
    TERMINA();
}

// Confirmar guarda o que o Resultado vai listar (zerar abria "0 ações").
void t_confirmar_guarda_o_que_a_tela_de_resultado_vai_listar(void)
{
    COMECA("confirmar preserva as ações para o Resultado listar");

    aparelho_limpo();
    e.rede = REDE_LIGADA;
    e.resultados[e.n_resultados++] = proposta("r-dentista", "Dentista", TIPO_EVENTO);
    e.resultados[e.n_resultados++] = proposta("r-exames", "Levar os exames", TIPO_TAREFA);

    ESPERA_IGUAL(uso_confirmar_resultado(hal, &e), OK);

    ESPERA_IGUAL(e.n_resultados, 2);
    ESPERA(e.esperando_resultado);
    ESPERA_TEXTO(e.resultados[0].item.titulo, "Dentista");
    ESPERA_TEXTO(e.resultados[1].item.titulo, "Levar os exames");
    TERMINA();
}

// O Conferir do desenho: kicker que conta e cartão de três linhas (tipo e
// destino numa linha; o endereço não muda a decisão).
void t_conferir_tem_o_kicker_e_o_cartao_de_tres_linhas(void)
{
    COMECA("conferir: o kicker conta as ações, e o cartão tem três linhas");

    aparelho_limpo();
    e.rede = REDE_LIGADA;
    e.resultados[e.n_resultados++] = proposta("r-dentista", "Dentista", TIPO_EVENTO);
    e.resultados[e.n_resultados++] = proposta("r-exames", "Levar os exames", TIPO_TAREFA);
    snprintf(e.falou, sizeof e.falou, "%s",
             "marca dentista amanhã às duas e lembra de levar os exames");

    vista_conferir_t v;
    vista_conferir(&e, &v);

    ESPERA_TEXTO(v.kicker, "2 AÇÕES NESTA GRAVAÇÃO");

    ESPERA_CONTEM(v.res[0].tipo, "EVENTO");
    ESPERA_CONTEM(v.res[0].tipo, "Google Agenda");
    ESPERA_CONTEM(v.res[1].tipo, "TAREFA");
    ESPERA_CONTEM(v.res[1].tipo, "Minhas tarefas");

    // Singular: "1 AÇÃO".
    e.n_resultados = 1;
    vista_conferir(&e, &v);
    ESPERA_TEXTO(v.kicker, "1 AÇÃO NESTA GRAVAÇÃO");
    TERMINA();
}

// A captura é apagada no dia em que NASCEU (falar às 23:58 e confirmar às
// 00:01 deixava o WAV para sempre).
void t_captura_e_apagada_no_dia_em_que_nasceu(void)
{
    COMECA("a captura da meia-noite é apagada no dia dela, não no de hoje");

    aparelho_limpo();
    e.rede = REDE_LIGADA;

    data_t ontem = data_soma_dias(e.hoje, -1);
    memset(&e.ultimo, 0, sizeof e.ultimo);
    snprintf(e.ultimo.id, sizeof e.ultimo.id, "%s", "2358-fala");
    e.ultimo.tipo   = TIPO_NADA;
    e.ultimo.origem = ORIGEM_AQUI;
    e.ultimo.dia    = ontem;
    ESPERA_IGUAL(cartao_grava_item(hal, ontem, &e.ultimo), OK);

    e.resultados[e.n_resultados++] = proposta("r-x", "Dentista", TIPO_EVENTO);
    ESPERA_IGUAL(uso_confirmar_resultado(hal, &e), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, ontem, "2358-fala", &lido), ERR_ARQUIVO);
    TERMINA();
}

// Confirmar uma vez só, por mais que o dedo insista: dois dentistas na
// agenda é o pior defeito possível. A trava é `esperando_resultado`.
void t_confirmar_duas_vezes_nao_manda_duas_vezes(void)
{
    COMECA("apertar OK de novo enquanto envia não duplica a ação");

    aparelho_limpo();
    e.rede = REDE_LIGADA;
    pc_nuvem_demora(true);          // segura a resposta: é a janela do bug
    e.resultados[e.n_resultados++] = proposta("r-dentista", "Dentista", TIPO_EVENTO);

    ESPERA_IGUAL(uso_confirmar_resultado(hal, &e), OK);
    ESPERA(e.esperando_resultado);

    ESPERA_IGUAL(e.nuvem_esperando, NUVEM_GESTO);

    ESPERA_IGUAL(uso_confirmar_resultado(hal, &e), ERR_INTERNO);

    vista_conferir_t v;
    vista_conferir(&e, &v);
    ESPERA(v.enviando);
    ESPERA(v.pontos >= 0 && v.pontos < 3);

    pc_nuvem_demora(false);
    TERMINA();
}

// A frase crua chega inteira ao Conferir (era cortada em 160 bytes).
void t_a_frase_longa_chega_inteira_ao_conferir(void)
{
    COMECA("Conferir · uma fala longa chega inteira, sem corte");

    aparelho_limpo();
    char longa[500];
    memset(longa, 'a', sizeof longa - 1);
    longa[sizeof longa - 1] = '\0';

    resultado_t r = proposta("n:1", "Ideia", TIPO_ANOTACAO);
    r.verbo = RES_ANOTOU;
    (void)uso_propor_resultado(&e, &r, 1, longa);

    vista_conferir_t v;
    vista_conferir(&e, &v);
    ESPERA_IGUAL((int)strlen(v.falou), 499);
    TERMINA();
}
