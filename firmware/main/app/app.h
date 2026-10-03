// app/app.h — o laço e o roteamento. Só ele conhece todas as camadas.
// Recebe evento e decide o que acontece; nunca lê arquivo, desenha pixel nem
// formata texto.
#ifndef APP_H
#define APP_H

#include "../nucleo/estado.h"
#include "../hal/hal.h"
#include "../tela/bitmap.h"
#include "../jogos/xadrez_maquina.h"

typedef struct {
    estado_t      estado;
    const hal_t  *hal;
    bitmap_t      tela;
    uint8_t       memoria_tela[(TELA_L + 7) / 8 * TELA_A];  // estático. ENGENHARIA §3
    bool          precisa_desenhar;

    // Que tela foi ao vidro da última vez: o quadro novo é outra tela ou a
    // mesma com o cursor noutro lugar? (hal/refresco.h)
    uint32_t      ultima_assinatura;

    // O foco da Home no quadro anterior: só "movi o cartão" tem parcial
    // garantido.
    int8_t        ultimo_lancador;

    // O overlay que está NO VIDRO: abrir uma caixa pede limpar a região antes
    // (EINK.md §5.6).
    overlay_id    ultimo_overlay;

    // A tela do quadro anterior: SAIR do teclado (a tela mais densa) pede a
    // waveform completa para apagar a grade.
    tela_id       tela_anterior;

    // A PÁGINA de leitura do quadro anterior (-1 = não pagina). Virar página
    // troca todo o conteúdo do retângulo.
    int16_t       ultima_pagina;

    // As superfícies do xadrez (menu, tabuleiro, confirmação, resultado) trocam
    // com o ritual de tela nova.
    uint8_t       ultima_pagina_xadrez;
    bool          xadrez_girou_tela;

    // O cursor do quadro anterior: quadro de NAVEGAÇÃO é parcial por decreto.
    int16_t       ultimo_cursor;
    int32_t       ultima_posicao_leitor;
    bool          ultima_capa_leitor;


    // O texto da obra aberta, emprestado da PSRAM. No app e não no estado:
    // quem empresta e devolve é o app.
    char *texto_emprestado;
    bool leitor_meta_pendente;

    // A busca da máquina: trabalho retomável do app, em fatias.
    xadrez_maquina_t maquina_xadrez;
} app_t;

// O retângulo da peça sobreposta, ou false. O motor pergunta antes de
// montar o plano (parcial de retângulo); o teste de invasão também.
bool app_caixa_area(app_t *ap, int *x, int *y, int *l, int *a);

void app_liga  (app_t *ap, const hal_t *hal);
void app_evento(app_t *ap, const evento_t *ev);
void app_desenha(app_t *ap);

// Consome a fila e desenha se algo mudou. Na placa é o corpo da task APP;
// no PC, o harness chama direto.
void app_passo(app_t *ap);

#endif
