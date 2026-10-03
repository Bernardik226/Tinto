// firmware/testes/t_indice.c — o índice, e quantas vezes o cartão é lido.
// `pc_listagens()` conta, e um teto aqui quebra o `make test` se uma
// varredura voltar.
#include "teste.h"
#include "dado/indice.h"
#include "dado/cartao.h"
#include "uso/uso.h"
#include "app/app.h"

static const hal_t *hal;
static estado_t e;

static void poe(data_t dia, const char *id, const char *titulo, tipo_t tipo)
{
    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,     sizeof it.id,     "%s", id);
    snprintf(it.titulo, sizeof it.titulo, "%s", titulo);
    it.tipo = tipo;
    it.dia  = dia;
    it.vence = dia;
    if (tipo == TIPO_EVENTO) it.origem = ORIGEM_GOOGLE;
    ESPERA_IGUAL(cartao_grava_item(hal, dia, &it), OK);
}

static void aparelho(data_t hoje)
{
    hal = pc_liga();
    pc_relogio(hoje, 9, 0);
    memset(&e, 0, sizeof e);
    e.hoje = hoje;
    e.dia_visto = hoje;
}

void t_o_indice_responde_sem_tocar_o_cartao(void)
{
    COMECA("índice · montar a tela não lê o cartão nenhuma vez");

    data_t hoje = { 2026, 9, 11 };
    aparelho(hoje);

    poe(hoje, "g:hoje1", "Dentista", TIPO_EVENTO);
    poe(hoje, "t:hoje2", "Pagar IPVA", TIPO_TAREFA);
    poe(data_soma_dias(hoje, 1), "g:amanha", "Reunião", TIPO_EVENTO);

    ESPERA_IGUAL(indice_monta(hal), OK);
    ESPERA(indice_pronto());
    ESPERA_IGUAL(indice_n(), 3);

    // A partir daqui, ZERO listagens.
    int antes = pc_listagens();

    ESPERA_IGUAL(uso_carregar_dia(hal, &e), OK);
    ESPERA_IGUAL(uso_carregar_pendentes(hal, &e), OK);
    ESPERA_IGUAL(uso_carregar_marcas(hal, &e), OK);
    ESPERA_IGUAL(uso_carregar_anotacoes(hal, &e), OK);

    ESPERA_IGUAL(pc_listagens() - antes, 0);

    // E as respostas continuam certas.
    ESPERA_IGUAL(e.n_itens, 2);                    // os dois de hoje
    ESPERA_IGUAL(e.n_pendentes, 1);                // a tarefa
    ESPERA(e.marcas_evento & (1u << 10));          // dia 11
    ESPERA(e.marcas_tarefa & (1u << 10));

    TERMINA();
}

void t_o_indice_anda_com_a_escrita(void)
{
    COMECA("índice · gravar e apagar mantêm a RAM em dia, sozinhos");

    data_t hoje = { 2026, 9, 11 };
    aparelho(hoje);
    ESPERA_IGUAL(indice_monta(hal), OK);

    poe(hoje, "g:novo", "Dentista", TIPO_EVENTO);
    ESPERA_IGUAL(indice_n(), 1);

    // Sem reler: quem grava mantém o índice.
    int antes = pc_listagens();
    ESPERA_IGUAL(uso_carregar_dia(hal, &e), OK);
    ESPERA_IGUAL(pc_listagens() - antes, 0);
    ESPERA_IGUAL(e.n_itens, 1);

    ESPERA_IGUAL(cartao_apaga_item(hal, hoje, "g:novo"), OK);
    ESPERA_IGUAL(indice_n(), 0);

    ESPERA_IGUAL(uso_carregar_dia(hal, &e), OK);
    ESPERA_IGUAL(e.n_itens, 0);

    TERMINA();
}

void t_a_poda_mantem_o_cartao_do_tamanho_da_janela(void)
{
    COMECA("janela · o que passou sai do cartão, menos o que é nosso");

    data_t hoje = { 2026, 9, 11 };
    aparelho(hoje);
    e.hora_confiavel = true;          // a poda só roda com hora de verdade
    ESPERA_IGUAL(indice_monta(hal), OK);

    data_t velho = data_soma_dias(hoje, -9);
    data_t ontem = data_soma_dias(hoje, -1);
    data_t longe = data_soma_dias(hoje, 20);

    poe(velho, "g:velho", "Reunião de setembro", TIPO_EVENTO);
    poe(longe, "g:longe", "Casamento", TIPO_EVENTO);
    poe(ontem, "g:ontem", "Almoço", TIPO_EVENTO);
    poe(hoje,  "g:hoje",  "Dentista", TIPO_EVENTO);

    // A anotação só existe aqui: não sai por idade.
    poe(velho, "nt:minha", "o que eu pensei", TIPO_ANOTACAO);

    // O que está ABERTO também não: o Tasks não reenvia o que não mudou.
    poe(velho, "t:sem_prazo", "Trocar o pneu", TIPO_TAREFA);
    poe(velho, "l:compras",   "Compras",       TIPO_LISTA);

    // A feita é histórico e envelhece.
    item_t feita = { 0 };
    snprintf(feita.id, sizeof feita.id, "%s", "t:paguei");
    snprintf(feita.titulo, sizeof feita.titulo, "%s", "Pagar o IPVA");
    feita.tipo = TIPO_TAREFA;
    feita.dia = feita.vence = feita.feita_em = velho;
    feita.feita = true;
    ESPERA_IGUAL(cartao_grava_item(hal, velho, &feita), OK);
    ESPERA_IGUAL(indice_monta(hal), OK);

    ESPERA_IGUAL(uso_poda_a_janela(hal, &e), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, velho, "g:velho", &lido), ERR_ARQUIVO);
    ESPERA_IGUAL(cartao_le_item(hal, longe, "g:longe", &lido), ERR_ARQUIVO);
    ESPERA_IGUAL(cartao_le_item(hal, ontem, "g:ontem", &lido), OK);
    ESPERA_IGUAL(cartao_le_item(hal, hoje,  "g:hoje",  &lido), OK);
    ESPERA_IGUAL(cartao_le_item(hal, velho, "nt:minha", &lido), OK);
    ESPERA_IGUAL(cartao_le_item(hal, velho, "t:sem_prazo", &lido), OK);
    ESPERA_IGUAL(cartao_le_item(hal, velho, "l:compras", &lido), OK);
    ESPERA_IGUAL(cartao_le_item(hal, velho, "t:paguei", &lido), ERR_ARQUIVO);

    TERMINA();
}

void t_as_marcas_do_mes_vem_do_servidor(void)
{
    COMECA("calendário · o mês vem em oito bytes, e a janela entra junto");

    data_t hoje = { 2026, 9, 11 };
    aparelho(hoje);
    ESPERA_IGUAL(indice_monta(hal), OK);

    poe(hoje, "g:hoje", "Dentista", TIPO_EVENTO);

    // O que o servidor disse do mês.
    e.marcas_srv_ano = 2026;
    e.marcas_srv_mes = 9;
    e.marcas_srv_evento = (1u << 19) | (1u << 24);
    e.marcas_srv_tarefa = (1u << 27);

    ESPERA_IGUAL(uso_carregar_marcas(hal, &e), OK);

    ESPERA(e.marcas_evento & (1u << 10));   // o de hoje, que é nosso
    ESPERA(e.marcas_evento & (1u << 19));   // e os do servidor
    ESPERA(e.marcas_evento & (1u << 24));
    ESPERA(e.marcas_tarefa & (1u << 27));

    // O pedido do mês sai junto do próximo pull.
    ESPERA_TEXTO(e.mes_pedido, "2026-09");

    // Outro mês: o que o servidor mandou não vale.
    uso_ir_para_dia(&e, (data_t){ 2026, 10, 5 });
    ESPERA_IGUAL(uso_carregar_marcas(hal, &e), OK);
    ESPERA_IGUAL((int)e.marcas_evento, 0);
    ESPERA_TEXTO(e.mes_pedido, "2026-10");

    TERMINA();
}

// Apagar leva a pasta do item junto, e o dia se ficou vazio: pasta morta
// pesaria no boot, que ainda varre o cartão.
void t_apagar_nao_deixa_pasta_para_tras(void)
{
    COMECA("cartão · apagar leva a pasta do item, e o dia se esvaziou");

    data_t hoje = { 2026, 9, 11 };
    aparelho(hoje);
    ESPERA_IGUAL(indice_monta(hal), OK);

    data_t velho = data_soma_dias(hoje, -5);
    poe(velho, "g:um", "Reunião", TIPO_EVENTO);
    poe(velho, "g:dois", "Almoço", TIPO_EVENTO);

    ESPERA(pc_tem_diretorio("/TINTO/itens/2026-09-06"));

    ESPERA_IGUAL(cartao_apaga_item(hal, velho, "g:um"), OK);
    ESPERA(!pc_tem_diretorio("/TINTO/itens/2026-09-06/g%um"));
    ESPERA(pc_tem_diretorio("/TINTO/itens/2026-09-06"));   // o outro ficou

    ESPERA_IGUAL(cartao_apaga_item(hal, velho, "g:dois"), OK);
    ESPERA(!pc_tem_diretorio("/TINTO/itens/2026-09-06"));  // e agora some

    // E a varredura do boot não paga por ele.
    ESPERA_IGUAL(indice_monta(hal), OK);
    ESPERA_IGUAL(indice_n(), 0);

    TERMINA();
}
