#include "campos.h"
#include "../nucleo/data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// A linha diz o que a LLM decidiu que isto é, e onde foi parar. O tipo vem
// em caps: é a ação. Não se edita (RN-23) — veio errado, descarta e fala de
// novo —, mas tem de estar VISÍVEL.
void vista_destino(const item_t *it, char *out, size_t n)
{
    switch (it->tipo) {
    case TIPO_ANOTACAO:                               // RN-25
        snprintf(out, n, "%s", "ANOTAÇÃO · fora do Google");
        break;
    case TIPO_EVENTO:
        // "Google Agenda", o nome do app onde a pessoa procura. O que a voz cria
        // vive numa agenda própria (RN-4E), que está no Google Agenda.
        snprintf(out, n, "%s", "EVENTO · Google Agenda");
        break;
    case TIPO_LISTA:
        snprintf(out, n, "%s", "LISTA · nas Tarefas");
        break;
    case TIPO_TAREFA:
        // O HÍBRIDO se nomeia: aparece na régua, e "TAREFA" ali discordaria da
        // tela. O destino continua o Tasks: "cadê isso no meu celular?".
        snprintf(out, n, "%s", it->hora[0] ? "TAREFA COM HORA · Minhas tarefas"
                                           : "TAREFA · Minhas tarefas");
        break;
    default:
        // Captura crua: ainda não é nada, e não vai a lugar nenhum.
        snprintf(out, n, "%s", "por estruturar");
        break;
    }
}


void vista_faixa(const item_t *it, bool h24, char *out, size_t n)
{
    char de[HORA_TEXTO], ate[HORA_TEXTO];
    hora_texto_hhmm(it->hora, h24, de,  sizeof de);
    hora_texto_hhmm(it->fim,  h24, ate, sizeof ate);

    if (it->dia_inteiro)    snprintf(out, n, "%s", "o dia todo");
    else if (de[0] && ate[0])
                            snprintf(out, n, "%s – %s", de, ate);
    else if (de[0])         snprintf(out, n, "%s", de);
    else                    out[0] = '\0';
}

int vista_wifi_da_barra(const estado_t *e)
{
    // Conectando também aparece: é quando a pessoa olha a barra esperando.
    if (e->rede == REDE_LIGADA)     return e->wifi_forca > 0 ? e->wifi_forca : 1;
    if (e->rede == REDE_CONECTANDO) return 0;
    return -1;
}

bool vista_ocupado(const estado_t *e)
{
    return e->wifi_procurando
        || e->sinc == SINC_ENVIANDO || e->sinc == SINC_RECEBENDO
        || e->sinc == SINC_ESPERANDO_REDE
        || e->rede == REDE_CONECTANDO;
}


void vista_maiuscula(char *s)
{
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;

        if (c >= 'a' && c <= 'z') { *s = (char)(c - 32); continue; }

        // Latin-1 em UTF-8: 0xC3 + o byte seguinte; minúsculas 0xA0–0xBE,
        // maiúsculas 0x80–0x9E (vinte de diferença). 0xB7 é "÷", sem par; 0xBF é
        // "ÿ", cuja maiúscula mora fora deste bloco.
        if (c == 0xC3) {
            unsigned char d = (unsigned char)s[1];
            if (d >= 0xA0 && d <= 0xBE && d != 0xB7) s[1] = (char)(d - 0x20);
            if (d) s++;
        }
    }
}

icone_id vista_sinc_da_barra(const estado_t *e)
{
    switch (e->sinc) {
    case SINC_RECEBENDO: return ICO_DESCENDO;
    case SINC_ENVIANDO:  return ICO_SUBINDO;
    case SINC_ERRO:      return ICO_SYNC_ERRO;
    // Esperando a rede mostra a mesma seta de subir: para quem olha, está a
    // caminho.
    case SINC_ESPERANDO_REDE: return ICO_SUBINDO;
    case SINC_OCIOSO:    break;
    }

    // Ocioso de sync, mas esperando outra coisa (redes, hora): uma forma, num
    // slot só. A ordem [sync] [wi-fi] [hora] [bateria] não deixa hora e bateria
    // dançarem.
    return vista_ocupado(e) ? ICO_SINCRONIZA : ICO_NENHUM;
}

bool vista_espera_visivel(nuvem_espera_t o_que)
{
    switch (o_que) {
    case NUVEM_CAPTURA:      // a fala subindo, com a tela travada
    case NUVEM_GESTO:        // renomear, apagar, concluir
    case NUVEM_ESCOLHA:      // ligar ou desligar uma agenda
    case NUVEM_AGENDAS:      // o catálogo que a tela está esperando
    case NUVEM_PAREAR:
    case NUVEM_PAREADO:
    case NUVEM_DESVINCULAR:
        return true;
    default:
        // NUVEM_PULL e NUVEM_REGISTRAR: ninguém está olhando a barra por eles.
        return false;
    }
}

void vista_hora_da_barra(const estado_t *e, char *out, size_t max)
{
    if (!out || !max) return;
    out[0] = '\0';
    if (!e) return;

    // RN-6G: hora não ajustada não é hora; barra vazia é melhor que meia-noite
    // falsa.
    if (!e->hora_confiavel) return;

    hora_texto(e->hora, e->minuto, e->config.valor[AJUSTE_HORA24] != 0,
               out, max);
}

void vista_hora_do_item(const estado_t *e, const char *hhmm,
                        char *out, size_t max)
{
    hora_texto_hhmm(hhmm, e && e->config.valor[AJUSTE_HORA24] != 0, out, max);
}

void vista_titulo_do_item(const estado_t *e, const item_t *it,
                          char *out, size_t max)
{
    (void)e;
    if (!out || !max) return;
    if (it->titulo[0]) { snprintf(out, max, "%s", it->titulo); return; }
    snprintf(out, max, "%s", "(Sem título)");
}

void vista_hora_do_campo(const estado_t *e, int hora, char *out, size_t max)
{
    if (!out || !max) return;

    if (!e || e->config.valor[AJUSTE_HORA24]) {
        snprintf(out, max, "%02d", hora);
        return;
    }

    int doze = hora % 12;
    if (doze == 0) doze = 12;
    snprintf(out, max, "%d %s", doze, hora < 12 ? "am" : "pm");
}

const char *vista_forca_texto(int forca)
{
    if (forca >= 66) return "forte";
    if (forca >= 33) return "média";
    return "fraca";
}

// ── a regra da rotina, em palavras ───────────────────────────────────
// O fio é curto ("s:1:135"); a tela devolve "seg, qua, sex".
void vista_rotina_em_palavras(const char *regra, char *out, size_t max)
{
    if (!out || !max) return;
    out[0] = '\0';
    if (!regra || !regra[0]) return;

    int intervalo = 1;
    const char *dp = strchr(regra, ':');
    if (dp) intervalo = atoi(dp + 1);
    if (intervalo < 1) intervalo = 1;

    static const char *SEMANA[7] = { "dom", "seg", "ter",
                                     "qua", "qui", "sex", "sáb" };

    switch (regra[0]) {
    case 'd':
        if (intervalo == 1) snprintf(out, max, "%s", "todo dia");
        else                snprintf(out, max, "a cada %d dias", intervalo);
        break;

    case 's': {
        // Os dias vêm depois do segundo `:`. Sem eles, "toda semana", como o
        // celular diz.
        const char *dias = dp ? strchr(dp + 1, ':') : NULL;
        if (dias && dias[1] >= '0' && dias[1] <= '6') {
            size_t usado = 0;
            for (const char *p = dias + 1; *p >= '0' && *p <= '6'; p++) {
                int n = snprintf(out + usado, max - usado, "%s%s",
                                 usado ? ", " : "", SEMANA[*p - '0']);
                if (n < 0 || (size_t)n >= max - usado) break;
                usado += (size_t)n;
            }
            if (intervalo > 1 && usado + 20 < max)
                snprintf(out + usado, max - usado,
                         " (a cada %d sem.)", intervalo);
        } else if (intervalo == 1) {
            snprintf(out, max, "%s", "toda semana");
        } else {
            snprintf(out, max, "a cada %d semanas", intervalo);
        }
        break;
    }

    case 'm':
        if (intervalo == 1) snprintf(out, max, "%s", "todo mês");
        else                snprintf(out, max, "a cada %d meses", intervalo);
        break;

    case 'a':
        snprintf(out, max, "%s", "todo ano");
        break;

    default:
        // Regra que este firmware não expande: "repete", sem inventar frequência.
        snprintf(out, max, "%s", "repete");
        break;
    }
}

bool vista_tarefa_no_dia(const estado_t *e, const item_t *it, data_t dia,
                         bool acompanha)
{
    if (!e || !it) return false;
    if (it->tipo != TIPO_TAREFA && it->tipo != TIPO_LISTA) return false;

    // Com HORA é compromisso, e mora na régua. Só o prazo próprio a traz.
    if (it->hora[0]) {
        if (!it->prazo.ano || it->feita) return false;
        return data_igual(it->prazo, dia) ||
               (acompanha && data_igual(dia, e->hoje));
    }

    // A CONCLUÍDA fica no dia em que foi feita; senão o fechado se acumularia
    // em hoje para sempre.
    if (it->feita) return data_igual(it->feita_em, dia);

    // O INTERVALO ("de segunda a sexta") vale em cada dia dele.
    if (it->vence.ano && it->prazo.ano)
        return data_compara(it->vence, dia) <= 0 &&
               data_compara(dia, it->prazo) <= 0;

    if (!it->vence.ano) return acompanha;

    // No dia do prazo, sempre; e em HOJE até ser feita (RN-34).
    return data_igual(it->vence, dia) ||
           (acompanha && data_igual(dia, e->hoje));
}

// "há 2 min", "agora", "nunca": de quando é o que está na tela.
void vista_quando_sincronizou(const estado_t *e, char *fora, size_t n)
{
    if (!e->sinc_ultima_ms) { snprintf(fora, n, "%s", "nunca"); return; }

    uint32_t faz = (e->agora_ms - e->sinc_ultima_ms) / 1000u;
    if (faz < 90)          snprintf(fora, n, "%s", "agora");
    else if (faz < 3600)   snprintf(fora, n, "há %u min", (unsigned)(faz / 60));
    else                   snprintf(fora, n, "há %u h",   (unsigned)(faz / 3600));
}

void vista_falta(int minutos, char *out, size_t max)
{
    if (minutos <= 0)           snprintf(out, max, "%s", "agora");
    else if (minutos < 60)      snprintf(out, max, "em %d min", minutos);
    else if (minutos % 60 == 0) snprintf(out, max, "em %d h", minutos / 60);
    else                        snprintf(out, max, "em %dh%02d",
                                         minutos / 60, minutos % 60);
}

void vista_maiuscula1(char *s)
{
    if (s[0] >= 'a' && s[0] <= 'z') s[0] = (char)(s[0] - 32);
}
