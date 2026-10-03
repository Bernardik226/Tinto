#include "sobre.h"
#include "campos.h"
#include "../nucleo/data.h"
#include <stdio.h>
#include <string.h>

#define VERSAO TINTO_VERSAO

const char *vista_sobre_versao(void) { return VERSAO; }

// ── Sobre ────────────────────────────────────────────────────────────
// Não repete bateria, rede nem conta (cada uma tem sua tela). Sobra produto,
// documentos e suporte.
void vista_sobre(const estado_t *e, vista_cartao_t *out)
{
    cartao_comeca(e, out, "Sobre");
    out->icone   = ICO_TINTO;
    // A MARCA: o assunto desta tela é o aparelho.
    out->logo = true;
    snprintf(out->rodape_esq, sizeof out->rodape_esq, "%s", "BACK ajustes");
    snprintf(out->rodape_dir, sizeof out->rodape_dir, "%s", "OK abrir");

    snprintf(out->fatos[out->n_fatos].rotulo, sizeof out->fatos[0].rotulo,
             "%s", "Versão");
    snprintf(out->fatos[out->n_fatos].valor, sizeof out->fatos[0].valor,
             "%s", VERSAO);
    out->n_fatos++;

    // A identificação mora em Minha conta, não aqui.

    // O último erro, e "nenhum" quando não houve: campo que só aparece na
    // falha ninguém aprende a ler.
    snprintf(out->fatos[out->n_fatos].rotulo, sizeof out->fatos[0].rotulo,
             "%s", "Último erro");
    snprintf(out->fatos[out->n_fatos].valor, sizeof out->fatos[0].valor,
             "%s", e->ultimo_erro == OK ? "nenhum" : "houve um");
    out->n_fatos++;

    // ── o HUB do aparelho ────────────────────────────────────────────────
    // Daqui: quanto cabe e como atualizar. A licença mora no repositório.
    snprintf(out->secao, sizeof out->secao, "%s", "APARELHO");

    out->dest[out->n_dest].ico = ICO_CAT_CARTAO;
    snprintf(out->dest[out->n_dest].titulo, sizeof out->dest[0].titulo,
             "%s", "Armazenamento");
    snprintf(out->dest[out->n_dest].sub, sizeof out->dest[0].sub,
             "%s", "Espaço e o que ocupa");
    out->n_dest++;
}
