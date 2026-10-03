#include "confirma.h"
#include "campos.h"
#include <stdio.h>
#include <string.h>

void vista_desconectar(const estado_t *e, vista_confirma_t *out)
{
    memset(out, 0, sizeof *out);
    snprintf(out->titulo, sizeof out->titulo, "%s", "Desconectar");
    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    out->bateria = e->bateria;
    out->wifi = vista_wifi_da_barra(e);
    out->sinc = vista_sinc_da_barra(e);

    snprintf(out->pergunta, sizeof out->pergunta, "%s",
             "Desconectar esta conta?");

    // A segunda frase importa: o medo é que as notas vão junto, e não vão.
    snprintf(out->explica, sizeof out->explica, "%s",
             "O Tinto para de sincronizar. O que já está no cartão fica — "
             "o dia, as tarefas e as anotações continuam aí. "
             "No aplicativo, este aparelho sai da sua conta.");

    snprintf(out->nao, sizeof out->nao, "%s", "não, voltar");
    snprintf(out->sim, sizeof out->sim, "%s", "sim, desconectar");

    out->cursor = e->cursor_overlay ? 1 : 0;
    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK voltar");
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK escolher");
}

// ── esquecer a rede ──────────────────────────────────────────────────
// A opção sem perda nasce selecionada, e a destrutiva REPETE o objeto ("Sim,
// esquecer Casa"): é a última chance de ver que é a rede errada.
void vista_esquecer_rede(const estado_t *e, vista_confirma_t *out)
{
    memset(out, 0, sizeof *out);
    snprintf(out->titulo, sizeof out->titulo, "%s", "Esquecer rede");
    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    out->bateria = e->bateria;
    out->wifi = vista_wifi_da_barra(e);
    out->sinc = vista_sinc_da_barra(e);

    const char *rede = e->wifi_salva[0] ? e->wifi_salva : e->wifi_atual;

    snprintf(out->pergunta, sizeof out->pergunta, "Esquecer \"%s\"?", rede);

    // O que se perde e o que custa desfazer: a senha vai junto.
    snprintf(out->explica, sizeof out->explica, "%s",
             "O Tinto deixa de conectar sozinho a esta rede. Para usá-la "
             "de novo, você precisa escolhê-la na lista e digitar a senha "
             "outra vez.");

    snprintf(out->nao, sizeof out->nao, "%s", "Não, manter a rede");
    snprintf(out->sim, sizeof out->sim, "Sim, esquecer \"%s\"", rede);

    out->cursor = e->cursor_overlay ? 1 : 0;
    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK cancelar");
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK confirmar");
}

// ── restaurar o aparelho ─────────────────────────────────────────────
// A MESMA confirmação das outras destrutivas: três caras diferentes fariam a
// pessoa não reconhecer, de relance, a tela que pede cuidado.
void vista_restaurar(const estado_t *e, vista_confirma_t *out)
{
    memset(out, 0, sizeof *out);
    snprintf(out->titulo, sizeof out->titulo, "%s", "Restaurar");
    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    out->bateria = e->bateria;
    out->wifi = vista_wifi_da_barra(e);
    out->sinc = vista_sinc_da_barra(e);

    snprintf(out->pergunta, sizeof out->pergunta, "%s",
             "Apagar e configurar de novo?");

    // Os TRÊS grupos que saem, e a frase que evita o pânico: a agenda não vai
    // junto.
    snprintf(out->explica, sizeof out->explica, "%s",
             "Saem deste aparelho: nome, notas e gravações; livros e "
             "progresso de leitura; Wi-Fi, conta e preferências. "
             "Nada será apagado da sua conta Google.");

    snprintf(out->nao, sizeof out->nao, "%s", "Não, voltar");
    snprintf(out->sim, sizeof out->sim, "%s", "Sim, restaurar aparelho");

    // O mesmo campo do fluxo de inicialização: uma fonte só para a escolha.
    out->cursor = e->inicio.cursor ? 1 : 0;
    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK cancelar");
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK confirmar");
}

// ── apagar uma ROTINA ────────────────────────────────────────────────
// O item guardado é a REGRA: apagá-lo apaga a série inteira no Google.
void vista_apagar_rotina(const estado_t *e, vista_confirma_t *out)
{
    memset(out, 0, sizeof *out);
    snprintf(out->titulo, sizeof out->titulo, "%s", "Apagar rotina");
    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    out->bateria = e->bateria;
    out->wifi = vista_wifi_da_barra(e);
    out->sinc = vista_sinc_da_barra(e);
    snprintf(out->pergunta, sizeof out->pergunta, "%s",
             "Apagar todas as repetições?");
    // O título da coisa dentro da frase: qual rotina.
    snprintf(out->explica, sizeof out->explica,
             "\"%s\" se repete, e o Tinto só sabe apagar a série inteira. "
             "Para tirar um dia só, use o Google Agenda no celular.",
             e->aberto.titulo);

    snprintf(out->nao, sizeof out->nao, "%s", "Não, manter");
    snprintf(out->sim, sizeof out->sim, "%s", "Sim, apagar todas");
    out->cursor = e->cursor_overlay ? 1 : 0;
    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK cancelar");
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK confirmar");
}

void vista_descartar_obra(const estado_t *e, vista_confirma_t *out)
{
    memset(out, 0, sizeof *out);
    snprintf(out->titulo, sizeof out->titulo, "%s", "Descartar livro");
    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    out->bateria = e->bateria;
    out->wifi = vista_wifi_da_barra(e);
    out->sinc = vista_sinc_da_barra(e);
    snprintf(out->pergunta, sizeof out->pergunta, "%s",
             "Descartar deste Tinto?");
    snprintf(out->explica, sizeof out->explica, "%s",
             "A cópia e o progresso saem apenas deste aparelho. Se a obra "
             "continuar no seu Acervo online, você poderá baixá-la novamente.");
    snprintf(out->nao, sizeof out->nao, "%s", "Não, manter");
    snprintf(out->sim, sizeof out->sim, "%s", "Sim, descartar");
    out->cursor = e->cursor_overlay ? 1 : 0;
    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK cancelar");
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK confirmar");
}
