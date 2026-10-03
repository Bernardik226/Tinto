// firmware/testes/t_ui_chrome.c — a moldura, medida em pixel.
// O HUD não é faixa preta atravessando o topo: o relevo vem de contorno,
// espaço e inversão pontual. Os testes contam TINTA em faixas conhecidas.
#include "teste.h"
#include "ui/chrome.h"
#include "ui/grid.h"
#include "tela/mapa.h"
#include "ui/agenda.h"
#include "dado/cartao.h"

static app_t        ap;
static const hal_t *hal;

#define HOJE ((data_t){2026, 9, 1})

static void liga(void)
{
    hal = pc_liga();
    pc_relogio(HOJE, 11, 17);
    app_liga(&ap, hal);
    ap.estado.bateria = 72;
    app_passo(&ap);
    ap.precisa_desenhar = true;
    app_desenha(&ap);
}

static int tinta_na_linha(int y)
{
    int n = 0;
    for (int x = 0; x < TELA_L; x++)
        if (gfx_le(&ap.tela, x, y)) n++;
    return n;
}

static int tinta_na_faixa(int y0, int altura)
{
    int n = 0;
    for (int y = y0; y < y0 + altura; y++) n += tinta_na_linha(y);
    return n;
}

// ── não existe faixa preta no topo ──────────────────────────────────
// Por LINHA, não por média: a média esconderia linhas inteiras pretas.
void t_chrome_a_barra_nao_e_uma_faixa_preta(void)
{
    COMECA("nenhuma linha da barra é uma faixa preta atravessando a tela");

    liga();
    for (int y = 0; y < BARRA_A; y++)
        ESPERA(tinta_na_linha(y) < TELA_L / 2);
    TERMINA();
}

// ── e a barra não ficou vazia ───────────────────────────────────────
// Sem esta prova, apagar a barra passaria no teste anterior.
void t_chrome_a_barra_continua_dizendo_as_coisas(void)
{
    COMECA("a barra clara continua com data, hora e bateria em tinta preta");

    liga();
    ESPERA(tinta_na_faixa(0, BARRA_A) > 60);

    // A metade direita: hora e bateria.
    int direita = 0;
    for (int y = 0; y < BARRA_A; y++)
        for (int x = TELA_L / 2; x < TELA_L; x++)
            if (gfx_le(&ap.tela, x, y)) direita++;
    ESPERA(direita > 20);

    // A esquerda: data ou contexto.
    int esquerda = 0;
    for (int y = 0; y < BARRA_A; y++)
        for (int x = 0; x < TELA_L / 2; x++)
            if (gfx_le(&ap.tela, x, y)) esquerda++;
    ESPERA(esquerda > 20);
    TERMINA();
}

// ── o rodapé é claro quando nada acontece ───────────────────────────
// O negativo do rodapé é ESTADO ATIVO (gravando), não decoração.
void t_chrome_o_rodape_e_claro_quando_nada_acontece(void)
{
    COMECA("parado, o rodapé é claro — negativo é estado ativo, não enfeite");

    liga();
    for (int y = GRID_RODAPE_Y + 1; y < TELA_A; y++)
        ESPERA(tinta_na_linha(y) < TELA_L / 2);
    TERMINA();
}


// ── o seletor nunca sai junto com a waveform completa (EINK §5.5) ────
// Entrar numa tela com seletor são dois quadros: a página sem ele, no
// completo, e ele num parcial. A lista é NOMEADA: varrer o enum acusaria
// telas sem linha, que saem iguais nos dois quadros por motivo legítimo.
static bool o_seletor_vem_no_parcial(tela_id t)
{
    static uint8_t final[(TELA_L + 7) / 8 * TELA_A];

    ap.estado.pilha[0]     = t;
    ap.estado.profundidade = 0;
    ap.estado.overlay      = OVERLAY_NADA;
    ap.estado.cursor       = 0;
    ap.precisa_desenhar    = true;
    app_desenha(&ap);

    const uint8_t *assentado = pc_tela_assentada();
    if (!assentado) return false;
    memcpy(final, ap.tela.bits, sizeof final);

    return memcmp(assentado, final, sizeof final) != 0;
}

void t_chrome_nenhuma_tela_assenta_o_proprio_seletor(void)
{
    COMECA("o seletor chega num parcial — nunca junto da waveform completa");

    hal = pc_liga();
    pc_relogio(HOJE, 11, 17);

    // Conteúdo: sem linha não há seletor e a prova seria vazia.
    for (int i = 0; i < 3; i++) {
        item_t it;
        memset(&it, 0, sizeof it);
        snprintf(it.id,     sizeof it.id,     "090%d-t", i);
        snprintf(it.titulo, sizeof it.titulo, "tarefa %d", i);
        it.tipo = TIPO_TAREFA;
        it.dia  = HOJE;
        cartao_grava_item(hal, HOJE, &it);
    }

    app_liga(&ap, hal);
    app_passo(&ap);

    ESPERA(o_seletor_vem_no_parcial(TELA_HOME));
    ESPERA(o_seletor_vem_no_parcial(TELA_AGENDA));

    // Armazenamento não zerava o cursor no primeiro quadro; Data e hora
    // inverte o campo sob o cursor, um bloco sólido como o da Home.
    ESPERA(o_seletor_vem_no_parcial(TELA_ARMAZENAMENTO));
    ESPERA(o_seletor_vem_no_parcial(TELA_DATA_HORA));
    TERMINA();
}


// ── os eventos são cards iguais, e o foco inverte o conteúdo ────────
// Inverte o BLOCO DE CONTEÚDO, não a coluna de hora, que fica legível.
void t_chrome_o_evento_em_foco_inverte_so_o_conteudo(void)
{
    COMECA("o evento em foco inverte o corpo, e a coluna de hora não");

    hal = pc_liga();
    pc_relogio(HOJE, 9, 0);

    for (int i = 0; i < 2; i++) {
        item_t it;
        memset(&it, 0, sizeof it);
        snprintf(it.id,     sizeof it.id,     "g:ev%d", i);
        snprintf(it.titulo, sizeof it.titulo, "Compromisso %d", i);
        snprintf(it.hora,   sizeof it.hora,   "1%d:00", 4 + i);
        it.tipo   = TIPO_EVENTO;
        it.origem = ORIGEM_GOOGLE;
        it.dia    = HOJE;
        cartao_grava_item(hal, HOJE, &it);
    }

    app_liga(&ap, hal);
    app_passo(&ap);
    ENTRA_NA_AGENDA(&ap);
    ap.precisa_desenhar = true;
    app_desenha(&ap);

    // Margem 10, hora 36, vão 6, filete 3, vão 6: o corpo em 61.
    const int HORA_X0 = 10, HORA_X1 = 46;
    const int CORPO_X0 = 61, CORPO_X1 = TELA_L - 10;

    bool achou = false;
    for (int y = GRID_MIOLO_Y; y < 300 && !achou; y++) {
        int corpo = 0, hora = 0;
        for (int x = CORPO_X0; x < CORPO_X1; x++) if (gfx_le(&ap.tela, x, y)) corpo++;
        for (int x = HORA_X0;  x < HORA_X1;  x++) if (gfx_le(&ap.tela, x, y)) hora++;

        // Corpo quase todo tinta, coluna de hora quase toda papel.
        if (corpo > (CORPO_X1 - CORPO_X0) * 8 / 10 &&
            hora  < (HORA_X1  - HORA_X0)  * 3 / 10)
            achou = true;
    }
    ESPERA(achou);
    TERMINA();
}


// ── hora e bateria não dançam quando o sync aparece ─────────────────
// O `t_grid` prova entre telas; este, entre estados do mesmo slot (as formas
// de sync têm larguras diferentes).
void t_chrome_o_canto_nao_danca_com_o_sync(void)
{
    COMECA("hora e bateria ficam no mesmo pixel em todo estado de sync");

    static const icone_id ESTADOS[] = {
        ICO_NENHUM, ICO_DESCENDO, ICO_SUBINDO, ICO_SINCRONIZA, ICO_SYNC_ERRO,
    };
    const int n = (int)(sizeof ESTADOS / sizeof ESTADOS[0]);

    static uint8_t memoria[(TELA_L + 7) / 8 * TELA_A];
    static bool referencia[70 * BARRA_A];
    bitmap_t q;

    // Os 70 px da direita: hora e bateria.
    const int x0 = TELA_L - 70;

    for (int k = 0; k < n; k++) {
        bitmap_liga(&q, memoria, TELA_L, TELA_A);
        gfx_limpa(&q, false);

        barra_t b = { .titulo = "Agenda", .hora = "09:14", .bateria = 72,
                      .wifi = 80, .sinc = ESTADOS[k] };
        chrome_barra(&q, &b);

        for (int y = 0; y < BARRA_A; y++)
            for (int x = 0; x < 70; x++) {
                bool p = gfx_le(&q, x0 + x, y);
                if (k == 0) referencia[y * 70 + x] = p;
                else        ESPERA(p == referencia[y * 70 + x]);
            }
    }
    TERMINA();
}


// ── muitas tarefas não estouram a tela ──────────────────────────────
// Nada invade o rodapé, e o que não coube vai para a página seguinte.
void t_chrome_muitas_tarefas_nao_quebram_a_tela(void)
{
    COMECA("vinte tarefas cabem sem invadir o rodapé, e o resto vira + N mais");

    hal = pc_liga();
    pc_relogio(HOJE, 9, 0);

    for (int i = 0; i < 20; i++) {
        item_t it;
        memset(&it, 0, sizeof it);
        snprintf(it.id,     sizeof it.id,     "090%02d-t", i);
        snprintf(it.titulo, sizeof it.titulo,
                 "Tarefa %d com um título bem comprido que quebra em duas linhas", i);
        it.tipo = TIPO_TAREFA;
        it.dia  = HOJE;
        cartao_grava_item(hal, HOJE, &it);
    }

    app_liga(&ap, hal);
    ap.estado.rede = REDE_LIGADA;
    app_passo(&ap);
    ENTRA_NA_AGENDA(&ap);
    ap.precisa_desenhar = true;
    app_desenha(&ap);

    // ── nada invade o rodapé ──
    int y0 = GRID_RODAPE_Y;
    int tinta_acima = 0;
    for (int x = 0; x < TELA_L; x++)
        if (gfx_le(&ap.tela, x, y0 - 1)) tinta_acima++;

    // A linha acima do rodapé pode ter texto, mas não cheia de conteúdo
    // cortado.
    ESPERA(tinta_acima < TELA_L);

    // ── e nenhuma some: vai para a página seguinte ──
    static vista_agenda_t v;
    vista_agenda(&ap.estado, &v);
    ESPERA_IGUAL(v.n_trabalho, 20);
    TERMINA();
}

// ── o contador diz o número E a direção ─────────────────────────────
// ▼ enquanto houver seguinte, ▲ na última.
void t_o_contador_de_pagina_mostra_a_direcao(void)
{
    COMECA("rodapé · a paginação diz o número e para onde ela vai");

    ESPERA_IGUAL(chrome_seta_da_pagina(1, 2), ICO_DESCENDO);
    ESPERA_IGUAL(chrome_seta_da_pagina(2, 3), ICO_DESCENDO);

    // Na última a seta inverte em vez de sumir.
    ESPERA_IGUAL(chrome_seta_da_pagina(3, 3), ICO_SUBINDO);
    ESPERA_IGUAL(chrome_seta_da_pagina(2, 2), ICO_SUBINDO);

    // Tela de uma página: nem direção nem contador.
    ESPERA_IGUAL(chrome_seta_da_pagina(1, 1), ICO_NENHUM);

    TERMINA();
}

// ── toda espera tem os pontinhos ────────────────────────────────────
// No centro do rodapé, onde mora o contador (quem espera não navega).
void t_o_rodape_anima_a_espera(void)
{
    COMECA("rodapé · a espera mostra os pontinhos, e não reticências");

    static uint8_t bits[(TELA_L + 7) / 8 * TELA_A];
    bitmap_t bm;
    bitmap_liga(&bm, bits, TELA_L, TELA_A);

    // Sem espera, nenhum ponto.
    memset(bits, 0, sizeof bits);
    chrome_rodape(&bm, "BACK voltar", "OK abrir", false);
    int limpo = chrome_pixels_do_rodape(&bm);

    // Com espera, tinta no centro, e ela MUDA de um quadro para outro.
    memset(bits, 0, sizeof bits);
    chrome_rodape_espera(2);
    chrome_rodape(&bm, "BACK voltar", "OK abrir", false);
    int com_dois = chrome_pixels_do_rodape(&bm);

    memset(bits, 0, sizeof bits);
    chrome_rodape_espera(3);
    chrome_rodape(&bm, "BACK voltar", "OK abrir", false);
    int com_tres = chrome_pixels_do_rodape(&bm);

    ESPERA(com_dois > limpo);
    ESPERA(com_tres > com_dois);

    // A espera vale um quadro, como o contador.
    memset(bits, 0, sizeof bits);
    chrome_rodape(&bm, "BACK voltar", "OK abrir", false);
    ESPERA_IGUAL(chrome_pixels_do_rodape(&bm), limpo);

    TERMINA();
}
