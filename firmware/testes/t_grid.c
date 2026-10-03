// firmware/testes/t_grid.c — os invariantes de tela, que reprovam o build.
// Sobreposição, telas com padrões diferentes, tinta desbotando e quadro
// errado eram coisas que ninguém MEDIA. Comentário não é teste.
#include "teste.h"
#include "vista/sobre.h"
#include "vista/armazenamento.h"
#include "vista/conexao.h"
#include "vista/fala.h"
#include "vista/armazenamento.h"
#include "vista/wifi.h"
#include "vista/conta.h"
#include "vista/menu.h"
#include "vista/calendario.h"
#include "vista/dia.h"
#include "vista/agenda.h"
#include "vista/inicializacao.h"
#include "ui/inicializacao.h"
#include "ui/nota.h"
#include "dado/cartao.h"
#include "uso/uso.h"
#include "ui/grid.h"
#include "tela/mapa.h"
#include "tela/texto.h"
#include "ui/voz.h"
#include "vista/quando.h"

static app_t       ap;
static const hal_t *hal;

#define HOJE ((data_t){2026, 8, 12})

// As telas que desenham sozinhas, sem contexto montado por outra.
static const tela_id AS_TELAS[] = {
    TELA_HOME,
    TELA_AGENDA, TELA_AJUSTES, TELA_CALENDARIO, TELA_DIA, TELA_SOBRE,
    TELA_CONTA, TELA_ARMAZENAMENTO, TELA_APARENCIA, TELA_DATA_HORA,
    TELA_SINCRONIZACAO, TELA_WIFI, TELA_SOM,
};
#define N_TELAS ((int)(sizeof AS_TELAS / sizeof AS_TELAS[0]))

static void liga(void)
{
    hal = pc_liga();
    pc_relogio(HOJE, 9, 14);
    app_liga(&ap, hal);
    app_passo(&ap);
}

// Põe a tela na raiz e desenha, sem botões: mede-se o DESENHO.
static void desenha(tela_id t)
{
    ap.estado.pilha[0]      = t;
    ap.estado.profundidade  = 0;
    ap.estado.overlay       = OVERLAY_NADA;
    ap.estado.cursor        = 0;
    ap.precisa_desenhar     = true;
    app_desenha(&ap);
}

static int tinta_na_linha(int y)
{
    int n = 0;
    for (int x = 0; x < TELA_L; x++) if (gfx_le(&ap.tela, x, y)) n++;
    return n;
}

// Ícone de ÁREA (44 px) não entra em linha de lista (15 px): saía por
// cima do texto.
void t_grid_icone_de_lista_cabe_na_linha(void)
{
    COMECA("ícone de ÁREA não entra em linha de lista");

    // 20 px dá folga para um ícone um pouco maior sem deixar passar um de 44.
    for (int i = 0; i < ICO_QUANTOS; i++) {
        if (i != ICO_AREA_AGENDA && i != ICO_AREA_ACERVO &&
            i != ICO_AREA_JOGOS  && i != ICO_AREA_AJUSTES)
            continue;
        ESPERA(ICONES[i].l > 20);   // são os grandes, e é por isso que
        ESPERA(ICONES[i].a > 20);   // eles não podem cair numa linha
    }

    liga();
    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,     sizeof it.id,     "%s", "n-dentista");
    snprintf(it.titulo, sizeof it.titulo, "%s", "Dentista");
    it.tipo = TIPO_EVENTO; it.dia = HOJE; it.vence = HOJE;
    ap.estado.aberto = it;
    ap.estado.aberto_valido = true;

    vista_quando_t v;
    vista_quando(&ap.estado, &v);
    for (int i = 0; i < v.cartao.n_dest; i++) {
        icone_id ico = v.cartao.dest[i].ico;
        if (ico == ICO_NENHUM) continue;
        ESPERA(ICONES[ico].l <= 20);
        ESPERA(ICONES[ico].a <= 20);
    }
    TERMINA();
}

void t_grid_a_barra_ocupa_a_mesma_altura_em_toda_tela(void)
{
    COMECA("nenhuma tela pinta faixa preta no topo — a barra é clara");

    liga();
    for (int i = 0; i < N_TELAS; i++) {
        desenha(AS_TELAS[i]);

        // Nenhuma linha do topo é faixa preta atravessando a tela, em toda tela.
        // Uma tela que desenhasse a própria barra é pega pela comparação do canto
        // de estado, abaixo.
        for (int y = GRID_BARRA_Y; y < GRID_BARRA_Y + GRID_BARRA_A; y++)
            ESPERA(tinta_na_linha(y) < TELA_L / 2);

        // Não se testa que a primeira linha do miolo é papel: bloco em negativo
        // colado na barra pode ser legítimo.
    }
    TERMINA();
}

// ── o canto de estado não dança ─────────────────────────────────────
// Hora e bateria nunca se movem: com o mesmo estado, a direita da barra é
// IDÊNTICA em toda tela, pixel a pixel.
void t_grid_o_canto_de_estado_e_identico_entre_telas(void)
{
    COMECA("hora e bateria saem no mesmo lugar em toda tela, pixel a pixel");

    liga();

    // 110 px: alcança o Wi-Fi e os pontinhos, não só hora e bateria.
    static bool referencia[110 * GRID_BARRA_A];
    const int x0 = TELA_L - 110;

    desenha(AS_TELAS[0]);
    for (int y = 0; y < GRID_BARRA_A; y++)
        for (int x = 0; x < 110; x++)
            referencia[y * 110 + x] = gfx_le(&ap.tela, x0 + x, GRID_BARRA_Y + y);

    for (int i = 1; i < N_TELAS; i++) {
        desenha(AS_TELAS[i]);
        for (int y = 0; y < GRID_BARRA_A; y++)
            for (int x = 0; x < 110; x++)
                ESPERA(gfx_le(&ap.tela, x0 + x, GRID_BARRA_Y + y)
                       == referencia[y * 110 + x]);
    }
    TERMINA();
}

// ── a faixa é sempre a mesma geometria ──────────────────────────────
// Voz, saltos e exceção usam UM retângulo ancorado no rodapé.
void t_grid_a_faixa_e_um_retangulo_so(void)
{
    COMECA("a faixa mora sempre no mesmo retângulo, ancorada no rodapé");

    ret_t f = grid_faixa();

    // Encostada no rodapé, nunca por baixo.
    ESPERA_IGUAL(f.y + f.a, GRID_RODAPE_Y);
    ESPERA_IGUAL(f.l, TELA_L);

    // O cartaz continua visível.
    ESPERA(f.y > GRID_MIOLO_Y);

    // Qualquer altura continua ancorada.
    for (int a = 0; a <= GRID_MIOLO_A; a += 12)
        ESPERA_IGUAL(grid_faixa_de(a).y + grid_faixa_de(a).a, GRID_RODAPE_Y);

    // Altura absurda é contida no miolo.
    ret_t gigante = grid_faixa_de(TELA_A * 2);
    ESPERA(gigante.y >= GRID_MIOLO_Y);
    ESPERA_IGUAL(gigante.y + gigante.a, GRID_RODAPE_Y);
    TERMINA();
}

// ── as zonas não se sobrepõem ───────────────────────────────────────
// Barra, miolo e rodapé cobrem a tela uma vez cada.
void t_grid_as_zonas_cobrem_a_tela_sem_encostar(void)
{
    COMECA("barra, miolo e rodapé cobrem os 416 px, com um dono cada");

    ret_t b = grid_barra(), m = grid_miolo(), r = grid_rodape();

    ESPERA_IGUAL(b.y, 0);
    ESPERA_IGUAL(b.y + b.a, m.y);          // encostam sem buraco
    ESPERA_IGUAL(m.y + m.a, r.y);
    ESPERA_IGUAL(r.y + r.a, TELA_A);       // e terminam no fim do painel

    for (int y = 0; y < TELA_A; y++) {
        int donos = grid_contem(b, 0, y) + grid_contem(m, 0, y) + grid_contem(r, 0, y);
        ESPERA_IGUAL(donos, 1);
    }
    TERMINA();
}

// ── a margem é uma só ───────────────────────────────────────────────
void t_grid_a_largura_util_fecha_com_a_margem(void)
{
    COMECA("a largura útil é o painel menos as duas margens, e nada mais");

    ESPERA_IGUAL(GRID_UTIL, TELA_L - 2 * GRID_MARGEM);
    ESPERA(GRID_UTIL > 0);
    ESPERA_IGUAL(grid_miolo().l, TELA_L);
    TERMINA();
}

// ── nenhuma tela desenha glifo que a fonte não tem ──────────────────
// Já pegou o "●" e o "›" saindo como tofu. `gfx_texto` conta em vez de
// abortar; aqui deixa de passar calado.
void t_grid_nenhuma_tela_pede_glifo_que_nao_existe(void)
{
    COMECA("nenhuma tela do sistema desenha glifo que a fonte não tem");

    liga();
    for (int i = 0; i < N_TELAS; i++) {
        gfx_zera_faltantes();
        desenha(AS_TELAS[i]);
        // O número da tela vai junto.
        if (gfx_faltantes()) printf("      tela %d: %d faltantes\n",
                                    (int)AS_TELAS[i], gfx_faltantes());
        ESPERA_IGUAL(gfx_faltantes(), 0);
    }

    // ── e as telas do PRIMEIRO USO ──────────────────────────────────────
    // Todas as fases vezes os quatro estados de rede e conta: a conclusão tem
    // três corpos, e os `›` moravam nos que um estado só não desenhava.
    for (int rede = 0; rede < 2; rede++)
    for (int conta = 0; conta < 2; conta++)
    // Até PREPARANDO, a última do enum (INICIO_HOME vale zero).
    for (int f = 0; f <= INICIO_PREPARANDO; f++) {
        gfx_zera_faltantes();
        ap.estado.inicio.fase = (inicio_fase_t)f;
        ap.estado.rede = rede ? REDE_LIGADA : REDE_DESLIGADA;
        snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s",
                 conta ? "eu@x.com" : "");
        snprintf(ap.estado.inicio.nome_pendente,
                 sizeof ap.estado.inicio.nome_pendente, "%s", "Usuário");

        vista_inicializacao_t v;
        vista_inicializacao(&ap.estado, &v);
        tela_inicializacao(&ap.tela, &v);

        if (gfx_faltantes())
            printf("      fase %d (rede=%d conta=%d): %d faltantes\n",
                   f, rede, conta, gfx_faltantes());
        ESPERA_IGUAL(gfx_faltantes(), 0);
    }
    ap.estado.inicio.fase = INICIO_HOME;
    ap.estado.rede = REDE_DESLIGADA;
    ap.estado.nome[0] = '\0';

    // E as três faixas, que desenham por cima de tudo.
    gfx_zera_faltantes();
    desenha(TELA_AGENDA);
    ui_faixa_recusa(&ap.tela, "usar a voz", RECUSA_REDE);
    ESPERA_IGUAL(gfx_faltantes(), 0);

    ap.estado.rede = REDE_LIGADA;
    pc_botao(IN_VOZ);
    app_passo(&ap);
    gfx_zera_faltantes();
    ap.precisa_desenhar = true;
    app_desenha(&ap);
    ESPERA_IGUAL(gfx_faltantes(), 0);

    pc_botao(IN_MENU);
    app_passo(&ap);
    gfx_zera_faltantes();
    ap.precisa_desenhar = true;
    app_desenha(&ap);
    ESPERA_IGUAL(gfx_faltantes(), 0);
    TERMINA();
}

// ── trocar de tela não deixa resto da anterior ──────────────────────
// Resto do PAINEL é do driver e do motor. Este fecha o resto do DESENHO:
// a mesma tela feita depois da tela mais cheia e sobre bitmap limpo tem de
// sair igual.
void t_grid_trocar_de_tela_nao_deixa_resto(void)
{
    COMECA("nenhuma tela herda pixel da tela anterior");

    static bool limpo[TELA_L * TELA_A];

    liga();
    for (int i = 0; i < N_TELAS; i++) {

        // 1. a tela sozinha, sobre bitmap zerado
        gfx_limpa(&ap.tela, false);
        desenha(AS_TELAS[i]);
        for (int y = 0; y < TELA_A; y++)
            for (int x = 0; x < TELA_L; x++)
                limpo[y * TELA_L + x] = gfx_le(&ap.tela, x, y);

        // 2. a mesma tela, depois de uma tela cheia de tinta
        gfx_limpa(&ap.tela, true);
        desenha(AS_TELAS[i]);

        for (int y = 0; y < TELA_A; y++)
            for (int x = 0; x < TELA_L; x++)
                if (gfx_le(&ap.tela, x, y) != limpo[y * TELA_L + x]) {
                    printf("      tela %d herdou pixel em (%d,%d)\n",
                           (int)AS_TELAS[i], x, y);
                    ESPERA(false);
                }
    }
    TERMINA();
}

// ── a faixa fecha e a tela volta inteira ────────────────────────────
void t_grid_fechar_a_faixa_devolve_a_tela_inteira(void)
{
    COMECA("abrir e fechar a gaveta devolve a tela igualzinha");

    static bool antes_da_gaveta[TELA_L * TELA_A];

    liga();
    app_passo(&ap);
    ENTRA_NA_AGENDA(&ap);   // a gaveta vive dentro das áreas, não na Home

    for (int y = 0; y < TELA_A; y++)
        for (int x = 0; x < TELA_L; x++)
            antes_da_gaveta[y * TELA_L + x] = gfx_le(&ap.tela, x, y);

    pc_botao(IN_MENU);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_MENU);

    pc_botao(IN_MENU);          // o mesmo botão fecha
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.overlay, OVERLAY_NADA);

    for (int y = 0; y < TELA_A; y++)
        for (int x = 0; x < TELA_L; x++)
            ESPERA(gfx_le(&ap.tela, x, y) == antes_da_gaveta[y * TELA_L + x]);
    TERMINA();
}

// Sair do teclado (a tela mais densa) não deixa o teclado na tela.
void t_grid_sair_do_teclado_nao_deixa_o_teclado(void)
{
    COMECA("sair do teclado repinta a tela inteira, sem deixar a grade");

    static bool so_a_lista[TELA_L * TELA_A];

    liga();

    // A tela de destino desenhada limpa: é com ela que se compara.
    gfx_limpa(&ap.tela, false);
    desenha(TELA_WIFI);
    for (int y = 0; y < TELA_A; y++)
        for (int x = 0; x < TELA_L; x++)
            so_a_lista[y * TELA_L + x] = gfx_le(&ap.tela, x, y);

    // Agora passando pelo teclado.
    gfx_limpa(&ap.tela, false);
    desenha(TELA_TECLADO);
    desenha(TELA_WIFI);

    for (int y = 0; y < TELA_A; y++)
        for (int x = 0; x < TELA_L; x++)
            if (gfx_le(&ap.tela, x, y) != so_a_lista[y * TELA_L + x]) {
                printf("      sobrou tecla em (%d,%d)\n", x, y);
                ESPERA(false);
            }
    TERMINA();
}

// ── nenhum rótulo é cortado no meio de um caractere ─────────────────
// `snprintf` num buffer pequeno corta no BYTE e deixa continuação órfã:
// caixas vazadas que passam nos testes de vista ("◀ ontem  amanhã ▶" tem
// 22 bytes e morava em `char[22]`).
static bool utf8_inteiro(const char *s)
{
    // Nenhum byte de continuação é o primeiro, e a string não termina no meio
    // de uma sequência.
    int faltam = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (faltam) {
            if ((*p & 0xC0) != 0x80) return false;
            faltam--;
            continue;
        }
        if      ((*p & 0x80) == 0x00) faltam = 0;
        else if ((*p & 0xE0) == 0xC0) faltam = 1;
        else if ((*p & 0xF0) == 0xE0) faltam = 2;
        else if ((*p & 0xF8) == 0xF0) faltam = 3;
        else return false;                  // byte de continuação solto
    }
    return faltam == 0;
}

void t_grid_nenhum_rotulo_e_cortado_no_meio_de_um_caractere(void)
{
    COMECA("nenhum rótulo do sistema é cortado no meio de um caractere");

    liga();

    // A Agenda em repouso: as setas multibyte.
    ap.estado.cursor = -1;
    vista_agenda_t h;
    vista_agenda(&ap.estado, &h);
    ESPERA(utf8_inteiro(h.rodape_esq));
    ESPERA(utf8_inteiro(h.rodape_dir));
    ESPERA_CONTEM(h.rodape_esq, "BACK");

    // As setas moram no cabeçalho do dia.
    ESPERA(utf8_inteiro(h.nav_dia));
    ESPERA(utf8_inteiro(h.dia_semana));
    ESPERA(utf8_inteiro(h.dia_longo));
    ESPERA_CONTEM(h.nav_dia, "▶");

    // E com cursor, que troca os dois rótulos.
    ap.estado.cursor = 0;
    vista_agenda(&ap.estado, &h);
    ESPERA(utf8_inteiro(h.rodape_esq));
    ESPERA(utf8_inteiro(h.rodape_dir));

    // O calendário: o "·" da contagem e as setas.
    vista_cal_t c;
    vista_calendario(&ap.estado, &c);
    ESPERA(utf8_inteiro(c.rodape_esq));
    ESPERA(utf8_inteiro(c.rodape_dir));
    ESPERA(utf8_inteiro(c.contagem));
    ESPERA(utf8_inteiro(c.nav_mes));
    ESPERA(utf8_inteiro(c.mes));

    // E toda tela de CARD.
    void (*cards[])(const estado_t *, vista_cartao_t *) = {
        vista_conta, vista_conexao, vista_sincronizacao,
        vista_aparencia, vista_som, vista_armazenamento,
        vista_sobre,
    };
    for (size_t m = 0; m < sizeof cards / sizeof cards[0]; m++) {
        vista_cartao_t v;
        cards[m](&ap.estado, &v);

        ESPERA(utf8_inteiro(v.titulo));
        ESPERA(utf8_inteiro(v.kicker));
        ESPERA(utf8_inteiro(v.nome));
        ESPERA(utf8_inteiro(v.corpo));
        ESPERA(utf8_inteiro(v.secao));
        ESPERA(utf8_inteiro(v.secao_info));
        ESPERA(utf8_inteiro(v.rodape_esq));
        ESPERA(utf8_inteiro(v.rodape_dir));
        for (int i = 0; i < v.n_fatos; i++) {
            ESPERA(utf8_inteiro(v.fatos[i].rotulo));
            ESPERA(utf8_inteiro(v.fatos[i].valor));
        }
        for (int i = 0; i < v.n_info; i++) {
            ESPERA(utf8_inteiro(v.info[i].rotulo));
            ESPERA(utf8_inteiro(v.info[i].sub));
            ESPERA(utf8_inteiro(v.info[i].valor));
        }
        for (int i = 0; i < v.n_dest; i++) {
            ESPERA(utf8_inteiro(v.dest[i].titulo));
            ESPERA(utf8_inteiro(v.dest[i].sub));
            ESPERA(utf8_inteiro(v.dest[i].valor));
        }
    }

    void (*menus[])(const estado_t *, int, vista_menu_t *) = {
        vista_menu, vista_ajustes, vista_wifi,
    };
    for (size_t m = 0; m < sizeof menus / sizeof menus[0]; m++) {
        vista_menu_t v;
        menus[m](&ap.estado, 12, &v);

        ESPERA(utf8_inteiro(v.titulo));
        ESPERA(utf8_inteiro(v.nota));
        ESPERA(utf8_inteiro(v.vazio));
        ESPERA(utf8_inteiro(v.rodape_esq));
        ESPERA(utf8_inteiro(v.rodape_dir));

        for (int i = 0; i < v.n; i++) {
            ESPERA(utf8_inteiro(v.linhas[i].texto));
            ESPERA(utf8_inteiro(v.linhas[i].valor));
        }
    }

    TERMINA();
}



// O rótulo e o valor não encostam: a coluna sai do rótulo mais largo da
// tela ("QUANDOhoje").
void t_coluna_do_campo_nao_encosta_no_valor(void)
{
    COMECA("o rótulo do campo não encosta no valor");

    const char *rotulos[] = { "QUANDO", "ONDE", "ESTADO", "LISTA",
                              "VENCE", "CRIADA" };

    for (size_t i = 0; i < sizeof rotulos / sizeof rotulos[0]; i++) {
        vista_nota_t v;
        memset(&v, 0, sizeof v);
        snprintf(v.campos[0].rotulo, sizeof v.campos[0].rotulo, "%s", rotulos[i]);
        v.n_campos = 1;

        int col = tela_nota_coluna(&v);
        int vao = col - gfx_largura(F_MIUDA, rotulos[i]);
        ESPERA(vao >= 10);
    }

    // O valor mais comprido ainda cabe.
    vista_nota_t v;
    memset(&v, 0, sizeof v);
    snprintf(v.campos[0].rotulo, sizeof v.campos[0].rotulo, "%s", "QUANDO");
    v.n_campos = 1;
    int sobra = 240 - 11 * 2 - tela_nota_coluna(&v);
    ESPERA(sobra >= gfx_largura(F_CORPO, "hoje · 20:00-21:00"));

    TERMINA();
}

// O valor do campo não perde dado para a ênfase ("14:00–15:…").
void t_o_valor_do_campo_nao_perde_dado_para_a_enfase(void)
{
    COMECA("o campo prefere perder a ênfase a perder o fim do horário");

    bitmap_t bm;
    static uint8_t px[240 * 416 / 8];
    bitmap_liga(&bm, px, 240, 416);

    vista_nota_t v;
    memset(&v, 0, sizeof v);
    snprintf(v.titulo, sizeof v.titulo, "%s", "Evento");
    snprintf(v.tl, sizeof v.tl, "%s", "Dentista");
    snprintf(v.campos[0].rotulo, sizeof v.campos[0].rotulo, "%s", "QUANDO");
    snprintf(v.campos[0].valor,  sizeof v.campos[0].valor,  "%s",
             "hoje · 14:00–15:00");
    v.campos[0].forte = true;
    v.n_campos = 1;

    int col   = tela_nota_coluna(&v);
    int sobra = 240 - 11 * 2 - col;

    // Uma das duas fontes cabe inteira.
    ESPERA(gfx_largura(F_CORPO,   v.campos[0].valor) <= sobra ||
           gfx_largura(F_CORPO_P, v.campos[0].valor) <= sobra);

    tela_nota(&bm, &v);   // e não estoura nem trava desenhando
    TERMINA();
}

// A marca de tarefa sai do PRAZO, não da pasta: o dia da fala abria vazio.
void t_marca_de_tarefa_sai_do_prazo(void)
{
    COMECA("a tarefa marca o dia do prazo, e sem prazo não marca nada");

    const hal_t *hal = pc_liga();
    data_t falada = { 2026, 9, 3 };
    data_t prazo  = { 2026, 9, 10 };

    // Falada dia 3, para vencer dia 10.
    item_t com;
    memset(&com, 0, sizeof com);
    snprintf(com.id,     sizeof com.id,     "%s", "0900-exames");
    snprintf(com.titulo, sizeof com.titulo, "%s", "Levar os exames");
    com.tipo  = TIPO_TAREFA;
    com.dia   = falada;
    com.vence = prazo;
    ESPERA_IGUAL(cartao_grava_item(hal, falada, &com), OK);

    // E uma sem prazo, falada no mesmo dia.
    item_t sem = com;
    snprintf(sem.id,     sizeof sem.id,     "%s", "0901-pasta");
    snprintf(sem.titulo, sizeof sem.titulo, "%s", "comprar pasta");
    memset(&sem.vence, 0, sizeof sem.vence);
    ESPERA_IGUAL(cartao_grava_item(hal, falada, &sem), OK);

    estado_t e;
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = e.dia_visto = falada;
    ESPERA_IGUAL(uso_carregar_marcas(hal, &e), OK);

    // O dia 10 leva a marca; o dia 3, não.
    ESPERA(e.marcas_tarefa & (1u << (10 - 1)));
    ESPERA(!(e.marcas_tarefa & (1u << (3 - 1))));
    TERMINA();
}

// A tira do calendário não sobrepõe rótulo e contagem.
void t_a_tira_do_calendario_nao_sobrepoe(void)
{
    COMECA("o rótulo do dia e a contagem não se encontram no meio");

    // O pior caso real.
    const char *ROT[] = { "qua 12 · hoje", "sáb 28 · amanhã", "dom 30" };
    const char *CON[] = { "2 eventos · 3 tarefas", "1 evento", "3 tarefas" };
    const int util = 240 - 11 * 2;

    for (size_t r = 0; r < sizeof ROT / sizeof ROT[0]; r++)
        for (size_t c = 0; c < sizeof CON / sizeof CON[0]; c++) {
            int wr = gfx_largura(F_MIUDA, ROT[r]);
            int wc = gfx_largura(F_MIUDA, CON[c]);

            // Ou cabem lado a lado, ou a contagem desce de linha.
            bool lado_a_lado = wr + 8 + wc <= util;
            ESPERA(lado_a_lado || wc <= util);
        }
    TERMINA();
}

// A contagem da tira e a lista do dia contam a mesma coisa.
void t_a_contagem_da_tira_conta_o_que_o_dia_mostra(void)
{
    COMECA("a tira conta as tarefas que o dia realmente mostra");

    estado_t e;
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = e.dia_visto = (data_t){ 2026, 9, 10 };
    e.hora = 9; e.minuto = 14;

    // Uma vence hoje, duas não vencem nunca.
    const bool COM_PRAZO[] = { true, false, false };
    for (int i = 0; i < 3; i++) {
        item_t *it = &e.itens[e.n_itens++];
        memset(it, 0, sizeof *it);
        snprintf(it->id,     sizeof it->id,     "t%d", i);
        snprintf(it->titulo, sizeof it->titulo, "tarefa %d", i);
        it->tipo = TIPO_TAREFA;
        it->dia  = e.hoje;
        if (COM_PRAZO[i]) it->vence = e.hoje;
        e.pendentes[e.n_pendentes++] = *it;   // como no aparelho
    }
    e.itens_validos = true;
    e.pendentes_validas = true;

    vista_cal_t c;
    vista_calendario(&e, &c);

    vista_dia_t d;
    vista_dia(&e, &d);

    ESPERA_IGUAL(d.n_tarefas, 1);
    ESPERA_CONTEM(c.tira_mais, "1 por fazer");
    TERMINA();
}

// O calendário mostra a concluída no dia da conclusão.
void t_o_calendario_mostra_a_concluida_no_dia_da_conclusao(void)
{
    COMECA("a tarefa concluída marca e aparece no dia em que foi fechada");

    const hal_t *hal = pc_liga();
    data_t falada = { 2026, 9, 3 };
    data_t fechou = { 2026, 9, 20 };

    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,     sizeof it.id,     "%s", "0900-comp");
    snprintf(it.titulo, sizeof it.titulo, "%s", "Comprar componentes");
    it.tipo     = TIPO_TAREFA;
    it.dia      = falada;
    it.feita    = true;
    it.feita_em = fechou;
    ESPERA_IGUAL(cartao_grava_item(hal, falada, &it), OK);

    estado_t e;
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = falada;
    e.dia_visto = fechou;
    ESPERA_IGUAL(uso_carregar_marcas(hal, &e), OK);

    // O dia 20 leva a marca; o 3, não.
    ESPERA(e.marcas_tarefa & (1u << (20 - 1)));
    ESPERA(!(e.marcas_tarefa & (1u << (3 - 1))));

    // O dia 20 a mostra riscada, vinda das PENDENTES.
    e.pendentes[e.n_pendentes++] = it;
    e.pendentes_validas = true;
    e.itens_validos = true;

    vista_dia_t d;
    vista_dia(&e, &d);
    ESPERA_IGUAL(d.n_tarefas, 1);
    ESPERA(d.tarefas[0].feita);
    TERMINA();
}

// A linha da tira diz o que as tarefas SÃO (fechadas não são "por
// fazer").
void t_a_linha_da_tira_nao_chama_feita_de_por_fazer(void)
{
    COMECA("a tira não chama tarefa concluída de \"por fazer\"");

    estado_t e;
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = e.dia_visto = (data_t){ 2026, 9, 20 };

    // Duas fechadas hoje, nenhuma pendente.
    for (int i = 0; i < 2; i++) {
        item_t *it = &e.itens[e.n_itens++];
        memset(it, 0, sizeof *it);
        snprintf(it->id, sizeof it->id, "t%d", i);
        it->tipo     = TIPO_TAREFA;
        it->dia      = e.hoje;
        it->feita    = true;
        it->feita_em = e.hoje;
        e.pendentes[e.n_pendentes++] = *it;
    }
    e.itens_validos = true;
    e.pendentes_validas = true;

    vista_cal_t v;
    vista_calendario(&e, &v);
    ESPERA_CONTEM(v.tira_mais, "concluída");
    ESPERA(strstr(v.tira_mais, "por fazer") == NULL);

    // Com uma pendente no meio, nem uma palavra nem outra.
    item_t *p = &e.itens[e.n_itens++];
    memset(p, 0, sizeof *p);
    snprintf(p->id, sizeof p->id, "%s", "t9");
    p->tipo  = TIPO_TAREFA;
    p->dia   = e.hoje;
    p->vence = e.hoje;
    e.pendentes[e.n_pendentes++] = *p;

    vista_calendario(&e, &v);
    ESPERA_CONTEM(v.tira_mais, "3 tarefas");
    TERMINA();
}

// O detalhe ROLA quando o conteúdo não cabe: paradas de leitura, depois os
// botões.
void t_o_detalhe_rola_quando_nao_cabe(void)
{
    bitmap_t bm;
    static uint8_t px[240 * 416 / 8];
    bitmap_liga(&bm, px, 240, 416);

    COMECA("o detalhe ganha paradas de rolagem quando o texto não cabe");

    vista_nota_t v;
    memset(&v, 0, sizeof v);
    snprintf(v.titulo, sizeof v.titulo, "%s", "Tarefa");
    snprintf(v.tl,     sizeof v.tl,     "%s", "Levar os exames");
    snprintf(v.kicker, sizeof v.kicker, "%s", "TAREFA · Minhas tarefas");
    snprintf(v.origem, sizeof v.origem, "%s", "Criado por voz no Tinto");

    const char *R[5][2] = {
        { "ESTADO",    "Concluída"      },
        { "LISTA",     "Minhas tarefas" },
        { "VENCE",     "sex 14 ago"     },
        { "CONCLUÍDA", "qua 12 ago"     },
        { "CRIADA",    "12 ago"         },
    };
    for (int i = 0; i < 5; i++) {
        snprintf(v.campos[i].rotulo, sizeof v.campos[0].rotulo, "%s", R[i][0]);
        snprintf(v.campos[i].valor,  sizeof v.campos[0].valor,  "%s", R[i][1]);
    }
    v.n_campos = 5;

    snprintf(v.botoes[0].texto, sizeof v.botoes[0].texto, "%s", "Desmarcar");
    snprintf(v.botoes[1].texto, sizeof v.botoes[1].texto, "%s",
             "Ver fala original");
    v.n_botoes = 2;

    // Sem descrição: cabe.
    ESPERA_IGUAL(tela_nota_paradas(&bm, &v), v.n_botoes);

    // Com descrição longa: rola.
    v.tem_resumo = true;
    snprintf(v.resumo, sizeof v.resumo, "%s",
             "Levar os exames de sangue e o encaminhamento do convênio para "
             "a consulta de retorno, e perguntar sobre o resultado da "
             "ressonância que ficou pendente da vez passada.");

    int paradas = tela_nota_paradas(&bm, &v);
    ESPERA(paradas > v.n_botoes);

    // Na última parada de leitura o texto termina DENTRO da tela.
    ESPERA(tela_nota_fim_visivel(&bm, &v, paradas - v.n_botoes - 1));
    TERMINA();
}

// A raiz de Ajustes são CINCO destinos, despachados por destino, não por
// rótulo.
void t_a_raiz_de_ajustes_sao_cinco_destinos(void)
{
    estado_t e;
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    COMECA("a raiz de Ajustes são cinco categorias, e todas abrem");

    e.hoje = (data_t){ 2026, 9, 2 };
    e.hora = 9; e.minuto = 14;

    vista_menu_t v;
    vista_ajustes(&e, 12, &v);

    ESPERA_IGUAL(v.n, 5);
    ESPERA_CONTEM(v.linhas[0].texto, "conta");
    ESPERA_CONTEM(v.linhas[1].texto, "Conexão");
    ESPERA_CONTEM(v.linhas[2].texto, "Hora e tela");
    ESPERA_CONTEM(v.linhas[3].texto, "Câmera");
// A quinta é o hub do aparelho.
    ESPERA_CONTEM(v.linhas[4].texto, "Sobre o Tinto");

    // Toda linha é destino; nenhuma mostra valor.
    for (int i = 0; i < v.n; i++) {
        ESPERA(!v.linhas[i].so_leitura);
        ESPERA(v.destino[i] != TELA_QUANTAS);
        ESPERA(v.linhas[i].valor[0] == '\0');
    }
    TERMINA();
}

// O BACK diz PARA ONDE volta.
void t_o_back_diz_para_onde_volta(void)
{
    COMECA("HUD · o BACK nomeia o destino, e não só a direção");

    static estado_t e;
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = (data_t){2026, 9, 3};

    // A raiz de Ajustes volta para a Home.
    vista_menu_t m;
    vista_ajustes(&e, 12, &m);
    ESPERA_CONTEM(m.rodape_esq, "início");

    // Uso de voz volta para Minha conta.
    e.quota.limite_s = 3600;
    vista_fala_t f;
    vista_fala(&e, &f);
    ESPERA_CONTEM(f.rodape_esq, "conta");

    e.espaco_total_kb = 1024u * 1024u;
    vista_cartao_t c;
    vista_armazenamento(&e, &c);
// Armazenamento volta para o Sobre.
    ESPERA_CONTEM(c.rodape_esq, "sobre");

    TERMINA();
}
