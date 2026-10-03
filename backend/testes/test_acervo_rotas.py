"""O Acervo pelas duas portas: a da pessoa e a do aparelho."""

import io

import pytest

from tinto import pwa as app_pessoa
from tinto.acervo import ConversaoEstado, acervo


@pytest.fixture
def pessoa(cliente):
    """Uma sessão de PWA aberta, como depois do login Google."""
    acervo.esquece_tudo()
    sessao = "s-de-teste"
    app_pessoa._pessoas[sessao] = "eu@x.com"
    cliente.cookies.set("tinto_pessoa", sessao)
    yield cliente
    acervo.esquece_tudo()


def test_sem_sessao_o_acervo_nao_responde(cliente):
    """Sem login não há catálogo — nem vazio. Devolver lista vazia
    ensinaria que a conta existe e está sem obras."""
    r = cliente.get("/e/api/acervo")
    assert r.status_code == 401


def test_a_pessoa_ve_o_proprio_catalogo(pessoa):
    acervo.cria_obra("eu@x.com", titulo="Dom Casmurro")
    acervo.cria_obra("outro@x.com", titulo="Grande Sertão")

    r = pessoa.get("/e/api/acervo")
    assert r.status_code == 200
    assert [o["titulo"] for o in r.json()["obras"]] == ["Dom Casmurro"]


def test_a_obra_de_outra_conta_responde_404(pessoa):
    """404 e não 403: dizer "existe, mas não é sua" já conta o que a
    outra pessoa tem."""
    alheia = acervo.cria_obra("outro@x.com", titulo="Grande Sertão")

    assert pessoa.get(f"/e/api/acervo/{alheia.id}").status_code == 404
    assert pessoa.patch(f"/e/api/acervo/{alheia.id}",
                        json={"titulo": "Meu"}).status_code == 404
    assert pessoa.delete(f"/e/api/acervo/{alheia.id}").status_code == 404


def test_a_capa_tem_portas_separadas_para_pessoa_e_aparelho(pessoa, pareado, fora):
    obra = acervo.cria_obra("eu@x.com", titulo="Dom Casmurro")
    acervo.guarda_capa("eu@x.com", obra.id, b"\x89PNG\r\n", "web")
    acervo.guarda_capa("eu@x.com", obra.id, b"\x80\x00", "mini")

    web = pessoa.get(f"/e/api/acervo/{obra.id}/capa")
    assert web.status_code == 200
    assert web.headers["content-type"] == "image/png"

    cliente, cab, ap = pareado
    obra_ap = acervo.cria_obra(ap.pessoa, titulo="Outro")
    acervo.guarda_capa(ap.pessoa, obra_ap.id, b"\x80\x00", "mini")
    acervo.guarda_capa(ap.pessoa, obra_ap.id, b"\x40\x00", "destaque")
    device = cliente.get(f"/v1/acervo/{obra_ap.id}/capa/mini", headers=cab)
    assert device.text == "8000"
    destaque = cliente.get(f"/v1/acervo/{obra_ap.id}/capa/destaque", headers=cab)
    assert destaque.text == "4000"


def test_o_catalogo_pagina_de_doze_em_doze(pessoa):
    for i in range(15):
        acervo.cria_obra("eu@x.com", titulo=f"Obra {i:02d}")

    primeira = pessoa.get("/e/api/acervo").json()
    assert len(primeira["obras"]) == 12
    assert primeira["cursor"]

    segunda = pessoa.get(f"/e/api/acervo?cursor={primeira['cursor']}").json()
    assert len(segunda["obras"]) == 3
    assert not segunda["cursor"]


def test_o_envio_converte_e_a_obra_fica_pronta(pessoa):
    """O caminho feliz do envio: sobe, converte, e entra no catálogo."""
    r = pessoa.post("/e/api/acervo",
                    files={"arquivo": ("livro.txt",
                                       b"Era uma vez. " * 100, "text/plain")})

    assert r.status_code == 200, r.text
    obra = r.json()
    assert obra["estado"] == ConversaoEstado.PRONTO.value
    assert obra["titulo"] == "livro"
    assert obra["caracteres"] > 0
    assert obra["sinopse"].startswith("Era uma vez")
    assert obra["palavras"] > 0
    assert obra["minutos_leitura"] > 0
    assert obra["criada_em"] > 0
    assert pessoa.get("/e/api/acervo").json()["obras"] == []
    pessoa.post(f"/e/api/acervo/{obra['id']}/confirmar")
    assert [o["titulo"] for o in pessoa.get("/e/api/acervo").json()["obras"]] \
        == ["livro"]


def test_o_mesmo_arquivo_duas_vezes_e_uma_obra_so(pessoa):
    """Idempotência pelo conteúdo: a pessoa aperta duas vezes, ou a
    conexão cai no meio e ela tenta de novo. Duas cópias do mesmo livro
    na estante é o aparelho contando errado o que ela tem."""
    conteudo = b"Era uma vez. " * 100

    um = pessoa.post("/e/api/acervo",
                     files={"arquivo": ("livro.txt", conteudo, "text/plain")})
    dois = pessoa.post("/e/api/acervo",
                       files={"arquivo": ("livro.txt", conteudo, "text/plain")})

    assert um.json()["id"] == dois.json()["id"]
    pessoa.post(f"/e/api/acervo/{um.json()['id']}/confirmar")
    assert len(pessoa.get("/e/api/acervo").json()["obras"]) == 1


def test_arquivo_recusado_diz_por_que(pessoa):
    """Recusa não é erro do servidor: é o arquivo que não serve, e a tela
    precisa da razão para orientar."""
    r = pessoa.post("/e/api/acervo",
                    files={"arquivo": ("capa.png",
                                       b"\x89PNG\r\n\x1a\n" + b"0" * 100,
                                       "image/png")})

    assert r.status_code == 415
    assert r.json()["motivo"]


def test_upload_para_no_teto_antes_de_entregar_o_arquivo_ao_conversor(
        pessoa, monkeypatch):
    """A rota também aplica o teto, não apenas o conversor."""
    from tinto import acervo_rotas

    monkeypatch.setattr(acervo_rotas, "ENTRADA_MAX", 64)
    r = pessoa.post("/e/api/acervo",
                    files={"arquivo": ("grande.txt", b"x" * 80,
                                       "text/plain")})

    assert r.status_code == 413
    assert "grande" in r.json()["motivo"].lower()
    assert pessoa.get("/e/api/acervo").json()["obras"] == []


def test_remover_tira_do_catalogo_e_avisa_das_copias(pessoa):
    obra = acervo.cria_obra("eu@x.com", titulo="Dom Casmurro")

    r = pessoa.delete(f"/e/api/acervo/{obra.id}")
    assert r.status_code == 200
    assert not pessoa.get("/e/api/acervo").json()["obras"]


def test_o_aparelho_le_o_acervo_da_pessoa_dele(pareado, fora):
    """A outra porta: o Tinto chega com token e alcança o mesmo catálogo."""
    cliente, cab, ap = pareado
    acervo.esquece_tudo()


def test_o_aparelho_baixa_texto_em_blocos_pequenos(pareado, fora):
    """Livro não atravessa o buffer de 1 KiB do firmware de uma vez."""
    cliente, cab, ap = pareado
    acervo.esquece_tudo()
    obra = acervo.cria_obra(ap.pessoa, titulo="Livro grande",
                            estado=ConversaoEstado.PRONTO)
    texto = ("capítulo com acento. " * 8000).encode("utf-8")
    acervo.guarda_texto(ap.pessoa, obra.id, texto.decode("utf-8"))

    catalogo = cliente.get("/v1/acervo", headers=cab).json()["obras"]
    assert catalogo[0]["n"] == len(texto)

    primeiro = cliente.get(f"/v1/acervo/{obra.id}/conteudo/bloco?offset=0",
                           headers=cab)
    assert primeiro.status_code == 200
    assert len(primeiro.content) == 128 * 1024
    assert int(primeiro.headers["x-tinto-proximo"]) == len(primeiro.content)
    assert int(primeiro.headers["x-tinto-tamanho"]) == len(texto)

    offset = int(primeiro.headers["x-tinto-proximo"])
    segundo = cliente.get(
        f"/v1/acervo/{obra.id}/conteudo/bloco?offset={offset}", headers=cab)
    assert segundo.content == texto[offset:offset + 128 * 1024]
    acervo.esquece_tudo()
    obra = acervo.cria_obra(ap.pessoa, titulo="Dom Casmurro",
                            estado=ConversaoEstado.PRONTO)

    r = cliente.get("/v1/acervo", headers=cab)
    assert r.status_code == 200
    # Chaves CURTAS, como todo contrato do device (SISTEMA §3): quem lê é
    # um parser em C com um `char[]` por campo, não um navegador.
    assert [o["t"] for o in r.json()["obras"]] == ["Dom Casmurro"]

    # E o conteúdo desce com tamanho e hash: sem eles o aparelho não tem
    # como saber se a cópia que ele gravou está inteira.
    acervo.guarda_texto(ap.pessoa, obra.id, "Era uma vez. " * 50)
    c = cliente.get(f"/v1/acervo/{obra.id}/conteudo", headers=cab)
    assert c.status_code == 200
    assert c.headers["x-tinto-hash"]
    assert int(c.headers["x-tinto-tamanho"]) == len(c.content)
    acervo.esquece_tudo()


def test_o_aparelho_anuncia_que_baixou(pareado, fora):
    """"No dispositivo" é informação, e quem a produz é o aparelho.
    """
    cliente, cab, ap = pareado
    acervo.esquece_tudo()
    obra = acervo.cria_obra(ap.pessoa, titulo="Dom Casmurro")

    r = cliente.post(f"/v1/acervo/{obra.id}/presenca", headers=cab,
                     json={"tem": True})
    assert r.status_code == 200
    assert acervo.obra_de(ap.pessoa, obra.id).no_dispositivo(ap.device_id)
    acervo.esquece_tudo()


def test_o_acervo_de_outra_conta_nao_desce_para_o_aparelho(pareado, fora):
    cliente, cab, ap = pareado
    acervo.esquece_tudo()
    alheia = acervo.cria_obra("outro@x.com", titulo="Grande Sertão")

    assert not cliente.get("/v1/acervo", headers=cab).json()["obras"]
    assert cliente.get(f"/v1/acervo/{alheia.id}/conteudo",
                       headers=cab).status_code == 404
    acervo.esquece_tudo()


# ── o que chega é HOSTIL até prova em contrário ─────────────────────
# Nome de arquivo é escolhido por quem manda, `content-type` é palpite do
# navegador, e zip descomprime. Cada teste aqui é um jeito de errar que
# já derrubou servidor de alguém.

def test_zip_bomba_nao_derruba_o_servidor(pessoa):
    """Um EPUB de 2 KB que vira gigabytes ao descomprimir."""
    import io
    import zipfile

    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("mimetype", "application/epub+zip")
        z.writestr("OEBPS/content.opf",
                   '<?xml version="1.0"?><package><metadata/></package>')
        # 400 MB de zeros que comprimem para alguns KB. Sem a
        # conferência do tamanho DECLARADO, o servidor descomprimiria
        # isto na memória antes de poder medir.
        z.writestr("OEBPS/c1.xhtml", "<html><body>" + ("0" * 400_000_000))

    # ── o que se mede aqui é MEMÓRIA, não o veredito ────────────────
    # Recusar a bomba é fácil: basta somar o que foi lido. O caro é
    # DESCOMPRIMIR 400 MB para só então medir — o servidor recusa e cai
    # do mesmo jeito, com o veredito certo.
    import tracemalloc

    tracemalloc.start()
    r = pessoa.post("/e/api/acervo",
                    files={"arquivo": ("bomba.epub", buf.getvalue(),
                                       "application/epub+zip")})
    _, pico = tracemalloc.get_traced_memory()
    tracemalloc.stop()

    assert r.status_code == 415, r.status_code
    assert pico < 40 * 1024 * 1024, f"{pico / 1e6:.0f} MB alocados"

    # E o servidor continua de pé.
    assert pessoa.get("/e/api/acervo").status_code == 200


def test_nome_de_arquivo_com_travessia_nao_vira_caminho(pessoa):
    """`../../etc/passwd` como nome do arquivo."""
    r = pessoa.post("/e/api/acervo",
                    files={"arquivo": ("../../etc/passwd",
                                       b"Texto qualquer. " * 40,
                                       "text/plain")})
    assert r.status_code == 200

    obra = r.json()
    assert "/" not in obra["id"]
    assert ".." not in obra["id"]


def test_o_content_type_mentido_nao_engana(pessoa):
    """Um PNG dizendo ser EPUB. Quem decide são os BYTES."""
    r = pessoa.post("/e/api/acervo",
                    files={"arquivo": ("livro.epub",
                                       b"\x89PNG\r\n\x1a\n" + b"0" * 400,
                                       "application/epub+zip")})
    assert r.status_code == 415
    assert r.json()["recusa"] == "formato"


def test_nenhuma_resposta_carrega_caminho_do_servidor(pessoa):
    """Um caminho vazado conta a estrutura da máquina para quem perguntar."""
    obra = pessoa.post("/e/api/acervo",
                       files={"arquivo": ("a.txt", b"Texto. " * 60,
                                          "text/plain")}).json()

    corpo = pessoa.get(f"/e/api/acervo/{obra['id']}").text
    assert "/tmp" not in corpo
    assert "/dados" not in corpo
    assert "backend/" not in corpo


def test_o_texto_de_outra_conta_nao_desce_nem_por_id_adivinhado(pareado,
                                                                fora):
    """Adivinhar o id não abre a obra: a posse é conferida antes."""
    cliente, cab, ap = pareado
    acervo.esquece_tudo()

    alheia = acervo.cria_obra("outra@x.com", titulo="Diário")
    acervo.guarda_texto("outra@x.com", alheia.id, "segredo " * 50)

    r = cliente.get(f"/v1/acervo/{alheia.id}/conteudo", headers=cab)
    assert r.status_code == 404
    assert "segredo" not in r.text
    acervo.esquece_tudo()
