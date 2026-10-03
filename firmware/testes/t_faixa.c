// firmware/testes/t_faixa.c — a faixa, a peça que cobre a tela.
// Um retângulo ancorado no rodapé, que limpa o chão antes de escrever e
// nunca toca um pixel de fora.
#include "teste.h"
#include "ui/faixa.h"
#include "ui/grid.h"
#include "ui/blocos.h"
#include "ui/voz.h"
#include "vista/menu.h"

static uint8_t   memoria[(TELA_L + 7) / 8 * TELA_A];
static bitmap_t  bm;
static bool      antes[TELA_L * TELA_A];

// Tela toda tinta: pixel preto dentro da área útil é chão não limpo;
// pixel apagado fora é invasão.
static void tela_toda_suja(void)
{
    bitmap_liga(&bm, memoria, TELA_L, TELA_A);
    gfx_limpa(&bm, true);
    for (int y = 0; y < TELA_A; y++)
        for (int x = 0; x < TELA_L; x++)
            antes[y * TELA_L + x] = gfx_le(&bm, x, y);
}

static int tinta_dentro(ret_t r)
{
    int n = 0;
    for (int y = r.y; y < r.y + r.a; y++)
        for (int x = r.x; x < r.x + r.l; x++)
            if (gfx_le(&bm, x, y)) n++;
    return n;
}

// ── a faixa limpa o chão ────────────────────────────────────────────
void t_faixa_limpa_o_chao_antes_de_escrever(void)
{
    COMECA("a faixa limpa o próprio chão: nada de trás atravessa");

    tela_toda_suja();
    faixa_t f = faixa_abre(&bm, 96, false);

    // A área de conteúdo sai limpa; sobra o filete.
    ret_t util = { f.x, (int16_t)f.y, (int16_t)f.util,
                   (int16_t)(f.r.y + f.r.a - f.y) };
    ESPERA_IGUAL(tinta_dentro(util), 0);
    TERMINA();
}

// ── a faixa não toca num pixel de fora ──────────────────────────────
void t_faixa_nao_escreve_fora_do_retangulo(void)
{
    COMECA("nenhum pixel fora do retângulo da faixa é tocado");

    tela_toda_suja();
    faixa_t f = faixa_abre(&bm, 96, false);
    faixa_botoes(&bm, &f, "BACK voltar", "OK ir");
    faixa_fecha(&bm, &f);

    for (int y = 0; y < TELA_A; y++)
        for (int x = 0; x < TELA_L; x++) {
            if (grid_contem(f.r, x, y)) continue;
            ESPERA(gfx_le(&bm, x, y) == antes[y * TELA_L + x]);
        }
    TERMINA();
}

// ── o cartaz nunca é coberto ────────────────────────────────────────
void t_faixa_nunca_cobre_a_barra_nem_o_topo_do_miolo(void)
{
    COMECA("a faixa sobe de baixo e nunca alcança a barra");

    tela_toda_suja();
    faixa_t f = faixa_abre(&bm, TELA_A, false);   // pede o painel inteiro

    ESPERA(f.r.y >= GRID_MIOLO_Y);
    ESPERA_IGUAL(f.r.y + f.r.a, GRID_RODAPE_Y);

    // E a barra continua como estava.
    for (int y = GRID_BARRA_Y; y < GRID_BARRA_Y + GRID_BARRA_A; y++)
        for (int x = 0; x < TELA_L; x++)
            ESPERA(gfx_le(&bm, x, y) == antes[y * TELA_L + x]);
    TERMINA();
}

// ── negativo é o estado ativo, e inverte a faixa inteira ────────────
void t_faixa_negativa_inverte_a_faixa_inteira(void)
{
    COMECA("a faixa negativa inverte tudo, e só ela");

    bitmap_liga(&bm, memoria, TELA_L, TELA_A);
    gfx_limpa(&bm, false);                     // papel limpo

    faixa_t f = faixa_abre(&bm, 96, true);
    faixa_botoes(&bm, &f, "● solte pra pausar", "ouvindo");
    faixa_fecha(&bm, &f);

    // O interior majoritariamente tinta.
    ESPERA(tinta_dentro(f.r) > (f.r.l * f.r.a) / 2);

    // E o miolo acima continua papel.
    ret_t acima = { 0, GRID_MIOLO_Y, TELA_L, (int16_t)(f.r.y - GRID_MIOLO_Y) };
    ESPERA_IGUAL(tinta_dentro(acima), 0);
    TERMINA();
}

// ── os três usos são o mesmo retângulo ──────────────────────────────
void t_faixa_os_tres_usos_dividem_a_mesma_geometria(void)
{
    COMECA("voz, saltos e exceção abrem no mesmo lugar");

    tela_toda_suja();
    faixa_t voz    = faixa_abre(&bm, FAIXA_VOZ_A,    true);
    faixa_t saltos = faixa_abre(&bm, FAIXA_SALTOS_A, false);
    faixa_t erro   = faixa_abre(&bm, FAIXA_AVISO_A,  false);

    // Ancoradas no mesmo lugar.
    ESPERA_IGUAL(voz.r.y + voz.r.a,       GRID_RODAPE_Y);
    ESPERA_IGUAL(saltos.r.y + saltos.r.a, GRID_RODAPE_Y);
    ESPERA_IGUAL(erro.r.y + erro.r.a,     GRID_RODAPE_Y);

    // Mesma largura e margem.
    ESPERA_IGUAL(voz.r.l, saltos.r.l);
    ESPERA_IGUAL(voz.x,   saltos.x);
    ESPERA_IGUAL(voz.x,   erro.x);
    ESPERA_IGUAL(voz.util, erro.util);
    TERMINA();
}

// ── texto comprido trunca, não vaza (RN-B7) ─────────────────────────
void t_faixa_botao_comprido_trunca_dentro_da_faixa(void)
{
    COMECA("RN-B7 · botão comprido trunca dentro da faixa, não vaza");

    tela_toda_suja();
    gfx_zera_faltantes();   // o contador é global: sem isto, herda-se o dos outros

    faixa_t f = faixa_abre(&bm, 96, false);
    faixa_botoes(&bm, &f,
                 "BACK voltar para a tela anterior do aparelho inteiro",
                 "OK confirmar tudo o que a inteligência entendeu agora");
    faixa_fecha(&bm, &f);

    for (int y = 0; y < TELA_A; y++)
        for (int x = 0; x < TELA_L; x++) {
            if (grid_contem(f.r, x, y)) continue;
            ESPERA(gfx_le(&bm, x, y) == antes[y * TELA_L + x]);
        }
    ESPERA_IGUAL(gfx_faltantes(), 0);
    TERMINA();
}

// ── a gaveta não vaza ───────────────────────────────────────────────
// Fora da faixa e do rodapé, a tela de trás fica EXATAMENTE como estava.
void t_faixa_a_gaveta_nao_vaza_por_cima_do_conteudo(void)
{
    COMECA("a gaveta aberta não toca em nada além da faixa e do rodapé");

    static app_t ap;
    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 12}, 9, 14);
    app_liga(&ap, hal);
    app_passo(&ap);
    ENTRA_NA_AGENDA(&ap);   // a gaveta vive dentro das áreas, não na Home

    // A Agenda, antes de abrir nada.
    for (int y = 0; y < TELA_A; y++)
        for (int x = 0; x < TELA_L; x++)
            antes[y * TELA_L + x] = gfx_le(&ap.tela, x, y);

    pc_botao(IN_MENU);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_MENU);

    int gx, gy, gl, ga;
    ESPERA(app_caixa_area(&ap, &gx, &gy, &gl, &ga));
    ret_t caixa = { (int16_t)gx, (int16_t)gy, (int16_t)gl, (int16_t)ga };

    // Ancorada no rodapé.
    ESPERA_IGUAL(caixa.y + caixa.a, GRID_RODAPE_Y);

    // O topo do miolo intacto.
    for (int y = GRID_MIOLO_Y; y < caixa.y; y++)
        for (int x = 0; x < TELA_L; x++)
            ESPERA(gfx_le(&ap.tela, x, y) == antes[y * TELA_L + x]);

    // Nada fora da faixa e do rodapé mudou.
    for (int y = 0; y < GRID_RODAPE_Y; y++)
        for (int x = 0; x < TELA_L; x++) {
            if (grid_contem(caixa, x, y)) continue;
            ESPERA(gfx_le(&ap.tela, x, y) == antes[y * TELA_L + x]);
        }
    TERMINA();
}

// ── na gaveta, texto e valor não se encavalam ───────────────────────
void t_faixa_na_gaveta_o_texto_cede_espaco_ao_valor(void)
{
    COMECA("na gaveta, o texto cede espaço ao valor e nenhum invade o outro");

    static app_t ap;
    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 12}, 9, 14);
    app_liga(&ap, hal);
    app_passo(&ap);
    pc_botao(IN_MENU);
    app_passo(&ap);

    vista_menu_t v;
    vista_menu(&ap.estado, 8, &v);

    for (int i = 0; i < v.n; i++) {
        int texto = bloco_acao_largura_do_texto(&v.linhas[i], GRID_UTIL);
        int valor = v.linhas[i].valor[0]
                  ? gfx_largura(F_MIUDA, v.linhas[i].valor) : 0;
        ESPERA(texto + valor <= GRID_UTIL);
    }
    TERMINA();
}

// ── o conteúdo não invade a linha de botões ─────────────────────────
// Sobra ao menos uma linha em branco entre o conteúdo e os botões.
void t_faixa_o_conteudo_nao_encosta_nos_botoes(void)
{
    COMECA("o conteúdo da faixa não invade a linha de botões");

    bitmap_liga(&bm, memoria, TELA_L, TELA_A);
    gfx_limpa(&bm, false);

    ui_faixa_recusa(&bm, "usar a voz", RECUSA_REDE);

    int x, y, l, a;
    ui_voz_area(&x, &y, &l, &a);            // a faixa de voz é a mais alta
    (void)x; (void)l; (void)a;

    ret_t r = grid_faixa_de(FAIXA_AVISO_A);
    faixa_t f = { .r = r, .x = GRID_MARGEM,
                  .y = (int16_t)(r.y + FAIXA_TOPO + 7),
                  .util = GRID_UTIL, .negativo = false };

    int topo_dos_botoes = f.y + faixa_conteudo_a(&f);

    // A linha acima dos botões está limpa.
    int tinta = 0;
    for (int px = r.x; px < r.x + r.l; px++)
        if (gfx_le(&bm, px, topo_dos_botoes - 1)) tinta++;
    ESPERA_IGUAL(tinta, 0);
    TERMINA();
}
