"""As entidades do sistema, do lado do servidor (`docs/SISTEMA.md` §0; se
discordarem, o documento vence).

A unidade é a **NOTA**, não o item: uma sessão de fala, com uma
transcrição e de uma a três ações. Item é o que uma ação vira quando é
confirmada.
"""

import json
import secrets
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path

from . import config
from .contrato import Acao, Item, Tipo


@dataclass
class Nota:
    """Uma sessão de fala: nasce no `●` e morre quando a pessoa confirma ou
    descarta. Cada ação confirmada aponta de volta para o `id` daqui.
    """

    id: str
    pessoa: str

    # A transcrição CRUA: é a prova, e é o que fica quando o áudio some.
    transcricao: str = ""

    # De uma a três ações: três é o teto do aparelho (`RESULTADOS_MAX`).
    acoes: list[Acao] = field(default_factory=list)

    criada_em: float = field(default_factory=time.time)

    # Confirmada, descartada ou aberta. Aberta não vira nada no Google
    # (RN-16).
    estado: str = "aberta"

    def confirmar(self) -> None:
        self.estado = "confirmada"

    def descartar(self) -> None:
        self.estado = "descartada"
        # A transcrição some junto: descartar promete que nada daquilo ficou.
        self.transcricao = ""
        self.acoes = []


@dataclass
class Anotacao:
    """A única coisa que é NOSSA: o Google não tem equivalente (o Keep não tem
    API). No backend para sobreviver a um cartão queimado, no cartão para ler
    sem rede. `nota_id` porque nada nasce no aparelho sem voz.
    """

    id: str
    pessoa: str
    titulo: str = ""
    corpo: str = ""
    nota_id: str = ""
    dia: str = ""          # o dia em que foi FALADA, e não muda (RN-26)
    criada_em: float = field(default_factory=time.time)


class Notas:
    """O guarda-notas.

    · **`pessoa` em toda consulta** (RN-91), nunca `WHERE id = :pedido`
    · nota **não confirmada** não gera nada no Google

    As ANOTAÇÕES vão para disco ("apagar o cartão e parear de novo devolve as
    suas anotações"); as notas vivem minutos, entre a fala e o OK.
    """

    def __init__(self, arquivo: str | None = None) -> None:
        self._notas: dict[str, Nota] = {}
        self._anotacoes: dict[str, Anotacao] = {}

        self.arquivo = Path(arquivo) if arquivo else None
        self._carrega()

    # ── notas ────────────────────────────────────────────────────────
    def abre(self, pessoa: str, transcricao: str,
             acoes: list[Acao]) -> Nota:
        n = Nota(id=f"nt:{secrets.token_hex(6)}", pessoa=pessoa,
                 transcricao=transcricao, acoes=list(acoes)[:3])
        self._notas[n.id] = n
        return n

    def de(self, pessoa: str, nota_id: str) -> Nota | None:
        """A nota, se for DESTA pessoa: `None` para o que é de outro faz a rota
        responder 404 e não vazar que existe (RN-92).
        """
        n = self._notas.get(nota_id)
        return n if n and n.pessoa == pessoa else None

    # ── anotações ────────────────────────────────────────────────────
    def anota(self, pessoa: str, acao: Acao, nota_id: str,
              dia: str) -> Anotacao:
        """A ação vira anotação, e o CORPO vem da nota: o gesto não carrega texto
        longo.
        """
        origem = self._notas.get(nota_id)
        corpo = acao.l or (origem.transcricao if origem else "")

        a = Anotacao(id=f"an:{secrets.token_hex(6)}", pessoa=pessoa,
                     titulo=acao.t, corpo=corpo, nota_id=nota_id, dia=dia)
        self._anotacoes[a.id] = a
        self.grava()
        return a

    def anotacao_de(self, pessoa: str, anotacao_id: str) -> Anotacao | None:
        a = self._anotacoes.get(anotacao_id)
        return a if a and a.pessoa == pessoa else None

    def renomeia_anotacao(self, pessoa: str, anotacao_id: str,
                          titulo: str) -> bool:
        """Renomear é EDITAR: sem isto o `editou` criaria uma segunda anotação.
        """
        a = self.anotacao_de(pessoa, anotacao_id)
        if not a:
            return False
        a.titulo = titulo[:63]
        self.grava()
        return True

    def anotacoes_de(self, pessoa: str, limite: int = 12) -> list[Anotacao]:
        """As mais novas primeiro: a pessoa lembra que falou, não em que terça.
        """
        minhas = [a for a in self._anotacoes.values() if a.pessoa == pessoa]
        minhas.sort(key=lambda a: a.criada_em, reverse=True)
        return minhas[:limite]

    def desceram_depois(self, pessoa: str, desde: float) -> list[Anotacao]:
        """As anotações que o aparelho ainda não tem.

        Descem pelo `pull`: **a única via de entrada de dado** (RN-48). O marcador
        é o relógio de criação: o Google não conhece anotação, não há cursor para
        herdar.
        """
        minhas = [a for a in self._anotacoes.values()
                  if a.pessoa == pessoa and a.criada_em > desde]
        minhas.sort(key=lambda a: a.criada_em)
        return minhas

    def apaga_anotacao(self, pessoa: str, anotacao_id: str) -> bool:
        a = self._anotacoes.get(anotacao_id)
        if not a or a.pessoa != pessoa:
            return False
        del self._anotacoes[anotacao_id]
        self.grava()
        return True

    # ── o instantâneo em disco ───────────────────────────────────────
    def grava(self) -> None:
        if not self.arquivo:
            return
        try:
            dados = {"v": 1,
                     "anotacoes": [asdict(a) for a in self._anotacoes.values()]}
            tmp = self.arquivo.with_suffix(".tmp")
            tmp.write_text(json.dumps(dados, ensure_ascii=False))
            tmp.replace(self.arquivo)
        except OSError:
            pass

    def _carrega(self) -> None:
        if not self.arquivo or not self.arquivo.exists():
            return
        try:
            dados = json.loads(self.arquivo.read_text())
        except (OSError, ValueError):
            return
        if dados.get("v") != 1:
            return
        for bruto in dados.get("anotacoes", []):
            try:
                a = Anotacao(**bruto)
            except TypeError:
                continue
            self._anotacoes[a.id] = a


def anotacao_para_item(a: Anotacao) -> Item:
    """Uma anotação → um item do contrato, igual aos outros (`o` = "n").

    Sem o corpo: o objeto do parser cabe em 320 bytes. O corpo mora no cartão
    (`texto.txt`) e em `/v1/anotacoes`, a rota do PWA.
    """
    return Item(id=a.id[:39], t=a.titulo[:63], d=a.dia[:10],
                tp=Tipo.ANOTACAO, o="n", nota=a.nota_id[:15])


def e_do_google(tipo: Tipo) -> bool:
    """Este tipo mora lá fora? Uma função só, para confirmar, renomear e apagar
    nunca mandarem uma anotação ao Calendar.
    """
    return tipo in (Tipo.EVENTO, Tipo.TAREFA, Tipo.LISTA)


notas = Notas(config.estado_ao_lado("notas"))
