// vista/nota.h — T-16, o detalhe do item. PURO.
// Resumo em cima e TRANSCRIÇÃO CRUA embaixo (RN-28): o que torna verificável
// que a IA estruturou e não inventou.
#ifndef VISTA_NOTA_H
#define VISTA_NOTA_H

#include "../nucleo/estado.h"
#include "../tela/icones.h"
#include "linhas.h"

// O que o botão FAZ, não o que diz: despachar pela primeira letra levou
// "Desmarcar" para a gravação.
typedef enum { BOTAO_CONCLUIR, BOTAO_REABRIR } botao_id;

// Três é o teto de uma fala; o quarto lugar é folga para o corte não
// parecer perda.
#define NOTA_CRIOU_MAX 4

typedef struct {
    char titulo[20];
    char hora[9];
    int  bateria;
    // -1 esconde o ícone do rádio: num aparelho que funciona offline, ícone
    // permanente seria lembrete do que não faz falta.
    int  wifi;
    icone_id sinc;   // o que a sincronização faz, como forma

    // O título INTEIRO: quebra em linhas e vira página, nunca reticências.
    char tl[128];
    // De onde veio: "Criado por voz no Tinto" ou "Adicionado no Google". Não se
    // sincroniza, e não é o mesmo que `onde` (cadê no celular).
    char origem[32];

    // ── o kicker e os campos ─────────────────────────────────────────────
    // "EVENTO · GOOGLE AGENDA" acima do título; depois pares rótulo/valor. Quais
    // campos existem depende do TIPO, e quem decide é a vista.
    char kicker[40];

    // `forte`: não há negrito em 12 px; o valor principal sobe um degrau de
    // corpo. 12 e 64 bytes: "CONCLUÍDA" tem 10 em UTF-8, e o endereço vai
    // inteiro (a tela quebra a linha).
    struct { char rotulo[12]; char valor[64]; bool forte; } campos[6];
    // Seis: a tarefa fechada tem ESTADO, LISTA, VENCE, CRIADA e CONCLUÍDA.
    int  n_campos;

    // Há gravação por trás? Libera "Ver fala original"; item do Google não
    // inventa uma.
    bool tem_fala;

    // A caixinha da origem: "G" se ADICIONADO no Google, ponto se nasceu de
    // uma fala. Não é onde o item mora.
    bool origem_google;

    // ── as ações do rodapé ───────────────────────────────────────────────
    // No máximo duas, concluir primeiro. Concluída, não se oferece de novo: o
    // estado já diz.
    struct { char texto[24]; bool principal; botao_id faz; } botoes[2];
    int  n_botoes;
    int  botao_foco;

    // O cursor CRU: as primeiras paradas são de leitura (rolagem), e quantas
    // são é geometria da tela.
    int  cursor_bruto;

    char onde[40];         // "Agenda · Tinto" — RN-4D, em linha própria
    char quando[32];      // "12 ago · 07:12 · 0:23"
    char faixa[20];       // "14:00 – 15:00" · "o dia todo" — só evento
    char local[48];       // onde é. Vazio quando não veio

    bool tem_resumo;
    char resumo[288];
    char transcricao[FALA_MAX];
    char aviso[40];       // "por transcrever" quando a IA ainda não passou

    // ── tudo o que ESTA fala criou ───────────────────────────────────────
    // Uma frase vira até três coisas; mostrá-las juntas deixa ver que nasceram
    // da mesma fala. 80 bytes: "Tarefa · " + título de 63.
    struct { char oque[80]; char estado[14]; } criou[NOTA_CRIOU_MAX];
    int  n_criou;

    // As ações em pop-over, as mesmas para todo tipo: agem sobre o que está
    // atrás, e a tela de trás é a prova do que vai mudar.
    bool acoes_abertas;
    linha_acao_t acoes[5];
    int  n_acoes;
    char sem_acoes[40];   // por que não há nenhuma, quando não há
    int  cursor_acao;

    char rodape_esq[22], rodape_dir[22];
} vista_nota_t;

void vista_nota(const estado_t *e, vista_nota_t *out);

#endif
