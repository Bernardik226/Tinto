// firmware/simulador/hal_pc.h — o aparelho inteiro rodando no PC.
// Cartão em memória, fila de eventos roteirizada, buffer de tela; o código
// de cima vê a mesma hal_t.
#ifndef HAL_PC_H
#define HAL_PC_H

#include "hal/hal.h"

// Prepara um aparelho JÁ CONFIGURADO (/TINTO/, dono, recibos): é o estado
// de quase toda a vida, e testar a home não deve passar pelo primeiro uso.
const hal_t *pc_liga(void);

// Zera a mídia: FAT saudável, sem /TINTO/, perfil nem recibo.
void pc_memoria_virgem(void);

// ── roteiro de entrada ───────────────────────────────────────────────
void pc_empurra(evento_t ev);
void pc_botao(entrada_t b);            // clique
void pc_segura(entrada_t b, int ms);   // segurado
void pc_tick(void);

// ── o cartão de mentira ──────────────────────────────────────────────
void pc_poe_arquivo(const char *caminho, const char *conteudo);

// Quantas escritas aconteceram: prova que algo NÃO escreveu (HARDWARE
// §11).
int  pc_escritas(void);

// Quantas formatações: apagar é irreversível, e "não aconteceu" precisa ser
// afirmável.
int  pc_formatacoes(void);
bool pc_tem_arquivo(const char *caminho);

// A pasta existe? Prova que apagar não deixou diretório para trás.
bool pc_tem_diretorio(const char *caminho);

// ── o rádio ─────────────────────────────────────────────────────────
// Duas redes no ar; aqui a varredura responde no passo seguinte.
void pc_redes(const char *a, int forca_a, const char *b, int forca_b);
void pc_rede_aberta(const char *nome, int forca);

// Quantas vezes o aparelho pediu a hora à rede.
int  pc_ntp_pedidos(void);

// O fuso que o aparelho aplicou, em minutos de UTC.
int  pc_fuso(void);
void pc_nuvem_rota_zera(void);

// Responde ao PULL, e não à linha do último pedido (as duas em voo).
void pc_nuvem_responde_pull(const char *json);

// ── o cofre, para o teste espiar ────────────────────────────────────
// `pc_segredo` devolve o valor guardado, ou "". `pc_liga` zera o cofre
// (aparelho novo); `pc_segredos_zera` limpa no meio de um teste.
const char *pc_segredo(const char *chave);
bool        pc_reiniciou(void);
void        pc_segredos_zera(void);

// A força do sinal que o rádio de mentira devolve.
void pc_wifi_forca(int forca);

// Responde à linha de AGORA (gesto, captura, pareamento, catálogo), mesmo
// com um pull pendurado.
void pc_nuvem_responde_ja(const char *json);

// A carga que o fuel gauge falso devolve. -1 = não sei.
void pc_bateria(int pct);

// ── a nuvem de mentira ──────────────────────────────────────────────
// O cenário diz o que o servidor responde e confere o que o aparelho pediu.
void        pc_nuvem_responde(const char *json);

// A resposta que DEMORA: o teste entrega quando quiser, ou nunca (prazo).
void        pc_nuvem_demora(bool sim);
void        pc_nuvem_entrega(void);
const char *pc_nuvem_rota(void);
const char *pc_nuvem_corpo(void);

// Tudo o que subiu desde o último `pc_nuvem_historico_zera`, uma linha por
// pedido ("<rota> <corpo>"): uma resposta puxa o próximo gesto no mesmo
// passo, e `pc_nuvem_corpo` guarda só o último.
const char *pc_nuvem_historico(void);
void        pc_nuvem_historico_zera(void);

// "GET" ou "POST" pela MESMA regra do hal de verdade: há corpo, é POST.
// Sem isto, método errado passava nos testes e o servidor respondia 405.
const char *pc_nuvem_metodo(void);

// O id da operação, o áudio que subiria e a credencial: provam sem rede que
// o aparelho manda o que o contrato exige.
const char *pc_nuvem_operacao(void);
const char *pc_nuvem_audio(void);
const char *pc_nuvem_servidor(void);
const char *pc_nuvem_token(void);

// O código HTTP que a próxima resposta vai carregar.
void pc_nuvem_codigo(int c);

// O formato mandou o rádio esquecer a rede salva?
bool pc_rede_esquecida(void);

// Listagens de diretório pedidas: no aparelho, cada uma é uma ida ao SD.
int  pc_listagens(void);

void pc_relogio(data_t d, int hora, int minuto);

// ── simular o pior dia ──────────────────────────────────────────────
// RN-64 (desligar no meio da escrita) e RN-A3 (sem cartão) são interruptores
// aqui, não acidentes que se espera acontecer.
void pc_sem_cartao(bool sem);
void pc_memoria_estado(memoria_estado_t estado);

// Buffers grandes emprestados AGORA: diferente de zero no fim de um teste é
// vazamento.
int  pc_emprestados(void);

// Injeta o erro exato da mídia por operação. Os interruptores abaixo são a
// forma curta de ERR_ARQUIVO.
typedef enum {
    OP_MEM_CRIAR_DIR = 0,
    OP_MEM_ESCREVER,
    OP_MEM_LER,
    OP_MEM_RENOMEAR,
    OP_MEM_APAGAR,
    OP_MEM_LISTAR,
} operacao_memoria_t;

void pc_falhar_operacao(operacao_memoria_t op, erro_t erro);

void pc_falhar_renomear(bool falhar);
void pc_falhar_escrever(bool falhar);
void pc_falhar_criar_diretorio(bool falhar);
void pc_falhar_listar(bool falhar);
void pc_avanca_ms(uint32_t ms);
void pc_docar(bool docado);

// ── o áudio de mentira ──────────────────────────────────────────────
// Sem microfone: simula o tempo passando e o nível oscilando.
bool pc_gravando(void);

// ── a tela ───────────────────────────────────────────────────────────
const uint8_t *pc_tela(int *l, int *a);
int  pc_quadros(void);      // quantas vezes mostrar() foi chamado

// Quadros de cada intenção já enviados. Contar é mais preciso que olhar o
// último: ENTRAR numa tela manda a página e o seletor num parcial (EINK
// §5.5).
int pc_pinturas(pintura_t p);

// O último quadro assentado pela waveform COMPLETA, ou NULL.
// EINK §5.5: o parcial não apaga o que o completo assentou.
const uint8_t *pc_tela_assentada(void);

#endif
