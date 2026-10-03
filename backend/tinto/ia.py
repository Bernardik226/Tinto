"""Whisper e a LLM. Só elas (o Google mora em `agenda.py`).

**A IA estrutura, não cria.** Ela recebe o que a pessoa falou e organiza:
resume, quebra em itens, extrai data, dá título. E só.

· não busca nada fora (sem web, sem conhecimento geral)
· não acrescenta o que não foi dito
· não opina, não sugere, não pergunta de volta
· não conversa: não existe chat

É restrição dura no prompt, e toda função de IA nova passa por ela.
"""

import json
from contextvars import ContextVar
from datetime import timedelta

import httpx

from . import config
from .contrato import Acao, Tipo

GROQ = "https://api.groq.com/openai/v1/audio/transcriptions"
ANTHROPIC = "https://api.anthropic.com/v1/messages"

# O buraco por onde o teste entra, o mesmo de `agenda.py`.
transporte = None
_tokens_ultima_estrutura: ContextVar[int] = ContextVar("tokens_estrutura", default=0)


def tokens_da_ultima_estrutura() -> int:
    """Uso da chamada corrente; ContextVar impede cruzar duas pessoas."""
    return _tokens_ultima_estrutura.get()


# ── Whisper ──────────────────────────────────────────────────────────
async def transcrever(audio: bytes) -> str:
    """O áudio → texto cru.

    BYTES, e não um caminho: o WAV mora no cartão, e quem monta o multipart é
    o `hal` do firmware. `language=pt` explícito: sem ele, uma fala que começa
    com um nome em inglês vira transcrição inteira em inglês.
    """
    if not audio:
        return ""

    async with httpx.AsyncClient(timeout=30, transport=transporte) as cliente:
        r = await cliente.post(
            GROQ,
            headers={"Authorization": f"Bearer {config.GROQ_API_KEY}"},
            files={"file": ("fala.wav", audio, "audio/wav")},
            data={"model": config.MODELO_STT, "language": "pt",
                  "response_format": "json"},
        )
        r.raise_for_status()
        return (r.json().get("text") or "").strip()


def duracao_s(audio: bytes) -> int:
    """Quantos segundos de fala há neste WAV, pelo cabeçalho (não pelo tamanho:
    a conta erraria calada se a taxa mudar). Zero se não der para ler:
    cobrar por arquivo que não se entendeu é cobrar por palpite.
    """
    import io
    import wave

    try:
        with wave.open(io.BytesIO(audio), "rb") as w:
            taxa = w.getframerate() or 1
            return int(w.getnframes() / taxa)
    except Exception:
        return 0


# ── a LLM ───────────────────────────────────────────────────────────
# O prompt separa o que ela É, o que NÃO faz e o formato: misturados, uma
# instrução se perde quando outra entra. O dia da semana vai por extenso:
# deduzido da data, "sexta" caía no sábado.
SEMANA = ("segunda-feira", "terça-feira", "quarta-feira", "quinta-feira",
          "sexta-feira", "sábado", "domingo")

PROMPT = """Você organiza o que uma pessoa acabou de falar num aparelho \
de mesa. Ela falou uma frase; você diz o que fazer com ela.

VOCÊ NÃO:
- busca nada fora do que foi dito
- acrescenta informação que não está na fala
- opina, sugere ou pergunta de volta
- conversa

REGRA DO TIPO — a pergunta é "isso se FAZ ou isso ACONTECE?":

- é coisa que a pessoa FAZ, e ela conclui → tarefa (2). Com ou sem hora:
  "academia às 7", "tomar o remédio às 22h", "pagar o boleto". Tarefa com
  hora aparece na régua do dia como um compromisso e continua se marcando
  como feita — é o híbrido, e é o que faz hábito e rotina funcionarem.
- é compromisso com gente, lugar ou consulta → evento (4). "reunião com o
  time às 10", "dentista quinta às três", "almoço com a Ana". Ele acontece
  e não se conclui.
- é uma LISTA nomeada, com coisas dentro → lista (3), e o que vai dentro
  vai em `itens`. "lista de compras: arroz e feijão" é lista; "comprar
  ração" é UMA tarefa. Uma coisa só nunca é lista — lista com um item é
  uma tarefa com uma tela a mais em volta.
- não é comando nenhum, é um pensamento → anotação (1)

A HORA não decide mais o tipo, e isso mudou em 04/09: ela decide ONDE a
coisa aparece — com hora, na régua do dia; sem hora, na lista do que há
para fazer. O que decide o tipo é se aquilo se conclui.

PRAZO é `d` sem `h`: "entregar o relatório até sexta" é tarefa com data e
sem hora. A data ali é o limite, não a hora de fazer.

HORA SEM DIA é HOJE — e amanhã, se a hora já passou. "tomar o remédio às
22h" sem dia nenhum não apareceria em dia nenhum: a hora põe a coisa na
régua, e régua sem dia não existe.

Na dúvida entre tarefa e anotação, escolha ANOTAÇÃO: ela não vai pro \
Google e não cria nada na vida da pessoa. Errar para anotação custa uma \
linha a mais numa lista; errar para tarefa põe uma obrigação falsa no \
celular dela.

DATAS: hoje é {hoje}, {semana}. Devolva sempre AAAA-MM-DD. Se a fala \
não tem data, deixe vazio — nunca invente hoje.

Os próximos dias, já contados:
{proximos}

Use ESTA tabela e não conte nos dedos. Com o dia da semana só escrito, \
"sexta às dez" caía no sábado — e ninguém descobre isso até faltar ao \
compromisso. "Semana que vem" é somar sete ao dia da tabela.

VERBOS — só existem quatro, e o aparelho não conhece outro:
`criou`, `editou`, `apagou`, `anotou`. "adicionar", "incluir", "põe na \
lista" são `criou`.

O QUE JÁ EXISTE vem no fim da fala, uma linha por coisa, com o id.

- a fala manda TIRAR, CANCELAR ou DESMARCAR uma delas → `apagou` com o \
`id` EXATO copiado da lista
- a fala MUDA uma delas (outro dia, outra hora, outro nome) → `editou` \
com o `id` EXATO e os campos já com o valor NOVO
- a fala MARCA, AGENDA ou CRIA alguma coisa → é `criou`, mesmo que já \
exista uma com o mesmo nome. "marca dentista quinta" com um dentista na \
sexta são DOIS compromissos, não uma remarcação: quem quer mudar diz \
"muda", "adia", "remarca"
- não achou na lista o que a fala menciona → é coisa nova: `criou`, sem \
`id`. Nunca invente um id, e nunca escolha "o mais parecido": apagar o \
compromisso errado é o pior erro que este aparelho pode cometer
- a fala é vaga entre duas ("apaga a reunião", e há três) → `anotou` \
com a frase, e a pessoa resolve olhando

Responda SÓ com JSON, neste formato:
{{"acoes":[{{"v":"criou","t":"título curto","h":"15:00","d":"2026-08-27",\
"tp":4}}]}}

Na lista (3), e SÓ nela, acrescente o que foi falado dentro:
{{"acoes":[{{"v":"criou","t":"Compras","tp":3,\
"itens":["arroz","feijão"]}}]}}

Apagar e editar apontam para o que existe:
{{"acoes":[{{"v":"apagou","id":"g:4f1ep2b","t":"Dentista","tp":4}}]}}
{{"acoes":[{{"v":"editou","id":"g:4f1ep2b","t":"Dentista",\
"d":"2026-09-12","h":"15:00","tp":4}}]}}

`f` é onde o evento ACABA, e só quando a fala disser: "das 14 às 16" é \
"h":"14:00","f":"16:00"; "por duas horas" às 9 é "f":"11:00". Sem fim \
dito, não ponha `f`.

`h` vazio quando não houver hora. No máximo TRÊS ações: o aparelho mostra \
três, e o que passar disso some da vista da pessoa sem aviso."""


def _lista_do_que_existe(itens) -> str:
    """O que já existe, uma linha por coisa: id, título e quando. É o mínimo
    para a LLM APONTAR ("muda o dentista pra sexta" não cria um segundo).
    """
    linhas = []
    for it in itens:
        quando = it.d or "sem data"
        if it.h:
            quando += f" {it.h}"
        tipo = {
            Tipo.ANOTACAO: "anotação",
            Tipo.TAREFA: "tarefa",
            Tipo.LISTA: "lista",
            Tipo.EVENTO: "evento",
        }.get(it.tp, "item")

        # Título e data podem coincidir; a casa é o desempate que a pessoa usa na
        # fala ("a Academia da lista Rotina").
        casa = ""
        if it.tp in (Tipo.TAREFA, Tipo.LISTA) and it.l:
            casa = f" · lista {it.l}"
        elif it.tp == Tipo.EVENTO and it.a:
            casa = f" · agenda {it.a}"

        linhas.append(f"{it.id} · {tipo} · {it.t} · {quando}{casa}")
    return "\n".join(linhas)


def _pergunta(falou: str, existentes) -> str:
    """A fala, e o que já existe embaixo dela."""
    lista = _lista_do_que_existe(existentes)
    return f"{falou}\n\nO QUE JÁ EXISTE:\n{lista}" if lista else falou


async def estruturar(falou: str, tz_min: int = 0,
                     existentes=None) -> list[Acao]:
    """O texto → as ações propostas, numa chamada só: duas custariam o dobro e
    a segunda poderia discordar da primeira.
    """
    from datetime import datetime, timedelta, timezone

    agora = datetime.now(timezone(timedelta(minutes=tz_min)))
    hoje = agora.strftime("%Y-%m-%d")
    semana = SEMANA[agora.weekday()]
    existentes = existentes or []

    # ── a tabela dos próximos dias ──────────────────────────────────
    # Oito linhas prontas em vez de uma conta: o modelo erra aritmética de
    # calendário, e o erro só aparece no dia em que a pessoa falta.
    linhas = []
    for n in range(0, 8):
        d = agora + timedelta(days=n)
        etiqueta = "hoje" if n == 0 else "amanhã" if n == 1 else ""
        linhas.append(f"{d.strftime('%Y-%m-%d')} = {SEMANA[d.weekday()]}"
                      + (f" ({etiqueta})" if etiqueta else ""))
    proximos = "\n".join(linhas)

    async with httpx.AsyncClient(timeout=30, transport=transporte) as cliente:
        r = await cliente.post(
            ANTHROPIC,
            headers={"x-api-key": config.ANTHROPIC_API_KEY,
                     "anthropic-version": "2023-06-01"},
            json={
                "model": config.MODELO_LLM,
                "max_tokens": 512,
                "system": PROMPT.format(hoje=hoje, semana=semana,
                                        proximos=proximos),
                "messages": [{"role": "user",
                              "content": _pergunta(falou, existentes)}],
            },
        )
        r.raise_for_status()
        bruto = r.json()

    uso = bruto.get("usage") or {}
    _tokens_ultima_estrutura.set(max(0, int(uso.get("input_tokens", 0) or 0)
                                     + int(uso.get("output_tokens", 0) or 0)))
    texto = "".join(p.get("text", "") for p in bruto.get("content", []))

    # Só os ids que a LLM VIU podem voltar: um id inventado vira, no máximo,
    # uma coisa nova, nunca um apagar.
    return _acoes_de(texto, {it.id for it in existentes}, agora)


def _acoes_de(texto: str, conhecidos: set[str] | None = None,
              agora=None) -> list[Acao]:
    """O JSON da LLM → ações válidas.

    Tolerante: JSON dentro de markdown ou com frase antes não derruba a
    captura (a pessoa já falou). Ação que não passa no Pydantic é DESCARTADA,
    não corrigida: inventar o que faltou é o que o prompt proíbe.
    """
    inicio = texto.find("{")
    fim = texto.rfind("}")
    if inicio < 0 or fim <= inicio:
        return []

    try:
        dados = json.loads(texto[inicio:fim + 1])
    except json.JSONDecodeError:
        return []

    saida: list[Acao] = []
    for i, bruta in enumerate(dados.get("acoes", [])[:3]):
        try:
            # O id nasce AQUI, provisório (`n:`): a coisa só vai existir no Google se
            # a pessoa confirmar.
            campos = {k: v for k, v in bruta.items() if k in
                      ("v", "t", "h", "d", "f", "l", "tp", "itens")}

            # O id só vale se a LLM o VIU na lista que foi mandada.
            alvo = str(bruta.get("id", ""))
            aponta = bool(conhecidos) and alvo in conhecidos

            # O verbo é NORMALIZADO, e não recusado: o aparelho conhece três
            # (`verbo_de`), e um `"adicionou"` fazia o Pydantic recusar a ação e a fala
            # se perder calada.
            if campos.get("v") not in ("criou", "editou", "apagou",
                                       "anotou"):
                campos["v"] = "criou"

            # ── apagar e editar SÓ apontando ────────────────────────
            # Um "apagou" sem alvo é DESCARTADO: nada é melhor que apagar o
            # compromisso errado.
            if not aponta and campos.get("v") == "apagou":
                continue
            if not aponta and campos.get("v") == "editou":
                campos["v"] = "criou"

            # "anotou", ou ação sem tipo, é ANOTAÇÃO: sem `tp`, o aparelho a tratava
            # como compromisso.
            if campos.get("v") == "anotou" or (
                    not campos.get("tp") and campos.get("v") == "criou"):
                campos["tp"] = int(Tipo.ANOTACAO)
                campos["v"] = "anotou"

            acao = Acao(id=alvo if aponta else f"n:{i + 1}", **campos)

            # ── HORA SEM DIA é hoje, e amanhã se já passou ──────────
            # Régua sem dia não existe. O modelo esquece metade das vezes, e é CONTA,
            # não interpretação: se faz aqui.
            if acao.h and not acao.d and agora is not None:
                dia = agora.date()
                try:
                    h, m = (int(x) for x in acao.h.split(":"))
                    if (h, m) <= (agora.hour, agora.minute):
                        dia = dia + timedelta(days=1)
                except ValueError:
                    pass
                acao.d = dia.strftime("%Y-%m-%d")

            saida.append(acao)
        except Exception:
            continue
    return saida
