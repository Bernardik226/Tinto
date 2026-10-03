"""A jornada inteira, do arquivo ao leitor."""

import pytest

from tinto import pwa as app_pessoa
from tinto.acervo import acervo


@pytest.fixture
def jornada(pareado, fora, cliente):
    """Uma conta com sessão de PWA e um Tinto pareado — a mesma pessoa."""
    _, cab, ap = pareado
    acervo.esquece_tudo()
    app_pessoa._pessoas["s-jornada"] = ap.pessoa
    cliente.cookies.set("tinto_pessoa", ap.pessoa and "s-jornada")
    yield cliente, cab, ap
    acervo.esquece_tudo()


def test_do_upload_ao_aparelho_o_livro_atravessa_inteiro(jornada):
    """Enviar no PWA → converter → catálogo → baixar no Tinto."""
    cliente, cab, ap = jornada
    texto = "Era uma vez, num lugar distante. " * 40

    # 1 · a pessoa manda pelo PWA
    enviada = cliente.post("/e/api/acervo",
                           files={"arquivo": ("dom.txt", texto.encode(),
                                              "text/plain")}).json()
    assert enviada["estado"] == "pronto"
    cliente.post(f"/e/api/acervo/{enviada['id']}/confirmar")

    # 2 · o APARELHO vê a mesma obra, e ainda não a tem
    lote = cliente.get("/v1/acervo", headers=cab).json()
    assert [o["t"] for o in lote["obras"]] == ["dom"]
    assert lote["obras"][0]["aqui"] is False

    # 3 · e baixa, com o tamanho e o hash para conferir a cópia
    conteudo = cliente.get(f"/v1/acervo/{enviada['id']}/conteudo", headers=cab)
    assert conteudo.text == texto.strip()

    # ── o `n` do lote é o do texto CONVERTIDO ────────────────────────
    # E não o do arquivo que subiu: a conversão apara espaço nas pontas,
    # então os dois diferem por um byte num .txt e por milhares num
    # EPUB. O que precisa bater é o par que o APARELHO usa para conferir
    # a cópia — o `n` anunciado e os bytes entregues. Um byte de
    # diferença aqui é uma obra que nunca vira local.
    assert lote["obras"][0]["n"] == len(conteudo.content)
    assert int(conteudo.headers["x-tinto-tamanho"]) == len(conteudo.content)

    # 4 · o aparelho anuncia que baixou, e o PWA passa a mostrar
    cliente.post(f"/v1/acervo/{enviada['id']}/presenca", headers=cab,
                 json={"tem": True})
    no_pwa = cliente.get("/e/api/acervo").json()["obras"][0]
    assert no_pwa["dispositivos"] == [ap.device_id]


def test_remover_online_nao_tira_a_copia_do_aparelho(jornada):
    """§: "após remoção online, aparelhos ainda locais continuam lendo"."""
    cliente, cab, ap = jornada

    obra = cliente.post("/e/api/acervo",
                        files={"arquivo": ("livro.txt", b"Texto. " * 50,
                                           "text/plain")}).json()
    cliente.post(f"/e/api/acervo/{obra['id']}/confirmar")
    cliente.post(f"/v1/acervo/{obra['id']}/presenca", headers=cab,
                 json={"tem": True})

    r = cliente.delete(f"/e/api/acervo/{obra['id']}")
    assert "continuarão disponíveis" in r.json()["aviso"]

    # Sai do catálogo dos dois lados — e é só isso que ela faz.
    assert cliente.get("/e/api/acervo").json()["obras"] == []
    assert cliente.get("/v1/acervo", headers=cab).json()["obras"] == []

    # Nenhum comando de apagar saiu para o aparelho: quem tem a cópia
    # continua com ela, e o firmware nem fica sabendo.
    assert cliente.get(f"/v1/acervo/{obra['id']}/conteudo",
                       headers=cab).status_code == 404


def test_dois_tintos_da_mesma_pessoa_veem_a_mesma_estante(jornada, cliente):
    """O Acervo é da CONTA. Cada aparelho decide o que baixa."""
    _, cab, ap = jornada
    from tinto.memoria import memoria

    obra = cliente.post("/e/api/acervo",
                        files={"arquivo": ("livro.txt", b"Texto. " * 50,
                                           "text/plain")}).json()
    cliente.post(f"/e/api/acervo/{obra['id']}/confirmar")

    # O segundo Tinto dela.
    memoria.provisiona("DD:EE:FF")
    tok2 = cliente.post("/v1/registrar",
                        json={"device_id": "DD:EE:FF"}).json()["device_token"]
    ap2 = memoria.por_token(tok2)
    ap2.pessoa = ap.pessoa
    cab2 = {"Authorization": f"Bearer {tok2}"}

    # Os dois veem a obra; só o primeiro a tem.
    cliente.post(f"/v1/acervo/{obra['id']}/presenca", headers=cab,
                 json={"tem": True})

    um = cliente.get("/v1/acervo", headers=cab).json()["obras"][0]
    dois = cliente.get("/v1/acervo", headers=cab2).json()["obras"][0]

    assert um["id"] == dois["id"]
    assert um["aqui"] is True
    assert dois["aqui"] is False


def test_o_acervo_de_outra_pessoa_nao_atravessa(jornada, cliente):
    """Dados de duas pessoas nunca aparecem no mesmo catálogo."""
    _, cab, ap = jornada

    alheia = acervo.cria_obra("outra@x.com", titulo="Diário")

    assert cliente.get("/v1/acervo", headers=cab).json()["obras"] == []
    assert cliente.get(f"/v1/acervo/{alheia.id}/conteudo",
                       headers=cab).status_code == 404
    assert cliente.get(f"/e/api/acervo/{alheia.id}").status_code == 404


def test_o_mesmo_arquivo_reenviado_nao_duplica_a_estante(jornada):
    """A pessoa aperta duas vezes, ou a conexão cai no meio."""
    cliente, cab, _ = jornada
    conteudo = b"Era uma vez. " * 60

    um = cliente.post("/e/api/acervo",
                      files={"arquivo": ("a.txt", conteudo, "text/plain")})
    dois = cliente.post("/e/api/acervo",
                        files={"arquivo": ("a.txt", conteudo, "text/plain")})

    assert um.json()["id"] == dois.json()["id"]
    cliente.post(f"/e/api/acervo/{um.json()['id']}/confirmar")
    assert len(cliente.get("/v1/acervo", headers=cab).json()["obras"]) == 1
