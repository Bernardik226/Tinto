// firmware/testes/t_nota.c — T-16, o detalhe do item.
// Resumo em cima e transcrição crua embaixo: verificável que a IA não
// inventou.
#include "teste.h"
#include "vista/anotacoes.h"
#include "uso/uso.h"
#include "dado/cartao.h"
#include "vista/nota.h"
#include "ui/nota.h"
#include "tela/bitmap.h"

static estado_t e;

static void nota_com(const char *titulo, tipo_t tipo, int dur_s,
                     const char *transcricao, const char *resumo)
{
    memset(&e, 0, sizeof e);
    e.config.valor[AJUSTE_HORA24] = 1;   // 24 h, o padrão de fábrica
    e.hoje = e.dia_visto = (data_t){2026, 8, 12};
    e.hora = 7; e.minuto = 44;
    e.bateria = 78;

    snprintf(e.aberto.id,     sizeof e.aberto.id,     "%s", "0712-nota");
    snprintf(e.aberto.titulo, sizeof e.aberto.titulo, "%s", titulo);
    snprintf(e.aberto.hora,   sizeof e.aberto.hora,   "%s", "07:12");
    e.aberto.tipo  = tipo;
    e.aberto.dia   = (data_t){2026, 8, 12};
    e.aberto.dur_s = (int16_t)dur_s;

    snprintf(e.texto.transcricao, sizeof e.texto.transcricao, "%s", transcricao);
    if (resumo && *resumo) {
        snprintf(e.texto.resumo, sizeof e.texto.resumo, "%s", resumo);
        e.texto.tem_resumo = true;
    }
    e.aberto_valido = true;
}

// RN-28: a transcrição crua está sempre lá.
void t_a_transcricao_crua_aparece_sempre(void)
{
    COMECA("RN-28 · a transcrição crua aparece, com os \"né\" e tudo");

    nota_com("ideia do painel", TIPO_ANOTACAO, 23,
             "então, tava pensando, né, que o painel…",
             "O painel da dock deve mostrar as tarefas do dia.");

    vista_nota_t v;
    vista_nota(&e, &v);

    ESPERA(v.tem_resumo);
    ESPERA_TEXTO(v.transcricao, "então, tava pensando, né, que o painel…");
    ESPERA_TEXTO(v.resumo, "O painel da dock deve mostrar as tarefas do dia.");

    TERMINA();
}

// RN-25: anotação nunca vai ao Google, e o aparelho diz isso.
void t_anotacao_diz_que_e_so_no_aparelho(void)
{
    COMECA("RN-25 · a anotação diz na cara que não vai ao Google");

    // O destino em linha própria (RN-4D), dizendo em qual produto procurar.
    nota_com("ideia do painel", TIPO_ANOTACAO, 23, "tava pensando", "resumo");
    vista_nota_t v;
    vista_nota(&e, &v);
    ESPERA_TEXTO(v.onde, "ANOTAÇÃO · fora do Google");

    // Tarefa com hora é o HÍBRIDO, e o kicker diz isso.
    nota_com("comprar pasta", TIPO_TAREFA, 12, "comprar pasta", "resumo");
    vista_nota(&e, &v);
    ESPERA_TEXTO(v.onde, "TAREFA COM HORA · Minhas tarefas");

    // Sem hora, uma tarefa como sempre.
    e.aberto.hora[0] = '\0';
    vista_nota(&e, &v);
    ESPERA_TEXTO(v.onde, "TAREFA · Minhas tarefas");

    TERMINA();
}

// RN-15: sem rede a nota abre igual, com um estado no lugar do resumo.
void t_sem_ia_a_nota_abre_do_mesmo_jeito(void)
{
    COMECA("RN-15 · sem a IA, a nota abre igual e diz o estado");

    nota_com("", TIPO_NADA, 41, "comprar pasta térmica hoje", "");
    vista_nota_t v;
    vista_nota(&e, &v);

    ESPERA(!v.tem_resumo);
    ESPERA_TEXTO(v.aviso, "por estruturar");
    ESPERA_TEXTO(v.transcricao, "comprar pasta térmica hoje");
    ESPERA_TEXTO(v.tl, "(Sem título)");

    TERMINA();
}

void t_sem_nem_transcricao_o_aviso_muda(void)
{
    COMECA("RN-A1 · sem transcrição, o estado é outro e cabe em 4 palavras");

    nota_com("", TIPO_NADA, 41, "", "");
    vista_nota_t v;
    vista_nota(&e, &v);

    ESPERA_TEXTO(v.aviso, "por transcrever · sem rede");

    TERMINA();
}

// 23 s vira "0:23".
void t_a_duracao_sai_em_minutos_e_segundos(void)
{
    COMECA("a duração do áudio sai como 0:23, não como 23");

    nota_com("ideia", TIPO_ANOTACAO, 23, "tava pensando", "resumo");
    vista_nota_t v;
    vista_nota(&e, &v);
    ESPERA_TEXTO(v.quando, "12 ago · 07:12 · 0:23");

    nota_com("longa", TIPO_ANOTACAO, 125, "tava pensando", "resumo");
    vista_nota(&e, &v);
    ESPERA_TEXTO(v.quando, "12 ago · 07:12 · 2:05");

    TERMINA();
}

// ── Anotações: por recência, agrupadas por dia ──────────────────────
// Não se acham pelo calendário: o dia é rótulo, não destino.
void t_anotacoes_lista_por_recencia_agrupada_por_dia(void)
{
    static app_t ap;
    COMECA("Anotações lista da mais nova pra mais velha, com o dia em cima");

    const hal_t *hal = pc_liga();
    const data_t hoje = {2026, 8, 25};
    pc_relogio(hoje, 9, 14);

    // Gravadas fora de ordem de propósito.
    struct { data_t d; const char *id, *t, *h; } fatos[] = {
        { {2026,8,23}, "1802-oulu",  "Conversa com o Oulu", "18:02" },
        { hoje,        "0741-pasta", "Pasta térmica",       "07:41" },
        { hoje,        "0915-painel","Ideia do painel",     "09:15" },
    };
    for (unsigned i = 0; i < sizeof fatos / sizeof fatos[0]; i++) {
        item_t it;
        memset(&it, 0, sizeof it);
        snprintf(it.id,     sizeof it.id,     "%s", fatos[i].id);
        snprintf(it.titulo, sizeof it.titulo, "%s", fatos[i].t);
        snprintf(it.hora,   sizeof it.hora,   "%s", fatos[i].h);
        it.tipo = TIPO_ANOTACAO;
        it.dia  = fatos[i].d;
        cartao_grava_item(hal, fatos[i].d, &it);
    }

    // E uma TAREFA de hoje, que não aparece aqui.
    item_t tarefa;
    memset(&tarefa, 0, sizeof tarefa);
    snprintf(tarefa.id,     sizeof tarefa.id,     "%s", "0800-ipva");
    snprintf(tarefa.titulo, sizeof tarefa.titulo, "%s", "Pagar IPVA");
    tarefa.tipo = TIPO_TAREFA;
    tarefa.dia  = hoje;
    cartao_grava_item(hal, hoje, &tarefa);

    app_liga(&ap, hal);
    app_passo(&ap);
    ESPERA_IGUAL(uso_carregar_anotacoes(hal, &ap.estado), OK);

    vista_anotacoes_t v;
    vista_anotacoes(&ap.estado, &v);

    ESPERA_IGUAL(v.n, 3);                       // a tarefa ficou de fora

    // Só a primeira de cada dia leva o rótulo.
    ESPERA_TEXTO(v.dia[0], "HOJE");
    ESPERA_TEXTO(v.dia[1], "");
    ESPERA(v.dia[2][0] != '\0');                // outro dia, outro rótulo
    ESPERA(strcmp(v.dia[2], "HOJE") != 0);
    ESPERA(strcmp(v.dia[2], "ONTEM") != 0);     // é de anteontem

    // A de outro dia por último.
    ESPERA_TEXTO(v.linhas[2].titulo, "Conversa com o Oulu");
    TERMINA();
}

// Sem nenhuma, a tela CONVIDA.
void t_anotacoes_vazia_convida_em_vez_de_calar(void)
{
    static app_t ap;
    COMECA("sem anotações, a tela diz o que fazer em vez de ficar vazia");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 25}, 9, 14);
    app_liga(&ap, hal);
    app_passo(&ap);
    ESPERA_IGUAL(uso_carregar_anotacoes(hal, &ap.estado), OK);

    vista_anotacoes_t v;
    vista_anotacoes(&ap.estado, &v);

    ESPERA_IGUAL(v.n, 0);
    ESPERA(v.vazio[0]);
    ESPERA_TEXTO(v.rodape_dir, "");    // não promete um OK que não faz nada
    TERMINA();
}

// A transcrição vem da NOTA, não do item: uma fala que vira duas ações tem
// uma nota só.
void t_a_transcricao_vem_da_nota_e_nao_do_item(void)
{
    static app_t ap;
    COMECA("duas ações da mesma fala mostram a MESMA transcrição");

    const hal_t *hal = pc_liga();
    const data_t hoje = {2026, 8, 26};
    pc_relogio(hoje, 9, 14);
    app_liga(&ap, hal);
    app_passo(&ap);

    // Uma fala que virou duas coisas.
    const char *falou = "marca dentista quinta e lembra de comprar pasta";
    ESPERA_IGUAL(cartao_grava_transcricao(hal, "nt:abc123", falou), OK);

    struct { const char *id, *t; tipo_t tp; } acoes[] = {
        { "g:1", "Dentista",      TIPO_EVENTO },
        { "t:2", "Comprar pasta", TIPO_TAREFA },
    };
    for (unsigned i = 0; i < 2; i++) {
        item_t it;
        memset(&it, 0, sizeof it);
        snprintf(it.id,     sizeof it.id,     "%s", acoes[i].id);
        snprintf(it.titulo, sizeof it.titulo, "%s", acoes[i].t);
        snprintf(it.nota,   sizeof it.nota,   "%s", "nt:abc123");
        it.tipo = acoes[i].tp;
        it.dia  = hoje;
        cartao_grava_item(hal, hoje, &it);
    }

    // As duas acham a mesma frase.
    for (unsigned i = 0; i < 2; i++) {
        item_t alvo;
        memset(&alvo, 0, sizeof alvo);
        snprintf(alvo.id, sizeof alvo.id, "%s", acoes[i].id);
        alvo.dia = hoje;

        ESPERA_IGUAL(uso_abrir_item(hal, &ap.estado, &alvo), OK);
        ESPERA(strstr(ap.estado.texto.transcricao, "dentista") != NULL);
        ESPERA_TEXTO(ap.estado.aberto.nota, "nt:abc123");
    }
    TERMINA();
}

// O que veio do Google não tem nota, e o detalhe diz isso.
void t_item_do_google_nao_tem_nota_nenhuma(void)
{
    static app_t ap;
    COMECA("item vindo do Google não tem nota, e a tela não inventa uma");

    const hal_t *hal = pc_liga();
    const data_t hoje = {2026, 8, 26};
    pc_relogio(hoje, 9, 14);
    app_liga(&ap, hal);
    app_passo(&ap);

    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,     sizeof it.id,     "%s", "g:vindo-de-fora");
    snprintf(it.titulo, sizeof it.titulo, "%s", "Daily do time");
    it.tipo   = TIPO_EVENTO;
    it.origem = ORIGEM_GOOGLE;
    it.dia    = hoje;
    cartao_grava_item(hal, hoje, &it);

    item_t alvo;
    memset(&alvo, 0, sizeof alvo);
    snprintf(alvo.id, sizeof alvo.id, "%s", "g:vindo-de-fora");
    alvo.dia = hoje;

    ESPERA_IGUAL(uso_abrir_item(hal, &ap.estado, &alvo), OK);
    ESPERA_TEXTO(ap.estado.aberto.nota, "");
    ESPERA_TEXTO(ap.estado.texto.transcricao, "");
    TERMINA();
}


// A origem não se sincroniza: criado por voz continua dizendo isso; do
// Google, nunca inventa uma fala. O vínculo é o `nota`.
void t_a_origem_do_item_nao_se_sincroniza(void)
{
    COMECA("o item diz se nasceu de uma fala aqui ou se veio do Google");

    // ── nasceu aqui, de uma fala ──
    nota_com("Dentista", TIPO_EVENTO, 23, "marca dentista amanhã", "Dentista");
    snprintf(e.aberto.nota, sizeof e.aberto.nota, "%s", "0712-fala");
    e.aberto.origem = ORIGEM_AQUI;

    vista_nota_t v;
    vista_nota(&e, &v);
    ESPERA_CONTEM(v.origem, "voz");
    ESPERA_CONTEM(v.origem, "Tinto");
    ESPERA(v.tem_fala);

    // ── e continua dizendo isso depois do sync ──
    e.aberto.origem = ORIGEM_GOOGLE;      // o sync mexeu nos campos
    vista_nota(&e, &v);
    ESPERA_CONTEM(v.origem, "voz");
    ESPERA(v.tem_fala);

    // ── nasceu no Google ──
    e.aberto.nota[0] = '\0';
    vista_nota(&e, &v);
    ESPERA_CONTEM(v.origem, "Google");
    ESPERA_SEM(v.origem, "voz");
    ESPERA(!v.tem_fala);

    // ── e a anotação concorda no feminino ──
    nota_com("Ideia da dock", TIPO_ANOTACAO, 23, "a dock deve mostrar", "A dock");
    snprintf(e.aberto.nota, sizeof e.aberto.nota, "%s", "2102-fala");
    vista_nota(&e, &v);
    ESPERA_CONTEM(v.origem, "Criada");
    TERMINA();
}


// Concluir mora no DETALHE da tarefa pendente.
void t_concluir_mora_no_detalhe_da_tarefa(void)
{
    COMECA("a tarefa pendente oferece concluir; a feita oferece desmarcar");

    nota_com("Levar os exames", TIPO_TAREFA, 23, "leva os exames", "Levar");
    snprintf(e.aberto.nota, sizeof e.aberto.nota, "%s", "0712-fala");
    e.rede = REDE_LIGADA;

    vista_nota_t v;
    vista_nota(&e, &v);

    // UM botão: a transcrição desce no corpo.
    ESPERA_IGUAL(v.n_botoes, 1);
    ESPERA_CONTEM(v.botoes[0].texto, "conclu");
    ESPERA_CONTEM(v.rodape_dir, "concluir");

    // E a fala continua alcançável, rolando.
    ESPERA(v.tem_fala);

    // Concluída, o gesto vira DESMARCAR, e não some.
    e.aberto.feita = true;
    vista_nota(&e, &v);
    ESPERA_IGUAL(v.n_botoes, 1);
    ESPERA_TEXTO(v.botoes[0].texto, "Desmarcar");
    ESPERA_CONTEM(v.rodape_dir, "reabrir");

    bool diz_concluida = false;
    for (int i = 0; i < v.n_campos; i++)
        if (strstr(v.campos[i].valor, "Concluída")) diz_concluida = true;
    ESPERA(diz_concluida);
    TERMINA();
}

// Apagar e renomear alcançam tudo, inclusive o que veio do Google (escopo
// `calendar` inteiro).
void t_apagar_alcanca_o_que_veio_do_google(void)
{
    COMECA("evento do Google se apaga e se renomeia, como qualquer item");

    nota_com("Reunião de produto", TIPO_EVENTO, 0, "", "");
    e.aberto.origem = ORIGEM_GOOGLE;
    e.aberto.nota[0] = '\0';
    e.rede = REDE_LIGADA;

    vista_nota_t v;
    vista_nota(&e, &v);

    bool tem_apagar = false, tem_renomear = false;
    for (int i = 0; i < v.n_acoes; i++) {
        if (v.acoes[i].icone == ICO_LIXO)     tem_apagar   = true;
        if (v.acoes[i].icone == ICO_RENOMEAR) tem_renomear = true;
    }
    ESPERA(tem_apagar);
    ESPERA(tem_renomear);

    // E a tela não diz que é só leitura.
    ESPERA_IGUAL(v.sem_acoes[0], '\0');
    TERMINA();
}

// O detalhe mostra a gravação inteira: todas as ações que saíram da mesma
// fala, com o estado de cada uma.
void t_fala_original_mostra_o_item_e_a_gravacao_inteira(void)
{
    COMECA("o detalhe mostra tudo o que a mesma fala criou");

    nota_com("Dentista", TIPO_EVENTO, 23,
             "marca dentista amanhã às duas e lembra de levar os exames", "");
    snprintf(e.aberto.nota, sizeof e.aberto.nota, "%s", "0712-fala");
    snprintf(e.aberto.hora, sizeof e.aberto.hora, "%s", "14:00");
    snprintf(e.aberto.fim,  sizeof e.aberto.fim,  "%s", "15:00");

    // As duas ações da mesma gravação.
    for (int i = 0; i < 2; i++) {
        item_t *o = &e.itens[e.n_itens++];
        memset(o, 0, sizeof *o);
        snprintf(o->nota, sizeof o->nota, "%s", "0712-fala");
        o->dia = e.hoje;
    }
    snprintf(e.itens[0].titulo, sizeof e.itens[0].titulo, "%s", "Dentista");
    e.itens[0].tipo = TIPO_EVENTO;
    snprintf(e.itens[1].titulo, sizeof e.itens[1].titulo, "%s", "Levar exames");
    e.itens[1].tipo = TIPO_TAREFA;
    e.itens_validos = true;

    vista_nota_t v;
    vista_nota(&e, &v);

    ESPERA_CONTEM(v.tl, "Dentista");
    ESPERA_CONTEM(v.faixa, "14:00");

    // Com o tipo em cada linha.
    ESPERA_IGUAL(v.n_criou, 2);
    ESPERA_TEXTO(v.criou[0].oque,   "Evento · Dentista");
    ESPERA_TEXTO(v.criou[0].estado, "marcado");
    ESPERA_TEXTO(v.criou[1].oque,   "Tarefa · Levar exames");
    ESPERA_TEXTO(v.criou[1].estado, "pendente");

    TERMINA();
}

// A tarefa concluída diz QUANDO foi concluída.
void t_o_detalhe_diz_quando_a_tarefa_foi_concluida(void)
{
    COMECA("a tarefa concluída mostra a data da conclusão");

    nota_com("Comprar componentes", TIPO_TAREFA, 0, "", "");
    e.aberto.feita    = true;
    e.aberto.feita_em = (data_t){ 2026, 9, 2 };
    e.hoje = e.dia_visto = (data_t){ 2026, 9, 5 };

    vista_nota_t v;
    vista_nota(&e, &v);

    bool tem = false;
    for (int i = 0; i < v.n_campos; i++)
        if (strcmp(v.campos[i].rotulo, "CONCLUÍDA") == 0) {
            tem = true;
            ESPERA_CONTEM(v.campos[i].valor, "2");
            ESPERA_CONTEM(v.campos[i].valor, "set");
        }
    ESPERA(tem);

    // Aberta, o campo não existe.
    e.aberto.feita = false;
    vista_nota(&e, &v);
    for (int i = 0; i < v.n_campos; i++)
        ESPERA(strcmp(v.campos[i].rotulo, "CONCLUÍDA") != 0);
    TERMINA();
}

// O PRAZO (`due`) abaixo de CRIADA, e depois de vencer diz que VENCEU.
void t_o_detalhe_separa_data_e_prazo_da_tarefa(void)
{
    COMECA("detalhe · o prazo aparece embaixo, e diz quando venceu");
    nota_com("Entregar relatório", TIPO_TAREFA, 0, "", "");
    e.hoje = (data_t){ 2026, 9, 11 };
    e.aberto.vence = (data_t){ 2026, 9, 14 };

    // Sem hora: com hora o campo é QUANDO.
    e.aberto.hora[0] = '\0';

    vista_nota_t v;
    vista_nota(&e, &v);

    int prazo = -1, criada = -1;
    for (int i = 0; i < v.n_campos; i++) {
        if (strcmp(v.campos[i].rotulo, "PRAZO") == 0)  prazo = i;
        if (strcmp(v.campos[i].rotulo, "CRIADA") == 0) criada = i;
    }
    ESPERA(prazo >= 0);
    ESPERA(criada >= 0 && prazo > criada);        // abaixo de CRIADA
    ESPERA_CONTEM(v.campos[prazo].valor, "em 3 dias");

    // Vencido: a palavra diz que o plano passou.
    e.aberto.vence = (data_t){ 2026, 9, 8 };
    vista_nota(&e, &v);
    for (int i = 0; i < v.n_campos; i++)
        if (strcmp(v.campos[i].rotulo, "PRAZO") == 0) {
            ESPERA_CONTEM(v.campos[i].valor, "venceu");
            ESPERA_CONTEM(v.campos[i].valor, "8 set");
        }

    // Sem prazo, o campo não existe.
    memset(&e.aberto.vence, 0, sizeof e.aberto.vence);
    vista_nota(&e, &v);
    for (int i = 0; i < v.n_campos; i++)
        ESPERA(strcmp(v.campos[i].rotulo, "PRAZO") != 0);
    TERMINA();
}

// O detalhe não omite: valor comprido QUEBRA.
void t_o_detalhe_quebra_o_valor_em_vez_de_cortar(void)
{
    COMECA("valor comprido no detalhe quebra a linha, não vira reticência");

    nota_com("Reunião de produto", TIPO_EVENTO, 0, "", "");
    snprintf(e.aberto.hora,  sizeof e.aberto.hora,  "%s", "14:00");
    snprintf(e.aberto.fim,   sizeof e.aberto.fim,   "%s", "15:30");
    snprintf(e.aberto.local, sizeof e.aberto.local, "%s",
             "Rua Bahia, 210 · Funcionários · Belo Horizonte");
    e.aberto.dia = e.hoje;

    vista_nota_t v;
    vista_nota(&e, &v);

    // A vista entrega o valor inteiro; quem quebra é a tela.
    for (int i = 0; i < v.n_campos; i++) {
        ESPERA(strstr(v.campos[i].valor, "…") == NULL);
        ESPERA(strstr(v.campos[i].valor, "...") == NULL);
    }

    bool onde = false;
    for (int i = 0; i < v.n_campos; i++)
        if (strcmp(v.campos[i].rotulo, "ONDE") == 0) {
            onde = true;
            ESPERA_CONTEM(v.campos[i].valor, "Belo Horizonte");
        }
    ESPERA(onde);

    // A faixa sai completa, com o fim.
    for (int i = 0; i < v.n_campos; i++)
        if (strcmp(v.campos[i].rotulo, "QUANDO") == 0)
            ESPERA_CONTEM(v.campos[i].valor, "15:30");
    TERMINA();
}

// O evento diz em qual AGENDA mora (o nome atravessa do backend).
void t_o_evento_diz_em_qual_agenda_mora(void)
{
    COMECA("o detalhe do evento diz de qual agenda ele é");

    nota_com("Reunião de produto", TIPO_EVENTO, 0, "", "");
    e.aberto.dia = e.hoje;
    snprintf(e.aberto.hora,   sizeof e.aberto.hora,   "%s", "09:30");
    snprintf(e.aberto.agenda, sizeof e.aberto.agenda, "%s", "Trabalho");

    vista_nota_t v;
    vista_nota(&e, &v);

    bool tem = false;
    for (int i = 0; i < v.n_campos; i++)
        if (strcmp(v.campos[i].rotulo, "AGENDA") == 0) {
            tem = true;
            ESPERA_TEXTO(v.campos[i].valor, "Trabalho");
        }
    ESPERA(tem);

    // Sem nome, a linha não aparece.
    e.aberto.agenda[0] = '\0';
    vista_nota(&e, &v);
    for (int i = 0; i < v.n_campos; i++)
        ESPERA(strcmp(v.campos[i].rotulo, "AGENDA") != 0);
    TERMINA();
}

// O detalhe não corta o título: o que não cabe vira página.
void t_o_detalhe_nao_corta_o_titulo(void)
{
    COMECA("detalhe · título comprido aparece inteiro, sem reticências");

    const char *longo =
        "Reunião de alinhamento com o time de hardware sobre a revisão "
        "da PCB e o cronograma de homologação";

    nota_com(longo, TIPO_EVENTO, 0, "", "");

    vista_nota_t v;
    vista_nota(&e, &v);

    ESPERA_TEXTO(v.tl, longo);

    // O desenho mede o título inteiro.
    static uint8_t bits[(TELA_L + 7) / 8 * TELA_A];
    bitmap_t bm;
    bitmap_liga(&bm, bits, TELA_L, TELA_A);

    ESPERA(tela_nota_titulo_linhas(&bm, &v) >= 3);

    TERMINA();
}

// A fala desce no próprio item, que já rola (era outra tela).
void t_a_fala_original_desce_no_proprio_item(void)
{
    COMECA("o detalhe mostra a fala rolando, sem saltar de tela");

    nota_com("ideia do painel da dock", TIPO_ANOTACAO, 23,
             "então, tava pensando aqui que o painel da dock talvez não "
             "devesse mostrar o próximo compromisso", "");

    // O vínculo com a gravação.
    snprintf(e.aberto.nota, sizeof e.aberto.nota, "%s", "nt:a1");

    vista_nota_t v;
    vista_nota(&e, &v);

    ESPERA(v.tem_fala);
    ESPERA_CONTEM(v.transcricao, "painel da dock");

    // Nenhum botão leva a outra tela.
    for (int i = 0; i < v.n_botoes; i++)
        ESPERA_SEM(v.botoes[i].texto, "fala original");

    TERMINA();
}
