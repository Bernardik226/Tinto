# Regras

As regras de comportamento do Tinto, numeradas. Cada uma está certa ou
errada, sem interpretação, e os testes citam o número (`RN-xx`) no título.
O detalhe de cada regra está no documento indicado.

---

## 1 · Voz

| # | Regra |
|---|---|
| **RN-11** | Gravar **nunca** exige escolha: sem tipo, sem destino, sem data |
| **RN-12** | Pausar não decide nada. **Só o OK finaliza** |
| **RN-13** | Os trechos de uma sessão vão para o **mesmo arquivo** |
| **RN-14** | Com gravação aberta, **o direcional trava** e o aparelho não dorme; depois do OK, continua travado até a resposta ou o prazo estourar |
| **RN-15** | **Sem rede o botão de voz não grava**, e a tela diz por quê. O que a IA não entendeu como comando vira anotação |
| **RN-16** | A IA **propõe**; nada vai para o Google sem o OK da pessoa |
| **RN-17** | Toda ação de voz deixa um item com a transcrição que a gerou |
| **RN-18** | Uma fala pode gerar **várias ações**, e Conferir mostra todas |
| **RN-19** | Conferir mostra **tipo · item · destino · frase crua**, com o não-destrutivo selecionado |
| **RN-1B** | Recusar a proposta **apaga a gravação junto**, e a tela diz isso antes |
| **RN-1C** | Enquanto não confirmada, a proposta **não toca no cartão** |

## 2 · Itens e tipos ([SISTEMA §0 e §2](SISTEMA.md))

| # | Regra |
|---|---|
| **RN-21** | **Falou hora → evento. Não falou → tarefa.** O `due` do Tasks descarta a hora |
| **RN-22** | Tarefa que **ganha hora vira evento** (`uso_mudar_data`) |
| **RN-23** | **O tipo é da IA e não se edita.** Veio errado: descarta e fala de novo |
| **RN-24** | O que a pessoa alterou à mão, a IA não sobrescreve |
| **RN-25** | **Anotação nunca vai para o Google**, e a tela diz isso |
| **RN-26** | A nota **pertence ao dia em que foi falada**, para sempre |
| **RN-28** | A transcrição crua fica visível abaixo do resumo |
| **RN-29** | Texto de conteúdo não se edita à mão: corrige-se apagando e falando de novo |
| **RN-2A** | *Substituída pela RN-4G.* Editar alcança qualquer agenda escolhida |
| **RN-2B** | **Evento não tem "feito".** Marcar evento é estado só do aparelho |
| **RN-2C** | Evento de **dia inteiro** vem primeiro e nunca "já passou" até a virada |

## 3 · O dia e a navegação ([UI.md](UI.md))

| # | Regra |
|---|---|
| **RN-31** | **O cartaz nunca fica vazio.** Sem compromisso, mostra data e contagem |
| **RN-32** | **Um destaque por tela** |
| **RN-33** | Só a zona de trabalho cede: o que não coube vira `+ N mais` |
| **RN-34** | Tarefa aberta aparece em **hoje** até ser feita, venha do dia que vier |
| **RN-35** | Feito fica **riscado onde está** e só some ao virar o dia |
| **RN-36** | Dia passado é só histórico: nenhuma tarefa aberta aparece nele |
| **RN-36A** | O calendário é sobre **datas**: entra evento, e tarefa por prazo ou conclusão |
| **RN-36B** | A tarefa concluída mora no dia em que foi **concluída** |
| **RN-37** | **OK** faz a ação da linha; **▶** entra no item |
| **RN-38** | **BACK segurado** volta para a home de qualquer profundidade, exceto gravando |
| **RN-39** | No máximo 3 níveis de profundidade |
| **RN-3A** | Mudança do Google **de hoje** aparece na agenda; de outro dia, marca no calendário |
| **RN-3B** | O marcador de novidade se apaga quando o cursor passa |
| **RN-3C** | Na dock, o aparelho deita (416×240) e trava *(hardware da v2)* |
| **RN-3F** | **Um botão, uma função.** BACK volta, MENU abre a gaveta, ◀ nunca volta |
| **RN-3G** | Só **BACK · OK · ◀ · ▶** aparecem no rodapé |
| **RN-3H** | No repouso, só o **power** desbloqueia; o botão de voz pede para desbloquear |
| **RN-3I** | Sem tocar em nada por N minutos (Ajustes), a tela **trava sozinha** — nunca com uma fala aberta, nunca no primeiro uso |

## 4 · Sincronização ([SISTEMA §3](SISTEMA.md))

| # | Regra |
|---|---|
| **RN-41** | **Nenhuma tela congela esperando rede**; a espera mostra os três pontinhos |
| **RN-42** | Marcar e desmarcar várias vezes termina **num estado só** |
| **RN-43** | Apagar some do cartão **e** do Google |
| **RN-44** | Só se confirma o que o servidor aceitou |
| **RN-45** | O gesto carrega **o horário do gesto**, não o do envio |
| **RN-48** | O `pull` é a única via de entrada de dado; `/v1/olhar` é consulta e não grava |
| **RN-49** | O `pull` é paginado, e só avança depois de aplicar o lote inteiro |
| **RN-4A** | **O id da operação nasce no device**; o servidor responde a repetição com o mesmo resultado |
| **RN-4B** | O que não coube é **contado**, nunca engolido em silêncio |
| **RN-4D** | **Conferir diz o destino**: tipo, produto e lista |
| **RN-4E** | O que a voz **cria** nasce na agenda "Tinto"; tarefa e lista, nas listas da pessoa |
| **RN-4G** | **Mexer alcança qualquer agenda escolhida**: editar e apagar vão para a agenda de onde o evento veio |
| **RN-4H** | A rotina desce como **regra**, nunca expandida |
| **RN-4I** | O dia tirado da série viaja na regra |
| **RN-4J** | **Apagar uma rotina pergunta antes** |
| **RN-4K** | Enquanto o servidor disser `mais`, a tela não repinta |
| **RN-4L** | O item do Google mora no dia que o Google diz |

## 5 · Voz e cota

| # | Regra |
|---|---|
| **RN-51** | A nota não tem teto de duração no aparelho: quem limita é a cota mensal do servidor |
| **RN-52** | O aparelho só **exibe** a cota que recebe do servidor; quem barra é o servidor |

## 6 · O cartão ([SISTEMA §7](SISTEMA.md))

| # | Regra |
|---|---|
| **RN-61** | Fora de `/TINTO/` o firmware não enxerga nada |
| **RN-62** | Sem `meta.json`, o diretório não existe |
| **RN-63** | Versão desconhecida → **ignorado**, nunca lido na marra |
| **RN-64** | **Escrita atômica em todo JSON**: `.tmp` + `rename` |
| **RN-65** | Meta ilegível aparece com marcador de defeito; nunca some calado |
| **RN-66** | Nunca existe item sem texto |
| **RN-67** | O áudio é apagado ao **confirmar** a ação |
| **RN-68** | Cartão sem `/TINTO/` → cria a árvore e segue |
| **RN-69** | *Substituída pela RN-6E* |
| **RN-6A** | O microSD é **memória interna obrigatória** |
| **RN-6B** | FAT válido sem `/TINTO/` é preparado **sem formatar** |
| **RN-6C** | Formatar só depois de tela própria, com **"não, voltar" selecionado** |
| **RN-6D** | O perfil acompanha o cartão; trocar o nome não troca o `proprietario_id` |
| **RN-6E** | **O índice responde, o cartão guarda**: lido uma vez no boot, as telas nunca abrem diretório |
| **RN-6F** | O primeiro uso retoma na primeira pendência; voz e MENU não escapam antes da conclusão |
| **RN-6G** | Hora que ninguém ajustou e nenhum NTP trouxe é um **contador**, não hora: a tela não a mostra |
| **RN-6H** | O mês do calendário são **8 bytes** vindos do servidor; um dia fora da janela é buscado ao abrir |
| **RN-6I** | O cartão guarda a **janela**: ontem, hoje e amanhã |

## 7 · Atualização

A atualização pela rede (OTA) é da v2. Na 1.0 o firmware se grava por cabo,
e a volta automática já vale:

| # | Regra |
|---|---|
| **RN-72** | O firmware novo tem 60 s para se declarar são, ou o bootloader volta ao anterior |

## 9 · Servidor e privacidade

| # | Regra |
|---|---|
| **RN-91** | Toda consulta filtra pela **pessoa**; nunca se recebe um id de pessoa de fora |
| **RN-92** | O que é de outra conta responde **404, nunca 403** |
| **RN-93** | O servidor apaga o áudio assim que transcreve |
| **RN-95** | Log guarda id, tamanho, duração e custo — **nunca conteúdo** |

## A · Exceções e mensagens

| # | Regra |
|---|---|
| **RN-A1** | Texto de exceção = **estado em poucas palavras + a ação**. Nunca código de erro |
| **RN-A2** | Confirmação só para o que destrói, com o "não" selecionado |
| **RN-A3** | Memória ausente impede o uso; falha de comunicação nunca oferece formatar |
| **RN-A5** | Cartão cheio → aviso passivo, nunca ação automática |
| **RN-A6** | Rede sumiu → nenhum aviso na cara; o ícone muda |

## B · Validação ([ENGENHARIA §7](ENGENHARIA.md))

| # | Regra |
|---|---|
| **RN-B6** | Todo campo string tem tamanho declarado no tipo; cópia sempre com limite |
| **RN-B7** | **Truncar, não rejeitar** |
| **RN-B8** | Campo faltando → valor padrão; campo a mais → ignorado |
| **RN-B9** | Campo com tipo errado → o item inteiro é ignorado |
| **RN-BA** | O servidor valida **entrada e saída** com Pydantic |
