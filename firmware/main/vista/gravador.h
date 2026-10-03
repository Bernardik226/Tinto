// vista/gravador.h — T-11 gravando · T-12 pausado · T-13 descartar.
// A pergunta desta tela é "ele está me ouvindo?".
//
// Depois do OK a faixa troca de assunto: "Estruturando", com o aparelho
// travado — a única trava fora do repouso, para a resposta não chegar a uma
// tela que mudou de assunto. Descartar fica para o Conferir. Faixa, não tela
// cheia: a fala foi dita de dentro de outra tela. Há PRAZO: passado ele, a
// faixa destrava e diz que não veio resposta.
#ifndef VISTA_GRAVADOR_H
#define VISTA_GRAVADOR_H

#include "../nucleo/estado.h"
#include "../nucleo/prazos.h"
#include "../tela/icones.h"

// Os prazos moram em nucleo/prazos.h (o hal precisa dos mesmos números).

typedef struct {
    char titulo[20];       // "Gravando" · "Pausado" · "Estruturando"
    char hora[9];
    int  bateria;
    // -1 esconde o ícone do rádio: num aparelho que funciona offline, ícone
    // permanente seria lembrete do que não faz falta.
    int  wifi;
    icone_id sinc;   // o que a sincronização faz, como forma

    char estado[20];       // "Gravando" · "Pausado"
    char tempo[8];         // "0:14" — em 46 px, a única coisa que muda
    bool pausado;          // o medidor fica VAZADO em vez de sumir
    int  trechos;
    char trecho_txt[20];   // "3 trechos · 0:38"

    // O quadro da onda anda com o TEMPO, não com um contador por desenho.
    int  onda;

    // As fatias da barra, em segundos: proporcionais, porque 2 s + 30 s + 2 s
    // não é a sessão de três trechos de 11 s.
    int  fatias[GRAV_MAX_TRECHOS];
    int  n_fatias;

    // ── depois do OK: a fala subindo ─────────────────────────────────────
    // `estruturando` trava e anima; cai quando a resposta chega, ou o prazo
    // estoura e `sem_resposta` fica no lugar. O espaço do `aviso` é reservado
    // desde o primeiro quadro: faixa que muda de altura vira parcial de tela
    // cheia.
    bool estruturando;
    // Qual dos três pontos está aceso, 0-2. Anda com o SEGUNDO, não com o
    // desenho.
    int  pontos;

    bool sem_resposta;

    // Há gravação aberta (gravando ou pausada). `pausado` sozinho também vale
    // nas faixas de falha, onde não há o que continuar.
    bool fase_gravando;

    bool nao_enviou;

    // Respondeu sem achar comando: outro estado que "sem resposta".
    bool nada_entendido;

    // Por que a gravação não começou. OK = começou.
    erro_t nao_comecou;
    char aviso[64];

    // T-13: a confirmação. RN-A2: só para o que destrói, com o "não"
    // pré-selecionado.
    bool confirmando;
    char pergunta[34];
    char perde[40];        // "38 segundos em 3 trechos."
    bool sim_selecionado;

    char rodape_esq[22], rodape_dir[22];
} vista_grav_t;

void vista_gravador(const estado_t *e, vista_grav_t *out);

#endif
