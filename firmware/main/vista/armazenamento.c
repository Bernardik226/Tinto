#include "armazenamento.h"
#include "campos.h"
#include <stdio.h>
#include <string.h>

// "5,1 GB", "250 MB": uma casa decimal no GB e nenhuma no MB, onde a
// diferença importa.
static void tamanho(uint32_t kb, char *fora, size_t n)
{
    if (kb >= 1024u * 1024u) {
        unsigned dec = (unsigned)(kb / (1024u * 1024u / 10u));
        snprintf(fora, n, "%u,%u GB", dec / 10u, dec % 10u);
    } else {
        snprintf(fora, n, "%u MB", (unsigned)(kb / 1024u));
    }
}

// ── Armazenamento ────────────────────────────────────────────────────
// A pergunta é "ainda dá para falar?": o LIVRE é o número grande, e o usado
// vem na frase que o explica. Só Restaurar recebe seleção.
void vista_armazenamento(const estado_t *e, vista_cartao_t *out)
{
    cartao_comeca(e, out, "Armazenamento");
    out->icone   = ICO_CAT_CARTAO;
    snprintf(out->kicker, sizeof out->kicker, "%s", "memória do cartão");
    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK sobre");
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK continuar");

    char v[20], w[20];

    if (e->espaco_total_kb == 0) {
        snprintf(out->nome, sizeof out->nome, "%s", "Não consegui medir");
        snprintf(out->corpo, sizeof out->corpo, "%s",
                 "O cartão não respondeu à medição. Isso costuma ser o "
                 "cartão, e não o que está dentro dele.");
        // Sem medida não se oferece apagar: o problema pode ser o cartão, não o
        // conteúdo.
        return;
    }

    uint32_t livre = e->espaco_total_kb > e->espaco_usado_kb
                   ? e->espaco_total_kb - e->espaco_usado_kb : 0;

    tamanho(livre, v, sizeof v);
    snprintf(out->nome, sizeof out->nome, "%s livres", v);

    tamanho(e->espaco_usado_kb, v, sizeof v);
    tamanho(e->espaco_total_kb, w, sizeof w);
    snprintf(out->corpo, sizeof out->corpo, "%s usados de %s. "
             "Nada é apagado sozinho.", v, w);

    // ── o que ocupa: passivo ─────────────────────────────────────────────
    // Nenhum botão apaga uma delas, então o cursor não para.
    snprintf(out->secao_info, sizeof out->secao_info, "%s", "O QUE OCUPA");

    tamanho(e->uso.itens_kb, v, sizeof v);
    out->info[out->n_info].ico = ICO_MIC;
    snprintf(out->info[out->n_info].rotulo, sizeof out->info[0].rotulo,
             "%s", "Notas e áudio");
    snprintf(out->info[out->n_info].sub, sizeof out->info[0].sub,
             "%s", "Capturas locais");
    snprintf(out->info[out->n_info].valor, sizeof out->info[0].valor, "%s", v);
    out->n_info++;

    tamanho(e->uso.acervo_kb, v, sizeof v);
    out->info[out->n_info].ico = ICO_NOTA;
    snprintf(out->info[out->n_info].rotulo, sizeof out->info[0].rotulo,
             "%s", "Acervo");
    snprintf(out->info[out->n_info].sub, sizeof out->info[0].sub,
             "%s", "Livros e textos");
    snprintf(out->info[out->n_info].valor, sizeof out->info[0].valor, "%s", v);
    out->n_info++;

    uint32_t sistema = e->espaco_usado_kb > e->uso.itens_kb + e->uso.acervo_kb
                     ? e->espaco_usado_kb - e->uso.itens_kb - e->uso.acervo_kb
                     : 0;
    tamanho(sistema, v, sizeof v);
    out->info[out->n_info].ico = ICO_AJUSTES;
    snprintf(out->info[out->n_info].rotulo, sizeof out->info[0].rotulo,
             "%s", "Sistema");
    snprintf(out->info[out->n_info].valor, sizeof out->info[0].valor, "%s", v);
    out->n_info++;

    // ── o que se aperta ──────────────────────────────────────────────────
    // Uma coisa só. Quem leva aqui é o Sobre (o hub).
    snprintf(out->secao, sizeof out->secao, "%s", "RECOMEÇAR");
    out->dest[0].ico = ICO_LIXO;
    snprintf(out->dest[0].titulo, sizeof out->dest[0].titulo,
             "%s", "Restaurar aparelho");
    snprintf(out->dest[0].sub, sizeof out->dest[0].sub,
             "%s", "Apagar conteúdo local");
    out->n_dest = 1;
}
