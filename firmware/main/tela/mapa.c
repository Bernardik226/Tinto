#include "mapa.h"

bool tela_tem_seletor(tela_id t)
{
    switch (t) {
    // As telas de leitura pura: nenhuma tem lista para andar.
    case TELA_BLOQUEADA:
    case TELA_SOBRE:
    case TELA_DOCK:
    case TELA_FALA:

    // O Leitor: ◀▶ viram PÁGINA, e não há linha selecionada.
    case TELA_LEITOR:
    case TELA_OBRA:

    // O Resultado é leitura: o OK fecha.
    case TELA_RESULTADO:
        return false;

    // Todo o resto: na dúvida, custa CPU, não tinta (ver mapa.h).
    default:
        return true;
    }
}

// Quem já foi olhado, na ordem do enum: lado a lado, o que falta salta.
bool tela_declarada(tela_id t)
{
    switch (t) {
    case TELA_AGENDA:
    case TELA_AJUSTES:
    case TELA_BLOQUEADA:
    case TELA_NOTA:
    case TELA_DIA:
    case TELA_CALENDARIO:
    case TELA_SOBRE:
    case TELA_DOCK:
    case TELA_FALA:
    case TELA_CONFERIR:
    case TELA_TECLADO:
    case TELA_QUANDO:
    case TELA_HORARIO:
    case TELA_ESCOLHER_DIA:
    case TELA_ESCOLHER_HORA:
    case TELA_APARENCIA:
    case TELA_SOM:
    case TELA_WIFI:
    case TELA_ARMAZENAMENTO:
    case TELA_CONTA:
    case TELA_SINCRONIZACAO:
    case TELA_DATA_HORA:
    case TELA_ANOTACOES:
    case TELA_VINCULAR:

    case TELA_HOME:
    case TELA_ACERVO:
    case TELA_OBRA:
    case TELA_LEITOR:
    case TELA_LEITURA_AJUSTES:
    case TELA_JOGOS:
    case TELA_XADREZ:
    case TELA_RESULTADO:
    case TELA_INICIO:
        return true;

    // A sentinela é o tamanho do enum, listada porque `-Wswitch` exige.
    case TELA_QUANTAS:
        break;
    }
    // Sem `default`: com -Wswitch-enum o compilador acusa a tela nova antes do
    // teste.
    return false;
}
