#include "menu.h"
#include "campos.h"
#include "../nucleo/data.h"
#include <stdio.h>
#include <string.h>

// Para onde a ÚLTIMA linha posta leva (evita um parâmetro a mais em vinte
// chamadas que não levam a lugar nenhum).
void vista_menu_leva(vista_menu_t *o, tela_id destino)
{
    if (o->n > 0) o->destino[o->n - 1] = destino;
}

void vista_menu_poe(vista_menu_t *o, const char *secao, icone_id ico,
                    const char *texto, const char *valor)
{
    if (o->n >= MENU_MAX) return;
    o->destino[o->n] = TELA_QUANTAS;   // não leva a lugar nenhum
    snprintf(o->secao[o->n], sizeof o->secao[0], "%s", secao ? secao : "");
    o->linhas[o->n].icone = ico;
    snprintf(o->linhas[o->n].texto, sizeof o->linhas[0].texto, "%s", texto);
    snprintf(o->linhas[o->n].valor, sizeof o->linhas[0].valor, "%s",
             valor ? valor : "");
    o->n++;
}

// Quanto uma linha ocupa, em LINHAS SIMPLES: a que segue um rótulo de seção
// custa duas, e a legenda pesa. Contar errado fazia linhas sumirem sem
// página nem contador.
static int peso(const vista_menu_t *o, int i)
{
    return 1 + (o->secao[i][0] ? 1 : 0) + (o->sub[i][0] ? 1 : 0);
}

void vista_menu_rola(vista_menu_t *o, int cabem)
{
    // A NOTA come espaço da lista (três linhas de miúda = duas de corpo); sem o
    // desconto ela não era desenhada.
    if (o->nota[0]) cabem -= 2;

    if (cabem < 1) cabem = 1;

    // Cursor -1 = tela sem seleção: não força zero. E o cursor não NASCE numa
    // linha que não se aperta (o OK abriria a ação errada): procura adiante,
    // depois atrás; sem nenhuma, a tela é de leitura.
    if (o->cursor >= 0 && o->cursor < o->n && o->linhas[o->cursor].so_leitura) {
        int achou = -1;
        for (int i = o->cursor + 1; i < o->n && achou < 0; i++)
            if (!o->linhas[i].so_leitura) achou = i;
        for (int i = o->cursor - 1; i >= 0 && achou < 0; i--)
            if (!o->linhas[i].so_leitura) achou = i;
        o->cursor = achou;
    }

    int cursor = o->cursor < 0 ? 0 : o->cursor;

    // ── PÁGINAS, não janela que desliza ─────────────────────────────────
    // Andar uma linha por vez deixava o texto meia linha acima do próprio
    // fantasma num painel diferencial. Em páginas, cada linha cai sempre no
    // mesmo lugar, e o que não coube está inteiro na próxima.
    o->primeira = 0;
    o->pagina   = 1;
    o->paginas  = 1;

    int inicio = 0, usado_pag = 0;
    for (int i = 0; i < o->n; i++) {
        int p = peso(o, i);

        // Não cabe no que sobrou: a página fecha aqui e a próxima começa NELA.
        if (usado_pag && usado_pag + p > cabem) {
            o->paginas++;
            inicio    = i;
            usado_pag = 0;
        }
        usado_pag += p;

        if (i == cursor) {
            o->primeira = inicio;
            o->pagina   = o->paginas;
        }
    }

    // O fim da página sai da MESMA conta que o começo; deixar a UI decidir
    // pintava a primeira linha da página seguinte aqui também.
    int usado = 0, ultima = o->primeira;
    for (int i = o->primeira; i < o->n; i++) {
        int p = peso(o, i);
        if (usado && usado + p > cabem) break;
        usado += p;
        ultima = i + 1;
    }
    o->ultima = ultima;

}

static void cabeca(const estado_t *e, vista_menu_t *o, const char *titulo)
{
    memset(o, 0, sizeof *o);
    snprintf(o->titulo, sizeof o->titulo, "%s", titulo);
    vista_hora_da_barra(e, o->hora, sizeof o->hora);
    o->bateria = e->bateria;
    o->wifi = vista_wifi_da_barra(e);
    o->sinc = vista_sinc_da_barra(e);
    o->cursor  = e->travado ? -1 : e->cursor;
}

void vista_menu(const estado_t *e, int cabem, vista_menu_t *out)
{
    cabeca(e, out, "Menu");

    if (e->pilha[e->profundidade] == TELA_LEITOR) {
        static const char *tamanhos[] = {"Pequeno", "Médio", "Grande"};
        static const char *familias[] = {"DejaVu Serif", "PT Sans", "FreeMono",
                                         "Literata", "Atkinson", "Inter", "Source Serif 4"};
        static const char *alinha[] = {"Justificado", "Esquerda", "Centro", "Direita"};
        static const char *rodapes[] = {"Porcentagem e páginas", "Barra e porcentagem", "Somente páginas"};
        int tam = e->leitor.letra, fam = e->leitor.familia;
        int ali = e->leitor.alinhamento;
        if (tam < 0 || tam > 2) tam = 1;
        if (fam < 0 || fam > 13) fam = 0;
        if (ali < 0 || ali > 3) ali = 0;
        vista_menu_poe(out, "LEITURA", ICO_LEITOR_TAMANHO, "Tamanho", tamanhos[tam]);
        vista_menu_poe(out, "", ICO_LEITOR_FONTE, "Fonte", familias[fam % 7]);
        vista_menu_poe(out, "", ICO_LEITOR_FONTE, "Peso", fam >= 7 ? "Forte" : "Normal");
        vista_menu_poe(out, "", ICO_LEITOR_ALINHA, "Alinhamento", alinha[ali]);
        int rodape = e->obra_aberta.rodape;
        if (rodape < 0 || rodape > 2) rodape = 0;
        vista_menu_poe(out, "", ICO_RELOGIO, "Progresso", rodapes[rodape]);
        vista_menu_poe(out, "OBRA", ICO_LIXO, "Descartar deste Tinto", "");
        snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK fechar");
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK escolher");
        vista_menu_rola(out, cabem);
        return;
    }

    if (e->pilha[e->profundidade] == TELA_OBRA) {
        const obra_t *o = &e->obra_aberta;
        vista_menu_poe(out, "OBRA", ICO_ENTRA,
                       o->offset_texto > 0 ? "Continuar leitura" : "Começar leitura", "");
        if (o->offset_texto > 0)
            vista_menu_poe(out, "", ICO_LIVRO_PEQUENO, "Ler desde o início", "");
        vista_menu_poe(out, "", ICO_VISTO,
                       o->concluida ? "Marcar como não lido" : "Marcar como concluído", "");
        vista_menu_poe(out, "", ICO_LIXO, "Descartar deste Tinto", "");
        snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK fechar");
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK escolher");
        vista_menu_rola(out, cabem);
        return;
    }

    // No Acervo, o MENU organiza a estante e dá as Anotações; sem Calendário.
    if (e->pilha[e->profundidade] == TELA_ACERVO) {
        int f = (int)e->acervo_filtro;
        vista_menu_poe(out, "MOSTRAR", ICO_LISTA, "Todos", "");
        vista_menu_poe(out, "", ICO_RELOGIO, "Em leitura", "");
        vista_menu_poe(out, "", ICO_LIVRO_PEQUENO, "Livros", "");
        vista_menu_poe(out, "", ICO_DOCUMENTO_PEQUENO, "Documentos", "");
        vista_menu_poe(out, "", ICO_VISTO, "Concluídos", "");
        if (f >= 0 && f < 5) out->linhas[f].marcado = true;
        vista_menu_poe(out, "ACERVO", ICO_NOTA, "Anotações", "");
        snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK fechar");
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK aplicar");
        vista_menu_rola(out, cabem);
        return;
    }


    char mes[20];
    snprintf(mes, sizeof mes, "%s", data_mes_longo(e->hoje));

    // ── a gaveta da Agenda: três destinos ───────────────────────────────
    // Ação contextual da tela aberta; a navegação é da Home. O Calendário fica:
    // é outro jeito de olhar a Agenda. Sem "Tudo por fazer": seria uma segunda
    // lista das mesmas tarefas.
    vista_menu_poe(out, "", ICO_EVENTO, "Calendário", mes);

    // Anotações moram no Acervo: duas portas para o mesmo lugar é navegação
    // concorrente.

    snprintf(out->nota, sizeof out->nota, "%s",
             "Segure BACK pra voltar pra Home de qualquer lugar");

    // FECHAR, não voltar: a gaveta é pop-over e devolve a tela de trás.
    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK fechar");
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK abrir");
    vista_menu_rola(out, cabem);
}


// ── a raiz de Ajustes: CINCO destinos ───────────────────────────────
// Índice do sistema: cada linha é uma categoria com uma descrição curta, e
// os ajustes miúdos ficam dentro dela. Nenhum valor na raiz.
void vista_ajustes(const estado_t *e, int cabem, vista_menu_t *out)
{
    (void)e;
    cabeca(e, out, "Ajustes");

    struct { icone_id ico; const char *titulo; const char *sobre;
             tela_id destino; } CAT[] = {
        { ICO_CAT_CONTA,   "Minha conta",
                           "Google, consumo e este Tinto",   TELA_CONTA },
        // Wi-Fi e estado da rede; a sincronização de agendas mora em Minha conta.
        { ICO_CAT_CONEXAO2, "Conexão",
                           "Wi-Fi e estado da rede",     TELA_WIFI },
        // "Hora e tela": não há ajuste de energia, e o nome cabe na barra (o título
        // da tela bate com a linha que a abriu).
        { ICO_CAT_TELA,    "Hora e tela",
                           "Relógio e bloqueio",         TELA_APARENCIA },
        { ICO_CAT_CAMERA,  "Câmera e voz",
                           "A fala e a captura",         TELA_SOM },
        // A categoria abre o HUB do aparelho: quanto cabe, como atualizar e o que
        // ele promete por escrito.
        { ICO_CAT_CARTAO,  "Sobre o Tinto",
                           "Espaço, versão e updates",   TELA_SOBRE },
    };

    for (size_t i = 0; i < sizeof CAT / sizeof CAT[0]; i++) {
        // A descrição vai no VALOR, ao lado: uma terceira linha por categoria
        // empurraria a quinta para fora.
        vista_menu_poe(out, "", CAT[i].ico, CAT[i].titulo, "");
        snprintf(out->sub[out->n - 1], sizeof out->sub[0], "%s", CAT[i].sobre);
        vista_menu_leva(out, CAT[i].destino);
    }

    // O rodapé nomeia o destino do BACK, não só "voltar".
    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK início");
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK abrir");
    vista_menu_rola(out, cabem);
}

// ── Aparência e Som ─────────────────────────────────────────────────
// Estados atuais no card, passivo; abaixo, só valores alteráveis.
static void cabeca_cartao(const estado_t *e, vista_cartao_t *o,
                          const char *titulo, const char *kicker)
{
    cartao_comeca(e, o, titulo);
    snprintf(o->kicker, sizeof o->kicker, "%s", kicker);
    snprintf(o->rodape_esq, sizeof o->rodape_esq, "%s", "BACK ajustes");
}

void vista_aparencia(const estado_t *e, vista_cartao_t *out)
{
    cabeca_cartao(e, out, "Hora e tela", "relógio e bloqueio");
    out->icone = ICO_CAT_TELA;
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK alterar");

    // O nome do card é o VEREDITO: dá para confiar no relógio?
    bool rede = e->config.valor[AJUSTE_HORA_REDE] != 0;
    snprintf(out->nome, sizeof out->nome, "%s",
             rede ? "Hora certa pela rede" : "Hora ajustada à mão");

    int blq = e->config.valor[AJUSTE_BLOQUEAR_MIN];   // 0 = nunca
    char trava[14];
    if (blq) snprintf(trava, sizeof trava, "após %d min", blq);
    else     snprintf(trava, sizeof trava, "%s", "desligado");

    // Os três fatos são os três assuntos do título, nessa ordem.
    cartao_fato(out, "Formato", e->config.valor[AJUSTE_HORA24] ? "24 h" : "12 h");
    cartao_fato(out, "Fonte", rede ? "pela rede" : "à mão");
    cartao_fato(out, "Bloqueio", trava);

    snprintf(out->secao, sizeof out->secao, "%s", "AJUSTES");

    // O ícone segue o valor: 12 h analógico, 24 h digital. Desempata também
    // de "Data e hora", que usa outro relógio.
    cartao_destino(out, e->config.valor[AJUSTE_HORA24] ? ICO_RELOGIO_24 : ICO_RELOGIO,
          "Formato da hora", "Como o relógio aparece",
          e->config.valor[AJUSTE_HORA24] ? "24 h" : "12 h");
    cartao_destino(out, ICO_RELOGIO, "Data e hora", "Ajuste automático",
          e->config.valor[AJUSTE_HORA_REDE] ? "pela rede" : "à mão");

    char so_min[10];
    if (blq) snprintf(so_min, sizeof so_min, "%d min", blq);
    else     snprintf(so_min, sizeof so_min, "%s", "nunca");
    // Trava, não dorme: a tela de bloqueio continua mostrando o dia.
    cartao_destino(out, ICO_CADEADO, "Bloquear após", "Tempo sem uso", so_min);
}

void vista_som(const estado_t *e, vista_cartao_t *out)
{
    // Só o gesto de falar por enquanto (não há saída de áudio). Os ajustes de
    // captura da câmera entram aqui.
    cabeca_cartao(e, out, "Câmera e voz", "captura");
    out->icone = ICO_CAT_CAMERA;
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK alterar");

    snprintf(out->nome, sizeof out->nome, "%s", "Câmera e voz");
    cartao_fato(out, "Microfone", "interno");
    cartao_fato(out, "Câmera", "desconectada");

    snprintf(out->secao, sizeof out->secao, "%s", "CONTROLES");

    cartao_destino(out, ICO_MIC, "Botão de voz", "Segurar ou um toque",
          e->config.valor[AJUSTE_VOZ_SEGURAR] ? "segurar" : "um toque");
}

int vista_aparencia_paradas(const estado_t *e)
{
    vista_cartao_t v;
    vista_aparencia(e, &v);
    return v.n_dest > 0 ? v.n_dest : 1;
}

int vista_som_paradas(const estado_t *e)
{
    vista_cartao_t v;
    vista_som(e, &v);
    return v.n_dest > 0 ? v.n_dest : 1;
}
