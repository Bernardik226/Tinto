#include "conta.h"
#include "campos.h"
#include <stdio.h>
#include <string.h>

// O que a linha sob o cursor FAZ, casado pelo destino e não pela posição:
// a lista muda de tamanho com o estado.
conta_acao_t vista_conta_acao(const estado_t *e)
{
    vista_cartao_t v;
    vista_conta(e, &v);

    // Cursor solto é o ícone de sair, no canto do card.
    if (e->cursor < 0) return v.tem_sair ? CONTA_DESCONECTAR : CONTA_NADA;
    if (e->cursor >= v.n_dest) return CONTA_NADA;

    const char *t = v.dest[e->cursor].titulo;
    if (strstr(t, "Conexão"))     return CONTA_WIFI;
    if (strstr(t, "Google") || strstr(t, "Aplicativo"))
                                      return CONTA_VINCULAR;
    if (strstr(t, "Proprietário")) return CONTA_DONO;
    if (strstr(t, "Uso de voz"))  return CONTA_VOZ;
    if (strstr(t, "Sincronizar")) return CONTA_SINCRONIZACAO;
    return CONTA_NADA;
}

// ── Sincronização ────────────────────────────────────────────────────
// O card responde "como está" (a última sincronia é fato, e o cursor não
// chega nele); a lista tem só escolhas reais.
void vista_sincronizacao(const estado_t *e, vista_cartao_t *out)
{
    cartao_comeca(e, out, "Sincronização");
    out->icone   = ICO_SINCRONIZA;
    snprintf(out->kicker, sizeof out->kicker, "%s", "estado atual");
    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK conta");
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK trocar");

    if (e->nome[0] == '\0') {
        snprintf(out->nome, sizeof out->nome, "%s", "Sem conta");
        snprintf(out->corpo, sizeof out->corpo, "%s",
                 "Conecte uma conta em Ajustes > Minha conta para escolher "
                 "o que entra na Agenda.");
        return;
    }

    // ── SEM REDE ─────────────────────────────────────────────────────────
    // O catálogo salvo continua à vista; o que a rede tira é poder MEXER, por
    // isso some a lista e não o card.
    if (e->rede != REDE_LIGADA) {
        snprintf(out->nome, sizeof out->nome, "%s", "Sem internet");
        cartao_fato(out, "Conta", e->nome);
        snprintf(out->corpo, sizeof out->corpo, "%s",
                 "O que já desceu continua na Agenda. Ligar ou desligar "
                 "uma agenda é um gesto que precisa de conexão.");
        cartao_destino(out, ICO_CAT_CONEXAO2, "Abrir Conexão", "Escolher uma rede",
                "");
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK conexão");
        return;
    }

    // ── BUSCANDO ─────────────────────────────────────────────────────────
    // O mesmo card, com os pontinhos.
    if (e->agendas_buscando || (e->n_agendas == 0 && !e->agendas_pedidas)) {
        snprintf(out->nome, sizeof out->nome, "%s", "Buscando agendas");
        out->pontos = (int)((e->agora_ms / 1000u) % 4u);
        cartao_fato(out, "Conta", e->nome);
        if (e->wifi_atual[0]) cartao_fato(out, "Rede", e->wifi_atual);
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "");
        return;
    }

    // ── o servidor não respondeu ─────────────────────────────────────
    if (e->n_agendas == 0) {
        snprintf(out->nome, sizeof out->nome, "%s", "Não consegui atualizar");
        cartao_fato(out, "Conta", e->nome);
        snprintf(out->corpo, sizeof out->corpo, "%s",
                 "O Google não respondeu. O que já está salvo continua "
                 "valendo na Agenda.");
        cartao_destino(out, ICO_SINCRONIZA, "Tentar novamente", "Buscar o catálogo",
                "");
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK tentar");
        return;
    }

    // ── o catálogo carregado ─────────────────────────────────────────
    snprintf(out->nome, sizeof out->nome, "%s", "Tudo atualizado");

    int ligadas = 0;
    for (int i = 0; i < e->n_agendas && i < AGENDAS_MAX; i++)
        if (e->agendas[i].ligada) ligadas++;

    char desde[20], quantas[16];
    vista_quando_sincronizou(e, desde, sizeof desde);
    snprintf(quantas, sizeof quantas, "%d de %d", ligadas, e->n_agendas);

    cartao_fato(out, "Conta", e->nome);
    cartao_fato(out, "Última sincronia", desde);
    cartao_fato(out, "Agendas ativas", quantas);

    snprintf(out->secao, sizeof out->secao, "%s", "AGENDAS VISÍVEIS");

    // ── quatro por página ────────────────────────────────────────────────
    // O card não rola (ui/cartao.c) e cabem quatro linhas de UMA linha — por
    // isso não há "ligada" à direita. ▲▼ vira a página; o rodapé diz "2/3".
    int n   = e->n_agendas < AGENDAS_MAX ? e->n_agendas : AGENDAS_MAX;
    int pag = e->cursor > 0 ? e->cursor / AGENDAS_POR_PAGINA : 0;
    int ini = pag * AGENDAS_POR_PAGINA;
    int fim = ini + AGENDAS_POR_PAGINA < n ? ini + AGENDAS_POR_PAGINA : n;
    out->pagina  = pag + 1;
    out->paginas = (n + AGENDAS_POR_PAGINA - 1) / AGENDAS_POR_PAGINA;
    if (out->cursor >= 0) out->cursor -= ini;

    // As que passaram do teto, ditas no rótulo da seção da última página.
    if (e->agendas_fora > 0 && out->pagina == out->paginas)
        snprintf(out->secao, sizeof out->secao, "+%d AGENDAS NÃO COUBERAM",
                 e->agendas_fora);

    for (int i = ini; i < fim; i++) {
        // O interruptor é a caixa, marcada ou não.
        char sub[28] = "";
        if (e->agendas[i].por_ano)
            snprintf(sub, sizeof sub, "%d por ano", e->agendas[i].por_ano);

        // ── a linha que está ESPERANDO ───────────────────────────────────────
        // O interruptor só vira na confirmação do servidor, mas entre o OK e a
        // resposta a linha mostra os pontinhos e a caixa para de afirmar. Duas
        // esperas, a mesma cara: na fila (`agenda_querendo`) ou em voo
        // (`NUVEM_ESCOLHA`, alvo em `agenda_alvo`).
        bool indo = (e->agenda_querendo &&
                     e->agenda_querida == (int8_t)i) ||
                    (e->nuvem_esperando == NUVEM_ESCOLHA &&
                     e->agenda_alvo == (int8_t)i);

        // Indo, os pontinhos no lugar do valor; parada, nada.
        char valor[12] = "";
        if (indo) {
            int n = (int)((e->agora_ms / 300u) % 3u) + 1;
            for (int p = 0; p < n; p++) snprintf(valor + p * 2, 3, "%s", "·");
        }

        cartao_destino(out, indo ? ICO_CAIXA
                          : e->agendas[i].ligada ? ICO_CAIXA_ON : ICO_CAIXA,
                e->agendas[i].nome, sub, valor);
    }
}

int vista_sincronizacao_paradas(const estado_t *e)
{
    vista_cartao_t v;
    vista_sincronizacao(e, &v);
    // Paginada, o cursor anda por todas as agendas, não só as da página.
    if (v.paginas > 1) return e->n_agendas;
    return v.n_dest > 0 ? v.n_dest : 1;
}

void vista_conta(const estado_t *e, vista_cartao_t *out)
{
    cartao_comeca(e, out, "Minha conta");

    // O DONO vale nos quatro estados: mora no cartão, sem depender de rede nem
    // de conta.
    snprintf(out->nome, sizeof out->nome, "%s",
             e->inicio.nome_pendente[0] ? e->inicio.nome_pendente
                                        : "ninguém ainda");

    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK ajustes");

    // ── sem rede, e "sem rede" tem DUAS caras ────────────────────────────
    // A segunda é Wi-Fi conectado e sem internet (portal de café, AP que não
    // roteia): para o rádio é REDE_LIGADA. O sinal é o mesmo X da barra
    // (SINC_ERRO). "Não alcança" não é "foi recusado": 401 e 409 têm tela
    // própria.
    bool sem_internet = e->rede == REDE_LIGADA && e->sinc == SINC_ERRO &&
                        e->recusa == RECUSA_NADA;

    if (e->rede == REDE_DESLIGADA || e->rede == REDE_SEM_SINAL ||
        sem_internet) {
        out->estado = CONTA_SEM_REDE;

        if (sem_internet) {
            // Não adianta procurar rede, ele já está numa: a saída é trocar de rede.
            snprintf(out->kicker, sizeof out->kicker, "%s", "sem internet");
            snprintf(out->corpo, sizeof out->corpo, "%s",
                     "O aparelho está no Wi-Fi, mas não alcança o "
                     "servidor. A rede pode exigir login numa página ou "
                     "estar sem internet.");
            cartao_destino(out, ICO_CAT_CONEXAO, "Abrir Conexão",
                    "Trocar de rede", "");
        } else {
            snprintf(out->kicker, sizeof out->kicker, "%s",
                     "conexão necessária");
            snprintf(out->corpo, sizeof out->corpo, "%s",
                     "O que já está no aparelho continua aqui. Conta, voz e "
                     "agendas só podem ser atualizados com rede.");
            cartao_destino(out, ICO_CAT_CONEXAO, "Abrir Conexão",
                    "Escolher uma rede", "");
        }

        // O nome do dono se troca SEMPRE: é do aparelho, não da conta.
        cartao_destino(out, ICO_CAT_CONTA, "Proprietário", "Editar nome",
                e->inicio.nome_pendente[0] ? e->inicio.nome_pendente : "—");
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK abrir");
        return;
    }

    // ── com rede, sem Google ─────────────────────────────────────────────
    // Diz o que se GANHA: o aparelho funciona inteiro offline.
    if (e->conta_reconectar) {
        out->estado = CONTA_REAUTORIZAR;
        snprintf(out->kicker, sizeof out->kicker, "%s", "reconecte o Google");
        cartao_fato(out, "Google", e->nome);
        cartao_fato(out, "Wi-Fi", e->wifi_atual);
        snprintf(out->corpo, sizeof out->corpo, "%s",
                 "A conta continua vinculada a este Tinto, mas o Google "
                 "pediu uma nova autorização.");
        cartao_destino(out, ICO_CAT_CONTA, "Aplicativo Tinto",
                "Entrar novamente", "");
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK abrir");
        return;
    }

    if (e->nome[0] == '\0') {
        out->estado = CONTA_SEM_GOOGLE;
        snprintf(out->kicker, sizeof out->kicker, "%s", "sem conta Google");
        cartao_fato(out, "Wi-Fi", e->wifi_atual);
        snprintf(out->corpo, sizeof out->corpo, "%s",
                 "Conectar traz a sua agenda e libera falar com o Tinto. "
                 "O resto do aparelho já funciona.");
        cartao_destino(out, ICO_CAT_CONTA, "Conectar conta Google",
                "Agenda e voz", "");

        // E o nome do dono, que se troca sempre.
        cartao_destino(out, ICO_CAT_CONTA, "Proprietário", "Editar nome",
                e->inicio.nome_pendente[0] ? e->inicio.nome_pendente : "—");
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK abrir");
        return;
    }

    // ── conectada ────────────────────────────────────────────────────
    out->estado = CONTA_CONECTADA;
    snprintf(out->kicker, sizeof out->kicker, "%s", "conta conectada");

    char desde[20];
    vista_quando_sincronizou(e, desde, sizeof desde);
    cartao_fato(out, "Google", e->nome);
    cartao_fato(out, "Última sincronia", desde);
    cartao_fato(out, "Wi-Fi", e->wifi_atual);

    snprintf(out->secao, sizeof out->secao, "%s", "GERENCIAR");

    char voz[16], agendas[16];
    if (e->quota.limite_s > 0)
        snprintf(voz, sizeof voz, "%d min", (int)(e->quota.usados_s / 60));
    else
        snprintf(voz, sizeof voz, "%s", "—");

    int ligadas = 0;
    for (int i = 0; i < e->n_agendas; i++) if (e->agendas[i].ligada) ligadas++;
    if (e->n_agendas > 0)
        snprintf(agendas, sizeof agendas, "%d de %d", ligadas, e->n_agendas);
    else
        snprintf(agendas, sizeof agendas, "%s", "—");

    cartao_destino(out, ICO_CAT_CONTA, "Proprietário", "Editar nome",
            e->inicio.nome_pendente[0] ? e->inicio.nome_pendente : "—");
    // O RELÓGIO: a linha mede minutos gastos no mês.
    cartao_destino(out, ICO_RELOGIO,   "Uso de voz",  "Consumo mensal", voz);
    // O ícone do CICLO: é sobre agenda.
    cartao_destino(out, ICO_SINCRONIZA, "Sincronizar agendas",
            "Selecionar as visíveis", agendas);
    cartao_destino(out, ICO_TINTO, "Aplicativo Tinto",
            "Livros, dispositivos e uso", "QR");

    out->tem_sair = true;
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK abrir");
}

int vista_conta_paradas(const estado_t *e)
{
    vista_cartao_t v;
    vista_conta(e, &v);

    // Só os destinos. A ação do canto vem pelo ▲ (cursor -1), e o card fica
    // fora: o cursor nunca para em informação.
    return v.n_dest > 0 ? v.n_dest : 1;
}
