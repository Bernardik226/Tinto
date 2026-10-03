// uso/nuvem.h — os casos de uso que atravessam a rede.
// O `pull` é a ÚNICA via de entrada de dado: um lugar para aplicar mudança e
// invalidar cache. Nada bloqueia (RN-41): estas funções pedem, e
// `uso_nuvem_resposta` aplica quando o hal avisa.
#ifndef USO_NUVEM_H
#define USO_NUVEM_H

#include "../hal/hal.h"
#include "../nucleo/estado.h"

// O que se espera da rede, para a resposta saber a que pergunta responde.
typedef enum {
    NUVEM_NADA = 0,
    NUVEM_PAREAR,      // pedi um código
    NUVEM_PAREADO,     // pergunto se já pareou
    NUVEM_PUSH,        // mandei a fila
    NUVEM_PULL,        // pedi o delta
    NUVEM_CAPTURA,     // mandei o áudio e espero o que a IA entendeu
    NUVEM_GESTO,       // mandei UM gesto e espero o ok do servidor
    NUVEM_REGISTRAR,   // me apresentei ao servidor e espero um token
    NUVEM_DESVINCULAR, // pedi para soltar a conta deste aparelho
    NUVEM_AGENDAS,     // pedi a lista de agendas da conta (T-32)
    NUVEM_ESCOLHA,     // liguei ou desliguei uma, e espero o ok

    // ── o que a pessoa está OLHANDO ──────────────────────────────────────
    // O mês da grade e o dia aberto, na linha do GESTO: quem está com o dedo no
    // botão não espera o long polling.
    NUVEM_OLHAR,

    // ── o acervo ─────────────────────────────────────────────────────────
    // Duas esperas: catálogo chega em ms, texto tem megabytes. Uma só travaria a
    // estante atrás de um livro.
    NUVEM_ACERVO,      // pedi o catálogo da conta
    NUVEM_OBRA,        // pedi o texto de UMA obra
    NUVEM_CAPA,        // pedi o bitmap monocromático da mesma obra
} nuvem_espera_t;

// Manda a fala: Whisper transcreve, Haiku estrutura, voltam as ações. A
// faixa fica em "Estruturando", aparelho travado, até chegar. O áudio inteiro
// sobe uma vez (transcrever por pausa cobrava a quota várias vezes). Sem
// rede nem tenta: a recusa foi no gesto.
erro_t uso_enviar_captura(const hal_t *hal, estado_t *e, const char *wav);

// O gesto que esperava a linha vagar. Sem nenhum, não faz nada.
erro_t uso_gesto_pendente(const hal_t *hal, estado_t *e);

// A captura que esperava a linha vagar: sem isso, uma fala com gesto em voo
// sumia.
erro_t uso_captura_pendente(const hal_t *hal, estado_t *e);

// A fila do cartão relida no boot: quantos esperam, a numeração e os
// apagar pendentes.
erro_t uso_gestos_do_cartao(const hal_t *hal, estado_t *e);

// Apagado aqui e ainda não lá? Enquanto for, o pull não o regrava.
bool uso_esta_apagando(const estado_t *e, const char *id);

// A escolha de agenda que ficou esperando a linha vagar.
erro_t uso_agenda_pendente(const hal_t *hal, estado_t *e);

erro_t uso_enviar_gesto(const hal_t *hal, estado_t *e,
                        const item_t *it, const char *verbo);

// Pede o delta (long polling). Os gestos sobem pela fila, na linha do
// gesto, e as lápides impedem o delta de ressuscitar o que foi apagado.
erro_t uso_sincronizar(const hal_t *hal, estado_t *e);

// ── a consulta: o mês da grade e o dia aberto ────────────────────────
// O aparelho guarda três dias; o mês vem pelas marcas do servidor e um dia
// distante é buscado ao abrir. Nada vai para o cartão. Rota própria, na
// linha do gesto: de carona no pull, o pedido esperava a próxima novidade do
// Google, e o `"dia":[]` de toda resposta zerava a consulta. Sem nada pedido,
// não faz nada.
erro_t uso_olhar(const hal_t *hal, estado_t *e);

// ── T-32 · quais agendas da conta entram ─────────────────────────────
// A curadoria impede feriados e aniversários de afogar a Agenda. A lista é
// pedida ao ENTRAR na tela, não guardada: muda no Google sem avisar.
erro_t uso_agendas(const hal_t *hal, estado_t *e);

// Liga ou desliga UMA agenda, pelo nome (o id é um e-mail de 60
// caracteres; o backend resolve o nome). O interruptor só vira quando o
// servidor confirma.
erro_t uso_escolher_agenda(const hal_t *hal, estado_t *e, int indice,
                           bool ligada);

// Pede o código de pareamento (T-27a).
erro_t uso_parear(const hal_t *hal, estado_t *e);

// Lê a credencial, entrega ao hal e registra se não houver token. Chamada
// quando a rede sobe. Chamar de novo é inofensivo: o servidor devolve o
// mesmo token a quem tem a prova.
erro_t uso_nuvem_ligar(const hal_t *hal, estado_t *e);

// Desvincula a conta: avisa o servidor E limpa aqui, nessa ordem. O que já
// desceu para o cartão FICA.
erro_t uso_desvincular(const hal_t *hal, estado_t *e);

// Aplica o que chegou: OK, ERR_REDE (sem resposta) ou ERR_FORMATO (fora do
// contrato).
erro_t uso_nuvem_resposta(const hal_t *hal, estado_t *e);

#endif
