// vista/lancador.c — a Home 2x2. PURO.
#include "lancador.h"
#include "campos.h"
#include "../nucleo/data.h"

#include <stdio.h>
#include <string.h>

// ── os quatro destinos ───────────────────────────────────────────────
// Tabela de DADO: rótulo é apresentação, e o roteamento mora em app/. A
// legenda diz o que se acha lá dentro. Cabe em 88 px de miúda, medida por
// `gfx_largura` (o teste confere), não pelo navegador do desenho.
static const cartao_lancador_t CARTOES[LANCADOR_CARTOES] = {
    { "Agenda",  "Hoje",         ICO_AREA_AGENDA  },
    { "Acervo",  "Seus livros",  ICO_AREA_ACERVO  },
    { "Jogos",   "Xadrez",       ICO_AREA_JOGOS   },
    { "Ajustes", "O Meu Tinto",  ICO_AREA_AJUSTES },
};


// ── a saudação ───────────────────────────────────────────────────────
// RN-6G: sem hora confiável, "Olá." — nunca "Boa noite" às três da tarde. Na
// mesma linha, para a composição não dançar.
static const char *saudacao_da_hora(const estado_t *e)
{
    if (!e->hora_confiavel) return "Olá.";
    if (e->hora < 12)       return "Bom dia.";
    if (e->hora < 18)       return "Boa tarde.";
    return "Boa noite.";
}

void vista_lancador(const estado_t *e, vista_lancador_t *out)
{
    memset(out, 0, sizeof *out);

    // ── a barra ──
    // Na Home a esquerda é a DATA.
    snprintf(out->data_curta, sizeof out->data_curta, "%s %d %s",
             data_semana_curta(e->hoje), e->hoje.dia, data_mes_curto(e->hoje));
    vista_maiuscula(out->data_curta);

    vista_hora_da_barra(e, out->hora, sizeof out->hora);

    out->bateria      = e->bateria;
    out->wifi         = vista_wifi_da_barra(e);
    out->sinc = vista_sinc_da_barra(e);

    // ── o cabeçalho ──
    // O NOME DO DONO (perfil), não o e-mail da conta (`estado.nome`). Sem dono,
    // o genérico. A vista entrega com e sem a moldura "TINTO DE", e a tela
    // escolhe pela largura.
    if (e->inicio.nome_pendente[0]) {
        snprintf(out->sobre, sizeof out->sobre, "Tinto de %s",
                 e->inicio.nome_pendente);
        snprintf(out->dono,  sizeof out->dono,  "%s",
                 e->inicio.nome_pendente);
        vista_maiuscula(out->dono);
    } else {
        snprintf(out->sobre, sizeof out->sobre, "%s", "seu tinto");
    }
    vista_maiuscula(out->sobre);
    snprintf(out->saudacao, sizeof out->saudacao, "%s", saudacao_da_hora(e));

    // ── a grade ──
    memcpy(out->cartoes, CARTOES, sizeof CARTOES);

    // O foco é do ESTADO: tela e vista perguntam ao mesmo lugar.
    out->foco = e->lancador;
    if (out->foco < 0)                 out->foco = 0;
    if (out->foco >= LANCADOR_CARTOES) out->foco = LANCADOR_CARTOES - 1;

    // ── o rodapé ──
    // Esquerda VAZIA: na Home o BACK não faz nada, e não se mostra ação
    // inexistente.
    out->rodape_esq[0] = '\0';
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK abrir");
}
