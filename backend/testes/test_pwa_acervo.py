"""O acervo e o envio, pelo que o backend precisa sustentar."""

import pytest

from tinto import pwa as app_pessoa
from tinto.acervo import ObraTipo, acervo


@pytest.fixture
def pessoa(cliente):
    acervo.esquece_tudo()
    app_pessoa._pessoas["s-acervo"] = "eu@x.com"
    cliente.cookies.set("tinto_pessoa", "s-acervo")
    yield cliente
    acervo.esquece_tudo()


def test_o_passo_de_revisar_recebe_amostra_e_contagem(pessoa):
    """O terceiro passo pergunta "o texto atravessou legível?"."""
    r = pessoa.post("/e/api/acervo",
                    files={"arquivo": ("dom.txt",
                                       "Dom Casmurro. Uma noite destas. "
                                       .encode() * 40, "text/plain")})
    obra = r.json()

    assert obra["amostra"].startswith("Dom Casmurro")
    assert obra["caracteres"] > 600
    assert obra["estado"] == "pronto"
    assert obra["publicada"] is False
    assert pessoa.get("/e/api/acervo").json()["obras"] == []


def test_so_a_confirmacao_final_publica_e_cancelar_descarta(pessoa):
    rascunho = pessoa.post(
        "/e/api/acervo",
        files={"arquivo": ("livro.txt", b"Texto do livro. " * 40,
                            "text/plain")},
    ).json()

    publicado = pessoa.post(
        f"/e/api/acervo/{rascunho['id']}/confirmar").json()
    assert publicado["publicada"] is True
    assert [o["id"] for o in pessoa.get("/e/api/acervo").json()["obras"]] \
        == [rascunho["id"]]

    outro = pessoa.post(
        "/e/api/acervo",
        files={"arquivo": ("cancelar.txt", b"Outro texto. " * 40,
                            "text/plain")},
    ).json()
    assert pessoa.delete(f"/e/api/acervo/{outro['id']}").status_code == 200
    assert pessoa.get(f"/e/api/acervo/{outro['id']}").status_code == 404


def test_o_passo_de_confirmar_corrige_o_que_a_maquina_errou(pessoa):
    """Título, autor e tipo se corrigem — e o resto não."""
    obra = pessoa.post("/e/api/acervo",
                       files={"arquivo": ("relatorio.txt",
                                          b"Texto do relatorio. " * 40,
                                          "text/plain")}).json()

    r = pessoa.patch(f"/e/api/acervo/{obra['id']}",
                     json={"titulo": "Relatório final", "autor": "Usuário",
                           "tipo": "livro"})
    assert r.status_code == 200

    corrigida = r.json()
    assert corrigida["titulo"] == "Relatório final"
    assert corrigida["autor"] == "Usuário"
    assert corrigida["tipo"] == ObraTipo.LIVRO.value


def test_cancelar_no_meio_nao_deixa_obra_pela_metade(pessoa):
    """Sair antes do passo 2 não cria nada."""
    assert pessoa.get("/e/api/acervo").json()["obras"] == []


def test_o_filtro_separa_livro_de_documento(pessoa):
    """Os três filtros da tela: Todos, Livros e Documentos."""
    acervo.cria_obra("eu@x.com", titulo="Dom Casmurro", tipo=ObraTipo.LIVRO)
    acervo.cria_obra("eu@x.com", titulo="Contrato", tipo=ObraTipo.DOCUMENTO)

    todos = pessoa.get("/e/api/acervo").json()["obras"]
    livros = pessoa.get("/e/api/acervo?filtro=livro").json()["obras"]
    docs = pessoa.get("/e/api/acervo?filtro=documento").json()["obras"]

    assert len(todos) == 2
    assert [o["titulo"] for o in livros] == ["Dom Casmurro"]
    assert [o["titulo"] for o in docs] == ["Contrato"]


def test_remover_avisa_que_as_copias_ficam(pessoa):
    """A frase é literal, e é a promessa que o aparelho cumpre."""
    obra = acervo.cria_obra("eu@x.com", titulo="Dom Casmurro")

    r = pessoa.delete(f"/e/api/acervo/{obra.id}")
    assert "continuarão disponíveis" in r.json()["aviso"]


def test_o_nome_do_aparelho_e_da_pessoa(pessoa):
    """Quem tem dois Tintos precisa distingui-los, e "AA:BB:CC" não é
    nome."""
    from tinto.memoria import memoria

    ap = memoria.provisiona("AA:BB:CC", "tinto-01")
    ap.pessoa = "eu@x.com"

    r = pessoa.patch("/e/api/aparelho/AA:BB:CC", json={"nome": "o da mesa"})
    assert r.status_code == 200
    assert memoria.por_device["AA:BB:CC"].nome == "o da mesa"

    # E o de outra conta responde 404, como tudo mais.
    outro = memoria.provisiona("FF:FF:FF")
    outro.pessoa = "outra@x.com"
    assert pessoa.patch("/e/api/aparelho/FF:FF:FF",
                        json={"nome": "meu"}).status_code == 404
