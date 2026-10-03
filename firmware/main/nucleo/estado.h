// nucleo/estado.h — o estado do aparelho, uma struct só.
//
// Só app/ escreve nele, e só a task APP o toca: por isso não há mutex. O tipo
// mora em nucleo/ porque vista/ e uso/ precisam dele e ficam abaixo de app/.
#ifndef NUCLEO_ESTADO_H
#define NUCLEO_ESTADO_H

#include "inicializacao.h"

#include "tipos.h"
#include "acervo.h"
#include "leitor.h"
#include "../jogos/xadrez.h"

typedef enum {
    XZ_PAG_INICIO = 0,
    XZ_PAG_MODOS,
    XZ_PAG_PREPARAR_MAQUINA,
    XZ_PAG_PREPARAR_LOCAL,
    XZ_PAG_OPCOES,
    XZ_PAG_SUBSTITUIR,
    XZ_PAG_TABULEIRO,
    XZ_PAG_PROMOCAO,
    XZ_PAG_MENU,
    XZ_PAG_HISTORICO,
    XZ_PAG_EMPATE,
    XZ_PAG_SAIR,
    XZ_PAG_ABANDONAR,
    XZ_PAG_FINAL,
    XZ_PAG_RESULTADO,
    XZ_PAG_ERRO
} xadrez_pagina_t;

typedef struct {
    xadrez_pos_t posicao;
    uint8_t pagina, cursor, menu_cursor, modo, cor_humana, cor_baixo;
    uint8_t dificuldade, orientacao, retorno;
    int8_t origem;
    bool tem_salva, salvamento_falhou, maquina_pensando, mostrar_ajuda;
    xadrez_mov_t promocao;
    xadrez_estado_t resultado;
    uint16_t n_lances, historico_total, historico_desloc;
    /* Pontos em metades: vitória soma 2; empate soma 1 para cada lado. */
    uint16_t placar_a2, placar_b2;
    xadrez_hist_item_t historico[4];
} xadrez_app_t;

// Todo array tem teto declarado; estourar é caso a tratar (contar e avisar).
#define ITENS_MAX 32

// Anotações por PÁGINA da lista; as demais ficam no índice.
#define ANOTACOES_MAX 6

// Quantos dias para trás a busca de anotações varre.
#define ANOTACOES_DIAS 14

// Tarefas abertas: cache de um INTERVALO, não do dia visto — tarefa aberta
// continua aberta amanhã. Mesmo teto do cache do dia, que é alimentado
// daqui: um pote menor cortaria o que a Agenda mostra.
#define PENDENTES_MAX  32


typedef enum {
    TELA_AGENDA = 0,
    TELA_AJUSTES,
    TELA_BLOQUEADA,
    TELA_NOTA,
    TELA_DIA,      // os compromissos de um dia
    TELA_CALENDARIO,
    TELA_SOBRE,
    TELA_DOCK,            // o aparelho deitado, carregando
    TELA_FALA,     // T-26, quanto ainda dá pra falar este mês
    TELA_CONFERIR,
    TELA_TECLADO,

    // A data de um item, mudada à mão: dias prontos (amanhã, depois...).
    // Não é Data e hora, que acerta o relógio do aparelho.
    TELA_QUANDO,

    // A hora de um item.
    TELA_HORARIO,

    // A grade do mês para ESCOLHER um dia. Rota própria: aqui o OK carimba a
    // data no item; no calendário o OK abre o dia.
    TELA_ESCOLHER_DIA,

    // O mostrador: qualquer hora, par da grade do mês.
    TELA_ESCOLHER_HORA,
    TELA_APARENCIA,

    TELA_SOM,
    TELA_WIFI,
    TELA_ARMAZENAMENTO,
    TELA_CONTA,
    TELA_SINCRONIZACAO,
    TELA_DATA_HORA,
    TELA_ANOTACOES,   // o que não é evento nem tarefa, por recência

    // O QR e o código de seis letras, um passo atrás de Minha Conta: quem
    // volta lá para ver a quota não precisa ver o QR todo dia.
    TELA_VINCULAR,

    // O lançador de quatro áreas; a Agenda é o primeiro cartão.
    TELA_HOME,

    TELA_ACERVO,           // a biblioteca: lendo agora + as demais obras
    TELA_OBRA,             // ficha editorial da obra
    TELA_LEITOR,           // o texto, paginado
    TELA_LEITURA_AJUSTES,  // fonte e tamanho, o painel que o MENU sobe
    TELA_JOGOS,            // só Xadrez na primeira versão
    TELA_XADREZ,

    // O que a fala DE FATO criou, depois da resposta do servidor. O Conferir
    // é a proposta, antes.
    TELA_RESULTADO,

    // O primeiro uso, enquanto `inicio.fase` não é INICIO_HOME. Mora no chão
    // da pilha: Wi-Fi e Conta empilham por cima e o BACK volta para ele.
    TELA_INICIO,

    // Sentinela: tela nova entra ANTES desta linha. Derivar o limite de uma
    // tela nomeada já deixou o QR fora de todo laço que varre o catálogo.
    TELA_QUANTAS,
} tela_id;

// O overlay se sobrepõe à tela e não gasta nível da pilha: fechar não é
// voltar, é tirar a gaveta da frente.
typedef enum {
    OVERLAY_NADA = 0,
    OVERLAY_MENU,
    OVERLAY_DESCARTAR,   // T-13, a única confirmação do gesto de falar
    OVERLAY_DESCARTAR_OBRA, // remove somente a cópia local de uma obra

    // Apagar uma ROTINA apaga a série inteira no Google, não só o dia.
    OVERLAY_APAGAR_ROTINA,

    // Desconectar pergunta antes: desfazer custa o consentimento de novo.
    OVERLAY_DESCONECTAR,

    // Esquecer a rede salva pergunta antes: apaga a senha do cartão.
    OVERLAY_ESQUECER,
    OVERLAY_GRAVANDO,    // fora da home: o gravador por cima da tela
    OVERLAY_ACOES,       // o que dá pra fazer com o item que está atrás
} overlay_id;

typedef enum { REDE_DESLIGADA = 0, REDE_CONECTANDO, REDE_LIGADA, REDE_SEM_SINAL } rede_t;

// Por que a última tentativa falhou: senha errada se resolve digitando de
// novo; rede muda, não.
typedef enum {
    WIFI_FALHA_NENHUMA = 0,
    WIFI_FALHA_SENHA,         // o AP recusou a credencial
    WIFI_FALHA_SEM_RESPOSTA,  // não achou o AP, ou ele não completou
} wifi_falha_t;

// GRAV_ESTRUTURANDO: depois do OK a fala sobe, e o aparelho fica travado
// até a resposta (ver `vista/gravador.h`).
typedef enum {
    GRAV_PARADA = 0,
    GRAV_GRAVANDO,
    GRAV_PAUSADA,
    GRAV_ESTRUTURANDO,
} grav_fase_t;

// SINC_ESPERANDO_REDE: há gesto gravado no cartão e não há rede. Não é erro
// nem envio — sobe quando der.
typedef enum { SINC_OCIOSO = 0, SINC_ENVIANDO, SINC_RECEBENDO, SINC_ERRO,
               SINC_ESPERANDO_REDE } sinc_t;

#define GRAV_MAX_TRECHOS 8

typedef struct {
    grav_fase_t fase;
    int32_t     ms;
    int8_t      trechos;

    // Duração de cada trecho, para a barra desenhá-los proporcionais.
    int16_t     trecho_s[GRAV_MAX_TRECHOS];

    // `desde_ms`: quando o áudio subiu. É o prazo que destrava o aparelho se
    // a resposta nunca vier; `sem_resposta` marca que ele venceu (a fala
    // continua no cartão).
    uint32_t    desde_ms;
    bool        sem_resposta;

    // O áudio nem saiu (a rede caiu antes). Outra frase que `sem_resposta`:
    // aqui não há resposta a esperar.
    bool        nao_enviou;

    // O id desta fala, fixado ao COMEÇAR. Recalcular do relógio cruzava o
    // minuto no meio da fala e o envio procurava um WAV que não existia.
    char        id[40];

    // O servidor respondeu sem ação nenhuma: não achou comando na fala.
    bool        nada_entendido;

    // Por que a gravação não começou (OK quando começou): sem cartão, cartão
    // cheio, microfone mudo. Sem isto o ● parecia só não ter pegado.
    erro_t      nao_comecou;
} gravacao_t;

// ── os ajustes, que moram no cartão ──────────────────────────────────
// Ajuste é o que a pessoa escolhe; flag é o que se escolhe por ela e vem do
// pull. Enum e não string: chave inexistente não compila.
typedef enum {
    // A persistência é por CHAVE de texto no config.json, não por índice:
    // remover um ajuste não desloca os outros.
    AJUSTE_HORA24 = 0,     // 0 = 12 h, 1 = 24 h
    AJUSTE_BLOQUEAR_MIN,   // minutos sem tocar em nada até a tela travar
    AJUSTE_VOZ_SEGURAR,    // 1 = segurar pra falar, 0 = aperta e solta

    // 1 = a hora vem da rede; 0 = a pessoa ajusta e o RTC conta. Nunca os
    // dois: o sync apagaria o ajuste manual sem avisar.
    AJUSTE_HORA_REDE,

    // Fuso em minutos do UTC (-180 = Brasília). Vem do Google Agenda e fica
    // no cartão para não acordar em UTC sem rede. Zero (UTC) é legítimo.
    AJUSTE_FUSO_MIN,

    AJUSTE_QUANTOS
} ajuste_t;

typedef struct {
    int16_t valor[AJUSTE_QUANTOS];
} config_t;

// Os filtros da Biblioteca. Anotações não são obra: têm tela própria.
typedef enum {
    ACERVO_TODOS = 0,
    ACERVO_EM_LEITURA,
    ACERVO_SO_LIVROS,
    ACERVO_SO_DOCUMENTOS,
    ACERVO_CONCLUIDOS,
} acervo_filtro_t;

#define PILHA_MAX 4

typedef struct {
    int32_t usados_s;
    int32_t limite_s;
    int16_t dias_pra_virar;
} quota_t;

typedef struct {
    // ── onde a pessoa está ──
    data_t  dia_visto;

    // Quatro níveis: o caminho mais fundo é Home → Ajustes → Minha Conta →
    // Conectar. Estourar troca o topo em vez de sumir com o gesto (`empilha`).
    tela_id pilha[PILHA_MAX];
    int8_t  profundidade;

    // O gesto recusado, que vira a faixa do rodapé até o próximo botão.
    // Vazio = nenhum aviso. A recusa acontece NO gesto, não depois.
    char    precisa_rede[28];

    // Por que foi recusado (recusa_t): cada motivo pede outra ação.
    int8_t  recusa;

    // O cartão em foco na Home (0 Agenda · 1 Acervo · 2 Jogos · 3 Ajustes).
    // Não é o `cursor`: `empilha` zera o cursor, e a Home lembra de onde se
    // saiu.
    int8_t  lancador;

    // Falar é gesto global: o resultado da fala não empilha, guarda a pilha
    // inteira e a devolve depois.
    tela_id pilha_antes[PILHA_MAX];
    int8_t  profundidade_antes;
    bool    veio_da_voz;
    int16_t cursor;
    overlay_id overlay;
    int16_t cursor_overlay;   // o cursor da gaveta, separado do da tela

    // ── o aparelho ──

    // Onde o sistema estava quando o vidro travou, para destravar no mesmo
    // lugar. Vetor próprio: `pilha_antes` é da voz, e os dois podem coexistir.
    tela_id pilha_travada[PILHA_MAX];
    int8_t  profundidade_travada;
    int16_t cursor_travado;

    bool    travado;
    bool    aviso_desbloqueio;
    uint32_t aviso_ate_ms;
    data_t  hoje;
    int8_t  hora, minuto;

    // RN-6G: hora que ninguém ajustou e nenhum NTP trouxe não é mostrada.
    bool    hora_confiavel;
    int8_t  bateria;      // 0-100
    bool    docado;
    rede_t  rede;

    // ── o que está acontecendo ──
    gravacao_t gravacao;
    sinc_t     sinc;

    // ── o que veio do servidor, e o device só exibe ──
    char    nome[24];     // o e-mail da conta Google; vazio = não pareado
    bool    conta_reconectar; // pareada, mas a concessão Google venceu

    // O LEMBRETE da conta caída já foi dado neste episódio. O 428 vem em
    // toda resposta, e a faixa voltava por cima de Minha Conta. Avisa só ao
    // perceber, ao ligar e ao entrar em Minha Conta; zera quando a conta
    // volta (`estado_conta_ok`).
    bool conta_avisada;

    // O código de pareamento (T-27a), de uso único. Vazio = ainda não chegou.
    char    codigo[8];

    // Quando o código vence, em ms do aparelho. O prazo vem do servidor
    // (`validade_s`); zero = sem código ou vencido.
    uint32_t codigo_ate_ms;

    // O MAC do eFuse: quem este aparelho é para o servidor.
    char    meu_id[24];

    // Sem token, o ciclo tenta se registrar de novo sozinho.
    bool    tem_token;

    // A colheita inteira já foi pedida nesta sessão? Uma vez por cartão
    // vazio, não por pull: lê o Google todo.
    bool    pediu_tudo;

    // Há itens do Google no cartão? Cacheado porque varrer custava ~700 ms
    // por pull; cai junto com os outros caches.
    bool    google_verificado;
    bool    tem_google;

    // Quando foi a última tentativa de se apresentar.
    uint32_t registro_ms;
    bool     registro_tentado;

    // As agendas da conta (T-32). Fora do cartão de propósito: a lista muda
    // no Google sem avisar, e é pedida ao entrar na tela.
    agenda_t agendas[AGENDAS_MAX];

    // Quantos gestos esperam em `/TINTO/gestos/` (teto `GESTOS_MAX`). A fila
    // mora no cartão: o gesto acontece offline e sobe quando houver rede.
    int8_t   gesto_n;

    // O último botão apertado. Sem toque por AJUSTE_BLOQUEAR_MIN, a tela trava.
    uint32_t ultimo_toque_ms;

    // Último pedido de NTP: um relógio sem confiança não pede a cada tique.
    uint32_t ntp_pedido_ms;

    // Última leitura do sinal. O evento do rádio só vem quando o estado muda,
    // e o "Sinal: forte" congelava no instante da conexão.
    uint32_t wifi_lido_ms;

    // Desde quando tenta conectar. Sem prazo, uma rede que associa e não dá
    // DHCP (portal cativo) deixava "conectando" para sempre.
    uint32_t conectando_desde_ms;

    // A escolha de agenda que espera a linha vagar (o long polling a ocupa
    // quase o tempo todo).
    bool     agenda_querendo;
    int8_t   agenda_querida;      // qual, no índice do catálogo
    bool     agenda_querida_on;

    // "Remarcado", "Hora mudada": faixa curta por cima do item. Num painel de
    // tinta nada anima, e sem ela a pessoa aperta OK de novo.
    char     feito[24];
    uint32_t feito_ms;

    // O nível da pilha para onde "mudar data/hora" volta. Procurar
    // `TELA_NOTA` falha quando abrir o item já trocou o topo.
    int8_t   voltar_nivel;

    // A hora no mostrador, ainda proposta: só toca o item no OK (RN-16).
    int8_t   escolha_h, escolha_m;
    int8_t   escolha_campo;   // 0 = hora, 1 = minuto

    // A obra que está descendo; uma por vez.
    char     obra_baixando[40];
    int32_t  capa_baixada;
    int8_t   capa_etapa;      // 0 cheia, 1 mini da estante, 2 destaque
    bool     acervo_valido;
    uint32_t acervo_ultimo_ms; // catálogo: só pulsa enquanto a Biblioteca está aberta

    acervo_filtro_t acervo_filtro;

    // O que o cartão tem, em RAM: reler a pasta a cada quadro custa meio
    // segundo.
    obra_t   acervo[OBRAS_MAX];
    int8_t   n_acervo;
    bool     acervo_visto[OBRAS_MAX]; // marca transitória da rodada online

    // A obra aberta e os números prontos para o rodapé. A posição canônica é
    // o offset, no `leitor_t`.
    obra_t   obra_aberta;
    char     obra_sinopse[OBRA_SINOPSE];

    // O texto mora num buffer estático do app: centenas de KB não cabem aqui.
    leitor_t leitor;
    const char *leitor_texto;

    char     pagina[LEITOR_PAGINA];
    int      leitura_pct;
    int      pagina_atual, paginas_total;
    bool     leitor_no_fim;
    bool     leitor_na_capa;   // página zero; retomada no meio pula a capa
    char     leitor_erro[40];

    // O WAV que espera a linha vagar: a captura não atropela um gesto em voo.
    char     captura_pendente[80];

    unsigned gesto_seq;          // último número usado
    char     gesto_em_voo[16];   // o arquivo que está subindo, ou vazio

    // Lápides: ids apagados aqui e ainda não no Google. Sem elas o pull
    // ressuscita o item. Caem quando o "apagou" sobe (ou é recusado de vez)
    // e são refeitas no boot a partir da fila.
    char     apagando[8][40];
    int8_t   n_apagando;

    // Desde quando a linha está ocupada. Sem prazo, uma resposta perdida
    // deixava o aparelho mudo até reiniciar.
    uint32_t nuvem_desde_ms;

    // A busca de agendas que espera a vez (o pull segura a linha por até
    // 25 s). Sem guardar o pedido, a tela dizia "Buscando" para sempre.
    bool     agendas_querendo;
    uint32_t agendas_desde_ms;
    int8_t   n_agendas;
    // Quantas a conta tem além das AGENDAS_MAX; a tela diz.
    int8_t   agendas_fora;

    // Já pedida nesta sessão: separa "não tem agenda" de "ainda não perguntei".
    bool     agendas_pedidas;

    // A pergunta está no ar agora. Bool e não `nuvem_esperando`: vista/ não
    // inclui uso/.
    bool     agendas_buscando;

    // O gesto NA LINHA agora, e o dia do item. A resposta traz o id
    // definitivo do Google para o item que nasceu com id provisório (`n:1`);
    // sem isso, a cópia local ficava ao lado da que volta no pull.
    char     gesto_id[40];
    data_t   gesto_dia;

    // A agenda que espera confirmação do servidor, pelo índice: duas escolhas
    // rápidas não trocam de interruptor.
    int8_t   agenda_alvo;
    quota_t quota;

    // ── caches, com bandeira de validade (nunca prazo) ──
    item_t  itens[ITENS_MAX];

    // Quantos itens não couberam no cache: contados e avisados (RN-4B).
    int16_t n_fora;
    int16_t n_itens;
    bool    itens_validos;

    // O servidor disse que o lote tem mais. Enquanto tiver, o cartão recebe e
    // a tela NÃO se redesenha: cada lote era um refresh de tinta.
    bool    pull_tem_mais;
    // Algum pedaço deste lote trouxe ou tirou algo. A tela relê quando o
    // lote acaba, mesmo que o ÚLTIMO pedaço venha vazio.
    bool    pull_mudou;

    // O que a pessoa está olhando fora da janela de três dias: o mês da grade
    // e o dia aberto. Vazios, `uso_olhar` não pergunta nada. São apagados
    // pela RESPOSTA, não pelo envio, para que um pedido perdido saia de novo.
    char    mes_pedido[8];    // "2026-09"
    char    dia_pedido[11];   // "2026-09-27"
    data_t  dia_pedido_data;  // o mesmo dia, para comparar sem parsear

    // Quando a última consulta saiu: o reenvio espera, em vez de sair a cada
    // passo do laço.
    uint32_t olhar_ms;

    // De que dia é a consulta que está em `itens` (ano 0 = nenhuma). Sem
    // isto, `uso_carregar_dia` a apagava no quadro seguinte.
    data_t  dia_consultado;

    // Dia fora da janela e ainda não consultado: a tela não afirma vazio.
    bool    dia_fora_da_janela;

    // O resultado da última fala, para o recibo. Some quando a tela sai (RN-19).
    item_t  ultimo;
    // Vazio até o resultado voltar: separa "estruturando" de "feito".
    resultado_t resultados[RESULTADOS_MAX];
    int8_t      n_resultados;
    char        falou[FALA_MAX];   // a frase crua, pra conferir sem sair da tela
    int8_t  ultimo_trechos;   // em quantos pedaços aquela fala saiu

    // ── o teclado ──
    // Só para renomear título e senha de Wi-Fi; conteúdo não se digita.
    char    digitando[64];
    int8_t  teclado_modo;    // 0 minúsculas · 1 maiúsculas · 2 números
    int8_t  teclado_lin, teclado_col;

    // ── as marcas do calendário ──
    // Que dias do mês têm evento e tarefa. Bit 0 = dia 1. É a união do
    // índice (a janela, vale sem rede) com o que o servidor mandou do mês.
    uint32_t marcas_tarefa;
    uint32_t marcas_evento;

    // As do servidor, separadas porque valem só para o mês que ele respondeu.
    uint32_t marcas_srv_evento;
    uint32_t marcas_srv_tarefa;
    int16_t  marcas_srv_ano;
    int8_t   marcas_srv_mes;
    int16_t  marcas_ano;
    int8_t   marcas_mes;
    bool     marcas_validas;

    // ── o item aberto, e só ele ──
    item_t  aberto;
    texto_t texto;
    bool    aberto_valido;

    // ── o que a pessoa escolheu ──
    config_t config;

    // ── o rádio ──
    // Varrer o ar custa segundos: acontece ao ENTRAR na tela.
    rede_wifi_t redes[REDES_MAX];
    int8_t      n_redes;
    bool        wifi_procurando;
    char        wifi_atual[33];   // vazio = não conectado a nada

    // A rede GUARDADA no cartão, que não é a conectada agora. A linha
    // "esquecer esta rede" só existe quando ela existe.
    char        wifi_salva[33];

    // Qual das duas telas de Wi-Fi está aberta: o estado ou a lista. Juntas,
    // cada rede que chegava empurrava a tela e os parciais se sobrepunham.
    bool        wifi_lista;

    // ── as anotações ──
    // Não vão pro Google (RN-25) e não se acham pelo dia: cache de um
    // intervalo, separado do dia visto.
    item_t   anotacoes[ANOTACOES_MAX];   // a PÁGINA aberta
    int8_t   n_anotacoes;
    bool     anotacoes_validas;
    int16_t  anotacoes_total;            // todas, no cartão
    int16_t  anotacoes_pagina;           // 0 = as mais novas

    // As tarefas abertas (ver PENDENTES_MAX).
    item_t   pendentes[PENDENTES_MAX];
    int8_t   n_pendentes;
    int16_t  n_pendentes_fora;   // quantas não couberam — RN-4B
    bool     pendentes_validas;
    char        wifi_alvo[33];    // a rede que se está tentando conectar

    // Zerado quando uma nova tentativa começa: erro velho não fica ao lado
    // de tentativa nova.
    wifi_falha_t wifi_falha;
    int8_t      wifi_forca;       // 0-100 do AP conectado, pro ícone da barra
    char        wifi_ip[16];      // vazio até o DHCP responder

    // ── a memória, medida ──
    // Em KiB: 32 GB estoura uint32_t em bytes. Zero nos dois = não mediu.
    uint32_t espaco_usado_kb;
    uint32_t espaco_total_kb;

    // Quanto cada parte da árvore ocupa; varrido ao entrar em Armazenamento.
    uso_cartao_t uso;

    // ── a nuvem ──
    // De quem é a resposta que está vindo. Uma pergunta por vez: escrever
    // por cima faz a resposta de um ser lida como a do outro.
    int8_t   nuvem_esperando;

    // O pull tem linha própria (long polling): os gestos não esperam por ele.
    // Quem diz de qual linha é a resposta é `hal->nuvem_linha`.
    bool     pull_esperando;

    // De quem era a ÚLTIMA resposta, decidido por quem a leu. Com duas
    // linhas, perguntar ao estado pendente tratava o delta como a fala.
    int8_t   nuvem_respondeu;
    int8_t   nuvem_em_voo;

    // Quando o último pull deu certo (`agora_ms`; 0 = nunca). Minha Conta
    // mostra "há 2 min".
    uint32_t sinc_ultima_ms;

    // As ações confirmadas estão subindo; o Resultado entra com a última.
    bool     esperando_resultado;
    char     cursor_pull[40];   // de onde continuar o delta
    // Cópia do relógio a cada tick: a vista é pura e não fala com o hal.
    uint32_t agora_ms;
    uint32_t nuvem_ultimo_ms;   // quando foi a última conversa
    uint32_t nuvem_contato_ms;  // quando foi a última que DEU CERTO
    bool     nuvem_ja_falou;    // false = nunca conversou com o servidor

    // ── diagnóstico ──
    // O último erro, para a tela Sobre: sem serial ligado, é o que sobra.
    erro_t   ultimo_erro;
    uint32_t eventos_vistos;

    // ── a fase raiz ──
    // RN-6F: enquanto não for INICIO_HOME, o primeiro uso manda nos botões.
    inicio_t inicio;

    // qual dos três contextos do teclado está aberto
    int8_t   teclado_contexto;

    // Estado local do jogo; as regras moram em jogos/.
    xadrez_app_t xadrez;
} estado_t;

// Invalidar cache tem dois nomes, porque a distinção é real. Esquecer um
// cache já deixou tarefa apagada no Google aparecendo na Agenda.

// Mudou o DIA VISTO, não o cartão. As tarefas abertas não dependem do dia, e
// derrubá-las custaria uma varredura do cartão a cada ◀▶.
static inline void estado_invalida_dia(estado_t *e)
{
    e->itens_validos = false;
}

// Mudou o CARTÃO: tudo o que deriva dele está velho.
static inline void estado_invalida_cartao(estado_t *e)
{
    e->itens_validos     = false;
    e->pendentes_validas = false;
    e->marcas_validas    = false;
    e->google_verificado = false;
    e->anotacoes_validas = false;

    // `aberto_valido` fica: o item que sumiu é tratado por `topo_ficou_orfao`,
    // que desempilha a tela em vez de deixá-la vazia.
}

// A conta voltou a valer: a próxima queda é episódio novo e merece aviso.
static inline void estado_conta_ok(estado_t *e)
{
    e->conta_reconectar = false;
    e->conta_avisada    = false;
}

// Os motivos de uma recusa, e cada um leva a um lugar diferente.
typedef enum {
    RECUSA_NADA = 0,
    RECUSA_REDE,      // sem Wi-Fi          → Ajustes › Wi-Fi
    RECUSA_MINUTOS,   // a quota acabou     → Minha Conta › Voz
    RECUSA_SEM_CONTA, // sem vínculo Google → parear
    RECUSA_CONTA,     // a conta caiu       → Minha Conta
    RECUSA_TOKEN,     // o servidor esqueceu este aparelho → ele se reapresenta

    // O servidor recusou de vez uma ação da fila, que saiu dela. O vidro diz:
    // fila que descarta calada promete o que não cumpre.
    RECUSA_ACAO,
} recusa_t;

// A Conta do primeiro uso espera o servidor por 20 s — o mesmo prazo do
// Wi-Fi. Estourado, a tela vira erro com saída: tentar de novo ou pular.
#define CONTA_ESPERA_MS (20u * 1000u)
static inline bool estado_servidor_sem_resposta(const estado_t *e)
{
    return !e->tem_token && e->inicio.espera_desde_ms &&
           e->agora_ms >= e->inicio.espera_desde_ms &&
           e->agora_ms - e->inicio.espera_desde_ms > CONTA_ESPERA_MS;
}

#endif
