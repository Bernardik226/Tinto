#include "gravador.h"
#include "campos.h"
#include <stdio.h>
#include <string.h>

static void mmss(int s, char *out, size_t max)
{
    snprintf(out, max, "%d:%02d", s / 60, s % 60);
}

void vista_gravador(const estado_t *e, vista_grav_t *out)
{
    memset(out, 0, sizeof *out);
    vista_hora_da_barra(e, out->hora, sizeof out->hora);
    out->bateria = e->bateria;
    out->wifi = vista_wifi_da_barra(e);
    out->sinc = vista_sinc_da_barra(e);

    int s = e->gravacao.ms / 1000;
    out->estruturando = e->gravacao.fase == GRAV_ESTRUTURANDO &&
                        !e->gravacao.sem_resposta &&
                        !e->gravacao.nao_enviou &&
                        !e->gravacao.nada_entendido &&
                        e->gravacao.nao_comecou == OK;
    out->sem_resposta = e->gravacao.sem_resposta;
    out->nao_enviou   = e->gravacao.nao_enviou;
    out->nao_comecou  = e->gravacao.nao_comecou;
    out->nada_entendido = e->gravacao.nada_entendido;

    // PAPEL em tudo que não está gravando. Negativo é estado ativo (continua se
    // você sair); pausa e espera não são.
    out->pausado = e->gravacao.fase != GRAV_GRAVANDO;

    // Há gravação ABERTA? "pausado" vale em toda faixa que não grava,
    // inclusive nas de falha.
    out->fase_gravando = e->gravacao.fase == GRAV_GRAVANDO ||
                         e->gravacao.fase == GRAV_PAUSADA;
    out->trechos = e->gravacao.trechos;

    mmss(s, out->tempo, sizeof out->tempo);

    if (e->overlay == OVERLAY_DESCARTAR) {
        // RN-A2: a única confirmação do gesto de falar, com o "não"
        // pré-selecionado.
        out->confirmando = true;
        snprintf(out->titulo,   sizeof out->titulo,   "%s", "Descartar");
        snprintf(out->pergunta, sizeof out->pergunta, "%s",
                 "Descartar esta gravação?");

        // O que se perde EM NÚMERO: "38 s em 3 trechos".
        char t[8];
        mmss(s, t, sizeof t);
        if (out->trechos > 1)
            snprintf(out->perde, sizeof out->perde, "%d segundos em %d trechos.",
                     s, out->trechos);
        else
            snprintf(out->perde, sizeof out->perde, "%d segundos.", s);

        out->sim_selecionado = e->cursor_overlay == 1;
        snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "▲▼ escolher");
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK confirmar");
        return;
    }

    const char *nome = out->nao_comecou != OK ? "Não gravei"
                     : out->nao_enviou     ? "Não enviei"
                     : out->nada_entendido ? "Não entendi"
                     : out->sem_resposta ? "Sem resposta"
                     : out->estruturando ? "Estruturando"
                     : out->pausado      ? "Pausado"
                                         : "Gravando";
    snprintf(out->titulo, sizeof out->titulo, "%s", nome);
    snprintf(out->estado, sizeof out->estado, "%s", nome);

    // Os trechos são a prova de que a retomada entrou no mesmo arquivo (RN-13).
    if (out->trechos > 1) {
        char t[8];
        mmss(s, t, sizeof t);
        snprintf(out->trecho_txt, sizeof out->trecho_txt, "%d trechos · %s",
                 out->trechos, t);
    }

    // Uma fatia por trecho, em segundos: a proporção é a informação.
    out->n_fatias = e->gravacao.trechos < GRAV_MAX_TRECHOS
                  ? e->gravacao.trechos : GRAV_MAX_TRECHOS;
    for (int i = 0; i < out->n_fatias; i++)
        out->fatias[i] = e->gravacao.trecho_s[i];

    // O trecho em curso ainda não está no array, mas é ele que cresce.
    if (!out->pausado && out->n_fatias < GRAV_MAX_TRECHOS) {
        int fechados = 0;
        for (int i = 0; i < out->n_fatias; i++) fechados += out->fatias[i];
        int em_curso = s - fechados;
        if (em_curso > 0) out->fatias[out->n_fatias++] = em_curso;
    }

    // A onda anda com o TEMPO; parada quando pausado (onda com o microfone
    // fechado é mentira).
    out->onda = out->pausado ? 0 : (s % 8);

    // ── a linha que se anima ─────────────────────────────────────────────
    // Os pontos andam com o segundo, um parcial de faixa por segundo: sem nada
    // se mexendo, esperando e travado têm a mesma cara.
    if (out->estruturando) {
        // A frase é FIXA e os pontos andam ao lado: reticências no texto mudavam a
        // largura da frase.
        snprintf(out->aviso, sizeof out->aviso, "%s", "Estruturando");
        uint32_t desde = e->agora_ms - e->gravacao.desde_ms;
        out->pontos = (int)((desde / 1000u) % 3u);
    } else if (out->nao_comecou != OK) {
        // O motivo CONCRETO (cartão, espaço, microfone) e a ação quando há.
        snprintf(out->aviso, sizeof out->aviso, "%s",
                 out->nao_comecou == ERR_SEM_CARTAO
                     ? "Sem cartão não tenho onde gravar."
               : out->nao_comecou == ERR_CHEIO
                     ? "O cartão está cheio. Apague alguma coisa."
               : out->nao_comecou == ERR_SOMENTE_LEITURA
                     ? "O cartão está travado em só leitura."
                     : "O microfone não respondeu.");
    } else if (out->pausado && out->fase_gravando) {
        // PAUSADO tem nome próprio, em negrito; abaixo, leve, o que fazer. Uma
        // linha medida: a segunda encostava no rodapé (213 px contra 210).
        snprintf(out->aviso, sizeof out->aviso, "%s",
                 "Aperte para continuar.");
    } else if (out->nada_entendido) {
        snprintf(out->aviso, sizeof out->aviso, "%s",
                 "Não achei um comando na fala. Ela foi descartada.");
    } else if (out->nao_enviou) {
        // Não saiu, e não ficou: ninguém procura uma fala que foi apagada.
        snprintf(out->aviso, sizeof out->aviso, "%s",
                 "Sem rede na hora de enviar. A gravação foi descartada.");
    } else if (out->sem_resposta) {
        // O servidor não respondeu, a gravação saiu do cartão, e falar de novo é o
        // gesto. Nenhuma tela reprocessa uma captura.
        snprintf(out->aviso, sizeof out->aviso, "%s",
                 "O servidor não respondeu. A gravação foi descartada.");
    }

    // Estruturando: nenhum botão faz nada, então nenhum rótulo; "aguarde" é o
    // estado.
    if (out->estruturando) {
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "aguarde");
        return;
    }
    if (out->sem_resposta || out->nao_enviou || out->nada_entendido ||
        out->nao_comecou != OK) {
        snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK fechar");
        return;
    }

    // A gramática do rodapé: saída à esquerda, o que o OK faz à direita. "solte"
    // só no modo PTT. Sem "●": a F_MIUDA não tem o glifo. A instrução de
    // continuar mora no corpo: no rodapé não cabia.
    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s",
             out->pausado ? "" : "solte pra pausar");
    // O MESMO rótulo gravando e pausado: é o mesmo gesto (fecha o WAV e abre o
    // Conferir). "terminei", na voz de quem fala.
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK terminei");
}
