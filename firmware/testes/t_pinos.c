#include "teste.h"
#include "pins.h"

// Mantém o teste compilável antes da implementação para que o primeiro
// ciclo TDD falhe por expectativa, e não por símbolo inexistente.
#ifndef PCF_VOZ
#define PCF_VOZ (-1)
#endif

void t_pinagem_final_dos_botoes(void)
{
    COMECA("a pinagem final põe o botão de voz no PCF");

    ESPERA_IGUAL(PCF_VOZ, 8);          // P10
    ESPERA_IGUAL(PCF_HALL, 11);        // P13, reservado
    ESPERA_IGUAL(PIN_BOTAO_POWER, 2);
    ESPERA_IGUAL(PIN_PCF_INT, 9);

    TERMINA();
}
