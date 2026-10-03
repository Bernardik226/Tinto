// hal/hal_sd.c — montagem observável e I/O da memória interna.
#include "hal_sd.h"
#include "memoria_hal.h"
#include "../pins.h"

#include "diskio_impl.h"
#include "diskio_sdmmc.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_vfs_fat.h"
#include "ff.h"
#include "sdmmc_cmd.h"

#include <string.h>

#define RAIZ_VFS "/sd"

static const char *TAG = "memoria";
static sdmmc_card_t cartao;
static FATFS *sistema_arquivos;
static BYTE unidade = FF_DRV_NOT_USED;
static char drive[3];
static memoria_estado_t estado = MEMORIA_COMUNICACAO;
static uint8_t trabalho[4096];

static uint64_t capacidade_bytes(void)
{
    return (uint64_t)cartao.csd.capacity * cartao.csd.sector_size;
}

static void registra(const char *fase, esp_err_t esp, FRESULT fat)
{
    // RAM interna livre junto da montagem: quando ela falta, o cartão perde o
    // buffer de DMA e a tela diz "precisa de reparo". Assim as causas se
    // separam numa linha.
    ESP_LOGI(TAG, "ram interna livre: %u KB · psram livre: %u KB",
             (unsigned)(mem_interna_livre() / 1024),
             (unsigned)(mem_psram_livre() / 1024));

    ESP_LOGI(TAG, "fase=%s esp=%s fat=%d capacidade=%llu",
             fase, esp_err_to_name(esp), (int)fat,
             (unsigned long long)capacidade_bytes());
}

static memoria_estado_t estado_do_esp(esp_err_t e)
{
    if (e == ESP_ERR_NOT_FOUND || e == ESP_ERR_INVALID_RESPONSE)
        return MEMORIA_AUSENTE;
    return MEMORIA_COMUNICACAO;
}

static memoria_estado_t estado_do_fat(FRESULT r)
{
    switch (r) {
    case FR_OK:              return MEMORIA_PRONTA;
    case FR_NO_FILESYSTEM:   return MEMORIA_SEM_FILESYSTEM;
    case FR_INT_ERR:         return MEMORIA_CORROMPIDA;
    case FR_WRITE_PROTECTED: return MEMORIA_SOMENTE_LEITURA;
    case FR_DISK_ERR:
    case FR_NOT_READY:       return MEMORIA_COMUNICACAO;
    default:                 return MEMORIA_CORROMPIDA;
    }
}

static erro_t erro_do_fat(FRESULT r)
{
    switch (r) {
    case FR_OK:
        return OK;
    case FR_WRITE_PROTECTED:
        estado = MEMORIA_SOMENTE_LEITURA;
        return ERR_SOMENTE_LEITURA;
    case FR_DENIED:
        return ERR_ARQUIVO;

    // Não existe ≠ não deu: um dia sem nada marcado não tem pasta.
    case FR_NO_PATH:
    case FR_NO_FILE:
        return ERR_SEM_PASTA;
    case FR_NO_FILESYSTEM:
        estado = MEMORIA_SEM_FILESYSTEM;
        return ERR_ARQUIVO;
    case FR_INT_ERR:
        estado = MEMORIA_CORROMPIDA;
        return ERR_ARQUIVO;
    case FR_DISK_ERR:
    case FR_NOT_READY:
        estado = MEMORIA_COMUNICACAO;
        return ERR_ARQUIVO;
    default:
        return ERR_ARQUIVO;
    }
}

static bool caminho_fat(char *out, size_t max, const char *caminho)
{
    if (!caminho) return false;
    size_t n_drive = strlen(drive);
    size_t n_caminho = strlen(caminho);
    if (n_drive + n_caminho >= max) return false;
    memcpy(out, drive, n_drive);
    memcpy(out + n_drive, caminho, n_caminho + 1);
    return true;
}

static void copia_nome(char out[40], const char *nome)
{
    size_t n = strlen(nome);
    if (n >= 40) n = 39;
    memcpy(out, nome, n);
    out[n] = '\0';
}

void hal_sd_liga(void)
{
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 1;
    slot.clk = PIN_SD_CLK;
    slot.cmd = PIN_SD_CMD;
    slot.d0 = PIN_SD_D0;
    slot.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    memset(&cartao, 0, sizeof cartao);
    sistema_arquivos = NULL;
    unidade = FF_DRV_NOT_USED;
    drive[0] = '\0';
    estado = MEMORIA_COMUNICACAO;

    esp_err_t e = host.init();
    if (e != ESP_OK) {
        registra("host", e, FR_NOT_READY);
        return;
    }
    e = sdmmc_host_init_slot(host.slot, &slot);
    if (e != ESP_OK) {
        registra("slot", e, FR_NOT_READY);
        return;
    }
    e = sdmmc_card_init(&host, &cartao);
    if (e != ESP_OK) {
        estado = estado_do_esp(e);
        registra("cartao", e, FR_NOT_READY);
        return;
    }
    e = ff_diskio_get_drive(&unidade);
    if (e != ESP_OK) {
        registra("drive", e, FR_INVALID_DRIVE);
        return;
    }
    ff_diskio_register_sdmmc(unidade, &cartao);
    drive[0] = (char)('0' + unidade);
    drive[1] = ':';
    drive[2] = '\0';

    esp_vfs_fat_conf_t vfs = {
        .base_path = RAIZ_VFS,
        .fat_drive = drive,
        .max_files = 6,
    };
    e = esp_vfs_fat_register_cfg(&vfs, &sistema_arquivos);
    if (e != ESP_OK) {
        registra("vfs", e, FR_NOT_ENABLED);
        return;
    }

    FRESULT r = f_mount(sistema_arquivos, drive, 1);
    estado = estado_do_fat(r);
    registra("montagem", ESP_OK, r);
}

memoria_estado_t hal_sd_memoria_estado(void) { return estado; }

erro_t hal_sd_ler(const char *caminho, char *out, size_t max)
{
    if (!caminho || !out || max == 0) return ERR_ARQUIVO;
    char fat[160];
    if (!caminho_fat(fat, sizeof fat, caminho)) return ERR_ARQUIVO;

    FIL arquivo;
    FRESULT r = f_open(&arquivo, fat, FA_READ);
    if (r != FR_OK) return erro_do_fat(r);

    UINT lidos = 0;
    r = f_read(&arquivo, out, (UINT)(max - 1), &lidos);
    out[lidos] = '\0';
    FRESULT fechamento = f_close(&arquivo);
    if (r != FR_OK) return erro_do_fat(r);
    return erro_do_fat(fechamento);
}

static erro_t cria_pais(char *fat)
{
    char *inicio = fat + strlen(drive);
    if (*inicio == '/') inicio++;
    for (char *p = inicio; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        FRESULT r = f_mkdir(fat);
        *p = '/';
        if (r != FR_OK && r != FR_EXIST) return erro_do_fat(r);
    }
    return OK;
}

erro_t hal_sd_escrever(const char *caminho, const char *conteudo)
{
    if (!caminho || !conteudo) return ERR_ARQUIVO;
    char fat[160];
    if (!caminho_fat(fat, sizeof fat, caminho)) return ERR_ARQUIVO;
    erro_t e = cria_pais(fat);
    if (e != OK) return e;

    FIL arquivo;
    FRESULT r = f_open(&arquivo, fat, FA_WRITE | FA_CREATE_ALWAYS);
    if (r != FR_OK) return erro_do_fat(r);

    size_t n = strlen(conteudo);
    UINT escritos = 0;
    r = f_write(&arquivo, conteudo, (UINT)n, &escritos);
    FRESULT sincronizacao = f_sync(&arquivo);
    FRESULT fechamento = f_close(&arquivo);
    if (r != FR_OK) return erro_do_fat(r);
    if (escritos != n) {
        estado = MEMORIA_CHEIA;
        return ERR_CHEIO;
    }
    if (sincronizacao != FR_OK) return erro_do_fat(sincronizacao);
    return erro_do_fat(fechamento);
}

erro_t hal_sd_anexar(const char *caminho, const char *conteudo)
{
    if (!caminho || !conteudo) return ERR_ARQUIVO;
    char fat[160];
    if (!caminho_fat(fat, sizeof fat, caminho)) return ERR_ARQUIVO;
    erro_t e = cria_pais(fat);
    if (e != OK) return e;

    FIL arquivo;
    FRESULT r = f_open(&arquivo, fat, FA_WRITE | FA_OPEN_APPEND);
    if (r != FR_OK) return erro_do_fat(r);
    size_t n = strlen(conteudo);
    UINT escritos = 0;
    r = f_write(&arquivo, conteudo, (UINT)n, &escritos);
    FRESULT sincronizacao = f_sync(&arquivo);
    FRESULT fechamento = f_close(&arquivo);
    if (r != FR_OK) return erro_do_fat(r);
    if (escritos != n) { estado = MEMORIA_CHEIA; return ERR_CHEIO; }
    if (sincronizacao != FR_OK) return erro_do_fat(sincronizacao);
    return erro_do_fat(fechamento);
}

erro_t hal_sd_renomear(const char *de, const char *para)
{
    char origem[160], destino[160];
    if (!caminho_fat(origem, sizeof origem, de) ||
        !caminho_fat(destino, sizeof destino, para))
        return ERR_ARQUIVO;

    FRESULT r = f_rename(origem, destino);

    // FatFs recusa renomear por cima (FR_EXIST); POSIX substitui, e quem chama
    // espera substituição (troca atômica). Sem isto, só a primeira gravação de
    // cada arquivo funcionava. O hal_pc não pega: lá sobrescreve.
    if (r == FR_EXIST) {
        registra("rename_sobre_existente", ESP_OK, r);
        f_unlink(destino);
        r = f_rename(origem, destino);
    }
    return erro_do_fat(r);
}

erro_t hal_sd_apagar(const char *caminho)
{
    char fat[160];
    if (!caminho_fat(fat, sizeof fat, caminho)) return ERR_ARQUIVO;
    FRESULT r = f_unlink(fat);
    if (r != FR_OK) return erro_do_fat(r);
    if (estado == MEMORIA_CHEIA) estado = MEMORIA_PRONTA;
    return OK;
}

erro_t hal_sd_criar_diretorio(const char *caminho)
{
    char fat[160];
    if (!caminho_fat(fat, sizeof fat, caminho)) return ERR_ARQUIVO;
    FRESULT r = f_mkdir(fat);
    if (r == FR_OK) return OK;
    if (r != FR_EXIST) return erro_do_fat(r);

    FILINFO info;
    r = f_stat(fat, &info);
    if (r == FR_OK && (info.fattrib & AM_DIR)) return OK;
    return r == FR_OK ? ERR_ARQUIVO : erro_do_fat(r);
}

erro_t hal_sd_tipo_caminho(const char *caminho, caminho_tipo_t *out)
{
    if (!out) return ERR_ARQUIVO;
    char fat[160];
    if (!caminho_fat(fat, sizeof fat, caminho)) return ERR_ARQUIVO;
    FILINFO info;
    FRESULT r = f_stat(fat, &info);
    if (r == FR_OK) {
        *out = (info.fattrib & AM_DIR) ? CAMINHO_DIRETORIO : CAMINHO_ARQUIVO;
        return OK;
    }
    if (r == FR_NO_FILE || r == FR_NO_PATH) {
        *out = CAMINHO_AUSENTE;
        return OK;
    }
    return erro_do_fat(r);
}

erro_t hal_sd_listar(const char *dir, int desde, char nomes[][40], int max,
                     int *quantos)
{
    if (!quantos || max < 0 || desde < 0) return ERR_ARQUIVO;
    *quantos = 0;
    char fat[160];
    if (!caminho_fat(fat, sizeof fat, dir)) return ERR_ARQUIVO;
    FF_DIR diretorio;
    FRESULT r = f_opendir(&diretorio, fat);
    if (r != FR_OK) return erro_do_fat(r);

    // O FatFs só anda para a frente: a página N relê as N-1 anteriores.
    // ponytail: O(n²) no total — se doer, guardar o `DIR` entre chamadas.
    FILINFO entrada;
    int pulados = 0;
    while (*quantos < max) {
        r = f_readdir(&diretorio, &entrada);
        if (r != FR_OK || entrada.fname[0] == '\0') break;
        if (entrada.fname[0] == '.') continue;
        if (pulados < desde) { pulados++; continue; }
        copia_nome(nomes[*quantos], entrada.fname);
        (*quantos)++;
    }
    FRESULT fechamento = f_closedir(&diretorio);
    if (r != FR_OK) return erro_do_fat(r);
    return erro_do_fat(fechamento);
}

// O total vem do FatFs, não do CSD: interessa o que o sistema de arquivos
// oferece.
erro_t hal_sd_espaco(uint32_t *usado_kb, uint32_t *total_kb)
{
    if (unidade == FF_DRV_NOT_USED || !sistema_arquivos)
        return estado == MEMORIA_AUSENTE ? ERR_SEM_CARTAO : ERR_ARQUIVO;

    DWORD livres_cl = 0;
    FATFS *fs = NULL;
    if (f_getfree(drive, &livres_cl, &fs) != FR_OK || !fs) return ERR_ARQUIVO;

    // FF_MAX_SS é o teto do FatFs (4096), não o setor do cartão (512). Usá-lo
    // multiplicava o cartão por oito.
#if FF_MAX_SS != FF_MIN_SS
    uint64_t setor = fs->ssize;
#else
    uint64_t setor = FF_MAX_SS;
#endif
    uint64_t total_cl = (uint64_t)fs->n_fatent - 2;
    uint64_t total_kb_64 = total_cl * fs->csize * setor / 1024;
    uint64_t livres_kb   = (uint64_t)livres_cl * fs->csize * setor / 1024;

    *total_kb = (uint32_t)total_kb_64;
    *usado_kb = (uint32_t)(total_kb_64 - livres_kb);

    // Na serial porque a tela arredonda em GB, e é no arredondamento que um
    // fator de oito se esconde.
    ESP_LOGI(TAG, "espaco setor=%llu cluster=%u usado=%lu KiB total=%lu KiB",
             (unsigned long long)setor, (unsigned)fs->csize,
             (unsigned long)*usado_kb, (unsigned long)*total_kb);
    return OK;
}

// Soma um diretório recursivamente, com teto de profundidade: a árvore é
// rasa (/TINTO/itens/2026-08-21/0900-fala) e um link maluco não pode estourar
// a pilha.
#define FUNDO_MAX 5

static uint64_t soma_dir(char *fat, int fundo)
{
    if (fundo > FUNDO_MAX) return 0;

    // FF_DIR, não DIR: o ESP-IDF renomeia o tipo do FatFs porque `DIR` já é do
    // dirent.h.
    FF_DIR d;
    if (f_opendir(&d, fat) != FR_OK) return 0;

    uint64_t total = 0;
    size_t   n = strlen(fat);

    for (;;) {
        FILINFO fno;
        if (f_readdir(&d, &fno) != FR_OK || fno.fname[0] == '\0') break;

        int escrito = snprintf(fat + n, 160 - n, "/%s", fno.fname);
        if (escrito <= 0 || (size_t)escrito >= 160 - n) continue;

        if (fno.fattrib & AM_DIR) total += soma_dir(fat, fundo + 1);
        else                      total += fno.fsize;

        fat[n] = '\0';
    }

    f_closedir(&d);
    return total;
}

erro_t hal_sd_uso_de(const char *dir, uint32_t *kb)
{
    if (!kb) return ERR_ARQUIVO;
    *kb = 0;

    if (unidade == FF_DRV_NOT_USED || !sistema_arquivos)
        return estado == MEMORIA_AUSENTE ? ERR_SEM_CARTAO : ERR_ARQUIVO;

    char fat[160];
    if (!caminho_fat(fat, sizeof fat, dir)) return ERR_ARQUIVO;

    *kb = (uint32_t)(soma_dir(fat, 0) / 1024);
    return OK;
}

// ── escrita em fluxo ─────────────────────────────────────────────────
static FIL  fluxo;
static bool fluxo_aberto;
static uint32_t fluxo_bytes;

erro_t hal_sd_fluxo_abre(const char *caminho)
{
    if (fluxo_aberto) return ERR_INTERNO;
    if (unidade == FF_DRV_NOT_USED || !sistema_arquivos)
        return estado == MEMORIA_AUSENTE ? ERR_SEM_CARTAO : ERR_ARQUIVO;

    char fat[160];
    if (!caminho_fat(fat, sizeof fat, caminho)) return ERR_ARQUIVO;
    erro_t e = cria_pais(fat);
    if (e != OK) return e;

    FRESULT r = f_open(&fluxo, fat, FA_WRITE | FA_CREATE_ALWAYS);
    if (r != FR_OK) return erro_do_fat(r);

    fluxo_aberto = true;
    fluxo_bytes  = 0;
    return OK;
}

erro_t hal_sd_fluxo_escreve(const void *dados, size_t n)
{
    if (!fluxo_aberto) return ERR_INTERNO;

    UINT escritos = 0;
    FRESULT r = f_write(&fluxo, dados, (UINT)n, &escritos);
    if (r != FR_OK) return erro_do_fat(r);
    if (escritos != n) return ERR_CHEIO;   // RN-4B: cheio CONTA, não engole

    fluxo_bytes += (uint32_t)escritos;
    return OK;
}

// Para o cabeçalho do WAV, que só se sabe preencher no fim.
erro_t hal_sd_fluxo_escreve_em(uint32_t pos, const void *dados, size_t n)
{
    if (!fluxo_aberto) return ERR_INTERNO;

    FSIZE_t volta = f_tell(&fluxo);
    if (f_lseek(&fluxo, pos) != FR_OK) return ERR_ARQUIVO;

    UINT escritos = 0;
    FRESULT r = f_write(&fluxo, dados, (UINT)n, &escritos);
    f_lseek(&fluxo, volta);

    if (r != FR_OK) return erro_do_fat(r);
    return escritos == n ? OK : ERR_CHEIO;
}

uint32_t hal_sd_fluxo_bytes(void) { return fluxo_bytes; }

erro_t hal_sd_fluxo_fecha(void)
{
    if (!fluxo_aberto) return OK;
    FRESULT r = f_close(&fluxo);
    fluxo_aberto = false;
    return r == FR_OK ? OK : erro_do_fat(r);
}

// ── leitura em fluxo ─────────────────────────────────────────────────
// FIL separado da escrita: um bug num caminho não corrompe o outro.
static FIL  leitura;
static bool leitura_aberta;

erro_t hal_sd_leitura_abre(const char *caminho)
{
    if (leitura_aberta) return ERR_INTERNO;
    if (unidade == FF_DRV_NOT_USED || !sistema_arquivos)
        return estado == MEMORIA_AUSENTE ? ERR_SEM_CARTAO : ERR_ARQUIVO;

    char fat[160];
    if (!caminho_fat(fat, sizeof fat, caminho)) return ERR_ARQUIVO;

    FRESULT r = f_open(&leitura, fat, FA_READ);
    if (r != FR_OK) return erro_do_fat(r);

    leitura_aberta = true;
    return OK;
}

// ── a porta estreita do áudio ────────────────────────────────────────
// `nuvem_esp.c` não pode incluir `hal_sd.h` (o lwip declara `OK` e
// `ERR_TIMEOUT`, como o `erro_t`). Estas quatro são a mesma leitura em fluxo
// com tipos primitivos.
int hal_nuvem_audio_abre(const char *caminho)
{
    return hal_sd_leitura_abre(caminho) == OK ? 0 : -1;
}

unsigned hal_nuvem_audio_tamanho(void)
{
    return (unsigned)hal_sd_leitura_tamanho();
}

int hal_nuvem_audio_le(void *dados, size_t n)
{
    return hal_sd_leitura_le(dados, n);
}

void hal_nuvem_audio_fecha(void)
{
    (void)hal_sd_leitura_fecha();
}

uint32_t hal_sd_leitura_tamanho(void)
{
    // `f_size` lê o que o `f_open` já preencheu: não acessa o cartão. O
    // multipart precisa do `Content-Length` antes do primeiro byte.
    return leitura_aberta ? (uint32_t)f_size(&leitura) : 0;
}

int hal_sd_leitura_le(void *dados, size_t n)
{
    if (!leitura_aberta) return -1;

    UINT lidos = 0;
    if (f_read(&leitura, dados, (UINT)n, &lidos) != FR_OK) return -1;
    return (int)lidos;
}

erro_t hal_sd_leitura_fecha(void)
{
    if (!leitura_aberta) return OK;
    FRESULT r = f_close(&leitura);
    leitura_aberta = false;
    return r == FR_OK ? OK : erro_do_fat(r);
}

erro_t hal_sd_formatar(void)
{
    if (unidade == FF_DRV_NOT_USED || !sistema_arquivos)
        return estado == MEMORIA_AUSENTE ? ERR_SEM_CARTAO : ERR_ARQUIVO;

    f_mount(NULL, drive, 0);

    // FM_FAT sozinho é FAT16, que não passa de ~4 GB: num cartão de 7,7 GB o
    // f_mkfs abortava sempre. Com os dois, o FatFs escolhe pelo tamanho; exFAT
    // está desligado, e FAT32 é o que qualquer computador lê.
    MKFS_PARM opcoes = { .fmt = FM_FAT | FM_FAT32 | FM_SFD, .n_fat = 2 };
    FRESULT r = f_mkfs(drive, &opcoes, trabalho, sizeof trabalho);
    registra("formatacao", ESP_OK, r);
    if (r != FR_OK) return erro_do_fat(r);

    r = f_mount(sistema_arquivos, drive, 1);
    registra("remontagem", ESP_OK, r);
    return erro_do_fat(r);
}

void hal_sd_aleatorio(uint8_t *out, size_t n)
{
    if (out && n) esp_fill_random(out, n);
}
