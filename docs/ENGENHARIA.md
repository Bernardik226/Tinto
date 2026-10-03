# Engenharia

Como o firmware executa num chip com FreeRTOS, como um erro viaja, quem aloca
memória, como se compila e se testa. O [CAMADAS.md](CAMADAS.md) diz quem
chama quem; este diz como isso roda de verdade.

---

## 1. O modelo de execução: várias tasks, uma só manda

```
    ┌────────────────────────────────────────────────┐
    │  task APP                                      │
    │  laço: espera evento → decide → desenha        │
    │  ★ A ÚNICA QUE TOCA O estado_t ★               │
    └────────────────────────────────────────────────┘
          ▲ fila de eventos
          │
    ┌─────┴──────┐  ┌────────────┐  ┌─────────┐  ┌──────────────────┐
    │ ENTRADA    │  │ AUDIO      │  │ TICK    │  │ REDE (duas linhas)│
    │ botões/INT │  │ I2S → WAV  │  │ 1 s     │  │ HTTP, TLS         │
    └────────────┘  └────────────┘  └─────────┘  └──────────────────┘
```

> **Só a task `APP` lê e escreve o `estado_t`. As outras mandam mensagem.**

Áudio, entrada e rede empurram um evento na fila e seguem. O `APP` consome,
decide e altera. Com uma dona só, o estado nunca é observado no meio de uma
transição e não existe deadlock possível. O custo é uma cópia por mensagem.

| Task | Prioridade | Por quê |
|---|---|---|
| AUDIO, ENTRADA | alta | perder amostra de I2S é irrecuperável |
| APP | normal | desenhar 200 ms tarde ninguém percebe |
| REDE | baixa | tudo nela é assíncrono por contrato |

A rede tem **duas linhas independentes**: uma para as ações da pessoa (fala,
gesto) e outra para o pull periódico, para que uma sincronização longa nunca
segure uma confirmação.

**Nenhum evento carrega ponteiro para memória de outra task.** Ou o dado cabe
na struct, ou vai um caminho de arquivo no cartão.

No PC é a mesma coisa, sem FreeRTOS: o `hal_pc` empurra eventos na mesma
fila, de forma síncrona e roteirizada. A arquitetura de execução é a mesma
nos dois lados; só muda quem produz os eventos.

---

## 2. Erros

**Um enum, retornado. Sempre** (`nucleo/tipos.h`).

```c
typedef enum {
    OK = 0,
    ERR_SEM_CARTAO, ERR_ARQUIVO, ERR_FORMATO, ERR_CHEIO,
    ERR_REDE, ERR_TIMEOUT, ERR_NAO_PAREADO, ERR_QUOTA,
    ERR_VERSAO_EXIGIDA, ERR_INTERNO, /* ... */
} erro_t;
```

- Quem produz valor usa out-param; o retorno é sempre o erro.
- Ninguém ignora retorno. `(void)` explícito quando for de propósito.
- A tradução para a tela mora num lugar só, `erro_texto(erro_t)`, em poucas
  palavras. `esp_err_t` **nunca** cruza a fronteira do `hal/` — ele
  arrastaria o ESP-IDF para as camadas que compilam no PC.

---

## 3. Memória

> **Nenhum `malloc` no caminho do dia a dia.**

| O quê | Onde |
|---|---|
| `estado_t`, filas, buffers de trabalho | estático |
| bitmap da tela (240×416 = 12,5 KB), áudio, HTTP | PSRAM, alocados uma vez |

O aparelho roda semanas sem reiniciar, e alocação dinâmica em MCU fragmenta
o heap devagar. Com tudo estático, se coube no link, coube para sempre.

**Todo array tem teto declarado**, e estourar o teto é um caso tratado:
conta, avisa e segue. A divisão entre RAM interna e PSRAM está no
[HARDWARE §10](HARDWARE.md); `make ram` confere o orçamento.

---

## 4. Convenções

| Assunto | Regra |
|---|---|
| Idioma | **português** nos identificadores: `uso_marcar`, `item_t` |
| Estilo | `snake_case`, chaves na mesma linha, 4 espaços |
| Prefixo por camada | `gfx_` · `chrome_` · `bloco_` · `tela_` · `vista_` · `uso_` · `cartao_` · `hal_` — uma violação de camada se lê a olho nu |
| Pinos | só em `pins.h` |
| `static` global | só em `app/` e `hal/` |
| Comentário | explica **por quê**, nunca o quê |

---

## 5. Build e teste

Dois builds do mesmo código. Todos os comandos rodam da raiz.

| Comando | O que faz |
|---|---|
| `make test` | compila o firmware com `gcc` + o `hal` do PC e roda a suíte de `firmware/testes/` |
| `make provas` | renderiza cada tela em PNG, em escala real, em `docs/provas/` |
| `make janela` | o aparelho no navegador, em `localhost:8080`, navegável pelo teclado |
| `make backend` | a suíte do servidor (`pytest`) |
| `make firmware` | build ESP-IDF para a placa |
| `make ram` | o orçamento de RAM interna, com a toolchain do IDF |

- **ESP-IDF 5.3, fixado** no `firmware/CMakeLists.txt` e no CI. Atualizar é
  um commit próprio.
- **Sem framework de teste em C**: assertions próprias (`ESPERA`,
  `ESPERA_IGUAL`, `ESPERA_TEXTO`). O harness já é o framework.
- `make test` roda antes o alvo `camadas`, que falha se uma camada pura
  incluir ESP-IDF.

---

## 6. Ajuste não é flag

| Tipo | Onde mora | Exemplo |
|---|---|---|
| **Compilação** (`#ifdef`) | no código | a bancada do e-ink (`BANCADA_EINK`) |
| **Ajuste** | cartão, em Ajustes | modo do botão de voz, formato de hora |

Se a pessoa escolhe, é ajuste e mora em Ajustes. Ajustes são locais: nenhum
sobe para o Google.

---

## 7. Validar nas fronteiras, uma vez só

*Valide na fronteira, confie depois.*

| # | Fronteira | O risco |
|---|---|---|
| 1 | JSON do servidor → device | string maior que o buffer = estouro em C |
| 2 | `meta.json` do cartão → device | cartão editado à mão ou gravado por versão futura |
| 3 | teclado → device | senha com 200 caracteres |
| 4 | device → servidor | id forjado, duração absurda |
| 5 | aplicativo web → servidor | arquivo enorme, tipo mentido |

Em C:

1. **Todo campo string tem tamanho declarado no tipo.**
2. **Copiar sempre com limite**, e sempre terminando.
3. **Campo faltando → valor padrão.** Faltar é o normal de um sistema que
   evolui.
4. **Campo com tipo errado → o item inteiro é ignorado.** Meia leitura é
   pior que nenhuma.
5. **Truncar, não rejeitar.** Um título de 200 caracteres vindo do Google
   vira 64 e segue; rejeitar faria o dia sumir por causa de um nome comprido.

No servidor, **Pydantic em toda rota, de entrada e de saída**. A de saída
protege o device: um campo que vira `null` por bug falha no servidor, em vez
de chegar como JSON quebrado num parser de ESP32.

---

## 8. CI

Dois workflows em `.github/workflows/`, a cada push:

| Workflow | O que faz |
|---|---|
| `testes.yml` | `make test` (firmware no PC) e `pytest` (servidor) |
| `firmware.yml` | build ESP-IDF com um servidor de exemplo, e guarda o binário como artefato |

A placa não entra no CI: ele garante que compila e que a lógica passa.
A hospedagem do servidor pode esperar o CI verde antes de publicar.

---

## 9. O estado do servidor

Na 1.0 o servidor guarda o estado num **arquivo JSON** (`TINTO_ESTADO`),
escrito de forma atômica, num volume persistente. Por isso roda com **um
worker só**: o estado é um dicionário em memória, e dois processos teriam
duas cópias.

O caminho natural quando isso não bastar é **Postgres**, com as mesmas
entidades de `backend/tinto/modelo.py` e três regras:

1. **Toda tabela de conteúdo tem o id da pessoa**, e toda consulta filtra
   por ele.
2. **Migração versionada desde a primeira tabela.**
3. **O segredo do aparelho é guardado como hash** (`secret_hash`), nunca em
   texto.

### Limites da 1.0

- Um processo, um worker. `Memoria` e `Acervo` serializam as mutações dentro
  do processo; um segundo worker teria outra cópia do estado.
- Envio para o acervo: até **50 MiB** por arquivo e 20 milhões de
  caracteres de texto convertido.
- **O refresh token do Google fica em texto claro no arquivo de estado.**
  Quem tem acesso ao volume tem acesso às agendas vinculadas: restrinja o
  acesso à hospedagem e não imprima o arquivo em log.
- O volume é a única cópia online: configure backup na sua hospedagem.
