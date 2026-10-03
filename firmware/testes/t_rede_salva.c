// firmware/testes/t_rede_salva.c — a rede que o aparelho lembra.
// A rede conectada volta sozinha no boot. O nome fica no cartão; a senha, no
// cofre (NVS).
#include "teste.h"
#include "dado/rede.h"

static app_t ap;

void t_a_rede_conectada_fica_guardada(void)
{
    COMECA("Wi-Fi · a rede conectada volta sozinha no boot seguinte");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 21}, 9, 19);

    rede_salva_t r;
    memset(&r, 0, sizeof r);
    snprintf(r.nome,  sizeof r.nome,  "%s", "casa");
    snprintf(r.senha, sizeof r.senha, "%s", "manteiga42");
    ESPERA_IGUAL(rede_grava(hal, &r), OK);

    rede_salva_t lida;
    ESPERA_IGUAL(rede_carrega(hal, &lida), OK);
    ESPERA_TEXTO(lida.nome,  "casa");
    ESPERA_TEXTO(lida.senha, "manteiga42");

    // E o aparelho tenta essa rede sozinho ao ligar.
    app_liga(&ap, hal);
    app_passo(&ap);

    ESPERA_TEXTO(ap.estado.wifi_alvo, "casa");

    TERMINA();
}

// Sem rede guardada não é erro: é o estado normal.
void t_sem_rede_guardada_o_boot_segue(void)
{
    COMECA("Wi-Fi · sem rede guardada o aparelho liga igual");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 21}, 9, 19);

    rede_salva_t lida;
    ESPERA(rede_carrega(hal, &lida) != OK);

    app_liga(&ap, hal);
    app_passo(&ap);

    ESPERA_TEXTO(ap.estado.wifi_alvo, "");
    ESPERA_IGUAL(ap.estado.rede, REDE_DESLIGADA);

    TERMINA();
}

// A lista marca "salva": o OK nela conecta direto.
void t_a_lista_marca_a_rede_conhecida(void)
{
    COMECA("T-28a · a lista marca como salva a rede que o aparelho conhece");

    const hal_t *hal = pc_liga();
    pc_relogio((data_t){2026, 8, 21}, 9, 19);
    pc_redes("casa", 82, "VIVO-A24C", 54);

    rede_salva_t r;
    memset(&r, 0, sizeof r);
    snprintf(r.nome,  sizeof r.nome,  "%s", "casa");
    snprintf(r.senha, sizeof r.senha, "%s", "manteiga42");
    (void)rede_grava(hal, &r);

    app_liga(&ap, hal);
    app_passo(&ap);

    ap.estado.pilha[++ap.estado.profundidade] = TELA_WIFI;
    ap.estado.wifi_procurando = true;
    hal->wifi_procurar();
    app_passo(&ap);

    bool casa_salva = false;
    for (int i = 0; i < ap.estado.n_redes; i++)
        if (strcmp(ap.estado.redes[i].nome, "casa") == 0)
            casa_salva = ap.estado.redes[i].salva;

    ESPERA(casa_salva);

    TERMINA();
}

// ── a senha sai do cartão ───────────────────────────────────────────
// O cartão sai com a unha; a NVS está na flash soldada (não é cofre, mas
// dessoldar é outra ordem de ataque). O SSID fica no cartão: não é segredo e
// ajuda a diagnosticar.
void t_a_senha_do_wifi_nao_mora_no_cartao(void)
{
    COMECA("a senha vai para a NVS, e o cartão guarda só o nome da rede");

    const hal_t *hal = pc_liga();
    pc_segredos_zera();

    rede_salva_t r;
    memset(&r, 0, sizeof r);
    snprintf(r.nome,  sizeof r.nome,  "%s", "casa-2g");
    snprintf(r.senha, sizeof r.senha, "%s", "segredo-da-casa");
    ESPERA_IGUAL(rede_grava(hal, &r), OK);

    // No cartão: o nome, e nada da senha.
    char json[192];
    ESPERA_IGUAL(hal->ler("/TINTO/sistema/wifi.json", json, sizeof json), OK);
    ESPERA_CONTEM(json, "casa-2g");
    ESPERA_SEM(json, "segredo-da-casa");

    // A senha está no cofre.
    ESPERA_TEXTO(pc_segredo("wifi_senha"), "segredo-da-casa");

    // A leitura junta os dois.
    rede_salva_t lida;
    ESPERA_IGUAL(rede_carrega(hal, &lida), OK);
    ESPERA_TEXTO(lida.nome,  "casa-2g");
    ESPERA_TEXTO(lida.senha, "segredo-da-casa");

    // Esquecer a rede leva a senha junto.
    ESPERA_IGUAL(rede_esquece(hal), OK);
    ESPERA_TEXTO(pc_segredo("wifi_senha"), "");

    TERMINA();
}

// ── o cartão antigo ─────────────────────────────────────────────────
// A primeira leitura migra a senha para o cofre e reescreve o cartão sem
// ela.
void t_o_cartao_velho_migra_a_senha_e_se_limpa(void)
{
    COMECA("cartão de antes: a senha migra para o cofre e sai do arquivo");

    const hal_t *hal = pc_liga();
    pc_segredos_zera();

    // O formato antigo, escrito à mão.
    pc_poe_arquivo("/TINTO/sistema/wifi.json",
                   "{\"ssid\":\"casa-2g\",\"senha\":\"segredo-da-casa\"}");

    rede_salva_t lida;
    ESPERA_IGUAL(rede_carrega(hal, &lida), OK);
    ESPERA_TEXTO(lida.nome,  "casa-2g");
    ESPERA_TEXTO(lida.senha, "segredo-da-casa");   // não se perde

    // E o cartão foi limpo na passagem.
    ESPERA_TEXTO(pc_segredo("wifi_senha"), "segredo-da-casa");

    char json[192];
    ESPERA_IGUAL(hal->ler("/TINTO/sistema/wifi.json", json, sizeof json), OK);
    ESPERA_SEM(json, "segredo-da-casa");

    TERMINA();
}
