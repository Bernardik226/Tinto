#include "teste.h"
#include "tela/motor.h"

// ── o motor de quadros ──────────────────────────────────────────────
// Quantos quadros um gesto vale e o que cada um leva, num dono só que
// devolve um PLANO. Só booleanos: roda inteiro no PC.

void t_motor_a_mesma_tela_e_um_quadro_so(void)
{
    COMECA("andar dentro da tela é UM quadro, e ele é parcial");
    // O caso comum (cursor, rolagem, teclado): um quadro.
    motor_fatos_t f = { .trocou_de_tela = false, .tem_cursor = true };
    plano_t p = motor_plano(&f);

    ESPERA(p.n == 1);
    ESPERA(p.quadro[0].trocou_de_tela == false);
    ESPERA(p.quadro[0].sem_seletor == false);
    TERMINA();
}

void t_motor_tela_sem_cursor_nao_paga_o_segundo_quadro(void)
{
    COMECA("tela SEM cursor troca em um quadro, não dois");
    // Tela nova sem cursor: um quadro só (dois iguais era montar a vista para
    // jogar fora).
    motor_fatos_t f = { .trocou_de_tela = true, .tem_cursor = false };
    plano_t p = motor_plano(&f);

    ESPERA(p.n == 1);
    ESPERA(p.quadro[0].trocou_de_tela == true);
    TERMINA();
}

void t_motor_tela_com_cursor_pinta_o_seletor_depois(void)
{
    COMECA("tela COM cursor: o completo pinta só o que vai ficar");
    // EINK §5.5: com cursor, o seletor vem num segundo quadro parcial.
    motor_fatos_t f = { .trocou_de_tela = true, .tem_cursor = true };
    plano_t p = motor_plano(&f);

    ESPERA(p.n == 2);
    ESPERA(p.quadro[0].trocou_de_tela == true);
    ESPERA(p.quadro[0].sem_seletor    == true);
    ESPERA(p.quadro[1].trocou_de_tela == false);
    ESPERA(p.quadro[1].sem_seletor    == false);
    TERMINA();
}

void t_motor_abrir_caixa_limpa_o_chao_antes(void)
{
    COMECA("abrir caixa: chão branco primeiro, caixa depois");
    // EINK §5.6: a caixa nova pede limpar a região antes, em dois parciais.
    motor_fatos_t f = { .trocou_de_tela = false, .tem_cursor = true,
                        .abriu_caixa = true, .tem_caixa = true };
    plano_t p = motor_plano(&f);

    ESPERA(p.n == 2);
    ESPERA(p.quadro[0].limpa_o_chao  == true);
    ESPERA(p.quadro[0].sem_overlay   == true);
    ESPERA(p.quadro[1].limpa_o_chao  == false);
    ESPERA(p.quadro[1].sem_overlay   == false);
    // Nenhum é completo: abrir menu não pisca o painel.
    ESPERA(p.quadro[0].trocou_de_tela == false);
    ESPERA(p.quadro[1].trocou_de_tela == false);
    TERMINA();
}

void t_motor_fechar_caixa_e_um_quadro_so(void)
{
    COMECA("fechar caixa não precisa de chão nenhum");
    // Fechar a caixa não precisa de preparação.
    motor_fatos_t f = { .trocou_de_tela = false, .tem_cursor = true,
                        .abriu_caixa = false, .tem_caixa = false };
    plano_t p = motor_plano(&f);

    ESPERA(p.n == 1);
    ESPERA(p.quadro[0].limpa_o_chao == false);
    TERMINA();
}

void t_motor_trocar_de_tela_com_caixa_nao_soma_os_dois_rituais(void)
{
    COMECA("trocar de tela com caixa aberta não vira três quadros");
    // Tela nova já entrega chão limpo: sem o ritual da caixa.
    motor_fatos_t f = { .trocou_de_tela = true, .tem_cursor = true,
                        .abriu_caixa = true, .tem_caixa = true };
    plano_t p = motor_plano(&f);

    ESPERA(p.n == 2);
    ESPERA(p.quadro[0].limpa_o_chao == false);
    TERMINA();
}

void t_motor_nunca_devolve_plano_vazio(void)
{
    COMECA("todo gesto vale pelo menos um quadro");
    // Nunca um plano vazio; quem descarta quadro igual é o painel.
    for (int i = 0; i < 16; i++) {
        motor_fatos_t f = {
            .trocou_de_tela = i & 1, .tem_cursor  = i & 2,
            .abriu_caixa    = i & 4, .tem_caixa   = i & 8,
        };
        plano_t p = motor_plano(&f);
        ESPERA(p.n >= 1);
        ESPERA(p.n <= MOTOR_MAX_QUADROS);

        // O último quadro é SEMPRE a tela como ela fica.
        ESPERA(p.quadro[p.n - 1].sem_seletor  == false);
        ESPERA(p.quadro[p.n - 1].sem_overlay  == false);
        ESPERA(p.quadro[p.n - 1].limpa_o_chao == false);
    }
    TERMINA();
}
