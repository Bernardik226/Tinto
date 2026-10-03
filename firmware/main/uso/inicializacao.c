#include "inicializacao.h"
#include "../dado/memoria.h"
#include "../dado/perfil.h"
#include "../dado/rede.h"
#include <stdio.h>
#include <string.h>

// A fase é DERIVADA dos documentos validados, nunca gravada à parte: um
// passo persistido avançaria mesmo com o recibo falhando.

static inicio_fase_t fase_da_midia(memoria_estado_t m)
{
    switch (m) {
    case MEMORIA_AUSENTE:         return INICIO_MEMORIA_AUSENTE;
    case MEMORIA_COMUNICACAO:     return INICIO_MEMORIA_COMUNICACAO;
    case MEMORIA_SEM_FILESYSTEM:  return INICIO_MEMORIA_REPARO;
    case MEMORIA_CORROMPIDA:      return INICIO_MEMORIA_REPARO;
    case MEMORIA_SOMENTE_LEITURA: return INICIO_MEMORIA_SOMENTE_LEITURA;
    case MEMORIA_CHEIA:           return INICIO_MEMORIA_CHEIA;
    case MEMORIA_PRONTA:          break;
    }
    return INICIO_HOME;
}

// Cheia e somente-leitura montam e só falham ao gravar: voltar para a fase
// certa, não "sem cartão".
static inicio_fase_t fase_do_erro(erro_t err)
{
    switch (err) {
    case ERR_SOMENTE_LEITURA: return INICIO_MEMORIA_SOMENTE_LEITURA;
    case ERR_CHEIO:           return INICIO_MEMORIA_CHEIA;
    case ERR_SEM_CARTAO:      return INICIO_MEMORIA_AUSENTE;
    default:                  return INICIO_MEMORIA_REPARO;
    }
}

// A ordem é fixa: sem nome, boas-vindas; nome sem conclusão, conclusão.
static inicio_fase_t primeira_pendencia(const hal_t *hal, estado_t *e)
{
    perfil_local_t p;
    if (perfil_carrega(hal, &p) != OK) return INICIO_BOAS_VINDAS;
    if (p.nome[0] == '\0')             return INICIO_BOAS_VINDAS;

    // O nome volta ao snapshot: depois de um reboot a conclusão ainda diz o
    // nome.
    snprintf(e->inicio.nome_pendente, sizeof e->inicio.nome_pendente,
             "%s", p.nome);
    e->inicio.nome_sobe = p.nome_sobe != 0;

    // Rede e depois conta (vincular exige rede; pular o Wi-Fi pula a conta).
    // Só enquanto o primeiro uso não fechou: sem rede depois disso é assunto do
    // rodapé, não volta às boas-vindas.
    if (p.onboarding_v < 1) {
        if (!e->inicio.pulou_wifi && e->rede != REDE_LIGADA)
            return INICIO_WIFI;

        if (!e->inicio.pulou_wifi && !e->inicio.pulou_conta &&
            e->rede == REDE_LIGADA && e->nome[0] == '\0')
            return INICIO_CONTA;
    }

    // RN-6G: sem NTP, hora só existe digitada — antes da conclusão, que promete
    // um aparelho pronto. Com rede o passo some: o NTP traz em segundos.
    // Digitada antes OU vinda do NTP neste boot.
    e->hora_confiavel = e->hora_confiavel || p.relogio_v > 0;
    // Só no primeiro uso: aparelho configurado que liga antes do Wi-Fi espera
    // o NTP.
    if (p.onboarding_v < 1 && p.relogio_v < 1 && e->rede != REDE_LIGADA) {
        // Carregados uma vez, na entrada da fase; depois são os que a pessoa edita.
        if (e->inicio.ano < 2024) uso_campos_do_relogio(e);
        return INICIO_DATA_HORA;
    }

    if (p.onboarding_v < 1) return INICIO_CONCLUSAO;
    return INICIO_HOME;
}

// Diagnostica a mídia e a árvore, e para na primeira coisa que impede o uso.
static inicio_fase_t diagnostica(const hal_t *hal, estado_t *e)
{
    memoria_estado_t m = hal->memoria_estado();
    e->inicio.diagnostico = m;
    if (m != MEMORIA_PRONTA) return fase_da_midia(m);

    arvore_estado_t arvore;
    erro_t err = memoria_inspeciona(hal, &arvore);
    if (err != OK) return fase_do_erro(err);

    // Sem isto, "precisa de reparo" é veredito sem prova.
    if (hal->registrar) {
        char msg[40];
        snprintf(msg, sizeof msg, "arvore=%d", (int)arvore);
        hal->registrar("memoria", msg);
    }

    // RN-63: versão mais nova tem tela própria; oferecer reparo convidaria a
    // apagar o trabalho dela.
    if (arvore == ARVORE_FORMATO_FUTURO) return INICIO_FORMATO_FUTURO;
    if (arvore == ARVORE_DANIFICADA)     return INICIO_MEMORIA_REPARO;

    err = memoria_prepara(hal);
    if (err != OK) return fase_do_erro(err);

    err = memoria_prova_escrita(hal);
    if (err != OK) return fase_do_erro(err);

    return primeira_pendencia(hal, e);
}

// ── os comandos que avançam ──────────────────────────────────────────
static erro_t salva_nome(const hal_t *hal, estado_t *e, const char *texto)
{
    erro_t err = perfil_renomeia(hal, texto, true);
    if (err == OK) e->inicio.nome_sobe = true;
    return err;
}

static erro_t salva_hora(const hal_t *hal, estado_t *e)
{
    perfil_local_t p;
    erro_t err = perfil_carrega(hal, &p);
    if (err != OK) return err;

    data_t d = { .ano = (int16_t)e->inicio.ano,
                 .mes = (int8_t)e->inicio.mes,
                 .dia = (int8_t)e->inicio.dia };
    hal->ajustar_relogio(d, e->inicio.hora, e->inicio.minuto);

    p.relogio_v = 1;
    return perfil_grava(hal, &p);
}

void uso_campos_do_relogio(estado_t *e)
{
    bool plausivel = e->hoje.ano >= 2024 && e->hoje.ano <= 2099 &&
                     e->hoje.mes >= 1 && e->hoje.mes <= 12 &&
                     e->hoje.dia >= 1 && e->hoje.dia <= 31;
    e->inicio.ano    = plausivel ? e->hoje.ano : 2026;
    e->inicio.mes    = plausivel ? e->hoje.mes : 1;
    e->inicio.dia    = plausivel ? e->hoje.dia : 1;
    e->inicio.hora   = plausivel ? e->hora     : 12;
    e->inicio.minuto = plausivel ? e->minuto   : 0;
    e->inicio.campo  = 0;
}

erro_t uso_ajustar_relogio(const hal_t *hal, estado_t *e)
{
    // O relógio anda ANTES do perfil: cartão em falta impede o recibo, nunca a
    // hora.
    data_t d = { .ano = (int16_t)e->inicio.ano,
                 .mes = (int8_t)e->inicio.mes,
                 .dia = (int8_t)e->inicio.dia };
    hal->ajustar_relogio(d, e->inicio.hora, e->inicio.minuto);

    perfil_local_t p;
    erro_t err = perfil_carrega(hal, &p);
    if (err != OK) return err;

    p.relogio_v = 1;
    return perfil_grava(hal, &p);
}

static erro_t conclui(const hal_t *hal)
{
    perfil_local_t p;
    erro_t err = perfil_carrega(hal, &p);
    if (err != OK) return err;
    p.onboarding_v = 1;
    return perfil_grava(hal, &p);
}

erro_t uso_configurar_dispositivo(const hal_t *hal, estado_t *estado,
                                  inicio_comando_t comando, const char *texto)
{
    if (!hal || !estado) return ERR_INTERNO;

    erro_t err = OK;
    switch (comando) {
    case INICIO_CMD_BOOT:
        break;

    // Pular não grava nada: continuar sem rede é escolha legítima.
    case INICIO_CMD_PULAR_WIFI:
        estado->inicio.pulou_wifi = true;
        break;

    case INICIO_CMD_PULAR_CONTA:
        estado->inicio.pulou_conta = true;
        break;

    case INICIO_CMD_SALVAR_NOME:
        err = salva_nome(hal, estado, texto ? texto : "");
        break;

    case INICIO_CMD_SALVAR_HORA:
        err = salva_hora(hal, estado);
        break;

    case INICIO_CMD_CONCLUIR:
        err = conclui(hal);
        break;

    case INICIO_CMD_FORMATAR:
        // RN-6C: o único caminho destrutivo, depois de duas telas e de um cursor
        // que começa no "não".
        err = hal->formatar_memoria();

        // E a rede do RÁDIO vai junto: sobrevivia ao formato e reconectava no Wi-Fi
        // do dono anterior. "Esquecer rede" em Ajustes é trocar de casa; formatar é
        // trocar de dono.
        if (hal->esquecer_rede) hal->esquecer_rede();

        // E a senha do cofre (flash). A RAM o app limpa reiniciando o chip.
        (void)rede_esquece(hal);
        break;
    }

    // Falhou ou não, o diagnóstico é refeito: a fase mostra o estado real.
    estado->inicio.fase = diagnostica(hal, estado);

    // A tela Sobre é onde se lê o erro sem serial; mídia travada é o erro que
    // a pessoa precisa saber.
    switch (estado->inicio.fase) {
    case INICIO_MEMORIA_AUSENTE:
        estado->ultimo_erro = ERR_SEM_CARTAO; break;
    case INICIO_MEMORIA_SOMENTE_LEITURA:
        estado->ultimo_erro = ERR_SOMENTE_LEITURA; break;
    case INICIO_MEMORIA_CHEIA:
        estado->ultimo_erro = ERR_CHEIO; break;
    case INICIO_MEMORIA_COMUNICACAO:
    case INICIO_MEMORIA_REPARO:
        estado->ultimo_erro = ERR_ARQUIVO; break;
    case INICIO_FORMATO_FUTURO:
        estado->ultimo_erro = ERR_FORMATO; break;
    default:
        break;
    }
    if (err != OK) estado->ultimo_erro = err;
    return err;
}
