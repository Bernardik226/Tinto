// ui/blocos.h — o que várias telas repetem. UI.md nível 3.
#ifndef UI_BLOCOS_H
#define UI_BLOCOS_H

#include "../vista/linhas.h"
#include "../tela/bitmap.h"

// O compromisso na régua do dia. `primeiro` e `ultimo` dizem onde a linha
// da régua nasce e morre (é contínua). `col_h` é a largura da calha da hora,
// medida na página: em 24 h é menor e devolve pixels ao título.
int bloco_timeline(bitmap_t *bm, int y, const linha_captura_t *l,
                   bool cursor, bool primeiro, bool ultimo, int col_h);
int bloco_acao   (bitmap_t *bm, int y, const linha_acao_t *a,          bool cursor);

// Uma linha que LEVA a outra tela: ícone, título, descrição e seta. Focada,
// inverte inteira. `serif` na raiz de Ajustes.
int bloco_destino(bitmap_t *bm, int y, int larg, icone_id ico,
                  const char *titulo, const char *sub, const char *valor,
                  bool foco, bool serif);

// A mesma linha com a geometria dada de fora, para desenhar dentro de uma
// caixa que não é a tela inteira.
int bloco_acao_em(bitmap_t *bm, int x, int y, int larg,
                  const linha_acao_t *a, bool cursor);

// Quanto o rótulo ocupa depois de ceder espaço ao valor: a regra "um não
// invade o outro" com teste.
int bloco_acao_largura_do_texto(const linha_acao_t *a, int larg);

#endif
