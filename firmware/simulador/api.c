// firmware/simulador/api.c — a superfície que o visualizador chama por ctypes.
// Opera o MESMO app_t dos testes: o visualizador é só outro produtor de
// eventos.
#include "hal_pc.h"
#include "nucleo/inicializacao.h"
#include "app/app.h"
#include "dado/acervo.h"
#include "uso/acervo.h"
#include "uso/uso.h"
#include "dado/cartao.h"
#include "dado/indice.h"
#include "nucleo/data.h"
#include "vista/teclado.h"
#include <string.h>
#include <stdio.h>

static app_t       ap;
static const hal_t *hal;
static int          n_postos;

void tinto_liga(void)
{
    hal = pc_liga();
    n_postos = 0;
    app_liga(&ap, hal);
    app_passo(&ap);
}

// tecla: usa os valores de entrada_t. ms > 0 = segurado.
void tinto_evento(int tecla, int ms)
{
    if (tecla > 0) {
        if (ms > 0) pc_segura((entrada_t)tecla, ms);
        else        pc_botao((entrada_t)tecla);
    }
    app_passo(&ap);
}

// RN-A3: sem cartão é a única condição que impede tudo. Tirar o cartão
// REINICIA o aparelho, como o próprio aviso pede.
void tinto_sem_cartao(int sem)
{
    pc_sem_cartao(sem != 0);
    app_liga(&ap, hal);
    app_passo(&ap);
}

void tinto_tick(void)
{
    pc_tick();
    pc_avanca_ms(1000);
    app_passo(&ap);
}

void tinto_docar(int docado)
{
    pc_docar(docado != 0);
    app_passo(&ap);
}

const unsigned char *tinto_tela(int *l, int *a)
{
    return pc_tela(l, a);
}

int tinto_largura(void) { int l = 0, a = 0; pc_tela(&l, &a); return l; }
int tinto_altura (void) { int l = 0, a = 0; pc_tela(&l, &a); return a; }

// Diagnóstico, para o rodapé da janela.
int tinto_cursor(void)       { return ap.estado.cursor; }
int tinto_profundidade(void) { return ap.estado.profundidade; }
int tinto_eventos(void)      { return (int)ap.estado.eventos_vistos; }
int tinto_docado(void)       { return ap.estado.docado ? 1 : 0; }
int tinto_tela_id(void)      { return (int)ap.estado.pilha[ap.estado.profundidade]; }

// Quantas agendas a conta tem além das que couberam.
void tinto_agendas_fora(int n)
{
    ap.estado.agendas_fora = (int8_t)n;
    ap.precisa_desenhar = true;
}

// Uma agenda da conta, à mão: na placa ela só chega pelo pull.
void tinto_agenda(const char *nome, int ligada, int por_ano)
{
    if (ap.estado.n_agendas >= AGENDAS_MAX) return;
    agenda_t *a = &ap.estado.agendas[ap.estado.n_agendas++];
    snprintf(a->nome, sizeof a->nome, "%s", nome ? nome : "");
    a->ligada  = ligada != 0;
    a->por_ano = (int16_t)por_ano;
    ap.estado.agendas_pedidas = true;
    ap.estado.agendas_buscando = false;
    ap.precisa_desenhar = true;
}

void tinto_nuvem_demora(int sim) { pc_nuvem_demora(sim != 0); }

void tinto_rede(int ligada)
{
    ap.estado.rede = ligada ? REDE_LIGADA : REDE_DESLIGADA;
}

// De quem é o aparelho ("TINTO DE <nome>" na Home).
void tinto_dono(const char *nome)
{
    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", nome ? nome : "");
    ap.precisa_desenhar = true;
    app_desenha(&ap);
}

// A quota, à mão: na placa ela só chega pelo pull (RN-52).
void tinto_quota(int usados_min, int limite_min, int dias_pra_virar)
{
    ap.estado.quota.usados_s = usados_min * 60;
    ap.estado.quota.limite_s = limite_min * 60;
    ap.estado.quota.dias_pra_virar = (int16_t)dias_pra_virar;
    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", "Convidado");
}

// Por que a última tentativa caiu: 0 nenhuma, 1 senha, 2 sem resposta.
// Separa as duas telas de erro da Conexão.
void tinto_wifi_falha(int qual)
{
    ap.estado.wifi_falha = (wifi_falha_t)qual;
    ap.precisa_desenhar = true;
}

// A tela bloqueante, na etapa pedida.
void tinto_preparando(int etapa)
{
    ap.estado.inicio.fase  = INICIO_PREPARANDO;
    ap.estado.inicio.etapa = (int8_t)etapa;
    ap.precisa_desenhar = true;
}

// O teclado da SENHA, com a rede alvo e o que já foi digitado.
void tinto_senha_de(const char *rede, const char *digitado)
{
    snprintf(ap.estado.wifi_alvo, sizeof ap.estado.wifi_alvo, "%s",
             rede ? rede : "");
    snprintf(ap.estado.digitando, sizeof ap.estado.digitando, "%s",
             digitado ? digitado : "");
    ap.estado.teclado_contexto = TECLADO_WIFI;
    ap.estado.profundidade = 1;
    ap.estado.pilha[1] = TELA_TECLADO;
    ap.precisa_desenhar = true;
}

void tinto_wifi_lista(int sim) { ap.estado.wifi_lista = sim != 0; }

// A rede GUARDADA no cartão (não a conectada): é ela que faz aparecer o
// "Esquecer esta rede".
void tinto_wifi_salva(const char *nome)
{
    snprintf(ap.estado.wifi_salva, sizeof ap.estado.wifi_salva, "%s",
             nome ? nome : "");
}

void tinto_wifi(int estado, int forca, const char *rede, const char *ip)
{
    ap.estado.rede       = (rede_t)estado;
    ap.estado.wifi_forca = (int8_t)forca;
    snprintf(ap.estado.wifi_atual, sizeof ap.estado.wifi_atual, "%s",
             rede ? rede : "");
    snprintf(ap.estado.wifi_alvo, sizeof ap.estado.wifi_alvo, "%s",
             rede ? rede : "");
    snprintf(ap.estado.wifi_ip, sizeof ap.estado.wifi_ip, "%s", ip ? ip : "");
    ap.precisa_desenhar = true;
}

void tinto_rede_no_ar(const char *nome, int forca, int salva)
{
    if (ap.estado.n_redes >= REDES_MAX) return;
    rede_wifi_t *r = &ap.estado.redes[ap.estado.n_redes++];
    memset(r, 0, sizeof *r);
    snprintf(r->nome, sizeof r->nome, "%s", nome ? nome : "");
    r->forca = (int8_t)forca;
    r->salva = salva != 0;
    ap.estado.wifi_procurando = false;
}

void tinto_conta(const char *nome, const char *codigo)
{
    snprintf(ap.estado.nome, sizeof ap.estado.nome, "%s", nome ? nome : "");
    snprintf(ap.estado.codigo, sizeof ap.estado.codigo, "%s",
             codigo ? codigo : "");
}

void tinto_espaco(int usado_mb, int total_mb, int itens_mb)
{
    ap.estado.espaco_usado_kb = (uint32_t)usado_mb * 1024u;
    ap.estado.espaco_total_kb = (uint32_t)total_mb * 1024u;
    ap.estado.uso.itens_kb    = (uint32_t)itens_mb * 1024u;
    ap.estado.uso.sistema_kb  = 2048u;
}

// A tela a mostrar, direto: as telas novas moram fundo demais para navegar
// até elas em cada prova.
void tinto_abre_tela(int id)
{
    ap.estado.pilha[1] = (tela_id)id;
    ap.estado.profundidade = 1;
    ap.estado.cursor = 0;
    ap.precisa_desenhar = true;
}

// Em qual agenda do Google o evento mora: o detalhe mostra o NOME, que na
// placa só chega pelo pull.
void tinto_poe_agenda(int indice, const char *nome)
{
    if (!ap.estado.itens_validos) app_desenha(&ap);
    indice--;
    if (indice < 0 || indice >= ap.estado.n_itens) return;

    item_t it = ap.estado.itens[indice];
    snprintf(it.agenda, sizeof it.agenda, "%s", nome ? nome : "");
    (void)cartao_grava_item(hal, it.dia, &it);

    ap.estado.itens_validos = false;
    app_desenha(&ap);
}

void tinto_poe_evento(int indice, const char *fim, const char *local,
                      int dia_inteiro)
{
    if (!ap.estado.itens_validos) app_desenha(&ap);
    indice--;                       // as cenas contam a partir de 1
    if (indice < 0 || indice >= ap.estado.n_itens) return;

    item_t it = ap.estado.itens[indice];
    snprintf(it.fim,   sizeof it.fim,   "%s", fim   ? fim   : "");
    snprintf(it.local, sizeof it.local, "%s", local ? local : "");
    it.dia_inteiro = dia_inteiro != 0;
    (void)cartao_grava_item(hal, it.dia, &it);

    ap.estado.itens_validos = false;
    app_desenha(&ap);
}

// O pior caso: N resultados, tudo comprido. Prova onde a caixa para de
// crescer.
void tinto_propoe_n(int n)
{
    resultado_t rs[RESULTADOS_MAX];
    memset(rs, 0, sizeof rs);
    const char *T[] = { "Reunião de alinhamento com o time de hardware",
                        "comprar pasta térmica e fita kapton na loja",
                        "Revisão de arquitetura do expansor de portas" };
    const int TP[] = { TIPO_EVENTO, TIPO_TAREFA, TIPO_EVENTO };

    if (n > RESULTADOS_MAX) n = RESULTADOS_MAX;
    for (int i = 0; i < n; i++) {
        rs[i].verbo = RES_CRIOU;
        snprintf(rs[i].item.id,     sizeof rs[i].item.id,     "prop-%d", i);
        snprintf(rs[i].item.titulo, sizeof rs[i].item.titulo, "%s", T[i]);
        rs[i].item.tipo  = (tipo_t)TP[i];
        rs[i].item.dia   = ap.estado.hoje;
        rs[i].item.vence = ap.estado.hoje;
        if (TP[i] == TIPO_EVENTO) {
            snprintf(rs[i].item.hora,  sizeof rs[i].item.hora,  "1%d:00", i + 3);
            snprintf(rs[i].item.fim,   sizeof rs[i].item.fim,   "1%d:30", i + 4);
            snprintf(rs[i].item.local, sizeof rs[i].item.local, "%s",
                     "Rua Bahia, 210 · Funcionarios, Belo Horizonte");
        }
    }
    (void)uso_propor_resultado(&ap.estado, rs, n,
        "marca reunião de alinhamento com o time de hardware às três, "
        "me lembra de comprar pasta térmica e fita kapton, e põe a revisão "
        "de arquitetura do expansor pra sexta de manhã");
    ap.estado.overlay      = OVERLAY_NADA;
    ap.estado.profundidade = 1;
    ap.estado.pilha[1]     = TELA_CONFERIR;
    app_desenha(&ap);
}

// A proposta que a IA devolveria.
void tinto_propoe(const char *titulo, int tipo, const char *hora,
                  int ano, int mes, int dia, const char *falou)
{
    resultado_t r;
    memset(&r, 0, sizeof r);
    r.verbo = tipo == TIPO_ANOTACAO ? RES_ANOTOU : RES_CRIOU;
    snprintf(r.item.id,     sizeof r.item.id,     "%s", "prop-1");
    snprintf(r.item.titulo, sizeof r.item.titulo, "%s", titulo);
    snprintf(r.item.hora,   sizeof r.item.hora,   "%s", hora ? hora : "");
    r.item.tipo  = (tipo_t)tipo;
    r.item.dia   = ap.estado.hoje;
    r.item.vence = (data_t){ (int16_t)ano, (int8_t)mes, (int8_t)dia };
    if (hora && hora[0]) {
        // Uma hora de fim plausível, para a confirmação mostrar a faixa inteira.
        int h = 0, m = 0;
        sscanf(hora, "%d:%d", &h, &m);
        snprintf(r.item.fim, sizeof r.item.fim, "%02d:%02d", (h + 1) % 24, m);
    }

    (void)uso_propor_resultado(&ap.estado, &r, 1, falou);
    ap.estado.overlay      = OVERLAY_NADA;
    ap.estado.profundidade = 1;
    ap.estado.pilha[1]     = TELA_CONFERIR;
    app_desenha(&ap);
}

// Põe um arquivo no cartão de mentira.
void tinto_poe_arquivo(const char *caminho, const char *conteudo)
{
    pc_poe_arquivo(caminho, conteudo);
}

// ── montar cenário sem recompilar ───────────────────────────────────
// Grava NO CARTÃO, não no cache: o primeiro gesto que invalidasse o cache
// esvaziaria a tela. Cenário passa pelo mesmo caminho da placa.
void tinto_poe_item(const char *titulo, int tipo, const char *hora,
                    int ano, int mes, int dia, int feita, int de_fora)
{
    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,     sizeof it.id,     "%02d-item", ++n_postos);
    snprintf(it.titulo, sizeof it.titulo, "%s", titulo ? titulo : "");
    snprintf(it.hora,   sizeof it.hora,   "%s", hora ? hora : "");
    it.tipo   = (tipo_t)tipo;
    it.vence  = (data_t){ (int16_t)ano, (int8_t)mes, (int8_t)dia };
    it.feita  = feita != 0;
    it.origem = de_fora ? ORIGEM_GOOGLE : ORIGEM_AQUI;
    it.dia    = ap.estado.hoje;

    // No cartão E no índice, como o pull faz: sem o índice as tarefas não
    // entram nas pendentes.
    (void)cartao_grava_item(hal, it.dia, &it);
    indice_poe(&it, it.dia);
    estado_invalida_cartao(&ap.estado);
    ap.precisa_desenhar = true;
}

// ── uma OBRA no acervo ──────────────────────────────────────────────
// `estado` é o `obra_estado_t`; `lido` é o offset, a posição canônica da
// leitura.
void tinto_poe_obra(const char *id, const char *titulo, const char *autor,
                    int tipo, int estado, int tamanho, int lido,
                    const char *texto)
{
    obra_t o;
    memset(&o, 0, sizeof o);
    snprintf(o.id,     sizeof o.id,     "%s", id ? id : "ob:x");
    snprintf(o.titulo, sizeof o.titulo, "%s", titulo ? titulo : "");
    snprintf(o.autor,  sizeof o.autor,  "%s", autor ? autor : "");
    o.tipo   = (obra_tipo_t)tipo;
    o.estado = (obra_estado_t)estado;
    o.tamanho = tamanho;
    o.baixado = estado == 2 ? tamanho : (estado == 1 ? tamanho * 64 / 100 : 0);
    o.offset_texto = lido;
    if (lido > 0) o.aberta_em = ap.estado.hoje;

    (void)acervo_grava_meta(hal, &o);
    if (texto && *texto) (void)acervo_grava_texto(hal, o.id, texto, true);

    (void)uso_carregar_acervo(hal, &ap.estado);
    ap.precisa_desenhar = true;
}

void tinto_poe_item_em(int ano, int mes, int dia, const char *titulo,
                       int tipo, int de_fora)
{
    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,     sizeof it.id,     "%02d%02d-outro", dia, ++n_postos);
    snprintf(it.titulo, sizeof it.titulo, "%s", titulo ? titulo : "");
    it.tipo   = (tipo_t)tipo;
    it.origem = de_fora ? ORIGEM_GOOGLE : ORIGEM_AQUI;
    it.dia    = (data_t){ (int16_t)ano, (int8_t)mes, (int8_t)dia };
    (void)cartao_grava_item(hal, it.dia, &it);
    indice_poe(&it, it.dia);
    estado_invalida_cartao(&ap.estado);
}

// A duração do áudio, que só existe em item que nasceu de uma fala.
void tinto_poe_dur(int indice, int segundos)
{
    char id[16];
    snprintf(id, sizeof id, "%02d-item", indice);

    item_t it;
    if (cartao_le_item(hal, ap.estado.hoje, id, &it) != OK) return;
    it.dur_s = (int16_t)segundos;
    (void)cartao_grava_item(hal, ap.estado.hoje, &it);
    ap.estado.itens_validos = false;
}

// Transcrição crua e o que a IA fez dela, nos arquivos onde moram de
// verdade (SISTEMA §7).
void tinto_poe_texto(int indice, const char *transcricao, const char *resumo)
{
    char cam[128], conteudo[640], d[DATA_TEXTO];
    data_para_texto(ap.estado.hoje, d, sizeof d);

    // O VÍNCULO primeiro: item transcrito sem `nota` é um estado que o cartão
    // real nunca produz (a tela o leria como "veio do Google"). E antes dos
    // arquivos, porque gravar o meta reescreve a pasta do item.
    {
        item_t it;
        char id[40];
        snprintf(id, sizeof id, "%02d-item", indice);
        if (cartao_le_item(hal, ap.estado.hoje, id, &it) == OK && !it.nota[0]) {
            snprintf(it.nota, sizeof it.nota, "%s", id);
            (void)cartao_grava_item(hal, ap.estado.hoje, &it);
        }
    }

    snprintf(cam, sizeof cam, "/TINTO/itens/%s/%02d-item/texto.txt",
             d, indice);
    (void)hal->escrever(cam, transcricao ? transcricao : "");

    if (resumo && *resumo) {
        snprintf(cam, sizeof cam, "/TINTO/itens/%s/%02d-item/proc.json",
                 d, indice);
        snprintf(conteudo, sizeof conteudo, "{\"r\":\"%s\"}", resumo);
        (void)hal->escrever(cam, conteudo);
    }

    estado_invalida_cartao(&ap.estado);
}

// Abre um item direto, pela posição no cache: o caminho por botão até a
// nota passa pelas capturas, que nascem depois dela.
void tinto_abre(int indice)
{
    if (!ap.estado.itens_validos)
        app_desenha(&ap);
    if (indice < 0 || indice >= ap.estado.n_itens) return;

    if (uso_abrir_item(hal, &ap.estado, &ap.estado.itens[indice]) != OK) return;
    ap.estado.profundidade = 1;
    ap.estado.pilha[1]     = TELA_NOTA;
    app_desenha(&ap);
}

void tinto_relogio(int ano, int mes, int dia, int hora, int minuto)
{
    ap.estado.hoje      = (data_t){ (int16_t)ano, (int8_t)mes, (int8_t)dia };
    ap.estado.dia_visto = ap.estado.hoje;   // acertar o relógio não muda o dia visto
    ap.estado.hora      = (int8_t)hora;
    ap.estado.minuto    = (int8_t)minuto;

    // O hal do PC também: quem lê o relógio é o hal, e um EV_TICK devolveria a
    // data antiga.
    pc_relogio(ap.estado.hoje, hora, minuto);

    ap.estado.itens_validos = false;
    ap.precisa_desenhar     = true;
}

// tinto_liga() já entrega cartão limpo; isto só derruba a bandeira.
void tinto_limpa_itens(void)
{
    ap.estado.itens_validos = false;
    ap.precisa_desenhar     = true;
}

void tinto_redesenha(void) { app_desenha(&ap); }

// ── a inicialização ────────────────────────────────────────────────
// Só para MONTAR cena: o simulador pede um cenário e o app decide a fase,
// como na placa.
void tinto_memoria_virgem(void)
{
    pc_memoria_virgem();
    app_liga(&ap, hal);
    app_desenha(&ap);
}

void tinto_memoria_estado(int estado)
{
    pc_memoria_estado((memoria_estado_t)estado);
    app_liga(&ap, hal);
    app_desenha(&ap);
}

void tinto_memoria_danificada(void)
{
    pc_memoria_virgem();
    hal->criar_diretorio("/TINTO");
    pc_poe_arquivo("/TINTO/nao-apague.txt", "trabalho de alguem");
    app_liga(&ap, hal);
    app_desenha(&ap);
}

void tinto_pula_para_data_hora(void)
{
    pc_memoria_virgem();
    // Os diretórios ANTES do arquivo: `formato.json` sem `/TINTO/sistema`
    // deixava a árvore pela metade, e o diagnóstico lia "MEMÓRIA".
    hal->criar_diretorio("/TINTO");
    hal->criar_diretorio("/TINTO/itens");
    hal->criar_diretorio("/TINTO/acervo");
    hal->criar_diretorio("/TINTO/entrada");
    hal->criar_diretorio("/TINTO/sistema");
    pc_poe_arquivo("/TINTO/sistema/formato.json", "{\"v\":1}");
    pc_poe_arquivo("/TINTO/sistema/perfil.json",
        "{\"v\":1,\"proprietario_id\":\"0123456789abcdef0123456789abcdef\","
        "\"nome\":\"Usuário\",\"onboarding_v\":0,\"relogio_v\":0}");
    app_liga(&ap, hal);
    app_desenha(&ap);
}

void tinto_pula_para_conclusao(void)
{
    tinto_pula_para_data_hora();
    pc_poe_arquivo("/TINTO/sistema/perfil.json",
        "{\"v\":1,\"proprietario_id\":\"0123456789abcdef0123456789abcdef\","
        "\"nome\":\"Usuário\",\"onboarding_v\":0,\"relogio_v\":1}");
    app_liga(&ap, hal);
    app_desenha(&ap);
}

// A fase do primeiro uso, direto: as telas de rede e conta dependem de
// Wi-Fi de verdade.
void tinto_fase_inicio(int fase)
{
    ap.estado.inicio.fase = (inicio_fase_t)fase;
    snprintf(ap.estado.meu_id, sizeof ap.estado.meu_id, "%s",
             "AA:BB:CC:11:22:33");
    ap.precisa_desenhar = true;
    app_desenha(&ap);
}

// O aparelho já se registrou no servidor? Decide qual tela de conta o
// primeiro uso mostra.
void tinto_tem_token(int sim)
{
    ap.estado.tem_token = sim != 0;
    ap.precisa_desenhar = true;
    app_desenha(&ap);
}

// Uma anotação no cartão, como a voz a deixaria.
void tinto_anota(int ano, int mes, int dia, const char *hora,
                 const char *titulo, const char *falou)
{
    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id, sizeof it.id, "a:%04d%02d%02d%c%c%c%c", ano, mes, dia,
             hora[0], hora[1], hora[3], hora[4]);
    snprintf(it.titulo, sizeof it.titulo, "%s", titulo);
    snprintf(it.hora, sizeof it.hora, "%s", hora);
    it.tipo   = TIPO_ANOTACAO;
    it.origem = ORIGEM_AQUI;
    it.dia    = (data_t){ (int16_t)ano, (int8_t)mes, (int8_t)dia };
    if (falou && falou[0]) {
        snprintf(it.nota, sizeof it.nota, "nt:%02d%c%c%c%c", dia, hora[0],
                 hora[1], hora[3], hora[4]);
        (void)cartao_grava_transcricao(hal, it.nota, falou);
    }
    (void)cartao_grava_item(hal, it.dia, &it);
}

// Abre a lista de anotações na primeira página, como o Acervo a abre.
void tinto_abre_anotacoes(void)
{
    ap.estado.pilha[1] = TELA_ANOTACOES;
    ap.estado.profundidade = 1;
    ap.estado.cursor = 0;
    ap.estado.anotacoes_pagina = 0;
    (void)uso_carregar_anotacoes(hal, &ap.estado);
    ap.precisa_desenhar = true;
}

// O servidor responde ao que está em voo (o gesto, o pull).
void tinto_nuvem_responde(const char *json)
{
    pc_nuvem_responde(json);
    pc_nuvem_entrega();
    app_passo(&ap);
}
