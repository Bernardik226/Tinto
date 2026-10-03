"""O Acervo da pessoa: catálogo único, online, e de mais ninguém."""

import pytest

from tinto.acervo import ConversaoEstado, ObraTipo, acervo


@pytest.fixture
def limpo_acervo(tmp_path):
    """Um acervo vazio, com arquivo próprio para provar que ele sobrevive."""
    acervo.esquece_tudo()
    acervo.arquivo = tmp_path / "acervo.json"
    yield acervo
    acervo.esquece_tudo()
    acervo.arquivo = None


def test_a_obra_nasce_recebida_e_pertence_a_uma_pessoa(limpo_acervo):
    """Toda obra tem dono desde o primeiro instante."""
    obra = limpo_acervo.cria_obra("eu@x.com", titulo="Dom Casmurro",
                                  tipo=ObraTipo.LIVRO)

    assert obra.pessoa == "eu@x.com"
    assert obra.titulo == "Dom Casmurro"
    assert obra.estado == ConversaoEstado.RECEBIDO
    assert obra.id


def test_a_capa_mora_fora_do_catalogo_e_respeita_o_dono(limpo_acervo):
    obra = limpo_acervo.cria_obra("eu@x.com", titulo="Dom Casmurro")
    assert limpo_acervo.guarda_capa("eu@x.com", obra.id, b"png", "web")
    assert limpo_acervo.capa_de("eu@x.com", obra.id, "web") == b"png"
    assert limpo_acervo.capa_de("outro@x.com", obra.id, "web") is None
    assert obra.capa == "sim"


def test_o_acervo_de_uma_conta_nao_alcanca_o_da_outra(limpo_acervo):
    """A obra de outra conta responde como se não existisse."""
    minha = limpo_acervo.cria_obra("eu@x.com", titulo="Dom Casmurro")
    limpo_acervo.cria_obra("outro@x.com", titulo="Grande Sertão")

    assert [o.titulo for o in limpo_acervo.lista_obras("eu@x.com").obras] \
        == ["Dom Casmurro"]

    assert limpo_acervo.obra_de("outro@x.com", minha.id) is None
    assert limpo_acervo.obra_de("eu@x.com", minha.id) is not None


def test_o_catalogo_vem_em_lotes_de_doze(limpo_acervo):
    """Doze por lote, e o cursor é OPACO."""
    for i in range(30):
        limpo_acervo.cria_obra("eu@x.com", titulo=f"Obra {i:02d}")

    primeira = limpo_acervo.lista_obras("eu@x.com")
    assert len(primeira.obras) == 12
    assert primeira.cursor

    segunda = limpo_acervo.lista_obras("eu@x.com", cursor=primeira.cursor)
    assert len(segunda.obras) == 12

    terceira = limpo_acervo.lista_obras("eu@x.com", cursor=segunda.cursor)
    assert len(terceira.obras) == 6
    assert not terceira.cursor

    vistos = [o.id for o in primeira.obras + segunda.obras + terceira.obras]
    assert len(set(vistos)) == 30


def test_o_filtro_olha_o_catalogo_inteiro_e_nao_o_lote(limpo_acervo):
    """§1: "o filtro consulta o catálogo inteiro, não apenas o lote visível"."""
    for i in range(20):
        limpo_acervo.cria_obra("eu@x.com", titulo=f"Livro {i:02d}",
                               tipo=ObraTipo.LIVRO)
    limpo_acervo.cria_obra("eu@x.com", titulo="Contrato",
                           tipo=ObraTipo.DOCUMENTO)

    so_documentos = limpo_acervo.lista_obras("eu@x.com",
                                             tipo=ObraTipo.DOCUMENTO)
    assert [o.titulo for o in so_documentos.obras] == ["Contrato"]


def test_a_conversao_anda_pelos_estados_declarados(limpo_acervo):
    """Cinco estados, e nenhum outro: recebido, convertendo, pronto,
    recusado, falhou. Estado inventado é estado que a tela não sabe
    desenhar."""
    obra = limpo_acervo.cria_obra("eu@x.com", titulo="Dom Casmurro")

    limpo_acervo.atualiza_obra("eu@x.com", obra.id,
                               estado=ConversaoEstado.CONVERTENDO)
    assert limpo_acervo.obra_de("eu@x.com", obra.id).estado \
        == ConversaoEstado.CONVERTENDO

    limpo_acervo.atualiza_obra("eu@x.com", obra.id,
                               estado=ConversaoEstado.PRONTO)
    assert limpo_acervo.obra_de("eu@x.com", obra.id).estado \
        == ConversaoEstado.PRONTO

    # E de outra conta não se mexe.
    assert not limpo_acervo.atualiza_obra("outro@x.com", obra.id,
                                          estado=ConversaoEstado.FALHOU)
    assert limpo_acervo.obra_de("eu@x.com", obra.id).estado \
        == ConversaoEstado.PRONTO


def test_remover_online_nao_apaga_a_copia_de_ninguem(limpo_acervo):
    """§ "Cópias já baixadas nos seus Tintos continuarão disponíveis"."""
    obra = limpo_acervo.cria_obra("eu@x.com", titulo="Dom Casmurro")
    limpo_acervo.marca_no_dispositivo("eu@x.com", obra.id, "AA:BB:CC", True)

    assert limpo_acervo.remove_obra("eu@x.com", obra.id)
    assert limpo_acervo.obra_de("eu@x.com", obra.id) is None
    assert not limpo_acervo.remove_obra("eu@x.com", obra.id)


def test_no_dispositivo_e_por_aparelho_e_o_acervo_e_da_conta(limpo_acervo):
    """§1: o catálogo é da conta; cada aparelho decide o que baixa."""
    obra = limpo_acervo.cria_obra("eu@x.com", titulo="Dom Casmurro")
    limpo_acervo.marca_no_dispositivo("eu@x.com", obra.id, "AA:BB:CC", True)

    lida = limpo_acervo.obra_de("eu@x.com", obra.id)
    assert lida.no_dispositivo("AA:BB:CC")
    assert not lida.no_dispositivo("DD:EE:FF")


def test_o_acervo_sobrevive_a_um_restart(limpo_acervo, tmp_path):
    """O catálogo é online: perder no restart é perder a obra da pessoa."""
    obra = limpo_acervo.cria_obra("eu@x.com", titulo="Dom Casmurro")
    limpo_acervo.marca_no_dispositivo("eu@x.com", obra.id, "AA:BB:CC", True)
    limpo_acervo.grava()

    outro = type(limpo_acervo)()
    outro.arquivo = tmp_path / "acervo.json"
    outro.carrega()

    lida = outro.obra_de("eu@x.com", obra.id)
    assert lida is not None
    assert lida.titulo == "Dom Casmurro"
    assert lida.no_dispositivo("AA:BB:CC")
