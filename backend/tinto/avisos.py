"""Quem está esperando o Google dizer alguma coisa.

O `pull` fica pendurado até vinte e cinco segundos. `events.watch` faz
toda mudança virar um POST na nossa porta, e este módulo acende o
`asyncio.Event` da pessoa para o pull acordar.

**Acelerador, e não dependência**: a pergunta periódica continua no laço
de espera, e canal expirado ou aviso perdido só deixam a sincronização
mais lenta. Em memória: quem espera também não sobrevive a reinício.
"""

import asyncio
import logging

log = logging.getLogger("tinto")

# pessoa → o Event que o aviso acende.
_esperas: dict[str, asyncio.Event] = {}

# id do canal → pessoa: o aviso do Google diz o CANAL, não a agenda.
_canais: dict[str, str] = {}


def espera_de(pessoa: str) -> asyncio.Event:
    """O Event desta pessoa, criado na primeira vez que alguém espera."""
    ev = _esperas.get(pessoa)
    if ev is None:
        ev = asyncio.Event()
        _esperas[pessoa] = ev
    return ev


def registra_canal(canal_id: str, pessoa: str) -> None:
    _canais[canal_id] = pessoa


def esquece_canal(canal_id: str) -> None:
    _canais.pop(canal_id, None)


def acorda(canal_id: str) -> str:
    """O Google avisou: acorda quem espera e diz de quem era.

    Vazio quando o canal não é conhecido (depois de um reinício): não é erro,
    o próximo `pull` daquela pessoa o registra de novo.
    """
    pessoa = _canais.get(canal_id, "")
    if not pessoa:
        return ""

    ev = _esperas.get(pessoa)
    if ev is not None:
        ev.set()
    return pessoa


def acorda_todos() -> None:
    """Para quando o canal não se reconhece: acordar todos custa uma pergunta
    ao Google por pessoa; perder o aviso custa um ciclo de atraso.
    """
    for ev in _esperas.values():
        ev.set()
