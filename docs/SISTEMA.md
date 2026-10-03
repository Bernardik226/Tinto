# O sistema

O que as coisas são, quem fala com quem, e o formato de cada fronteira: o
aparelho, o servidor, o Google e o aplicativo web.

---

## 0. O modelo de dados

### Duas origens

| Origem | Como entrou | Quem manda nele |
|---|---|---|
| `ORIGEM_AQUI` | a pessoa **falou** | o Tinto propõe, ela confirma |
| `ORIGEM_GOOGLE` | veio do Calendar ou do Tasks | o Google |

**Não existe item criado à mão.** O teclado virtual serve para a senha do
Wi-Fi e para renomear; criar item por teclado seria voltar ao celular.

### A nota, e por que ela não é um item

Uma **nota** é uma sessão de fala:

```
NOTA
 ├── transcrição   o que foi dito, cru          ← a prova
 ├── áudio         .wav                          ← temporário
 └── ações[1..3]   o que a IA entendeu           ← evento · tarefa · lista · anotação
```

Uma fala gera **de uma a três ações** ("marca dentista quinta às três e
lembra de comprar pasta térmica" são duas). Três é o que cabe na tela de
Conferir com a frase crua embaixo.

**O áudio vive entre o botão de voz e o OK de Conferir**, e depois é
apagado. A transcrição fica. Cada ação guarda **de que nota veio**, e é
isso que permite abrir uma tarefa de anteontem e ver o que foi dito.

### Os quatro tipos

| Tipo | Mora em | O Tinto pode |
|---|---|---|
| **Evento** | Google Calendar | criar na agenda "Tinto"; ler e editar nas agendas escolhidas |
| **Tarefa** | Google Tasks | tudo |
| **Lista** | Google Tasks | ler título, total e marcados |
| **Anotação** | o seu servidor | tudo — nunca vai para o Google |

**A rotina não é um quinto tipo.** "Todo dia às sete" é um evento que se
repete, e o aparelho guarda **a regra**, não as ocorrências (§2).

**O tipo é da IA, e não se edita.** Veio errado: descarta e fala de novo. A
régua que ela segue sai do formato do destino (§2): falou hora → evento;
coisa a fazer sem hora → tarefa; enumeração → lista; pensamento → anotação.
Na dúvida entre tarefa e anotação, anotação.

### Tudo que muda dado precisa de rede

Não existe fila. Falar, marcar feita, renomear, mudar a data e apagar exigem
rede, e **a recusa acontece antes de tocar no cartão**: o gesto nunca fica
pela metade. A mensagem diz o que não deu ("conecte para marcar feita"), não
só que a rede caiu.

### As regras do modelo

1. Nada nasce no aparelho sem voz.
2. Nenhum áudio sobrevive à confirmação.
3. Nenhum gesto que muda dado acontece sem rede.
4. Nenhuma ação existe sem a nota que a gerou — exceto o que veio do Google.
5. Nenhuma anotação vai para o Google.
6. O tipo não se edita.

---

## 1. As peças

```
   ┌──────────┐        ┌──────────┐        ┌──────────┐
   │  TINTO   │◄──────►│ SERVIDOR │◄──────►│  GOOGLE  │
   │ (ESP32)  │  HTTPS │ (FastAPI)│        │ Cal/Tasks│
   └──────────┘        └────┬─────┘        └────▲─────┘
        │                   │                   │
   ┌────▼─────┐        ┌────▼──────┐       ┌────┴─────┐
   │ microSD  │        │ Whisper   │       │ CELULAR  │
   └──────────┘        │ LLM       │       │ app normal│
                       │ app web   │       └──────────┘
                       └───────────┘
```

**O celular não é peça do sistema.** Ele fala com o Google pelo aplicativo
normal, sem saber que o Tinto existe. É isso que fecha o ciclo sem escrever
aplicativo de celular.

| Dado | Dono | Cópias |
|---|---|---|
| Evento, tarefa, lista | Google | servidor + cartão |
| Anotação | servidor | cartão |
| Áudio | cartão | servidor, só até transcrever |
| Transcrição e resumo | servidor | cartão |
| Acervo (livros, textos) | servidor | cartão, se baixado |
| Ajustes e perfil local | cartão | — |

---

## 2. O contrato com o Google

### Restrições da API que decidem o desenho

| Restrição | Consequência |
|---|---|
| O *device flow* (código na TV) não aceita Calendar nem Tasks | o ESP32 não fala com o Google; o servidor fala |
| App OAuth em *Testing* tem refresh token que expira em 7 dias | o projeto OAuth precisa estar *In Production*, mesmo sem verificação |
| Escopo sensível sem verificação: aviso na tela e teto de 100 contas | ótimo para uso pessoal |
| **O Tasks não guarda hora** — o `due` descarta | falou hora → evento; não falou → tarefa |

### O mapa de tipos

| Tinto | Recurso Google | O que atravessa | O que **não** atravessa |
|---|---|---|---|
| Evento com hora | `event` com `start.dateTime` | título · dia · início · fim · local | — |
| Evento de dia inteiro | `event` com `start.date` | título · dia | — |
| Tarefa com data | `task` com `due` | título · dia · feito | **a hora** |
| Lista | `taskList` + `task` | título · itens · marcados | ordem manual |

Se "me lembra de pagar o boleto amanhã às três" virasse tarefa, as três
sumiriam. Por isso ela nasce evento, e **o recibo diz onde foi parar**.

### Detalhes de formato que erram sozinhos

| O quê | A armadilha |
|---|---|
| `end.date` de dia inteiro | é **exclusivo**: o dia 14 vai como `14` → `15` |
| `due` de tarefa | é meia-noite UTC; com fuso local, desloca o dia |
| `status` | tarefa tem `completed`; **evento não tem "feito"** |

### Escopos

| Escopo | Para quê |
|---|---|
| `calendar` | ler as agendas escolhidas e editar, renomear e apagar eventos nelas |
| `tasks` | tarefas e listas |

**O que a voz cria nasce numa agenda chamada "Tinto"**, criada pelo próprio
servidor. No celular ela aparece junto das outras, com cor própria, e pode
ser desligada num toque: o que o aparelho criou não se mistura com o que a
pessoa criou. Mexer (renomear, mudar data, apagar) vai para a agenda de onde
o evento veio. Tarefas e listas nascem nas listas da própria pessoa.

### Idempotência

| Produto | Aceita id do cliente? | Como não duplicar |
|---|---|---|
| Calendar | sim | o id da operação vindo do device vira o id do evento |
| Tasks | não | o servidor guarda `operação → task_id` |

### Curadoria

| Recurso | O que o servidor faz |
|---|---|
| `calendarList` | escolhida em Ajustes → Agendas; padrão: só a principal |
| `responseStatus` | `declined` some; `needsAction` ganha marca |
| `reminders` | ignorado — o aparelho não tem som |
| fuso | o servidor entrega em hora local; o device nunca converte |
| agenda principal | chamada "Agenda principal", nunca pelo e-mail |

### A janela: ontem, hoje e amanhã

O cartão guarda **três dias**. Dias fora da janela são buscados quando a
pessoa os abre; o mês vem como um bit por dia (8 bytes), para as marcas do
calendário. Na virada do dia, o que saiu da janela e veio do Google é
apagado. Anotações não saem por idade.

### O índice em RAM

Abrir diretório num SD custa dezenas de milissegundos, e as telas perguntam o
tempo todo. O cartão é lido **uma vez, no boot**, para um array na PSRAM, e
as telas filtram esse array. Quem grava (`cartao_grava_item`) mantém o
índice. Ele não é persistido: reconstruir no boot é barato e nunca discorda
do cartão.

### A rotina: uma regra, não noventa ocorrências

O servidor lê com `singleEvents=false`: vem o evento mestre com o
`recurrence`, e o aparelho expande na hora de desenhar. O campo `rr` leva a
regra, curta:

```
d:1                todo dia
s:1:12345          toda semana, seg a sex (0=dom … 6=sáb)
m:1                todo mês, no mesmo dia
a:1                todo ano
|u=20261130        até essa data, inclusive
|x=0915,0922       menos esses dias (MMDD)
```

- **A âncora anda:** `d` é a primeira ocorrência que ainda vale, não o
  começo da série.
- **"Dez vezes" vira uma data**, calculada pelo servidor.
- **Os buracos viajam na regra:** ocorrências apagadas ou movidas chegam do
  Google como eventos à parte; o servidor as acumula em `|x=`.

`nucleo/rotina.h` responde "este dia é da rotina?", e é puro — testado no PC.
**Apagar uma rotina pergunta antes**, porque apaga a série inteira no Google.

---

## 3. O contrato device ↔ servidor

O formato é do Tinto, não do Google: nada aninhado além de um nível, chaves
de uma ou duas letras, **nenhum campo que mude de tipo**.

```jsonc
// evento
{ "id":"g:a1b2c3", "t":"Dentista", "h":"14:00", "f":"15:00",
  "l":"Rua Bahia, 210", "o":"g" }

// evento de dia inteiro
{ "id":"g:f6g7h8", "t":"Entrega do documento", "di":true, "o":"g" }

// tarefa: d = data agendada, p = prazo, c = conclusão
{ "id":"t:MTk4Nz", "t":"Comprar fita kapton", "d":"2026-08-05",
  "p":"2026-08-08", "ok":false, "o":"g" }

// lista
{ "id":"l:MDk4Nz", "t":"Compras da semana", "n":7, "k":2, "o":"g" }
```

`o` é a origem (`g` = Google, `n` = nasceu aqui). `di` vence a hora. `l`
(local) tem teto de 48 bytes e é truncado.

### Rotas do aparelho

| Rota | O quê |
|---|---|
| `POST /v1/registrar` | primeiro contato; devolve o token do aparelho |
| `POST /v1/parear/iniciar` · `GET /v1/parear/estado` | pareamento com a conta Google |
| `POST /v1/desparear` | desfaz o vínculo |
| `GET /v1/pull` | o que mudou, paginado; pode esperar mudança |
| `POST /v1/push` | um gesto: marcar, renomear, mudar data, apagar |
| `POST /v1/captura` | o áudio de uma fala → o que a IA entendeu |
| `GET /v1/olhar` | o mês da grade e um dia fora da janela |
| `GET/POST /v1/agendas` | as agendas visíveis |
| `GET /v1/anotacoes` | as anotações |
| `GET /v1/acervo…` | catálogo, conteúdo e capa do acervo (§8) |

Todas autenticadas pelo token do aparelho, exceto `registrar`.

### Pareamento

1. No Tinto, a tela de conta mostra um **QR** e um código.
2. No celular, a pessoa abre o endereço, **entra com o Google ali** e confirma
   o código.
3. O servidor amarra a conta ao aparelho.

O consentimento acontece no navegador, que é onde ele pode acontecer. **O
aparelho só guarda um token do seu servidor**, nunca uma credencial Google.

### Quem pode usar o servidor

O aparelho **se registra sozinho** assim que tem Wi-Fi: manda o número dele
(o MAC) e uma prova secreta criada no primeiro boot, e recebe um token.
Registrar não dá acesso a nada: agenda, voz e acervo exigem uma conta
vinculada.

O único portão é **`TINTO_CONTAS`**, a lista de e-mails Google que podem
entrar, com o limite de voz de cada um:

```
TINTO_CONTAS=voce@gmail.com:120,mae@gmail.com:30,pai@gmail.com
```

- Um e-mail fora da lista é recusado no login com o Google, antes de qualquer
  credencial ser guardada. **Lista vazia = ninguém entra.**
- Tirar um e-mail da lista corta o acesso na hora: os aparelhos dele passam
  a receber "sem conta" e a sessão no aplicativo deixa de valer. O vínculo
  fica guardado, e voltar à lista o devolve.
- Cada pessoa vincula quantos Tintos quiser; eles dividem a mesma cota.
- Registro que nunca ganhou conta e não aparece há 7 dias é apagado, e o
  servidor guarda no máximo 32 aparelhos sem conta — acima disso recusa
  registros novos (429) e avisa no log.
- A primeira prova apresentada fica. Quem souber o MAC de um Tinto que
  nunca se registrou pode registrá-lo antes; esse registro, sem conta,
  expira em 7 dias, e o aparelho verdadeiro entra em seguida.
- Quem hospeda acompanha a porta pelo **log do servidor**: registros novos,
  logins recusados, logins, vínculos e desvínculos. Token e prova nunca vão
  para o log.

### O nome do aparelho

É um campo só, o mesmo no Tinto e no aplicativo ("Tinto da família"), e muda
dos dois lados:

- trocado **no Tinto**, ele fica marcado para subir e vai de carona no
  próximo `pull` (`&nome=`); o servidor adota, e a marca sai quando a
  resposta confirma;
- trocado **no aplicativo**, o Tinto recebe no `pull` seguinte
  (`aparelho_nome`) e grava no cartão — desde que não haja troca local
  esperando subir.

Assim, formatar o cartão e refazer o primeiro uso não traz de volta o nome
antigo guardado no servidor. Aspas e barra invertida são removidas dos dois
lados, porque o nome é gravado num JSON sem escape; o teto é de 24
caracteres.

---

## 3.5 · O que impede de travar

### O pull é paginado, sempre

```jsonc
GET /v1/pull?n=20
→ { "itens":[...], "removidos":[...], "cursor":"...", "mais":true }
```

- Teto de itens **e de bytes** por resposta.
- O aparelho só avança depois de aplicar o lote inteiro: travar no meio
  repete o lote, nunca pula.
- **`mais` segura o desenho**: enquanto houver mais, o aparelho grava e não
  repinta, para a agenda não encher de uma em uma.
- Com `esperar`, o servidor segura a conexão por até ~25 s quando não há
  nada novo, e responde assim que algo muda.

### Toda escrita é idempotente

O pior bug possível: a resposta de uma captura se perde, o aparelho tenta de
novo, e nascem dois eventos. Por isso **o id da operação nasce no device**, e
o servidor responde a repetição com o mesmo resultado, sem reexecutar.

### A captura é uma pergunta, e a resposta vem na chamada

```
POST /v1/captura  {wav}  →  {falou, acoes[], nota}
```

Enquanto transcreve e classifica, o aparelho fica na faixa de voz dizendo
"Estruturando", travado — a única espera bloqueante fora do repouso. Há um
prazo; passado ele, a faixa destrava e diz onde a fala está.

**Zero ações não é resposta vazia:** se a IA não achou comando, o servidor
devolve uma anotação com a transcrição. Nada do que foi falado se perde.

### Sem reenvio automático

Gesto que falhou falhou, e a tela diz qual. Só o pull, que acontece sozinho,
tem backoff: dez aparelhos batendo de segundo em segundo num servidor caído
transformam uma queda de um minuto numa de uma hora.

### TLS

O aparelho usa o bundle de CAs do ESP-IDF: pinar o certificado folha quebraria
todos os aparelhos no dia em que ele renovasse.

### A versão da rota é um contrato

O servidor nunca quebra `/v1/`; mudança incompatível vira `/v2/`. Do lado do
cartão, o `"v"` do `meta.json` faz o mesmo: formato desconhecido é ignorado.

---

## 4. A IA

| Etapa | O que faz |
|---|---|
| 1. Gravar | WAV no cartão (16 kHz, mono) |
| 2. Transcrever | Whisper (`whisper-large-v3-turbo`, via Groq) |
| 3. Estruturar | LLM (Claude Haiku) → classifica e gera título e conteúdo, numa chamada |

Os modelos são configuráveis por variável de ambiente (`MODELO_STT`,
`MODELO_LLM`).

### A IA estrutura, não cria

Ela recebe o que você falou e organiza: resume, separa em ações, extrai data,
dá título. **Não busca nada fora, não acrescenta o que você não disse, não
opina e não conversa.** É restrição dura no prompt.

**O título é o produto principal**: ele é lido no celular, na lista do
Google, longe do aparelho.

### A IA propõe, a pessoa confirma

Nada vai para o Google sem o OK na tela de **Conferir**, que mostra cada ação
(tipo · título · destino) com a frase crua embaixo. Para editar algo que já
existe, o servidor manda ao modelo a lista de candidatos com id: o modelo não
inventa id que está numa lista fechada na frente dele.

### Onde roda: no servidor, sempre

1. A chave de API não pode existir no aparelho — um dump da flash a revela.
2. O prompt é versionado no servidor: ajustar a classificação não exige
   regravar firmware.
3. Trocar de modelo não toca no aparelho.

---

## 5. Latência

O aparelho recebe mudanças por **pull**: periódico, ao acordar, e com espera
longa (§3.5) quando está ocioso e conectado. Do lado do servidor:

| Fonte | Como o servidor sabe que mudou |
|---|---|
| Tasks | consulta periódica — o Tasks não tem push |
| Calendar, sem webhook | consulta periódica |
| Calendar, com webhook (`TINTO_WEBHOOK_URL`) | o Google avisa na hora; exige domínio verificado no Search Console |

Uma mudança feita no celular aparece no aparelho em segundos a poucos
minutos. O webhook é opcional: sem ele, a sincronização continua por
consulta.

---

## 6. O aplicativo web

Servido pelo mesmo container, em `/e`. É um PWA: instala na tela do celular.
Cada pessoa vê só os aparelhos dela.

Por ele a pessoa entra com o Google, vincula, renomeia e desvincula os seus
Tintos, acompanha o uso de voz no mês, **envia textos e livros** para o
acervo e lê as suas anotações.

Não há painel de administração: quem hospeda controla quem entra e o limite
de cada um por `TINTO_CONTAS`, e acompanha pelo log.

---

## 7. O cartão

O microSD é **memória interna obrigatória**. O firmware só enxerga o que
está dentro de `/TINTO/`, e **um item só existe se tiver `meta.json`**.

```
/TINTO/
  itens/     notas e itens da agenda, por DATA
  acervo/    livros e documentos, por OBRA
  sistema/   config.json, formato.json, perfil.json

/TINTO/itens/2026-08-03/1422-reuniao-do-time/
    meta.json    título, tipo, data, origem, vínculo com a nota
    audio.wav    só até confirmar
    texto.txt    transcrição crua
    proc.json    saída da IA

/TINTO/acervo/ob%tysYaCTOJgoh/
    meta.json · texto.txt · capa · pos.json
```

### Três camadas contra lixo de sistema de arquivos

1. **Namespace:** fora de `/TINTO/` nada é enxergado.
2. **Sem `meta.json`, o diretório não existe** — `.Trash-1000`,
   `System Volume Information` e `.DS_Store` estão cobertos por construção.
3. **`"v"` no meta:** versão desconhecida é ignorada, não lida torta.

### Escrita atômica

Todo JSON vai em `arquivo.tmp`, fecha, e é movido por cima com `rename()`.
**Nunca existe um `meta.json` pela metade.** Meta ilegível aparece na lista
com marcador de defeito, e nunca some calado.

`pos.json` (posição de leitura) é separado do `meta.json` porque muda a cada
página virada, e reescrever o arquivo canônico centenas de vezes gasta a
célula que dói perder.

### Montagem e preparação

FatFs com nome longo na pilha (`CONFIG_FATFS_LFN_STACK`, `MAX_LFN=64`, UTF-8).

O boot distingue memória ausente, falha de comunicação, sistema de arquivos
que precisa de reparo, somente leitura, cheia, versão futura, estrutura
danificada e pronta. **Ausência ou comunicação instável nunca oferece
formatar.**

FAT válido sem `/TINTO/` é cartão novo: o firmware cria a árvore sem formatar
e sem tocar no que existe fora dela. Formatar é recuperação destrutiva, só
depois de uma confirmação própria com "não, voltar" selecionado. Ao preparar
um cartão, uma prova de escrita cria, grava, relê, renomeia e apaga um
arquivo em `/TINTO/sistema/`.

### Perfil local

`perfil.json` guarda a versão, o nome visível (até 24 caracteres), o
`onboarding_v` e um `proprietario_id` aleatório de 128 bits:

```json
{"v":1,"proprietario_id":"0123456789abcdef0123456789abcdef","nome":"Usuário","nome_sobe":0,"onboarding_v":1,"relogio_v":1}
```

Trocar o nome não troca o identificador: perfil e conteúdo acompanham o
cartão.

---

## 8. O acervo

O **catálogo** é da conta e mora no servidor; a **cópia** é do aparelho e mora
no cartão.

1. **O aparelho decide o que baixa.** O aplicativo web só mostra "No
   dispositivo".
2. **Remover online não apaga a cópia.**
3. Posição de leitura e cópia local são do aparelho.

| Rota | O quê |
|---|---|
| `GET /v1/acervo?cursor=` | o catálogo, em chaves curtas (`id`, `t`, `a`, `tp`, `n`, `c`, `aqui`) |
| `GET /v1/acervo/{id}/conteudo` | o texto cru, com `x-tinto-hash` e `x-tinto-tamanho` |
| `GET /v1/acervo/{id}/capa/mini` | a capa 42×75 em 1 bit com dithering, já preparada pelo servidor |
| `POST /v1/acervo/{id}/presenca` | o aparelho anuncia que baixou ou apagou |

O conteúdo desce em `texto.part` e só vira `texto.txt` depois de conferido o
tamanho: **o leitor só abre `texto.txt`**. Campo desconhecido é ignorado,
nunca erro. Obra de outra conta responde **404**, nunca 403.

O servidor converte PDF, EPUB e texto para texto puro. Estados da conversão:
`recebido` · `convertendo` · `pronto` · `recusado` (o arquivo não serve) ·
`falhou` (erro do servidor). Só obra `pronto` desce.

As respostas reais ficam congeladas em `backend/testes/fixtures/acervo/` e são
usadas pelos testes dos dois lados (`firmware/testes/t_acervo_contrato.c`): mudar o
formato quebra os dois de uma vez.

---

## 9. O primeiro uso

```text
BOOT
  └─ verificar memória
       ├─ falha ──> recuperação
       └─ pronta
            ├─ sem nome ──────────> boas-vindas → nome
            ├─ sem rede ──────────> Wi-Fi          (pode pular)
            ├─ sem conta ─────────> conta Google   (pode pular; pular o Wi-Fi pula esta)
            ├─ sem hora confiável ─> data e hora    (só sem rede; com rede vem do NTP)
            ├─ não concluído ─────> conclusão
            └─ concluído ─────────> HOME
```

A fase visível é sempre **a primeira pendência**, calculada a partir do que
está gravado no cartão e do estado da rede. Cada etapa grava antes de o
estado em RAM avançar; depois de um corte de energia, o aparelho retoma dali.
Wi-Fi e conta são opcionais: o aparelho funciona inteiro sem eles, e podem
ser feitos depois em Ajustes. Depois de concluído, perder a rede não devolve
ninguém ao primeiro uso.

### Desconectar e apagar

| Ação | Apaga | Mantém |
|---|---|---|
| **Desconectar** | o vínculo com a conta, dos dois lados | tudo no cartão |
| **Apagar o aparelho** | NVS e `/TINTO/` | nada |

---

## 10. O log de campo

`dado/log.h` grava no cartão um log rotativo de eventos: ligou, gravou,
descartou, marcou, erro, dormiu — com id, tamanho, duração e custo. **Nunca
conteúdo:** o que a pessoa falou não entra em log. Quando o aparelho está
longe da serial, o log do cartão é a história que sobra.
