// vista/campos.h — o que MAIS DE UMA tela diz sobre o mesmo item. PURO.
// Bloco que aparece em duas telas mora aqui; texto de tela única, na tela.
//
// RN-4D: dizer o destino é obrigatório — se a tela não disser onde a coisa
// foi parar, a pessoa procura no lugar errado e conclui que não pegou.
#ifndef VISTA_CAMPOS_H
#define VISTA_CAMPOS_H

#include "../nucleo/estado.h"
#include "../uso/nuvem.h"
#include "../nucleo/tipos.h"
#include "../tela/icones.h"

// "Agenda · Tinto", "Tarefas · Minhas tarefas" ou "fora do Google" (a
// anotação, que não existe do outro lado — RN-25).
void vista_destino(const item_t *it, char *out, size_t n);


// "14:00 – 15:00", "14:00" ou "o dia todo". Meia-risca, não hífen (hífen
// entre horas se lê como parte do número). "O dia todo" por extenso: linha
// vazia parece dado faltando.
void vista_faixa(const item_t *it, bool h24, char *out, size_t n);

// O rádio na barra: -1 quando não há nada a dizer, 0-100 quando há. Toda
// vista responde a mesma pergunta.
int vista_wifi_da_barra(const estado_t *e);

// A hora da BARRA: RN-6G (hora que ninguém ajustou não aparece) + o formato
// escolhido. O destino quer `HORA_TEXTO` bytes.
void vista_hora_da_barra(const estado_t *e, char *out, size_t max);

// A hora de um ITEM no formato escolhido; o cartão guarda sempre "14:00".
// O que não for hora ("dia") passa inteiro.
void vista_hora_do_item(const estado_t *e, const char *hhmm,
                        char *out, size_t max);

// O título para lista e detalhe. RN-B8: vazio é normal antes de processar
// ("Gravação 15:08"). Item que existe nunca desenha vazio.
void vista_titulo_do_item(const estado_t *e, const item_t *it,
                          char *out, size_t max);

// A hora de um campo editável: "14" em 24 h, "2 pm" em 12 h. O campo segue
// de 0 a 23 e o meridiano vira sozinho: sem um seletor am/pm a mais.
void vista_hora_do_campo(const estado_t *e, int hora, char *out, size_t max);

// A força do sinal em PALAVRA. Aqui porque quem decide redesenhar também
// precisa dela: o que conta é trocar de palavra, não oscilar o dBm.
const char *vista_forca_texto(int forca);

// Vale animar a barra por esta espera? Só pelo que a pessoa disparou (fala,
// gesto, agenda, catálogo). Pull e registro são invisíveis: animar por eles
// montava a vista a cada segundo.
bool vista_espera_visivel(nuvem_espera_t o_que);

// O que a sincronização faz, como ÍCONE: quatro formas distintas, nunca uma
// girando (em e-ink, coisa que gira vira borrão). `ICO_NENHUM` é o normal.
icone_id vista_sinc_da_barra(const estado_t *e);

// O aparelho espera alguma coisa (redes, servidor, hora)? Um lugar só, para
// toda tela mostrar o indicador na mesma espera.
bool vista_ocupado(const estado_t *e);


// Caixa alta que entende português: o `a-z` ASCII não alcança "ç" nem
// acentos (UTF-8, dois bytes), e "terça" saía "TERçA". PURO.
void vista_maiuscula(char *s);

// ── esta tarefa pertence a ESTE dia? ─────────────────────────────────
// Uma regra, um lugar (a Agenda, o dia e a tira já discordaram):
//
//   · com HORA (o híbrido) é compromisso: mora na régua do dia dela e não
//     entra aqui, nem vira atrasada. Só um PRAZO próprio, diferente do dia
//     da hora, a traz para cá;
//   · a CONCLUÍDA mora no dia em que foi feita;
//   · a com PRAZO aparece no dia do prazo;
//   · a SEM PRAZO não pertence a dia nenhum.
//
// `acompanha`: a Agenda e o bloqueio são a página de trabalho, e mostram em
// HOJE também a sem prazo e a de prazo ainda aberta. O dia mostra só o dele.
bool vista_tarefa_no_dia(const estado_t *e, const item_t *it, data_t dia,
                         bool acompanha);

// A regra da rotina em palavras ("todo dia", "seg, qua, sex"). Vazio quando
// não se repete.
void vista_rotina_em_palavras(const char *regra, char *out, size_t max);

// "há 2 min", "agora", "nunca": de quando é a última sincronia.
void vista_quando_sincronizou(const estado_t *e, char *fora, size_t n);

// Quanto falta: "agora", "em 40 min", "em 2 h", "em 4h46". Um formato para
// a Agenda, o bloqueio e a dock.
void vista_falta(int minutos, char *out, size_t max);

// "setembro" → "Setembro", para título.
void vista_maiuscula1(char *s);

#endif
