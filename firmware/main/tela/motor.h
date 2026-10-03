#ifndef TELA_MOTOR_H
#define TELA_MOTOR_H

// O motor de quadros: quantos quadros um gesto vale, e o que cada um leva.
//
// Um dono só do display recebe o evento e devolve um PLANO; quem executa não
// decide nada. A decisão morava em três lugares (app, refresco.c, hal_esp.c)
// que se cruzavam por acaso. Só booleanos: não conhece tela, cursor nem
// painel, e por isso tem teste no PC.

#include <stdbool.h>

// Dois: os rituais que custam um quadro extra (§5.5 o seletor, §5.6 o chão
// da caixa) nunca acontecem juntos.
#define MOTOR_MAX_QUADROS 2

// O que o app sabe e o motor não tem como deduzir.
typedef struct {
    // A única informação que não está no bitmap.
    bool trocou_de_tela;

    // Se a tela tem seletor (§5.5).
    bool tem_cursor;

    // Pop-over ou gaveta ACABOU de abrir. Fechar não conta: a caixa é tinta
    // leve, e o parcial a apaga.
    bool abriu_caixa;

    // Há caixa com geometria conhecida para limpar.
    bool tem_caixa;
} motor_fatos_t;

// Um quadro do plano: três instruções para quem MONTA o bitmap e o que
// segue para o painel.
typedef struct {
    // Monte com o cursor fora da lista (`cursor = -1`): o mesmo desenho sem o
    // seletor.
    bool sem_seletor;

    // Monte a tela de trás sem o pop-over por cima.
    bool sem_overlay;

    // Depois de montar, apague o retângulo da caixa (o chão do §5.6). Anda
    // junto de `sem_overlay`.
    bool limpa_o_chao;

    // O que vai no `hal->mostrar`. Só o primeiro quadro de uma troca leva
    // `true`: o segundo é o seletor chegando por cima.
    bool trocou_de_tela;
} quadro_t;

typedef struct {
    quadro_t quadro[MOTOR_MAX_QUADROS];
    int n;
} plano_t;

// Nunca vazio, e o último quadro é SEMPRE a tela como fica. Quem descarta
// quadro que não move tinta é o painel.
plano_t motor_plano(const motor_fatos_t *f);

#endif
