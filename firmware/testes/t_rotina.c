// firmware/testes/t_rotina.c — a regra da rotina.
// Se "a cada duas semanas" errar, o vidro mostra compromisso em dia que
// ninguém marcou.
#include "teste.h"
#include "nucleo/rotina.h"
#include "vista/campos.h"

static data_t D(int ano, int mes, int dia)
{
    data_t d = { (int16_t)ano, (int8_t)mes, (int8_t)dia };
    return d;
}

void t_rotina_todo_dia(void)
{
    COMECA("rotina · todo dia, e só a partir da âncora");

    data_t ancora = D(2026, 9, 11);

    ESPERA(rotina_no_dia("d:1", ancora, D(2026, 9, 11)));
    ESPERA(rotina_no_dia("d:1", ancora, D(2026, 9, 12)));
    ESPERA(rotina_no_dia("d:1", ancora, D(2026, 10, 3)));

    // Antes da âncora, não.
    ESPERA(!rotina_no_dia("d:1", ancora, D(2026, 9, 10)));

    // De dois em dois.
    ESPERA(rotina_no_dia("d:2", ancora, D(2026, 9, 13)));
    ESPERA(!rotina_no_dia("d:2", ancora, D(2026, 9, 12)));

    TERMINA();
}

void t_rotina_dias_da_semana(void)
{
    COMECA("rotina · seg, qua e sex — e a cada duas semanas");

    // 2026-09-11 é uma sexta.
    data_t sexta = D(2026, 9, 11);

    ESPERA(rotina_no_dia("s:1:135", sexta, D(2026, 9, 11)));   // sex
    ESPERA(rotina_no_dia("s:1:135", sexta, D(2026, 9, 14)));   // seg
    ESPERA(rotina_no_dia("s:1:135", sexta, D(2026, 9, 16)));   // qua
    ESPERA(!rotina_no_dia("s:1:135", sexta, D(2026, 9, 15)));  // ter
    ESPERA(!rotina_no_dia("s:1:135", sexta, D(2026, 9, 13)));  // dom

    // Sem dias escolhidos, o dia da semana da âncora.
    ESPERA(rotina_no_dia("s:1", sexta, D(2026, 9, 18)));
    ESPERA(!rotina_no_dia("s:1", sexta, D(2026, 9, 17)));

    // A cada DUAS semanas.
    ESPERA(rotina_no_dia("s:2", sexta, D(2026, 9, 25)));
    ESPERA(!rotina_no_dia("s:2", sexta, D(2026, 9, 18)));

    TERMINA();
}

void t_rotina_mes_e_ano(void)
{
    COMECA("rotina · todo mês no mesmo dia, e todo ano na mesma data");

    data_t dia5 = D(2026, 9, 5);

    ESPERA(rotina_no_dia("m:1", dia5, D(2026, 10, 5)));
    ESPERA(rotina_no_dia("m:1", dia5, D(2027, 1, 5)));
    ESPERA(!rotina_no_dia("m:1", dia5, D(2026, 10, 6)));
    ESPERA(rotina_no_dia("m:2", dia5, D(2026, 11, 5)));
    ESPERA(!rotina_no_dia("m:2", dia5, D(2026, 10, 5)));

    ESPERA(rotina_no_dia("a:1", dia5, D(2027, 9, 5)));
    ESPERA(!rotina_no_dia("a:1", dia5, D(2027, 9, 6)));

    TERMINA();
}

void t_rotina_acaba_e_tem_buraco(void)
{
    COMECA("rotina · o fim e os dias apagados saem dela");

    data_t ancora = D(2026, 9, 11);

    // Até 20/09, inclusive.
    ESPERA(rotina_no_dia("d:1|u=20260920", ancora, D(2026, 9, 20)));
    ESPERA(!rotina_no_dia("d:1|u=20260920", ancora, D(2026, 9, 21)));

    // O dia apagado no celular é o buraco na regra.
    ESPERA(!rotina_no_dia("d:1|x=0915", ancora, D(2026, 9, 15)));
    ESPERA(rotina_no_dia("d:1|x=0915", ancora, D(2026, 9, 16)));

    // Dois buracos.
    ESPERA(!rotina_no_dia("d:1|x=0915,0922", ancora, D(2026, 9, 22)));
    ESPERA(rotina_no_dia("d:1|x=0915,0922", ancora, D(2026, 9, 23)));

    TERMINA();
}

void t_rotina_sem_regra_e_regra_estranha(void)
{
    COMECA("rotina · sem regra é um dia só; regra desconhecida também");

    data_t ancora = D(2026, 9, 11);

    ESPERA(rotina_no_dia("", ancora, D(2026, 9, 11)));
    ESPERA(!rotina_no_dia("", ancora, D(2026, 9, 12)));

    // Frequência desconhecida: só o dia da âncora.
    ESPERA(rotina_no_dia("z:1", ancora, D(2026, 9, 11)));
    ESPERA(!rotina_no_dia("z:1", ancora, D(2026, 9, 12)));

    TERMINA();
}

// A regra em português na tela.
void t_rotina_em_palavras(void)
{
    COMECA("rotina · a regra vira frase na tela");

    char txt[40];

    vista_rotina_em_palavras("d:1", txt, sizeof txt);
    ESPERA_TEXTO(txt, "todo dia");

    vista_rotina_em_palavras("d:3", txt, sizeof txt);
    ESPERA_TEXTO(txt, "a cada 3 dias");

    vista_rotina_em_palavras("s:1:135", txt, sizeof txt);
    ESPERA_TEXTO(txt, "seg, qua, sex");

    vista_rotina_em_palavras("s:1", txt, sizeof txt);
    ESPERA_TEXTO(txt, "toda semana");

    vista_rotina_em_palavras("m:1|u=20261130", txt, sizeof txt);
    ESPERA_TEXTO(txt, "todo mês");

    // O que não se repete não diz nada.
    vista_rotina_em_palavras("", txt, sizeof txt);
    ESPERA_TEXTO(txt, "");

    TERMINA();
}
