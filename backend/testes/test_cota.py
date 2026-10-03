"""A cota de voz é da PESSOA, e não do aparelho."""
import time

import pytest

from tinto import config
from tinto.memoria import Aparelho, Memoria


def _com_pessoa(m: Memoria, device_id: str, quem: str) -> Aparelho:
    ap = m.provisiona(device_id)
    ap.pessoa = quem
    return ap


def test_dois_tintos_da_mesma_pessoa_dividem_a_mesma_cota():
    m = Memoria()
    mesa = _com_pessoa(m, "aa:01", "usuario@exemplo.com")
    cozinha = _com_pessoa(m, "aa:02", "usuario@exemplo.com")

    m.gastou(mesa, 600, capturou=True)

    usados_mesa, limite_mesa, _ = m.quota(mesa)
    usados_cozinha, limite_cozinha, _ = m.quota(cozinha)

    assert usados_mesa == 600
    assert usados_cozinha == 600          # o mesmo minuto, visto dos dois
    assert limite_mesa == limite_cozinha


def test_a_conta_guarda_os_ultimos_envios_com_segundos_exatos():
    m = Memoria()
    ap = _com_pessoa(m, "aa:01", "usuario@exemplo.com")
    m.gastou(ap, 62, capturou=True, tokens=314)
    m.gastou(ap, 9, capturou=True)

    recentes = m.usos_recentes(ap)
    assert [u["segundos"] for u in recentes[:2]] == [9, 62]
    assert all("em" in u for u in recentes)
    assert recentes[1]["tokens"] == 314


def test_o_limite_da_conta_vale_para_todos_os_aparelhos_dela(monkeypatch):
    monkeypatch.setattr(config, "CONTAS", {"usuario@exemplo.com": 60})
    m = Memoria()
    mesa = _com_pessoa(m, "aa:01", "usuario@exemplo.com")
    cozinha = _com_pessoa(m, "aa:02", "usuario@exemplo.com")

    assert m.cabe_falar(cozinha, 30)
    m.gastou(cozinha, 45, capturou=True)
    assert not m.cabe_falar(mesa, 30)     # o que a cozinha gastou falta na mesa


def test_contas_diferentes_nao_se_misturam():
    m = Memoria()
    meu = _com_pessoa(m, "aa:01", "usuario@exemplo.com")
    dela = _com_pessoa(m, "aa:02", "convidado@exemplo.com")

    m.gastou(meu, 600, capturou=True)

    assert m.quota(dela)[0] == 0


def test_o_aparelho_sem_conta_tem_a_cota_dele():
    """Antes de parear ele não tem pessoa, e ainda assim é contado."""
    m = Memoria()
    um = m.provisiona("aa:01")
    outro = m.provisiona("aa:02")

    m.gastou(um, 300, capturou=True)

    assert m.quota(um)[0] == 300
    assert m.quota(outro)[0] == 0


def test_a_cota_antiga_do_aparelho_vira_cota_da_pessoa_no_boot(tmp_path):
    """A migração acontece ao LER o arquivo, e não num script à parte."""
    arquivo = tmp_path / "estado.json"
    arquivo.write_text('''{"v": 1, "aparelhos": [
      {"device_id": "aa:01", "token": "t1", "pessoa": "usuario@exemplo.com",
       "usados_s": 400, "capturas": 4, "limite_s": 1800, "ciclo_em": %f},
      {"device_id": "aa:02", "token": "t2", "pessoa": "usuario@exemplo.com",
       "usados_s": 900, "capturas": 9, "limite_s": 1800, "ciclo_em": %f}
    ], "pareamentos": [], "pessoas": {}, "operacoes": []}''' % (
        time.time(), time.time()))

    m = Memoria(str(arquivo))
    ap = m.por_device["aa:01"]

    usados, limite, _ = m.quota(ap)
    assert usados == 900          # o maior dos dois, não a soma
    assert limite == 1800         # o padrão: o limite vem de TINTO_CONTAS


def test_a_cota_sobrevive_ao_reinicio(tmp_path, monkeypatch):
    monkeypatch.setattr(config, "CONTAS", {"usuario@exemplo.com": 1200})
    arquivo = tmp_path / "estado.json"

    m = Memoria(str(arquivo))
    ap = _com_pessoa(m, "aa:01", "usuario@exemplo.com")
    m.gastou(ap, 300, capturou=True)

    outra = Memoria(str(arquivo))
    usados, limite, _ = outra.quota(outra.por_device["aa:01"])
    assert (usados, limite) == (300, 1200)


@pytest.fixture
def pessoa(cliente):
    """Uma sessão do aplicativo, para ver a cota pelos olhos de quem usa."""
    from tinto import pwa as app_pessoa

    app_pessoa._pessoas["s-cota"] = "usuario@exemplo.com"
    cliente.cookies.set("tinto_pessoa", "s-cota")
    return "usuario@exemplo.com"


def test_o_app_nao_conta_a_mesma_fala_duas_vezes(cliente, pessoa):
    """Dois Tintos na conta, e os minutos aparecem uma vez."""
    from tinto.memoria import memoria

    for device_id in ("aa:01", "aa:02"):
        ap = memoria.provisiona(device_id)
        ap.pessoa = pessoa

    memoria.gastou(memoria.por_device["aa:01"], 300, capturou=True)

    voz = cliente.get("/e/api/conta").json()["voz"]
    assert voz["usados"] == 300
    assert voz["recentes"][0]["segundos"] == 300


# ── TINTO_CONTAS: quem entra, e com quantos minutos ─────────────────
def test_tinto_contas_le_o_limite_de_cada_email(monkeypatch):
    monkeypatch.setenv("TINTO_CONTAS",
                       " Voce@Gmail.com:120, mae@gmail.com:30 ,pai@gmail.com")
    contas = config._contas("TINTO_CONTAS")
    assert contas == {"voce@gmail.com": 7200, "mae@gmail.com": 1800,
                      "pai@gmail.com": None}

    monkeypatch.setattr(config, "CONTAS", contas)
    assert config.limite_de("VOCE@gmail.com") == 7200
    assert config.limite_de("pai@gmail.com") == config.QUOTA_PADRAO_S


def test_tinto_contas_vazia_nao_deixa_ninguem_entrar(monkeypatch):
    """Falha fechada: esquecer a variável tranca, nunca abre."""
    monkeypatch.setenv("TINTO_CONTAS", "")
    monkeypatch.setattr(config, "CONTAS", config._contas("TINTO_CONTAS"))
    assert not config.entra_conta("qualquer@gmail.com")


# ── "anotou" sem tipo é anotação ─────────────────────────────────────
def test_anotou_sem_tipo_sai_como_anotacao():
    """O modelo às vezes manda o verbo e esquece o `tp`: a ação ia ao
    aparelho com tipo vazio, e ele a tratava como compromisso."""
    import json
    from tinto import ia
    from tinto.contrato import Tipo

    texto = json.dumps({"acoes": [
        {"v": "anotou", "t": "ideia do case"},
        {"v": "criou", "t": "sem tipo nenhum"},
        {"v": "criou", "t": "dentista", "tp": 4, "h": "15:00",
         "d": "2026-10-02"},
    ]})
    acoes = ia._acoes_de(texto)
    assert [(a.v, a.tp) for a in acoes] == [
        ("anotou", Tipo.ANOTACAO), ("anotou", Tipo.ANOTACAO),
        ("criou", Tipo.EVENTO)]
