#include "teste.h"
#include "dado/perfil.h"
#include "dado/memoria.h"
#include <string.h>

// RN-6D: o perfil acompanha a memória.

static const hal_t *memoria_pronta(void)
{
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    memoria_prepara(hal);
    return hal;
}

void t_perfil_ausente_nao_e_formato_invalido(void)
{
    COMECA("cartão sem perfil.json é primeiro uso, não arquivo corrompido");
    const hal_t *hal = memoria_pronta();
    perfil_local_t p;
    ESPERA_IGUAL(perfil_carrega(hal, &p), ERR_ARQUIVO);
    ESPERA_TEXTO(p.proprietario_id, "");
    ESPERA_IGUAL(p.onboarding_v, 0);
    TERMINA();
}

void t_perfil_grava_e_rele_o_schema_exato(void)
{
    COMECA("RN-6D · perfil.json grava o schema normativo e volta igual");
    const hal_t *hal = memoria_pronta();

    perfil_local_t p;
    memset(&p, 0, sizeof p);
    snprintf(p.proprietario_id, sizeof p.proprietario_id,
             "0123456789abcdef0123456789abcdef");
    snprintf(p.nome, sizeof p.nome, "Usuário");
    p.onboarding_v = 1;
    ESPERA_IGUAL(perfil_grava(hal, &p), OK);

    char cru[256];
    ESPERA_IGUAL(hal->ler("/TINTO/sistema/perfil.json", cru, sizeof cru), OK);
    ESPERA_TEXTO(cru,
        "{\"v\":1,\"proprietario_id\":\"0123456789abcdef0123456789abcdef\","
        "\"nome\":\"Usuário\",\"nome_sobe\":0,\"onboarding_v\":1,"
        "\"relogio_v\":0}");

    perfil_local_t lido;
    ESPERA_IGUAL(perfil_carrega(hal, &lido), OK);
    ESPERA_TEXTO(lido.proprietario_id, p.proprietario_id);
    ESPERA_TEXTO(lido.nome, "Usuário");
    ESPERA_IGUAL(lido.onboarding_v, 1);
    TERMINA();
}

void t_perfil_nao_regenera_id_de_cartao_que_ja_tem_dono(void)
{
    COMECA("RN-6D · trocar o nome não troca o proprietário");
    const hal_t *hal = memoria_pronta();

    perfil_local_t p;
    memset(&p, 0, sizeof p);
    perfil_novo_id(hal, p.proprietario_id);
    snprintf(p.nome, sizeof p.nome, "Usuário");
    ESPERA_IGUAL(perfil_grava(hal, &p), OK);

    char id_original[33];
    snprintf(id_original, sizeof id_original, "%s", p.proprietario_id);

    perfil_local_t depois;
    ESPERA_IGUAL(perfil_carrega(hal, &depois), OK);
    snprintf(depois.nome, sizeof depois.nome, "Convidado");
    ESPERA_IGUAL(perfil_grava(hal, &depois), OK);

    perfil_local_t final;
    ESPERA_IGUAL(perfil_carrega(hal, &final), OK);
    ESPERA_TEXTO(final.proprietario_id, id_original);
    ESPERA_TEXTO(final.nome, "Convidado");
    TERMINA();
}

void t_perfil_id_tem_32_hex(void)
{
    COMECA("o proprietário nasce com 32 hex, e dois cartões não colidem");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();
    char a[33], b[33];
    perfil_novo_id(hal, a);
    perfil_novo_id(hal, b);

    ESPERA_IGUAL((int)strlen(a), 32);
    for (int i = 0; i < 32; i++)
        ESPERA((a[i] >= '0' && a[i] <= '9') || (a[i] >= 'a' && a[i] <= 'f'));
    ESPERA(strcmp(a, b) != 0);
    TERMINA();
}

void t_perfil_trunca_nome_sem_partir_utf8(void)
{
    COMECA("RN-B7 · nome longo trunca em caractere inteiro, nunca no acento");
    char out[NOME_UTF8_MAX];

    // 30 caracteres de dois bytes: contar bytes cortaria no meio de um.
    const char *acentos =
        "áááááááááááááááááááááááááááááá";
    ESPERA_IGUAL(perfil_nome_trunca(acentos, out, sizeof out),
                 NOME_CARACTERES_MAX);
    ESPERA_IGUAL((int)strlen(out), NOME_CARACTERES_MAX * 2);

    // O último byte não é continuação órfã.
    ESPERA(((unsigned char)out[strlen(out) - 1] & 0xC0) == 0x80);
    ESPERA(((unsigned char)out[strlen(out) - 2] & 0xE0) == 0xC0);

    ESPERA_IGUAL(perfil_nome_trunca("Usuário", out, sizeof out), 7);
    ESPERA_TEXTO(out, "Usuário");
    TERMINA();
}

void t_perfil_rename_falho_nao_deixa_perfil_pela_metade(void)
{
    COMECA("RN-64 · rename que falha não publica perfil pela metade");
    const hal_t *hal = memoria_pronta();

    perfil_local_t p;
    memset(&p, 0, sizeof p);
    snprintf(p.proprietario_id, sizeof p.proprietario_id, "aa");
    snprintf(p.nome, sizeof p.nome, "Usuário");
    ESPERA_IGUAL(perfil_grava(hal, &p), OK);

    pc_falhar_operacao(OP_MEM_RENOMEAR, ERR_ARQUIVO);
    snprintf(p.nome, sizeof p.nome, "Nome que nunca chegou");
    ESPERA_IGUAL(perfil_grava(hal, &p), ERR_ARQUIVO);

    pc_falhar_operacao(OP_MEM_RENOMEAR, OK);
    perfil_local_t lido;
    ESPERA_IGUAL(perfil_carrega(hal, &lido), OK);
    ESPERA_TEXTO(lido.nome, "Usuário");
    TERMINA();
}


// O nome entra no JSON sem escape: aspas e barra invertida saem na entrada,
// senão o perfil fica ilegível e o aparelho volta ao primeiro uso.
void t_renomear_limpa_o_que_quebraria_o_perfil(void)
{
    COMECA("perfil · nome com aspas não quebra o perfil.json");
    const hal_t *hal = memoria_pronta();

    ESPERA_IGUAL(perfil_renomeia(hal, "Tinto \"da sala\" \\", true), OK);

    perfil_local_t lido;
    ESPERA_IGUAL(perfil_carrega(hal, &lido), OK);
    ESPERA_TEXTO(lido.nome, "Tinto da sala ");
    ESPERA(lido.nome_sobe);              // trocado aqui: marcado para subir

    // Vindo do servidor, a marca sai.
    ESPERA_IGUAL(perfil_renomeia(hal, "Mesa", false), OK);
    ESPERA_IGUAL(perfil_carrega(hal, &lido), OK);
    ESPERA(!lido.nome_sobe);
    TERMINA();
}
