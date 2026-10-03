#include "tipos.h"

const char *erro_texto(erro_t e)
{
    // RN-A1: estado em poucas palavras + a ação. Nunca ESP_ERR_*.
    switch (e) {
    case OK:                 return "";
    case ERR_SEM_CARTAO:     return "sem cartão";
    case ERR_ARQUIVO:        return "não consegui ler";
    case ERR_FORMATO:        return "arquivo estranho";
    case ERR_SOMENTE_LEITURA: return "somente leitura";
    case ERR_CHEIO:          return "cartão cheio";
    case ERR_REDE:           return "sem contato";
    case ERR_TIMEOUT:        return "demorou demais";
    case ERR_NAO_PAREADO:    return "não conectado";
    case ERR_QUOTA:          return "fala do mês acabou";
    case ERR_VERSAO_EXIGIDA: return "sistema desatualizado";
    case ERR_INTERNO:        return "algo deu errado";

    // Quase nunca chega à tela: `dado/` o lê como dia vazio.
    case ERR_SEM_PASTA:      return "não achei";

    // A frase fala do que a pessoa faz: tentar de novo.
    case ERR_DADO_INCOMPLETO: return "cópia incompleta";
    }
    return "algo deu errado";
}

static int dias_no_mes(int ano, int mes)
{
    static const int t[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
    if (mes < 1 || mes > 12) return 0;
    if (mes == 2) {
        bool bissexto = (ano % 4 == 0 && ano % 100 != 0) || ano % 400 == 0;
        return bissexto ? 29 : 28;
    }
    return t[mes - 1];
}

bool data_valida(data_t d)
{
    if (d.ano < 2020 || d.ano > 2200) return false;
    if (d.mes < 1 || d.mes > 12)      return false;
    return d.dia >= 1 && d.dia <= dias_no_mes(d.ano, d.mes);
}

bool data_igual(data_t a, data_t b)
{
    return a.ano == b.ano && a.mes == b.mes && a.dia == b.dia;
}

int data_compara(data_t a, data_t b)
{
    if (a.ano != b.ano) return a.ano - b.ano;
    if (a.mes != b.mes) return a.mes - b.mes;
    return a.dia - b.dia;
}
