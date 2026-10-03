// firmware/testes/t_fala.c — T-26, quanto ainda dá para falar.
// Em minutos, nunca em dinheiro. RN-52: o aparelho só exibe.
#include "teste.h"
#include "vista/conta.h"
#include "vista/fala.h"
#include "ui/grid.h"
#include "tela/texto.h"
#include "ui/fala.h"

static estado_t e;

static void mes_com(int usados_min, int limite_min, int dia, int dias_pra_virar)
{
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = e.dia_visto = (data_t){2026, 8, (int8_t)dia};
    e.hora = 9; e.minuto = 14;
    e.hora_confiavel = true;
    e.bateria = 78;
    e.quota.usados_s = usados_min * 60;
    e.quota.limite_s = limite_min * 60;
    e.quota.dias_pra_virar = (int16_t)dias_pra_virar;
    snprintf(e.nome, sizeof e.nome, "%s", "Convidado");
}

// O número grande é o USADO, em minutos ("12:00" parecia relógio).
void t_a_fala_do_mes_mostra_o_numero_e_o_que_sobra(void)
{
    COMECA("T-26 · o usado em minutos, de quanto, o que resta e quando renova");

    mes_com(12, 120, 12, 19);
    vista_fala_t v;
    vista_fala(&e, &v);

    ESPERA_TEXTO(v.titulo, "Uso de voz");
    ESPERA_TEXTO(v.mes, "agosto");
    ESPERA_TEXTO(v.usados, "12");
    ESPERA_TEXTO(v.unidade, "min");
    ESPERA_TEXTO(v.de, "usados de 120 min");
    ESPERA_TEXTO(v.restam, "restam 108 min");
    ESPERA_TEXTO(v.renova, "renova em 19 dias");
    ESPERA_IGUAL(v.pct, 90);          // a barra é o que RESTA
    ESPERA_CONTEM(v.explica, "Só conta o que é enviado");

    // O que resta arredonda para baixo.
    e.quota.usados_s = 12 * 60 + 30;
    vista_fala(&e, &v);
    ESPERA_TEXTO(v.usados, "12");
    ESPERA_TEXTO(v.restam, "restam 107 min");

    // Menos de um minuto vai em segundos.
    e.quota.usados_s = 45;
    vista_fala(&e, &v);
    ESPERA_TEXTO(v.usados, "45");
    ESPERA_TEXTO(v.unidade, "s");
    TERMINA();
}

void t_esgotado_mostra_zero(void)
{
    COMECA("T-26 · esgotado, tudo usado e nada restando");

    mes_com(125, 120, 20, 1);
    vista_fala_t v;
    vista_fala(&e, &v);

    ESPERA_TEXTO(v.usados, "120");
    ESPERA_TEXTO(v.restam, "restam 0 min");
    ESPERA_IGUAL(v.pct, 0);
    ESPERA_TEXTO(v.renova, "renova em 1 dia");
    TERMINA();
}

// O número grande só leva glifos da fonte grande, e cabe até o teto.
void t_o_numero_grande_cabe_e_so_tem_digitos(void)
{
    COMECA("T-26 · o número grande cabe no card e só tem glifo da fonte");

    static uint8_t mem[TELA_L / 8 * TELA_A];
    bitmap_t bm;
    bitmap_liga(&bm, mem, TELA_L, TELA_A);
    const int util = TELA_L - GRID_MARGEM_DESTINO * 2 - 16;

    int casos[] = { 0, 1, 9, 59, 99, 118, 120, 999 };
    for (int i = 0; i < 8; i++) {
        mes_com(casos[i], casos[i] > 120 ? 999 : 120, 12, 19);
        e.quota.usados_s += 59;     // o pior caso de largura de cada faixa
        vista_fala_t v;
        vista_fala(&e, &v);
        for (const char *c = v.usados; *c; c++)
            ESPERA(*c >= '0' && *c <= '9');
        ESPERA(gfx_largura(F_ENORME, v.usados) + 8 +
               gfx_largura(F_CORPO_P, v.unidade) <= util);
        ESPERA(gfx_largura(F_CORPO_P, v.de) <= util);
        ESPERA(gfx_largura(F_MIUDA, v.restam) <= util);
        ESPERA(gfx_largura(F_MIUDA, v.renova) <= util);

        // Desenhada, nenhum glifo faltando.
        gfx_zera_faltantes();
        tela_fala(&bm, &v);
        ESPERA_IGUAL(gfx_faltantes(), 0);
    }

    mes_com(0, 0, 12, 0);
    vista_fala_t v;
    vista_fala(&e, &v);
    ESPERA(gfx_largura(F_ENORME, v.usados) > 0);   // o "–" existe na fonte
    TERMINA();
}

// Sem conta não há número.
void t_sem_conta_a_fala_do_mes_nao_inventa_numero(void)
{
    COMECA("T-26 · sem conta pareada, a tela não inventa número");

    mes_com(0, 0, 12, 0);
    e.nome[0] = '\0';
    vista_fala_t v;
    vista_fala(&e, &v);

    ESPERA_TEXTO(v.usados, "–");
    ESPERA_TEXTO(v.de, "sem conta pareada");
    ESPERA_IGUAL(v.pct, 0);
    ESPERA_TEXTO(v.restam, "");
    ESPERA_TEXTO(v.renova, "");
    TERMINA();
}

// A tela tem porta: Minha conta › Uso de voz.
void t_ajustes_abre_a_fala_do_mes(void)
{
    COMECA("T-26 · a voz se abre por Minha Conta, e só por lá");

    static app_t ap;
    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 12}, 9, 14);
    app_liga(&ap, hal);
    app_passo(&ap);

    // Com conta, a linha existe.
    ap.estado.rede = REDE_LIGADA;
    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", "eu@x.com");
    ap.estado.quota.limite_s = 1800;

    ENTRA_NOS_AJUSTES(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_AJUSTES);

    // Uma porta só (não há "Fala do mês" em Ajustes).
    vista_menu_t aj;
    vista_ajustes(&ap.estado, 12, &aj);
    for (int i = 0; i < aj.n; i++)
        ESPERA(strcmp(aj.linhas[i].texto, "Fala do mês") != 0);

    DESCE_ATE(&ap, vista_ajustes, "Minha conta");
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_CONTA);

    // "Uso de voz" é o segundo destino de Minha conta.
    ap.estado.cursor = 1;
    pc_botao(IN_OK);
    app_passo(&ap);

    ESPERA_IGUAL(ap.estado.pilha[ap.estado.profundidade], TELA_FALA);

    TERMINA();
}
