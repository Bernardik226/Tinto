#include "teste.h"
#include "dado/memoria.h"

void t_memoria_distingue_ausencia_de_filesystem(void)
{
    COMECA("RN-6A · ausência e filesystem inválido não são o mesmo estado");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    pc_memoria_estado(MEMORIA_AUSENTE);
    ESPERA_IGUAL(hal->memoria_estado(), MEMORIA_AUSENTE);
    pc_memoria_estado(MEMORIA_SEM_FILESYSTEM);
    ESPERA_IGUAL(hal->memoria_estado(), MEMORIA_SEM_FILESYSTEM);
    TERMINA();
}

void t_memoria_distingue_diretorio_vazio_de_falha(void)
{
    COMECA("diretório vazio devolve zero; falha devolve erro");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    char nomes[2][40];
    int quantos = -1;

    ESPERA_IGUAL(hal->criar_diretorio("/TINTO"), OK);
    ESPERA_IGUAL(hal->listar("/TINTO", 0, nomes, 2, &quantos), OK);
    ESPERA_IGUAL(quantos, 0);

    pc_falhar_listar(true);
    ESPERA_IGUAL(hal->listar("/TINTO", 0, nomes, 2, &quantos), ERR_ARQUIVO);
    TERMINA();
}

void t_memoria_informa_tipo_do_caminho(void)
{
    COMECA("caminho distingue ausência, arquivo e diretório");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    caminho_tipo_t tipo;

    ESPERA_IGUAL(hal->tipo_caminho("/TINTO", &tipo), OK);
    ESPERA_IGUAL(tipo, CAMINHO_AUSENTE);
    ESPERA_IGUAL(hal->criar_diretorio("/TINTO"), OK);
    ESPERA_IGUAL(hal->tipo_caminho("/TINTO", &tipo), OK);
    ESPERA_IGUAL(tipo, CAMINHO_DIRETORIO);
    ESPERA_IGUAL(hal->escrever("/TINTO/a.txt", "a"), OK);
    ESPERA_IGUAL(hal->tipo_caminho("/TINTO/a.txt", &tipo), OK);
    ESPERA_IGUAL(tipo, CAMINHO_ARQUIVO);
    TERMINA();
}

void t_memoria_injeta_falha_ao_criar_diretorio(void)
{
    COMECA("falha de diretório pode ser roteirizada no PC");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    pc_falhar_criar_diretorio(true);
    ESPERA_IGUAL(hal->criar_diretorio("/TINTO"), ERR_ARQUIVO);
    TERMINA();
}

void t_memoria_lista_diretorio_vazio_criado(void)
{
    COMECA("diretório criado aparece na listagem do pai");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    char nomes[2][40];
    int quantos = 0;

    ESPERA_IGUAL(hal->criar_diretorio("/TINTO"), OK);
    ESPERA_IGUAL(hal->criar_diretorio("/TINTO/itens"), OK);
    ESPERA_IGUAL(hal->listar("/TINTO", 0, nomes, 2, &quantos), OK);
    ESPERA_IGUAL(quantos, 1);
    ESPERA_TEXTO(nomes[0], "itens");
    TERMINA();
}

void t_memoria_nao_confunde_prefixo_com_diretorio_pai(void)
{
    COMECA("/TINTO2 não é filho de /TINTO");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    char nomes[2][40];
    int quantos = -1;

    ESPERA_IGUAL(hal->criar_diretorio("/TINTO"), OK);
    // A pasta vizinha existe de verdade: o hal do PC recusa escrever em pasta
    // inexistente, como o cartão.
    ESPERA_IGUAL(hal->criar_diretorio("/TINTO2"), OK);
    ESPERA_IGUAL(hal->escrever("/TINTO2/fora.txt", "fora"), OK);
    ESPERA_IGUAL(hal->listar("/TINTO", 0, nomes, 2, &quantos), OK);
    ESPERA_IGUAL(quantos, 0);
    TERMINA();
}

void t_memoria_cheia_e_observavel_e_apagar_recupera(void)
{
    COMECA("memória cheia é observável; apagar recupera espaço");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();

    char caminho[32];
    for (int i = 0; i < 64; i++) {
        snprintf(caminho, sizeof caminho, "/arquivo-%02d", i);
        ESPERA_IGUAL(hal->escrever(caminho, "x"), OK);
    }

    ESPERA_IGUAL(hal->memoria_estado(), MEMORIA_PRONTA);
    ESPERA_IGUAL(hal->renomear("/arquivo-00", "/nao-cabe"), ERR_CHEIO);
    ESPERA_IGUAL(hal->memoria_estado(), MEMORIA_CHEIA);
    ESPERA(pc_tem_arquivo("/arquivo-00"));

    ESPERA_IGUAL(hal->apagar("/arquivo-00"), OK);
    ESPERA_IGUAL(hal->memoria_estado(), MEMORIA_PRONTA);
    ESPERA_IGUAL(hal->escrever("/agora-cabe", "x"), OK);
    ESPERA_IGUAL(hal->escrever("/continua-sem-caber", "x"), ERR_CHEIO);
    ESPERA_IGUAL(hal->memoria_estado(), MEMORIA_CHEIA);

    hal = pc_liga();
    pc_memoria_virgem();
    // Pasta tem teto próprio no falso: duas por item.
    for (int i = 0; i < 256; i++) {
        snprintf(caminho, sizeof caminho, "/diretorio-%03d", i);
        ESPERA_IGUAL(hal->criar_diretorio(caminho), OK);
    }
    ESPERA_IGUAL(hal->criar_diretorio("/diretorio-extra"), ERR_CHEIO);
    ESPERA_IGUAL(hal->memoria_estado(), MEMORIA_CHEIA);

    TERMINA();
}

void t_memoria_cheia_ou_somente_leitura_continua_legivel(void)
{
    COMECA("memória cheia ou somente leitura continua legível");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    ESPERA_IGUAL(hal->criar_diretorio("/TINTO"), OK);
    ESPERA_IGUAL(hal->escrever("/TINTO/item.txt", "conteudo"), OK);

    memoria_estado_t estados[] = {
        MEMORIA_CHEIA,
        MEMORIA_SOMENTE_LEITURA,
    };
    for (size_t i = 0; i < sizeof estados / sizeof estados[0]; i++) {
        pc_memoria_estado(estados[i]);

        char conteudo[16];
        ESPERA_IGUAL(hal->ler("/TINTO/item.txt", conteudo,
                              sizeof conteudo), OK);
        ESPERA_TEXTO(conteudo, "conteudo");

        caminho_tipo_t tipo = CAMINHO_AUSENTE;
        ESPERA_IGUAL(hal->tipo_caminho("/TINTO/item.txt", &tipo), OK);
        ESPERA_IGUAL(tipo, CAMINHO_ARQUIVO);

        char nomes[2][40];
        int quantos = -1;
        ESPERA_IGUAL(hal->listar("/TINTO", 0, nomes, 2, &quantos), OK);
        ESPERA_IGUAL(quantos, 1);
        ESPERA_TEXTO(nomes[0], "item.txt");
    }

    TERMINA();
}

void t_mutacoes_respeitam_estado_da_memoria(void)
{
    COMECA("apagar, renomear e formatar respeitam o estado da memória");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    ESPERA_IGUAL(hal->escrever("/origem", "x"), OK);

    pc_memoria_estado(MEMORIA_SOMENTE_LEITURA);
    ESPERA_IGUAL(hal->apagar("/origem"), ERR_SOMENTE_LEITURA);
    ESPERA_IGUAL(hal->renomear("/origem", "/destino"),
                 ERR_SOMENTE_LEITURA);
    ESPERA_IGUAL(hal->formatar_memoria(), ERR_SOMENTE_LEITURA);
    ESPERA(pc_tem_arquivo("/origem"));

    pc_memoria_estado(MEMORIA_COMUNICACAO);
    ESPERA_IGUAL(hal->apagar("/origem"), ERR_ARQUIVO);
    ESPERA_IGUAL(hal->renomear("/origem", "/destino"), ERR_ARQUIVO);
    ESPERA_IGUAL(hal->formatar_memoria(), ERR_ARQUIVO);
    ESPERA(pc_tem_arquivo("/origem"));

    pc_memoria_estado(MEMORIA_AUSENTE);
    ESPERA_IGUAL(hal->apagar("/origem"), ERR_SEM_CARTAO);
    ESPERA_IGUAL(hal->renomear("/origem", "/destino"), ERR_SEM_CARTAO);
    ESPERA_IGUAL(hal->formatar_memoria(), ERR_SEM_CARTAO);

    pc_memoria_estado(MEMORIA_SEM_FILESYSTEM);
    ESPERA_IGUAL(hal->formatar_memoria(), OK);
    ESPERA_IGUAL(hal->memoria_estado(), MEMORIA_PRONTA);
    ESPERA(!pc_tem_arquivo("/origem"));

    TERMINA();
}

// ── a árvore /TINTO/ ────────────────────────────────────────────────
// RN-6B: FAT válido sem /TINTO/ é provisionado sem formatar. Danificada ou
// de versão futura vira recuperação, nunca conserto calado.

void t_arvore_ausente_e_virgem(void)
{
    COMECA("RN-6B · /TINTO/ ausente é virgem, não danificada");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    arvore_estado_t estado = ARVORE_DANIFICADA;
    ESPERA_IGUAL(memoria_inspeciona(hal, &estado), OK);
    ESPERA_IGUAL(estado, ARVORE_VIRGEM);
    TERMINA();
}

void t_arvore_sem_formato_e_danificada(void)
{
    COMECA("/TINTO/ COM conteúdo e sem formato.json é danificada");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    ESPERA_IGUAL(hal->criar_diretorio("/TINTO"), OK);
    // Com conteúdo dentro: estrutura quebrada, não provisionamento parcial.
    pc_poe_arquivo("/TINTO/itens/2026-08-20/x/meta.json", "{}");

    arvore_estado_t estado = ARVORE_PRONTA;
    ESPERA_IGUAL(memoria_inspeciona(hal, &estado), OK);
    ESPERA_IGUAL(estado, ARVORE_DANIFICADA);
    TERMINA();
}

void t_arvore_com_raiz_de_arquivo_e_danificada(void)
{
    COMECA("/TINTO como arquivo é danificada, nunca provisionada por cima");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    pc_poe_arquivo("/TINTO", "isto nao e um diretorio");

    arvore_estado_t estado = ARVORE_PRONTA;
    ESPERA_IGUAL(memoria_inspeciona(hal, &estado), OK);
    ESPERA_IGUAL(estado, ARVORE_DANIFICADA);
    TERMINA();
}

void t_arvore_de_versao_futura_nao_e_tocada(void)
{
    COMECA("RN-63 · formato futuro é reconhecido, e não sobrescrito");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    ESPERA_IGUAL(hal->criar_diretorio("/TINTO"), OK);
    ESPERA_IGUAL(hal->criar_diretorio("/TINTO/sistema"), OK);
    pc_poe_arquivo("/TINTO/sistema/formato.json", "{\"v\":99}");

    arvore_estado_t estado = ARVORE_PRONTA;
    ESPERA_IGUAL(memoria_inspeciona(hal, &estado), OK);
    ESPERA_IGUAL(estado, ARVORE_FORMATO_FUTURO);

    // Preparar uma árvore desconhecida apagaria o trabalho de outra versão.
    ESPERA_IGUAL(memoria_prepara(hal), ERR_FORMATO);
    char buf[32];
    ESPERA_IGUAL(hal->ler("/TINTO/sistema/formato.json", buf, sizeof buf), OK);
    ESPERA_TEXTO(buf, "{\"v\":99}");
    TERMINA();
}

void t_arvore_sem_uma_pasta_e_parcial(void)
{
    COMECA("formato atual com pasta faltando é parcial, e é reparável");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    ESPERA_IGUAL(hal->criar_diretorio("/TINTO"), OK);
    ESPERA_IGUAL(hal->criar_diretorio("/TINTO/sistema"), OK);
    ESPERA_IGUAL(hal->criar_diretorio("/TINTO/itens"), OK);
    ESPERA_IGUAL(hal->criar_diretorio("/TINTO/acervo"), OK);
    pc_poe_arquivo("/TINTO/sistema/formato.json", "{\"v\":1}");

    arvore_estado_t estado = ARVORE_PRONTA;
    ESPERA_IGUAL(memoria_inspeciona(hal, &estado), OK);
    ESPERA_IGUAL(estado, ARVORE_PARCIAL);   // falta /TINTO/entrada

    ESPERA_IGUAL(memoria_prepara(hal), OK);
    ESPERA_IGUAL(memoria_inspeciona(hal, &estado), OK);
    ESPERA_IGUAL(estado, ARVORE_PRONTA);
    TERMINA();
}

void t_prepara_nao_toca_no_que_esta_fora_da_raiz(void)
{
    COMECA("RN-61 · provisionar não lê, não move e não apaga fora de /TINTO/");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    pc_poe_arquivo("/fotos/nao-tocar.jpg", "JFIF-conteudo-alheio");
    pc_poe_arquivo("/leiame.txt", "documento do dono do cartao");

    ESPERA_IGUAL(memoria_prepara(hal), OK);

    char buf[64];
    ESPERA_IGUAL(hal->ler("/fotos/nao-tocar.jpg", buf, sizeof buf), OK);
    ESPERA_TEXTO(buf, "JFIF-conteudo-alheio");
    ESPERA_IGUAL(hal->ler("/leiame.txt", buf, sizeof buf), OK);
    ESPERA_TEXTO(buf, "documento do dono do cartao");

    arvore_estado_t estado = ARVORE_VIRGEM;
    ESPERA_IGUAL(memoria_inspeciona(hal, &estado), OK);
    ESPERA_IGUAL(estado, ARVORE_PRONTA);
    TERMINA();
}

void t_prepara_propaga_falha_de_cada_operacao(void)
{
    COMECA("provisionar propaga a falha de criar, escrever, reler e renomear");

    operacao_memoria_t ops[] = {
        OP_MEM_CRIAR_DIR,
        OP_MEM_ESCREVER,
        OP_MEM_LER,
        OP_MEM_RENOMEAR,
    };
    for (size_t i = 0; i < sizeof ops / sizeof ops[0]; i++) {
        const hal_t *hal = pc_liga();
    pc_memoria_virgem();
        pc_falhar_operacao(ops[i], ERR_ARQUIVO);
        ESPERA_IGUAL(memoria_prepara(hal), ERR_ARQUIVO);
    }
    TERMINA();
}

void t_formato_anterior_sobrevive_ao_rename_falho(void)
{
    COMECA("RN-64 · rename que falha deixa o formato.json anterior intacto");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    ESPERA_IGUAL(hal->criar_diretorio("/TINTO"), OK);
    ESPERA_IGUAL(hal->criar_diretorio("/TINTO/sistema"), OK);
    pc_poe_arquivo("/TINTO/sistema/formato.json", "{\"v\":1}");

    pc_falhar_operacao(OP_MEM_RENOMEAR, ERR_ARQUIVO);
    ESPERA_IGUAL(memoria_prepara(hal), ERR_ARQUIVO);

    // O .tmp pode ter ficado; o definitivo não pode ter virado lixo.
    char buf[32];
    ESPERA_IGUAL(hal->ler("/TINTO/sistema/formato.json", buf, sizeof buf), OK);
    ESPERA_TEXTO(buf, "{\"v\":1}");
    TERMINA();
}

void t_prova_de_escrita_nao_deixa_residuo(void)
{
    COMECA("a prova de escrita grava, relê e não deixa prova.tmp para trás");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    ESPERA_IGUAL(memoria_prepara(hal), OK);
    ESPERA_IGUAL(memoria_prova_escrita(hal), OK);
    ESPERA(!pc_tem_arquivo("/TINTO/sistema/prova.tmp"));
    ESPERA(!pc_tem_arquivo("/TINTO/sistema/prova"));
    TERMINA();
}

void t_prova_de_escrita_limpa_residuo_do_boot_anterior(void)
{
    COMECA("prova.tmp herdada de um corte anterior some no boot seguinte");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    ESPERA_IGUAL(memoria_prepara(hal), OK);
    pc_poe_arquivo("/TINTO/sistema/prova.tmp", "corte no meio da escrita");

    ESPERA_IGUAL(memoria_prova_escrita(hal), OK);
    ESPERA(!pc_tem_arquivo("/TINTO/sistema/prova.tmp"));
    TERMINA();
}

void t_prova_de_escrita_denuncia_memoria_travada(void)
{
    COMECA("somente leitura e cheia aparecem na prova, não viram ausência");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    ESPERA_IGUAL(memoria_prepara(hal), OK);

    pc_memoria_estado(MEMORIA_SOMENTE_LEITURA);
    ESPERA_IGUAL(memoria_prova_escrita(hal), ERR_SOMENTE_LEITURA);

    pc_memoria_estado(MEMORIA_CHEIA);
    ESPERA_IGUAL(memoria_prova_escrita(hal), ERR_CHEIO);
    TERMINA();
}

void t_arvore_vazia_e_virgem_e_nao_reparo(void)
{
    COMECA("/TINTO/ vazia é provisionamento inacabado, não estrutura quebrada");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();

    // O que o LFN desligado deixava: a pasta criada e o formato.json nunca
    // escrito.
    ESPERA_IGUAL(hal->criar_diretorio("/TINTO"), OK);
    ESPERA_IGUAL(hal->criar_diretorio("/TINTO/itens"), OK);
    ESPERA_IGUAL(hal->criar_diretorio("/TINTO/sistema"), OK);

    arvore_estado_t estado = ARVORE_DANIFICADA;
    ESPERA_IGUAL(memoria_inspeciona(hal, &estado), OK);
    ESPERA_IGUAL(estado, ARVORE_VIRGEM);

    ESPERA_IGUAL(memoria_prepara(hal), OK);
    ESPERA_IGUAL(memoria_inspeciona(hal, &estado), OK);
    ESPERA_IGUAL(estado, ARVORE_PRONTA);
    TERMINA();
}

void t_arvore_com_arquivo_alheio_continua_danificada(void)
{
    COMECA("um arquivo dentro de /TINTO/ já é conteúdo: volta a ser reparo");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    ESPERA_IGUAL(hal->criar_diretorio("/TINTO"), OK);
    pc_poe_arquivo("/TINTO/algo.txt", "conteudo de alguem");

    arvore_estado_t estado = ARVORE_VIRGEM;
    ESPERA_IGUAL(memoria_inspeciona(hal, &estado), OK);
    ESPERA_IGUAL(estado, ARVORE_DANIFICADA);
    TERMINA();
}
