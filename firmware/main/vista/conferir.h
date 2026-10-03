// vista/conferir.h — o Conferir: a tela que fecha o gesto de falar.
//
//   segura ●  ──▶  gravando          a faixa que sobe do rodapé
//   solta     ──▶  pausado
//   OK        ──▶  "Estruturando"    a MESMA faixa, aparelho travado
//                       │  a resposta chega (ou o prazo estoura e a faixa
//                       ▼  destrava dizendo onde a fala está)
//   ESTA TELA: o que ele entendeu, por resultado, e a frase crua
//     ▸ Marcar na agenda        ← confirma e aplica TUDO
//     ▸ Descartar a gravação    ← o áudio e a sugestão, juntos
//
// Só existe COM resultado: a espera é da faixa. Tela cheia, não pop-over: o
// gesto de falar acabou e a pergunta é sobre o que ele entendeu. O que não
// cabe ROLA — cortar o local ou a frase crua tiraria o que se está
// conferindo.
//
// Por que confirmar em vez de aplicar e oferecer desfazer: nada entra no
// Google da pessoa sem ela ter visto, e quem virou as costas não desfaz. O
// áudio já está no cartão desde o OK (RN-15); em jogo está só a
// interpretação.
#ifndef VISTA_CONFERIR_H
#define VISTA_CONFERIR_H

#include "../nucleo/estado.h"
#include "../tela/icones.h"
#include "linhas.h"

// Uma linha de resultado. RN-4D: o destino é obrigatório.
typedef struct {
    icone_id icone;
    // "EVENTO · Google Agenda": o que é e para onde vai, numa linha.
    char tipo[40];
    char titulo[40];
    // 40: "sex 14 ago · 15:00 – 16:00" dá 29 bytes (· e – são multibyte).
    char quando[40];
    char local[48];      // onde é, quando o evento traz
    char onde[32];       // "Agenda · Tinto"

    // O ANTES ("qui 11 set · 15:00") quando a ação mexe no que existe: conferir
    // é ver o que muda. Vazio quando nasce agora.
    char antes[40];

    // A ação APAGA: título riscado e botão "Apagar" — desfazer por voz é falar
    // tudo de novo.
    bool apagando;
} linha_resultado_t;

typedef struct {
    char titulo[20];
    char hora[9];
    int  bateria;
    // -1 esconde o ícone do rádio: num aparelho que funciona offline, ícone
    // permanente seria lembrete do que não faz falta.
    int  wifi;
    icone_id sinc;   // o que a sincronização faz, como forma

    // "2 trechos · 0:38": a única coisa daqui que não veio da IA.
    char trechos[24];

    // "2 AÇÕES NESTA GRAVAÇÃO": quantas coisas nasceram, antes de rolar.
    char kicker[32];

    // As ações estão EM VOO (o servidor leva perto de um minuto). O OK não faz
    // nada e a tela mostra que trabalha: apertar de novo duplicava o dentista
    // na agenda de verdade.
    bool enviando;
    int  pontos;

    linha_resultado_t res[RESULTADOS_MAX];
    int  n_res;
    char falou[FALA_MAX]; // a frase crua: é o que torna conferível

    // Duas, nesta ordem, e a primeira (não-destrutiva) vem selecionada, como
    // no T-13.
    linha_acao_t decisao[2];
    int  cursor;      // 0 · 1 · -1 quando está lendo em vez de decidindo
    int  cursor_res;  // qual resultado o cursor está lendo, ou -1

    char rodape_esq[22], rodape_dir[22];
} vista_conferir_t;

void vista_conferir(const estado_t *e, vista_conferir_t *out);

// Quantas linhas o cursor visita: os resultados e as duas decisões.
int vista_recibo_linhas(const estado_t *e);

#endif
