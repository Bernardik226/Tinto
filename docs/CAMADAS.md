# Camadas do firmware

Quem pode chamar quem, onde mora o estado, e como o aparelho inteiro roda no
PC. Código que contraria isto está errado mesmo que funcione.

---

## 1. As camadas

```
    entrada                    saída
       │                         ▲
       ▼                         │
  ┌─────────────────────────────────────┐
  │  app/     LAÇO E ROTEAMENTO         │  ← só ele conhece todas
  └─────────────────────────────────────┘
       │  chama                  │  chama
       ▼                         ▼
  ┌──────────────────┐   ┌──────────────────┐
  │  uso/  CASOS DE  │   │  vista/ MONTAGEM │
  │        USO       │   │                  │
  └──────────────────┘   └──────────────────┘
       │                         │
       ▼                         ▼
  ┌──────────────────┐   ┌──────────────────┐
  │  dado/  CARTÃO   │   │  ui/  COMPOSIÇÃO │
  └──────────────────┘   └──────────────────┘
       │                         │
       │                         ▼
       │                 ┌──────────────────┐
       │                 │  tela/  TINTA    │
       │                 └──────────────────┘
       │                         │
       ▼                         ▼
  ┌─────────────────────────────────────┐
  │  hal/    HARDWARE, ATRÁS DE PAPÉIS  │
  └─────────────────────────────────────┘
```

Mais duas pastas sem posição no desenho: `nucleo/` (tipos, datas, erros e o
estado — puro, usado por todos) e `jogos/` (o xadrez, com as regras e o
adversário, puro).

### A regra de dependência

**Cada camada só chama a de baixo, e nunca a de cima.** `tela/` não sabe que
existe cartão. `dado/` não sabe que existe tela. `uso/` não desenha. `app/`
não escreve arquivo nem pinta pixel — ele só decide.

| Camada | O que faz | O que **nunca** faz |
|---|---|---|
| `app/` | Recebe evento, decide o que acontece, guarda o estado | ler arquivo, desenhar, formatar texto |
| `uso/` | Um caso de uso por função; cada um deixa o sistema coerente sozinho | saber qual tela está aberta |
| `vista/` | Transforma dado cru em **linhas prontas**: cotas, selos, `+N mais` | tocar em pixel ou em arquivo |
| `ui/` | Compõe a tela a partir da struct de vista, um arquivo por tela | ler dado, decidir regra |
| `dado/` | Lê e escreve o cartão e mantém o **índice** em RAM. O único que conhece caminho de arquivo e JSON | decidir regra de negócio |
| `tela/` | Primitivas — bitmap, fonte, ícone — e o motor de quadros | consultar qualquer coisa |
| `hal/` | Fala com o hardware **por papel**: entrada, display, cartão, áudio, relógio, rede | existir no binário do PC |

**O índice mora em `dado/`, e não no estado do app.** `cartao_grava_item` e
`cartao_apaga_item` o atualizam sozinhas: nenhum caso de uso precisa lembrar.

**`vista/`, `tela/` e `nucleo/` não incluem nada do ESP-IDF.** É o que os
mantém compiláveis no PC, e `make test` verifica essa fronteira antes de
rodar (alvo `camadas`).

**`tela/` recebe tudo pronto.** Se uma tela precisou perguntar "que dia é
hoje?", quem devia ter respondido é `vista/`.

A organização interna de `ui/` e `tela/` — os quatro níveis e o HUD — está
no [UI.md](UI.md).

---

## 2. Onde mora o estado

**Uma struct só, `estado_t`, em `nucleo/estado.h`.** Sem `static` espalhado.
Resumida:

```c
typedef struct {
    // onde a pessoa está
    data_t   dia_visto;
    tela_id  pilha[...];       // navegação em pilha
    int      profundidade;
    overlay_id overlay;        // sobreposto, não empilha
    int      cursor;

    // o aparelho
    bool     travado;
    data_t   hoje;  int hora, minuto;
    rede_t   rede;

    // o que está acontecendo
    gravacao_t gravacao;       // parado · gravando · pausado · estruturando
    sinc_t     sinc;

    // o que veio do servidor, e o device só exibe
    char     nome[...];        // vazio = não pareado
    quota_t  quota;

    // caches, com bandeira de validade
    item_t   itens[...];
    bool     itens_validos;
} estado_t;
```

1. **Só `app/` escreve nele.** `uso/` recebe ponteiro e mexe; ninguém mais.
2. **Cache tem bandeira de validade, nunca prazo.** Quem muda o cartão
   derruba a bandeira. Cache que expira por tempo mente durante o tempo.
3. **Nada de estado dentro de tela.** Rolagem e item selecionado são do
   estado.

---

## 3. Os casos de uso

Cada caso de uso é **uma função em `uso/`**, e deixa o sistema coerente
sozinho: grava no cartão, enfileira o que precisa subir e invalida o cache.
Nunca metade disso.

| Área | Funções |
|---|---|
| Voz | `uso_gravar_comeca` · `_pausa` · `_retoma` · `uso_salvar_captura` · `uso_enviar_captura` · `uso_descartar_captura` · `uso_propor_resultado` · `uso_confirmar_resultado` · `uso_descartar_resultado` |
| Itens | `uso_marcar` · `uso_renomear` · `uso_trocar_tipo` · `uso_mudar_data` · `uso_apagar_item` · `uso_liberar_audio` · `uso_abrir_item` |
| Agenda | `uso_carregar_dia` · `uso_ir_para_dia` · `uso_pedir_o_dia` · `uso_agendas` · `uso_escolher_agenda` · `uso_poda_a_janela` |
| Nuvem | `uso_parear` · `uso_desvincular` · `uso_sincronizar` · `uso_enviar_gesto` · `uso_nuvem_resposta` · `uso_procurar_atualizacao` |
| Acervo | `uso_carregar_acervo` · `uso_acervo_sincroniza` · `uso_acervo_baixa` · `uso_acervo_descarta_local` |
| Sistema | `uso_configurar_dispositivo` · `uso_salvar_ajuste` · `uso_ajustar_relogio` |

**Critério para um caso de uso novo:** ele só existe se **não** for
combinação dos que já existem *e* deixar o sistema coerente sozinho. Botões
como power, MENU e BACK **não são caso de uso**: são roteamento, e
roteamento mora em `app/`.

`uso_mudar_data` mostra por que a regra fica aqui e não na tela: **tarefa
que ganha hora vira evento**, porque o Google Tasks descarta a hora
([SISTEMA §2](SISTEMA.md)). A conversão acontece dentro do caso de uso.

`uso_configurar_dispositivo` é a única porta do primeiro uso: diagnosticar e
preparar o cartão, gravar perfil e recibos, e avançar a etapa. Cada etapa é
gravada antes de o estado em RAM avançar, então a retomada depois de um
corte deriva da primeira pendência gravada.

---

## 4. O ciclo de vida de um item

São **três dimensões independentes**:

```
CONTEÚDO      só áudio  →  transcrito  →  estruturado
ÁUDIO         tem  ·  liberado            (some por ação manual, nunca sozinho)
ESPELHO       só aqui  ·  no Google  ·  veio do Google  ·  privado
```

**A invariante:** nunca existe item **sem áudio e sem texto**. Se não há
texto, o áudio é a única cópia e a opção de liberá-lo nem aparece.

### Online e offline

O cache no cartão é **visual**: sem rede, o aparelho mostra o que já tem,
e as ações que mudam alguma coisa pedem rede.

| | sem rede | com rede |
|---|---|---|
| Ver o dia, os compromissos e as tarefas | ✅ do cartão | ✅ |
| Ler o acervo, jogar xadrez | ✅ | ✅ |
| Andar entre dias da janela | ✅ | ✅ |
| Falar | ❌ a tela diz por quê | ✅ |
| Marcar feita, renomear, apagar | ❌ | ✅ |
| Receber o que mudou | ❌ | ✅ |

**Nenhuma tela congela esperando rede.** A chamada acontece por trás; a
espera é mostrada com os três pontinhos animados, e o resultado chega como
evento.

### A liberdade sobre o que foi gravado

Trocar o tipo, renomear, apagar o áudio depois que virou texto, apagar tudo
(com confirmação que diz o que some). **O que a pessoa escreveu à mão, a IA
não sobrescreve.**

### Anotações: o que fica só seu

Anotação é o tipo **sem equivalente no Google**, de propósito. Ela é
transcrita e resumida como as outras, mas **nunca sai do seu servidor e do
seu cartão**. Desconectar a conta Google não a afeta.

---

## 5. O harness: o aparelho inteiro rodando no PC

`hal/` é **uma struct de ponteiros de função** (`hal_t`, em `hal/hal.h`),
entregue ao `app/` na partida:

```c
typedef struct {
    bool     (*proximo_evento)(evento_t *out);
    void     (*mostrar)(const uint8_t *bits, int l, int a, pintura_t intencao);
    erro_t   (*ler)(const char *caminho, char *out, size_t max);
    erro_t   (*escrever)(const char *caminho, const char *conteudo);
    erro_t   (*renomear)(const char *de, const char *para);
    bool     (*docado)(void);
    void     (*relogio)(data_t *d, int *hora, int *minuto);
    /* ... áudio, Wi-Fi, nuvem, segredos, bateria ... */
} hal_t;
```

Na placa, `firmware/main/hal/hal_esp.c` preenche isso com ESP-IDF. No PC,
`firmware/simulador/hal_pc.c` preenche com um cartão em memória, uma fila de eventos
roteirizada, uma nuvem falsa e um buffer de tela que vira PNG. **Os dois
builds compilam o mesmo `firmware/main/`.**

Isso permite testar o aparelho de verdade, de ponta a ponta:

```c
void t_falar_pausar_retomar_e_confirmar(void)
{
    liga();
    pc_botao(IN_VOZ);            // começa
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_GRAVANDO);

    segundos(9);
    pc_botao(IN_VOZ);            // solta: pausa, não termina
    app_passo(&ap);
    ESPERA_IGUAL(ap.estado.gravacao.fase, GRAV_PAUSADA);
    ...
}
```

E toda tela vira PNG em qualquer estado, sem gravar a placa (`make provas`),
ou navegável no navegador (`make janela`).

O custo é uma indireção por chamada de hardware — irrelevante num aparelho
que desenha no máximo uma tela por segundo.

---

## 6. Rede

O firmware conhece **o servidor, um token, e nada mais**. Não conhece o
Google, não tem chave de IA e não decide nada sobre conteúdo.

**Toda chamada é assíncrona:** o `app/` pede, segue desenhando, e o
resultado chega como evento. O contrato completo —
rotas, formato, idempotência, pareamento — está no [SISTEMA §3](SISTEMA.md).
