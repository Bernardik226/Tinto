// firmware/testes/t_navega.c — o aparelho navega.
// Nada é chamado por dentro: entra botão, sai estado.
#include "teste.h"
#include "vista/quando.h"
#include "vista/calendario.h"
#include "vista/nota.h"
#include "dado/cartao.h"
#include "uso/uso.h"
#include "vista/agenda.h"
#include "uso/nuvem.h"
#include "vista/campos.h"
#include "vista/dia.h"

static app_t       ap;
static const hal_t *hal;

#define HOJE ((data_t){2026, 8, 18})

static void poe(const char *id, const char *titulo, tipo_t tipo)
{
    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,     sizeof it.id,     "%s", id);
    snprintf(it.titulo, sizeof it.titulo, "%s", titulo);
    it.tipo = tipo;
    it.dia  = HOJE;
    cartao_grava_item(hal, HOJE, &it);
}

static void liga_com_duas_tarefas(void)
{
    hal = pc_liga();
    pc_relogio(HOJE, 9, 14);
    poe("0900-ipva", "Pagar IPVA",   TIPO_TAREFA);
    poe("0901-luz",  "Conta de luz", TIPO_TAREFA);
    app_liga(&ap, hal);
    // Liga a rede: o que se prova aqui é a navegação.
    ap.estado.rede = REDE_LIGADA;
    app_passo(&ap);

    // Liga na Home; entrar na Agenda mora aqui para a forma da Home custar uma
    // linha, não trinta.
    ENTRA_NA_AGENDA(&ap);
}

// Sem compromisso no dia, a primeira tarefa já é a linha 0.
static void desce_ate_o_trabalho(void)
{
    app_passo(&ap);
}

// O gesto mais repetido, do botão até o cartão.
void t_ok_marca_a_tarefa_do_cursor(void)
{
    COMECA("RN-37 · o OK faz a ação da linha: marca a tarefa do cursor");

    liga_com_duas_tarefas();
    desce_ate_o_trabalho();

    pc_botao(IN_OK);
    app_passo(&ap);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, HOJE, "0900-ipva", &lido), OK);
    ESPERA(lido.feita);

    TERMINA();
}

// O cache invalidado pelo caso de uso é levantado de novo pelo laço: senão
// a tela mostraria o mundo de antes do gesto.
void t_a_home_redesenha_com_o_cartao_novo(void)
{
    COMECA("depois do gesto, a home relê o cartão em vez de mentir");

    liga_com_duas_tarefas();
    desce_ate_o_trabalho();

    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA(ap.estado.itens_validos);   // o laço levantou de volta

    vista_agenda_t v;
    vista_agenda(&ap.estado, &v);
    // RN-35: a feita fica ONDE ESTAVA (senão o próximo OK acertaria outra).
    ESPERA_TEXTO(v.trabalho[0].titulo, "Pagar IPVA");
    ESPERA(v.trabalho[0].feita);
    ESPERA_TEXTO(v.trabalho[1].titulo, "Conta de luz");

    TERMINA();
}

// RN-37: o ▶ entra no item; o OK nunca faz isso.
void t_direita_entra_no_item_e_back_volta(void)
{
    COMECA("RN-37 · ▶ entra no item, e o BACK traz de volta");

    liga_com_duas_tarefas();
    desce_ate_o_trabalho();

    pc_botao(IN_DIR);
    app_passo(&ap);

    // Home → Agenda → tarefa: dois degraus.
    ESPERA_IGUAL(ap.estado.profundidade, 2);
    ESPERA_IGUAL(ap.estado.pilha[2], TELA_NOTA);

    pc_botao(IN_VOLTAR);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.profundidade, 1);
    ESPERA_IGUAL(ap.estado.pilha[1], TELA_AGENDA);

    TERMINA();
}

// Dentro de um item o ▶ não faz nada: a pilha não cresce sozinha.
void t_direita_dentro_do_item_nao_empilha(void)
{
    COMECA("o ▶ não empilha onde não há para onde entrar");

    liga_com_duas_tarefas();
    desce_ate_o_trabalho();

    for (int i = 0; i < 6; i++) {
        pc_botao(IN_DIR);
        app_passo(&ap);
    }

    ESPERA_IGUAL(ap.estado.profundidade, 2);
    ESPERA_IGUAL(ap.estado.pilha[2], TELA_NOTA);

    TERMINA();
}

// RN-14: gravando, o direcional trava.
void t_travado_nao_anda(void)
{
    COMECA("travado, o cursor não anda");

    liga_com_duas_tarefas();
    ap.estado.travado = true;

    pc_botao(IN_BAIXO);
    pc_botao(IN_BAIXO);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.cursor, 0);

    TERMINA();
}

// ── o tick não é um gesto ───────────────────────────────────────────

void t_tick_com_a_tela_parada_nao_pede_quadro(void)
{
    COMECA("o tick de 1 s não pede quadro com a tela parada");
    // Parado na Agenda, nenhum quadro por segundo: montar a vista a cada tick
    // deixava o aparelho lento no botão.
    liga_com_duas_tarefas();

    // Conta QUADROS, não a bandeira (o passo a zera no mesmo passo).
    int antes = pc_quadros();
    for (int i = 0; i < 10; i++) { pc_tick(); app_passo(&ap); }

    ESPERA(pc_quadros() == antes);
    TERMINA();
}

void t_tick_que_vira_o_minuto_pede_quadro(void)
{
    COMECA("virar o minuto pede quadro, porque o relógio está na tela");
    // A outra metade: o relógio da barra é conteúdo e tem de aparecer.
    liga_com_duas_tarefas();

    int antes = pc_quadros();
    pc_relogio(HOJE, 9, 15);
    pc_tick();
    app_passo(&ap);

    ESPERA(ap.estado.minuto == 15);
    ESPERA(pc_quadros() > antes);
    TERMINA();
}

void t_tick_gravando_pede_quadro(void)
{
    COMECA("gravando, o tick pede quadro: o contador anda");
    // O tempo de gravação anda no tick e aparece na tela.
    liga_com_duas_tarefas();
    ap.estado.rede = REDE_LIGADA;    // falar exige rede desde 25/08
    pc_segura(IN_VOZ, 900);
    app_passo(&ap);
    ESPERA(ap.estado.gravacao.fase != GRAV_PARADA);

    int antes = pc_quadros();
    pc_tick();
    app_passo(&ap);

    ESPERA(pc_quadros() > antes);
    TERMINA();
}

// A confirmação aparece ONDE a pessoa estava: vindo do dia, abrir o item
// troca o topo, e a volta caía na Home.
void t_a_confirmacao_volta_para_onde_a_pessoa_estava(void)
{
    COMECA("mudar a hora vindo do dia volta para o item, não para a Home");

    liga_com_duas_tarefas();
    ap.estado.rede = REDE_LIGADA;

    item_t ev;
    memset(&ev, 0, sizeof ev);
    snprintf(ev.id,     sizeof ev.id,     "%s", "g:fundo");
    snprintf(ev.titulo, sizeof ev.titulo, "%s", "Reunião");
    snprintf(ev.hora,   sizeof ev.hora,   "%s", "15:00");
    ev.tipo = TIPO_EVENTO; ev.dia = HOJE; ev.vence = HOJE;
    ESPERA_IGUAL(cartao_grava_item(hal, HOJE, &ev), OK);

    // A pilha CHEIA, como no caminho pelo calendário.
    ap.estado.profundidade = 0;
    ap.estado.pilha[0] = TELA_HOME;
    ap.estado.pilha[++ap.estado.profundidade] = TELA_AGENDA;
    ap.estado.pilha[++ap.estado.profundidade] = TELA_CALENDARIO;
    ap.estado.pilha[++ap.estado.profundidade] = TELA_DIA;

    ESPERA_IGUAL(uso_abrir_item(hal, &ap.estado, &ev), OK);
    ap.estado.pilha[ap.estado.profundidade] = TELA_NOTA;   // o topo trocado

    // MENU › Mudar a hora › primeira linha.
    pc_botao(IN_MENU);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_ACOES);
    pc_botao(IN_BAIXO);
    app_passo(&ap);
    pc_botao(IN_BAIXO);
    app_passo(&ap);
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_HORARIO);

    pc_nuvem_responde("{\"ok\":true,\"id\":\"g:fundo\"}");
    pc_botao(IN_OK);
    app_passo(&ap);

    // No ITEM, com o recibo por cima.
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_NOTA);
    ESPERA(ap.estado.profundidade > 0);
    ESPERA(ap.estado.feito[0]);
    pc_nuvem_responde(NULL);
    TERMINA();
}

// O MOSTRADOR marca qualquer hora, como a grade marca qualquer dia.
void t_o_mostrador_marca_qualquer_hora(void)
{
    COMECA("mudar a hora tem forma livre, e ◀▶ troca a casa");

    liga_com_duas_tarefas();
    ap.estado.rede = REDE_LIGADA;

    item_t ev;
    memset(&ev, 0, sizeof ev);
    snprintf(ev.id,     sizeof ev.id,     "%s", "g:reuniao");
    snprintf(ev.titulo, sizeof ev.titulo, "%s", "Reunião");
    snprintf(ev.hora,   sizeof ev.hora,   "%s", "15:00");
    ev.tipo = TIPO_EVENTO; ev.origem = ORIGEM_GOOGLE;
    ev.dia = HOJE; ev.vence = HOJE;
    ESPERA_IGUAL(cartao_grava_item(hal, HOJE, &ev), OK);
    ESPERA_IGUAL(uso_abrir_item(hal, &ap.estado, &ev), OK);
    ap.estado.pilha[++ap.estado.profundidade] = TELA_HORARIO;
    ap.estado.cursor = 0;

    vista_quando_t q;
    vista_horario(&ap.estado, &q);
    int livre = -1;
    for (int i = 0; i < q.cartao.n_dest; i++)
        if (q.acao[i] == QUANDO_RELOGIO) livre = i;
    ESPERA(livre >= 0);

    for (int i = 0; i < livre; i++) { pc_botao(IN_BAIXO); app_passo(&ap); }
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_ESCOLHER_HORA);

    // Abre na hora que a coisa já tem.
    ESPERA_IGUAL(ap.estado.escolha_h, 15);
    ESPERA_IGUAL(ap.estado.escolha_m, 0);

    // ▲ sobe a hora; ▶ troca para o minuto, de cinco em cinco.
    pc_botao(IN_CIMA);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.escolha_h, 16);

    pc_botao(IN_DIR);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.escolha_campo, 1);

    pc_botao(IN_CIMA);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.escolha_m, 5);

    // O OK marca 16:05. A resposta vem com o MESMO id (outra resposta
    // renomearia o item).
    pc_nuvem_responde("{\"ok\":true,\"id\":\"g:reuniao\"}");
    pc_botao(IN_OK);
    app_passo(&ap);

    item_t lido;
    ESPERA_IGUAL(cartao_acha_item(hal, "g:reuniao", &lido, NULL), OK);
    ESPERA_TEXTO(lido.hora, "16:05");
    ESPERA(ap.estado.pilha[ap.estado.profundidade] != TELA_ESCOLHER_HORA);

    // A resposta armada sai do caminho: senão vira a do próximo teste.
    pc_nuvem_responde(NULL);
    TERMINA();
}

// O HÍBRIDO vive na régua e se marca, na Agenda e no dia.
void t_tarefa_com_hora_vive_na_regua_e_se_marca(void)
{
    COMECA("tarefa com hora fica na régua do dia, com caixa, e se marca");

    liga_com_duas_tarefas();
    ap.estado.rede = REDE_LIGADA;

    // Uma tarefa com prazo de hoje e hora.
    item_t h;
    memset(&h, 0, sizeof h);
    snprintf(h.id,     sizeof h.id,     "%s", "t:academia");
    snprintf(h.titulo, sizeof h.titulo, "%s", "Academia");
    snprintf(h.hora,   sizeof h.hora,   "%s", "07:00");
    h.tipo = TIPO_TAREFA; h.origem = ORIGEM_GOOGLE;
    h.dia = HOJE; h.vence = HOJE;
    ESPERA_IGUAL(cartao_grava_item(hal, HOJE, &h), OK);
    estado_invalida_cartao(&ap.estado);
    pc_tick();
    app_passo(&ap);

    // ── na AGENDA ──
    vista_agenda_t vh;
    vista_agenda(&ap.estado, &vh);

    int na_regua = -1;
    for (int i = 0; i < vh.n_agenda; i++)
        if (strstr(vh.agenda[i].oque, "Academia")) na_regua = i;
    ESPERA(na_regua >= 0);
    ESPERA(vh.agenda[na_regua].caixa);
    ESPERA(!vh.agenda[na_regua].feita);

    // E NÃO aparece também nas tarefas: seria a mesma duas vezes.
    for (int i = 0; i < vh.n_trabalho; i++)
        ESPERA(!strstr(vh.trabalho[i].titulo, "Academia"));

    // ── no DIA que o calendário abre ──
    vista_dia_t vd;
    vista_dia(&ap.estado, &vd);

    int no_dia = -1;
    for (int i = 0; i < vd.n_compromissos; i++)
        if (strstr(vd.compromissos[i].titulo, "Academia")) no_dia = i;
    ESPERA(no_dia >= 0);
    ESPERA(vd.compromissos[no_dia].caixa);

    for (int i = 0; i < vd.n_tarefas; i++)
        ESPERA(!strstr(vd.tarefas[i].titulo, "Academia"));

    // ── e ela SE MARCA ──
    ESPERA_IGUAL(uso_marcar(hal, &ap.estado, &h, true), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, HOJE, "t:academia", &lido), OK);
    ESPERA(lido.feita);

    // Riscada onde está (RN-35).
    estado_invalida_cartao(&ap.estado);
    pc_tick();
    app_passo(&ap);
    vista_dia(&ap.estado, &vd);
    no_dia = -1;
    for (int i = 0; i < vd.n_compromissos; i++)
        if (strstr(vd.compromissos[i].titulo, "Academia")) no_dia = i;
    ESPERA(no_dia >= 0);
    ESPERA(vd.compromissos[no_dia].feita);
    TERMINA();
}

// Escolher no mês é OUTRA rota: o OK carimba a data em vez de abrir o dia.
void t_escolher_no_mes_tem_rota_propria(void)
{
    COMECA("escolher no mês é tela própria, e o OK marca em vez de abrir");

    liga_com_duas_tarefas();
    ap.estado.rede = REDE_LIGADA;
    desce_ate_o_trabalho();

    pc_botao(IN_DIR);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_NOTA);

    char id[40];
    snprintf(id, sizeof id, "%s", ap.estado.aberto.id);

    // MENU › Pôr data › Escolher no mês.
    pc_botao(IN_MENU);
    app_passo(&ap);
    pc_botao(IN_BAIXO);
    app_passo(&ap);
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_QUANDO);

    vista_quando_t q;
    vista_quando(&ap.estado, &q);
    int linha = -1;
    for (int i = 0; i < q.cartao.n_dest; i++)
        if (q.acao[i] == QUANDO_CALENDARIO) linha = i;
    ESPERA(linha >= 0);

    for (int i = 0; i < linha; i++) { pc_botao(IN_BAIXO); app_passo(&ap); }
    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_ESCOLHER_DIA);

    // A tira diz o dia que vai ser marcado.
    vista_cal_t vc;
    vista_escolher_dia(&ap.estado, &vc);
    ESPERA(vc.titulo[0]);
    ESPERA(vc.n_tira == 1);
    ESPERA(strstr(vc.rodape_dir, "marcar") != NULL);

    // Anda um dia e marca.
    pc_botao(IN_DIR);
    app_passo(&ap);
    data_t escolhido = ap.estado.dia_visto;

    pc_botao(IN_OK);
    app_passo(&ap);

    item_t lido;
    ESPERA_IGUAL(cartao_acha_item(hal, id, &lido, NULL), OK);
    ESPERA(data_igual(lido.vence, escolhido));

    // E volta para o ITEM.
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_NOTA);
    TERMINA();
}

// Da tela do item até a data nova, com a mão: quatro toques.
void t_mudar_a_data_pela_mao(void)
{
    COMECA("mudar a data · do item até o dia novo, sem falar");

    liga_com_duas_tarefas();
    ap.estado.rede = REDE_LIGADA;
    desce_ate_o_trabalho();

    pc_botao(IN_DIR);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_NOTA);

    data_t antes = ap.estado.aberto.vence;
    char id[40];
    snprintf(id, sizeof id, "%s", ap.estado.aberto.id);

    pc_botao(IN_MENU);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_ACOES);
    pc_botao(IN_BAIXO);
    app_passo(&ap);
    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_QUANDO);

    pc_nuvem_historico_zera();
    pc_botao(IN_OK);
    app_passo(&ap);

    item_t lido;
    ESPERA_IGUAL(cartao_acha_item(hal, id, &lido, NULL), OK);
    ESPERA(!data_igual(lido.vence, antes));
    ESPERA(data_igual(lido.vence, data_soma_dias(ap.estado.hoje, 1)));

    // Volta ao DETALHE com o item vivo e o recibo por cima (caía em "em
    // construção").
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_NOTA);
    ESPERA(ap.estado.aberto_valido);
    ESPERA(data_igual(ap.estado.aberto.vence, lido.vence));
    ESPERA_TEXTO(ap.estado.feito, "Remarcado");

    pc_botao(IN_VOLTAR);
    app_passo(&ap);
    ESPERA(!ap.estado.feito[0]);

    ESPERA(strstr(pc_nuvem_historico(), "/v1/push") != NULL);
    ESPERA(strstr(pc_nuvem_historico(), "editou") != NULL);

    ESPERA(ap.estado.pilha[ap.estado.profundidade] != TELA_QUANDO);
    TERMINA();
}

// Sem rede, o gesto ACONTECE e espera no cartão, como no calendário do
// celular. A recusa fica para falar (precisa de Whisper e LLM).
void t_gesto_sem_rede_acontece_e_espera_na_fila(void)
{
    COMECA("sem rede, marcar feita acontece e o gesto espera no cartão");

    liga_com_duas_tarefas();
    ap.estado.rede = REDE_DESLIGADA;
    desce_ate_o_trabalho();

    pc_nuvem_historico_zera();
    pc_botao(IN_OK);
    app_passo(&ap);

    // Nenhuma faixa: nada foi recusado.
    ESPERA(!ap.estado.precisa_rede[0]);

    // O cartão já mudou.
    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, HOJE, "0900-ipva", &lido), OK);
    ESPERA(lido.feita);

    // Guardado, esperando a rede.
    ESPERA_IGUAL(ap.estado.gesto_n, 1);
    ESPERA(!strstr(pc_nuvem_historico(), "/v1/push"));

    // A rede volta e ele sobe sozinho; a resposta vai armada porque o pedido é
    // que empurra o evento.
    pc_nuvem_responde("{\"ok\":true,\"id\":\"0900-ipva\"}");
    ap.estado.rede = REDE_LIGADA;
    pc_avanca_ms(1000);
    pc_tick();
    app_passo(&ap);

    ESPERA(strstr(pc_nuvem_historico(), "/v1/push") != NULL);
    ESPERA_CONTEM(pc_nuvem_corpo(), "0900-ipva");

    // O arquivo só sai porque o servidor confirmou.
    ESPERA_IGUAL(ap.estado.gesto_n, 0);
    TERMINA();
}


// Voltar do calendário devolve a Agenda de hoje, não o dia distante.
void t_voltar_do_calendario_devolve_a_home_de_hoje(void)
{
    COMECA("voltar de outro dia devolve a home de HOJE, com os itens");

    liga_com_duas_tarefas();
    app_passo(&ap);

    vista_agenda_t v;
    vista_agenda(&ap.estado, &v);
    ESPERA_IGUAL(v.n_trabalho, 2);

    // Com o cabeçalho selecionado, ◀▶ trocam o dia, sem empilhar.
    pc_botao(IN_ESQ);
    app_passo(&ap);
    pc_botao(IN_DIR);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_AGENDA);
    ESPERA(!data_igual(ap.estado.dia_visto, ap.estado.hoje));

    pc_botao(IN_ESQ);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_AGENDA);
    ESPERA(data_igual(ap.estado.dia_visto, ap.estado.hoje));

    vista_agenda(&ap.estado, &v);
    ESPERA_IGUAL(v.n_trabalho, 2);

    TERMINA();
}


// Dia vazio não anuncia seção: um título sobre o vazio parece carga que
// falhou.
void t_dia_futuro_vazio_nao_anuncia_secao(void)
{
    COMECA("dia vazio: só a data e a marca d'água, sem rótulo de seção");

    liga_com_duas_tarefas();
    app_passo(&ap);

    pc_botao(IN_ESQ);            // solta o cursor
    app_passo(&ap);
    pc_botao(IN_DIR);            // amanhã, que está vazio
    app_passo(&ap);

    vista_dia_t v;
    vista_dia(&ap.estado, &v);

    ESPERA_IGUAL(v.n_compromissos, 0);
    ESPERA_IGUAL(v.n_tarefas, 0);
    ESPERA_TEXTO(v.rotulo, "");
    ESPERA_TEXTO(v.quando, "");
    ESPERA(v.vazio[0] != '\0');

    // Nem linha para o cursor.
    ESPERA_IGUAL(vista_dia_linhas(&ap.estado), 0);

    TERMINA();
}

// Com conteúdo, o rótulo volta e diz o quanto falta.
void t_dia_futuro_com_compromisso_diz_quando(void)
{
    COMECA("dia futuro com compromisso: o rótulo diz que falta um dia");

    hal = pc_liga();
    pc_relogio(HOJE, 9, 14);

    data_t amanha = data_soma_dias(HOJE, 1);
    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,     sizeof it.id,     "%s", "g:abc");
    snprintf(it.titulo, sizeof it.titulo, "%s", "Dentista");
    snprintf(it.hora,   sizeof it.hora,   "%s", "15:00");
    it.tipo   = TIPO_EVENTO;
    it.origem = ORIGEM_GOOGLE;
    it.dia    = amanha;
    ESPERA_IGUAL(cartao_grava_item(hal, amanha, &it), OK);

    app_liga(&ap, hal);
    app_passo(&ap);
    ENTRA_NA_AGENDA(&ap);

    pc_botao(IN_ESQ);
    app_passo(&ap);
    pc_botao(IN_DIR);
    app_passo(&ap);

    vista_dia_t v;
    vista_dia(&ap.estado, &v);

    ESPERA_IGUAL(v.n_compromissos, 1);
    ESPERA_TEXTO(v.compromissos[0].titulo, "Dentista");
    ESPERA_TEXTO(v.rotulo, "Vai acontecer");
    ESPERA_TEXTO(v.quando, "amanhã");
    ESPERA_TEXTO(v.vazio, "");

    TERMINA();
}

// UMA tela de detalhe para todo tipo: o tipo decide o conteúdo, não a
// tela.
void t_um_so_detalhe_para_todo_tipo(void)
{
    COMECA("tarefa, evento e anotação abrem o mesmo detalhe");

    liga_com_duas_tarefas();
    desce_ate_o_trabalho();

    pc_botao(IN_DIR);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_NOTA);
    ESPERA_IGUAL(ap.estado.aberto.tipo, TIPO_TAREFA);
    TERMINA();
}

// O detalhe da tarefa responde: o foco anda, concluir muda o estado na
// hora (relendo a cópia `aberto`), e a gaveta abre.
void t_o_detalhe_da_tarefa_responde(void)
{
    COMECA("no detalhe da tarefa os botões andam e o estado muda na hora");

    liga_com_duas_tarefas();
    desce_ate_o_trabalho();
    pc_botao(IN_DIR);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_NOTA);

    // Criada por voz; a transcrição desce no corpo.
    snprintf(ap.estado.aberto.nota, sizeof ap.estado.aberto.nota,
             "%s", "0712-fala");

    // ── 1. o botão está em foco, e o foco não escapa ──
    vista_nota_t v;
    vista_nota(&ap.estado, &v);
    ESPERA_IGUAL(v.n_botoes, 1);
    ESPERA_IGUAL(v.botao_foco, 0);

    pc_botao(IN_BAIXO);
    app_passo(&ap);
    vista_nota(&ap.estado, &v);
    ESPERA_IGUAL(v.botao_foco, 0);

    pc_botao(IN_CIMA);
    app_passo(&ap);
    vista_nota(&ap.estado, &v);
    ESPERA_IGUAL(v.botao_foco, 0);

    // ── 3. a gaveta abre pelo MENU (o ▶ é "entra no item"), e o apagar está
    // lá ──
    pc_botao(IN_MENU);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_ACOES);
    pc_botao(IN_MENU);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_NADA);

    // ── 2. concluir muda o estado NA HORA ──
    ESPERA(!ap.estado.aberto.feita);
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA(ap.estado.aberto.feita);

    vista_nota(&ap.estado, &v);
    bool diz_concluida = false;
    for (int i = 0; i < v.n_campos; i++)
        if (strstr(v.campos[i].valor, "Concluída")) diz_concluida = true;
    ESPERA(diz_concluida);
    TERMINA();
}

// DESMARCAR desmarca, não abre a fala: o despacho olhava a primeira
// letra do rótulo.
void t_desmarcar_nao_abre_a_fala(void)
{
    COMECA("o botão de desmarcar desmarca, e não abre a fala original");

    liga_com_duas_tarefas();
    desce_ate_o_trabalho();
    pc_botao(IN_DIR);
    app_passo(&ap);

    // Criada por voz e no CARTÃO: marcar relê o item de lá.
    item_t com_fala = ap.estado.aberto;
    snprintf(com_fala.nota, sizeof com_fala.nota, "%s", "0712-fala");
    ESPERA_IGUAL(cartao_grava_item(hal, com_fala.dia, &com_fala), OK);
    ESPERA_IGUAL(uso_abrir_item(hal, &ap.estado, &com_fala), OK);

    // Marca...
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA(ap.estado.aberto.feita);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_NOTA);

    // ...e desmarca, sem sair da tela.
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA(!ap.estado.aberto.feita);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_NOTA);

    // A fala continua alcançável no próprio item.
    vista_nota_t vn;
    vista_nota(&ap.estado, &vn);
    ESPERA(vn.tem_fala);

    pc_botao(IN_BAIXO);
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_NOTA);
    TERMINA();
}

// O índice aponta para a fonte certa (`pendentes`, não `itens`): resolver
// no array errado abre o item errado sem erro nenhum.
void t_abrir_tarefa_pela_lista_abre_a_tarefa_certa(void)
{
    COMECA("abrir a tarefa da lista abre justamente ela");

    hal = pc_liga();
    pc_relogio(HOJE, 9, 14);

    // Faladas em outro dia, vencendo hoje: `itens` e `pendentes` divergem.
    data_t agosto = { 2026, 8, 12 };
    for (int i = 0; i < 2; i++) {
        item_t t;
        memset(&t, 0, sizeof t);
        snprintf(t.id,     sizeof t.id,     "p%d", i);
        snprintf(t.titulo, sizeof t.titulo, "tarefa %d", i);
        t.tipo  = TIPO_TAREFA;
        t.dia   = agosto;
        t.vence = HOJE;
        ESPERA_IGUAL(cartao_grava_item(hal, agosto, &t), OK);
    }

    // Um EVENTO hoje ocupa `itens[0]`; sem ele os arrays coincidiriam.
    item_t ev;
    memset(&ev, 0, sizeof ev);
    snprintf(ev.id,     sizeof ev.id,     "%s", "ev");
    snprintf(ev.titulo, sizeof ev.titulo, "%s", "Dentista");
    snprintf(ev.hora,   sizeof ev.hora,   "%s", "14:00");
    ev.tipo = TIPO_EVENTO;
    ev.dia  = HOJE;
    ESPERA_IGUAL(cartao_grava_item(hal, HOJE, &ev), OK);

    app_liga(&ap, hal);
    ap.estado.rede = REDE_LIGADA;
    app_passo(&ap);

    ap.estado.dia_visto = ap.estado.hoje;
    ESPERA_IGUAL(uso_carregar_pendentes(hal, &ap.estado), OK);
    ESPERA_IGUAL(ap.estado.n_pendentes, 2);

    // Cursor na primeira tarefa, depois do compromisso.
    ap.estado.pilha[++ap.estado.profundidade] = TELA_DIA;
    ap.estado.cursor = 1;

    pc_botao(IN_DIR);
    app_passo(&ap);

    // O ID EXATO: "outra tarefa" continua sendo uma tarefa.
    vista_dia_t vd;
    vista_dia(&ap.estado, &vd);
    ESPERA_IGUAL(vd.n_tarefas, 2);

    ESPERA(ap.estado.aberto_valido);
    ESPERA_TEXTO(ap.estado.aberto.id,
                 ap.estado.pendentes[vd.tarefas[0].indice].id);
    TERMINA();
}

// A ação de fundo aparece na barra, e anima: um símbolo parado não diz que
// algo está a caminho.
void t_o_gesto_em_voo_aparece_na_barra(void)
{
    COMECA("renomear mostra na barra que a ação está a caminho");

    liga_com_duas_tarefas();
    desce_ate_o_trabalho();
    pc_botao(IN_DIR);
    app_passo(&ap);

    pc_nuvem_demora(true);          // segura a resposta: é a janela

    // Renomear: MENU, a primeira ação, OK.
    pc_botao(IN_MENU);
    app_passo(&ap);
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_TECLADO);

    ap.estado.nuvem_esperando = NUVEM_GESTO;
    ap.estado.sinc = SINC_ENVIANDO;

    ESPERA_IGUAL(vista_sinc_da_barra(&ap.estado), ICO_SUBINDO);

    // Sai quadro a cada tick enquanto há algo em voo, decidido pelo
    // `app_passo` (forçar a pintura aqui fazia o teste passar com o defeito).
    int antes = pc_pinturas(PINTURA_MESMA_TELA);
    for (int i = 0; i < 3; i++) { pc_tick(); app_passo(&ap); }
    ESPERA(pc_pinturas(PINTURA_MESMA_TELA) > antes);

    pc_nuvem_demora(false);
    TERMINA();
}

// Travar não é voltar para o começo: acordar devolve a tela e a linha.
void t_destravar_devolve_a_tela_em_que_estava(void)
{
    COMECA("o power devolve a tela de onde saiu, e não a Agenda");

    liga_com_duas_tarefas();
    ENTRA_NOS_AJUSTES(&ap);

    tela_id onde = ap.estado.pilha[ap.estado.profundidade];
    int8_t  fundo = ap.estado.profundidade;
    int16_t linha = ap.estado.cursor;
    ESPERA(onde != TELA_HOME);

    pc_botao(IN_POWER);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_BLOQUEADA);
    ESPERA(ap.estado.travado);

    pc_botao(IN_POWER);
    app_passo(&ap);

    ESPERA(!ap.estado.travado);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], onde);
    ESPERA_IGUAL(ap.estado.profundidade, fundo);

    // O cursor junto.
    ESPERA_IGUAL(ap.estado.cursor, linha);

    TERMINA();
}
