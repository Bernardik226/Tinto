// nucleo/tipos.h — os tipos que atravessam o sistema inteiro.
// PURO: nada daqui inclui ESP-IDF. Compila com gcc.
#ifndef NUCLEO_TIPOS_H
#define NUCLEO_TIPOS_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// A frase crua de uma fala, em bytes UTF-8 (~45 s). O servidor corta no
// mesmo teto, sem partir um acento.
#define FALA_MAX 640

// ── erros ────────────────────────────────────────────────────────────
// Um enum, sempre retornado; valor sai por out-param. esp_err_t não cruza o hal.
typedef enum {
    OK = 0,
    ERR_SEM_CARTAO,
    ERR_ARQUIVO,
    ERR_FORMATO,
    ERR_SOMENTE_LEITURA,
    ERR_CHEIO,
    ERR_REDE,
    ERR_TIMEOUT,
    ERR_NAO_PAREADO,
    ERR_QUOTA,
    ERR_VERSAO_EXIGIDA,
    ERR_INTERNO,

    // A pasta não existe: um dia sem nada marcado. Código próprio para não
    // cair no balde de erro de disco — absorver aquele balde faria um cartão
    // morrendo parecer agenda vazia.
    ERR_SEM_PASTA,

    // A cópia chegou pela metade: o arquivo existe, e por isso não pode virar
    // obra.
    ERR_DADO_INCOMPLETO,
} erro_t;

// RN-A1: a tela mostra palavras, nunca código. A tradução mora só aqui.
const char *erro_texto(erro_t e);

// ── data ─────────────────────────────────────────────────────────────
typedef struct {
    int16_t ano;
    int8_t  mes;   // 1-12
    int8_t  dia;   // 1-31
} data_t;

bool data_valida(data_t d);
bool data_igual(data_t a, data_t b);
int  data_compara(data_t a, data_t b);   // <0, 0, >0

// ── tipos de item ────────────────────────────────────────────────────
// TIPO_NADA é o estado normal de captura nova.
typedef enum {
    TIPO_NADA = 0,
    TIPO_ANOTACAO,
    TIPO_TAREFA,
    TIPO_LISTA,
    TIPO_EVENTO,
} tipo_t;

// ── origem ───────────────────────────────────────────────────────────
// O campo `o` do contrato.
typedef enum {
    ORIGEM_AQUI = 0,   // nasceu no aparelho
    ORIGEM_GOOGLE,     // veio de fora — filete grosso à esquerda
} origem_t;

// ── item ─────────────────────────────────────────────────────────────
// O tamanho mora no tipo: nada de char* para buffer alheio.
typedef struct {
    char     id[40];
    // 128: o detalhe mostra o título inteiro, e o corte no backend acontece no
    // mesmo teto.
    char     titulo[128];
    char     hora[6];      // "14:00" — vazio se não tem hora
    char     fim[6];       // "15:00" — vazio se não se sabe onde acaba

    // O `l` do contrato. 96: o detalhe é onde se vai para ver o endereço
    // inteiro. Acima disso trunca (RN-B7) — recusar o item seria pior.
    char     local[96];

    // No Google, dia inteiro usa `start.date` com `end.date` EXCLUSIVO (14 → 15).
    // Sem o campo, "o dia todo" e "sem hora" virariam o mesmo JSON.
    bool     dia_inteiro;

    // É uma ocorrência de algo que se repete: apagar aqui apaga aquele dia, e a
    // tela precisa poder dizer isso.
    bool     repete;

    // A regra da rotina ("s:1:135|u=20261130|x=0915"), lida por `nucleo/rotina.h`;
    // vazia no que acontece uma vez. O `dia` do item é a âncora: a primeira
    // ocorrência que ainda vale.
    char     regra[48];

    tipo_t   tipo;
    origem_t origem;
    bool     feita;

    // Quando foi concluída (ano 0 = aberta). `dia` é onde ela nasceu e não muda
    // (RN-26); a Agenda e o calendário mostram o feito pelo dia em que fechou.
    data_t   feita_em;

    data_t   vence;        // data agendada; ano 0 = sem data
    data_t   prazo;        // limite de entrega; ano 0 = sem prazo
    // RN-24: campo escrito à mão trava, e a IA só escreve no que está no padrão.
    // A trava mora no meta porque quem a consulta é o backend.
    bool     titulo_manual;

    int16_t  dur_s;        // duração do áudio, 0 = não tem/não sabe

    // O `n` e o `k` do contrato: tamanho da LISTA e quantos feitos. O Tinto não
    // abre lista item a item; estes dois números bastam.
    int16_t  lista_n;
    int16_t  lista_k;

    // O nome da agenda do Google onde o evento mora ("Trabalho"). Nome, não id:
    // o id é um e-mail ilegível. Vazio quando não se sabe — a linha some em vez
    // de inventar. Só evento: a lista da tarefa viaja no `local`.
    char     agenda[32];

    // De que nota (fala) este item veio; vazio = veio do Google. É de onde o
    // detalhe tira a transcrição.
    char     nota[16];

    // O meta existe e não se interpreta (versão futura ou arquivo corrompido).
    // O item aparece com marcador: sumir calado seria nota perdida.
    bool     defeito;

    // RN-26 + RN-34: o item mora na pasta do dia em que foi falado. O caso de
    // uso precisa desse dia para gravar no lugar certo.
    data_t   dia;
} item_t;

// ── o que uma fala virou ─────────────────────────────────────────────
// RN-18: uma frase pode gerar várias coisas. Três é o que cabe na tela com a
// frase crua embaixo, que é o que permite conferir o que a IA ouviu.
#define RESULTADOS_MAX 3

typedef enum {
    RES_CRIOU = 0,   // nasceu uma coisa nova
    RES_EDITOU,      // mexeu no que já existia — é esta que mais precisa de desfazer
    RES_ANOTOU,      // não achou comando, e virou anotação (RN-15)
    RES_APAGOU,      // "cancela o dentista" — some daqui e do Google
} res_verbo_t;

typedef struct {
    res_verbo_t verbo;
    item_t      item;

    // ── o item como ele estava ───────────────────────────────────────────
    // Editar e apagar por voz mexem no que existe, e conferir é ver o que MUDA.
    // Só o que a tela mostra, não um `item_t` inteiro. `antes_dia` é a PASTA onde
    // ele mora (RN-26): sem ela, confirmar escreveria no dia errado.
    bool   tem_antes;
    data_t antes_dia;
    data_t antes_vence;
    char   antes_hora[8];
    char   antes_titulo[64];
} resultado_t;

// ── o que ocupa o cartão ─────────────────────────────────────────────
// Em KiB: bytes estouram o uint32_t num cartão grande.
typedef struct {
    uint32_t itens_kb;     // notas, meta e áudio — o que a pessoa falou
    uint32_t acervo_kb;
    uint32_t sistema_kb;   // perfil, config, log, recibos
} uso_cartao_t;

// ── uma agenda do Google ─────────────────────────────────────────────
// A curadoria impede que feriados e aniversários afoguem a Agenda.
#define AGENDAS_MAX 12

// A versão do firmware, num lugar só (Sobre, Atualizar, comparação com o servidor).
#define TINTO_VERSAO "1.0.0"

typedef struct {
    char    nome[32];
    bool    ligada;
    int16_t por_ano;   // quantos eventos ela traz por ano, 0 = não se sabe
} agenda_t;

// ── uma rede sem fio ─────────────────────────────────────────────────
// Aqui e não em estado.h porque o hal devolve a lista. 33 bytes: o SSID tem
// 32 e ninguém garante o terminador.
#define REDES_MAX 8

typedef struct {
    char    nome[33];
    int8_t  forca;   // 0-100, como o resto do sistema — nunca dBm na tela
    bool    salva;   // já tem senha guardada no cartão

    // Sem senha (`WIFI_AUTH_OPEN`): conecta direto, sem passar pelo teclado.
    bool    aberta;
} rede_wifi_t;

// ── a credencial deste aparelho (/TINTO/sistema/nuvem.json) ─────────
//
//   servidor  — com quem falar
//   device_id — quem ele é: o MAC do eFuse
//   prova     — que ele é ele: nasce no primeiro boot e nunca sai; sem ela,
//               saber o MAC bastaria para pegar o token
//   token     — o que autoriza cada chamada
//
// Prova e token nunca aparecem na tela. Um dump do flash revela no máximo o
// acesso a ESTE serviço, nunca a conta Google.
typedef struct {
    char servidor[80];
    char device_id[20];    // "AA:BB:CC:44:55:66"
    char prova[33];        // 16 bytes do hal, em hex
    char token[48];

    // De quem é o que está no cartão. O cache sobrevive a desconectar, mas é de
    // uma pessoa: outra conta neste Tinto não pode ver a agenda da anterior.
    char conta[64];
} nuvem_cred_t;

// ── o texto de um item ───────────────────────────────────────────────
// Transcrição crua em `texto.txt`, saída da IA em `proc.json`: reprocessar não
// paga a transcrição de novo. Fora do item_t: carrega um por vez, quando a
// nota abre.
typedef struct {
    char resumo[288];       // o que aquilo significa — vem da IA
    char transcricao[FALA_MAX];  // o que você disse, com os "né" e as repetições
    bool tem_resumo;        // false = ainda não passou pela IA
} texto_t;

// Vista GRANDE que vive um quadro: estática (a pilha da task APP não aguenta,
// e já deu boot loop) e, na placa, na PSRAM — não faz DMA nem precisa
// disputar a RAM interna.
#ifdef ESP_PLATFORM
#define VISTA_TRANSITORIA __attribute__((section(".ext_ram.bss")))
#else
#define VISTA_TRANSITORIA
#endif

#endif
