// nucleo/inicializacao.h — estados puros do boot e da memória interna.
#ifndef NUCLEO_INICIALIZACAO_H
#define NUCLEO_INICIALIZACAO_H

#include "tipos.h"

typedef enum {
    MEMORIA_PRONTA = 0,
    MEMORIA_AUSENTE,
    MEMORIA_COMUNICACAO,
    MEMORIA_SEM_FILESYSTEM,
    MEMORIA_CORROMPIDA,
    MEMORIA_SOMENTE_LEITURA,
    MEMORIA_CHEIA,
} memoria_estado_t;

typedef enum {
    CAMINHO_AUSENTE = 0,
    CAMINHO_ARQUIVO,
    CAMINHO_DIRETORIO,
} caminho_tipo_t;

// ── o dono da memória ────────────────────────────────────────────────
// RN-6D: o perfil acompanha o CARTÃO. O id nasce uma vez; trocar o nome não
// troca o dono. 24 é o teto em CARACTERES (RN-B7): em UTF-8 um caractere
// ocupa até 4 bytes, e cortar no byte partiria um acento.
#define NOME_CARACTERES_MAX 24
#define NOME_UTF8_MAX       (NOME_CARACTERES_MAX * 4 + 1)

typedef struct {
    char    proprietario_id[33];   // 16 bytes em hex, e o terminador
    char    nome[NOME_UTF8_MAX];
    uint8_t onboarding_v;          // 0 = primeiro uso não concluiu
    uint8_t relogio_v;             // 0 = a hora nunca foi ajustada

    // O nome mudou aqui e ainda não subiu: ligado, o aparelho manda o dele;
    // desligado, adota o do servidor.
    uint8_t nome_sobe;
} perfil_local_t;

// ── a fase raiz ──────────────────────────────────────────────────────
// RN-6F: enquanto não for INICIO_HOME, o primeiro uso manda nos botões.
typedef enum {
    INICIO_HOME = 0,               // o aparelho é o aparelho
    INICIO_MEMORIA_AUSENTE,
    INICIO_MEMORIA_COMUNICACAO,
    INICIO_MEMORIA_REPARO,         // sem filesystem ou árvore danificada
    // RN-6C: a confirmação destrutiva é tela própria, para apagar exigir dois
    // gestos deliberados.
    INICIO_CONFIRMAR_FORMATAR,
    INICIO_MEMORIA_SOMENTE_LEITURA,
    INICIO_MEMORIA_CHEIA,
    INICIO_FORMATO_FUTURO,
    INICIO_BOAS_VINDAS,
    INICIO_NOME,

    // Wi-Fi e Conta são opcionais: o aparelho funciona offline. Pular o Wi-Fi
    // pula a Conta, que exige rede.
    INICIO_WIFI,
    INICIO_CONTA,
    INICIO_CONFIRMAR_DONO,
    INICIO_DATA_HORA,
    INICIO_CONCLUSAO,
    INICIO_PREPARANDO,             // operação bloqueante em curso
} inicio_fase_t;

// A etapa anterior, para o voltar. Devolve a própria fase na primeira
// etapa e fora do primeiro uso: depois de concluído, o aparelho tem dono
// (RN-6D) e não se volta.
inicio_fase_t inicio_fase_anterior(inicio_fase_t fase);

// Se dá para voltar a partir daqui.
bool inicio_pode_voltar(inicio_fase_t fase);

typedef enum {
    INICIO_CMD_BOOT = 0,
    INICIO_CMD_SALVAR_NOME,
    INICIO_CMD_SALVAR_HORA,
    INICIO_CMD_CONCLUIR,
    INICIO_CMD_FORMATAR,

    // Pular um passo opcional. Não grava nada: a próxima pendência é decidida
    // pelo caso de uso.
    INICIO_CMD_PULAR_WIFI,
    INICIO_CMD_PULAR_CONTA,
} inicio_comando_t;

typedef struct {
    inicio_fase_t    fase;
    memoria_estado_t diagnostico;
    int8_t           cursor;
    bool             formatar_pendente;
    uint32_t         espera_desde_ms; // a Conta esperando o servidor, desde quando
    bool             servidor_visto;  // OK no "Conectado ao servidor"

    // Etapa da operação bloqueante (0 apagando, 1 preparando, 2 reiniciando).
    // Avança sozinha; a tela só mostra que anda.
    int8_t           etapa;

    // A formatação veio de Ajustes, não de um cartão quebrado: o "não, voltar"
    // precisa saber para onde volta.
    bool             formatar_de_ajustes;

    // Passos opcionais já oferecidos nesta sessão. Só em memória: um reboot no
    // meio do primeiro uso volta a oferecer.
    bool             pulou_wifi;
    bool             pulou_conta;
    char             nome_pendente[NOME_UTF8_MAX];
    bool             nome_sobe;   // ver perfil_local_t

    // Os cinco campos da tela de data e hora, e qual deles está ativo.
    int16_t          ano;
    int8_t           mes, dia, hora, minuto;
    int8_t           campo;
} inicio_t;

#endif
