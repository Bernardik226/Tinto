# A camada de tela

Como `ui/` e `tela/` se organizam por dentro. O [CAMADAS.md](CAMADAS.md) diz
que a saída recebe uma struct pronta e pinta; este documento diz como.

**Quatro níveis, cada um só usa o de baixo.** O nível 1 mora em `tela/`; os
níveis 2, 3 e 4, em `ui/`.

---

## Os quatro níveis

```
  ┌──────────────────────────────────────────────┐
  │ 4 · TELAS      uma função, uma struct        │  ui/agenda.c, ui/nota.c…
  └──────────────────────────────────────────────┘
                      │ usa
  ┌──────────────────────────────────────────────┐
  │ 3 · BLOCOS     o que várias telas repetem    │  ui/blocos.c
  └──────────────────────────────────────────────┘
                      │ usa
  ┌──────────────────────────────────────────────┐
  │ 2 · CHROME     as peças do sistema Tinto     │  ui/chrome.c
  └──────────────────────────────────────────────┘
                      │ usa
  ┌──────────────────────────────────────────────┐
  │ 1 · TINTA      primitivas, sem saber do Tinto│  tela/
  └──────────────────────────────────────────────┘
```

| Nível | Sabe o que é… | **Não** sabe |
|---|---|---|
| **tela/** | pixel, retângulo, glifo, fonte | que existe uma barra de título |
| **chrome** | barra, rodapé, filete, cursor, selo, caixa | que existe uma tarefa |
| **blocos** | linha de tarefa, cabeça de item, linha de ação | qual tela está aberta |
| **telas** | a composição de uma tela inteira | de onde vieram os dados |

### A regra que sustenta o empilhamento

**Toda função de desenho recebe um `y` e devolve o `y` de baixo.**

```c
int y = 0;
y = chrome_barra(bm, "Tarefa", &v->barra);
y = bloco_cabeca(bm, y, &v->cabeca);
y = chrome_filete(bm, y, FILETE_FINO);
for (int i = 0; i < v->n_acoes; i++)
    y = bloco_acao(bm, y, &v->acoes[i]);
chrome_rodape(bm, "◀ voltar", "OK marcar feita");
```

**Nenhuma tela conhece a altura de nada**: ela empilha e pergunta onde parou.
Mudar o corpo de 12 para 13 pt não deveria tocar em nenhuma tela.

---

## Nível 1 · `tela/`: primitivas

Não conhecem o Tinto; poderiam desenhar qualquer coisa. Pixel, retângulo,
linhas, inversão de região, texto com tracking entre pares, quebra de
parágrafo e ícones.

As fontes são geradas no tamanho de uso por `firmware/ferramentas/fontes.py`, a
partir dos TTFs em `firmware/assets/fontes/`. Os ícones saem de
`firmware/ferramentas/icones.py`.

**`gfx_texto` falha alto em glifo faltante.** Em modo de prova, aborta com o
caractere e a fonte: glifo pulado em silêncio produz um texto que "quase"
sai certo, e nenhum teste pega.

---

## Nível 2 · `ui/chrome.c`: as peças do sistema

Conhecem a linguagem visual do Tinto, não o conteúdo: barra de título,
rodapé, filete, cursor, selo, caixa de marcar, ícone de tipo, confirmação.

### O HUD: a moldura que não muda

A barra de cima e o rodapé são **iguais em toda tela**, desenhados só por
`chrome_barra` e `chrome_rodape`. Nenhuma tela escreve nessas faixas por
conta própria: a moldura é o que faz doze telas parecerem um aparelho só.

#### Barra de título, 23 px, clara

```
┌───────────────────────────────────────┐
│ QUA 12 AGO            ⇅ ((( 07:41 ▮   │
└───────────────────────────────────────┘
  └ o que é esta tela      └ o estado do aparelho
```

**Esquerda:** o nome da tela em caixa alta. Na agenda, a data.

**Direita, nesta ordem fixa:** `[sync] [wi-fi] [hora] [bateria]`. Sync e
Wi-Fi aparecem e somem, e ficam à esquerda justamente para que hora e
bateria **nunca se movam**.

| Indicador | Estados | Como se lê em 1 bit |
|---|---|---|
| **sync** | ocioso *(some)* · subindo · descendo · processando · erro | formas distintas, nunca uma girando — em e-ink animação vira borrão |
| **wi-fi** | desligado · conectando · forte · médio · fraco · sem sinal | leque; a força se lê pela contagem de arcos |
| **hora** | sempre | **nunca some** |
| **bateria** | níveis · vazia · carregando | pilha em pé |

#### Rodapé, 17 px

```
┌───────────────────────────────────────┐
│ ◀ calendário               OK marcar  │
└───────────────────────────────────────┘
  └ a SAÍDA                  └ o que o OK faz AGORA
```

**Esquerda é sempre a saída**, para onde o BACK leva. **Direita é sempre o
que o OK faz na linha do cursor.** Sem a saída escrita, quem entrou fundo não
sabe como sair.

Só aparecem BACK, OK e, quando a tela usa, ◀ e ▶. MENU, voz e power
fazem a mesma coisa em toda tela e não gastam o rodapé.

O rodapé fica **em negativo quando o aparelho está fazendo algo que continua
se você sair** — hoje, só gravando. Negativo é estado ativo, não affordance.

---

## Nível 3 · `ui/blocos.c`: o que várias telas repetem

Um bloco só existe se aparece em **duas ou mais** telas; bloco de tela única
mora na própria tela.

| Bloco | Aparece em |
|---|---|
| Linha de tarefa — caixa · texto · selo | agenda · tarefa · dia |
| Cabeça de item — título grande · metadados | nota · tarefa |
| Bloco rotulado — rótulo miúdo + corpo | nota · tarefa · ajustes · recibo |
| Citação — a fala crua | nota · tarefa · conferir |
| Linha de ação — texto + valor à direita | tarefa · ajustes · conta |
| Linha de opção — caixa + texto + subtexto | ajustes |
| Resultado — ícone · verbo · título · destino | recibo |

---

## O vocabulário visual

**A espera tem uma forma só: três pontinhos que andam.** Numa tela de tinta,
esperar e travar têm a mesma cara, e sem nada se movendo a pessoa aperta de
novo. Os pontinhos andam com o relógio (`agora_ms / 1000 % 4`), nunca com um
contador de desenhos, e o quadro é um parcial só da faixa que muda. Nada de
"…" parado no texto.

**Seleção é inversão, sempre.** Fundo preto, texto e ícone brancos, a peça
inteira. Sem moldura de foco, sem sublinhado.

**Três formas, três significados:**

| Forma | O que é | Onde |
|---|---|---|
| **cartão** 106×136 | uma área do aparelho | a Home 2×2 |
| **destino**, linha de 48 px | uma tela dentro da área | raiz de Ajustes |
| **linha**, ~28 px | um valor que muda ali mesmo | Som, Aparência |

*Para onde eu vou* contra *o que eu mudo aqui*: se as duas parecessem
iguais, o OK viraria loteria.

**As medidas moram em `ui/grid.h`, e só lá.** Uma medida escrita dentro da
tela que a usou primeiro é uma medida que a segunda tela vai adivinhar.

**Papéis de fonte:**

| Fonte | Papel |
|---|---|
| `F_EDITORIAL` | título de destino, nome de item, saudação |
| `F_TITULO` | o tempo da gravação, a marca de um estado |
| `F_CORPO` | o valor principal de um campo |
| `F_CORPO_P` | o texto das listas |
| `F_MIUDA` | rótulos, descrições, o rodapé |
| `F_CITACAO` | a fala crua, e nada mais |

**O que não existe:** sombra, cinza, trama, canto arredondado, toggle. O
painel tem duas cores, e cada uma significa uma coisa só.

---

## Nível 4 · `ui/<tela>.c`: uma função, uma struct

```c
void tela_agenda  (bitmap_t *bm, const vista_agenda_t *v);
void tela_nota  (bitmap_t *bm, const vista_nota_t *v);
/* ... */
```

Um arquivo por tela, curto. Se crescer demais, ou nasceu um bloco que outras
telas vão querer, ou entrou decisão que é da `vista/`.

### O contrato, em três regras

**1. A tela não decide nada sobre dado.**

```c
// ERRADO — regra de negócio dentro do desenho
if (item->vence_em < hoje) desenha_selo("atrasada");

// CERTO — vista/ já resolveu; a tela só pergunta se o campo existe
if (l->selo[0]) chrome_selo(bm, x, y, l->selo, l->selo_neg);
```

A tela pergunta **"este campo existe?"**, nunca **"este dado significa o
quê?"**.

**2. A tela não lê nada.** Sem cartão, sem relógio, sem estado global.

**3. A struct de vista é plana e já formatada.** `"14:00"`, não
`{h:14,m:0}`. Quem formata é `vista/`, porque formatar é decidir, e decidir
é testável no PC.

---

## O primeiro uso

A recuperação da memória e o primeiro uso ficam **acima da pilha normal**:

```text
BOOT → verificar memória
         ├─ falha  → recuperação
         └─ pronta → boas-vindas → Termos → Aviso → nome → conclusão → HOME
```

A fase visível é sempre a primeira pendência gravada no cartão. Reiniciar
retoma dali, sem repetir etapa fechada. Antes da conclusão, voz e MENU não
escapam do fluxo.

---

## Como se acrescenta uma tela

1. Desenhe a tela antes do código.
2. Defina `vista_<x>_t`, plana e já formatada.
3. Escreva `vista/<x>.c` **e o teste dele**, que roda no PC.
4. Escreva `ui/<x>.c`, só composição.
5. Gere o PNG com `make provas` e compare com o desenho.
6. Só então ligue no roteamento de `app/`.

Fazendo 4 antes de 3, a tela vira o lugar onde a decisão mora.

---

## Os cinco cheiros

| Cheiro | O que significa |
|---|---|
| Uma tela desenha a própria barra de título | extrair para o chrome |
| Um `if` sobre data, contagem ou estado dentro de `ui/` | regra de negócio vazou para o desenho |
| Duas telas com a mesma sequência de 5+ linhas | nasceu um bloco |
| Uma constante de altura escrita à mão | o empilhamento por `y` foi furado |
| `ui/` incluindo `dado/` ou ESP-IDF | quebrou a camada — e o build do PC, que é o alarme |
