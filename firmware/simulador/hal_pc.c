#include "hal_pc.h"
#include "dado/indice.h"
#include "tela/bitmap.h"   // TELA_L / TELA_A — o tamanho do buffer
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ENGENHARIA §3: tudo estático, aqui também; o harness não pode ter perfil
// de memória diferente do alvo.

#define MAX_ARQ      64
// Pasta é barata e são duas por item (`itens/<dia>/<id>/`): o teto do FALSO
// não pode decidir quantos dias o aparelho suporta.
#define MAX_DIR     256
#define MAX_CAMINHO  96
// Uma capa 240×360 em hexadecimal ocupa 21600 caracteres.
#define MAX_CONTEUDO 32768
#define MAX_EVENTOS  64

typedef struct {
    char caminho[MAX_CAMINHO];
    char conteudo[MAX_CONTEUDO];
    bool usado;
} arquivo_t;

typedef struct {
    char caminho[MAX_CAMINHO];
    bool usado;
} diretorio_t;

static struct {
    // O áudio de mentira.
    char      wav[128];
    bool      gravando;
    uint32_t  grav_desde;
    int       grav_s, trechos;
    int       escritas;
    arquivo_t arquivos[MAX_ARQ];
    diretorio_t diretorios[MAX_DIR];
    memoria_estado_t memoria_estado;
    uint8_t   proximo_aleatorio;
    int       formatacoes;

    evento_t  eventos[MAX_EVENTOS];
    int       ev_cabeca, ev_cauda;

    data_t    data;
    int       hora, minuto;
    uint32_t  ms;
    bool      docado;

    // OK = a operação passa; outro valor é o erro que a mídia devolve antes de
    // a operação acontecer.
    erro_t    falha_op[OP_MEM_LISTAR + 1];

    uint8_t   tela[(TELA_L + 7) / 8 * TELA_A];
    int       tela_l, tela_a;
    int       quadros;
    uint8_t   tela_nova[(240 + 7) / 8 * 416];  // o quadro que o completo assentou
    bool      tem_tela_nova;
    int       pinturas[5];  // quantos quadros de cada intenção
} f;

// ── entrada ──────────────────────────────────────────────────────────
static bool fk_proximo_evento(evento_t *out)
{
    if (f.ev_cabeca == f.ev_cauda) return false;
    *out = f.eventos[f.ev_cabeca];
    f.ev_cabeca = (f.ev_cabeca + 1) % MAX_EVENTOS;
    return true;
}

void pc_empurra(evento_t ev)
{
    int prox = (f.ev_cauda + 1) % MAX_EVENTOS;
    if (prox == f.ev_cabeca) return;   // fila cheia: descarta e segue
    f.eventos[f.ev_cauda] = ev;
    f.ev_cauda = prox;
}

void pc_botao(entrada_t b)
{
    pc_empurra((evento_t){ .tipo = EV_BOTAO, .botao = b, .ms = 0 });
}

void pc_segura(entrada_t b, int ms)
{
    pc_empurra((evento_t){ .tipo = EV_BOTAO, .botao = b, .ms = ms });
}

void pc_tick(void)
{
    pc_empurra((evento_t){ .tipo = EV_TICK });
}

void pc_docar(bool docado)
{
    f.docado = docado;
    pc_empurra((evento_t){ .tipo = EV_DOCADO, .valor = docado ? 1 : 0 });
}

// ── tela ─────────────────────────────────────────────────────────────
static void fk_mostrar(const uint8_t *bits, int l, int a,
                       pintura_t intencao)
{
    // Sem tinta, mas CONTA as intenções: prova que o app declara ao vidro o
    // gesto certo.
    if (intencao >= 0 && intencao < 5) f.pinturas[intencao]++;

    size_t n = (size_t)((l + 7) / 8) * a;
    if (n > sizeof f.tela) n = sizeof f.tela;

    // ── o que a waveform COMPLETA assentou ──────────────────────────
    // EINK §5.5: o parcial não apaga o que o completo assentou, então o completo
    // pinta a página SEM seletor. Guardar este quadro torna a regra conferível
    // no PC.
    if (intencao == PINTURA_TELA_NOVA) {
        memcpy(f.tela_nova, bits, n);
        f.tem_tela_nova = true;
    }

    memcpy(f.tela, bits, n);
    f.tela_l = l;
    f.tela_a = a;
    f.quadros++;
}

const uint8_t *pc_tela(int *l, int *a)
{
    if (l) *l = f.tela_l;
    if (a) *a = f.tela_a;
    return f.tela;
}

int pc_quadros(void) { return f.quadros; }
const uint8_t *pc_tela_assentada(void)
{
    return f.tem_tela_nova ? f.tela_nova : NULL;
}

int pc_pinturas(pintura_t p) { return (p >= 0 && p < 5) ? f.pinturas[p] : 0; }

// ── cartão ───────────────────────────────────────────────────────────
static arquivo_t *acha(const char *caminho)
{
    for (int i = 0; i < MAX_ARQ; i++)
        if (f.arquivos[i].usado && strcmp(f.arquivos[i].caminho, caminho) == 0)
            return &f.arquivos[i];
    return NULL;
}

static erro_t erro_de_disponibilidade(void)
{
    switch (f.memoria_estado) {
    case MEMORIA_PRONTA:
    case MEMORIA_SOMENTE_LEITURA:
    case MEMORIA_CHEIA:
        return OK;
    case MEMORIA_AUSENTE:
        return ERR_SEM_CARTAO;
    default:
        return ERR_ARQUIVO;
    }
}

static erro_t erro_para_mutacao(void)
{
    switch (f.memoria_estado) {
    case MEMORIA_PRONTA:          return OK;
    case MEMORIA_AUSENTE:         return ERR_SEM_CARTAO;
    case MEMORIA_SOMENTE_LEITURA: return ERR_SOMENTE_LEITURA;
    case MEMORIA_CHEIA:           return ERR_CHEIO;
    default:                      return ERR_ARQUIVO;
    }
}

static bool diretorio_existe(const char *caminho);

// O FatFs recusa criar ou abrir dentro de pasta que não existe
// (FR_NO_PATH), e o hal do PC recusa junto: mais permissivo que o cartão, ele
// aprovava o que a placa reprova.
static bool pai_existe(const char *caminho)
{
    const char *barra = strrchr(caminho, '/');
    if (!barra || barra == caminho) return true;     // raiz

    char pai[256];
    size_t n = (size_t)(barra - caminho);
    if (n >= sizeof pai) return false;
    memcpy(pai, caminho, n);
    pai[n] = '\0';
    return diretorio_existe(pai);
}

// CONFIG_FATFS_MAX_LFN=64: componente maior o cartão recusa, e o hal_sd
// monta o caminho num buffer de 160.
#define PC_COMPONENTE_MAX 64
#define PC_CAMINHO_MAX   160

static bool caminho_cabe(const char *caminho)
{
    if (strlen(caminho) >= PC_CAMINHO_MAX) return false;

    const char *inicio = caminho;
    while (*inicio) {
        if (*inicio == '/') { inicio++; continue; }
        const char *fim = strchr(inicio, '/');
        size_t n = fim ? (size_t)(fim - inicio) : strlen(inicio);
        if (n > PC_COMPONENTE_MAX) return false;
        if (!fim) break;
        inicio = fim + 1;
    }
    return true;
}

// ── o que o FAT RECUSA ──────────────────────────────────────────────
// FatFs devolve `FR_INVALID_NAME` para estes caracteres (`ff.c`,
// `create_name`). Sem esta conferência o `n:1` da IA passava em todos os
// testes e falhava em silêncio na placa. O simulador não precisa ser um FAT:
// precisa RECUSAR o que o FAT recusa.
static bool nome_valido(const char *caminho)
{
    for (const char *c = caminho; *c; c++)
        if (strchr("*:<>|\"?", *c)) return false;
    return true;
}

static erro_t fk_ler(const char *caminho, char *out, size_t max)
{
    erro_t estado = erro_de_disponibilidade();
    if (estado != OK) return estado;
    if (f.falha_op[OP_MEM_LER] != OK) return f.falha_op[OP_MEM_LER];
    if (!nome_valido(caminho)) return ERR_ARQUIVO;
    arquivo_t *a = acha(caminho);
    if (!a) return ERR_ARQUIVO;
    // RN-B6: cópia sempre com limite, e sempre terminando.
    size_t n = strlen(a->conteudo);
    if (n >= max) n = max - 1;
    memcpy(out, a->conteudo, n);
    out[n] = '\0';
    return OK;
}

static erro_t fk_escrever(const char *caminho, const char *conteudo)
{
    erro_t estado = erro_para_mutacao();
    if (estado != OK) return estado;
    f.escritas++;
    if (f.falha_op[OP_MEM_ESCREVER] != OK) return f.falha_op[OP_MEM_ESCREVER];
    if (!nome_valido(caminho)) return ERR_ARQUIVO;
    if (!caminho_cabe(caminho) || !pai_existe(caminho)) return ERR_ARQUIVO;
    arquivo_t *a = acha(caminho);
    if (!a) {
        for (int i = 0; i < MAX_ARQ && !a; i++)
            if (!f.arquivos[i].usado) a = &f.arquivos[i];
        if (!a) {
            f.memoria_estado = MEMORIA_CHEIA;
            return ERR_CHEIO;
        }
        a->usado = true;
        snprintf(a->caminho, sizeof a->caminho, "%s", caminho);
    }
    snprintf(a->conteudo, sizeof a->conteudo, "%s", conteudo);
    return OK;
}

static erro_t fk_anexar(const char *caminho, const char *conteudo)
{
    erro_t estado = erro_para_mutacao();
    if (estado != OK) return estado;
    arquivo_t *a = acha(caminho);
    if (!a) return fk_escrever(caminho, conteudo);
    size_t usados = strlen(a->conteudo), entram = strlen(conteudo);
    if (usados + entram >= sizeof a->conteudo) return ERR_CHEIO;
    memcpy(a->conteudo + usados, conteudo, entram + 1);
    return OK;
}

static erro_t fk_apagar(const char *caminho)
{
    // Apagar é a saída legítima de CHEIA; os demais estados bloqueiam a mutação
    // como no hardware.
    erro_t estado = erro_para_mutacao();
    if (estado != OK && estado != ERR_CHEIO) return estado;
    if (f.falha_op[OP_MEM_APAGAR] != OK) return f.falha_op[OP_MEM_APAGAR];

    arquivo_t *a = acha(caminho);
    if (!a) {
        // ── diretório VAZIO some, como no FAT ───────────────────────
        // `f_unlink` apaga arquivo e diretório vazio. A pasta que sobrava a cada
        // item apagado deixava o boot lento.
        for (int i = 0; i < MAX_DIR; i++) {
            if (!f.diretorios[i].usado ||
                strcmp(f.diretorios[i].caminho, caminho) != 0)
                continue;

            size_t n = strlen(caminho);
            for (int j = 0; j < MAX_ARQ; j++)
                if (f.arquivos[j].usado &&
                    strncmp(f.arquivos[j].caminho, caminho, n) == 0 &&
                    f.arquivos[j].caminho[n] == '/')
                    return ERR_ARQUIVO;          // FR_DENIED: não está vazio
            for (int j = 0; j < MAX_DIR; j++)
                if (f.diretorios[j].usado &&
                    strncmp(f.diretorios[j].caminho, caminho, n) == 0 &&
                    f.diretorios[j].caminho[n] == '/')
                    return ERR_ARQUIVO;

            f.diretorios[i].usado = false;
            return OK;
        }
        return ERR_ARQUIVO;
    }
    a->usado = false;
    if (f.memoria_estado == MEMORIA_CHEIA)
        f.memoria_estado = MEMORIA_PRONTA;
    return OK;
}

static erro_t fk_renomear(const char *de, const char *para)
{
    erro_t estado = erro_para_mutacao();
    if (estado != OK) return estado;

    // O rename de verdade é atômico: o alvo vira o novo conteúdo, ou nada muda.
    if (f.falha_op[OP_MEM_RENOMEAR] != OK) return f.falha_op[OP_MEM_RENOMEAR];
    if (!caminho_cabe(para) || !pai_existe(para)) return ERR_ARQUIVO;

    arquivo_t *o = acha(de);
    if (!o) return ERR_ARQUIVO;

    arquivo_t *d = acha(para);
    if (!d) {
        for (int i = 0; i < MAX_ARQ && !d; i++)
            if (!f.arquivos[i].usado) d = &f.arquivos[i];
        if (!d) {
            f.memoria_estado = MEMORIA_CHEIA;
            return ERR_CHEIO;
        }
        d->usado = true;
        snprintf(d->caminho, sizeof d->caminho, "%s", para);
    }
    memcpy(d->conteudo, o->conteudo, sizeof d->conteudo);
    o->usado = false;
    return OK;
}

void pc_sem_cartao(bool sem)
{
    f.memoria_estado = sem ? MEMORIA_AUSENTE : MEMORIA_PRONTA;
}

void pc_memoria_estado(memoria_estado_t estado)
{
    f.memoria_estado = estado;
}

void pc_falhar_operacao(operacao_memoria_t op, erro_t erro)
{
    f.falha_op[op] = erro;
}

void pc_falhar_renomear(bool falhar)
{
    pc_falhar_operacao(OP_MEM_RENOMEAR, falhar ? ERR_ARQUIVO : OK);
}

void pc_falhar_escrever(bool falhar)
{
    pc_falhar_operacao(OP_MEM_ESCREVER, falhar ? ERR_ARQUIVO : OK);
}

static bool diretorio_existe(const char *caminho)
{
    for (int i = 0; i < MAX_DIR; i++)
        if (f.diretorios[i].usado &&
            strcmp(f.diretorios[i].caminho, caminho) == 0)
            return true;
    return false;
}

static erro_t fk_criar_diretorio(const char *caminho)
{
    erro_t estado = erro_para_mutacao();
    if (estado != OK) return estado;
    if (f.falha_op[OP_MEM_CRIAR_DIR] != OK) return f.falha_op[OP_MEM_CRIAR_DIR];
    if (!nome_valido(caminho)) return ERR_ARQUIVO;
    if (!caminho_cabe(caminho) || !pai_existe(caminho)) return ERR_ARQUIVO;
    if (diretorio_existe(caminho)) return OK;
    for (int i = 0; i < MAX_DIR; i++) {
        if (f.diretorios[i].usado) continue;
        f.diretorios[i].usado = true;
        snprintf(f.diretorios[i].caminho,
                 sizeof f.diretorios[i].caminho, "%s", caminho);
        return OK;
    }
    f.memoria_estado = MEMORIA_CHEIA;
    return ERR_CHEIO;
}

static erro_t fk_tipo_caminho(const char *caminho, caminho_tipo_t *out)
{
    erro_t estado = erro_de_disponibilidade();
    if (estado != OK) return estado;
    if (!out) return ERR_ARQUIVO;
    if (acha(caminho)) {
        *out = CAMINHO_ARQUIVO;
        return OK;
    }
    if (diretorio_existe(caminho)) {
        *out = CAMINHO_DIRETORIO;
        return OK;
    }
    *out = CAMINHO_AUSENTE;
    return OK;
}

static memoria_estado_t fk_memoria_estado(void) { return f.memoria_estado; }

static erro_t fk_formatar_memoria(void)
{
    f.formatacoes++;
    switch (f.memoria_estado) {
    case MEMORIA_AUSENTE:         return ERR_SEM_CARTAO;
    case MEMORIA_COMUNICACAO:     return ERR_ARQUIVO;
    case MEMORIA_SOMENTE_LEITURA: return ERR_SOMENTE_LEITURA;
    default:                      break;
    }
    memset(f.arquivos, 0, sizeof f.arquivos);
    memset(f.diretorios, 0, sizeof f.diretorios);
    f.memoria_estado = MEMORIA_PRONTA;
    return OK;
}

// Conta o que foi escrito de verdade: a tela precisa de um número que MUDE
// quando se grava.
static erro_t fk_espaco(uint32_t *usado_kb, uint32_t *total_kb)
{
    if (f.memoria_estado == MEMORIA_AUSENTE) return ERR_SEM_CARTAO;

    uint64_t bytes = 0;
    for (int i = 0; i < MAX_ARQ; i++)
        if (f.arquivos[i].usado) bytes += strlen(f.arquivos[i].conteudo) + 1;

    // O que o cartão da bancada relata (8 GB nominais, 7,74 GB depois do FAT):
    // prova com número impossível ensina errado.
    *total_kb = 7561376u;
    *usado_kb = (uint32_t)(bytes / 1024);
    return OK;
}

// ── o rádio de mentira ───────────────────────────────────────────────
static rede_wifi_t redes_do_ar[REDES_MAX];
static int         n_redes_do_ar;

void pc_redes(const char *a, int forca_a, const char *b, int forca_b)
{
    n_redes_do_ar = 0;
    if (a) { snprintf(redes_do_ar[n_redes_do_ar].nome,
                      sizeof redes_do_ar[0].nome, "%s", a);
             redes_do_ar[n_redes_do_ar].forca = (int8_t)forca_a;
             redes_do_ar[n_redes_do_ar].aberta = false;
             n_redes_do_ar++; }
    if (b) { snprintf(redes_do_ar[n_redes_do_ar].nome,
                      sizeof redes_do_ar[0].nome, "%s", b);
             redes_do_ar[n_redes_do_ar].forca = (int8_t)forca_b;
             redes_do_ar[n_redes_do_ar].aberta = false;
             n_redes_do_ar++; }
}

// Uma rede SEM senha: muda o fluxo (conecta sem passar pelo teclado).
void pc_rede_aberta(const char *nome, int forca)
{
    if (!nome || n_redes_do_ar >= REDES_MAX) return;
    snprintf(redes_do_ar[n_redes_do_ar].nome,
             sizeof redes_do_ar[0].nome, "%s", nome);
    redes_do_ar[n_redes_do_ar].forca  = (int8_t)forca;
    redes_do_ar[n_redes_do_ar].aberta = true;
    n_redes_do_ar++;
}

// O NTP falso só conta: o que se prova é que o aparelho PERGUNTA na hora
// certa.
static int ntp_pedidos;

int pc_ntp_pedidos(void) { return ntp_pedidos; }

static void fk_hora_da_rede(void)
{
    ntp_pedidos++;
}

// Derrubar o cliente é metade do contrato: ligar duas vezes sem descer no
// meio abortava a placa.
static void fk_hora_da_rede_para(void) {}

// O fuso aplicado. `pc_relogio` já entrega a hora local; isto prova que o
// aparelho APLICOU o que recebeu.
static int fuso_aplicado;

int pc_fuso(void) { return fuso_aplicado; }

static void fk_fuso(int minutos) { fuso_aplicado = minutos; }

// -1 é "não sei", o padrão; o cenário que testa a barra diz o valor.
static int bateria_falsa = -1;

void pc_bateria(int pct) { bateria_falsa = pct; }

static int fk_bateria(void) { return bateria_falsa; }

static void fk_wifi_procurar(void)
{
    pc_empurra((evento_t){ .tipo = EV_WIFI_REDES });
}

static int fk_wifi_redes(rede_wifi_t *out, int max)
{
    int n = n_redes_do_ar < max ? n_redes_do_ar : max;
    for (int i = 0; i < n; i++) out[i] = redes_do_ar[i];
    return n;
}

static erro_t fk_wifi_conectar(const char *nome, const char *senha)
{
    (void)senha;
    return nome && nome[0] ? OK : ERR_REDE;
}

// ── o rádio, do ponto de vista de quem pergunta ─────────────────────
// Sem `wifi_estado` aqui, nenhum teste notava a força do sinal congelada.
static int  wifi_forca_falsa = 90;
static int  wifi_estado_falso = 2;   // REDE_LIGADA

void pc_wifi_forca(int forca)  { wifi_forca_falsa = forca; }

static int fk_wifi_estado(char *ip, size_t max, int *forca)
{
    if (ip && max) snprintf(ip, max, "%s", "192.168.0.31");
    if (forca) *forca = wifi_forca_falsa;
    return wifi_estado_falso;
}

// A soma do que está sob um prefixo: a MESMA pergunta que o FatFs responde
// na placa.
static erro_t fk_uso_de(const char *dir, uint32_t *kb)
{
    if (!dir || !kb) return ERR_ARQUIVO;
    if (f.memoria_estado == MEMORIA_AUSENTE) return ERR_SEM_CARTAO;

    size_t n = strlen(dir);
    uint64_t bytes = 0;
    for (int i = 0; i < MAX_ARQ; i++) {
        if (!f.arquivos[i].usado) continue;
        if (strncmp(f.arquivos[i].caminho, dir, n) != 0) continue;
        bytes += strlen(f.arquivos[i].conteudo) + 1;
    }
    *kb = (uint32_t)(bytes / 1024);
    return OK;
}

static void fk_aleatorio(uint8_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++) out[i] = f.proximo_aleatorio++;
}

void pc_falhar_criar_diretorio(bool falhar)
{
    pc_falhar_operacao(OP_MEM_CRIAR_DIR, falhar ? ERR_ARQUIVO : OK);
}

void pc_falhar_listar(bool falhar)
{
    pc_falhar_operacao(OP_MEM_LISTAR, falhar ? ERR_ARQUIVO : OK);
}

static void lista_caminho(const char *caminho, const char *dir,
                          char nomes[][40], int max, int *achados)
{
    size_t n = strlen(dir);
    while (n > 1 && dir[n - 1] == '/') n--;
    if (*achados >= max || strncmp(caminho, dir, n) != 0) return;
    if (caminho[n] != '\0' && caminho[n] != '/') return;
    const char *resto = caminho + n;
    if (*resto == '/') resto++;
    const char *barra = strchr(resto, '/');
    size_t tam = barra ? (size_t)(barra - resto) : strlen(resto);
    if (tam == 0 || tam >= 40) return;

    for (int j = 0; j < *achados; j++)
        if (strncmp(nomes[j], resto, tam) == 0 && nomes[j][tam] == '\0')
            return;

    memcpy(nomes[*achados], resto, tam);
    nomes[*achados][tam] = '\0';
    (*achados)++;
}

// Listagens de diretório pedidas: no aparelho, cada uma é uma ida ao SD.
static int listagens;
int pc_listagens(void) { return listagens; }

// ── o buffer grande, e a conta de quem devolveu ─────────────────────
// Na placa é PSRAM (`memoria_hal.h`); aqui a metade que importa é contar.
// `emprestados` que não volta a zero é vazamento.
static int emprestados;
int  pc_emprestados(void) { return emprestados; }

static void *fk_emprestar(size_t desejado, size_t minimo, size_t *real)
{
    void *p = malloc(desejado);
    size_t tam = desejado;
    if (!p && minimo && minimo < desejado) { p = malloc(minimo); tam = minimo; }
    if (!p) return NULL;
    if (real) *real = tam;
    emprestados++;
    return p;
}

static void fk_devolver(void *p)
{
    if (!p) return;
    free(p);
    emprestados--;
}

static erro_t fk_listar(const char *dir, int desde, char nomes[][40], int max,
                        int *quantos)
{
    listagens++;
    if (quantos) *quantos = 0;
    erro_t estado = erro_de_disponibilidade();
    if (estado != OK) return estado;
    if (!quantos) return ERR_ARQUIVO;
    if (f.falha_op[OP_MEM_LISTAR] != OK) return f.falha_op[OP_MEM_LISTAR];
    if (desde < 0) return ERR_ARQUIVO;

    // A dedup do `lista_caminho` olha o que já entrou: a listagem é montada
    // INTEIRA e só depois fatiada, senão a página 2 repetiria um nome.
    char todos[MAX_DIR][40];
    int achados = 0;
    for (int i = 0; i < MAX_ARQ && achados < MAX_DIR; i++) {
        if (!f.arquivos[i].usado) continue;
        lista_caminho(f.arquivos[i].caminho, dir, todos, MAX_DIR, &achados);
    }
    for (int i = 0; i < MAX_DIR && achados < MAX_DIR; i++)
        if (f.diretorios[i].usado)
            lista_caminho(f.diretorios[i].caminho, dir,
                          todos, MAX_DIR, &achados);

    int copiados = 0;
    for (int i = desde; i < achados && copiados < max; i++)
        memcpy(nomes[copiados++], todos[i], 40);
    // ── pasta que não existe é ERRO, como no FAT ────────────────────
    // OK com zero para qualquer caminho escondeu um amanhã sem pasta que ficava
    // com os itens de hoje na RAM.
    if (achados == 0 && !diretorio_existe(dir)) return ERR_SEM_PASTA;

    *quantos = copiados;
    return OK;
}

void pc_poe_arquivo(const char *caminho, const char *conteudo)
{
    erro_t guardado = f.falha_op[OP_MEM_ESCREVER];
    f.falha_op[OP_MEM_ESCREVER] = OK;   // montar cenário nunca falha

    // Montar cenário cria as pastas do caminho. É preparação de teste: em
    // produção, quem precisa da pasta continua tendo que criar.
    char pasta[256];
    for (const char *b = strchr(caminho + 1, '/'); b; b = strchr(b + 1, '/')) {
        size_t n = (size_t)(b - caminho);
        if (n >= sizeof pasta) break;
        memcpy(pasta, caminho, n);
        pasta[n] = '\0';
        if (!diretorio_existe(pasta)) {
            for (int i = 0; i < MAX_DIR; i++) {
                if (f.diretorios[i].usado) continue;
                f.diretorios[i].usado = true;
                snprintf(f.diretorios[i].caminho,
                         sizeof f.diretorios[i].caminho, "%s", pasta);
                break;
            }
        }
    }

    fk_escrever(caminho, conteudo);
    f.falha_op[OP_MEM_ESCREVER] = guardado;
}

bool pc_tem_arquivo(const char *caminho) { return acha(caminho) != NULL; }

bool pc_tem_diretorio(const char *caminho)
{
    return diretorio_existe(caminho);
}


// ── resto ────────────────────────────────────────────────────────────
static bool     fk_docado(void)             { return f.docado; }
static uint32_t fk_agora_ms(void)           { return f.ms; }

static void fk_ajustar_relogio(data_t d, int hora, int minuto)
{
    f.data   = d;
    f.hora   = hora;
    f.minuto = minuto;
}

static void fk_relogio(data_t *d, int *hora, int *minuto)
{
    if (d)      *d      = f.data;
    if (hora)   *hora   = f.hora;
    if (minuto) *minuto = f.minuto;
}

// ── áudio de mentira ────────────────────────────────────────────────
// Sem microfone: simula o TEMPO e os trechos. RN-13: os trechos de uma
// sessão vão para o MESMO arquivo.
static erro_t fk_audio_inicia(const char *caminho)
{
    // O WAV é escrito NO CARTÃO: sem cartão, recusa (a tela precisa mostrar o
    // ● que não faz nada).
    if (f.memoria_estado == MEMORIA_AUSENTE)      return ERR_SEM_CARTAO;
    if (f.memoria_estado == MEMORIA_SOMENTE_LEITURA)   return ERR_SOMENTE_LEITURA;
    if (f.memoria_estado == MEMORIA_CHEIA)        return ERR_CHEIO;

    snprintf(f.wav, sizeof f.wav, "%s", caminho ? caminho : "");
    f.gravando   = true;
    f.grav_desde = f.ms;
    f.grav_s     = 0;
    f.trechos    = 1;
    return OK;
}

static erro_t fk_audio_pausa(void)
{
    if (!f.gravando) return ERR_INTERNO;
    f.grav_s  += (int)((f.ms - f.grav_desde) / 1000);
    f.gravando = false;
    return OK;
}

static erro_t fk_audio_retoma(void)
{
    if (f.gravando) return ERR_INTERNO;
    f.gravando   = true;
    f.grav_desde = f.ms;
    f.trechos++;               // RN-13: outro trecho, o MESMO arquivo
    return OK;
}

static erro_t fk_audio_fecha(int *dur_s, int *trechos)
{
    if (f.gravando) fk_audio_pausa();
    if (dur_s)   *dur_s   = f.grav_s;
    if (trechos) *trechos = f.trechos;
    return OK;
}

static erro_t fk_audio_descarta(void)
{
    f.gravando = false;
    f.grav_s = 0; f.trechos = 0; f.wav[0] = '\0';
    return OK;
}


bool pc_gravando(void) { return f.gravando; }


// ── os segredos, fora do "cartão" ───────────────────────────────────
// Na placa é a NVS; aqui é RAM, separada do sistema de arquivos falso para
// provar que a senha NÃO está no cartão. `pc_liga` zera o cofre: entrega um
// aparelho NOVO, e um teste de registro não pode nascer registrado.
#define SEGREDOS_MAX 8

static struct {
    char chave[24];
    char valor[80];
    bool usado;
} segredos[SEGREDOS_MAX];

void pc_segredos_zera(void)
{
    memset(segredos, 0, sizeof segredos);
}

const char *pc_segredo(const char *chave)
{
    for (int i = 0; i < SEGREDOS_MAX; i++)
        if (segredos[i].usado && strcmp(segredos[i].chave, chave) == 0)
            return segredos[i].valor;
    return "";
}

static erro_t fk_segredo_grava(const char *chave, const char *valor)
{
    if (!chave || !valor) return ERR_INTERNO;

    int livre = -1;
    for (int i = 0; i < SEGREDOS_MAX; i++) {
        if (segredos[i].usado && strcmp(segredos[i].chave, chave) == 0) {
            snprintf(segredos[i].valor, sizeof segredos[i].valor, "%s", valor);
            return OK;
        }
        if (!segredos[i].usado && livre < 0) livre = i;
    }
    if (livre < 0) return ERR_CHEIO;

    snprintf(segredos[livre].chave, sizeof segredos[livre].chave, "%s", chave);
    snprintf(segredos[livre].valor, sizeof segredos[livre].valor, "%s", valor);
    segredos[livre].usado = true;
    return OK;
}

static erro_t fk_segredo_le(const char *chave, char *out, size_t max)
{
    if (!chave || !out || !max) return ERR_INTERNO;
    out[0] = '\0';

    for (int i = 0; i < SEGREDOS_MAX; i++)
        if (segredos[i].usado && strcmp(segredos[i].chave, chave) == 0) {
            snprintf(out, max, "%s", segredos[i].valor);
            return OK;
        }
    return ERR_ARQUIVO;
}

static erro_t fk_segredo_apaga(const char *chave)
{
    if (!chave) return ERR_INTERNO;
    for (int i = 0; i < SEGREDOS_MAX; i++)
        if (segredos[i].usado && strcmp(segredos[i].chave, chave) == 0)
            segredos[i].usado = false;
    return OK;
}

// ── a nuvem de mentira ──────────────────────────────────────────────
// Responde o que o cenário mandou e guarda o que o aparelho pediu.
static char nuvem_rota[NUVEM_ROTA_MAX];
static char nuvem_corpo[1024];
// ── as DUAS linhas, também aqui ─────────────────────────────────────
// Uma para o pull (pendurado), outra para o que nasce de um gesto. Com uma
// linha só, o teste do gesto que não espera o pull passaria à toa.
#define PC_LINHA_JA   0
#define PC_LINHA_PULL 1

// O mesmo teto de 16 KiB da placa: senão testa um truncamento que não
// existe lá.
static char nuvem_resposta_pronta[2][16 * 1024 + 1];
static bool nuvem_tem_resposta[2];
static int  nuvem_linha_ultima;   // de quem foi o último pedido
static int  nuvem_linha_lida;     // de quem foi a última resposta lida

// A linha da resposta mais RECENTE. A leitura não consome: decide qual das
// duas o cenário mandou responder por último.
static int  nuvem_linha_fresca = -1;

// Qual linha ainda espera resposta: decide o alvo do `pc_nuvem_responde`.
static bool nuvem_pendente[2];

static int pc_linha_da_rota(const char *rota)
{
    return (rota && strncmp(rota, "/v1/pull", 8) == 0) ? PC_LINHA_PULL
                                                       : PC_LINHA_JA;
}
static char nuvem_operacao[80];
static int  nuvem_codigo_falso = 200;
static char nuvem_audio[128];
static char nuvem_servidor[80];
static char nuvem_token[48];

// ── a resposta que DEMORA ───────────────────────────────────────────
// Ligada, o pedido não empurra evento: o teste entrega com
// `pc_nuvem_entrega`, ou nunca. Prova o que acontece ENQUANTO se espera
// ("Estruturando").
static bool nuvem_demora;

void pc_nuvem_demora(bool sim) { nuvem_demora = sim; }

void pc_nuvem_entrega(void)
{
    pc_empurra((evento_t){ .tipo = EV_REDE_RESULTADO });
}

static void responde_na_linha(int linha, const char *json)
{
    snprintf(nuvem_resposta_pronta[linha], sizeof nuvem_resposta_pronta[0],
             "%s", json ? json : "");
    nuvem_tem_resposta[linha] = json != NULL;
    if (json) nuvem_linha_fresca = linha;
}

// Responde na linha do último PEDIDO, se ele ainda espera; senão na de
// agora (testes que armam `nuvem_esperando` na mão).
void pc_nuvem_responde(const char *json)
{
    responde_na_linha(nuvem_pendente[nuvem_linha_ultima] ? nuvem_linha_ultima
                                                         : PC_LINHA_JA, json);
}

void pc_nuvem_responde_ja(const char *json)
{
    responde_na_linha(PC_LINHA_JA, json);
}

// Responde ao pull, sem depender de qual pedido saiu por último.
void pc_nuvem_responde_pull(const char *json)
{
    responde_na_linha(PC_LINHA_PULL, json);
}

const char *pc_nuvem_rota(void)     { return nuvem_rota; }

// A mesma regra do `nuvem_esp.c`: corpo presente é POST, ausente é GET.
const char *pc_nuvem_metodo(void)   { return nuvem_corpo[0] ? "POST" : "GET"; }
const char *pc_nuvem_corpo(void)    { return nuvem_corpo; }

// ── TUDO o que subiu, e não só o último ────────────────────────────
// Com a fila no cartão, uma resposta puxa o próximo gesto DENTRO do mesmo
// `app_passo`: o histórico prova "as três subiram, cada uma uma vez".
static char nuvem_historico[2048];
const char *pc_nuvem_historico(void) { return nuvem_historico; }
void pc_nuvem_historico_zera(void)   { nuvem_historico[0] = '\0'; }

static void guarda_no_historico(const char *rota, const char *corpo)
{
    size_t tem = strlen(nuvem_historico);
    if (tem + 400 >= sizeof nuvem_historico) return;
    snprintf(nuvem_historico + tem, sizeof nuvem_historico - tem, "%s %s\n",
             rota ? rota : "", corpo ? corpo : "");
}
const char *pc_nuvem_operacao(void) { return nuvem_operacao; }
const char *pc_nuvem_audio(void)    { return nuvem_audio; }
const char *pc_nuvem_servidor(void) { return nuvem_servidor; }
const char *pc_nuvem_token(void)    { return nuvem_token; }

// Guarda o CAMINHO que o hal leria; na placa o WAV vira multipart.
static void fk_nuvem_pede_audio(const char *rota, const char *caminho,
                                const char *operacao)
{
    snprintf(nuvem_rota,  sizeof nuvem_rota,  "%s", rota ? rota : "");
    nuvem_linha_ultima = pc_linha_da_rota(rota);
    nuvem_pendente[nuvem_linha_ultima] = true;
    snprintf(nuvem_audio, sizeof nuvem_audio, "%s", caminho ? caminho : "");
    snprintf(nuvem_operacao, sizeof nuvem_operacao, "%s",
             operacao ? operacao : "");
    nuvem_corpo[0] = '\0';

    // O mesmo evento do pedido normal: acima do hal, áudio e JSON voltam
    // igual.
    if (!nuvem_demora) pc_empurra((evento_t){ .tipo = EV_REDE_RESULTADO });
}

static void fk_nuvem_credencial(const char *servidor, const char *token)
{
    snprintf(nuvem_servidor, sizeof nuvem_servidor, "%s",
             servidor ? servidor : "");
    snprintf(nuvem_token, sizeof nuvem_token, "%s", token ? token : "");
}

// O falso só registra que foi chamado.
static bool rede_esquecida;
bool pc_rede_esquecida(void) { return rede_esquecida; }
static void fk_esquecer_rede(void) { rede_esquecida = true; }

// MAC fixo: um id que mudasse a cada execução mudaria o teste de registro.
static void fk_id_aparelho(char *out, size_t max)
{
    snprintf(out, max, "AA:BB:CC:44:55:66");
}

static void fk_nuvem_pede(const char *rota, const char *corpo,
                          const char *operacao)
{
    guarda_no_historico(rota, corpo);
    snprintf(nuvem_rota,  sizeof nuvem_rota,  "%s", rota ? rota : "");
    nuvem_linha_ultima = pc_linha_da_rota(rota);
    nuvem_pendente[nuvem_linha_ultima] = true;
    // Vazio quando não há corpo: é o que `pc_nuvem_metodo` lê para dizer GET.
    snprintf(nuvem_corpo, sizeof nuvem_corpo, "%s", corpo ? corpo : "");
    snprintf(nuvem_operacao, sizeof nuvem_operacao, "%s",
             operacao ? operacao : "");
    if (!nuvem_demora) pc_empurra((evento_t){ .tipo = EV_REDE_RESULTADO });
}

// Prova que uma chamada NÃO aconteceu: sem zerar, a rota antiga confunde.
void pc_nuvem_rota_zera(void)
{
    nuvem_codigo_falso = 200;
    nuvem_demora = false;
    nuvem_rota[0] = 0;
    nuvem_audio[0] = 0;
    nuvem_operacao[0] = 0;
}

// As recusas que só existem como número: sem minutos, conta caída, token
// recusado.
void pc_nuvem_codigo(int c) { nuvem_codigo_falso = c; }
static int fk_nuvem_codigo(void) { return nuvem_codigo_falso; }

static erro_t fk_nuvem_resposta(char *out, size_t max)
{
    // A de AGORA primeiro, como na placa quando as duas terminam juntas. Sem
    // nenhuma pronta, a do último PEDIDO: o erro de uma captura aplicado ao
    // delta deixaria a captura esperando para sempre.
    int qual = nuvem_linha_fresca >= 0 && nuvem_tem_resposta[nuvem_linha_fresca]
                 ? nuvem_linha_fresca
             : nuvem_tem_resposta[PC_LINHA_JA]   ? PC_LINHA_JA
             : nuvem_tem_resposta[PC_LINHA_PULL] ? PC_LINHA_PULL
                                                 : nuvem_linha_ultima;
    nuvem_linha_lida = qual;
    nuvem_pendente[qual] = false;

    if (!nuvem_tem_resposta[qual]) return ERR_REDE;
    if (nuvem_codigo_falso < 200 || nuvem_codigo_falso > 299)
        return ERR_REDE;

    snprintf(out, max, "%s", nuvem_resposta_pronta[qual]);
    return OK;
}

static int fk_nuvem_linha(void) { return nuvem_linha_lida; }
static bool reiniciou;
bool pc_reiniciou(void) { return reiniciou; }
static void fk_reiniciar(void) { reiniciou = true; }
static void fk_registrar(const char *assunto, const char *texto)
{
    (void)assunto; (void)texto;    // no PC o diagnóstico é o próprio teste
}

void pc_relogio(data_t d, int hora, int minuto)
{
    f.data = d; f.hora = hora; f.minuto = minuto;
}

void pc_avanca_ms(uint32_t ms) { f.ms += ms; }

// ── a montagem ───────────────────────────────────────────────────────
static const hal_t HAL = {
    .proximo_evento = fk_proximo_evento,
    .mostrar        = fk_mostrar,
    .ler            = fk_ler,
    .escrever       = fk_escrever,
    .anexar         = fk_anexar,
    .apagar         = fk_apagar,
    .renomear       = fk_renomear,
    .listar         = fk_listar,
    .memoria_estado = fk_memoria_estado,

    // No PC é `malloc`, e o contador prova que o buffer voltou.
    .emprestar = fk_emprestar,
    .devolver  = fk_devolver,
    .criar_diretorio = fk_criar_diretorio,
    .tipo_caminho   = fk_tipo_caminho,
    .formatar_memoria = fk_formatar_memoria,
    .aleatorio      = fk_aleatorio,
    .espaco         = fk_espaco,
    .uso_de         = fk_uso_de,
    .wifi_procurar  = fk_wifi_procurar,
    .wifi_redes     = fk_wifi_redes,
    .wifi_conectar  = fk_wifi_conectar,
    .wifi_estado    = fk_wifi_estado,
    .hora_da_rede   = fk_hora_da_rede,
    .hora_da_rede_para = fk_hora_da_rede_para,
    .fuso           = fk_fuso,
    .bateria        = fk_bateria,
    .docado         = fk_docado,
    .relogio        = fk_relogio,
    .ajustar_relogio = fk_ajustar_relogio,
    .registrar      = fk_registrar,
    .audio_inicia   = fk_audio_inicia,
    .audio_pausa    = fk_audio_pausa,
    .audio_retoma   = fk_audio_retoma,
    .audio_fecha    = fk_audio_fecha,
    .audio_descarta = fk_audio_descarta,
    .agora_ms       = fk_agora_ms,
    .reiniciar      = fk_reiniciar,
    .nuvem_pede       = fk_nuvem_pede,
    .nuvem_pede_audio = fk_nuvem_pede_audio,
    .nuvem_credencial = fk_nuvem_credencial,
    .segredo_grava    = fk_segredo_grava,
    .segredo_le       = fk_segredo_le,
    .segredo_apaga    = fk_segredo_apaga,
    .nuvem_resposta   = fk_nuvem_resposta,
    .nuvem_linha      = fk_nuvem_linha,
    .nuvem_codigo     = fk_nuvem_codigo,
    .id_aparelho      = fk_id_aparelho,
    .esquecer_rede    = fk_esquecer_rede,
};

static void configura_cartao(void);

const hal_t *pc_liga(void)
{
    // O índice é do aparelho ligado: religar é outro aparelho, e um teste não
    // pode herdar os itens do anterior.
    indice_solta(&HAL);

    // O código HTTP e a demora voltam ao normal: são globais, e um 403 deixado
    // para trás quebrava os testes seguintes.
    nuvem_codigo_falso = 200;
    nuvem_demora       = false;
    memset(segredos, 0, sizeof segredos);

    // Um pedido em voo no teste anterior roubava a resposta do seguinte.
    memset(nuvem_pendente, 0, sizeof nuvem_pendente);
    nuvem_linha_ultima = PC_LINHA_JA;
    // E uma resposta armada e não lida respondia o primeiro pedido do
    // aparelho seguinte.
    memset(nuvem_tem_resposta, 0, sizeof nuvem_tem_resposta);
    nuvem_linha_fresca = -1;
    rede_esquecida = false;
    reiniciou = false;

    memset(&f, 0, sizeof f);
    f.data   = (data_t){ .ano = 2026, .mes = 8, .dia = 14 };
    f.hora   = 9;
    f.minuto = 14;
    f.memoria_estado = MEMORIA_PRONTA;
    f.tela_l = TELA_L;
    f.tela_a = TELA_A;
    configura_cartao();
    return &HAL;
}

int pc_escritas(void) { return f.escritas; }
int pc_formatacoes(void) { return f.formatacoes; }

// ── o cartão que o aparelho já tem ──────────────────────────────────
// Direto na mídia falsa, sem dado/: descreve o RESULTADO de um primeiro uso,
// não o repete.
static void cria_dir(const char *caminho)
{
    for (int i = 0; i < MAX_DIR; i++) {
        if (f.diretorios[i].usado) continue;
        f.diretorios[i].usado = true;
        snprintf(f.diretorios[i].caminho,
                 sizeof f.diretorios[i].caminho, "%s", caminho);
        return;
    }
}

void pc_memoria_virgem(void)
{
    memset(f.arquivos, 0, sizeof f.arquivos);
    memset(f.diretorios, 0, sizeof f.diretorios);
    f.memoria_estado = MEMORIA_PRONTA;
    f.escritas = 0;
}

static void configura_cartao(void)
{
    cria_dir("/TINTO");
    cria_dir("/TINTO/itens");
    cria_dir("/TINTO/acervo");
    cria_dir("/TINTO/entrada");
    cria_dir("/TINTO/sistema");
    pc_poe_arquivo("/TINTO/sistema/formato.json", "{\"v\":1}");
    pc_poe_arquivo("/TINTO/sistema/perfil.json",
        "{\"v\":1,\"proprietario_id\":\"0123456789abcdef0123456789abcdef\","
        "\"nome\":\"Usuário\",\"onboarding_v\":1,\"relogio_v\":1}");
    f.escritas = 0;   // montar o cenário não conta como escrita do aparelho
}
