#include "rotina.h"
#include <stdlib.h>
#include <string.h>

// "…|u=20261130" → 20261130. Zero quando não tem fim.
static long ate_quando(const char *regra)
{
    const char *u = strstr(regra, "|u=");
    return u ? strtol(u + 3, NULL, 10) : 0;
}

// Aquele dia foi tirado da série? Compara MMDD: na janela de três meses,
// mesmo mês e dia é o mesmo dia.
static bool foi_tirado(const char *regra, data_t dia)
{
    const char *x = strstr(regra, "|x=");
    if (!x) return false;

    char mmdd[5];
    mmdd[0] = (char)('0' + dia.mes / 10);
    mmdd[1] = (char)('0' + dia.mes % 10);
    mmdd[2] = (char)('0' + dia.dia / 10);
    mmdd[3] = (char)('0' + dia.dia % 10);
    mmdd[4] = '\0';

    for (const char *p = x + 3; *p; ) {
        if (strncmp(p, mmdd, 4) == 0) return true;
        const char *virgula = strchr(p, ',');
        if (!virgula) break;
        p = virgula + 1;
    }
    return false;
}

// Dias da semana da regra semanal, em bitmask. Zero = o dia da semana da
// âncora, como o Google faz.
static int dias_da_semana(const char *regra)
{
    // "s:1:12345" — o terceiro campo, quando existe.
    const char *p = strchr(regra, ':');
    if (!p) return 0;
    p = strchr(p + 1, ':');
    if (!p) return 0;

    int mascara = 0;
    for (p++; *p >= '0' && *p <= '6'; p++) mascara |= 1 << (*p - '0');
    return mascara;
}

bool rotina_no_dia(const char *regra, data_t ancora, data_t dia)
{
    if (!regra || !regra[0]) return data_igual(ancora, dia);
    if (data_compara(dia, ancora) < 0) return false;

    long ate = ate_quando(regra);
    if (ate) {
        long este = (long)dia.ano * 10000 + dia.mes * 100 + dia.dia;
        if (este > ate) return false;
    }

    if (foi_tirado(regra, dia)) return false;

    int intervalo = 1;
    const char *dois_pontos = strchr(regra, ':');
    if (dois_pontos) intervalo = (int)strtol(dois_pontos + 1, NULL, 10);
    if (intervalo < 1) intervalo = 1;

    int passados = data_dias_entre(ancora, dia);

    switch (regra[0]) {
    case 'd':
        return passados % intervalo == 0;

    case 's': {
        int mascara = dias_da_semana(regra);
        if (!mascara) mascara = 1 << data_dia_da_semana(ancora);
        if (!(mascara & (1 << data_dia_da_semana(dia)))) return false;

        // A semana conta a partir do DOMINGO da âncora: "a cada duas semanas, seg e
        // qua" começando numa quarta aceita a segunda da mesma semana.
        int desde_domingo = data_dia_da_semana(ancora);
        int semanas = (passados + desde_domingo) / 7;
        return semanas % intervalo == 0;
    }

    case 'm': {
        if (dia.dia != ancora.dia) return false;
        int meses = (dia.ano - ancora.ano) * 12 + (dia.mes - ancora.mes);
        return meses >= 0 && meses % intervalo == 0;
    }

    case 'a': {
        if (dia.dia != ancora.dia || dia.mes != ancora.mes) return false;
        int anos = dia.ano - ancora.ano;
        return anos >= 0 && anos % intervalo == 0;
    }

    default:
        // Regra desconhecida: só o dia da âncora. Inventar uma semana afirmaria
        // compromissos que ninguém marcou.
        return data_igual(ancora, dia);
    }
}
