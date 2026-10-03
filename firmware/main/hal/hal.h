// hal/hal.h — a fronteira com o hardware, por PAPEL e nunca por peça.
//
// Uma struct de ponteiros de função entregue ao app na partida: na placa,
// hal_esp.c preenche com ESP-IDF; no PC, hal_pc.c simula. É o que deixa o
// aparelho inteiro rodar no PC.
#ifndef HAL_H
#define HAL_H

#include "rota.h"
#include "../nucleo/tipos.h"
#include "../nucleo/inicializacao.h"

// A política de refresh: decisão de experiência, compila e tem teste no PC.
#include "refresco.h"

// ── entrada ──────────────────────────────────────────────────────────
// O papel entrega IN_CIMA, nunca "pino 12 baixou". Oito botões vêm do PCF8575
// numa leitura de dois bytes; só IN_POWER é GPIO direto. O INT do PCF acorda
// o chip sem desbloqueá-lo.
typedef enum {
    IN_NADA = 0,
    IN_CIMA, IN_BAIXO, IN_ESQ, IN_DIR,
    IN_OK,
    IN_MENU,       // a gaveta, e nada mais — RN-3F
    IN_VOLTAR,     // BACK: volta um nível; segurado, volta pra home — RN-38
    IN_VOZ,
    IN_POWER,      // clique dorme/acorda; segurado desliga
} entrada_t;

// O que atravessa a fila de eventos. Nenhum evento carrega ponteiro para
// memória de outra task.
typedef enum {
    EV_NADA = 0,
    EV_BOTAO_APERTO,   // borda física; hoje só a voz distingue apertar/soltar
    EV_BOTAO,          // botao + ms (ms > 0 = foi segurado)
    EV_TICK,           // 1 s — relógio, tempo de gravação, timeouts
    EV_DOCADO,         // o hall mudou; valor = 1 docado, 0 na mão
    EV_AUDIO_FIM,      // WAV fechado; valor = duração em ms
    EV_REDE_PULL,
    EV_REDE_PUSH,
    EV_REDE_RESULTADO,
    EV_OTA,
    EV_WIFI_REDES,        // a varredura terminou; a lista está pronta
    EV_WIFI_ESTADO,       // conectou, caiu, ou ganhou endereço
    EV_HORA_DA_REDE,      // o relógio foi acertado por NTP
    EV_ACORDOU_BLOQUEADO, // wake pelo INT do PCF; botao diz a origem
} evento_tipo_t;

typedef struct {
    evento_tipo_t tipo;
    entrada_t     botao;
    int32_t       ms;      // quanto tempo o botão ficou pressionado
    int32_t       valor;
} evento_t;

// ── o contrato ───────────────────────────────────────────────────────
typedef struct {
    // entrada — devolve false quando não há evento pendente
    bool (*proximo_evento)(evento_t *out);

    // tela — recebe o bitmap pronto e pinta. A intenção do quadro (`refresco.h`)
    // é a única informação que não está no bitmap: trocar de tela e rolar uma
    // lista mudam tudo, e pedem refreshes opostos.
    void (*mostrar)(const uint8_t *bits, int l, int a, pintura_t intencao);

    // cartão — o único lugar que conhece caminho de arquivo
    erro_t (*ler)(const char *caminho, char *out, size_t max);
    erro_t (*escrever)(const char *caminho, const char *conteudo);
    erro_t (*anexar)(const char *caminho, const char *conteudo);
    // ── os segredos, fora do cartão ─────────────────────────────────────
    // Senha do Wi-Fi, prova e token ficam na NVS (flash soldada), não no microSD,
    // que sai com a unha. Não é cofre — sem criptografia de flash, um dump lê —
    // mas dessoldar é outra ordem de ataque. Chave ausente devolve ERR_ARQUIVO
    // com destino zerado: é o estado normal.
    erro_t (*segredo_grava)(const char *chave, const char *valor);
    erro_t (*segredo_le)(const char *chave, char *out, size_t max);
    erro_t (*segredo_apaga)(const char *chave);

    erro_t (*apagar)(const char *caminho);
    // `desde` é o cursor: quantas entradas pular. Sem ele, um diretório maior
    // que `max` não tinha segunda página.
    erro_t (*listar)(const char *dir, int desde, char nomes[][40], int max,
                     int *quantos);
    memoria_estado_t (*memoria_estado)(void);

    // ── buffer grande, e de onde ele vem ─────────────────────────────────
    // Buffer grande e temporário vem da PSRAM e é devolvido (`memoria_hal.h`); no
    // PC, do malloc. Um static grande em app/ prende RAM interna e tira o buffer
    // de DMA do cartão — o aparelho abre a tela de reparo sem nada quebrado.
    void *(*emprestar)(size_t desejado, size_t minimo, size_t *real);
    void  (*devolver)(void *p);
    erro_t (*criar_diretorio)(const char *caminho);
    erro_t (*tipo_caminho)(const char *caminho, caminho_tipo_t *out);
    erro_t (*formatar_memoria)(void);

    // Apaga a credencial de Wi-Fi guardada no RÁDIO (NVS), que sobrevive a
    // formatar o cartão. "Apagar e preparar" promete um aparelho novo, sem a
    // senha da casa do dono anterior.
    void   (*esquecer_rede)(void);
    void   (*aleatorio)(uint8_t *out, size_t n);

    // O MAC do eFuse ("AA:BB:CC:44:55:66"): não muda nunca, e identifica o
    // aparelho no log do servidor.
    void   (*id_aparelho)(char *out, size_t max);

    // Em KiB. Quem não consegue medir devolve erro: "0 de 0" parece cartão
    // cheio.
    erro_t (*espaco)(uint32_t *usado_kb, uint32_t *total_kb);

    // Quanto uma pasta ocupa, recursivo. Varre a árvore: só a tela de
    // Armazenamento chama.
    erro_t (*uso_de)(const char *dir, uint32_t *kb);

    // RN-64: escrita atômica é .tmp + rename; nunca existe meta.json pela
    // metade. O papel existe para nenhuma camada esquecer disso.
    erro_t (*renomear)(const char *de, const char *para);

    // resto
    bool     (*docado)(void);
    void     (*relogio)(data_t *d, int *hora, int *minuto);

    // RN-6G: sem NTP, o RTC conta de um zero arbitrário. Ajustar à mão é o que
    // transforma isso em hora.
    void     (*ajustar_relogio)(data_t d, int hora, int minuto);
    uint32_t (*agora_ms)(void);
    void     (*reiniciar)(void);  // retorna no PC; na placa não volta

    // Diagnóstico na serial (ESP_LOGI na placa; nada no PC). Só estado, nunca
    // conteúdo pessoal.
    void     (*registrar)(const char *assunto, const char *texto);

    // ── áudio ────────────────────────────────────────────────────────────
    // O papel é GRAVAR. RN-13: os trechos de uma sessão vão para o MESMO arquivo,
    // por isso `retomar` existe e não é um segundo `iniciar`.
    erro_t (*audio_inicia)(const char *caminho);
    erro_t (*audio_pausa)(void);
    erro_t (*audio_retoma)(void);
    erro_t (*audio_fecha)(int *dur_s, int *trechos);
    erro_t (*audio_descarta)(void);


    // ── o rádio ──────────────────────────────────────────────────────────
    // `wifi_procurar` só pede: a lista pronta chega como EV_WIFI_REDES, e só
    // então `wifi_redes` copia. Bloquear congelaria a tela.
    void   (*wifi_procurar)(void);
    int    (*wifi_redes)(rede_wifi_t *out, int max);
    erro_t (*wifi_conectar)(const char *nome, const char *senha);

    // Onde o rádio está (`rede_t` como int: o hal não conhece o estado). O IP
    // é a diferença entre associado e funcionando; a força é do AP atual.
    int    (*wifi_estado)(char *ip, size_t max, int *forca);

    // Por que a última tentativa caiu: 0 nenhuma, 1 senha, 2 sem resposta. Um
    // hal que não distingue devolve 0.
    int    (*wifi_falha)(void);

    // Pede a hora à rede. Quem decide SE pede é o app (AJUSTE_HORA_REDE).
    void   (*hora_da_rede)(void);

    // Derruba o cliente NTP quando a pessoa desliga a hora pela rede; deixá-lo
    // vivo fazia o próximo religar abortar o aparelho.
    void   (*hora_da_rede_para)(void);

    // Aplica o fuso (minutos do UTC). Sem ele, `localtime_r` devolve UTC com cara
    // de hora certa.
    void   (*fuso)(int minutos);

    // A carga, 0-100; -1 quando não há como saber. "Não sei" não pode virar
    // "cheia".
    int    (*bateria)(void);

    // ── a nuvem ──────────────────────────────────────────────────────────
    // Sempre assíncrona (RN-41): o app pede, segue desenhando, e a resposta chega
    // como EV_REDE_RESULTADO. `corpo` nulo é GET; com corpo, POST. `operacao` é o
    // id que torna o retry seguro, e nasce no device: gerado no servidor, todo
    // retry seria operação nova. Nulo quando a chamada não muda nada. A rota
    // cabe em NUVEM_ROTA_MAX bytes, com a query.
    void   (*nuvem_pede)(const char *rota, const char *corpo,
                         const char *operacao);

    // O áudio do cartão vira multipart, e quem o lê é o hal: uso/ não abre
    // arquivo.
    void   (*nuvem_pede_audio)(const char *rota, const char *caminho_wav,
                               const char *operacao);

    // O endereço e o token, uma vez, depois de ler o cartão. O token não passeia
    // pelas camadas de cima.
    void   (*nuvem_credencial)(const char *servidor, const char *token);

    // O corpo da última resposta. ERR_REDE quando não houve, ou quando falhou
    // (inclui o servidor dizendo não).
    erro_t (*nuvem_resposta)(char *out, size_t max);

    // De qual LINHA veio a resposta lida: 0 = agora (gestos, catálogo, captura);
    // 1 = o pull, pendurado no long polling. As duas podem estar em voo juntas.
    int    (*nuvem_linha)(void);

    // O código HTTP da última resposta (0 = nem houve). É o que separa "sem
    // minutos", "reconecte a conta" e "token recusado", que pedem ações
    // diferentes.
    int    (*nuvem_codigo)(void);
} hal_t;

#endif
