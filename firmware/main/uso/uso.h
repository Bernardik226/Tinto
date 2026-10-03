// uso/uso.h — os casos de uso. A lista é FECHADA.
//
// Cada função deixa o sistema COERENTE sozinha: grava no cartão, enfileira o
// que sobe e invalida o cache — nunca metade. "Marcar feita" já existiu em
// três lugares e um esquecia de enfileirar. uso/ nunca sabe que tela está
// aberta, onde está o cursor, nem desenha.
#ifndef USO_H
#define USO_H

#include "../nucleo/estado.h"
#include "../hal/hal.h"

// O item é lido pelo par (dia, id); o resto da struct é ignorado: o caso de
// uso relê do cartão, porque gravar cache velho ressuscita campo. Aceita
// ponteiro para dentro do cache: copia dia e id primeiro.
erro_t uso_marcar     (const hal_t *hal, estado_t *e, const item_t *it, bool feita);
erro_t uso_renomear   (const hal_t *hal, estado_t *e, const item_t *it, const char *titulo);
erro_t uso_apagar_item(const hal_t *hal, estado_t *e, const item_t *it);

// Hora vazia = sem hora.
erro_t uso_mudar_data (const hal_t *hal, estado_t *e, const item_t *it,
                       data_t nova, const char *hora);

// Não toca cartão nem fila: andar entre dias não sobe nada.
void   uso_ir_para_dia(estado_t *e, data_t dia);

// Os carregar_* são a contraparte da invalidação, não casos de uso: alguém
// levanta de volta a bandeira do cache, e app/ não lê arquivo.
// As ANOTAÇÕES, das mais novas para as mais velhas, do índice.
erro_t uso_carregar_anotacoes(const hal_t *hal, estado_t *e);

// As tarefas ABERTAS de um intervalo, não do dia.
erro_t uso_carregar_pendentes(const hal_t *hal, estado_t *e);

erro_t uso_carregar_dia(const hal_t *hal, estado_t *e);

// Carrega UM item e o texto dele para o estado (detalhe). Contraparte de
// cache.
erro_t uso_abrir_item(const hal_t *hal, estado_t *e, const item_t *it);

// As marcas do mês do dia visto: uma vez por mês visitado.
erro_t uso_carregar_marcas(const hal_t *hal, estado_t *e);

// Apaga do cartão o que veio de fora e saiu da JANELA (ontem, hoje e
// amanhã): uma rotina diária deixava uma pasta por dia. O que nasceu aqui
// NÃO sai: anotação só existe neste aparelho.
erro_t uso_poda_a_janela(const hal_t *hal, estado_t *e);

// Pede ao servidor o dia aberto, quando fora da janela. Só o OK do
// calendário: andar o cursor não pede nada.
void uso_pedir_o_dia(estado_t *e);

// O dia visto está fora da janela? Aritmética pura.
bool uso_dia_fora_da_janela(const estado_t *e);

// Um gesto mudou o Google: repede o mesmo dia sem apagar o desenhado.
void uso_reconsultar_o_dia(estado_t *e);

// Grava em sistema/config.json e NÃO enfileira: ajuste é do aparelho. O
// valor é aparado para o intervalo da chave.
erro_t uso_salvar_ajuste(const hal_t *hal, estado_t *e,
                         ajuste_t chave, int valor);

// Lê o config na partida.
erro_t uso_carregar_config(const hal_t *hal, estado_t *e);

// ── a fala ───────────────────────────────────────────────────────────
// RN-11: gravar nunca exige escolha (tipo, destino, data). RN-12: pausar não
// decide nada; só o OK finaliza.
erro_t uso_gravar_comeca(const hal_t *hal, estado_t *e);
erro_t uso_gravar_pausa (const hal_t *hal, estado_t *e);
erro_t uso_gravar_retoma(const hal_t *hal, estado_t *e);

// Fecha o WAV, cria o item, enfileira e invalida. RN-66: nunca existe item
// sem áudio e sem texto.
erro_t uso_salvar_captura(const hal_t *hal, estado_t *e);

// Apaga o diretório e NÃO enfileira: o que nunca existiu lá fora não se
// desfaz lá fora.
erro_t uso_descartar_captura(const hal_t *hal, estado_t *e);

// A fala que NÃO conseguiu subir: apaga o WAV e o item recém-criado. Sem
// servidor ele nunca vira nada e ficaria preso em "por transcrever".
erro_t uso_descartar_fala_nao_enviada(const hal_t *hal, estado_t *e);

// ── o resultado da fala: chega, é proposto, e só então vale ──────────
// `propor` guarda na RAM: o que a IA entendeu ainda é sugestão.
// `confirmar` grava, enfileira e invalida, ANTES de voltar — o BACK cai numa
// tela já atualizada. `descartar` joga fora a proposta e a gravação (RN-15:
// nada falado se perde sem a pessoa mandar; aqui ela mandou).
erro_t uso_propor_resultado(estado_t *e, const resultado_t *rs, int n,
                            const char *falou);

// Com o cartão na mão: ações sobre o que já existe ganham o ANTES, que o
// Conferir mostra como "de → para".
erro_t uso_propor_com_antes(const hal_t *hal, estado_t *e,
                            const resultado_t *rs, int n, const char *falou);
erro_t uso_confirmar_resultado(const hal_t *hal, estado_t *e);
erro_t uso_descartar_resultado(const hal_t *hal, estado_t *e);

#endif
