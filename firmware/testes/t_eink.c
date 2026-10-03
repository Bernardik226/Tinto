#include "teste.h"
#include "vista/campos.h"
#include "hal/uc8253.h"
#include "hal/refresco.h"
#include "ui/rolagem.h"
#include "ui/pagina.h"
#include "ui/nota.h"
#include "vista/nota.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    char texto[16384];
    size_t usados;

    // Bytes que atravessaram depois do último 0x13: o plano novo recebeu a
    // tela inteira ou só a faixa?
    uint8_t ultimo_comando;
    size_t  dados_do_ultimo_plano;
} trilha_t;

static void anexa(trilha_t *t, const char *trecho)
{
    // snprintf devolve o que TERIA escrito: somar sem conferir estoura o
    // buffer.
    if (t->usados >= sizeof t->texto) return;
    size_t espaco = sizeof t->texto - t->usados;
    int n = snprintf(t->texto + t->usados, espaco, "%s", trecho);
    if (n <= 0) return;
    t->usados += ((size_t)n < espaco) ? (size_t)n : espaco - 1;
}

static bool guarda_comando(void *contexto, uint8_t valor)
{
    trilha_t *t = contexto;
    t->ultimo_comando = valor;
    if (valor == 0x13) t->dados_do_ultimo_plano = 0;
    char trecho[8];
    snprintf(trecho, sizeof trecho, "C%02X;", valor);
    anexa(t, trecho);
    return true;
}

static bool guarda_dados(void *contexto, const uint8_t *valores, size_t quantos)
{
    trilha_t *t = contexto;
    if (t->ultimo_comando == 0x13) t->dados_do_ultimo_plano += quantos;
    char trecho[16];
    snprintf(trecho, sizeof trecho, "B%zu:", quantos);
    anexa(t, trecho);
    for (size_t i = 0; i < quantos; i++) {
        snprintf(trecho, sizeof trecho, "%02X", valores[i]);
        anexa(t, trecho);
    }
    anexa(t, ";");
    return true;
}

static bool guarda_espera(void *contexto)
{
    anexa(contexto, "W;");
    return true;
}

static uc8253_io_t io_de(trilha_t *t)
{
    uc8253_io_t io = {
        .contexto = t,
        .comando = guarda_comando,
        .dados = guarda_dados,
        .espera = guarda_espera,
    };
    return io;
}

void t_uc8253_inicia_com_a_configuracao_do_painel(void)
{
    COMECA("UC8253 inicia com a configuração específica do painel");
    trilha_t t = {0};
    uc8253_io_t io = io_de(&t);

    // Soft reset, BUSY no meio, PSR e a RESOLUÇÃO (0x61). Sem ela cada linha
    // cai deslocada: o rasgo em blocos.
    ESPERA(uc8253_inicia(&io));
    ESPERA_TEXTO(t.texto, "C00;B2:1E0D;W;C00;B2:1F0D;C61;B3:F001A0;");
    TERMINA();
}

void t_uc8253_gira_o_framebuffer_para_o_gabinete(void)
{
    COMECA("UC8253 gira pelo PSR, não reescrevendo o framebuffer");
    trilha_t t = {0};
    uc8253_io_t io = io_de(&t);
    const uint8_t bits[] = { 0x80, 0x40 };

    ESPERA(uc8253_atualiza(&io, bits, false, sizeof bits, UC8253_COMPLETO));
    // Quem orienta é o PSR. O RAM do painel usa 0 = tinta: 80 40 sai 7F BF.
    ESPERA_CONTEM(t.texto, "C13;B2:7FBF;");
    TERMINA();
}

void t_uc8253_primeiro_quadro_faz_refresh_completo(void)
{
    COMECA("UC8253 full sem quadro anterior parte do branco");
    trilha_t t = {0};
    uc8253_io_t io = io_de(&t);
    const uint8_t bits[] = { 0xA5, 0x5A };

    // Sem anterior, o plano velho é BRANCO.
    ESPERA(uc8253_atualiza(&io, bits, false, sizeof bits, UC8253_COMPLETO));
    ESPERA_TEXTO(t.texto,
        // Toda escrita de plano vai dentro da janela, mesmo antes de um completo.
        "C91;C90;B7:00EF0000019F01;C10;B2:FFFF;C92;"
        "C91;C90;B7:00EF0000019F01;C13;B2:5AA5;C92;"
        // CCSET=0x00 desliga o TSFIX: o completo usa o sensor.
        "CE0;B1:00;C50;B1:97;"
        // PON, DRF e POF; sem POF a tinta não assenta.
        "C04;W;C12;W;C02;W;"
        // O fechamento: o plano velho passa a ser este quadro.
        "C91;C90;B7:00EF0000019F01;C10;B2:5AA5;C92;");
    TERMINA();
}

void t_uc8253_o_plano_velho_e_escrito_depois_do_refresh(void)
{
    COMECA("UC8253 escreve o plano velho DEPOIS do refresh, com o mesmo quadro");
    trilha_t t = {0};
    uc8253_io_t io = io_de(&t);
    const uint8_t agora[]  = { 0xA5, 0x5A };

    // O controlador não copia o novo sobre o velho sozinho: o driver oficial
    // escreve o 0x10 depois de cada refresh (`writeImageAgain`). Sem isso o
    // quadro novo SOMA ao anterior. Depois do refresh, nunca antes.
    ESPERA(uc8253_atualiza(&io, agora, true, sizeof agora, UC8253_COMPLETO));
    ESPERA_TEXTO(t.texto,
        "C91;C90;B7:00EF0000019F01;C13;B2:5AA5;C92;"
        "CE0;B1:00;C50;B1:97;"
        "C04;W;C12;W;C02;W;"
        "C91;C90;B7:00EF0000019F01;C10;B2:5AA5;C92;");
    TERMINA();
}

void t_uc8253_dois_quadros_deixam_a_referencia_em_dia(void)
{
    COMECA("UC8253 dois quadros seguidos deixam o plano velho em dia");
    trilha_t t = {0};
    uc8253_io_t io = io_de(&t);
    const uint8_t um[]   = { 0xA5, 0x5A };
    const uint8_t dois[] = { 0x0F, 0xF0 };

    ESPERA(uc8253_atualiza(&io, um,   true, sizeof um,   UC8253_PARCIAL));
    ESPERA(uc8253_atualiza(&io, dois, true, sizeof dois, UC8253_PARCIAL));

    // O primeiro ciclo deixa o quadro UM no plano velho, e o segundo compara
    // contra ele.
    ESPERA_CONTEM(t.texto, "C10;B2:5AA5;");
    ESPERA_CONTEM(t.texto, "C13;B2:F00F;");
    ESPERA_CONTEM(t.texto, "C10;B2:F00F;");
    TERMINA();
}

void t_uc8253_parcial_entra_no_modo_parcial(void)
{
    COMECA("UC8253 parcial entra em modo parcial antes de mexer no vidro");
    trilha_t t = {0};
    uc8253_io_t io = io_de(&t);
    const uint8_t bits[] = { 0xA5, 0x5A };

    ESPERA(uc8253_atualiza(&io, bits, false, sizeof bits, UC8253_PARCIAL));

    // PTIN (0x91) e PTL (0x90), a tela inteira: sem eles o 0x12 roda o ciclo
    // normal.
    ESPERA_CONTEM(t.texto, "C91;C90;B7:00EF0000019F01;");
    // E sai do modo no fim.
    ESPERA_CONTEM(t.texto, "C92;");
    TERMINA();
}

void t_uc8253_parcial_usa_a_waveform_de_fabrica(void)
{
    COMECA("UC8253 parcial usa a tabela curta do OTP, como o driver oficial");
    trilha_t t = {0};
    uc8253_io_t io = io_de(&t);
    const uint8_t bits[] = { 0xA5, 0x5A };

    ESPERA(uc8253_atualiza(&io, bits, false, sizeof bits, UC8253_PARCIAL));

    // TSFIX com 0x6E, o número do `_Update_Part` do GxEPD2.
    ESPERA_CONTEM(t.texto, "CE0;B1:02;CE5;B1:6E;");
    ESPERA_CONTEM(t.texto, "C50;B1:D7;");
    // Sem LUT por registrador: o PSR fica com REG=0.
    ESPERA_SEM(t.texto, "C00;B2:3F0D;");
    ESPERA_SEM(t.texto, "C20;");
    // E o TSFIX não fica pendurado.
    ESPERA_CONTEM(t.texto, "C02;W;C00;B2:1E0D;");
    TERMINA();
}

void t_uc8253_parcial_desfaz_a_temperatura_forcada(void)
{
    COMECA("UC8253 o completo desliga o TSFIX que o parcial acendeu");
    trilha_t t = {0};
    uc8253_io_t io = io_de(&t);
    const uint8_t bits[] = { 0xA5, 0x5A };

    ESPERA(uc8253_atualiza(&io, bits, false, sizeof bits, UC8253_PARCIAL));
    // O soft reset no fim do ciclo traz a base inteira junto (PSR e
    // resolução): sozinho, levava a resolução embora.
    ESPERA_CONTEM(t.texto, "C02;W;C00;B2:1E0D;W;C00;B2:1F0D;C61;B3:F001A0;");

    trilha_t depois = {0};
    uc8253_io_t io2 = io_de(&depois);
    ESPERA(uc8253_atualiza(&io2, bits, true, sizeof bits, UC8253_COMPLETO));
    ESPERA_CONTEM(depois.texto, "CE0;B1:00;");
    TERMINA();
}

void t_uc8253_parcial_separa_as_tres_operacoes(void)
{
    COMECA("UC8253 dá uma sessão parcial a cada operação do ciclo");
    trilha_t t = {0};
    uc8253_io_t io = io_de(&t);
    const uint8_t bits[] = { 0xA5, 0x5A };

    ESPERA(uc8253_atualiza(&io, bits, false, sizeof bits, UC8253_PARCIAL));
    // Como no driver oficial: escrita do plano novo, refresh e fechamento,
    // cada um na sua sessão parcial.
    ESPERA_TEXTO(t.texto,
        // Sem referência, o plano velho vai branco antes.
        "C91;C90;B7:00EF0000019F01;C10;B2:FFFF;C92;"
        // A escrita do quadro novo.
        "C91;C90;B7:00EF0000019F01;C13;B2:5AA5;C92;"
        // O refresh, na sua janela.
        "C91;C90;B7:00EF0000019F01;CE0;B1:02;CE5;B1:6E;C50;B1:D7;"
        "C04;W;C12;W;C02;W;"
        // A temperatura forçada é desfeita antes do PTOUT, com a base inteira.
        "C00;B2:1E0D;W;C00;B2:1F0D;C61;B3:F001A0;C92;"
        // E o fechamento.
        "C91;C90;B7:00EF0000019F01;C10;B2:5AA5;C92;");
    TERMINA();
}

void t_uc8253_a_faxina_leva_o_vidro_aos_extremos(void)
{
    COMECA("UC8253 a faxina leva o vidro a preto e de volta");
    trilha_t t = {0};
    uc8253_io_t io = io_de(&t);

    ESPERA(uc8253_limpa(&io, 2, 1));

    // Os dois planos com o MESMO valor: levar o vidro a um extremo pelas
    // fases de inversão.
    ESPERA_TEXTO(t.texto,
        "C10;B2:0000;C13;B2:0000;"          // tudo preto
        "CE0;B1:00;C50;B1:97;C04;W;C12;W;C02;W;"
        "C10;B2:FFFF;C13;B2:FFFF;"          // e tudo branco
        "CE0;B1:00;C50;B1:97;C04;W;C12;W;C02;W;");
    TERMINA();
}

void t_uc8253_a_faxina_nao_finge_temperatura(void)
{
    COMECA("UC8253 a faxina usa a waveform nativa, não a rápida");
    trilha_t t = {0};
    uc8253_io_t io = io_de(&t);

    ESPERA(uc8253_limpa(&io, 2, 1));

    // Limpar não usa TSFIX: a waveform rápida não tem inversão e não tira
    // resíduo.
    ESPERA_SEM(t.texto, "CE5;");
    ESPERA_CONTEM(t.texto, "CE0;B1:00;");
    TERMINA();
}

void t_uc8253_o_completo_tambem_usa_a_waveform_nativa(void)
{
    COMECA("UC8253 o completo usa a waveform que limpa");
    trilha_t t = {0};
    uc8253_io_t io = io_de(&t);
    const uint8_t bits[] = { 0xA5, 0x5A };

    ESPERA(uc8253_atualiza(&io, bits, false, sizeof bits, UC8253_COMPLETO));
    ESPERA_SEM(t.texto, "CE5;");
    // No completo a escrita continua na janela, mas o REFRESH não.
    ESPERA_CONTEM(t.texto, "C92;CE0;B1:00;C50;B1:97;C04;W;C12;W;C02;W;C91;");
    TERMINA();
}

// ── a política de refresco ──────────────────────────────────────────
// Fora do hal_esp.c, para ter teste no PC.

void t_uc8253_nao_desliga_o_painel_entre_dois_quadros(void)
{
    COMECA("UC8253 fecha o ciclo com POF, sempre");
    trilha_t t = {0};
    uc8253_io_t io = io_de(&t);
    const uint8_t bits[] = { 0xA5, 0x5A };

    // Sem o POF o quadro aparece um comando atrasado, sobreposto ao anterior
    // (este teste já exigiu o contrário; a bancada desmentiu).
    ESPERA(uc8253_atualiza(&io, bits, false, sizeof bits, UC8253_COMPLETO));
    ESPERA_CONTEM(t.texto, "C12;W;C02;W;");

    trilha_t p = {0};
    uc8253_io_t io2 = io_de(&p);
    ESPERA(uc8253_atualiza(&io2, bits, true, sizeof bits, UC8253_PARCIAL));
    ESPERA_CONTEM(p.texto, "C12;W;C02;W;");
    TERMINA();
}

void t_uc8253_desliga_manda_power_off(void)
{
    COMECA("UC8253 desliga manda POF e espera o painel baixar");
    trilha_t t = {0};
    uc8253_io_t io = io_de(&t);

    ESPERA(uc8253_desliga(&io));
    ESPERA_TEXTO(t.texto, "C02;W;");
    TERMINA();
}

// ── o refresh de uma FAIXA ──────────────────────────────────────────
// Barra e rodapé mudam sozinhos: a janela PTL repinta só eles. Com PSR 0x1F a
// faixa do quadro é a do painel; em outra orientação a conta muda.
void t_uc8253_faixa_pinta_so_a_faixa(void)
{
    COMECA("UC8253 pinta só a faixa pedida, e diz isso na janela");
    trilha_t t = {0};
    uc8253_io_t io = io_de(&t);

    // 240 px: 30 bytes por linha.
    static uint8_t bits[30 * 8];
    for (size_t i = 0; i < sizeof bits; i++) bits[i] = (uint8_t)i;

    uc8253_faixa_t faixa = { .y0 = 2, .y1 = 3 };
    ESPERA(uc8253_atualiza_faixa(&io, bits, sizeof bits, 30, &faixa));

    // A janela cobre a largura toda e só as linhas 2 e 3.
    ESPERA_CONTEM(t.texto, "C90;B7:00EF00020003");

    // E só duas linhas atravessam o SPI (60 bytes por plano).
    ESPERA_IGUAL(t.dados_do_ultimo_plano, 60);

    TERMINA();
}

// Quando MUITO muda na mesma tela (Wi-Fi achando redes), o parcial deixa o
// quadro velho por baixo: vira completo.
void t_refresco_quadro_que_muda_muito_pede_completo(void)
{
    COMECA("parcial só quando POUCO muda; um terço da tela já pede completo");

    const size_t total = 12480;          // 240x416 em 1 bit

    // O relógio: dois dígitos, parcial.
    ESPERA(refresco_escolhe_medindo(false, true, 40, total) == REFRESCO_PARCIAL);

    // O cursor andando: parcial.
    ESPERA(refresco_escolhe_medindo(false, true, 300, total) == REFRESCO_PARCIAL);

    // Metade da tela: completo.
    ESPERA(refresco_escolhe_medindo(false, true, total / 2, total)
           == REFRESCO_COMPLETO);

    // A fronteira é um terço.
    ESPERA(refresco_escolhe_medindo(false, true, total / 3 - 10, total)
           == REFRESCO_PARCIAL);
    ESPERA(refresco_escolhe_medindo(false, true, total / 3 + 10, total)
           == REFRESCO_COMPLETO);

    // Sem quadro anterior, completo sempre.
    ESPERA(refresco_escolhe_medindo(false, false, 0, total) == REFRESCO_COMPLETO);
    TERMINA();
}

// Sair do teclado repinta inteiro: a grade ficava presa no vidro.
void t_eink_sair_do_teclado_repinta_inteiro(void)
{
    COMECA("sair do teclado manda um quadro completo");

    static app_t ap;
    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 12}, 9, 14);
    app_liga(&ap, hal);
    app_passo(&ap);

    ap.estado.pilha[++ap.estado.profundidade] = TELA_TECLADO;
    ap.precisa_desenhar = true;
    app_desenha(&ap);
    ESPERA_IGUAL(ap.tela_anterior, TELA_TECLADO);

    // Sai: o app registra que veio do teclado.
    ap.estado.pilha[ap.estado.profundidade] = TELA_WIFI;
    ap.precisa_desenhar = true;
    app_desenha(&ap);
    ESPERA_IGUAL(ap.tela_anterior, TELA_WIFI);
    TERMINA();
}

// ── a intenção vence a medida ───────────────────────────────────────
// A regra do 1/3 é para a tela que muda sozinha. O FOCO mede perto do corte
// (mover o cartão da Home dá 30,8–33,0%), e não pode ter o custo decidido
// por contagem.
void t_refresco_o_foco_e_parcial_mesmo_medindo_muito(void)
{
    COMECA("mover o foco é parcial mesmo quando a medida diz \"muito mudou\"");

    const size_t total = 12480;

    // Bem acima do corte: 40% da tela.
    ESPERA_IGUAL(refresco_por_intencao(PINTURA_FOCO, true, 4992, total),
                 REFRESCO_PARCIAL);

    // A mesma tela mudando SOZINHA continua obedecendo a medida.
    ESPERA_IGUAL(refresco_por_intencao(PINTURA_MESMA_TELA, true, 4992, total),
                 REFRESCO_COMPLETO);
    ESPERA_IGUAL(refresco_por_intencao(PINTURA_MESMA_TELA, true, 100, total),
                 REFRESCO_PARCIAL);

    // Tela nova é completo sempre.
    ESPERA_IGUAL(refresco_por_intencao(PINTURA_TELA_NOVA, true, 10, total),
                 REFRESCO_COMPLETO);

    // Sem quadro anterior, nada é parcial.
    ESPERA_IGUAL(refresco_por_intencao(PINTURA_FOCO, false, 10, total),
                 REFRESCO_COMPLETO);
    TERMINA();
}

// ── e o app declara essa intenção de verdade ────────────────────────
// O fio que liga a política ao app.
void t_refresco_mover_o_foco_na_home_declara_a_intencao(void)
{
    COMECA("mover o cartão da Home chega ao vidro como movimento de foco");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 9, 1}, 11, 17);
    static app_t ap;
    app_liga(&ap, hal);
    app_passo(&ap);

    // Entrar na Home é tela nova. Conta-se: o último quadro de uma navegação é
    // o seletor, num parcial (§5.5).
    ESPERA_IGUAL(pc_pinturas(PINTURA_TELA_NOVA), 1);
    ESPERA_IGUAL(pc_pinturas(PINTURA_FOCO), 0);

    // Mover o cartão: nenhuma tela nova.
    pc_botao(IN_DIR);
    app_passo(&ap);
    ESPERA_IGUAL(pc_pinturas(PINTURA_TELA_NOVA), 1);   // não subiu
    ESPERA(pc_pinturas(PINTURA_FOCO) >= 1);

    // Abrir a área É tela nova.
    pc_botao(IN_OK);
    app_passo(&ap);
    ESPERA_IGUAL(pc_pinturas(PINTURA_TELA_NOVA), 2);
    TERMINA();
}

// ── páginas, e não rolagem ──────────────────────────────────────────
// Rolar 56 px deixava o texto sobre o próprio fantasma. Página inteira troca
// todo o conteúdo.
void t_a_rolagem_anda_de_pagina_inteira(void)
{
    COMECA("e-ink · o corpo longo anda de página, e não de 56 px");

    const int area = 300;

    // Cabe inteiro: uma parada.
    ESPERA_IGUAL(rolagem_paradas(280, area), 1);
    ESPERA_IGUAL(rolagem_desloc(0, 280, area), 0);

    // Duas telas e um pedaço: três páginas.
    ESPERA_IGUAL(rolagem_paradas(700, area), 3);

    // Cada página salta a área inteira, sem clamp na última (sobra branco, de
    // propósito).
    ESPERA_IGUAL(rolagem_desloc(0, 700, area), 0);
    ESPERA_IGUAL(rolagem_desloc(1, 700, area), area);
    ESPERA_IGUAL(rolagem_desloc(2, 700, area), area * 2);

    TERMINA();
}

// Rolar um detalhe continua na MESMA TELA: deslocamento curto, parcial.
void t_virar_pagina_chega_ao_vidro_como_tela_nova(void)
{
    COMECA("e-ink · detalhe rola suave com parcial, sem virar página");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 9, 2}, 11, 17);
    static app_t ap;
    app_liga(&ap, hal);
    app_passo(&ap);

    // Uma nota com texto que não cabe.
    snprintf(ap.estado.aberto.id, sizeof ap.estado.aberto.id, "%s", "0712-n");
    snprintf(ap.estado.aberto.titulo, sizeof ap.estado.aberto.titulo,
             "%s", "ideia do painel");
    snprintf(ap.estado.aberto.hora, sizeof ap.estado.aberto.hora, "%s", "07:12");
    ap.estado.aberto.tipo = TIPO_TAREFA;
    snprintf(ap.estado.aberto.nota, sizeof ap.estado.aberto.nota, "%s", "0712-n");
    ap.estado.aberto.dia  = ap.estado.hoje;
    ap.estado.aberto_valido = true;
    size_t cabe = sizeof ap.estado.texto.transcricao - 1;
    for (size_t i = 0; i < cabe; i++)
        ap.estado.texto.transcricao[i] =
            (i % 7 == 6) ? ' ' : (char)('a' + (int)(i % 20));
    ap.estado.texto.transcricao[cabe] = '\0';

    size_t cabe_r = sizeof ap.estado.texto.resumo - 1;
    for (size_t i = 0; i < cabe_r; i++)
        ap.estado.texto.resumo[i] =
            (i % 7 == 6) ? ' ' : (char)('a' + (int)(i % 20));
    ap.estado.texto.resumo[cabe_r] = '\0';
    ap.estado.texto.tem_resumo = true;
    ap.estado.profundidade = 1;
    ap.estado.pilha[1] = TELA_NOTA;
    ap.estado.cursor = 0;
    ap.precisa_desenhar = true;
    app_passo(&ap);

    int novas_antes = pc_pinturas(PINTURA_TELA_NOVA);
    int foco_antes  = pc_pinturas(PINTURA_FOCO);

    pc_botao(IN_BAIXO);
    app_passo(&ap);

    ESPERA_IGUAL(pc_pinturas(PINTURA_TELA_NOVA), novas_antes);
    ESPERA(pc_pinturas(PINTURA_FOCO) > foco_antes);

    TERMINA();
}

// ── a paginação é uma peça do sistema ───────────────────────────────
// "2 / 3": tem mais, quanto mais, onde estou. A conta mora num lugar só.
void t_a_paginacao_conta_as_paginas_num_lugar_so(void)
{
    COMECA("páginas · a conta é uma só, e o contador diz onde se está");

    const int area = 300;

    // Cabe inteiro: uma página e nenhum contador.
    ESPERA_IGUAL(pagina_total(280, area), 1);

    ESPERA_IGUAL(pagina_total(700, area), 3);
    ESPERA_IGUAL(pagina_atual(0, 700, area), 1);
    ESPERA_IGUAL(pagina_atual(area, 700, area), 2);
    ESPERA_IGUAL(pagina_atual(area * 2, 700, area), 3);

    // Deslocamento maior que o conteúdo não inventa página.
    ESPERA_IGUAL(pagina_atual(area * 9, 700, area), 3);

    TERMINA();
}

// Mover o cursor nunca paga refresh completo: trocar um destino de card
// mede ~34%, acima do corte, e a tela piscava a cada toque.
void t_mover_o_cursor_nunca_paga_refresh_completo(void)
{
    COMECA("e-ink · andar com o cursor é foco, e foco é sempre parcial");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 9, 3}, 11, 17);
    static app_t ap;
    app_liga(&ap, hal);
    app_passo(&ap);

    // Uma tela de card.
    ap.estado.profundidade = 1;
    ap.estado.pilha[1] = TELA_AJUSTES;
    ap.estado.cursor = 0;
    ap.precisa_desenhar = true;
    app_passo(&ap);

    int completos = pc_pinturas(PINTURA_TELA_NOVA);
    int focos     = pc_pinturas(PINTURA_FOCO);

    // Três descidas, nenhuma completa.
    for (int i = 0; i < 3; i++) {
        pc_botao(IN_BAIXO);
        app_passo(&ap);
    }

    ESPERA_IGUAL(pc_pinturas(PINTURA_TELA_NOVA), completos);
    ESPERA(pc_pinturas(PINTURA_FOCO) > focos);

    TERMINA();
}


// O pull de fundo NÃO anima a barra: com long polling seria montar a vista
// a cada segundo. Anima só o que a pessoa espera.
void t_o_pull_de_fundo_nao_anima_a_barra(void)
{
    COMECA("e-ink · o que anima a barra é o que a pessoa está esperando");

    // O que a pessoa disparou: anima.
    ESPERA(vista_espera_visivel(NUVEM_CAPTURA));
    ESPERA(vista_espera_visivel(NUVEM_GESTO));
    ESPERA(vista_espera_visivel(NUVEM_ESCOLHA));
    ESPERA(vista_espera_visivel(NUVEM_AGENDAS));

    // O que o aparelho faz sozinho: não.
    ESPERA(!vista_espera_visivel(NUVEM_PULL));
    ESPERA(!vista_espera_visivel(NUVEM_REGISTRAR));
    ESPERA(!vista_espera_visivel(NUVEM_NADA));

    TERMINA();
}

// O desenho não espera a fila esvaziar: com a fila enchendo tão rápido
// quanto esvazia, o quadro era adiado para sempre. Teto por passo.
void t_o_desenho_nao_espera_a_fila_esvaziar(void)
{
    COMECA("app · com a fila cheia, o quadro sai mesmo assim");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 9, 3}, 11, 17);
    static app_t ap;
    app_liga(&ap, hal);
    app_passo(&ap);

    int antes = pc_pinturas(PINTURA_TELA_NOVA) + pc_pinturas(PINTURA_FOCO) +
                pc_pinturas(PINTURA_MESMA_TELA);

    // Muito mais eventos do que um passo trata.
    for (int i = 0; i < 200; i++) pc_botao(i % 2 ? IN_BAIXO : IN_CIMA);
    app_passo(&ap);

    int depois = pc_pinturas(PINTURA_TELA_NOVA) + pc_pinturas(PINTURA_FOCO) +
                 pc_pinturas(PINTURA_MESMA_TELA);

    ESPERA(depois > antes);

    TERMINA();
}

// O segundo quadro de uma navegação não relê o cartão: a bandeira do
// cache impede a segunda leitura.
void t_montar_duas_vezes_le_o_cartao_uma_vez(void)
{
    COMECA("cartão · dois quadros, uma varredura");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 9, 3}, 9, 0);
    static app_t ap4;
    app_liga(&ap4, hal);
    app_passo(&ap4);

    // Uma navegação de verdade: dois quadros.
    estado_invalida_cartao(&ap4.estado);
    ap4.precisa_desenhar = true;
    app_passo(&ap4);
    int primeira = pc_listagens();

    // De novo, com o cache quente: nenhuma listagem a mais.
    ap4.precisa_desenhar = true;
    app_passo(&ap4);

    ESPERA_IGUAL(pc_listagens(), primeira);

    TERMINA();
}
