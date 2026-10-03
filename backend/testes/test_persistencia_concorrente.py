"""O monólito aceita concorrência dentro do seu único processo."""

from concurrent.futures import ThreadPoolExecutor

from tinto.acervo import Acervo
from tinto.memoria import Memoria


def test_sessoes_concorrentes_chegam_inteiras_ao_proximo_boot(tmp_path):
    arquivo = tmp_path / "estado.json"
    memoria = Memoria(str(arquivo))

    with ThreadPoolExecutor(max_workers=12) as grupo:
        fichas = list(grupo.map(
            lambda n: memoria.abre_sessao(f"pessoa-{n}@x.com"), range(120)))

    outra = Memoria(str(arquivo))
    assert {outra.dono_da_sessao(f) for f in fichas} == {
        f"pessoa-{n}@x.com" for n in range(120)
    }


def test_catalogo_concorrente_chega_inteiro_ao_proximo_boot(tmp_path):
    arquivo = tmp_path / "acervo.json"
    acervo = Acervo()
    acervo.arquivo = arquivo

    def inclui(n):
        acervo.cria_obra("eu@x.com", titulo=f"Obra {n:03d}")
        acervo.grava()

    with ThreadPoolExecutor(max_workers=12) as grupo:
        list(grupo.map(inclui, range(120)))

    outro = Acervo()
    outro.arquivo = arquivo
    outro.carrega()
    encontrados = []
    cursor = ""
    while True:
        pagina = outro.lista_obras("eu@x.com", cursor=cursor)
        encontrados.extend(o.titulo for o in pagina.obras)
        if not pagina.cursor:
            break
        cursor = pagina.cursor
    assert set(encontrados) == {f"Obra {n:03d}" for n in range(120)}
