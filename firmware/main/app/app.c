#include "app.h"
#include "../dado/indice.h"
#include "tela/motor.h"
#include "tela/mapa.h"
#include "../tela/texto.h"
#include "../nucleo/data.h"
#include "../uso/uso.h"
#include "../uso/inicializacao.h"
#include "../uso/nuvem.h"
#include "../dado/log.h"
#include "../dado/rede.h"
#include "../dado/cartao.h"
#include "../dado/perfil.h"
#include "../vista/agenda.h"
#include "../vista/vazia.h"
#include "../vista/nota.h"
#include "../vista/dia.h"
#include "../vista/calendario.h"
#include "../vista/menu.h"
#include "../vista/gravador.h"
#include "../vista/sobre.h"
#include "../vista/fala.h"
#include "../vista/wifi.h"
#include "../vista/armazenamento.h"
#include "../vista/conta.h"
#include "../vista/vincular.h"
#include "../vista/confirma.h"
#include "../vista/relogio.h"
#include "../vista/bloqueio.h"
#include "../vista/conferir.h"
#include "../vista/quando.h"
#include "../ui/hora.h"
#include "../ui/acervo.h"
#include "../ui/obra.h"
#include "../ui/leitor.h"
#include "../uso/acervo.h"
#include "../dado/acervo.h"
#include "../dado/xadrez.h"
#include "../nucleo/leitor.h"
#include "../vista/teclado.h"
#include "../ui/agenda.h"
#include "../ui/lancador.h"
#include "../ui/resultado.h"
#include "../ui/vazia.h"
#include "../ui/nota.h"
#include "../ui/dia.h"
#include "../ui/calendario.h"
#include "../ui/menu.h"
#include "../ui/ajustes.h"
#include "../ui/cartao.h"
#include "../vista/conexao.h"
#include "../vista/dock.h"
#include "../ui/dock.h"
#include "../ui/voz.h"
#include "../ui/anotacoes.h"
#include "../ui/vincular.h"
#include "../ui/confirma.h"
#include "../ui/gravador.h"
#include "../ui/fala.h"
#include "../ui/relogio.h"
#include "../ui/bloqueio.h"
#include "../ui/conferir.h"
#include "../ui/teclado.h"
#include "../ui/inicializacao.h"
#include "../vista/inicializacao.h"
#include "../ui/xadrez.h"
#include "../vista/xadrez.h"
#include "../ui/chrome.h"
#include <stdio.h>
#include <string.h>

// Diagnóstico: quanto cada chamada segura a task APP.
#define CRONOMETRA(nome, chamada) do {                                    \
    uint32_t _t0 = ap->hal->agora_ms ? ap->hal->agora_ms() : 0;           \
    chamada;                                                              \
    uint32_t _dt = (ap->hal->agora_ms ? ap->hal->agora_ms() : 0) - _t0;   \
    if (_dt > 200 && ap->hal->registrar) {                                \
        char _m[64];                                                      \
        snprintf(_m, sizeof _m, "%s levou %u ms", nome, (unsigned)_dt);   \
        ap->hal->registrar("perf", _m);                                   \
    }                                                                     \
} while (0)

// app/ decide, e só: qual tela está aberta, onde está o cursor e o que cada
// botão faz agora. Ler é de uso/ → dado/, pintar de ui/, formatar de vista/.
static void atualiza_pagina(app_t *ap);
static void ok_na_obra(app_t *ap);
static void voz_volta(estado_t *e);

// Quanto a memória tem e quanto já foi. Varre a FAT: ao ligar e ao ENTRAR
// em Ajustes, nunca por quadro.
// O que cada parte da árvore ocupa: só a tela de Armazenamento manda medir.
static void mede_o_que_ocupa(app_t *ap)
{
    memset(&ap->estado.uso, 0, sizeof ap->estado.uso);
    if (!ap->hal->uso_de) return;

    (void)ap->hal->uso_de("/TINTO/itens",   &ap->estado.uso.itens_kb);
    (void)ap->hal->uso_de("/TINTO/acervo",  &ap->estado.uso.acervo_kb);
    (void)ap->hal->uso_de("/TINTO/sistema", &ap->estado.uso.sistema_kb);
}

static void mede_a_memoria(app_t *ap)
{
    uint32_t usado = 0, total = 0;
    if (ap->hal->espaco && ap->hal->espaco(&usado, &total) == OK) {
        ap->estado.espaco_usado_kb = usado;
        ap->estado.espaco_total_kb = total;
    } else {
        // Memória que sumiu para de mostrar o tamanho que tinha.
        ap->estado.espaco_usado_kb = ap->estado.espaco_total_kb = 0;
    }
}

// ── o primeiro uso é uma tela da pilha ──────────────────────────────
// TELA_INICIO está na pilha (ou na guardada pela trava) enquanto a fase não
// é INICIO_HOME, e só então. Quem muda a fase não mexe na pilha: esta regra,
// rodada a cada evento e quadro, assenta uma na outra. Só o Restaurar troca
// o topo à mão (o "não" dele volta para Armazenamento).
static void assenta_inicio(estado_t *e)
{
    bool travada = e->pilha[e->profundidade] == TELA_BLOQUEADA;
    const tela_id *p = travada ? e->pilha_travada : e->pilha;
    int prof         = travada ? e->profundidade_travada : e->profundidade;

    bool na_pilha = false;
    for (int i = 0; i <= prof; i++) na_pilha |= p[i] == TELA_INICIO;
    bool no_inicio = e->inicio.fase != INICIO_HOME;
    if (na_pilha == no_inicio) return;

    e->pilha[0]     = no_inicio ? TELA_INICIO : TELA_HOME;
    e->profundidade = 0;
    e->cursor       = 0;
    e->overlay      = OVERLAY_NADA;
    e->travado      = false;
}

void app_liga(app_t *ap, const hal_t *hal)
{
    memset(ap, 0, sizeof *ap);
    ap->ultima_pagina_xadrez = UINT8_MAX;
    ap->hal = hal;
    bitmap_liga(&ap->tela, ap->memoria_tela, TELA_L, TELA_A);

    // O chão do sistema é a Home 2x2; a Agenda é o primeiro cartão.
    ap->estado.pilha[0]     = TELA_HOME;
    ap->estado.profundidade = 0;
    ap->estado.bateria      = 100;
    ap->estado.rede         = REDE_DESLIGADA;
    // Ligar conta como toque: o prazo do bloqueio começa aqui.
    if (hal->agora_ms) ap->estado.ultimo_toque_ms = hal->agora_ms();

    int h = 0, m = 0;
    hal->relogio(&ap->estado.hoje, &h, &m);
    ap->estado.hora   = (int8_t)h;
    ap->estado.minuto = (int8_t)m;

    ap->estado.dia_visto = ap->estado.hoje;
    ap->estado.docado    = hal->docado();

    // RN-A3: sem cartão o aparelho diz isso, em vez de uma Agenda vazia.
    // RN-68: cartão sem /TINTO/ cria a árvore e segue. RN-6A: a memória decide
    // se existe aparelho; o caso de uso devolve a fase raiz.
    // A fila de gestos é relida aqui: recoloca as lápides antes do primeiro
    // pull, que senão traria de volta o que foi apagado.
    (void)uso_gestos_do_cartao(hal, &ap->estado);

    erro_t err = uso_configurar_dispositivo(hal, &ap->estado,
                                            INICIO_CMD_BOOT, NULL);
    if (err != OK) ap->estado.ultimo_erro = err;

    // Os Ajustes valem desde o primeiro uso: sem eles a hora pela rede ficava
    // desligada no onboarding e o aparelho não se apresentava ao servidor.
    (void)uso_carregar_config(hal, &ap->estado);

    // A rede conhecida volta sozinha. O fuso guardado vale desde o boot: sem
    // ele o aparelho acorda em UTC até o pull chegar.
    if (hal->fuso) hal->fuso(ap->estado.config.valor[AJUSTE_FUSO_MIN]);

    rede_salva_t guardada;
    if (rede_carrega(hal, &guardada) == OK) {
        snprintf(ap->estado.wifi_salva, sizeof ap->estado.wifi_salva,
                 "%s", guardada.nome);
        if (hal->wifi_conectar) {
            snprintf(ap->estado.wifi_alvo, sizeof ap->estado.wifi_alvo,
                     "%s", guardada.nome);
            ap->estado.rede = REDE_CONECTANDO;
            ap->estado.conectando_desde_ms = ap->hal->agora_ms();
            (void)hal->wifi_conectar(guardada.nome, guardada.senha);
        }
    }

    if (ap->estado.inicio.fase != INICIO_HOME) {
        ap->precisa_desenhar = true;
        return;
    }

    // ── o ÍNDICE, antes do primeiro quadro ──────────────────────────────
    // A única varredura do cartão; daqui em diante toda pergunta de tela é
    // respondida na RAM. Falhar não é agenda vazia: `indice_pronto()` fica falso
    // e quem pergunta recebe erro.
    if (indice_monta(hal) != OK && ap->estado.ultimo_erro == OK)
        ap->estado.ultimo_erro = ERR_ARQUIVO;

    // E o cartão volta ao tamanho da janela (dias velhos de um aparelho que
    // ficou desligado).
    (void)uso_poda_a_janela(hal, &ap->estado);

    mede_a_memoria(ap);

    (void)dado_log(hal, LOG_LIGOU, 0, ap->estado.hora, ap->estado.minuto);

    ap->precisa_desenhar = true;
}

// Quantas linhas o cursor visita AGORA: quem sabe é a vista. A lista de
// AÇÃO é mais baixa que a de captura (uma linha, sem subtítulo).
static int cabe_acoes(const bitmap_t *bm)
{
    int sobra = bm->a - BARRA_A - RODAPE_A - 20;
    int n = sobra / (gfx_altura_linha(F_CORPO) + 7);
    return n < 1 ? 1 : n;
}

static int linhas_da_tela(app_t *ap)
{
    switch (ap->estado.pilha[ap->estado.profundidade]) {
    case TELA_AGENDA:
        return vista_agenda_linhas(&ap->estado);
    case TELA_DIA:
        return vista_dia_linhas(&ap->estado);
    case TELA_CONFERIR:
        return vista_recibo_linhas(&ap->estado);
    case TELA_ARMAZENAMENTO: {
        // Uma parada só, o Restaurar: as linhas de uso informam.
        vista_cartao_t v;
        vista_armazenamento(&ap->estado, &v);
        return v.n_dest > 0 ? v.n_dest : 1;
    }
    case TELA_SOBRE: {
        vista_cartao_t v;
        vista_sobre(&ap->estado, &v);
        return v.n_dest > 0 ? v.n_dest : 1;
    }
    // Sem esta linha a tela declarava zero paradas e o ▼ não andava.
    case TELA_QUANDO: {
        vista_quando_t v;
        vista_quando(&ap->estado, &v);
        return v.cartao.n_dest > 0 ? v.cartao.n_dest : 1;
    }
    case TELA_HORARIO: {
        vista_quando_t v;
        vista_horario(&ap->estado, &v);
        return v.cartao.n_dest > 0 ? v.cartao.n_dest : 1;
    }
    case TELA_ACERVO: {
        vista_acervo_t v;
        vista_acervo(&ap->estado, &v);
        int paradas = v.n + (v.tem_destaque ? 1 : 0);
        return paradas > 0 ? paradas : 1;
    }
    case TELA_OBRA: {
        vista_obra_t v;
        vista_obra(&ap->estado, &v);
        return tela_obra_paradas(&ap->tela, &v);
    }
    // O leitor não tem lista: ◀▶ viram página.
    case TELA_LEITOR:
        return 1;
    case TELA_DATA_HORA:
        return 1;   // tela de campos: o cursor não é de linha
    case TELA_CONTA:
        return vista_conta_paradas(&ap->estado);
    case TELA_SINCRONIZACAO:
        return vista_sincronizacao_paradas(&ap->estado);
    case TELA_WIFI: {
        if (!ap->estado.wifi_lista) return vista_conexao_paradas(&ap->estado);
        vista_menu_t v;
        vista_wifi(&ap->estado, cabe_acoes(&ap->tela), &v);
        return v.n;
    }
    case TELA_NOTA: {
        // As paradas de LEITURA e depois os botões; quem conta é a tela (altura é
        // geometria).
        vista_nota_t v;
        vista_nota(&ap->estado, &v);
        return tela_nota_paradas(&ap->tela, &v);
    }
    case TELA_RESULTADO: {
        vista_resultado_t v;
        vista_resultado(&ap->estado, &v);
        return tela_resultado_paradas(&ap->tela, &v);
    }
    case TELA_ANOTACOES:
        return vista_anotacoes_linhas(&ap->estado);
    case TELA_FALA:
    case TELA_TECLADO:
        return 1;
    case TELA_AJUSTES: {
        vista_menu_t v;
        vista_ajustes(&ap->estado, cabe_acoes(&ap->tela), &v);
        return v.n;
    }
    case TELA_APARENCIA:
        return vista_aparencia_paradas(&ap->estado);
    case TELA_SOM:
        return vista_som_paradas(&ap->estado);
    default:
        return 1;
    }
}

// No calendário o ◀▶ anda de DIA; o mês vira sozinho na borda.
// `data_soma_dias` cuida de mês, ano e bissexto.
static void anda_dia(app_t *ap, int dias)
{
    uso_ir_para_dia(&ap->estado,
                    data_soma_dias(ap->estado.dia_visto, dias));
}

// ── o teclado ────────────────────────────────────────────────────────
// Direcional próprio com WRAP-AROUND: senão uma senha custaria cinquenta
// cliques.
static void anda_teclado(estado_t *e, int dl, int dc)
{
    e->teclado_lin = (int8_t)((e->teclado_lin + dl + TEC_LINS) % TEC_LINS);
    e->teclado_col = (int8_t)((e->teclado_col + dc + TEC_COLS) % TEC_COLS);
}

// Declarada aqui: o teclado precisa dela, e ela mora junto do `empilha`.
static void troca_topo(estado_t *e, tela_id t);

static void ok_no_teclado(app_t *ap)
{
    estado_t *e = &ap->estado;
    vista_teclado_t v;
    vista_teclado(e, &v);

    const char *t = v.teclas[e->teclado_lin][e->teclado_col];
    if (!t[0]) return;

    size_t n = strlen(e->digitando);

    if      (strcmp(t, "ABC") == 0) e->teclado_modo = (int8_t)((e->teclado_modo + 1) % 2);
    else if (strcmp(t, "123") == 0) e->teclado_modo = 2;
    else if (strcmp(t, "esp") == 0) { if (n + 1 < sizeof e->digitando) { e->digitando[n] = ' '; e->digitando[n+1] = '\0'; } }
    else if (strcmp(t, "del") == 0) { if (n) e->digitando[n - 1] = '\0'; }
    else if (strcmp(t, "ok")  == 0) {
        // O contexto decide o destino do texto.
        // O nome da rede oculta vira o alvo, e o MESMO teclado volta pedindo a
        // senha: dois teclados em sequência, não um formulário.
        if (e->teclado_contexto == TECLADO_WIFI_SSID) {
            if (!e->digitando[0]) return;
            snprintf(e->wifi_alvo, sizeof e->wifi_alvo, "%s", e->digitando);
            e->digitando[0] = '\0';
            e->teclado_contexto = TECLADO_WIFI;
            e->teclado_lin = e->teclado_col = 0;
            return;
        }

        // A senha vai ao rádio e NÃO fica no estado.
        if (e->teclado_contexto == TECLADO_WIFI) {
            e->rede = REDE_CONECTANDO;
        e->conectando_desde_ms = ap->hal->agora_ms();
            e->conectando_desde_ms = ap->hal->agora_ms();
            if (ap->hal->wifi_conectar)
                (void)ap->hal->wifi_conectar(e->wifi_alvo, e->digitando);

            // Guarda ANTES de saber se conectou: senha certa perdida porque a conexão
            // demorou faria digitar tudo de novo.
            rede_salva_t nova;
            memset(&nova, 0, sizeof nova);
            snprintf(nova.nome,  sizeof nova.nome,  "%s", e->wifi_alvo);
            snprintf(nova.senha, sizeof nova.senha, "%s", e->digitando);
            (void)rede_grava(ap->hal, &nova);
            e->digitando[0] = '\0';
            e->wifi_lista = false;
        troca_topo(e, TELA_WIFI);   // de volta à lista, conectando
            return;
        }

        // Renomear o dono em Minha Conta: grava e volta. Vazio não confirma.
        if (e->teclado_contexto == TECLADO_DONO) {
            if (!e->digitando[0]) return;

            // Troca local: marcada, sobe na próxima sincronização.
            if (perfil_renomeia(ap->hal, e->digitando, true) == OK) {
                e->inicio.nome_sobe = true;
                perfil_local_t p;
                if (perfil_carrega(ap->hal, &p) == OK)
                    snprintf(e->inicio.nome_pendente,
                             sizeof e->inicio.nome_pendente, "%s", p.nome);
            }
            e->digitando[0] = '\0';
            e->teclado_contexto = TECLADO_RENOMEAR;
            if (e->profundidade > 0) e->profundidade--;
            return;
        }

        if (e->teclado_contexto == TECLADO_PROPRIETARIO) {
            if (!e->digitando[0]) return;     // nome vazio não confirma nada
            snprintf(e->inicio.nome_pendente, sizeof e->inicio.nome_pendente,
                     "%s", e->digitando);
            e->inicio.fase = INICIO_CONFIRMAR_DONO;
            return;
        }
        // RN-24: o escrito à mão TRAVA, e quem grava é o caso de uso.
        if (e->aberto_valido && e->digitando[0])
            (void)uso_renomear(ap->hal, e, &e->aberto, e->digitando);
        e->digitando[0] = '\0';
        if (e->profundidade > 0) e->profundidade--;
    }
    else if (n + 1 < sizeof e->digitando) {
        e->digitando[n] = t[0];
        e->digitando[n + 1] = '\0';
    }
}

// A linha `i` da tela atual só informa? Quem responde é a vista.
static bool linha_so_leitura(app_t *ap, int i)
{
    estado_t *e = &ap->estado;
    vista_menu_t v;

    // Toda tela de menu: Minha Conta tem linhas informativas no meio, e o ▼
    // parava nelas sem mover a moldura.
    switch (e->pilha[e->profundidade]) {
    case TELA_WIFI:          vista_wifi(e, cabe_acoes(&ap->tela), &v); break;


    case TELA_AJUSTES:       vista_ajustes(e, cabe_acoes(&ap->tela), &v); break;

    default:                 return false;
    }

    return i >= 0 && i < v.n && v.linhas[i].so_leitura;
}

static void anda_cursor(app_t *ap, int passo)
{
    estado_t *e = &ap->estado;

    // O estado alcança o que a TELA já mostra antes de andar: a vista desenha
    // a moldura na primeira linha apertável, e o primeiro ▼ não moveria nada.
    // Aqui e não em `empilha`: a lista muda de tamanho com o estado.
    for (int i = 0; i < 8 && linha_so_leitura(ap, e->cursor); i++) {
        if (e->cursor >= 32) break;
        e->cursor++;
    }

    // As confirmações de duas opções têm o direcional, mesmo travado (travar é
    // para não navegar, não para não responder). TODAS, não uma lista à mão.
    if (e->overlay == OVERLAY_DESCARTAR ||
        e->overlay == OVERLAY_DESCARTAR_OBRA ||
        e->overlay == OVERLAY_APAGAR_ROTINA ||
        e->overlay == OVERLAY_DESCONECTAR ||
        e->overlay == OVERLAY_ESQUECER) {
        e->cursor_overlay = (int16_t)(e->cursor_overlay ? 0 : 1);
        return;
    }

    if (e->overlay == OVERLAY_ACOES) {
        vista_nota_t v;
        vista_nota(e, &v);
        e->cursor_overlay += (int16_t)passo;
        if (e->cursor_overlay < 0) e->cursor_overlay = 0;
        if (e->cursor_overlay > v.n_acoes - 1)
            e->cursor_overlay = (int16_t)(v.n_acoes - 1);
        return;
    }

    // Com a gaveta aberta o direcional é dela.
    if (e->overlay == OVERLAY_MENU) {
        vista_menu_t v;
        vista_menu(e, cabe_acoes(&ap->tela), &v);
        e->cursor_overlay += (int16_t)passo;
        if (e->cursor_overlay < 0)      e->cursor_overlay = 0;
        if (e->cursor_overlay > v.n - 1) e->cursor_overlay = (int16_t)(v.n - 1);
        return;
    }

    // RN-14: gravando, ou na dock (RN-3C), o cursor some e para de andar.
    if (e->travado) return;

    if (e->pilha[e->profundidade] == TELA_TECLADO) {
        anda_teclado(e, passo, 0);
        return;
    }

    // No calendário o cursor é um dia: ▲▼ andam de SEMANA, ◀▶ de dia.
    if (e->pilha[e->profundidade] == TELA_CALENDARIO ||
        e->pilha[e->profundidade] == TELA_ESCOLHER_DIA) {
        anda_dia(ap, passo * 7);
        return;
    }

    // ── o mostrador: ▲▼ mudam a casa sob o cursor ───────────────────────
    // O minuto anda de CINCO em cinco. As casas dão a volta (23 → 00, 55 → 00).
    if (e->pilha[e->profundidade] == TELA_ESCOLHER_HORA) {
        if (e->escolha_campo == 0) {
            int h = e->escolha_h - passo;   // ▲ sobe o número
            e->escolha_h = (int8_t)((h + 24) % 24);
        } else {
            int m = e->escolha_m - passo * 5;
            e->escolha_m = (int8_t)((m + 60) % 60);
        }
        return;
    }

    int n = linhas_da_tela(ap);

    e->cursor += (int16_t)passo;

    // Na HOME, subir além do topo volta ao repouso, sem seleção, e libera ◀▶
    // para os dias. Em MINHA CONTA o -1 é o ícone de sair no card. Nas outras o
    // cursor prende no topo.
    tela_id topo_agora = e->pilha[e->profundidade];
    int piso = (topo_agora == TELA_AGENDA || topo_agora == TELA_CONTA)
             ? -1 : 0;
    if (e->cursor < piso)  e->cursor = (int16_t)piso;
    if (e->cursor > n - 1) e->cursor = (int16_t)(n - 1);

    // ── e PULA o que só informa ─────────────────────────────────────────
    // No MESMO sentido; sem nada nesse sentido, volta.
    for (int tentativas = 0; tentativas < n; tentativas++) {
        if (!linha_so_leitura(ap, e->cursor)) return;

        int novo = e->cursor + (passo >= 0 ? 1 : -1);
        if (novo < 0 || novo > n - 1) break;
        e->cursor = (int16_t)novo;
    }

    // Nenhuma parada no sentido pedido: volta pelo outro lado.
    for (int i = 0; i < n; i++)
        if (!linha_so_leitura(ap, i)) { e->cursor = (int16_t)i; return; }
}

// Gravar não abre tela nem gasta nível da pilha. Fora da Agenda o gravador
// vem como overlay, com a tela de trás visível em volta.
static void abre_gravador(estado_t *e)
{
    // A gaveta sai da frente: ela é menu, e a fala é o gesto.
    e->overlay = e->pilha[e->profundidade] == TELA_AGENDA
               ? OVERLAY_NADA : OVERLAY_GRAVANDO;
}

// PILHA_MAX níveis: cada navegação custa refresh, e quem entra fundo
// demais não sai.
static void empilha(estado_t *e, tela_id t)
{
    // Estourou: TROCA O TOPO, nunca desiste calado (o gesto sumiria). Perde-se
    // um degrau do BACK, o que é melhor que perder o gesto.
    if (e->profundidade >= PILHA_MAX - 1) {
        e->pilha[e->profundidade] = t;
        e->cursor = 0;
        return;
    }
    e->profundidade++;
    e->pilha[e->profundidade] = t;
    e->cursor = 0;
}

static uint8_t cursor_do_turno(const xadrez_pos_t *p)
{
    int preferida = p->turno == XZ_BRANCAS ? XZ_CASA('e', 2) : XZ_CASA('e', 7);
    uint8_t peca = xadrez_peca_em(p, preferida);
    if (peca && xadrez_cor(peca) == (xadrez_cor_t)p->turno)
        return (uint8_t)preferida;
    for (int i = 0; i < 64; i++) {
        peca = xadrez_peca_em(p, i);
        if (peca && xadrez_cor(peca) == (xadrez_cor_t)p->turno)
            return (uint8_t)i;
    }
    return 0;
}

static void inicia_maquina_xadrez(app_t *ap)
{
    xadrez_app_t *x = &ap->estado.xadrez;
    x->maquina_pensando = x->modo == XZ_MODO_MAQUINA &&
        x->pagina == XZ_PAG_TABULEIRO && x->resultado == XZ_EM_CURSO &&
        x->posicao.turno != x->cor_humana;
    if (x->maquina_pensando)
        xadrez_maquina_inicia(&ap->maquina_xadrez, &x->posicao,
                              (xadrez_dificuldade_t)x->dificuldade);
}

static void abre_xadrez(app_t *ap)
{
    estado_t *e = &ap->estado;
    xadrez_salvo_t salvo;
    erro_t erro = xadrez_carrega(ap->hal, &salvo);
    if (erro == OK) {
        e->xadrez.posicao = salvo.posicao;
        e->xadrez.cursor = salvo.cursor;
        e->xadrez.origem = salvo.origem;
        e->xadrez.modo = salvo.modo;
        e->xadrez.cor_humana = salvo.cor_humana;
        e->xadrez.cor_baixo = salvo.cor_baixo;
        e->xadrez.dificuldade = salvo.dificuldade;
        e->xadrez.orientacao = salvo.orientacao;
        e->xadrez.mostrar_ajuda = salvo.mostrar_ajuda;
        e->xadrez.n_lances = salvo.n_lances;
        e->xadrez.placar_a2 = salvo.placar_a2;
        e->xadrez.placar_b2 = salvo.placar_b2;
        e->xadrez.tem_salva = true;
        e->xadrez.resultado = salvo.resultado != XZ_EM_CURSO
                           ? salvo.resultado : xadrez_estado(&e->xadrez.posicao);
        e->xadrez.pagina = e->xadrez.resultado == XZ_EM_CURSO
                         ? XZ_PAG_INICIO : XZ_PAG_FINAL;
        e->xadrez.menu_cursor = 0;
    } else {
        memset(&e->xadrez, 0, sizeof e->xadrez);
        e->xadrez.origem = -1;
        e->xadrez.mostrar_ajuda = false;
        e->xadrez.pagina = erro == ERR_ARQUIVO ? XZ_PAG_MODOS : XZ_PAG_ERRO;
    }
    empilha(e, TELA_XADREZ);
    inicia_maquina_xadrez(ap);
}

static void salva_xadrez(app_t *ap, xadrez_mov_t lance)
{
    xadrez_app_t *x = &ap->estado.xadrez;
    xadrez_salvo_t s = {
        .posicao = x->posicao, .modo = x->modo,
        .cor_humana = x->cor_humana, .cor_baixo = x->cor_baixo,
        .dificuldade = x->dificuldade,
        .orientacao = x->orientacao, .cursor = x->cursor,
        .origem = x->origem, .resultado = xadrez_estado(&x->posicao),
        .mostrar_ajuda = x->mostrar_ajuda,
        .placar_a2 = x->placar_a2, .placar_b2 = x->placar_b2,
    };
    x->salvamento_falhou = xadrez_salva_lance(ap->hal, &s, lance) != OK;
    x->n_lances++;
    if (!x->salvamento_falhou) x->tem_salva = true;
}

static void salva_opcoes_xadrez(app_t *ap)
{
    xadrez_app_t *x = &ap->estado.xadrez;
    if (!x->tem_salva) return;
    xadrez_salvo_t s = {
        .posicao = x->posicao, .modo = x->modo,
        .cor_humana = x->cor_humana, .cor_baixo = x->cor_baixo,
        .dificuldade = x->dificuldade, .orientacao = x->orientacao,
        .cursor = x->cursor, .origem = x->origem, .resultado = x->resultado,
        .n_lances = x->n_lances,
        .mostrar_ajuda = x->mostrar_ajuda,
        .placar_a2 = x->placar_a2, .placar_b2 = x->placar_b2,
    };
    x->salvamento_falhou = xadrez_salva_opcoes(ap->hal, &s) != OK;
}

static void carrega_historico_xadrez(app_t *ap)
{
    xadrez_app_t *x = &ap->estado.xadrez;
    erro_t e = xadrez_historico(ap->hal, x->historico_desloc,
                               x->historico, 4, &x->historico_total);
    if (e != OK) {
        memset(x->historico, 0, sizeof x->historico);
        x->historico_total = 0;
    }
}

static void acumula_resultado_xadrez(xadrez_app_t *x)
{
    if (x->resultado == XZ_MATE_BRANCAS || x->resultado == XZ_MATE_PRETAS) {
        bool brancas = x->resultado == XZ_MATE_BRANCAS;
        bool primeiro_brancas = x->modo == XZ_MODO_MAQUINA
                              ? x->cor_humana == XZ_BRANCAS
                              : x->cor_baixo == XZ_BRANCAS;
        if (brancas == primeiro_brancas) x->placar_a2 += 2;
        else x->placar_b2 += 2;
    } else if (x->resultado != XZ_EM_CURSO) {
        x->placar_a2++; x->placar_b2++;
    }
}

static void revanche_xadrez(app_t *ap)
{
    estado_t *e = &ap->estado;
    xadrez_app_t *x = &e->xadrez;
    acumula_resultado_xadrez(x);
    (void)xadrez_apaga(ap->hal);
    xadrez_nova(&x->posicao);
    x->cor_humana ^= 1u;
    x->cor_baixo ^= 1u;
    x->cursor = cursor_do_turno(&x->posicao);
    e->cursor = x->cursor;
    x->origem = -1;
    x->n_lances = 0;
    x->resultado = XZ_EM_CURSO;
    x->pagina = XZ_PAG_TABULEIRO;
    x->menu_cursor = 0;
    x->maquina_pensando = false;

    xadrez_salvo_t s = {
        .posicao = x->posicao, .modo = x->modo,
        .cor_humana = x->cor_humana, .cor_baixo = x->cor_baixo,
        .dificuldade = x->dificuldade, .orientacao = x->orientacao,
        .cursor = x->cursor, .origem = x->origem, .resultado = x->resultado,
        .mostrar_ajuda = x->mostrar_ajuda,
        .placar_a2 = x->placar_a2, .placar_b2 = x->placar_b2,
    };
    x->salvamento_falhou = xadrez_salva_opcoes(ap->hal, &s) != OK;
    x->tem_salva = !x->salvamento_falhou;
    inicia_maquina_xadrez(ap);
}

static void encerra_confronto_xadrez(app_t *ap)
{
    xadrez_app_t *x = &ap->estado.xadrez;
    (void)xadrez_apaga(ap->hal);
    x->tem_salva = false;
    x->maquina_pensando = false;
    x->placar_a2 = x->placar_b2 = 0;
}

static bool entrada_xadrez(app_t *ap, entrada_t botao)
{
    estado_t *e = &ap->estado;
    if (e->pilha[e->profundidade] != TELA_XADREZ) return false;
    xadrez_app_t *x = &e->xadrez;

    if (botao == IN_MENU && x->pagina == XZ_PAG_TABULEIRO) {
        x->pagina = XZ_PAG_MENU;
        x->menu_cursor = 0;
        ap->precisa_desenhar = true;
        return true;
    }

    if (x->orientacao != XZ_VERTICAL &&
        (x->pagina == XZ_PAG_TABULEIRO || x->pagina == XZ_PAG_PROMOCAO ||
         x->pagina == XZ_PAG_MENU || x->pagina == XZ_PAG_FINAL)) {
        static const entrada_t tabuleiro[] = {
            [IN_CIMA] = IN_ESQ, [IN_BAIXO] = IN_DIR,
            [IN_ESQ] = IN_BAIXO, [IN_DIR] = IN_CIMA,
        };
        static const entrada_t gaveta[] = {
            [IN_CIMA] = IN_DIR, [IN_BAIXO] = IN_ESQ,
            [IN_ESQ] = IN_CIMA, [IN_DIR] = IN_BAIXO,
        };
        if (botao >= IN_CIMA && botao <= IN_DIR) {
            bool no_tabuleiro = x->pagina == XZ_PAG_TABULEIRO ||
                                x->pagina == XZ_PAG_PROMOCAO;
            bool direita = x->orientacao == XZ_HORIZONTAL_DIREITA;
            bool usa_tabuleiro = x->pagina == XZ_PAG_MENU ? direita
                                                          : no_tabuleiro == direita;
            botao = usa_tabuleiro ? tabuleiro[botao] : gaveta[botao];
        }
    }

    if (botao == IN_VOLTAR) {
        if (x->pagina == XZ_PAG_PROMOCAO) {
            x->pagina = XZ_PAG_TABULEIRO;
        } else if (x->pagina == XZ_PAG_TABULEIRO && x->origem >= 0) {
            x->origem = -1;
        } else if (x->pagina == XZ_PAG_TABULEIRO) {
            x->pagina = XZ_PAG_SAIR;
            x->menu_cursor = 0;
        } else if (x->pagina == XZ_PAG_SAIR || x->pagina == XZ_PAG_SUBSTITUIR) {
            x->pagina = x->pagina == XZ_PAG_SAIR ? XZ_PAG_TABULEIRO : XZ_PAG_INICIO;
        } else if (x->pagina == XZ_PAG_MENU || x->pagina == XZ_PAG_EMPATE ||
                   x->pagina == XZ_PAG_ABANDONAR) {
            x->pagina = XZ_PAG_TABULEIRO;
        } else if (x->pagina == XZ_PAG_FINAL) {
            /* A posição final é leitura; OK leva ao placar. */
        } else if (x->pagina == XZ_PAG_HISTORICO) {
            x->pagina = x->retorno;
        } else if (x->pagina == XZ_PAG_OPCOES) {
            x->pagina = x->retorno;
        } else if (x->pagina == XZ_PAG_PREPARAR_LOCAL ||
                   x->pagina == XZ_PAG_PREPARAR_MAQUINA) {
            x->pagina = XZ_PAG_MODOS;
        } else if (x->pagina == XZ_PAG_RESULTADO) {
            encerra_confronto_xadrez(ap);
            voz_volta(e);
        } else {
            voz_volta(e);
        }
        ap->precisa_desenhar = true;
        return true;
    }

    if (botao == IN_OK) {
        if (x->pagina == XZ_PAG_TABULEIRO && x->maquina_pensando)
            return true;
        if (x->pagina == XZ_PAG_INICIO) {
            if (x->menu_cursor == 0) {
                x->pagina = XZ_PAG_TABULEIRO;
                inicia_maquina_xadrez(ap);
            } else if (x->menu_cursor == 1) {
                x->pagina = XZ_PAG_SUBSTITUIR; x->menu_cursor = 0;
            } else {
                x->retorno = XZ_PAG_INICIO;
                x->pagina = XZ_PAG_OPCOES;
                x->menu_cursor = x->orientacao == XZ_VERTICAL ? 0 : 1;
            }
        } else if (x->pagina == XZ_PAG_MODOS) {
            if (x->menu_cursor < 2) {
                x->pagina = x->menu_cursor == 0 ? XZ_PAG_PREPARAR_MAQUINA
                                                 : XZ_PAG_PREPARAR_LOCAL;
                x->menu_cursor = 0;
                x->cor_humana = XZ_BRANCAS;
                x->cor_baixo = XZ_BRANCAS;
                x->dificuldade = XZM_MEDIA;
            } else {
                x->retorno = XZ_PAG_MODOS;
                x->pagina = XZ_PAG_OPCOES;
                x->menu_cursor = x->orientacao == XZ_VERTICAL ? 0 : 1;
            }
        } else if (x->pagina == XZ_PAG_OPCOES) {
            if (x->menu_cursor == 0) x->orientacao = XZ_VERTICAL;
            else if (x->orientacao == XZ_VERTICAL)
                x->orientacao = XZ_HORIZONTAL_DIREITA;
            salva_opcoes_xadrez(ap);
            x->pagina = x->retorno;
            x->menu_cursor = 0;
        } else if (x->pagina == XZ_PAG_PREPARAR_MAQUINA) {
            if (x->menu_cursor == 2) {
                (void)xadrez_apaga(ap->hal);
                xadrez_nova(&x->posicao);
                x->modo = XZ_MODO_MAQUINA;
                x->cor_baixo = x->cor_humana;
                x->cursor = cursor_do_turno(&x->posicao);
                e->cursor = x->cursor;
                x->origem = -1;
                x->tem_salva = false;
                x->n_lances = 0;
                x->placar_a2 = x->placar_b2 = 0;
                x->resultado = XZ_EM_CURSO;
                x->mostrar_ajuda = false;
                x->pagina = XZ_PAG_TABULEIRO;
                inicia_maquina_xadrez(ap);
            }
        } else if (x->pagina == XZ_PAG_PREPARAR_LOCAL) {
            if (x->menu_cursor == 1) {
                (void)xadrez_apaga(ap->hal);
                xadrez_nova(&x->posicao);
                x->modo = XZ_MODO_LOCAL;
                x->cor_baixo = x->cor_humana;
                x->maquina_pensando = false;
                x->cursor = XZ_E2;
                e->cursor = x->cursor;
                x->origem = -1;
                x->tem_salva = false;
                x->n_lances = 0;
                x->placar_a2 = x->placar_b2 = 0;
                x->resultado = XZ_EM_CURSO;
                x->mostrar_ajuda = false;
                x->pagina = XZ_PAG_TABULEIRO;
            }
        } else if (x->pagina == XZ_PAG_SUBSTITUIR) {
            if (x->menu_cursor == 0) x->pagina = XZ_PAG_INICIO;
            else {
                (void)xadrez_apaga(ap->hal);
                x->tem_salva = false;
                x->pagina = XZ_PAG_MODOS;
                x->menu_cursor = 0;
            }
        } else if (x->pagina == XZ_PAG_TABULEIRO) {
            uint8_t peca = xadrez_peca_em(&x->posicao, x->cursor);
            if (x->origem < 0) {
                if (peca && xadrez_cor(peca) == (xadrez_cor_t)x->posicao.turno)
                    x->origem = (int8_t)x->cursor;
            } else {
                xadrez_mov_t m[XADREZ_MOV_MAX];
                int n = xadrez_movimentos(&x->posicao, x->origem, m,
                                           XADREZ_MOV_MAX);
                for (int i = 0; i < n; i++) if (m[i].para == x->cursor) {
                    if (m[i].promocao) {
                        x->promocao = m[i];
                        x->promocao.promocao = XZ_DAMA;
                        x->pagina = XZ_PAG_PROMOCAO;
                    } else if (xadrez_joga(&x->posicao, m[i])) {
                        x->origem = -1;
                        salva_xadrez(ap, m[i]);
                        x->resultado = xadrez_estado(&x->posicao);
                        if (x->resultado != XZ_EM_CURSO)
                            x->pagina = XZ_PAG_FINAL;
                        else
                            inicia_maquina_xadrez(ap);
                    }
                    break;
                }
            }
        } else if (x->pagina == XZ_PAG_PROMOCAO) {
            xadrez_mov_t m = x->promocao;
            if (xadrez_joga(&x->posicao, m)) {
                x->cursor = m.para; x->origem = -1;
                salva_xadrez(ap, m);
                x->resultado = xadrez_estado(&x->posicao);
                x->pagina = x->resultado == XZ_EM_CURSO
                          ? XZ_PAG_TABULEIRO : XZ_PAG_FINAL;
                if (x->pagina == XZ_PAG_TABULEIRO)
                    inicia_maquina_xadrez(ap);
            }
        } else if (x->pagina == XZ_PAG_MENU) {
            if (x->menu_cursor == 0) {
                x->historico_desloc = 0;
                carrega_historico_xadrez(ap);
                x->retorno = XZ_PAG_TABULEIRO;
                x->pagina = XZ_PAG_HISTORICO;
            } else if (x->menu_cursor == 1) {
                x->orientacao = (uint8_t)((x->orientacao + 1u) % 3u);
                ap->xadrez_girou_tela = true;
                salva_opcoes_xadrez(ap);
                x->pagina = XZ_PAG_TABULEIRO;
            } else if (x->menu_cursor == 2) {
                x->mostrar_ajuda = !x->mostrar_ajuda;
                salva_opcoes_xadrez(ap);
                x->pagina = XZ_PAG_TABULEIRO;
            } else {
                x->pagina = XZ_PAG_EMPATE;
                x->menu_cursor = 0;
            }
        } else if (x->pagina == XZ_PAG_EMPATE) {
            if (x->menu_cursor == 0) x->pagina = XZ_PAG_TABULEIRO;
            else {
                x->resultado = XZ_EMPATE_ACORDO;
                salva_opcoes_xadrez(ap);
                x->pagina = XZ_PAG_RESULTADO;
                x->menu_cursor = 0;
                x->maquina_pensando = false;
            }
        } else if (x->pagina == XZ_PAG_ABANDONAR) {
            if (x->menu_cursor == 0) x->pagina = XZ_PAG_TABULEIRO;
            else {
                (void)xadrez_apaga(ap->hal);
                x->tem_salva = false;
                x->maquina_pensando = false;
                voz_volta(e);
            }
        } else if (x->pagina == XZ_PAG_FINAL) {
            x->pagina = XZ_PAG_RESULTADO;
            x->menu_cursor = 0;
        } else if (x->pagina == XZ_PAG_RESULTADO) {
            if (x->menu_cursor == 0) {
                revanche_xadrez(ap);
            } else if (x->menu_cursor == 1) {
                x->historico_desloc = 0;
                carrega_historico_xadrez(ap);
                x->retorno = XZ_PAG_RESULTADO;
                x->pagina = XZ_PAG_HISTORICO;
            } else {
                encerra_confronto_xadrez(ap);
                x->pagina = XZ_PAG_MODOS;
                x->menu_cursor = 0;
            }
        } else if (x->pagina == XZ_PAG_SAIR) {
            if (x->menu_cursor == 0) x->pagina = XZ_PAG_TABULEIRO;
            else if (x->menu_cursor == 2) {
                x->pagina = XZ_PAG_ABANDONAR;
                x->menu_cursor = 0;
            } else {
                x->maquina_pensando = false;
                voz_volta(e);
            }
        } else if (x->pagina == XZ_PAG_ERRO) {
            (void)xadrez_apaga(ap->hal);
            x->pagina = XZ_PAG_MODOS;
        }
        ap->precisa_desenhar = true;
        return true;
    }

    if (x->pagina == XZ_PAG_TABULEIRO && !x->maquina_pensando &&
        (botao == IN_CIMA || botao == IN_BAIXO ||
         botao == IN_ESQ || botao == IN_DIR)) {
        int dx = botao == IN_DIR ? 1 : botao == IN_ESQ ? -1 : 0;
        int dy = botao == IN_CIMA ? 1 : botao == IN_BAIXO ? -1 : 0;
        if (x->cor_baixo == XZ_PRETAS) { dx = -dx; dy = -dy; }
        x->cursor = (uint8_t)(x->origem < 0
            ? xadrez_navega_peca(&x->posicao, x->cursor, dx, dy)
            : xadrez_navega_destino(&x->posicao, x->origem,
                                     x->cursor, dx, dy));
        e->cursor = x->cursor;
        ap->precisa_desenhar = true;
        return true;
    }
    if (x->pagina == XZ_PAG_INICIO &&
        (botao == IN_CIMA || botao == IN_BAIXO)) {
        x->menu_cursor = (uint8_t)((x->menu_cursor +
            (botao == IN_BAIXO ? 1 : 2)) % 3);
        e->cursor = x->menu_cursor;
        ap->precisa_desenhar = true;
        return true;
    }
    if (x->pagina == XZ_PAG_MODOS &&
        (botao == IN_CIMA || botao == IN_BAIXO)) {
        x->menu_cursor = (uint8_t)((x->menu_cursor +
            (botao == IN_BAIXO ? 1 : 2)) % 3);
        e->cursor = x->menu_cursor;
        ap->precisa_desenhar = true;
        return true;
    }
    if (x->pagina == XZ_PAG_PREPARAR_MAQUINA &&
        (botao == IN_CIMA || botao == IN_BAIXO ||
         botao == IN_ESQ || botao == IN_DIR)) {
        if (botao == IN_CIMA)
            x->menu_cursor = (uint8_t)((x->menu_cursor + 2) % 3);
        else if (botao == IN_BAIXO)
            x->menu_cursor = (uint8_t)((x->menu_cursor + 1) % 3);
        else if (x->menu_cursor == 0) {
            x->cor_humana ^= 1u;
            x->cor_baixo = x->cor_humana;
        }
        else if (x->menu_cursor == 1)
            x->dificuldade = (uint8_t)((x->dificuldade +
                (botao == IN_DIR ? 1 : 2)) % 3);
        e->cursor = x->menu_cursor;
        ap->precisa_desenhar = true;
        return true;
    }
    if (x->pagina == XZ_PAG_PREPARAR_LOCAL &&
        (botao == IN_CIMA || botao == IN_BAIXO ||
         botao == IN_ESQ || botao == IN_DIR)) {
        if (botao == IN_CIMA || botao == IN_BAIXO)
            x->menu_cursor = botao == IN_BAIXO ? 1 : 0;
        else if (x->menu_cursor == 0) {
            x->cor_humana = x->cor_humana == XZ_BRANCAS ? XZ_PRETAS : XZ_BRANCAS;
            x->cor_baixo = x->cor_humana;
        }
        e->cursor = x->menu_cursor;
        ap->precisa_desenhar = true;
        return true;
    }
    if ((x->pagina == XZ_PAG_SUBSTITUIR || x->pagina == XZ_PAG_SAIR) &&
        (botao == IN_CIMA || botao == IN_BAIXO)) {
        int max = x->pagina == XZ_PAG_SAIR ? 2 : 1;
        x->menu_cursor += botao == IN_BAIXO ? 1 : max;
        x->menu_cursor %= max + 1;
        e->cursor = x->menu_cursor;
        ap->precisa_desenhar = true;
        return true;
    }
    if ((x->pagina == XZ_PAG_OPCOES || x->pagina == XZ_PAG_EMPATE ||
         x->pagina == XZ_PAG_ABANDONAR) &&
        (botao == IN_CIMA || botao == IN_BAIXO)) {
        x->menu_cursor ^= 1u;
        e->cursor = x->menu_cursor;
        ap->precisa_desenhar = true;
        return true;
    }
    if (x->pagina == XZ_PAG_MENU &&
        (botao == IN_CIMA || botao == IN_BAIXO)) {
        int max = x->modo == XZ_MODO_LOCAL ? 4 : 3;
        x->menu_cursor = (uint8_t)((x->menu_cursor +
            (botao == IN_BAIXO ? 1 : max - 1)) % max);
        e->cursor = x->menu_cursor;
        ap->precisa_desenhar = true;
        return true;
    }
    if (x->pagina == XZ_PAG_RESULTADO &&
        (botao == IN_CIMA || botao == IN_BAIXO)) {
        x->menu_cursor = (uint8_t)((x->menu_cursor +
            (botao == IN_BAIXO ? 1 : 2)) % 3);
        e->cursor = x->menu_cursor;
        ap->precisa_desenhar = true;
        return true;
    }
    if (x->pagina == XZ_PAG_HISTORICO &&
        (botao == IN_CIMA || botao == IN_BAIXO)) {
        if (botao == IN_BAIXO && x->historico_desloc + 1 < x->historico_total)
            x->historico_desloc++;
        else if (botao == IN_CIMA && x->historico_desloc > 0)
            x->historico_desloc--;
        carrega_historico_xadrez(ap);
        ap->precisa_desenhar = true;
        return true;
    }
    if (x->pagina == XZ_PAG_PROMOCAO &&
        (botao == IN_ESQ || botao == IN_DIR)) {
        static const uint8_t ordem[] = { XZ_DAMA, XZ_TORRE, XZ_BISPO, XZ_CAVALO };
        int i = 0;
        while (i < 4 && ordem[i] != x->promocao.promocao) i++;
        i = (i + (botao == IN_DIR ? 1 : 3)) % 4;
        x->promocao.promocao = ordem[i];
        ap->precisa_desenhar = true;
        return true;
    }
    return botao != IN_VOZ && botao != IN_POWER;
}

static void passo_maquina_xadrez(app_t *ap)
{
    xadrez_app_t *x = &ap->estado.xadrez;
    if (ap->estado.pilha[ap->estado.profundidade] != TELA_XADREZ ||
        x->pagina != XZ_PAG_TABULEIRO || !x->maquina_pensando)
        return;

    xadrez_mov_t lance;
    xadrez_maquina_estado_t estado = xadrez_maquina_passo(
        &ap->maquina_xadrez, 32, &lance);
    if (estado == XZM_BUSCANDO) return;

    x->maquina_pensando = false;
    if (estado == XZM_PRONTO && xadrez_joga(&x->posicao, lance)) {
        x->origem = -1;
        x->cursor = cursor_do_turno(&x->posicao);
        ap->estado.cursor = x->cursor;
        salva_xadrez(ap, lance);
    }
    x->resultado = xadrez_estado(&x->posicao);
    if (x->resultado != XZ_EM_CURSO) x->pagina = XZ_PAG_FINAL;
    ap->precisa_desenhar = true;
}

// ── falar é global, e voltar de falar também ────────────────────────
// O resultado da fala NÃO empilha: guarda a pilha inteira, ocupa a tela e a
// devolve no fim. Funciona de qualquer profundidade, e a volta é exata.
static void voz_mostra(estado_t *e, tela_id t)
{
    memcpy(e->pilha_antes, e->pilha, sizeof e->pilha);
    e->profundidade_antes = e->profundidade;
    e->veio_da_voz        = true;

    e->profundidade = 0;
    e->pilha[0]     = t;
    e->cursor       = 0;
}

// A tela do topo mostra UM item que deixou de existir ("apaga essa
// tarefa", dito de dentro dela). Voltar para lá seria cursor num fantasma.
static bool topo_ficou_orfao(const estado_t *e)
{
    if (e->profundidade <= 0) return false;
    tela_id t = e->pilha[e->profundidade];
    return t == TELA_NOTA && !e->aberto_valido;
}

static void voz_volta(estado_t *e)
{
    if (e->veio_da_voz) {
        memcpy(e->pilha, e->pilha_antes, sizeof e->pilha);
        e->profundidade = e->profundidade_antes;
        e->veio_da_voz  = false;
    } else if (e->profundidade > 0) {
        e->profundidade--;
    }

    while (topo_ficou_orfao(e)) e->profundidade--;
    e->cursor = 0;
}

// ── o vidro trava, e acordar devolve o lugar ────────────────────────
// Como `voz_mostra`: guarda a pilha inteira e a devolve. Dormir é
// interrupção, não navegação.
static void trava_o_vidro(estado_t *e)
{
    // Já travado não guarda de novo: o INT do expansor acorda por qualquer
    // botão, e salvaria TELA_BLOQUEADA por cima do que a pessoa fazia.
    if (e->pilha[e->profundidade] != TELA_BLOQUEADA) {
        memcpy(e->pilha_travada, e->pilha, sizeof e->pilha);
        e->profundidade_travada = e->profundidade;
        e->cursor_travado       = e->cursor;
    }

    e->profundidade = 0;
    e->pilha[0]     = TELA_BLOQUEADA;
    e->overlay      = OVERLAY_NADA;
    e->travado      = true;   // RN-3C: parado, não navega
}

// ── o bloqueio por tempo ─────────────────────────────────────────────
// Sem toque por AJUSTE_BLOQUEAR_MIN, a mesma trava do power. Nunca com fala
// aberta (RN-14) nem no primeiro uso.
static void bloqueia_se_ocioso(app_t *ap, uint32_t agora)
{
    estado_t *e = &ap->estado;
    if (e->inicio.fase != INICIO_HOME) return;
    if (e->gravacao.fase != GRAV_PARADA) return;
    if (e->pilha[e->profundidade] == TELA_BLOQUEADA) return;

    uint32_t limite = (uint32_t)e->config.valor[AJUSTE_BLOQUEAR_MIN] * 60000u;
    if (limite == 0 || agora - e->ultimo_toque_ms < limite) return;

    (void)dado_log(ap->hal, LOG_DORMIU, 0, e->hora, e->minuto);
    trava_o_vidro(e);
    ap->precisa_desenhar = true;
}

static void destrava_o_vidro(estado_t *e)
{
    memcpy(e->pilha, e->pilha_travada, sizeof e->pilha);
    e->profundidade = e->profundidade_travada;
    e->cursor       = e->cursor_travado;

    // Nasceu travado (primeiro boot, ou a dock): sem nada guardado, vai para a
    // Agenda.
    if (e->pilha[e->profundidade] == TELA_BLOQUEADA) {
        e->profundidade = 0;
        e->pilha[0]     = TELA_HOME;
        e->cursor       = 0;
    }

    // O item aberto pode ter sumido enquanto o vidro dormia (o pull continua).
    while (topo_ficou_orfao(e)) e->profundidade--;

    e->travado           = false;
    e->aviso_desbloqueio = false;
}

// Troca a tela do TOPO sem gastar nível: redes → senha. O BACK do teclado
// devolve a lista de redes, de onde a senha foi pedida.
static void troca_topo(estado_t *e, tela_id t)
{
    e->pilha[e->profundidade] = t;
    e->cursor = 0;
}


// O cache vale? Se não, levanta antes de qualquer pergunta. Só relê quando
// a bandeira caiu.
static void garante_cache(app_t *ap)
{
    // RN-6F: no primeiro uso a árvore pode nem existir; ler só encheria o
    // `ultimo_erro` de falha falsa.
    if (ap->estado.inicio.fase != INICIO_HOME) return;

    // A Agenda anda só entre ontem, hoje e amanhã. O dia distante que o
    // calendário deixa para trás volta a hoje aqui, num lugar só (são cinco
    // caminhos de volta), senão a Agenda desenharia o dia 30 com a data de hoje.
    int longe = data_dias_entre(ap->estado.hoje, ap->estado.dia_visto);
    if (ap->estado.pilha[ap->estado.profundidade] == TELA_AGENDA &&
        (longe < -1 || longe > 1)) {
        ap->estado.dia_visto     = ap->estado.hoje;
        estado_invalida_dia(&ap->estado);
    }

    if (!ap->estado.itens_validos) {
        erro_t err = OK;
        CRONOMETRA("dia", err = uso_carregar_dia(ap->hal, &ap->estado));
        if (err != OK) ap->estado.ultimo_erro = err;
    }
}

// As tarefas ABERTAS, por intervalo e não por dia, com bandeira de
// validade. NÃO cai quando o dia visto muda: andar entre os dias troca os
// compromissos e deixa o bloco de tarefas parado.
static void garante_pendentes(app_t *ap)
{
    if (ap->estado.inicio.fase != INICIO_HOME) return;
    if (ap->estado.pendentes_validas) return;

    erro_t err = OK;
    CRONOMETRA("pendentes", err = uso_carregar_pendentes(ap->hal, &ap->estado));
    if (err != OK) ap->estado.ultimo_erro = err;
}

// As marcas do calendário: quando o mês muda, nunca por quadro.
static void garante_marcas(app_t *ap)
{
    estado_t *e = &ap->estado;
    if (e->marcas_validas &&
        e->marcas_ano == e->dia_visto.ano &&
        e->marcas_mes == e->dia_visto.mes) return;

    erro_t err = OK;
    CRONOMETRA("marcas", err = uso_carregar_marcas(ap->hal, e));
    if (err != OK) e->ultimo_erro = err;
}

// Qual item o `alvo` da Agenda aponta. Ele indexa DOIS arrays: eventos em
// `itens` (o dia) e tarefas em `pendentes` (o intervalo). Ler o errado não
// dá erro, dá o ITEM ERRADO: por isso a escolha mora numa função só.
static const item_t *item_do_alvo(const estado_t *e, const vista_agenda_t *v)
{
    if (v->alvo < 0) return NULL;

    if (v->alvo_pendente)
        return v->alvo < e->n_pendentes ? &e->pendentes[v->alvo] : NULL;

    return v->alvo < e->n_itens ? &e->itens[v->alvo] : NULL;
}

// O item sob o cursor, ou NULL quando a linha não tem item.
static const item_t *item_sob_o_cursor(app_t *ap)
{
    estado_t *e = &ap->estado;

    if (e->pilha[e->profundidade] == TELA_AGENDA) {
        static VISTA_TRANSITORIA vista_agenda_t v;
        vista_agenda(e, &v);
        // A vista já decidiu qual item é a linha do cursor, e em qual array.
        return item_do_alvo(e, &v);
    }

    if (e->pilha[e->profundidade] == TELA_DIA) {
        // No dia, compromissos e depois tarefas: a ordem em que a tela desenha e o
        // cursor percorre.
        vista_dia_t v;
        vista_dia(e, &v);
        if (e->cursor < 0) return NULL;
        if (e->cursor < v.n_compromissos)
            return &e->itens[v.compromissos[e->cursor].indice];

        // A tarefa vem de `pendentes`; os compromissos, de `itens`.
        int t = e->cursor - v.n_compromissos;
        if (t >= v.n_tarefas) return NULL;
        return &e->pendentes[v.tarefas[t].indice];
    }

    return NULL;
}

// ◀▶ escolhe o campo, ▲▼ muda o valor (eixos diferentes). Aqui porque o
// primeiro uso e Ajustes ajustam a hora com o MESMO gesto. Devolve true
// quando consumiu o botão.
static bool edita_data_hora(estado_t *e, entrada_t botao)
{
    int c = e->inicio.campo;
    if (c < 0 || c > 4) c = 0;

    if (botao == IN_ESQ || botao == IN_DIR) {
        c += botao == IN_DIR ? 1 : -1;
        if (c < 0) c = 0;
        if (c > 4) c = 4;
        e->inicio.campo = (int8_t)c;
        return true;
    }

    if (botao != IN_CIMA && botao != IN_BAIXO) return false;
    int passo = botao == IN_CIMA ? 1 : -1;

    switch (c) {
    case 0: {                                   // dia: até o fim DESTE mês
        int teto = data_dias_no_mes(e->inicio.ano, e->inicio.mes);
        int v = e->inicio.dia + passo;
        if (v > teto) v = 1;
        if (v < 1)    v = teto;
        e->inicio.dia = (int8_t)v;
        break;
    }
    case 1: {
        int v = e->inicio.mes + passo;
        if (v > 12) v = 1;
        if (v < 1)  v = 12;
        e->inicio.mes = (int8_t)v;
        break;
    }
    case 2:
        // O ano não dá volta: depois de 2099 não vem 2024.
        e->inicio.ano = (int16_t)(e->inicio.ano + passo);
        if (e->inicio.ano < 2024) e->inicio.ano = 2024;
        if (e->inicio.ano > 2099) e->inicio.ano = 2099;
        break;
    case 3: {
        int v = e->inicio.hora + passo;
        if (v > 23) v = 0;
        if (v < 0)  v = 23;
        e->inicio.hora = (int8_t)v;
        break;
    }
    default: {
        int v = e->inicio.minuto + passo;
        if (v > 59) v = 0;
        if (v < 0)  v = 59;
        e->inicio.minuto = (int8_t)v;
        break;
    }
    }

    // Trocar mês ou ano pode deixar o dia fora dele (31/01 → fevereiro).
    int teto = data_dias_no_mes(e->inicio.ano, e->inicio.mes);
    if (e->inicio.dia > teto) e->inicio.dia = (int8_t)teto;
    return true;
}

// Data e hora: interruptor, fuso e campos. ▲▼ andam entre eles; nos campos
// ◀▶ escolhe e ▲▼ muda. ◀ no primeiro campo sobe para o fuso. True quando
// consumiu o botão.
static bool edita_no_relogio(app_t *ap, entrada_t botao)
{
    estado_t *e = &ap->estado;
    if (e->pilha[e->profundidade] != TELA_DATA_HORA) return false;

    // A conta de `vista_relogio`: com NTP os campos não se editam; com CONTA o
    // fuso também não (desce no pull).
    bool pela_rede = e->config.valor[AJUSTE_HORA_REDE] != 0;
    bool do_google = e->nome[0] != '\0';
    int  linhas    = 3;
    if (pela_rede) linhas = do_google ? 1 : 2;
    if (e->cursor >= linhas) e->cursor = (int16_t)(linhas - 1);

    // ── na linha dos campos ──
    if (e->cursor == 2) {
        if (botao == IN_ESQ && e->inicio.campo <= 0) {
            e->cursor = 1;
            ap->precisa_desenhar = true;
            return true;
        }
        if (edita_data_hora(e, botao)) {
            ap->precisa_desenhar = true;
            return true;
        }
        return false;
    }

    // ── no interruptor e no fuso ──
    if (botao == IN_CIMA || botao == IN_BAIXO) {
        int novo = e->cursor + (botao == IN_CIMA ? -1 : +1);
        if (novo < 0) novo = 0;
        if (novo >= linhas) novo = linhas - 1;
        // Na ponta o toque é consumido: senão o cursor genérico andaria para linhas
        // que não existem.
        if (novo == e->cursor) return true;
        e->cursor = (int16_t)novo;
        if (novo == 2) e->inicio.campo = 0;
        ap->precisa_desenhar = true;
        return true;
    }

    // No FUSO, ◀▶ andam de meia em meia hora.
    if ((botao == IN_ESQ || botao == IN_DIR) &&
        e->cursor == 1 && !(pela_rede && do_google)) {
        int novo = e->config.valor[AJUSTE_FUSO_MIN]
                 + (botao == IN_DIR ? 30 : -30);
        (void)uso_salvar_ajuste(ap->hal, e, AJUSTE_FUSO_MIN, novo);
        ap->precisa_desenhar = true;
        return true;
    }

    return false;
}

// O OK no card de Conexão: lê a mesma vista que a tela desenha e casa pelo
// título.
static void ok_na_conexao(app_t *ap)
{
    estado_t *e = &ap->estado;
    vista_cartao_t v;
    vista_conexao(e, &v);
    if (e->cursor < 0 || e->cursor >= v.n_dest) return;

    const char *t = v.dest[e->cursor].titulo;

    // A LISTA é outra tela, e a varredura começa junto: o card não muda de
    // tamanho enquanto as redes chegam.
    if (strstr(t, "Procurar")) {
        e->wifi_lista = true;
        e->cursor = 0;
        e->n_redes = 0;
        e->wifi_procurando = true;
        if (ap->hal->wifi_procurar) ap->hal->wifi_procurar();
        return;
    }

    // Digitar a senha de novo, para a MESMA rede (o alvo já está no estado).
    if (strstr(t, "Digitar")) {
        e->digitando[0] = '\0';
        e->teclado_contexto = TECLADO_WIFI;
        e->teclado_lin = e->teclado_col = 0;
        e->wifi_falha = WIFI_FALHA_NENHUMA;
        troca_topo(e, TELA_TECLADO);
        return;
    }

    // Tentar de novo: a mesma rede e senha; foi a rede que não respondeu.
    if (strstr(t, "Tentar")) {
        rede_salva_t guardada;
        e->wifi_falha = WIFI_FALHA_NENHUMA;
        e->rede = REDE_CONECTANDO;
        e->conectando_desde_ms = ap->hal->agora_ms();
        if (rede_carrega(ap->hal, &guardada) == OK && ap->hal->wifi_conectar)
            (void)ap->hal->wifi_conectar(guardada.nome, guardada.senha);
        return;
    }

    // Esquecer PERGUNTA antes.
    if (strstr(t, "Esquecer")) {
        e->overlay = OVERLAY_ESQUECER;
        e->cursor_overlay = 0;         // a segura nasce escolhida (RN-6C)
        ap->precisa_desenhar = true;
        return;
    }
}

static void ok_no_wifi(app_t *ap)
{
    estado_t *e = &ap->estado;

    if (!e->wifi_lista) { ok_na_conexao(ap); return; }

    vista_menu_t v;
    vista_wifi(e, cabe_acoes(&ap->tela), &v);
    if (e->cursor < 0 || e->cursor >= v.n) return;

    const char *linha = v.linhas[e->cursor].texto;

    if (strcmp(linha, "Procurar de novo") == 0) {
        e->n_redes = 0;
        e->wifi_procurando = true;
        if (ap->hal->wifi_procurar) ap->hal->wifi_procurar();
        return;
    }

    if (strcmp(linha, "Rede oculta") == 0) {
        e->wifi_alvo[0] = '\0';
        e->digitando[0] = '\0';
        e->teclado_contexto = TECLADO_WIFI_SSID;
        e->teclado_lin = e->teclado_col = 0;
        troca_topo(e, TELA_TECLADO);
        return;
    }

    // Uma rede da lista: salva conecta direto; nova pede a senha.
    for (int i = 0; i < e->n_redes; i++) {
        if (strcmp(linha, e->redes[i].nome) != 0) continue;

        snprintf(e->wifi_alvo, sizeof e->wifi_alvo, "%s", e->redes[i].nome);
        if (e->redes[i].salva) {
            rede_salva_t guardada;
            e->rede = REDE_CONECTANDO;
        e->conectando_desde_ms = ap->hal->agora_ms();
            e->conectando_desde_ms = ap->hal->agora_ms();
            if (rede_carrega(ap->hal, &guardada) == OK && ap->hal->wifi_conectar)
                (void)ap->hal->wifi_conectar(guardada.nome, guardada.senha);
        } else if (e->redes[i].aberta) {
            // Aberta: sem senha, sem teclado.
            rede_salva_t nova;
            memset(&nova, 0, sizeof nova);
            snprintf(nova.nome, sizeof nova.nome, "%s", e->redes[i].nome);
            (void)rede_grava(ap->hal, &nova);
            snprintf(e->wifi_salva, sizeof e->wifi_salva, "%s", nova.nome);
            e->rede = REDE_CONECTANDO;
        e->conectando_desde_ms = ap->hal->agora_ms();
            e->conectando_desde_ms = ap->hal->agora_ms();
            if (ap->hal->wifi_conectar)
                (void)ap->hal->wifi_conectar(nova.nome, "");
        } else {
            e->digitando[0] = '\0';
            e->teclado_contexto = TECLADO_WIFI;
            e->teclado_lin = e->teclado_col = 0;
            troca_topo(e, TELA_TECLADO);
        }
        return;
    }
}

// O OK em Minha Conta: pergunta o que a linha FAZ, não a posição (a lista
// muda de tamanho). Nada espera rede: pede e volta (RN-41).
static void ok_na_conta(app_t *ap)
{
    estado_t *e = &ap->estado;

    switch (vista_conta_acao(e)) {
    case CONTA_WIFI:
        e->n_redes = 0;
        e->wifi_procurando = true;
        if (ap->hal->wifi_procurar) ap->hal->wifi_procurar();
        e->wifi_lista = false;
        troca_topo(e, TELA_WIFI);
        return;

    case CONTA_VINCULAR:
        // Abrir não PEDE nada: quem pede é o OK lá dentro (código de uso único).
        empilha(e, TELA_VINCULAR);
        return;

    case CONTA_VOZ:
        empilha(e, TELA_FALA);
        return;

    case CONTA_SINCRONIZACAO:
        // Pede a lista AO ENTRAR, não no desenho (o painel redesenha sozinho).
        (void)uso_agendas(ap->hal, e);
        empilha(e, TELA_SINCRONIZACAO);
        return;

    case CONTA_DONO:
        // O teclado abre com o nome atual: trocar é quase sempre corrigir uma letra.
        snprintf(e->digitando, sizeof e->digitando, "%s",
                 e->inicio.nome_pendente);
        e->teclado_contexto = TECLADO_DONO;
        e->teclado_lin = e->teclado_col = 0;
        empilha(e, TELA_TECLADO);
        return;

    case CONTA_DESCONECTAR:
        // Pergunta antes, nascendo no "não" (RN-6C), e diz que o cartão fica.
        e->overlay = OVERLAY_DESCONECTAR;
        e->cursor_overlay = 0;
        return;

    case CONTA_NADA:
    default:
        return;
    }
}

// A raiz de Ajustes é um ÍNDICE: o destino vem tipado da vista (comparar
// rótulo já deixou a tela sem porta).
static void ok_nos_ajustes(app_t *ap)
{
    estado_t *e = &ap->estado;
    vista_menu_t v;
    vista_ajustes(e, cabe_acoes(&ap->tela), &v);

    if (e->cursor < 0 || e->cursor >= v.n) return;

    tela_id destino = v.destino[e->cursor];
    if (destino == TELA_QUANTAS) return;

    // ── o que ENTRAR numa categoria dispara ─────────────────────────────
    // As que custam tempo real acontecem na entrada, não por quadro.
    if (destino == TELA_ARMAZENAMENTO) {
        mede_o_que_ocupa(ap);
    } else if (destino == TELA_WIFI) {
        // Varrer o ar leva segundos: pede-se ao entrar, e a lista chega pelo
        // EV_WIFI_REDES.
        e->n_redes = 0;
        e->wifi_procurando = true;
        if (ap->hal->wifi_procurar) ap->hal->wifi_procurar();
    }

    empilha(e, destino);
}

// Pela AÇÃO da linha, nunca pelo índice: índice fixo casaria este código
// com a ordem da lista em outro arquivo, e tirar uma linha abriria outra
// coisa sem erro de compilação.
static void ok_na_gaveta(app_t *ap)
{
    estado_t *e = &ap->estado;
    vista_menu_t v;
    vista_menu(e, cabe_acoes(&ap->tela), &v);
    if (e->cursor_overlay < 0 || e->cursor_overlay >= v.n) return;

    const char *linha = v.linhas[e->cursor_overlay].texto;
    e->overlay = OVERLAY_NADA;

    if (strcmp(linha, "Calendário") == 0) {
        empilha(e, TELA_CALENDARIO);
    } else if (strcmp(linha, "Todos") == 0) {
        e->acervo_filtro = ACERVO_TODOS;
        e->cursor = 0;
    } else if (strcmp(linha, "Em leitura") == 0) {
        e->acervo_filtro = ACERVO_EM_LEITURA;
        e->cursor = 0;
    } else if (strcmp(linha, "Livros") == 0) {
        e->acervo_filtro = ACERVO_SO_LIVROS;
        e->cursor = 0;
    } else if (strcmp(linha, "Documentos") == 0) {
        e->acervo_filtro = ACERVO_SO_DOCUMENTOS;
        e->cursor = 0;
    } else if (strcmp(linha, "Concluídos") == 0) {
        e->acervo_filtro = ACERVO_CONCLUIDOS;
        e->cursor = 0;
    } else if (strcmp(linha, "Anotações") == 0) {
        // Carrega ao ENTRAR, na primeira página: as mais novas.
        e->anotacoes_pagina = 0;
        e->cursor = 0;
        CRONOMETRA("anotacoes", (void)uso_carregar_anotacoes(ap->hal, e));
        empilha(e, TELA_ANOTACOES);
    } else if (strcmp(linha, "Tamanho") == 0) {
        leitor_tamanho_t novo = (leitor_tamanho_t)((e->leitor.letra + 1) % 3);
        leitor_recompoe(&e->leitor, novo);
        e->obra_aberta.fonte = (int8_t)(e->leitor.familia * 3 + novo);
        e->paginas_total = 0;
        atualiza_pagina(ap);
        e->overlay = OVERLAY_MENU;
    } else if (strcmp(linha, "Fonte") == 0) {
        int forte = e->leitor.familia >= 7;
        leitor_familia_t nova = (leitor_familia_t)(((e->leitor.familia % 7 + 1) % 7) + forte * 7);
        leitor_familia(&e->leitor, nova);
        e->obra_aberta.fonte = (int8_t)(nova * 3 + e->leitor.letra);
        e->paginas_total = 0;
        atualiza_pagina(ap);
        e->overlay = OVERLAY_MENU;
    } else if (strcmp(linha, "Peso") == 0) {
        leitor_familia_t nova = (leitor_familia_t)((e->leitor.familia + 7) % 14);
        leitor_familia(&e->leitor, nova);
        e->obra_aberta.fonte = (int8_t)(nova * 3 + e->leitor.letra);
        e->paginas_total = 0;
        atualiza_pagina(ap);
        e->overlay = OVERLAY_MENU;
    } else if (strcmp(linha, "Alinhamento") == 0) {
        e->leitor.alinhamento =
            (leitor_alinhamento_t)((e->leitor.alinhamento + 1) % 4);
        e->obra_aberta.alinhamento = (int8_t)e->leitor.alinhamento;
        (void)acervo_grava_meta(ap->hal, &e->obra_aberta);
        e->overlay = OVERLAY_MENU;
    } else if (strcmp(linha, "Progresso") == 0) {
        e->obra_aberta.rodape = (int8_t)((e->obra_aberta.rodape + 1) % 3);
        (void)acervo_grava_meta(ap->hal, &e->obra_aberta);
        e->overlay = OVERLAY_MENU;
    } else if (strcmp(linha, "Descartar deste Tinto") == 0) {
        e->overlay = OVERLAY_DESCARTAR_OBRA;
        e->cursor_overlay = 0;
    } else if (strcmp(linha, "Começar leitura") == 0 ||
               strcmp(linha, "Continuar leitura") == 0) {
        ok_na_obra(ap);
    } else if (strcmp(linha, "Ler desde o início") == 0) {
        e->obra_aberta.offset_texto = 0;
        for (int i = 0; i < e->n_acervo; i++)
            if (strcmp(e->acervo[i].id, e->obra_aberta.id) == 0)
                e->acervo[i].offset_texto = 0;
        ok_na_obra(ap);
    } else if (strcmp(linha, "Marcar como concluído") == 0 ||
               strcmp(linha, "Marcar como não lido") == 0) {
        e->obra_aberta.concluida = !e->obra_aberta.concluida;
        (void)acervo_grava_meta(ap->hal, &e->obra_aberta);
        for (int i = 0; i < e->n_acervo; i++)
            if (strcmp(e->acervo[i].id, e->obra_aberta.id) == 0)
                e->acervo[i] = e->obra_aberta;
    }
}

static void ok_na_home(app_t *ap)
{
    estado_t *e = &ap->estado;
    static VISTA_TRANSITORIA vista_agenda_t v;
    vista_agenda(e, &v);

    // Em repouso o OK não faz nada, e o rodapé não promete.
    if (e->cursor < 0) return;

    // Na linha de tarefa, marcar: o gesto mais repetido custa um toque (RN-37).
    if (v.cursor_trabalho >= 0 && v.alvo >= 0) {
        const item_t *it = item_do_alvo(e, &v);
        if (!it) return;
        (void)uso_marcar(ap->hal, e, it, !it->feita);
        return;
    }

    // No compromisso, abrir o item: de onde veio e a fala que o gerou.
    if (v.cursor_agenda >= 0) {
        const item_t *alvo = item_do_alvo(e, &v);
        if (!alvo) return;

        // O HÍBRIDO se marca daqui: a caixa na régua promete, o OK cumpre (RN-37).
        // O ▶ continua abrindo o detalhe.
        if (v.agenda[v.cursor_agenda].caixa) {
            (void)uso_marcar(ap->hal, e, alvo, !alvo->feita);
            return;
        }

        if (uso_abrir_item(ap->hal, e, alvo) == OK)
            empilha(e, TELA_NOTA);
        return;
    }

}

// ── de volta ao ITEM, com o que mudou dito ──────────────────────────
// RELÊ o item (o cartão acabou de mudar; o detalhe exige o item válido) e a
// faixa DIZ o que aconteceu: sem ela a pessoa aperta de novo.
static void volta_ao_item(app_t *ap, const char *o_que)
{
    estado_t *e = &ap->estado;

    item_t recarregado;
    data_t onde;
    if (cartao_acha_item(ap->hal, e->aberto.id, &recarregado, &onde) == OK) {
        recarregado.dia = onde;
        (void)uso_abrir_item(ap->hal, e, &recarregado);
    } else {
        e->aberto_valido = false;
    }

    // Volta para o NÍVEL de onde saiu: procurar `TELA_NOTA` na pilha falha
    // quando abrir o item já trocou o topo, e a confirmação caía na Home.
    if (e->voltar_nivel >= 0 && e->voltar_nivel < PILHA_MAX)
        e->profundidade = e->voltar_nivel;
    e->pilha[e->profundidade] = TELA_NOTA;

    snprintf(e->feito, sizeof e->feito, "%s", o_que ? o_que : "");
    e->feito_ms = e->agora_ms;
    ap->precisa_desenhar = true;
}

// Devolve o texto emprestado: ao sair do leitor e antes de abrir outra obra.
static void solta_o_texto(app_t *ap)
{
    if (!ap->texto_emprestado) return;
    if (ap->hal->devolver) ap->hal->devolver(ap->texto_emprestado);
    ap->texto_emprestado = NULL;
    ap->estado.leitor_texto = NULL;
}

// A página composta e os números do rodapé. A POSIÇÃO é gravada a cada
// virada: fechar sem guardar onde parou faria voltar ao começo.
static void atualiza_pagina(app_t *ap)
{
    estado_t *e = &ap->estado;
    int32_t antes = e->obra_aberta.offset_texto;

    (void)leitor_pagina(&e->leitor, e->pagina, sizeof e->pagina);
    e->leitura_pct = leitor_pct(&e->leitor);
    e->leitor_no_fim = leitor_no_fim(&e->leitor);

    // O TOTAL é varrido ao abrir e ao trocar a fonte; a página anda com o
    // offset.
    if (e->paginas_total <= 0) {
        // A contagem completa acontece aos poucos em app_passo: abrir não espera o
        // livro todo.
        if (e->pagina_atual <= 0) e->pagina_atual = 1;
    } else {
        int32_t agora = leitor_posicao(&e->leitor);
        if (agora > antes && e->pagina_atual < e->paginas_total)
            e->pagina_atual++;
        else if (agora < antes && e->pagina_atual > 1)
            e->pagina_atual--;
    }

    e->obra_aberta.offset_texto = leitor_posicao(&e->leitor);
    e->obra_aberta.aberta_em = e->hoje;
    ap->leitor_meta_pendente = true;
    e->acervo_valido = false;
}

static int32_t quebra_pagina_real(const char *texto, int32_t tamanho,
                                  leitor_tamanho_t tam, leitor_familia_t fam)
{
    fonte_t f = vista_fonte_leitor(tam, fam);
    const int y = GRID_BARRA_A + 10;
    const int linhas = (GRID_RODAPE_Y - 4 - y) / gfx_altura_linha(f);
    return gfx_pagina(NULL, 0, 0, TELA_L - 28, linhas, f, texto, tamanho);
}

// ── abrir uma obra ───────────────────────────────────────────────────
// O OK faz o que a MARCA da linha promete: a obra que está aqui abre; a só
// online começa a descer.
static void ok_no_acervo(app_t *ap)
{
    estado_t *e = &ap->estado;

    vista_acervo_t v;
    vista_acervo(e, &v);
    if (e->cursor < 0) return;
    int i;
    if (v.tem_destaque && e->cursor == 0) {
        i = v.destaque.indice;
    } else {
        int linha = e->cursor - (v.tem_destaque ? 1 : 0);
        if (linha < 0 || linha >= v.n) return;
        i = v.linhas[linha].indice;
    }
    if (i < 0 || i >= e->n_acervo) return;

    const obra_t *o = &e->acervo[i];

    if (o->estado != OBRA_AQUI) {
        // Não abre: a linha mostra o progresso e a pessoa fica na estante.
        (void)uso_acervo_baixa(ap->hal, e, o->id);
        return;
    }

    e->obra_aberta = *o;
    e->leitor_erro[0] = '\0';

    // O texto vem EMPRESTADO da PSRAM (`memoria_hal.h`): um static aqui já
    // estourou a DRAM no link.
    solta_o_texto(ap);

    size_t cabe = 0;
    char *texto = ap->hal->emprestar
                ? ap->hal->emprestar(LEITOR_TEXTO_MAX, LEITOR_TEXTO_MIN, &cabe)
                : NULL;
    if (!texto) {
        snprintf(e->leitor_erro, sizeof e->leitor_erro, "%s",
                 "Sem memória para abrir");
        empilha(e, TELA_LEITOR);
        return;
    }
    ap->texto_emprestado = texto;

    if (acervo_le_texto(ap->hal, o->id, texto, cabe) != OK) {
        // Cópia danificada NÃO abre.
        snprintf(e->leitor_erro, sizeof e->leitor_erro, "%s",
                 "Não foi possível abrir");
        solta_o_texto(ap);
        empilha(e, TELA_LEITOR);
        return;
    }

    // Reconhece a resposta do CATÁLOGO gravada por engano como texto (uma
    // versão do protocolo fez isso): descarta só a cópia local e mantém a origem
    // online.
    if (texto[0] == '{' && strstr(texto, "\"obras\"") != NULL) {
        snprintf(e->leitor_erro, sizeof e->leitor_erro, "%s",
                 "Cópia incompleta. Baixe novamente");
        obra_t ruim = *o;
        solta_o_texto(ap);
        (void)uso_acervo_descarta_local(ap->hal, e, &ruim);
        empilha(e, TELA_LEITOR);
        return;
    }

    leitor_abre(&e->leitor, texto, (int32_t)strlen(texto), o->offset_texto);
    int cfg = o->fonte;
    if (cfg < 0 || cfg > 41) cfg = 1;
    leitor_recompoe(&e->leitor, (leitor_tamanho_t)(cfg % 3));
    leitor_familia(&e->leitor, (leitor_familia_t)(cfg / 3));
    int ali = o->alinhamento;
    if (ali < 0 || ali > 3) ali = LEITOR_JUSTIFICADO;
    e->leitor.alinhamento = (leitor_alinhamento_t)ali;
    leitor_quebra_com(&e->leitor, quebra_pagina_real);
    e->leitor_na_capa = o->offset_texto == 0;

    // A prova diária usa a miniatura (245 bytes), não a capa grande; a grande
    // só é lida na página de capa.
    if (o->tem_capa && e->rede == REDE_LIGADA &&
        e->nuvem_esperando == NUVEM_NADA) {
        uint8_t prova[378];
        erro_t capa_ok = acervo_le_capa_mini(ap->hal, o->id,
                                             prova, 245);
        if (capa_ok == OK)
            capa_ok = acervo_le_capa_destaque(ap->hal, o->id,
                                              prova, sizeof prova);
        if (capa_ok != OK) {
            (void)uso_acervo_capas_pequenas_baixa(ap->hal, e, o->id);
        }
    }
    e->leitor_texto = texto;
    e->paginas_total = 0;      // varrido de novo: é outra obra
    e->pagina_atual = 1;
    atualiza_pagina(ap);
    (void)acervo_marca_aberta(ap->hal, o->id);
    empilha(e, TELA_LEITOR);
}

static void ok_na_obra(app_t *ap)
{
    estado_t *e = &ap->estado;
    vista_acervo_t v;
    vista_acervo(e, &v);
    int cursor = -1;
    if (v.tem_destaque &&
        strcmp(e->acervo[v.destaque.indice].id, e->obra_aberta.id) == 0)
        cursor = 0;
    for (int n = 0; cursor < 0 && n < v.n; n++)
        if (strcmp(e->acervo[v.linhas[n].indice].id, e->obra_aberta.id) == 0)
            cursor = n + (v.tem_destaque ? 1 : 0);
    if (cursor < 0) return;
    e->cursor = (int16_t)cursor;
    ok_no_acervo(ap);
}

// ── o OK em QUANDO ───────────────────────────────────────────────────
// Pela AÇÃO que a vista declarou, não pelo texto.
static void ok_no_quando(app_t *ap)
{
    estado_t *e = &ap->estado;

    vista_quando_t v;
    vista_quando(e, &v);
    if (e->cursor < 0 || e->cursor >= v.cartao.n_dest) return;

    switch (v.acao[e->cursor]) {
    case QUANDO_CALENDARIO:
        // A grade de ESCOLHA começa no dia em que a coisa está.
        e->dia_visto = e->aberto.vence.ano ? e->aberto.vence : e->hoje;
        empilha(e, TELA_ESCOLHER_DIA);
        return;

    case QUANDO_SEM_DATA: {
        data_t nenhuma;
        memset(&nenhuma, 0, sizeof nenhuma);
        (void)uso_mudar_data(ap->hal, e, &e->aberto, nenhuma, e->aberto.hora);
        break;
    }

    case QUANDO_DIA:
    default:
        // A HORA vai junto, a de agora: mudar o dia não mexe na hora.
        (void)uso_mudar_data(ap->hal, e, &e->aberto, v.datas[e->cursor],
                             e->aberto.hora);
        break;
    }

    volta_ao_item(ap, "Remarcado");
}

// ── o OK em MUDAR A HORA ─────────────────────────────────────────────
// Só a hora entra; "dia inteiro" é tirá-la (`start.date` do Calendar).
static void ok_no_horario(app_t *ap)
{
    estado_t *e = &ap->estado;

    vista_quando_t v;
    vista_horario(e, &v);
    if (e->cursor < 0 || e->cursor >= v.cartao.n_dest) return;

    // O MOSTRADOR abre no horário que a coisa já tem.
    if (v.acao[e->cursor] == QUANDO_RELOGIO) {
        int h = 9, m = 0;
        if (e->aberto.hora[0]) sscanf(e->aberto.hora, "%d:%d", &h, &m);
        e->escolha_h = (int8_t)h;
        e->escolha_m = (int8_t)m;
        e->escolha_campo = 0;
        empilha(e, TELA_ESCOLHER_HORA);
        return;
    }

    const char *hora = v.acao[e->cursor] == QUANDO_DIA_INTEIRO
                     ? "" : v.horas[e->cursor];

    (void)uso_mudar_data(ap->hal, e, &e->aberto, e->aberto.vence, hora);

    volta_ao_item(ap, hora[0] ? "Hora mudada" : "Agora é dia inteiro");
}

static void ok_nas_acoes(app_t *ap)
{
    estado_t *e = &ap->estado;
    vista_nota_t v;
    vista_nota(e, &v);
    if (e->cursor_overlay < 0 || e->cursor_overlay >= v.n_acoes) return;

    // Pelo ÍCONE, não pelo texto (despachar pelo rótulo já mandou "Desmarcar"
    // para a fala).
    icone_id acao = v.acoes[e->cursor_overlay].icone;
    e->overlay = OVERLAY_NADA;

    if (acao == ICO_RENOMEAR) {
        snprintf(e->digitando, sizeof e->digitando, "%s", e->aberto.titulo);
        e->teclado_modo = 0;
        e->teclado_lin = e->teclado_col = 0;
        empilha(e, TELA_TECLADO);
    } else if (acao == ICO_EVENTO) {
        // De onde saiu, para saber para onde voltar.
        e->voltar_nivel = e->profundidade;
        e->cursor = 0;
        empilha(e, TELA_QUANDO);
    } else if (acao == ICO_RELOGIO) {
        e->voltar_nivel = e->profundidade;
        e->cursor = 0;
        empilha(e, TELA_HORARIO);
    } else if (acao == ICO_LIXO) {
        // A ROTINA pergunta antes: apagar a regra apaga a série inteira no Google.
        if (e->aberto.regra[0]) {
            e->overlay = OVERLAY_APAGAR_ROTINA;
            e->cursor_overlay = 0;
            return;
        }
        (void)uso_apagar_item(ap->hal, e, &e->aberto);
        e->aberto_valido = false;
        if (e->profundidade > 0) e->profundidade--;
    }
}

static void ok_no_som(app_t *ap)
{
    estado_t *e = &ap->estado;
    vista_cartao_t v;
    vista_som(e, &v);
    if (e->cursor < 0 || e->cursor >= v.n_dest) return;

    const char *t = v.dest[e->cursor].titulo;

    if (strcmp(t, "Botão de voz") == 0) {
        // Alterna aqui: duas opções não valem uma tela.
        (void)uso_salvar_ajuste(ap->hal, e, AJUSTE_VOZ_SEGURAR,
                                !e->config.valor[AJUSTE_VOZ_SEGURAR]);
    }
}

static void ok_na_aparencia(app_t *ap)
{
    estado_t *e = &ap->estado;
    vista_cartao_t v;
    vista_aparencia(e, &v);
    if (e->cursor < 0 || e->cursor >= v.n_dest) return;

    const char *t = v.dest[e->cursor].titulo;

    if (strstr(t, "Formato")) {
        (void)uso_salvar_ajuste(ap->hal, e, AJUSTE_HORA24,
                                !e->config.valor[AJUSTE_HORA24]);
    } else if (strstr(t, "Bloquear")) {
        // Cicla 1 → 3 → 5 → 10 → 30 → nunca → 1. Zero é "nunca".
        static const int P[] = { 1, 3, 5, 10, 30, 0 };
        int atual = e->config.valor[AJUSTE_BLOQUEAR_MIN], prox = P[0];
        for (size_t i = 0; i < sizeof P / sizeof P[0]; i++)
            if (P[i] == atual) { prox = P[(i + 1) % (sizeof P / sizeof P[0])]; break; }
        (void)uso_salvar_ajuste(ap->hal, e, AJUSTE_BLOQUEAR_MIN, prox);
    } else {
        // "Data e hora" é destino: acertar data é escolher cinco números. Os campos
        // partem da hora atual.
        uso_campos_do_relogio(e);
        empilha(e, TELA_DATA_HORA);
    }
}

// ▶ ABRE o que está sob o cursor, sem marcar (RN-37: o OK marca, abrir
// custa um toque a mais).
static void abre_sob_o_cursor(app_t *ap)
{
    estado_t *e = &ap->estado;
    static VISTA_TRANSITORIA vista_agenda_t v;
    vista_agenda(e, &v);

    const item_t *alvo = item_do_alvo(e, &v);
    if (!alvo) return;
    if (uso_abrir_item(ap->hal, e, alvo) == OK)
        empilha(e, TELA_NOTA);
}

// ── o lançador: quatro cartões ──────────────────────────────────────
//
//     0 Agenda    1 Acervo
//     2 Jogos     3 Ajustes
//
// Mora aqui porque é ROTEAMENTO (qual tela o OK empilha); a vista sabe só
// rótulo e ícone.
static tela_id area_do_cartao(int i)
{
    switch (i) {
    case 0:  return TELA_AGENDA;
    case 1:  return TELA_ACERVO;
    case 2:  return TELA_JOGOS;
    default: return TELA_AJUSTES;
    }
}

// O foco anda na grade (false quando a tela não é o lançador). A borda NÃO
// dá a volta: o ▶ do Acervo cair na Agenda pareceria botão falhando.
// ── as anotações, por página ─────────────────────────────────────────
// ▲▼ andam e, na ponta, viram a página; ◀▶ viram direto.
static bool vira_pagina_das_anotacoes(app_t *ap, int passo)
{
    estado_t *e = &ap->estado;
    int paginas = (e->anotacoes_total + ANOTACOES_MAX - 1) / ANOTACOES_MAX;
    int nova = e->anotacoes_pagina + passo;
    if (nova < 0 || nova >= paginas) return false;
    e->anotacoes_pagina = (int16_t)nova;
    (void)uso_carregar_anotacoes(ap->hal, e);
    return true;
}

static bool anda_nas_anotacoes(app_t *ap, entrada_t botao)
{
    estado_t *e = &ap->estado;
    if (e->pilha[e->profundidade] != TELA_ANOTACOES || e->overlay) return false;

    if (botao == IN_ESQ || botao == IN_DIR) {
        if (vira_pagina_das_anotacoes(ap, botao == IN_DIR ? 1 : -1)) {
            e->cursor = 0;
            ap->precisa_desenhar = true;
        }
        return true;
    }
    if (botao == IN_BAIXO && e->cursor >= e->n_anotacoes - 1) {
        if (vira_pagina_das_anotacoes(ap, 1)) {
            e->cursor = 0;
            ap->precisa_desenhar = true;
        }
        return true;
    }
    if (botao == IN_CIMA && e->cursor <= 0) {
        if (vira_pagina_das_anotacoes(ap, -1)) {
            e->cursor = (int16_t)(e->n_anotacoes - 1);
            ap->precisa_desenhar = true;
        }
        return true;
    }
    return false;
}

static bool anda_no_lancador(app_t *ap, entrada_t botao)
{
    estado_t *e = &ap->estado;

    if (e->pilha[e->profundidade] != TELA_HOME) return false;
    if (e->travado || e->overlay != OVERLAY_NADA) return false;

    int i = e->lancador;
    const int coluna = i % 2, linha = i / 2;

    switch (botao) {
    case IN_DIR:   if (coluna == 0) i += 1; break;
    case IN_ESQ:   if (coluna == 1) i -= 1; break;
    case IN_BAIXO: if (linha  == 0) i += 2; break;
    case IN_CIMA:  if (linha  == 1) i -= 2; break;
    default: return false;
    }

    if (i != e->lancador) {
        e->lancador = (int8_t)i;
        ap->precisa_desenhar = true;
    }
    return true;
}

// O direcional horizontal na Agenda. False quando não é com ela.
static bool anda_na_home(app_t *ap, entrada_t botao)
{
    estado_t *e = &ap->estado;

    if (e->pilha[e->profundidade] != TELA_AGENDA) return false;
    if (e->travado || e->overlay != OVERLAY_NADA) return false;

    // ── com linha selecionada, o gesto é da LINHA ──────────────────────
    if (e->cursor >= 0) {
        if (botao == IN_ESQ) {          // solta o cursor
            e->cursor = -1;
            ap->precisa_desenhar = true;
            return true;
        }

        // ▶ ABRE o que está sob o cursor (o OK marca).
        abre_sob_o_cursor(ap);
        return true;
    }

    // ── com o CABEÇALHO selecionado, o gesto é do DIA ───────────────────
    // Ontem, hoje e amanhã: o ◀▶ é o "e amanhã?" de relance; procurar o dia 27
    // é o calendário. E NÃO empilha: é a mesma tela com outro dia.
    int passo = botao == IN_DIR ? 1 : -1;
    data_t alvo = data_soma_dias(e->dia_visto, passo);

    if (data_dias_entre(e->hoje, alvo) < -1 || data_dias_entre(e->hoje, alvo) > 1)
        return true;   // a borda segura, e o gesto foi consumido aqui

    e->dia_visto = alvo;
    estado_invalida_dia(e);
    (void)uso_carregar_dia(ap->hal, e);
    ap->precisa_desenhar = true;
    return true;
}


// A faixa de voz sai e o aparelho destrava. Um lugar só para as três
// saídas (resposta, prazo, recusa).
static void fecha_a_fala(app_t *ap)
{
    memset(&ap->estado.gravacao, 0, sizeof ap->estado.gravacao);
    ap->estado.travado = false;
    ap->precisa_desenhar = true;
}

static void aperta_ok(app_t *ap)
{
    estado_t *e = &ap->estado;

    // Desconectar e esquecer: só o "sim" movido de propósito confirma; o OK no
    // "não" desiste (RN-6C).

    if (e->overlay == OVERLAY_ESQUECER) {
        if (e->cursor_overlay == 1) {
            (void)rede_esquece(ap->hal);
            e->wifi_salva[0] = '\0';
            e->wifi_atual[0] = '\0';
            e->wifi_ip[0]    = '\0';
            e->rede          = REDE_DESLIGADA;
            for (int i = 0; i < e->n_redes; i++) e->redes[i].salva = false;
            if (e->cursor > 0) e->cursor--;
        }
        e->overlay = OVERLAY_NADA;
        e->cursor_overlay = 0;
        return;
    }

    if (e->overlay == OVERLAY_DESCONECTAR) {
        if (e->cursor_overlay == 1)
            (void)uso_desvincular(ap->hal, e);
        e->overlay = OVERLAY_NADA;
        e->cursor_overlay = 0;
        return;
    }

    if (e->overlay == OVERLAY_APAGAR_ROTINA) {
        if (e->cursor_overlay == 1) {
            (void)uso_apagar_item(ap->hal, e, &e->aberto);
            e->aberto_valido = false;
            if (e->profundidade > 0) e->profundidade--;
        }
        e->overlay = OVERLAY_NADA;
        e->cursor_overlay = 0;
        return;
    }

    if (e->overlay == OVERLAY_DESCARTAR_OBRA) {
        if (e->cursor_overlay == 1) {
            obra_t obra = e->obra_aberta;
            solta_o_texto(ap);
            if (uso_acervo_descarta_local(ap->hal, e, &obra) == OK) {
                e->obra_aberta = (obra_t){0};
                if (e->profundidade > 0) e->profundidade--;
                snprintf(e->feito, sizeof e->feito, "%s", "Cópia descartada");
                e->feito_ms = e->agora_ms;
            }
        }
        e->overlay = OVERLAY_NADA;
        e->cursor_overlay = 0;
        return;
    }

    if (e->overlay == OVERLAY_DESCARTAR) {
        if (e->cursor_overlay == 1) {
            (void)uso_descartar_captura(ap->hal, e);
            e->overlay = OVERLAY_NADA;
        } else {
            // "não, continuar": volta ao gravador.
            e->overlay = e->pilha[e->profundidade] == TELA_AGENDA
                       ? OVERLAY_NADA : OVERLAY_GRAVANDO;
        }
        e->cursor_overlay = 0;
        return;
    }

    // Estruturando, nada é aceito (ver `vista/gravador.h`).
    if (e->gravacao.fase == GRAV_ESTRUTURANDO) return;

    // RN-12: só o OK finaliza: fecha o WAV, cria o item e manda o áudio num
    // caso de uso só.
    if (e->gravacao.fase != GRAV_PARADA) {
        int8_t trechos = e->gravacao.trechos;
        int32_t ms     = e->gravacao.ms;
        int16_t fatias[GRAV_MAX_TRECHOS];
        memcpy(fatias, e->gravacao.trecho_s, sizeof fatias);

        if (uso_salvar_captura(ap->hal, e) != OK) return;

        // A faixa NÃO some, troca de assunto. Tempo e fatias voltam ao lugar
        // (o caso de uso limpa a gravação): é o que ela mostra durante a espera.
        e->gravacao.fase    = GRAV_ESTRUTURANDO;
        e->gravacao.trechos = trechos;
        e->gravacao.ms      = ms;
        memcpy(e->gravacao.trecho_s, fatias, sizeof fatias);

        // O áudio saiu? Espera, travado. Não saiu (rede caiu): a faixa já diz onde
        // a fala está.
        if (e->nuvem_esperando == NUVEM_CAPTURA) {
            e->gravacao.desde_ms = e->agora_ms;
            e->travado = true;

            // Carimba saída e chegada da voz: upload, transcrição e LLM são do outro
            // lado, e "está lento" precisa de números.
            if (ap->hal->registrar) {
                char msg[48];
                snprintf(msg, sizeof msg, "voz: áudio subiu, %d s de fala",
                         (int)(ms / 1000));
                ap->hal->registrar("app", msg);
            }
        } else {
            // O áudio não saiu: sem servidor esta captura nunca vira nada.
            (void)uso_descartar_fala_nao_enviada(ap->hal, e);
            e->gravacao.nao_enviou = true;
        }

        ap->precisa_desenhar = true;
        return;
    }

    if (e->travado) return;

    if (e->overlay == OVERLAY_ACOES) { ok_nas_acoes(ap); return; }
    if (e->overlay == OVERLAY_MENU)  { ok_na_gaveta(ap);  return; }

    if (e->pilha[e->profundidade] == TELA_AGENDA) { ok_na_home(ap); return; }

    // O mostrador e a grade de ESCOLHA carimbam e voltam ao item, sem segunda
    // confirmação: mudar data se desfaz com outro gesto.
    if (e->pilha[e->profundidade] == TELA_ESCOLHER_HORA) {
        char hhmm[6];
        snprintf(hhmm, sizeof hhmm, "%02d:%02d", e->escolha_h, e->escolha_m);
        (void)uso_mudar_data(ap->hal, e, &e->aberto, e->aberto.vence, hhmm);
        volta_ao_item(ap, "Hora mudada");
        return;
    }

    if (e->pilha[e->profundidade] == TELA_ESCOLHER_DIA) {
        (void)uso_mudar_data(ap->hal, e, &e->aberto, e->dia_visto,
                             e->aberto.hora);
        volta_ao_item(ap, "Remarcado");
        return;
    }

    if (e->pilha[e->profundidade] == TELA_CALENDARIO) {
        // Abrir pede o dia ao servidor quando está fora da janela.
        uso_pedir_o_dia(e);

        // Dia pedido, se está fora da janela e quantos itens havia: separa pedido
        // que não saiu, resposta que não chegou e resposta vazia.
        if (ap->hal->registrar) {
            char msg[72];
            snprintf(msg, sizeof msg, "OK %04d-%02d-%02d · fora=%d · pedido=%s",
                     e->dia_visto.ano, e->dia_visto.mes, e->dia_visto.dia,
                     uso_dia_fora_da_janela(e) ? 1 : 0,
                     e->dia_pedido[0] ? e->dia_pedido : "(nenhum)");
            ap->hal->registrar("dia", msg);
        }
        empilha(e, TELA_DIA);
        return;
    }

    // ── a Home abre a área do cartão em foco ────────────────────────────
    // Entrar numa área troca a composição inteira (refresh completo, pelo
    // motor); aqui só se empilha.
    if (e->pilha[e->profundidade] == TELA_HOME) {
        tela_id area = area_do_cartao(e->lancador);

        // Ajustes mostra quanto sobrou no cartão: mede ao entrar.
        if (area == TELA_AJUSTES) mede_a_memoria(ap);

        // A estante vem do cartão ao ENTRAR, não por quadro.
        if (area == TELA_ACERVO) {
            (void)uso_carregar_acervo(ap->hal, e);

            // O catálogo da conta é pedido na entrada; a estante local aparece já.
            (void)uso_acervo_sincroniza(ap->hal, e);
        }

        // O Acervo suspende o long poll da Agenda enquanto aberto. Entrar na Agenda
        // retoma o pull (`uso_sincronizar` evita duplicata e recusa sem rede).
        if (area == TELA_AGENDA)
            (void)uso_sincronizar(ap->hal, e);

        empilha(e, area);
        ap->precisa_desenhar = true;
        return;
    }

    // O OK no Resultado fecha e devolve a tela, a profundidade e o foco de onde
    // a pessoa falou.
    if (e->pilha[e->profundidade] == TELA_RESULTADO) {
        e->n_resultados = 0;
        voz_volta(e);
        ap->precisa_desenhar = true;
        return;
    }

    if (e->pilha[e->profundidade] == TELA_JOGOS) {
        abre_xadrez(ap);
        ap->precisa_desenhar = true;
        return;
    }

    if (e->pilha[e->profundidade] == TELA_CONFERIR) {
        if (!e->n_resultados) return;

        // Já está indo: segunda tranca contra evento duplicado.
        if (e->esperando_resultado) return;

        // Nas paradas de RESULTADO o OK não faz nada: elas são para rolar.
        int d = e->cursor - e->n_resultados;
        if (d < 0) return;

        if (d == 0) {
            // Confirma e ESPERA: o Resultado entra quando a última ação voltar.
            if (uso_confirmar_resultado(ap->hal, e) == OK) {
                // Sem nada em voo (só anotação), o Resultado entra agora.
                if (!e->esperando_resultado) troca_topo(e, TELA_RESULTADO);
                ap->precisa_desenhar = true;
                return;
            }
        } else {
            (void)uso_descartar_resultado(ap->hal, e);
        }
        voz_volta(e);
        return;
    }

    if (e->pilha[e->profundidade] == TELA_ANOTACOES) {
        // Abre no detalhe do item.
        vista_anotacoes_t v;
        vista_anotacoes(e, &v);
        if (v.alvo >= 0 &&
            uso_abrir_item(ap->hal, e, &e->anotacoes[v.alvo]) == OK)
            empilha(e, TELA_NOTA);
        return;
    }
    if (e->pilha[e->profundidade] == TELA_ACERVO)  { ok_no_acervo(ap);  return; }
    if (e->pilha[e->profundidade] == TELA_OBRA)    { ok_na_obra(ap);    return; }
    if (e->pilha[e->profundidade] == TELA_QUANDO)  { ok_no_quando(ap);  return; }
    if (e->pilha[e->profundidade] == TELA_HORARIO) { ok_no_horario(ap); return; }
    if (e->pilha[e->profundidade] == TELA_TECLADO) { ok_no_teclado(ap); return; }
    if (e->pilha[e->profundidade] == TELA_WIFI)    { ok_no_wifi(ap);    return; }
    if (e->pilha[e->profundidade] == TELA_CONTA) {
        // Cursor solto = o ícone de SAIR no canto do card.
        if (e->cursor < 0) {
            vista_cartao_t v;
            vista_conta(&ap->estado, &v);
            if (v.tem_sair) {
                // Pergunta antes, nascendo no "não" (RN-6C).
                e->overlay = OVERLAY_DESCONECTAR;
                e->cursor_overlay = 0;
                ap->precisa_desenhar = true;
            }
            return;
        }
        ok_na_conta(ap);
        return;
    }

    // ── T-27a · o OK gera o código ──────────────────────────────────────
    // E gera OUTRO quando o anterior expirou.
    if (e->pilha[e->profundidade] == TELA_VINCULAR) {
        vista_vincular_t v;
        vista_vincular(e, &v);
        if (v.pode_gerar) {
            (void)uso_parear(ap->hal, e);
            ap->precisa_desenhar = true;
        }
        return;
    }

    // ── T-32 · o interruptor ────────────────────────────────────────────
    // A lista é homogênea: a linha sob o cursor é a agenda de mesmo índice.
    if (e->pilha[e->profundidade] == TELA_SINCRONIZACAO) {
        if (e->cursor < 0 || e->cursor >= e->n_agendas) return;
        (void)uso_escolher_agenda(ap->hal, e, e->cursor,
                                  !e->agendas[e->cursor].ligada);
        ap->precisa_desenhar = true;
        return;
    }

    // Restaurar abre a pergunta de duas etapas do cartão danificado. A fase de
    // inicialização volta a valer e suspende a pilha (RN-6F).
    if (e->pilha[e->profundidade] == TELA_SOBRE) {
        vista_cartao_t v;
        vista_sobre(e, &v);
        if (e->cursor < 0 || e->cursor >= v.n_dest) return;

        const char *t = v.dest[e->cursor].titulo;
        if (strstr(t, "Armazenamento")) {
            mede_a_memoria(ap);
            empilha(e, TELA_ARMAZENAMENTO);
        }
        return;
    }

    if (e->pilha[e->profundidade] == TELA_ARMAZENAMENTO) {
        if (e->espaco_total_kb == 0) return;    // sem medida, sem oferta



        e->inicio.fase                = INICIO_CONFIRMAR_FORMATAR;
        e->inicio.cursor              = 0;      // "não, voltar" (RN-6C)
        e->inicio.formatar_de_ajustes = true;
        troca_topo(e, TELA_INICIO);   // no lugar dela: a pilha já está no teto
        ap->precisa_desenhar = true;
        return;
    }

    // Data e hora: o OK salva os cinco campos e a hora vira hora (RN-6G).
    if (e->pilha[e->profundidade] == TELA_DATA_HORA) {
        vista_relogio_t vr;
        vista_relogio(e, &vr);

        // O interruptor alterna nos DOIS sentidos.
        if (vr.no_interruptor) {
            int novo = e->config.valor[AJUSTE_HORA_REDE] ? 0 : 1;
            (void)uso_salvar_ajuste(ap->hal, e, AJUSTE_HORA_REDE, novo);

            // Ligou com rede: pergunta agora, sem esperar o próximo evento de rede.
            if (novo && e->rede == REDE_LIGADA && ap->hal->hora_da_rede)
                ap->hal->hora_da_rede();

            // Desligou: o cliente NTP DESCE (vivo, o próximo `init` reiniciava a
            // placa).
            if (!novo && ap->hal->hora_da_rede_para)
                ap->hal->hora_da_rede_para();

            // Passou para manual: os campos partem do que está valendo.
            if (!novo) uso_campos_do_relogio(e);

            e->cursor = 0;
        } else if (vr.editavel) {
            // Só nos campos: no fuso, salvar gravaria os campos carregados ao abrir.
            erro_t err = uso_ajustar_relogio(ap->hal, e);
            if (err != OK) e->ultimo_erro = err;

            // A hora nova aparece NO MESMO quadro.
            int h = 0, m = 0;
            ap->hal->relogio(&e->hoje, &h, &m);
            e->hora = (int8_t)h; e->minuto = (int8_t)m;
            e->dia_visto = e->hoje;
            e->hora_confiavel = true;
            estado_invalida_dia(e);   // outro dia, outro conteúdo
        }
        return;
    }

    if (e->pilha[e->profundidade] == TELA_NOTA) {
        // O OK age no botão em FOCO (no máximo dois, na ordem do desenho).
        if (e->aberto_valido) {
            vista_nota_t v;
            vista_nota(e, &v);

            // Numa parada de LEITURA o OK não age.
            int leitura = tela_nota_paradas(&ap->tela, &v) - v.n_botoes;
            int b = e->cursor - leitura;

            if (v.n_botoes > 0 && b >= 0 && b < v.n_botoes) {
                botao_id faz = v.botoes[b].faz;

                if (faz == BOTAO_CONCLUIR || faz == BOTAO_REABRIR) {
                    (void)uso_marcar(ap->hal, e, &e->aberto,
                                     faz == BOTAO_CONCLUIR);

                    // E RELÊ: `e->aberto` é uma cópia, e continuaria "Pendente".
                    item_t alvo_item = e->aberto;
                    (void)uso_abrir_item(ap->hal, e, &alvo_item);

                    ap->precisa_desenhar = true;
                    return;
                }
                return;
            }
        }
        return;
    }
    if (e->pilha[e->profundidade] == TELA_AJUSTES)   { ok_nos_ajustes(ap);  return; }
    if (e->pilha[e->profundidade] == TELA_APARENCIA) { ok_na_aparencia(ap); return; }
    if (e->pilha[e->profundidade] == TELA_SOM)       { ok_no_som(ap);       return; }

    const item_t *it = item_sob_o_cursor(ap);
    if (!it) return;

    // ── na régua do DIA, o híbrido se marca ─────────────────────────────
    // Como na Agenda: a mesma régua, o mesmo gesto. Compromisso continua
    // abrindo (RN-2B).
    if (e->pilha[e->profundidade] == TELA_DIA &&
        it->tipo == TIPO_TAREFA && it->hora[0]) {
        (void)uso_marcar(ap->hal, e, it, !it->feita);
        return;
    }

    // Nas outras listas, o OK abre a linha.
    if (uso_abrir_item(ap->hal, e, it) == OK)
        empilha(e, TELA_NOTA);
}


static void aperta_direita(app_t *ap)
{
    estado_t *es = &ap->estado;

    if (es->pilha[es->profundidade] == TELA_ACERVO) {
        vista_acervo_t v;
        vista_acervo(es, &v);
        int i = -1;
        if (v.tem_destaque && es->cursor == 0) i = v.destaque.indice;
        else {
            int linha = es->cursor - (v.tem_destaque ? 1 : 0);
            if (linha >= 0 && linha < v.n) i = v.linhas[linha].indice;
        }
        if (i >= 0 && i < es->n_acervo) {
            es->obra_aberta = es->acervo[i];
            es->obra_sinopse[0] = '\0';
            (void)acervo_le_sinopse(ap->hal, es->obra_aberta.id,
                                    es->obra_sinopse, sizeof es->obra_sinopse);
            empilha(es, TELA_OBRA);
        }
        return;
    }

    // No detalhe o ▶ não faz nada: é o fim do caminho, e as ações moram no
    // MENU.
    if (es->pilha[es->profundidade] == TELA_NOTA) return;

    if (ap->estado.travado) return;

    const item_t *it = item_sob_o_cursor(ap);
    if (!it) return;

    tela_id destino = TELA_NOTA;
    if (uso_abrir_item(ap->hal, &ap->estado, it) != OK) return;
    empilha(&ap->estado, destino);
}

// Um comando do primeiro uso, com o erro DITO na serial: engolir o retorno
// escondeu um rename que falhava e deixava a tela parada.
static erro_t manda_inicio(app_t *ap, inicio_comando_t cmd, const char *texto)
{
    erro_t err = uso_configurar_dispositivo(ap->hal, &ap->estado, cmd, texto);
    if (err == OK) return OK;

    ap->estado.ultimo_erro = err;
    if (ap->hal->registrar) {
        char linha[64];
        snprintf(linha, sizeof linha, "comando %d falhou com erro %d",
                 (int)cmd, (int)err);
        ap->hal->registrar("inicio", linha);
    }
    return err;
}

// A tela emprestada pela etapa cumpriu (a rede subiu, a conta vinculou):
// desce até o primeiro uso, que segue.
static void etapa_cumprida(app_t *ap, inicio_fase_t fase)
{
    estado_t *e = &ap->estado;
    if (e->pilha[0] != TELA_INICIO || e->profundidade == 0 ||
        e->inicio.fase != fase)
        return;
    e->profundidade = 0;
    e->cursor       = 0;
    manda_inicio(ap, INICIO_CMD_BOOT, NULL);
}

// ── a fase raiz ──────────────────────────────────────────────────────
// Enquanto o primeiro uso não termina, um roteamento só. O power continua
// dormindo.
static void evento_inicializacao(app_t *ap, const evento_t *ev)
{
    estado_t *e = &ap->estado;
    if (ev->tipo != EV_BOTAO) return;

    // BACK VOLTA UMA ETAPA: errar o nome não pode custar reiniciar. Concluído,
    // o primeiro uso fecha; as telas de mídia não voltam. Vale no teclado
    // também (lá quem apaga é a tecla "del").
    if (ev->botao == IN_VOLTAR && inicio_pode_voltar(e->inicio.fase)) {
        inicio_fase_t ant = inicio_fase_anterior(e->inicio.fase);

        // Data e hora só é etapa sem rede; a conta só com rede.
        for (;;) {
            if (ant == INICIO_DATA_HORA && e->rede == REDE_LIGADA)
                ant = inicio_fase_anterior(ant);
            else if (ant == INICIO_CONTA && e->rede != REDE_LIGADA)
                ant = inicio_fase_anterior(ant);
            else break;
        }
        if (ant == INICIO_DATA_HORA && e->inicio.ano < 2024)
            uso_campos_do_relogio(e);

        // Pular é deixar para depois: voltar reabre a etapa.
        if (ant == INICIO_WIFI)  e->inicio.pulou_wifi  = false;
        if (ant == INICIO_WIFI || ant == INICIO_CONTA)
            e->inicio.pulou_conta = false;

        e->inicio.fase   = ant;
        e->inicio.cursor = 0;
        return;
    }

    // ▲▼ movem entre as opções; fases sem opção ignoram.
    int n_opcoes = (e->inicio.fase == INICIO_MEMORIA_REPARO ||
                    e->inicio.fase == INICIO_CONFIRMAR_FORMATAR) ? 2 : 0;
    if (n_opcoes && (ev->botao == IN_CIMA || ev->botao == IN_BAIXO)) {
        e->inicio.cursor = (int8_t)(ev->botao == IN_BAIXO
                                    ? (e->inicio.cursor + 1) % n_opcoes
                                    : (e->inicio.cursor + n_opcoes - 1) % n_opcoes);
        return;
    }

    switch (e->inicio.fase) {
    // Diagnóstico é informação: nada a confirmar.
    case INICIO_MEMORIA_AUSENTE:
    case INICIO_MEMORIA_COMUNICACAO:
    case INICIO_MEMORIA_SOMENTE_LEITURA:
    case INICIO_MEMORIA_CHEIA:
    case INICIO_FORMATO_FUTURO:
    case INICIO_PREPARANDO:
        return;

    case INICIO_BOAS_VINDAS:
        if (ev->botao == IN_OK) e->inicio.fase = INICIO_NOME;
        return;

    case INICIO_NOME:
        // O teclado do resto do aparelho, em contexto tipado.
        e->teclado_contexto = TECLADO_PROPRIETARIO;
        if (ev->botao == IN_CIMA)  { anda_teclado(e, -1, 0); return; }
        if (ev->botao == IN_BAIXO) { anda_teclado(e,  1, 0); return; }
        if (ev->botao == IN_ESQ)   { anda_teclado(e, 0, -1); return; }
        if (ev->botao == IN_DIR)   { anda_teclado(e, 0,  1); return; }
        if (ev->botao == IN_OK)    ok_no_teclado(ap);
        return;

    case INICIO_CONFIRMAR_DONO:
        // O ◀ volta ao teclado com o que já estava escrito.
        if (ev->botao != IN_OK) return;
        manda_inicio(ap, INICIO_CMD_SALVAR_NOME, e->inicio.nome_pendente);
        e->digitando[0] = '\0';
        return;

    case INICIO_CONCLUSAO:
        if (ev->botao == IN_OK) manda_inicio(ap, INICIO_CMD_CONCLUIR, NULL);
        return;

    // ── os dois passos que se pulam ─────────────────────────────────────
    // Reusam as telas de Wi-Fi e de vincular: gêmeas para o primeiro uso
    // divergiriam.
    case INICIO_WIFI:
        if (ev->botao == IN_DIR) {
            // "▶ pular"; pular por engano não custa nada (o BACK reabre). Com rede já
            // conectada, o ▶ só segue.
            manda_inicio(ap, e->rede == REDE_LIGADA ? INICIO_CMD_BOOT
                                                    : INICIO_CMD_PULAR_WIFI,
                         NULL);
            return;
        }
        if (ev->botao == IN_OK) {
            e->n_redes = 0;
            e->wifi_procurando = true;
            if (ap->hal->wifi_procurar) ap->hal->wifi_procurar();
            e->wifi_lista = false;
            empilha(e, TELA_WIFI);
        }
        return;

    case INICIO_CONTA:
        // Sem resposta no prazo: OK tenta de novo, ▶ pula (abaixo).
        if (estado_servidor_sem_resposta(e) && ev->botao == IN_OK) {
            e->inicio.espera_desde_ms = e->agora_ms ? e->agora_ms : 1;
            e->registro_tentado = false;
            if (e->nuvem_esperando == NUVEM_REGISTRAR)
                e->nuvem_esperando = NUVEM_NADA;
            if (!e->hora_confiavel && ap->hal->hora_da_rede)
                ap->hal->hora_da_rede();     // a hora chega e registra
            else
                (void)uso_nuvem_ligar(ap->hal, e);
            return;
        }
        // Ainda se apresentando ao servidor: espera, sem pular.
        if (!e->tem_token && !estado_servidor_sem_resposta(e) &&
            (ev->botao == IN_DIR || ev->botao == IN_OK))
            return;
        // Conectado ao servidor: só o OK segue, para a tela da conta.
        if (e->tem_token && !e->nome[0] && !e->inicio.servidor_visto) {
            if (ev->botao == IN_OK) e->inicio.servidor_visto = true;
            return;
        }
        // Já vinculada: ▶ e OK seguem para a próxima pendência.
        if (e->nome[0] && (ev->botao == IN_DIR || ev->botao == IN_OK)) {
            manda_inicio(ap, INICIO_CMD_BOOT, NULL);
            return;
        }
        if (ev->botao == IN_DIR) {
            manda_inicio(ap, INICIO_CMD_PULAR_CONTA, NULL);
            return;
        }
        if (ev->botao == IN_OK) {
            // A tela de Conectar assume; lá o OK gera o código.
            empilha(e, TELA_VINCULAR);
        }
        return;

    case INICIO_MEMORIA_REPARO:
        // RN-6C: escolher "formatar" abre a pergunta, que nasce de novo no "não".
        if (ev->botao == IN_OK && e->inicio.cursor == 1) {
            e->inicio.fase   = INICIO_CONFIRMAR_FORMATAR;
            e->inicio.cursor = 0;
        }
        // "verificar no computador": nada a fazer no aparelho.
        return;

    case INICIO_CONFIRMAR_FORMATAR:
        // Desistir volta para onde a pergunta começou: o reparo, ou Armazenamento.
        if (ev->botao == IN_VOLTAR ||
            (ev->botao == IN_OK && e->inicio.cursor != 1)) {
            if (e->inicio.formatar_de_ajustes) {
                e->inicio.fase                = INICIO_HOME;
                e->inicio.formatar_de_ajustes = false;
                troca_topo(e, TELA_ARMAZENAMENTO);
            } else {
                e->inicio.fase = INICIO_MEMORIA_REPARO;
            }
            e->inicio.cursor = 0;
            return;
        }
        if (ev->botao != IN_OK) return;
        // Só aqui, depois de duas telas e do cursor movido de propósito. A tela de
        // preparação é desenhada ANTES; quem dispara é o app_passo.
        e->inicio.fase              = INICIO_PREPARANDO;
        e->inicio.formatar_pendente = true;

        // Restaurar é trocar de dono: com rede, a conta sai do servidor antes (o
        // formato espera a resposta ou o prazo). Sem rede, solta-se pelo app.
        if (e->rede == REDE_LIGADA && e->tem_token && e->nome[0])
            (void)uso_desvincular(ap->hal, e);
        return;

    case INICIO_DATA_HORA: {
        // ◀▶ escolhe o campo, ▲▼ muda o valor: o mesmo gesto de Ajustes.
        if (edita_data_hora(e, ev->botao)) return;
        if (ev->botao == IN_OK) manda_inicio(ap, INICIO_CMD_SALVAR_HORA, NULL);
        return;
    }

    case INICIO_HOME:
        return;
    }
}

// ── o aparelho que ainda não tem token ──────────────────────────────
// Tenta se registrar a cada trinta segundos, com ou sem primeiro uso (o
// ciclo normal exige conta). Um 401 zera `registro_ms` e a próxima volta já
// se reapresenta.
static void tenta_registro(app_t *ap)
{
    estado_t *e = &ap->estado;
    if (!e->tem_token &&
        e->rede == REDE_LIGADA && e->hora_confiavel &&
        e->nuvem_esperando == NUVEM_NADA &&
        e->agora_ms - e->registro_ms > 30u * 1000u)
        (void)uso_nuvem_ligar(ap->hal, e);
}

void app_evento(app_t *ap, const evento_t *ev)
{
    estado_t *e = &ap->estado;
    e->eventos_vistos++;
    assenta_inicio(e);

    if (ev->tipo == EV_BOTAO || ev->tipo == EV_BOTAO_APERTO)
        e->ultimo_toque_ms = ap->hal->agora_ms();

    // No modo PTT, a borda de APERTAR começa ou retoma, e soltar pausa. No
    // modo "um toque" a borda é ignorada. Apertar gravando não pausa.
    evento_t gesto;
    if (ev->tipo == EV_BOTAO_APERTO) {
        if (ev->botao != IN_VOZ ||
            !e->config.valor[AJUSTE_VOZ_SEGURAR] ||
            e->gravacao.fase == GRAV_GRAVANDO)
            return;
        gesto = *ev;
        gesto.tipo = EV_BOTAO;
        ev = &gesto;
    }

    // O RELÓGIO ANDA SEMPRE, antes de qualquer roteamento (no onboarding ele
    // ficava parado). Redesenhar é pelo MINUTO: por segundo seria desgaste do
    // painel.
    if (ev->tipo == EV_TICK) {
        e->agora_ms = ap->hal->agora_ms();

        // O que a PESSOA espera do servidor anima uma vez por segundo, e só
        // enquanto houver: sem isso trabalhando e travado têm a mesma cara, e no
        // Conferir apertar de novo duplica o evento. Vale também para os gestos
        // (o ícone da barra precisa sumir ao terminar). O pull de fundo não conta:
        // com long polling seria quase sempre.
        if (e->gravacao.fase == GRAV_ESTRUTURANDO || e->esperando_resultado ||
            vista_espera_visivel(e->nuvem_esperando))
            ap->precisa_desenhar = true;

        // A Conta do primeiro uso esperando o servidor: o prazo corre e os
        // pontinhos andam.
        if (e->inicio.fase == INICIO_CONTA && !e->tem_token) {
            if (!e->inicio.espera_desde_ms)
                e->inicio.espera_desde_ms = e->agora_ms ? e->agora_ms : 1;
            if (e->pilha[e->profundidade] == TELA_INICIO)
                ap->precisa_desenhar = true;
        } else {
            e->inicio.espera_desde_ms = 0;
        }

        int h = 0, m = 0;
        data_t hoje = e->hoje;
        ap->hal->relogio(&hoje, &h, &m);

        if (h != e->hora || m != e->minuto || !data_igual(hoje, e->hoje))
            ap->precisa_desenhar = true;

        // Virou o dia: a janela anda com o relógio e o cartão não cresce sozinho.
        if (!data_igual(hoje, e->hoje)) {
            e->hoje = hoje;
            (void)uso_poda_a_janela(ap->hal, e);
        }
        e->hoje   = hoje;
        e->hora   = (int8_t)h;
        e->minuto = (int8_t)m;

        // A carga redesenha por FAIXA da barra (quatro estados), não por ponto.
        // -1 ("não sei") mantém o último valor: uma falha de I2C não esvazia a barra.
        if (ap->hal->bateria) {
            int b = ap->hal->bateria();
            if (b >= 0) {
                int antes  = e->bateria / 25;
                int agora_ = b / 25;
                if (antes != agora_) ap->precisa_desenhar = true;
                e->bateria = (int8_t)b;
            }
        }
    }

    // O cartão pode sumir DEPOIS do boot (tampa, contato, queda): a tela de
    // memória aparece na hora, inclusive gravando (RN-A3). O hal atualiza o
    // estado ao falhar; aqui só se pergunta, no tick.
    if (ev->tipo == EV_TICK && e->inicio.fase == INICIO_HOME) {
        memoria_estado_t agora = ap->hal->memoria_estado();
        if (agora != MEMORIA_PRONTA) {
            if (e->gravacao.fase != GRAV_PARADA)
                (void)ap->hal->audio_descarta();

            inicio_fase_t antes = e->inicio.fase;
            (void)uso_configurar_dispositivo(ap->hal, e, INICIO_CMD_BOOT, NULL);

            // SÓ redesenha se a fase mudou.
            if (e->inicio.fase != antes) ap->precisa_desenhar = true;
            return;
        }
    }

    // Nas telas de mídia não há onde escrever: só os botões do primeiro uso
    // chegam. Fora delas, rádio, relógio, servidor e tick passam pelo mesmo
    // código.
    bool cartao_pronto = e->inicio.fase == INICIO_HOME ||
                         e->inicio.fase >= INICIO_BOAS_VINDAS;
    if (!cartao_pronto && ev->tipo != EV_BOTAO) return;

    garante_cache(ap);

    // Avisos saem com QUALQUER botão, e o botão CONTINUA valendo (engolir o
    // toque fazia três toques por um gesto). Pergunta não: essa espera
    // resposta.
    if (ev->tipo == EV_BOTAO && e->feito[0]) {
        e->feito[0] = '\0';
        ap->precisa_desenhar = true;
    }

    if (ev->tipo == EV_BOTAO &&
        (e->precisa_rede[0] || e->recusa != RECUSA_NADA)) {
        bool ok = ev->botao == IN_OK;
        int8_t motivo = e->recusa;

        e->precisa_rede[0] = '\0';
        e->recusa = RECUSA_NADA;
        ap->precisa_desenhar = true;

        // O OK LEVA ao lugar de resolver, conforme o motivo, e consome o toque
        // ("sim, me leva lá").
        if (ok && !e->travado) {
            tela_id destino = motivo == RECUSA_MINUTOS ? TELA_FALA
                                                       : TELA_CONTA;

            // Menos quando já está lá: empilhar a mesma tela custaria dois BACK.
            if (e->pilha[e->profundidade] != destino) {
                e->overlay = OVERLAY_NADA;
                empilha(e, destino);
            }
        }

        // O OK é consumido (fechou o aviso); os outros botões seguem. O ● é o gesto
        // de novo: a rede voltou, e tem de gravar.
        if (ok) return;
    }

    // A faixa de "sem resposta" também é aviso: qualquer botão a dispensa e
    // segue valendo; o OK só fecha. Senão o ● seria engolido para sempre.
    if (ev->tipo == EV_BOTAO && (e->gravacao.sem_resposta ||
                                 e->gravacao.nao_enviou ||
                                 e->gravacao.nada_entendido ||
                                 e->gravacao.nao_comecou != OK)) {
        bool ok = ev->botao == IN_OK;
        fecha_a_fala(ap);
        if (ok) return;
    }

    // O BACK SEGURADO passa direto: é o botão de pânico (RN-38).
    if (ev->tipo == EV_BOTAO && !e->n_resultados && ev->botao != IN_VOZ &&
        !(ev->botao == IN_VOLTAR && ev->ms >= 600) &&
        e->pilha[e->profundidade] == TELA_CONFERIR) {
        voz_volta(e);
        ap->precisa_desenhar = true;
        return;
    }

    // ── o Conferir não tem saída lateral ────────────────────────────────
    // Só descartar ou confirmar: sair sem decidir deixaria uma fala estruturada
    // esperando sem tela que a mostre. ▲▼ e OK são a decisão; o BACK segurado é a
    // saída de emergência.
    if (ev->tipo == EV_BOTAO && e->n_resultados &&
        e->pilha[e->profundidade] == TELA_CONFERIR &&
        ev->botao != IN_OK && ev->botao != IN_CIMA && ev->botao != IN_BAIXO &&
        !(ev->botao == IN_VOLTAR && ev->ms >= 600))
        return;

    // Bloqueado, só o power desbloqueia (o INT do PCF acorda por qualquer
    // botão, sem conceder ação). A voz recebe resposta explícita.
    if (ev->tipo == EV_BOTAO &&
        e->pilha[e->profundidade] == TELA_BLOQUEADA) {
        if (ev->botao == IN_POWER) {
            destrava_o_vidro(e);
            ap->precisa_desenhar = true;
        } else if (ev->botao == IN_VOZ) {
            e->aviso_desbloqueio = true;
            e->aviso_ate_ms = ap->hal->agora_ms() + 2000;
            ap->precisa_desenhar = true;
        }
        return;
    }

    // Os botões do primeiro uso. O power não entra: trava como em todo lugar.
    if (ev->tipo == EV_BOTAO && ev->botao != IN_POWER &&
        e->pilha[e->profundidade] == TELA_INICIO) {
        evento_inicializacao(ap, ev);
        ap->precisa_desenhar = true;
        return;
    }

    switch (ev->tipo) {
    case EV_BOTAO:
        if (!(ev->botao == IN_VOLTAR && ev->ms >= 600) &&
            entrada_xadrez(ap, ev->botao)) return;
        switch (ev->botao) {
        // Em Data e hora o direcional EDITA campo, como no primeiro uso.
        case IN_CIMA:
        case IN_BAIXO:
            if (edita_no_relogio(ap, ev->botao)) break;
            if (anda_nas_anotacoes(ap, ev->botao)) break;
            // A Home é GRADE: ▲▼ andam de fileira. Intercepta antes de `anda_cursor`,
            // que não representa duas dimensões.
            if (anda_no_lancador(ap, ev->botao)) break;
            anda_cursor(ap, ev->botao == IN_CIMA ? -1 : +1);
            break;

        case IN_OK:  aperta_ok(ap); break;

        // ● grava DE QUALQUER TELA: a ideia aparece em outro lugar.
        case IN_VOZ:
            // Sem rede o ● NÃO grava, e diz na hora: falar é comando, e a recusa é no
            // gesto, antes da ideia inteira ser dita.
            if (e->gravacao.fase == GRAV_PARADA && e->rede != REDE_LIGADA) {
                snprintf(e->precisa_rede, sizeof e->precisa_rede, "%s",
                         "usar a voz");
                break;
            }
            if (e->gravacao.fase == GRAV_PARADA) {
                erro_t err = uso_gravar_comeca(ap->hal, e);
                if (err == OK) abre_gravador(e);
                else {
                    // Falhou ao começar: a faixa abre AQUI com o motivo (o Sobre ninguém abre
                    // no meio de uma ideia).
                    e->ultimo_erro = err;
                    e->gravacao.nao_comecou = err;
                    abre_gravador(e);
                }
            } else if (e->gravacao.fase == GRAV_ESTRUTURANDO) {
                // Travado, o ● não faz nada (senão o hal reabria um áudio já enviado).
                break;
            } else if (e->gravacao.fase == GRAV_GRAVANDO) {
                // RN-12: soltar PAUSA, não termina.
                (void)uso_gravar_pausa(ap->hal, e);
            } else {
                (void)uso_gravar_retoma(ap->hal, e);
            }
            break;

        // RN-14: gravação aberta NÃO dorme.
        case IN_POWER:
            if (e->gravacao.fase != GRAV_PARADA) break;
            if (e->pilha[e->profundidade] == TELA_BLOQUEADA) {
                destrava_o_vidro(e);
            } else {
                (void)dado_log(ap->hal, LOG_DORMIU, 0, e->hora, e->minuto);
                trava_o_vidro(e);
            }
            break;

        // RN-3F: o MENU abre a gaveta e mais nada; apertar de novo fecha. É
        // pop-over: não gasta nível da pilha.
        case IN_MENU:
            // O MENU é o submenu DA PÁGINA. No detalhe ele abre as ações do item
            // (renomear, apagar...). Na Home não abre nada: ela já é o espaço de
            // opções, e uma gaveta ali seria navegação concorrente.
            if (e->pilha[e->profundidade] == TELA_NOTA) {
                if (!e->aberto_valido) break;
                e->overlay = e->overlay == OVERLAY_ACOES ? OVERLAY_NADA
                                                         : OVERLAY_ACOES;
                e->cursor_overlay = 0;
                break;
            }

            // Só abre onde há gaveta: a da Agenda (Calendário) e as do Acervo.
            // Em Ajustes e Jogos a gaveta da Agenda aparecia fora do app dela.
            switch (e->pilha[e->profundidade]) {
            case TELA_AGENDA:
            case TELA_DIA:
            case TELA_ACERVO:
            case TELA_OBRA:
            case TELA_LEITOR:
                e->overlay = e->overlay == OVERLAY_MENU ? OVERLAY_NADA
                                                        : OVERLAY_MENU;
                e->cursor_overlay = 0;
                break;
            default:
                break;
            }
            break;


        // RN-38: BACK segurado volta à Home de qualquer profundidade.
        case IN_VOLTAR:
            // Gravando, o BACK DESCARTA, e passa pela confirmação (RN-A2). Depois do OK,
            // quem descarta é o Conferir.
            if ((e->gravacao.fase == GRAV_GRAVANDO ||
                 e->gravacao.fase == GRAV_PAUSADA) &&
                e->overlay != OVERLAY_DESCARTAR) {
                e->overlay = OVERLAY_DESCARTAR;
                e->cursor_overlay = 0;      // o "não" pré-selecionado
                break;
            }
            if (e->overlay == OVERLAY_DESCARTAR) {
                e->overlay = e->pilha[e->profundidade] == TELA_AGENDA
                           ? OVERLAY_NADA : OVERLAY_GRAVANDO;
                break;
            }

            // Sair do LEITOR devolve a PSRAM emprestada.
            if (e->pilha[e->profundidade] == TELA_LEITOR && ev->ms < 600)
                solta_o_texto(ap);

            // A gaveta sai da frente primeiro: é o que está por cima.
            if (e->overlay != OVERLAY_NADA && ev->ms < 600) {
                e->overlay = OVERLAY_NADA;
                break;
            }
            // O teclado de Wi-Fi trocou o topo: o BACK devolve a lista de redes. Da
            // lista, volta ao card de Conexão.
            if (ev->ms < 600 &&
                e->pilha[e->profundidade] == TELA_WIFI && e->wifi_lista) {
                e->wifi_lista = false;
                e->cursor = 0;
                break;
            }

            if (ev->ms < 600 &&
                e->pilha[e->profundidade] == TELA_TECLADO &&
                (e->teclado_contexto == TECLADO_WIFI ||
                 e->teclado_contexto == TECLADO_WIFI_SSID)) {
                e->digitando[0] = '\0';
                e->wifi_lista = false;
        troca_topo(e, TELA_WIFI);
                break;
            }

            if (ev->ms >= 600) {
                // O BACK segurado também encerra a ESPERA: sem saída, servidor fora do ar
                // seria um minuto e meio de tijolo. A gravação vai junto.
                if (e->gravacao.fase == GRAV_ESTRUTURANDO) {
                    (void)uso_descartar_fala_nao_enviada(ap->hal, e);
                    memset(&e->gravacao, 0, sizeof e->gravacao);
                    e->travado = false;
                }

                // RN-38: o botão de pânico encerra também a volta da voz.
                e->overlay      = OVERLAY_NADA;
                e->veio_da_voz  = false;
                e->profundidade = 0;
                e->cursor       = 0;
                // A raiz volta a ser a HOME explicitamente: o resultado da voz ocupa
                // `pilha[0]` enquanto aberto, e ficaria como raiz sem saída.
                e->pilha[0]     = TELA_HOME;
            } else {
                voz_volta(e);
            }
            break;

        // RN-3F: o ◀ NUNCA volta. Na Agenda, ◀▶ andam entre os dias; o calendário
        // está na gaveta do MENU.
        case IN_ESQ:
        case IN_DIR:
            if (edita_no_relogio(ap, ev->botao)) break;
            if (anda_nas_anotacoes(ap, ev->botao)) break;
            if (anda_no_lancador(ap, ev->botao)) break;
            if (anda_na_home(ap, ev->botao)) break;

            if (e->pilha[e->profundidade] == TELA_TECLADO) {
                anda_teclado(e, 0, ev->botao == IN_DIR ? +1 : -1);
            } else if (e->pilha[e->profundidade] == TELA_LEITOR) {
                // No leitor ◀▶ viram PÁGINA.
                if (ev->botao == IN_DIR) {
                    if (e->leitor_na_capa) e->leitor_na_capa = false;
                    else leitor_avanca(&e->leitor);
                } else {
                    if (!e->leitor_na_capa && leitor_posicao(&e->leitor) == 0)
                        e->leitor_na_capa = true;
                    else if (!e->leitor_na_capa)
                        leitor_volta(&e->leitor);
                }
                atualiza_pagina(ap);
            } else if (e->pilha[e->profundidade] == TELA_ESCOLHER_HORA) {
                // ◀▶ troca de casa, ▲▼ muda o número.
                e->escolha_campo = ev->botao == IN_DIR ? 1 : 0;
            } else if (e->pilha[e->profundidade] == TELA_CALENDARIO ||
                       e->pilha[e->profundidade] == TELA_ESCOLHER_DIA) {
                // Na grade de ESCOLHA ◀▶ andam de dia também: é a mesma grade.
                anda_dia(ap, ev->botao == IN_DIR ? +1 : -1);
            } else if (ev->botao == IN_DIR) {
                aperta_direita(ap);
            }
            break;

        default: break;
        }
        break;

    case EV_TICK: {
        uint32_t agora = e->agora_ms;
        bloqueia_se_ocioso(ap, agora);

        // ENTRAR em Minha Conta avisa da conta caída, uma vez (o terceiro momento
        // do lembrete; os outros dois estão em `uso/nuvem.c`). `tela_anterior`
        // garante que vale só na entrada.
        if (e->pilha[e->profundidade] == TELA_CONTA &&
            ap->tela_anterior != TELA_CONTA && e->conta_reconectar) {
            e->recusa = RECUSA_CONTA;
            ap->precisa_desenhar = true;
        }

        // O recibo do gesto expira sozinho.
        if (e->feito[0] && agora - e->feito_ms > FEITO_PRAZO_MS) {
            e->feito[0] = '\0';
            ap->precisa_desenhar = true;
        }

        // Guardado antes de expirar: o aviso sumindo é mudança visível.
        bool ja_avisou = e->aviso_desbloqueio;
        if (e->aviso_desbloqueio &&
            (int32_t)(agora - e->aviso_ate_ms) >= 0)
            e->aviso_desbloqueio = false;

        // Com a Biblioteca à vista, o catálogo acompanha o app. Fora dela, nada de
        // polling.
        bool vendo_acervo = e->pilha[e->profundidade] == TELA_ACERVO;
        if (vendo_acervo && e->rede == REDE_LIGADA &&
            e->nuvem_esperando == NUVEM_NADA &&
            e->gravacao.fase == GRAV_PARADA &&
            agora - e->acervo_ultimo_ms > 5u * 1000u) {
            e->acervo_ultimo_ms = agora;
            (void)uso_acervo_sincroniza(ap->hal, e);
        }


        // ── a rede, no ritmo de uma agenda de mesa ──────────────────────────
        // Pull a cada 5 s, com `esperar=1`: o servidor segura até 25 s, e a janela
        // fica aberta ~83% do tempo. A latência vira o tempo de o servidor perceber;
        // o custo é uma requisição pendurada. O gesto tem linha própria e não
        // espera. Nunca durante a gravação (o upload não disputa a fila), e cede a
        // vez a gestos guardados.
        if (!vendo_acervo && e->nome[0] && e->rede == REDE_LIGADA &&
            e->nuvem_esperando == NUVEM_NADA &&
            e->gesto_n == 0 && !e->agendas_querendo &&
            !e->agenda_querendo &&
            e->gravacao.fase == GRAV_PARADA &&
            // O resto do lote não espera o ciclo: o servidor disse que tem mais.
            (e->pull_tem_mais || agora - e->nuvem_ultimo_ms > 5u * 1000u)) {
            e->nuvem_ultimo_ms = agora;
            (void)uso_sincronizar(ap->hal, e);
        }

        // A rede caiu no meio da fala: não existe guardar áudio para depois (a fala
        // de ontem subindo hoje). O arquivo vai embora e a tela diz por quê. A
        // proposta na tela também cai: confirmar seria mandar sem rede.
        if (e->rede != REDE_LIGADA && e->n_resultados > 0 &&
            !e->esperando_resultado) {
            (void)uso_descartar_resultado(ap->hal, e);
            snprintf(e->precisa_rede, sizeof e->precisa_rede, "%s",
                     "A rede caiu. A gravação foi descartada.");
            if (e->pilha[e->profundidade] == TELA_CONFERIR) voz_volta(e);
            ap->precisa_desenhar = true;
        }

        if (e->rede != REDE_LIGADA && e->gravacao.fase != GRAV_PARADA) {
            // Estruturando com a rede caída: não há o que esperar.
            if (e->gravacao.fase == GRAV_ESTRUTURANDO) {
                (void)uso_descartar_fala_nao_enviada(ap->hal, e);
                e->gravacao.fase = GRAV_PARADA;
                e->nuvem_esperando = NUVEM_NADA;
            } else {
                (void)uso_descartar_captura(ap->hal, e);
            }
            snprintf(e->precisa_rede, sizeof e->precisa_rede, "%s",
                     "A rede caiu. A gravação foi descartada.");
            e->travado = false;
            e->overlay = OVERLAY_NADA;
            ap->precisa_desenhar = true;
        }

        // ── a linha da nuvem que ficou presa ────────────────────────────────
        // Prazo do cliente HTTP mais folga: solta DEPOIS de ele desistir, senão a
        // resposta atrasada cairia numa linha com outro dono.
        if (e->nuvem_esperando != NUVEM_NADA &&
            agora - e->nuvem_desde_ms > ESTRUTURA_PRAZO_MS) {
            // A obra que descia volta a ser só online (o `.part` fica). Sem soltar, o
            // próximo texto seria gravado com o nome dela.
            if (e->nuvem_esperando == NUVEM_OBRA ||
                e->nuvem_esperando == NUVEM_CAPA)
                e->obra_baixando[0] = '\0';

            e->nuvem_esperando = NUVEM_NADA;
            e->sinc            = SINC_ERRO;
            e->ultimo_erro     = ERR_TIMEOUT;
        }

        // A linha do PULL solta pelo mesmo prazo.
        if (e->pull_esperando &&
            agora - e->nuvem_desde_ms > ESTRUTURA_PRAZO_MS)
            e->pull_esperando = false;

        // O gesto guardado sai antes de tudo o que o aparelho faz sozinho.
        if (e->gesto_n > 0 && e->nuvem_esperando == NUVEM_NADA)
            (void)uso_gesto_pendente(ap->hal, e);

        // O mês da grade e o dia aberto, logo depois do gesto, na linha do GESTO:
        // a pessoa está com o dedo no botão.
        if (e->nuvem_esperando == NUVEM_NADA)
            (void)uso_olhar(ap->hal, e);

        // A fala que esperou a LINHA (segundos, nunca a rede). Se o Wi-Fi cair
        // nessa janela ela desiste: áudio guardado para depois vira ação que
        // ninguém lembra de ter pedido.
        if (e->captura_pendente[0] && e->rede != REDE_LIGADA) {
            e->captura_pendente[0] = '\0';
            (void)uso_descartar_fala_nao_enviada(ap->hal, e);
            e->gravacao.nao_enviou = true;
            e->sinc = SINC_OCIOSO;
            ap->precisa_desenhar = true;
        }

        if (e->captura_pendente[0] && e->nuvem_esperando == NUVEM_NADA)
            (void)uso_captura_pendente(ap->hal, e);

        if (e->agenda_querendo && e->nuvem_esperando == NUVEM_NADA)
            (void)uso_agenda_pendente(ap->hal, e);

        // ── conectar tem PRAZO ──────────────────────────────────────────────
        // Vinte segundos: associar leva dois ou três; o resto é rede que aceita e
        // não entrega endereço (portal cativo).
        if (e->rede == REDE_CONECTANDO &&
            agora - e->conectando_desde_ms > 20u * 1000u) {
            e->rede = REDE_DESLIGADA;

            // SEM_RESPOSTA, não SENHA: oferecer o teclado não resolveria.
            e->wifi_falha = WIFI_FALHA_SEM_RESPOSTA;
            ap->precisa_desenhar = true;
        }

        // ── o sinal, relido ─────────────────────────────────────────────────
        // A cada dez segundos, conectado (leitura local). O redesenho segue a
        // PALAVRA, não o número.
        if (e->rede == REDE_LIGADA && ap->hal->wifi_estado &&
            agora - e->wifi_lido_ms > 10u * 1000u) {
            e->wifi_lido_ms = agora;

            // Buffer próprio: não reescreve o IP que a Conexão está mostrando.
            char ip[24];
            int forca = 0;
            (void)ap->hal->wifi_estado(ip, sizeof ip, &forca);

            const char *antes = vista_forca_texto(e->wifi_forca);
            e->wifi_forca = (int8_t)forca;
            if (strcmp(antes, vista_forca_texto(e->wifi_forca)) != 0)
                ap->precisa_desenhar = true;
        }

        // ── a hora que não se acertou ───────────────────────────────────────
        // O NTP era pedido só ao conectar, e falhava com o TLS subindo. Aqui se
        // repete até acertar: relógio errado que não se corrige é pior que parado.
        if (!e->hora_confiavel && e->rede == REDE_LIGADA &&
            e->config.valor[AJUSTE_HORA_REDE] && ap->hal->hora_da_rede &&
            agora >= e->ntp_pedido_ms &&
            agora - e->ntp_pedido_ms > 30u * 1000u) {
            e->ntp_pedido_ms = agora ? agora : 1;
            ap->hal->hora_da_rede();
        }

        // A busca de agendas que esperou a vez sai assim que a linha vaga.
        if (e->agendas_querendo && e->nuvem_esperando == NUVEM_NADA)
            (void)uso_agendas(ap->hal, e);

        // E a busca que não volta mais: prazo generoso (25 s de long polling na
        // frente), mas existe — "Buscando" eterno é pedido que morreu.
        if (e->agendas_buscando &&
            agora - e->agendas_desde_ms > 45u * 1000u) {
            e->agendas_buscando = false;
            e->agendas_querendo = false;
            if (e->nuvem_esperando == NUVEM_AGENDAS)
                e->nuvem_esperando = NUVEM_NADA;
            if (e->pilha[e->profundidade] == TELA_SINCRONIZACAO)
                ap->precisa_desenhar = true;
        }

        // Buscando, o tick faz os pontinhos andarem.
        if (e->agendas_buscando &&
            e->pilha[e->profundidade] == TELA_SINCRONIZACAO)
            ap->precisa_desenhar = true;

        tenta_registro(ap);

        // Durante o pareamento, pergunta a cada cinco segundos: a pessoa está com
        // o celular na mão.
        if (e->codigo[0] && e->rede == REDE_LIGADA &&
            e->nuvem_esperando == NUVEM_NADA &&
            agora - e->nuvem_ultimo_ms > 5000u && ap->hal->nuvem_pede) {
            e->nuvem_ultimo_ms = agora;
            e->nuvem_esperando = NUVEM_PAREADO;
            ap->hal->nuvem_pede("/v1/parear/estado", NULL, NULL);
        }

        // ── o prazo da estruturação ─────────────────────────────────────────
        // Pelo pior caso, a resposta que não chega. Estourado, a faixa destrava e
        // diz onde a fala está.
        if (e->gravacao.fase == GRAV_ESTRUTURANDO && !e->gravacao.sem_resposta) {
            if (agora - e->gravacao.desde_ms > ESTRUTURA_PRAZO_MS) {
                // Gravação não tratada não fica no cartão: reprocessar é falar de novo.
            (void)uso_descartar_fala_nao_enviada(ap->hal, e);
            e->gravacao.sem_resposta = true;
                e->travado = false;
            }
            // Os pontos andam com o segundo durante a espera; depois do prazo a faixa
            // fica parada.
            ap->precisa_desenhar = true;
        }

        // O tempo de gravação anda no TICK, e bate com o áudio gravado.
        if (e->gravacao.fase == GRAV_GRAVANDO) {
            e->gravacao.ms += 1000;
            // O segundo entra no trecho ATUAL: fatias proporcionais.
            int i = e->gravacao.trechos - 1;
            if (i >= 0 && i < GRAV_MAX_TRECHOS) e->gravacao.trecho_s[i]++;

            // O contador andou: este tick vale um quadro.
            ap->precisa_desenhar = true;
        }

        // O tick sai por aqui, sem cair no redesenho geral do fim de app_evento:
        // por segundo, para sempre, seria montar a vista à toa. Quem redesenha no
        // tick diz o nome: o minuto, a gravação e o aviso que expirou.
        if (!e->aviso_desbloqueio && ja_avisou) ap->precisa_desenhar = true;
        return;
    }

    // O rádio terminou de varrer; o app copia a lista quando o hal avisa.
    case EV_WIFI_REDES: {
        if (ap->hal->wifi_redes)
            e->n_redes = (int8_t)ap->hal->wifi_redes(e->redes, REDES_MAX);

        // "Salva" é decisão do cartão: é o que faz o OK conectar direto.
        rede_salva_t guardada;
        if (rede_carrega(ap->hal, &guardada) == OK) {
            snprintf(e->wifi_salva, sizeof e->wifi_salva, "%s", guardada.nome);
            for (int i = 0; i < e->n_redes; i++)
                e->redes[i].salva =
                    strcmp(e->redes[i].nome, guardada.nome) == 0;
        } else {
            e->wifi_salva[0] = '\0';
        }

        e->wifi_procurando = false;
        ap->precisa_desenhar = true;
        break;
    }

    // O rádio mudou de estado. Sem IP o aparelho está na rede e não fala com
    // ninguém: não é conectado.
    case EV_WIFI_ESTADO: {
        int forca = 0;
        int estado = ap->hal->wifi_estado
                   ? ap->hal->wifi_estado(e->wifi_ip, sizeof e->wifi_ip, &forca)
                   : 0;
        e->rede = (rede_t)estado;
        e->wifi_forca = (int8_t)forca;

        // Por que caiu, para a Conexão separar senha errada de roteador mudo.
        if (ap->hal->wifi_falha)
            e->wifi_falha = (wifi_falha_t)ap->hal->wifi_falha();
        if (e->rede == REDE_LIGADA) e->wifi_falha = WIFI_FALHA_NENHUMA;

        if (e->rede == REDE_LIGADA) {
            snprintf(e->wifi_atual, sizeof e->wifi_atual, "%s", e->wifi_alvo);

            // Com endereço e a hora pela rede escolhida, é aqui que se pergunta.
            if (e->config.valor[AJUSTE_HORA_REDE] && ap->hal->hora_da_rede &&
                (!e->ntp_pedido_ms ||
                 (e->agora_ms >= e->ntp_pedido_ms &&
                  e->agora_ms - e->ntp_pedido_ms > 30u * 1000u))) {
                e->ntp_pedido_ms = e->agora_ms ? e->agora_ms : 1;
                ap->hal->hora_da_rede();
            }

            // O aparelho se apresenta ANTES de tudo da nuvem: sem token, toda rota dá
            // 401. Depois do registro, `uso_nuvem_ligar` só entrega endereço e token.
            (void)uso_nuvem_ligar(ap->hal, e);

            // E o delta desce: a rede voltou. Só com o aparelho apresentado (senão o
            // pull voltaria 401).
            if (e->nome[0] && e->nuvem_esperando == NUVEM_NADA)
                (void)uso_sincronizar(ap->hal, e);

            etapa_cumprida(ap, INICIO_WIFI);
        }
        else if (e->rede == REDE_DESLIGADA)
            e->wifi_atual[0] = '\0';

        ap->precisa_desenhar = true;
        break;
    }

    // O NTP respondeu: a hora vira hora (RN-6G) e o dia pode ter mudado.
    case EV_HORA_DA_REDE: {
        int h = 0, m = 0;
        ap->hal->relogio(&e->hoje, &h, &m);
        e->hora = (int8_t)h; e->minuto = (int8_t)m;
        e->hora_confiavel = true;
        e->dia_visto = e->hoje;
        estado_invalida_dia(e);
        (void)uso_poda_a_janela(ap->hal, e);   // agora com o "hoje" de verdade

        // Agora o TLS funciona: é agora que o aparelho se apresenta (a tentativa
        // do evento do rádio saiu com o relógio em 1970).
        if (e->rede == REDE_LIGADA && e->nuvem_esperando == NUVEM_NADA)
            (void)uso_nuvem_ligar(ap->hal, e);

        ap->precisa_desenhar = true;
        break;
    }

    // A nuvem respondeu: uso/ aplica pelo contrato; o app entrega e navega.
    case EV_REDE_RESULTADO: {
        // Lê PRIMEIRO, decide DEPOIS: com duas linhas, o pendente pode não ser o
        // que chegou.
        bool estruturando = e->gravacao.fase == GRAV_ESTRUTURANDO;
        (void)uso_nuvem_resposta(ap->hal, e);

        if (e->nome[0]) etapa_cumprida(ap, INICIO_CONTA);

        bool era_a_fala = estruturando &&
                          e->nuvem_respondeu == NUVEM_CAPTURA;

        // ── a última ação voltou: o Resultado entra ─────────────────────────
        // Só agora, com a linha vaga E a fila vazia (com três ações a linha vaga
        // entre uma e outra). TROCA o topo: voltar ao Conferir ofereceria a decisão
        // de novo.
        if (e->esperando_resultado && e->nuvem_esperando == NUVEM_NADA &&
            e->gesto_n == 0) {
            e->esperando_resultado = false;
            troca_topo(e, TELA_RESULTADO);
            ap->precisa_desenhar = true;
        }

        if (era_a_fala && ap->hal->registrar) {
            char msg[48];
            snprintf(msg, sizeof msg, "voz: servidor respondeu em %u ms, %d ações",
                     (unsigned)(e->agora_ms - e->gravacao.desde_ms),
                     e->n_resultados);
            ap->hal->registrar("app", msg);
        }

        if (era_a_fala) {
            if (e->n_resultados) {
                // Entendeu: a faixa sai e o Conferir ocupa a tela.
                fecha_a_fala(ap);
                e->overlay = OVERLAY_NADA;
                voz_mostra(e, TELA_CONFERIR);
            } else if (e->recusa != RECUSA_NADA) {
                // Recusa (minutos, conta, aparelho): a faixa de recusa ocupa o mesmo
                // retângulo e leva ao lugar de resolver.
                fecha_a_fala(ap);
            } else {
                // Respondeu sem ação (fala sem comando vira anotação do lado de lá, então
                // não deveria acontecer): a faixa diz onde a fala está, sem dizer "sem
                // resposta".
                (void)uso_descartar_fala_nao_enviada(ap->hal, e);
                e->gravacao.nada_entendido = true;
                e->travado = false;
            }
        }

        ap->precisa_desenhar = true;
        break;
    }

    case EV_ACORDOU_BLOQUEADO:
        trava_o_vidro(e);
        if (ev->botao == IN_VOZ) {
            e->aviso_desbloqueio = true;
            e->aviso_ate_ms = ap->hal->agora_ms() + 2000;
        }
        break;

    case EV_DOCADO:
        e->docado = ev->valor != 0;
        break;

    default:
        break;
    }

    ap->precisa_desenhar = true;
}

// A identidade da tela desenhada: fase, profundidade e topo da pilha. Telas
// diferentes nunca coincidem; andar dentro da mesma não muda o número. O
// OVERLAY não entra: a gaveta é parte da tela de baixo, e abrir menu não
// pode piscar o painel.
static uint32_t assinatura_da_tela(const app_t *ap)
{
    return (uint32_t)ap->estado.inicio.fase * 100000u
         + (uint32_t)ap->estado.profundidade * 10000u
         + (uint32_t)ap->estado.pilha[ap->estado.profundidade] * 100u
         + 1u;
}

// O único lugar que fala com o vidro, e que diz ao painel se é tela nova ou
// a mesma.
static void manda_pro_vidro(app_t *ap, pintura_t intencao)
{
    ap->hal->mostrar(ap->tela.bits, ap->tela.l, ap->tela.a, intencao);
    ap->precisa_desenhar = false;
}

static bool pinta_capa_do_cartao(app_t *ap, const obra_t *o,
                                 int x, int y, int l, int a)
{
    if (!o || !o->capa_aqui || l < 3 || a < 3) return false;
    if ((l == 240 && a == 360) || (l >= 60 && a >= 90)) {
        size_t real = 0;
        uint8_t *bits = ap->hal->emprestar(10800, 10800, &real);
        if (!bits || acervo_le_capa(ap->hal, o->id, bits, real) != OK) {
            ap->hal->devolver(bits);
            return false;
        }
        for (int dy = 0; dy < a; dy++)
            for (int dx = 0; dx < l; dx++) {
                int sy = dy * 360 / a, sx = dx * 240 / l;
                gfx_pixel(&ap->tela, x + dx, y + dy,
                          bits[sy * 30 + sx / 8] & (0x80u >> (sx % 8)));
            }
        ap->hal->devolver(bits);
        return true;
    }

    uint8_t bits[378];
    const bool destaque = l >= 42 && a >= 63;
    int fonte_l = destaque ? 42 : 34;
    int fonte_a = destaque ? 63 : 49;
    int passo = (fonte_l + 7) / 8;
    erro_t err = destaque
        ? acervo_le_capa_destaque(ap->hal, o->id, bits, sizeof bits)
        : acervo_le_capa_mini(ap->hal, o->id, bits, sizeof bits);
    if (err != OK) return false;
    // A miniatura já está reduzida no SD: cada movimento lê 490 caracteres.
    int dl = l - 2, da = a - 2;
    if (dl * fonte_a > da * fonte_l) dl = da * fonte_l / fonte_a;
    else                             da = dl * fonte_a / fonte_l;
    int ox = x + (l - dl) / 2, oy = y + (a - da) / 2;
    gfx_ret(&ap->tela, ox, oy, dl, da, false);
    for (int dy = 0; dy < da; dy++) {
        int sy = dy * fonte_a / da;
        for (int dx = 0; dx < dl; dx++) {
            int sx = dx * fonte_l / dl;
            bool preto = bits[sy * passo + sx / 8] & (0x80u >> (sx & 7));
            gfx_pixel(&ap->tela, ox + dx, oy + dy, preto);
        }
    }
    return true;
}

static void pinta_capa_do_leitor(app_t *ap)
{
    estado_t *e = &ap->estado;
    if (!e->leitor_na_capa) return;

    // Página zero: a capa ocupa o papel útil, na proporção. Sem arquivo
    // válido, uma capa editorial com o título.
    const int l = TELA_L, a = 360;
    const int x = 0, topo = (TELA_A - a) / 2;
    if (!e->obra_aberta.capa_aqui ||
        !pinta_capa_do_cartao(ap, &e->obra_aberta, x, topo, l, a)) {
        gfx_ret(&ap->tela, x, topo, l, a, false);
        gfx_paragrafo_centro(&ap->tela, x + 10, topo + a / 3,
                             l - 20, 4, F_EDITORIAL,
                             e->obra_aberta.titulo);
    }
}

static void pinta_capas_do_acervo(app_t *ap, const vista_acervo_t *v)
{
    int x, y, l, a;
    if (v->tem_destaque &&
        tela_acervo_capa_area(v, -1, &x, &y, &l, &a)) {
        int i = v->destaque.indice;
        if (i >= 0 && i < ap->estado.n_acervo)
            pinta_capa_do_cartao(ap, &ap->estado.acervo[i], x, y, l, a);
    }
    for (int n = 0; n < v->n; n++) {
        int i = v->linhas[n].indice;
        if (i < 0 || i >= ap->estado.n_acervo) continue;
        if (tela_acervo_capa_area(v, n, &x, &y, &l, &a))
            pinta_capa_do_cartao(ap, &ap->estado.acervo[i], x, y, l, a);
    }
}

// ── o quadro, com ou sem o seletor ──────────────────────────────────
// Desenha no bitmap sem falar com o vidro: o app_desenha pode pedir o MESMO
// quadro duas vezes, sem e com seletor (EINK.md §5.5). `cursor = -1` já
// significa "sem cursor" para as vistas.
static void monta_quadro(app_t *ap, bool sem_seletor, bool sem_overlay)
{
    assenta_inicio(&ap->estado);
    garante_cache(ap);
    garante_pendentes(ap);

    // A fase raiz vem antes de tudo. Uma linha de serial por transição separa
    // "congelou" de "tela sem seleção".
    static inicio_fase_t ultima = INICIO_HOME;
    if (ap->estado.inicio.fase != ultima && ap->hal->registrar) {
        char msg[48];
        snprintf(msg, sizeof msg, "fase=%d cursor=%d",
                 (int)ap->estado.inicio.fase, (int)ap->estado.inicio.cursor);
        ap->hal->registrar("inicio", msg);
        ultima = ap->estado.inicio.fase;
    }

    tela_id topo = ap->estado.pilha[ap->estado.profundidade];
    if (topo == TELA_INICIO && ap->estado.inicio.fase == INICIO_NOME) {
        vista_teclado_t v;
        vista_teclado(&ap->estado, &v);
        tela_teclado(&ap->tela, &v);
        return;
    }

    // Restaurar usa a confirmação do SISTEMA, não o desenho do primeiro uso.
    if (topo == TELA_INICIO &&
        ap->estado.inicio.fase == INICIO_CONFIRMAR_FORMATAR) {
        vista_confirma_t v;
        vista_restaurar(&ap->estado, &v);
        tela_confirma(&ap->tela, &v);
        return;
    }

    if (topo == TELA_INICIO) {
        vista_inicializacao_t v;
        vista_inicializacao(&ap->estado, &v);
        tela_inicializacao(&ap->tela, &v);
        return;
    }

    // ── a DOCK vem antes de tudo ───────────────────────────────────────
    // Deitado e carregando, é olhado, não operado.
    if (ap->estado.docado && ap->estado.pilha[ap->estado.profundidade]
                             == TELA_BLOQUEADA) {
        vista_dock_t v;
        vista_dock(&ap->estado, &v);
        tela_dock(&ap->tela, &v);
        return;
    }

    // O bloqueio vem antes do resto: nenhuma tela é operável dormindo.
    if (ap->estado.pilha[ap->estado.profundidade] == TELA_BLOQUEADA) {
        vista_bloqueio_t v;
        vista_bloqueio(&ap->estado, &v);
        tela_bloqueio(&ap->tela, &v);
        return;
    }

    tela_id atual = ap->estado.pilha[ap->estado.profundidade];

    if (atual == TELA_HOME) {
        vista_lancador_t v;
        vista_lancador(&ap->estado, &v);

        // EINK §5.5: a Home entra sem o foco no completo e o recebe num parcial;
        // senão o cartão preto ficava carimbado no vidro.
        if (sem_seletor) v.foco = -1;

        tela_lancador(&ap->tela, &v);
    } else if (atual == TELA_JOGOS) {
        vista_xadrez_t v;
        vista_jogos(&ap->estado, &v);
        if (sem_seletor) v.cursor = 255;
        tela_jogos(&ap->tela, &v);
    } else if (atual == TELA_XADREZ) {
        vista_xadrez_t v;
        vista_xadrez(&ap->estado, &v);
        if (sem_seletor) v.cursor = 255;
        tela_xadrez(&ap->tela, &v);
    } else if (atual == TELA_VINCULAR) {
        vista_vincular_t v;
        vista_vincular(&ap->estado, &v);
        tela_vincular(&ap->tela, &v);
    } else if (atual == TELA_AGENDA) {
        static VISTA_TRANSITORIA vista_agenda_t v;
        vista_agenda(&ap->estado, &v);
        if (sem_seletor) {
            v.cursor = -1;
            v.cursor_agenda = v.cursor_trabalho = -1;
        }
        tela_agenda(&ap->tela, &v);
    } else if (atual == TELA_TECLADO) {
        vista_teclado_t v;
        vista_teclado(&ap->estado, &v);
        tela_teclado(&ap->tela, &v);
    } else if (atual == TELA_ANOTACOES) {
        // O cartão mudou (anotação apagada ou nova): a página se refaz.
        if (!ap->estado.anotacoes_validas)
            (void)uso_carregar_anotacoes(ap->hal, &ap->estado);
        vista_anotacoes_t v;
        vista_anotacoes(&ap->estado, &v);
        if (sem_seletor) v.cursor = -1;
        tela_anotacoes(&ap->tela, &v);
    } else if (atual == TELA_CONFERIR) {
        vista_conferir_t v;
        vista_conferir(&ap->estado, &v);
        tela_conferir(&ap->tela, &v);
    } else if (atual == TELA_CONTA) {
        vista_cartao_t v;
        vista_conta(&ap->estado, &v);
        // -2 e não -1: aqui o -1 é o ícone de sair, que o quadro sem seletor
        // desenharia aceso.
        v.cursor = sem_seletor ? -2 : ap->estado.cursor;
        tela_cartao(&ap->tela, &v);
    } else if (atual == TELA_SINCRONIZACAO) {
        vista_cartao_t v;
        vista_sincronizacao(&ap->estado, &v);
        // -2: no card o -1 é a ação do canto (EINK §5.5).
        if (sem_seletor) v.cursor = -2;
        tela_cartao(&ap->tela, &v);
    } else if (atual == TELA_DATA_HORA) {
        vista_relogio_t v;
        vista_relogio(&ap->estado, &v);

        // EINK §5.5: os três cursores desta tela (campo, interruptor, fuso) somem
        // juntos; o campo em negativo assentado não sairia.
        if (sem_seletor) {
            v.campo          = -1;
            v.no_interruptor = false;
            v.no_fuso        = false;
        }

        tela_relogio(&ap->tela, &v);
    } else if (atual == TELA_ARMAZENAMENTO) {
        vista_cartao_t vc;
        vista_armazenamento(&ap->estado, &vc);
        // -2: no card o -1 é a ação do canto.
        vc.cursor = sem_seletor ? -2 : ap->estado.cursor;
        tela_cartao(&ap->tela, &vc);
    } else if (atual == TELA_WIFI) {
        // DUAS telas sob o mesmo id: o card ("estou na rede?") e a lista (que
        // muda enquanto varre).
        if (ap->estado.wifi_lista) {
            vista_menu_t v;
            vista_wifi(&ap->estado, cabe_acoes(&ap->tela), &v);
            if (sem_seletor) v.cursor = -1;
            tela_menu(&ap->tela, &v);
        } else {
            vista_cartao_t v;
            vista_conexao(&ap->estado, &v);
            // -2: no card o -1 é a ação do canto.
            v.cursor = sem_seletor ? -2 : ap->estado.cursor;
            tela_cartao(&ap->tela, &v);
        }
    } else if (atual == TELA_FALA) {
        // Tela de leitura: `sem_seletor` não muda nada.
        vista_fala_t v;
        vista_fala(&ap->estado, &v);
        tela_fala(&ap->tela, &v);
    } else if (atual == TELA_SOBRE) {
        vista_cartao_t vc;
        vista_sobre(&ap->estado, &vc);
        vc.cursor = sem_seletor ? -2 : ap->estado.cursor;
        tela_cartao(&ap->tela, &vc);
    } else if (atual == TELA_APARENCIA) {
        vista_cartao_t vc;
        vista_aparencia(&ap->estado, &vc);
        vc.cursor = sem_seletor ? -2 : ap->estado.cursor;
        tela_cartao(&ap->tela, &vc);
    } else if (atual == TELA_SOM) {
        vista_cartao_t vc;
        vista_som(&ap->estado, &vc);
        vc.cursor = sem_seletor ? -2 : ap->estado.cursor;
        tela_cartao(&ap->tela, &vc);
    } else if (atual == TELA_QUANDO) {
        vista_quando_t vq;
        vista_quando(&ap->estado, &vq);
        vq.cartao.cursor = sem_seletor ? -2 : ap->estado.cursor;
        tela_cartao(&ap->tela, &vq.cartao);
    } else if (atual == TELA_ACERVO) {
        vista_acervo_t va;
        vista_acervo(&ap->estado, &va);
        if (sem_seletor) {
            va.cursor = -1;
            va.destaque_focado = false;
        }
        tela_acervo(&ap->tela, &va);
        pinta_capas_do_acervo(ap, &va);
    } else if (atual == TELA_OBRA) {
        vista_obra_t vo;
        vista_obra(&ap->estado, &vo);
        tela_obra(&ap->tela, &vo);
        int x, y, l, a;
        if (tela_obra_capa_area(&vo, &x, &y, &l, &a))
            (void)pinta_capa_do_cartao(ap, &ap->estado.obra_aberta, x, y, l, a);
    } else if (atual == TELA_LEITOR) {
        vista_leitor_t vl;
        vista_leitor(&ap->estado, &vl);
        tela_leitor(&ap->tela, &vl);
        pinta_capa_do_leitor(ap);
    } else if (atual == TELA_ESCOLHER_HORA) {
        vista_mostrador_t vm;
        vista_escolher_hora(&ap->estado, &vm);
        tela_mostrador(&ap->tela, &vm);
    } else if (atual == TELA_ESCOLHER_DIA) {
        vista_cal_t vc;
        vista_escolher_dia(&ap->estado, &vc);
        tela_calendario(&ap->tela, &vc);
    } else if (atual == TELA_HORARIO) {
        vista_quando_t vq;
        vista_horario(&ap->estado, &vq);
        vq.cartao.cursor = sem_seletor ? -2 : ap->estado.cursor;
        tela_cartao(&ap->tela, &vq.cartao);
    } else if (atual == TELA_AJUSTES) {
        vista_menu_t v;
        vista_ajustes(&ap->estado, cabe_acoes(&ap->tela), &v);
        if (sem_seletor) v.cursor = -1;
        tela_ajustes(&ap->tela, &v);
    } else if (atual == TELA_QUANTAS) {
        vista_menu_t v;
        vista_ajustes(&ap->estado, cabe_acoes(&ap->tela), &v);
        if (sem_seletor) v.cursor = -1;
        tela_menu(&ap->tela, &v);
    } else if (atual == TELA_CALENDARIO) {
        garante_marcas(ap);
        vista_cal_t v;
        vista_calendario(&ap->estado, &v);
        // `cursor_dia` é o DIA (1..31): 0 é "nenhum".
        if (sem_seletor) v.cursor_dia = 0;
        tela_calendario(&ap->tela, &v);
    } else if (atual == TELA_DIA) {
        vista_dia_t v;
        vista_dia(&ap->estado, &v);
        if (sem_seletor) v.cursor = -1;
        tela_dia(&ap->tela, &v);
    } else if (atual == TELA_RESULTADO) {
        vista_resultado_t v;
        vista_resultado(&ap->estado, &v);
        tela_resultado(&ap->tela, &v);
    } else if (atual == TELA_NOTA && ap->estado.aberto_valido) {
        vista_nota_t v;
        vista_nota(&ap->estado, &v);
        if (sem_seletor) v.cursor_acao = -1;
        tela_nota(&ap->tela, &v);
    } else {
        vista_vazia_t v;
        vista_vazia(&ap->estado, &v);
        tela_vazia(&ap->tela, &v);
    }

    // A VOZ por cima de tudo: uma forma só, a faixa do rodapé, em qualquer
    // tela. Também quando não houve gravação: "não gravei" é estado da voz.
    if (!sem_overlay &&
        (ap->estado.gravacao.fase != GRAV_PARADA ||
         ap->estado.gravacao.nao_comecou != OK) &&
        ap->estado.overlay != OVERLAY_DESCARTAR) {
        vista_grav_t v;
        vista_gravador(&ap->estado, &v);
        ui_voz(&ap->tela, &v);
    }

    // ── o que acabou de acontecer ───────────────────────────────────────
    // Por cima da tela do item; some em segundos ou no primeiro botão.
    if (!sem_overlay && ap->estado.feito[0]) {
        char detalhe[40] = "";
        const item_t *it = &ap->estado.aberto;
        if (ap->estado.aberto_valido && it->vence.ano) {
            char hh[12] = "";
            if (it->hora[0])
                vista_hora_do_item(&ap->estado, it->hora, hh, sizeof hh);
            snprintf(detalhe, sizeof detalhe, "%s %d %s%s%s",
                     data_semana_curta(it->vence), it->vence.dia,
                     data_mes_curto(it->vence), hh[0] ? " · " : "", hh);
        }
        ui_faixa_feito(&ap->tela, ap->estado.feito, detalhe);
    }

    // A recusa no gesto: a mesma faixa, e `recusa` diz o motivo.
    if (!sem_overlay && (ap->estado.precisa_rede[0] ||
                         ap->estado.recusa != RECUSA_NADA))
        ui_faixa_recusa(&ap->tela, ap->estado.precisa_rede,
                        ap->estado.recusa ? ap->estado.recusa : RECUSA_REDE);

    // O descarte continua pop-over: ele PERGUNTA.
    if (!sem_overlay && ap->estado.overlay == OVERLAY_DESCARTAR) {
        vista_grav_t v;
        vista_gravador(&ap->estado, &v);
        tela_gravador_popover(&ap->tela, &v);
    }

    // Desconectar: tela cheia, porque pergunta.
    if (!sem_overlay && ap->estado.overlay == OVERLAY_DESCONECTAR) {
        vista_confirma_t v;
        vista_desconectar(&ap->estado, &v);
        tela_confirma(&ap->tela, &v);
    }

    if (!sem_overlay && ap->estado.overlay == OVERLAY_APAGAR_ROTINA) {
        vista_confirma_t v;
        vista_apagar_rotina(&ap->estado, &v);
        tela_confirma(&ap->tela, &v);
    }

    if (!sem_overlay && ap->estado.overlay == OVERLAY_DESCARTAR_OBRA) {
        vista_confirma_t v;
        vista_descartar_obra(&ap->estado, &v);
        tela_confirma(&ap->tela, &v);
    }

    if (!sem_overlay && ap->estado.overlay == OVERLAY_ESQUECER) {
        vista_confirma_t v;
        vista_esquecer_rede(&ap->estado, &v);
        tela_confirma(&ap->tela, &v);
    }

    // A gaveta por último: é pop-over.
    if (!sem_overlay && ap->estado.overlay == OVERLAY_MENU) {
        vista_menu_t v;
        vista_menu(&ap->estado, cabe_acoes(&ap->tela), &v);
        v.cursor = ap->estado.travado ? -1 : ap->estado.cursor_overlay;
        tela_menu_popover(&ap->tela, &v);
    }
}

// ── a caixa precisa de chão limpo antes de ser desenhada ────────────
// Um pop-over sobre texto assentado pelo completo saía transparente no
// parcial. Um quadro antes leva a região a BRANCO, e a caixa vem depois
// (EINK §5.6). Esta função dá só a GEOMETRIA: o motor precisa saber se há
// caixa antes de montar o plano (fatos, plano, execução). False sem caixa.
bool app_caixa_area(app_t *ap, int *x, int *y, int *l, int *a)
{
    if (ap->estado.pilha[ap->estado.profundidade] == TELA_XADREZ &&
        ap->estado.xadrez.pagina == XZ_PAG_MENU) {
        vista_xadrez_t v;
        vista_xadrez(&ap->estado, &v);
        tela_xadrez_menu_area(&ap->tela, &v, x, y, l, a);
        return true;
    }
    if (ap->estado.overlay == OVERLAY_MENU) {
        vista_menu_t v;
        vista_menu(&ap->estado, cabe_acoes(&ap->tela), &v);
        tela_menu_popover_area(&ap->tela, &v, x, y, l, a);
        return true;
    }
    // Gravando, a peça é a faixa de voz, de retângulo fixo.
    if (ap->estado.gravacao.fase != GRAV_PARADA &&
        ap->estado.overlay != OVERLAY_DESCARTAR) {
        ui_voz_area(x, y, l, a);
        return true;
    }
    // O "Buscando" da Sincronização anima só a faixa da manchete.
    if (ap->estado.pilha[ap->estado.profundidade] == TELA_SINCRONIZACAO &&
        ap->estado.agendas_buscando) {
        tela_menu_manchete_area(&ap->tela, x, y, l, a);
        return true;
    }
    if (ap->estado.precisa_rede[0]) {
        ret_t r = grid_faixa_de(FAIXA_AVISO_A);
        *x = r.x; *y = r.y; *l = r.l; *a = r.a;
        return true;
    }
    if (ap->estado.overlay == OVERLAY_GRAVANDO ||
        ap->estado.overlay == OVERLAY_DESCARTAR) {
        vista_grav_t v;
        vista_gravador(&ap->estado, &v);
        tela_gravador_popover_area(&ap->tela, &v, x, y, l, a);
        return true;
    }
    return false;
}

// A página de LEITURA, ou -1 quando a tela não pagina. Andar entre botões
// não vira página.
static int pagina_de_leitura(app_t *ap)
{
    int cursor = ap->estado.cursor;
    int paginas = 0;

    switch (ap->estado.pilha[ap->estado.profundidade]) {
    // Detalhe rola em passos curtos com parcial; só o Resultado pagina.
    case TELA_RESULTADO: {
        vista_resultado_t v;
        vista_resultado(&ap->estado, &v);
        paginas = tela_resultado_paradas(&ap->tela, &v);
        break;
    }
    default:
        return -1;
    }

    if (paginas <= 1) return -1;
    if (cursor < 0) cursor = 0;
    if (cursor >= paginas) cursor = paginas - 1;
    return cursor;
}

// O app levanta os fatos, pede o plano ao motor e executa.
void app_desenha(app_t *ap)
{
    uint32_t agora  = assinatura_da_tela(ap);
    bool     trocou = (agora != ap->ultima_assinatura);
    ap->ultima_assinatura = agora;

    // SAIR DO TECLADO repinta inteiro, sempre: a grade densa ficava congelada
    // no vidro. Errar custa 2,5 s uma vez; a grade presa custa parecer quebrado.
    tela_id topo_agora = ap->estado.pilha[ap->estado.profundidade];
    if (ap->tela_anterior == TELA_TECLADO && topo_agora != TELA_TECLADO)
        trocou = true;
    ap->tela_anterior = topo_agora;

    // As superfícies do xadrez trocam quase todo o papel: declaradas como
    // mudança, o motor limpa sem seletor antes.
    uint8_t pagina_xadrez = topo_agora == TELA_XADREZ
                          ? ap->estado.xadrez.pagina : UINT8_MAX;
    uint8_t pagina_xadrez_anterior = ap->ultima_pagina_xadrez;
    uint8_t superficie_xadrez = pagina_xadrez == XZ_PAG_MENU
                              ? XZ_PAG_TABULEIRO : pagina_xadrez;
    uint8_t superficie_xadrez_anterior = pagina_xadrez_anterior == XZ_PAG_MENU
                                       ? XZ_PAG_TABULEIRO
                                       : pagina_xadrez_anterior;
    if (!trocou && superficie_xadrez != superficie_xadrez_anterior)
        trocou = true;
    if (ap->xadrez_girou_tela) trocou = true;
    ap->xadrez_girou_tela = false;
    ap->ultima_pagina_xadrez = pagina_xadrez;

    // VIRAR PÁGINA é tela nova: no parcial o texto reapareceria perto do
    // fantasma.
    int pagina = pagina_de_leitura(ap);
    if (!trocou && pagina >= 0 && pagina != ap->ultima_pagina)
        trocou = true;
    ap->ultima_pagina = (int16_t)pagina;

    // Capa e texto são superfícies densas da mesma tela: trocar entre elas não
    // é foco.
    if (!trocou && topo_agora == TELA_LEITOR &&
        ap->estado.leitor_na_capa != ap->ultima_capa_leitor)
        trocou = true;

    bool abriu_caixa = (ap->estado.overlay != ap->ultimo_overlay &&
                        ap->estado.overlay != OVERLAY_NADA) ||
                       (topo_agora == TELA_XADREZ &&
                        pagina_xadrez == XZ_PAG_MENU &&
                        pagina_xadrez_anterior != XZ_PAG_MENU);
    ap->ultimo_overlay = ap->estado.overlay;

    int x, y, l, a;
    motor_fatos_t fatos = {
        .trocou_de_tela = trocou,
        .tem_cursor  = tela_tem_seletor(ap->estado.pilha[ap->estado.profundidade]),
        .abriu_caixa = abriu_caixa,
        .tem_caixa   = app_caixa_area(ap, &x, &y, &l, &a),
    };
    plano_t plano = motor_plano(&fatos);

    // ANDAR COM O CURSOR É FOCO, em qualquer tela: trocar um destino de card
    // pelo outro mede ~34% contra o corte de 33,3%, e a tela piscaria a cada
    // toque. Gesto de navegação não tem o custo decidido por contagem de bytes.
    // Virar página continua sendo tela nova.
    bool cursor_andou = !trocou &&
                        ap->estado.cursor != ap->ultimo_cursor;
    int32_t pos_leitor = topo_agora == TELA_LEITOR
                       ? leitor_posicao(&ap->estado.leitor) : -1;
    bool pagina_leitor_andou = !trocou && topo_agora == TELA_LEITOR &&
        (pos_leitor != ap->ultima_posicao_leitor ||
         ap->estado.leitor_na_capa != ap->ultima_capa_leitor);
    ap->ultima_posicao_leitor = pos_leitor;
    ap->ultima_capa_leitor = ap->estado.leitor_na_capa;

    bool foco_andou = cursor_andou || pagina_leitor_andou ||
                      (!trocou && topo_agora == TELA_HOME &&
                       ap->estado.lancador != ap->ultimo_lancador);
    ap->ultimo_lancador = ap->estado.lancador;
    ap->ultimo_cursor   = (int16_t)ap->estado.cursor;

    for (int i = 0; i < plano.n; i++) {
        const quadro_t *q = &plano.quadro[i];


        monta_quadro(ap, q->sem_seletor, q->sem_overlay);

        // O buraco branco onde a caixa vai cair, com a geometria já lida no mesmo
        // gesto.
        if (q->limpa_o_chao) gfx_limpa_ret(&ap->tela, x, y, l, a);

        manda_pro_vidro(ap,
              q->trocou_de_tela         ? PINTURA_TELA_NOVA
            : foco_andou                ? PINTURA_FOCO
            :                             PINTURA_MESMA_TELA);
    }
}

void app_passo(app_t *ap)
{
    // Oito eventos por passo: esvaziar a fila inteira adiava o desenho para
    // sempre quando ela enchia tão rápido quanto esvaziava (tela congelada,
    // `precisa_desenhar` permanente). O resto espera o próximo passo.
    evento_t ev;
    for (int n = 0; n < 8 && ap->hal->proximo_evento(&ev); n++)
        app_evento(ap, &ev);

    if (ap->precisa_desenhar)
        app_desenha(ap);

    // Uma fatia da máquina por volta, depois do quadro: o lance aparece antes
    // da resposta.
    passo_maquina_xadrez(ap);

    // Contar o livro não segura a tela: quatro páginas por volta.
    if (ap->estado.pilha[ap->estado.profundidade] == TELA_LEITOR &&
        ap->estado.paginas_total <= 0 && ap->estado.leitor_texto) {
        int total = 0, numero = 1;
        if (leitor_conta_passo(&ap->estado.leitor, 4, &total, &numero)) {
            ap->estado.paginas_total = total;
            ap->estado.pagina_atual = numero;
            ap->precisa_desenhar = true;
        }
    }

    // O quadro responde primeiro; a gravação no cartão vem depois.
    if (ap->leitor_meta_pendente) {
        ap->leitor_meta_pendente = false;
        (void)acervo_grava_meta(ap->hal, &ap->estado.obra_aberta);
    }

    // A formatação bloqueia por segundos: desenhar antes faz "Preparando a
    // memória" existir de verdade.
    bool desvinculando = ap->estado.nuvem_esperando == NUVEM_DESVINCULAR &&
        ap->estado.agora_ms - ap->estado.nuvem_desde_ms < 10u * 1000u;
    if (ap->estado.inicio.formatar_pendente && !desvinculando) {
        ap->estado.inicio.formatar_pendente = false;
        // Cartão e flash apagados, o chip reinicia: Restaurar vale como tirar da
        // tomada.
        if (manda_inicio(ap, INICIO_CMD_FORMATAR, NULL) == OK &&
            ap->hal->reiniciar) {
            inicio_fase_t depois = ap->estado.inicio.fase;
            ap->estado.inicio.fase  = INICIO_PREPARANDO;
            ap->estado.inicio.etapa = 2;      // "Reiniciando o aparelho"
            app_desenha(ap);
            ap->hal->reiniciar();
            // Só no PC se chega aqui: o simulador segue como se tivesse religado.
            ap->estado.inicio.fase  = depois;
            ap->estado.inicio.etapa = 0;
        }
        ap->precisa_desenhar = true;
    }

    // O bloqueio NÃO dorme: é lock, como no Kindle. Deep sleep reiniciava o
    // aparelho a cada desbloqueio, e light sleep virou ciclo de reinícios.
    // Economia de verdade (deep sleep retomando o estado) fica para quando
    // houver bancada para medir consumo.
}
