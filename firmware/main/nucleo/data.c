#include "data.h"
#include <stdio.h>
#include <string.h>

static const char *SEMANA[] = { "dom","seg","ter","qua","qui","sex","sáb" };
static const char *MESES[]  = { "jan","fev","mar","abr","mai","jun",
                                "jul","ago","set","out","nov","dez" };

void data_para_texto(data_t d, char *out, size_t max)
{
    snprintf(out, max, "%04d-%02d-%02d", d.ano, d.mes, d.dia);
}

erro_t data_de_texto(const char *s, data_t *out)
{
    if (!s || strlen(s) < 10) return ERR_FORMATO;
    for (int i = 0; i < 10; i++) {
        bool digito = (i != 4 && i != 7);
        if (digito  && (s[i] < '0' || s[i] > '9')) return ERR_FORMATO;
        if (!digito && s[i] != '-')                return ERR_FORMATO;
    }
    data_t d = {
        .ano = (int16_t)((s[0]-'0')*1000 + (s[1]-'0')*100 + (s[2]-'0')*10 + (s[3]-'0')),
        .mes = (int8_t) ((s[5]-'0')*10 + (s[6]-'0')),
        .dia = (int8_t) ((s[8]-'0')*10 + (s[9]-'0')),
    };
    if (!data_valida(d)) return ERR_FORMATO;
    *out = d;
    return OK;
}

// Dias desde uma época fixa (days_from_civil, de Howard Hinnant): gregoriano
// proléptico, sem caso especial de bissexto espalhado.
static long dias_civis(int ano, int mes, int dia)
{
    ano -= mes <= 2;
    long era = (ano >= 0 ? ano : ano - 399) / 400;
    unsigned aoe = (unsigned)(ano - era * 400);                    // [0, 399]
    unsigned doa = (unsigned)(153 * (mes + (mes > 2 ? -3 : 9)) + 2) / 5
                 + (unsigned)dia - 1;                              // [0, 365]
    unsigned doe = aoe * 365 + aoe / 4 - aoe / 100 + doa;          // [0, 146096]
    return era * 146097L + (long)doe - 719468L;
}

static data_t de_dias_civis(long z)
{
    z += 719468L;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned aoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
    long ano = (long)aoe + era * 400;
    unsigned doa = doe - (365*aoe + aoe/4 - aoe/100);
    unsigned mp  = (5*doa + 2) / 153;
    unsigned dia = doa - (153*mp + 2)/5 + 1;
    unsigned mes = mp + (mp < 10 ? 3 : -9);
    ano += (mes <= 2);
    return (data_t){ .ano = (int16_t)ano, .mes = (int8_t)mes, .dia = (int8_t)dia };
}

data_t data_soma_dias(data_t d, int dias)
{
    return de_dias_civis(dias_civis(d.ano, d.mes, d.dia) + dias);
}

int data_dias_entre(data_t a, data_t b)
{
    return (int)(dias_civis(b.ano, b.mes, b.dia) - dias_civis(a.ano, a.mes, a.dia));
}

int data_dia_da_semana(data_t d)
{
    long z = dias_civis(d.ano, d.mes, d.dia);
    return (int)((z % 7 + 11) % 7);   // 1970-01-01 foi quinta
}

const char *data_semana_curta(data_t d) { return SEMANA[data_dia_da_semana(d)]; }

const char *data_mes_curto(data_t d)
{
    if (d.mes < 1 || d.mes > 12) return "???";
    return MESES[d.mes - 1];
}

const char *data_mes_longo(data_t d)
{
    static const char *M[] = { "janeiro", "fevereiro", "março", "abril",
                               "maio", "junho", "julho", "agosto",
                               "setembro", "outubro", "novembro", "dezembro" };
    if (d.mes < 1 || d.mes > 12) return "";
    return M[d.mes - 1];
}

int data_dias_no_mes(int ano, int mes)
{
    static const int D[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if (mes < 1 || mes > 12) return 0;
    if (mes == 2) {
        bool bissexto = (ano % 4 == 0 && ano % 100 != 0) || ano % 400 == 0;
        return bissexto ? 29 : 28;
    }
    return D[mes - 1];
}

const char *data_semana_longa(data_t d)
{
    // Minúsculas; a caixa alta é aplicada pela vista onde for rótulo.
    static const char *DIAS[7] = {
        "domingo", "segunda-feira", "terça-feira", "quarta-feira",
        "quinta-feira", "sexta-feira", "sábado",
    };
    int i = data_dia_da_semana(d);
    return (i >= 0 && i < 7) ? DIAS[i] : "";
}

// ── a hora, no formato que a pessoa escolheu ─────────────────────────
void hora_texto(int h, int m, bool h24, char *out, size_t max)
{
    if (!out || !max) return;
    out[0] = '\0';

    if (h < 0 || h > 23 || m < 0 || m > 59) return;

    if (h24) {
        snprintf(out, max, "%02d:%02d", h, m);
        return;
    }

    // Meia-noite e meio-dia dão zero no `% 12`, e "0:30 am" não existe.
    int doze = h % 12;
    if (doze == 0) doze = 12;

    snprintf(out, max, "%d:%02d %s", doze, m, h < 12 ? "am" : "pm");
}

void hora_texto_hhmm(const char *hhmm, bool h24, char *out, size_t max)
{
    if (!out || !max) return;
    out[0] = '\0';
    if (!hhmm || !hhmm[0]) return;

    // O que não é hora ("dia", rótulo do dia inteiro) passa inteiro.
    int h = 0, m = 0;
    if (hhmm[0] < '0' || hhmm[0] > '9' ||
        sscanf(hhmm, "%d:%d", &h, &m) != 2 ||
        h < 0 || h > 23 || m < 0 || m > 59) {
        snprintf(out, max, "%s", hhmm);
        return;
    }

    hora_texto(h, m, h24, out, max);
}

int data_minutos(const char *hhmm)
{
    if (!hhmm || strlen(hhmm) < 5 || hhmm[2] != ':') return -1;
    for (int i = 0; i < 5; i++)
        if (i != 2 && (hhmm[i] < '0' || hhmm[i] > '9')) return -1;
    int h = (hhmm[0] - '0') * 10 + (hhmm[1] - '0');
    int m = (hhmm[3] - '0') * 10 + (hhmm[4] - '0');
    return h < 24 && m < 60 ? h * 60 + m : -1;
}
