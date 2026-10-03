"""O contrato aparelho ↔ backend, do lado do servidor.

O formato já está gravado na placa. As regras do `SISTEMA.md` §3, que o
parser do firmware impõe:

· nada aninhado além de um nível
· chaves de uma ou duas letras
· **nenhum campo opcional que mude o TIPO do valor**: `null` onde o C
  espera string não dá erro, dá lixo na tela

Pydantic valida a SAÍDA também (RN-BA): protege o parser, que não se
corrige por deploy.
"""

from enum import IntEnum

from pydantic import BaseModel, Field


class Tipo(IntEnum):
    """Espelha `tipo_t` de `firmware/main/nucleo/tipos.h`: os números
    atravessam a rede em `tp`, e trocar a ordem renomeia o que está no
    cartão.
    """

    NADA = 0
    ANOTACAO = 1
    TAREFA = 2
    LISTA = 3
    EVENTO = 4


class Item(BaseModel):
    """Um item do `pull`, cortado ao que o firmware lê: o aparelho não tem RAM
    para guardar o que não vai mostrar.
    """

    id: str = Field(max_length=39)
    t: str = Field(default="", max_length=127)

    # "14:00", ou vazio sem hora: string vazia, nunca `null` (o parser lê
    # string, e `null` viraria a palavra "null" na tela).
    h: str = Field(default="", max_length=5)
    f: str = Field(default="", max_length=5)

    # O que cabe em 240 px e duas linhas de fonte miúda; endereço comprido é
    # TRUNCADO (RN-B7).
    l: str = Field(default="", max_length=95)

    # "O dia todo" não é "não sei a hora": viram JSON diferente no Google
    # (`start.date` × `start.dateTime`). Ele VENCE a hora.
    di: bool = False

    # Uma OCORRÊNCIA de algo que se repete? O Tinto não edita a série: a tela
    # diz que mexer aqui mexe só naquele dia.
    r: bool = False

    # A REGRA da rotina, curta: "s:1:12345|u=20261130|x=0915"; vazia no que
    # acontece uma vez. Ver `google.regra_do_evento`.
    rr: str = Field(default="", max_length=47)

    d: str = Field(default="", max_length=10)  # "2026-08-27"
    p: str = Field(default="", max_length=10)  # prazo; vazio = só a data
    ok: bool = False

    # O NOME da agenda ("Trabalho"), não o id (um e-mail de sessenta
    # caracteres): é o que a pessoa procura no celular. Vazio quando não se
    # sabe, e a tela some com a linha. Tarefa não usa: a lista viaja no `l`.
    a: str = Field(default="", max_length=31)

    # QUANDO foi concluída, "2026-09-02"; vazio = aberta. `d` é o prazo, esta
    # é o feito: a tarefa concluída aparece no dia em que foi FEITA, como no
    # Google.
    c: str = Field(default="", max_length=10)
    tp: Tipo = Tipo.NADA

    # Origem: "g" veio do Google, "n" nasceu no aparelho (o filete à
    # esquerda).
    o: str = Field(default="g", max_length=1)

    # Só listas: bastam para a home dizer "quanto falta ali dentro" sem
    # carregar o dentro.
    n: int = 0
    k: int = 0

    # De que NOTA este item veio; vazio = veio do Google. Sem isto, uma
    # anotação que volta num cartão novo perde o vínculo com a fala.
    nota: str = Field(default="", max_length=15)


class Removido(BaseModel):
    """Um id que saiu, embrulhado em objeto: o parser varre `removidos` com
    `contrato_proximo`, que só enxerga o que começa com `{`. Strings puras
    sumiriam no firmware sem erro.
    """

    id: str = Field(max_length=39)


class RespostaOlhar(BaseModel):
    """O que a pessoa está OLHANDO agora: consulta, e não colheita.

    Fora do `pull` porque ele fica pendurado no long polling, e a consulta é
    de quem está com o dedo no botão. O `dd` é o ECO do dia pedido: a
    resposta só vale para o dia que ela diz responder.
    """

    mcm: str = Field(default="", max_length=7)   # "2026-09"
    mce: int = 0
    mct: int = 0

    # O dia que esta resposta responde, e os itens dele: consulta, não vão
    # para o cartão.
    dd: str = Field(default="", max_length=10)   # "2026-09-27"
    dia: list[Item] = []


class RespostaPull(BaseModel):
    """O delta. `removidos` faz o que foi apagado no celular sair daqui."""

    itens: list[Item] = []
    removidos: list[Removido] = []

    # O cursor do paginado: um aparelho meses na gaveta traz 500 eventos
    # (SISTEMA §3.5).
    cursor: str = ""
    mais: bool = False

    # O FUSO, em minutos de UTC, do Google Agenda: é nele que os eventos foram
    # marcados. Já com horário de verão: a tabela de DST muda por decreto.
    tz_min: int = 0

    quota_usados: int = 0
    quota_limite: int = 0
    quota_dias: int = 0

    # O nome do aparelho; sem troca local esperando subir, o aparelho adota.
    aparelho_nome: str = ""

    # ── recomeço: apague o que veio de fora antes de aplicar este lote ──
    # Sem isto, desligar uma agenda não tirava do cartão o que já desceu. O
    # backend não sabe o que o aparelho tem, só o que ELE esqueceu.
    # **Só na primeira página do lote**: nas seguintes apagaria o que as
    # anteriores acabaram de gravar.
    zerar: bool = False


class Acao(BaseModel):
    """Uma coisa que a LLM entendeu da fala. O verbo (`v`) separa "marca
    dentista quinta" de "muda o dentista pra sexta".
    """

    v: str = Field(default="criou", max_length=8)

    id: str = Field(max_length=39)
    t: str = Field(default="", max_length=127)
    h: str = Field(default="", max_length=5)
    d: str = Field(default="", max_length=10)
    p: str = Field(default="", max_length=10)
    l: str = Field(default="", max_length=95)

    # O fim que a fala disse: fica na nota e entra no gesto quando o OK volta.
    f: str = Field(default="", max_length=5, exclude=True)

    # O TIPO explícito, e não pelo prefixo do id: na captura a coisa está
    # sendo criada agora, e quem decidiu o que ela é foi a LLM.
    tp: Tipo = Tipo.NADA

    # O QUE VAI DENTRO da lista ("arroz", "feijão"). `exclude=True`: a resposta
    # da captura é lida num `char json[1024]`, e as compras estourariam o
    # buffer. Ficam na NOTA e entram quando o OK volta como gesto: a lista
    # nasce no Google com o que foi falado.
    itens: list[str] = Field(default=[], max_length=20, exclude=True)


class RespostaCaptura(BaseModel):
    """O que a IA entendeu.

    `falou` é a transcrição CRUA: sem ela, conferir vira confiar. Vem mesmo
    com zero ações, para a tela dizer "ouvi isto, e não entendi como
    comando".
    """

    falou: str = Field(default="", max_length=636)

    # Três é o teto do aparelho (`RESULTADOS_MAX`): o resto sumiria da vista
    # sem aviso, então o limite é declarado dos dois lados.
    acoes: list[Acao] = Field(default=[], max_length=3)

    # O id da NOTA que gerou estas ações: cada ação confirmada aponta de volta
    # para a fala.
    nota: str = Field(default="", max_length=39)



class Gesto(BaseModel):
    """O corpo do `POST /v1/push`. UM gesto, nunca um lote: `em` guarda o
    instante, para não sobrescrever uma mudança posterior feita no Google.
    """

    v: str = Field(default="criou", max_length=8)
    id: str = Field(max_length=39)
    t: str = Field(default="", max_length=127)
    h: str = Field(default="", max_length=5)
    d: str = Field(default="", max_length=10)
    p: str = Field(default="", max_length=10)
    tp: Tipo = Tipo.NADA
    ok: bool = False

    # Onde o evento ACABA ("das 14 às 16"). O aparelho não manda: o servidor
    # preenche a partir da nota (`app.push`).
    f: str = Field(default="", max_length=5)

    # Momento LOCAL do gesto, não da volta da rede. O minuto basta: é a
    # precisão exibida; conflitos no mesmo minuto não são descartados.
    em: str = Field(default="", max_length=25)

    # De que NOTA este gesto veio; vazio no item que chegou do Google.
    nota: str = Field(default="", max_length=39)
