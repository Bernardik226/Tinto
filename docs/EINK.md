# O painel e-ink

Como o quadro sai do framebuffer e vira tinta no vidro: o que o controlador
faz, em que ordem, quem é dono de cada plano, e quando o refresh é parcial ou
completo.

O [UI.md](UI.md) diz como a tela se organiza; este diz **o que o vidro
aceita**. Uma tela que desrespeita este documento sai ilegível por mais
correto que esteja o desenho.

O painel é o **WeAct GDEY037T03** (3.7", 240×416, controlador **UC8253**). O
driver `firmware/main/hal/uc8253.c` foi escrito do zero, a partir de:

| Referência | Onde |
|---|---|
| Datasheet do UC8253 | [UC8253.pdf](https://v4.cecdn.yun300.cn/100001_1909185148/UC8253.pdf) |
| Painel GDEY037T03 | [good-display.com/product/437.html](https://www.good-display.com/product/437.html) |
| GxEPD2 (driver de referência, GPL-3.0) | [github.com/ZinggJM/GxEPD2](https://github.com/ZinggJM/GxEPD2) |

O que o datasheet e o driver de referência não decidem foi medido no vidro,
e está marcado como tal.

---

## 1. O modelo mental: o vidro é o estado

E-ink é o oposto de uma tela comum, e três diferenças decidem a arquitetura:

**1. O vidro guarda a imagem sozinho**, sem energia. Se o firmware perdeu a
conta do que está lá, não há como perguntar: o painel é write-only.

**2. O controlador não pinta pixel: ele roda uma waveform**, uma sequência de
pulsos que empurra a partícula de um extremo ao outro. Leva ~350 ms no
parcial e ~1,5–2,5 s no completo. Interromper no meio deixa tinta cinza.

**3. O refresh é diferencial.** O controlador tem dois planos de SRAM:

| Plano | Comando | O que é |
|---|---|---|
| velho | `0x10` (DTM1) | o que o controlador acredita estar no vidro |
| novo | `0x13` (DTM2) | o que se quer que passe a estar |

O `0x12` (DRF) **compara os dois** e move só os pixels que diferem.

> **Se o plano velho não corresponder ao vidro, a imagem sai sobreposta.** O
> que já estava escrito não é apagado, porque para o controlador aquele pixel
> "não mudou". Letra por cima de letra, quadro anterior aparecendo por baixo:
> **é sempre o plano velho** — não é ghosting, não é SPI, não é lentidão.

---

## 2. O contrato do controlador

| Cmd | Nome | O que faz |
|---|---|---|
| `0x00` | PSR | modo, orientação, booster, soft reset |
| `0x02` | POF | desliga o booster; fecha o ciclo |
| `0x04` | PON | liga o booster; abre o ciclo |
| `0x07` | DSLP | deep sleep (`0xA5`); sai só por reset de hardware |
| `0x10` | DTM1 | escreve o plano velho |
| `0x12` | DRF | roda a waveform |
| `0x13` | DTM2 | escreve o plano novo |
| `0x50` | VCOM/DDX | `0x97` no completo, `0xD7` no parcial |
| `0x61` | TRES | resolução (240×416) |
| `0x90` | PTL | a janela onde a operação vale |
| `0x91` / `0x92` | PTIN / PTOUT | entra / sai do modo parcial |
| `0xE0` | CCSET | `0x02` = usa a temperatura forçada |
| `0xE5` | TSSET | temperatura forçada: `0x6E` parcial, `0x5A` completo |

### O PSR

`0x1F` = `0001 1111`:

| Bits | Campo | Valor | Significado |
|---|---|---|---|
| 7:6 | RES | `00` | sobrescrito pelo TRES |
| 5 | REG | `0` | LUT de fábrica (OTP) |
| 4 | KW/R | `1` | preto e branco: `0x10` é OLD e `0x13` é NEW |
| 3 | UD | `1` | varredura de gate para cima |
| 2 | SHL | `1` | source para a direita |
| 1 | SHD_N | `1` | booster ligado |
| 0 | RST_N | `1` | sem soft reset |

**UD e SHL são a orientação, e é o único lugar onde ela se decide.** Girar em
software é reescrever 12 480 bytes por quadro. As quatro combinações foram
varridas no vidro; `0x1F` é a que se lê.

### A polaridade

No SRAM do painel, **`0` é tinta e `1` é branco**. O framebuffer do Tinto
usa o contrário (`1 = tinta`, `tela/bitmap.c`), então **todo byte é invertido
no caminho**, dentro de `uc8253.c`.

---

## 3. A sequência de um quadro

```
   ┌─ escrita ──────────────────────────────────────────┐
   │  0x91  PTIN            entra em modo parcial       │
   │  0x90  PTL  <janela>   onde os bytes vão cair      │
   │  0x13  DTM2 <quadro>   o plano NOVO                │
   │  0x92  PTOUT           sai                         │
   └────────────────────────────────────────────────────┘
   ┌─ refresh ──────────────────────────────────────────┐
   │  0x91  PTIN            (só no parcial)             │
   │  0x90  PTL  <janela>   (só no parcial)             │
   │  0xE0  CCSET 0x02      temperatura forçada         │
   │  0xE5  TSSET 0x6E      0x6E parcial · 0x5A completo│
   │  0x50  VCOM  0xD7      0xD7 parcial · 0x97 completo│
   │  0x04  PON             liga o booster    → BUSY    │
   │  0x12  DRF             roda a waveform   → BUSY    │
   │  0x02  POF             desliga o booster → BUSY    │
   │  0x00  PSR  0x1E,0x1F  desfaz a temperatura forçada│
   │  0x92  PTOUT           (só no parcial)             │
   └────────────────────────────────────────────────────┘
   ┌─ fechamento ───────────────────────────────────────┐
   │  0x91  PTIN                                        │
   │  0x90  PTL  <a mesma janela>                       │
   │  0x10  DTM1 <o MESMO quadro>   o plano VELHO       │
   │  0x92  PTOUT                                       │
   └────────────────────────────────────────────────────┘
```

### 3.1 O plano velho é escrito depois do refresh, com o mesmo quadro

**O controlador não copia NEW para OLD sozinho.** Quem mantém o plano velho
é o software, no fim de cada ciclo (na GxEPD2, `writeImageAgain()` depois de
`refresh()`). Sem isso, o plano velho continua sendo o branco da
inicialização, e todo parcial passa a somar o quadro novo ao anterior.

### 3.2 Escrever os dois planos não dispara o refresh

O datasheet (§10, DATA STOP) sugere que escrever `0x10` e `0x13` em sequência
poderia disparar o refresh. Não dispara: `uc8253_limpa`, a faxina do boot,
faz exatamente isso e só então chama o DRF.

### 3.3 A escrita também fica dentro de PTIN/PTOUT

Inclusive no refresh completo. A janela diz ao controlador **onde no SRAM**
os bytes caem; não é otimização.

### 3.4 A temperatura forçada é desfeita ao fim de todo ciclo

PSR `0x1E` (soft reset) seguido de `0x1F`. Sem isso o ciclo seguinte roda com
a waveform errada. Como o soft reset devolve o TRES ao default, o driver
remanda a configuração base depois de cada um.

---

## 4. Os três modos

| Modo | Waveform | Custo | Quando |
|---|---|---|---|
| **parcial** | TSSET `0x6E`, VCOM `0xD7` | ~350 ms | tudo que acontece dentro de uma tela |
| **completo** | TSSET `0x5A`, VCOM `0x97` | ~2,5 s | primeiro quadro depois do boot ou de acordar |
| **faxina** | completo, preto e branco alternados | ~6 s | boot |

Não existe quarto modo. LUT própria, temperatura mentida e sessões parciais
múltiplas foram tentadas e não sobreviveram ao vidro (§8).

---

## 5. A política do sistema: quando cada modo

Isto é decisão de experiência, não de driver, e mora em dois arquivos que
compilam e têm teste no PC:

| Arquivo | Responde |
|---|---|
| `tela/motor.c` | **quantos** quadros um gesto vale, e o que cada um leva |
| `hal/refresco.c` | **como** cada quadro é pintado |

O `app.c` levanta os fatos, pede o plano e executa. `tela/mapa.c` guarda as
propriedades de cada tela (hoje, se ela tem seletor).

### 5.1 As regras

| Situação | Modo |
|---|---|
| primeiro quadro depois do boot | completo |
| primeiro quadro depois de acordar | completo — dormir apaga o SRAM do controlador |
| trocou de tela | flash uniforme + parcial (§5.7) |
| cursor, rolagem, teclado, relógio, medidor | parcial |
| abrir/fechar pop-over ou gaveta | parcial (§5.6) |
| mudou mais de um terço da tela | completo (§5.3) |
| quadro idêntico ao que está no vidro | **nenhum** |

"Trocou de tela" não se deduz do bitmap — rolar uma lista muda o quadro
inteiro e é a mesma tela. Só o app sabe: `assinatura_da_tela()` em
`app/app.c`, que não conta o overlay.

### 5.2 O pop-over

Pop-over é **manter o quadro anterior, menos na região da caixa**. A vista
redesenha a tela de trás inteira e a caixa por cima. Hoje a janela do `0x90`
é a tela inteira; recortá-la no retângulo da caixa é uma otimização possível,
ainda não medida.

### 5.3 O ghosting

**1. Parcial só quando pouco muda.** Acima de um terço da tela, o quadro é
completo: um parcial grande custa quase o mesmo e entrega um quadro pior.

**2. A waveform longa só roda no boot.** Orçamentos automáticos (um completo
a cada N parciais) e um gesto manual de "limpar sombra" foram testados: os
primeiros produziram piscadas no meio da navegação sem resolver o rastro do
cursor, e o segundo deixava a tela pior em vez de melhor (§8).

### 5.4 Dormir

O vidro segura a imagem sem energia, mas o SRAM do controlador não sobrevive,
e o deep sleep do painel só sai por reset de hardware. **Depois de dormir, o
plano velho é desconhecido: o primeiro quadro ao acordar é completo.**

### 5.5 A tinta do completo não sai num parcial

> **O que a waveform completa assenta, o parcial não apaga.**

O refresh completo **também é diferencial**. Nos dois modos, o par (velho,
novo) escolhe a LUT de cada pixel:

| Par | LUT | O que faz |
|---|---|---|
| velho ≠ novo | KW / WK — transição | empurra a partícula |
| velho == novo | KK / WW — retenção | segura onde está |

O que separa os modos é a **duração das LUTs de transição**: tinta empurrada
pelo curso longo não volta com um empurrão curto.

**Corolário:** o plano velho tem de estar **sempre certo**, nunca
"neutralizado". Um pixel cujo velho mente cai na LUT errada.

**E o sentido inverso:** nada que um parcial vá desfazer pode ser assentado
pela waveform completa. Por isso **trocar para uma tela com seletor são dois
quadros**:

| # | Quadro | Modo |
|---|---|---|
| 1 | a página **sem o seletor** | completo / flash |
| 2 | a página com o seletor | parcial |

Numa tela sem cursor os dois quadros são idênticos e o hal descarta o
segundo. O simulador guarda o quadro assentado (`pc_tela_assentada`), e um
teste confere, para uma lista nomeada de telas, que o seletor não foi junto
com a página.

### 5.6 A caixa precisa de chão limpo

Um pop-over é uma caixa branca sobre texto que o completo assentou. Um
parcial direto sairia com a caixa **transparente**. Abrir uma caixa são dois
quadros parciais:

| # | Quadro |
|---|---|
| 1 | a tela de trás com a região da caixa em branco |
| 2 | a tela com a caixa desenhada |

Cada pop-over expõe a própria geometria (`tela_menu_popover_area`,
`tela_gravador_popover_area`): quem desenha e quem apaga leem o mesmo
retângulo. Fechar não precisa disso: a caixa é tinta leve.

### 5.7 O flash uniforme: como a tela troca de página

A waveform nativa do completo passa por várias fases de inversão, custa
~2,5 s e duas piscadas — parece travado. A troca de tela é: **a tela vai a
preto de uma vez e volta com a página nova**, num parcial. Uma piscada,
~700 ms, como num Kindle.

O segundo quadro parte de um fundo **uniforme**, e parcial com alvo
previsível é o caso em que ele funciona melhor. A capa do xadrez, com massas
pretas grandes, parte de branco.

O preço: sem as fases de inversão a tinta firma menos e o ghosting acumula
ao longo do uso; o boot seguinte o limpa (§5.3).

### 5.8 Como se reconhece um quadro empilhado

Numa transição correta, **durante a piscada** o vidro já mostra a imagem
nova. Se a piscada acontece e o conteúdo continua o anterior, o plano novo
não era o quadro novo naquele momento. As causas possíveis:

1. o `0x13` do ciclo não chegou;
2. o quadro pedido era igual ao do vidro;
3. dois quadros foram pedidos em sequência (§5.5) e o que pisca é o
   primeiro.

O terceiro é o mais comum. A primeira pergunta é **quantos quadros o app
pediu para aquele gesto**.

---

## 6. Onde o driver diverge da referência

| Ponto | Estado |
|---|---|
| plano velho reescrito depois do refresh | igual à referência |
| escrita dentro de PTIN/PTL/PTOUT | igual à referência |
| temperatura forçada desfeita ao fim do ciclo | igual à referência |
| TRES | o driver manda (a referência confia no default) e o remanda depois de cada soft reset |
| janela do pop-over | tela inteira; recorte não medido (§5.2) |

`writeImageForFullRefresh()` da GxEPD2, que escreve o quadro novo nos dois
planos, **não** é o caminho normal: é um completo não-diferencial. Aplicado
aqui, deixou a tela manchada, porque com velho == novo todo pixel cai em
retenção (§5.5).

---

## 7. A bancada

Quando o datasheet e a referência não respondem, o vidro responde. O
instrumento é `firmware/main/bancada/eink.c`, ligado por `BANCADA_EINK` em
`bancada/eink.h`. Foi assim que se decidiram orientação e polaridade.

1. A pergunta é **binária ou uma varredura pequena**.
2. A bancada **imprime na serial o que mandou**; olha-se o vidro e a serial
   lado a lado.
3. **Uma variável por gravação.**
4. A resposta vira comentário no código, na linha do número, e uma linha
   neste documento.

---

## 8. O que já foi tentado e não funciona

Cada linha custou uma gravação. Sem razão nova, não repetir.

| Tentativa | Resultado |
|---|---|
| tirar o POF do fim do ciclo | criou atraso; o POF é obrigatório |
| plano velho branco no completo | não resolveu |
| plano velho igual ao negativo do quadro | tela cinza e manchada |
| LUT própria nos registradores `0x20..0x25` | o vidro não mudou |
| mentir a temperatura para forçar waveform | o vidro não mudou |
| SPI de 10 para 2 MHz | nenhuma diferença — o meio físico está descartado |
| assentamento automático 600 ms após a interação | piscada periódica é pior que o rastro |
| full rápido (`0x5A`) na troca de tela | roubou contraste e deixou o quadro anterior sobreposto |
| plano velho == plano novo no completo | tela manchada; tudo cai em retenção (§5.5) |
| firmar com a página no plano novo | o seletor, parcial, não inverte sobre tinta firme |
| firmar com preto no plano novo | tela preta; o parcial não apaga |
| faxina do boot na troca de tela | quase 4 s no gesto mais frequente |
| amostrar o BUSY de 2 em 2 ms | quadro só aparecia na segunda tentativa |
| um completo a cada 12 parciais | o desbotamento não sumiu |
| gesto manual "limpar sombra" (waveform longa a pedido) | resultado instável; mais atrapalhava do que limpava |

### 8.1 Hipóteses descartadas sem gravação

| Hipótese | O que a derrubou |
|---|---|
| escrever os dois planos dispara o ciclo (§3.2) | a faxina do boot faz isso e funciona |
| a espera do BUSY perde a borda de descida | a serial mostra tempos coerentes nas quatro esperas do ciclo |
| o cursor das listas carimba por ser faixa cheia | `chrome_cursor()` é contorno vazado |

### 8.2 O refresh de faixa

Quando as linhas que mudaram cabem em um quarto da tela, o hal atualiza **só
essa faixa** (`uc8253_atualiza_faixa`): é o que torna barato o relógio da
barra e a legenda do rodapé.

- A faixa é de **linhas**, nunca de colunas: a janela alinha o eixo
  horizontal em blocos de 8 px.
- **Só as linhas da faixa atravessam o SPI.** O controlador escreve os bytes
  na janela; mandar o quadro inteiro faria a faixa aparecer com o começo do
  buffer.
- Depende do PSR `0x1F`: com a imagem girada, a linha `y` do quadro seria
  outra linha no vidro.

A trilha de comandos está travada em `firmware/testes/t_eink.c`.

---

## 9. Regras para quem escreve tela

1. **Toda tela sai por um único `hal->mostrar`.**
2. **Desenhe o quadro inteiro, sempre.** Quem decide o que se move é a
   comparação entre os planos.
3. **Diga se trocou de tela.** É a única informação que não está no bitmap.
4. **Nada de animação rápida, nada de relógio com segundos.** Um quadro por
   segundo é o teto.
5. **Não peça um quadro idêntico ao anterior.**
6. **Se a tela parecer sobreposta, o defeito é do plano velho** — não
   conserte no desenho.
