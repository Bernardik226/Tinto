"""O chão dos testes: um Google falso, um aparelho pareado, e nada de rede."""

import sys
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tinto import agenda, config, ia, oauth          # noqa: E402
from tinto import portaria                           # noqa: E402
from tinto import pwa as app_pessoa                  # noqa: E402
from tinto.app import app                            # noqa: E402
from tinto.memoria import memoria                    # noqa: E402
from tinto.modelo import notas                       # noqa: E402

from .fora import Fora                             # noqa: E402


class _TodasAsContas(dict):
    """Uma TINTO_CONTAS que contém qualquer e-mail, com o limite padrão."""

    def __contains__(self, email):
        return True


@pytest.fixture
def fora(monkeypatch):
    """O mundo lá fora, plugado nos três lugares que falam com ele."""
    g = Fora()
    t = g.transporte()
    monkeypatch.setattr(agenda, "transporte", t)
    monkeypatch.setattr(oauth, "transporte", t)
    monkeypatch.setattr(ia, "transporte", t)

    for nome in ("GOOGLE_CLIENT_ID", "GOOGLE_CLIENT_SECRET",
                 "GROQ_API_KEY", "ANTHROPIC_API_KEY"):
        monkeypatch.setattr(config, nome, "de-teste")

    return g


@pytest.fixture
def limpo(monkeypatch):
    """Estado zerado entre testes."""
    memoria.aparelhos.clear()
    memoria.por_device.clear()
    memoria.pareamentos.clear()
    memoria.operacoes.clear()
    memoria.pessoas.clear()
    memoria.cotas.clear()
    memoria.sessoes.clear()
    memoria.arquivo = None
    notas._notas.clear()
    notas._anotacoes.clear()
    notas.arquivo = None

    # As sessões do aplicativo, que vivem em módulo e não em `memoria`.
    monkeypatch.setattr(config, "CONTAS", _TodasAsContas())

    # Os contadores de tentativa, que são por IP e globais ao processo: a
    # suíte inteira vem do mesmo endereço, e sem zerar aqui o vigésimo
    # teste que registra um aparelho leva 429 por causa dos dezenove
    # anteriores.
    portaria.esquece_tudo()

    app_pessoa._pessoas.clear()
    app_pessoa._indo.clear()
    yield


@pytest.fixture
def cliente(limpo):
    return TestClient(app)


@pytest.fixture
def pareado(cliente):
    """Um aparelho com conta, que é o estado em que ele passa a vida."""
    r = cliente.post("/v1/registrar", json={"device_id": "AA:BB:CC"})
    token = r.json()["device_token"]

    ap = memoria.por_token(token)
    ap.pessoa = "eu@x.com"
    ap.google_refresh = "ref-1"
    ap.tz_nome = "America/Sao_Paulo"

    return cliente, {"Authorization": f"Bearer {token}"}, ap
