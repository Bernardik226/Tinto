"""O arquivo que chega → o texto que o Tinto lê."""

import io
import zipfile

from PIL import Image

from tinto.conversao import (Recusa, extrai_texto, inspeciona_upload,
                             normaliza_obra, normaliza_texto_editorial,
                             reconstroi_paginas_pdf, prepara_capa,
                             prepara_capa_mini,
                             prepara_capa_web)
from tinto.conversao import _extrai_pagina_pdf


def test_pdf_extrai_texto_corrido_sem_espacos_geometricos():
    class Pagina:
        def extract_text(self, **opcoes):
            assert opcoes == {}
            return "Uma palavra inteira"

    assert _extrai_pagina_pdf(Pagina()) == "Uma palavra inteira"


def test_metadados_editoriais_calculam_autor_sinopse_e_tempo():
    texto = "Autor: Clarice Lispector\n\n" + ("Uma palavra clara. " * 450)
    obra = normaliza_obra(texto, {"titulo": "A hora da estrela"})

    assert obra.autor == "Clarice Lispector"
    assert obra.sinopse.startswith("Uma palavra clara")
    assert obra.palavras >= 1350
    assert obra.minutos_leitura == 7


def _png_capa() -> bytes:
    buf = io.BytesIO()
    imagem = Image.new("RGB", (20, 30), "white")
    for y in range(5, 25):
        for x in range(4, 16):
            imagem.putpixel((x, y), (0, 0, 0))
    imagem.save(buf, format="PNG")
    return buf.getvalue()


def _epub(titulo="Dom Casmurro", texto="Capítulo Primeiro. Do título.",
          com_capa=False) -> bytes:
    """Um EPUB mínimo de verdade: zip com mimetype, OPF e um XHTML."""
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w") as z:
        z.writestr("mimetype", "application/epub+zip")
        z.writestr("OEBPS/content.opf", f"""<?xml version="1.0"?>
<package xmlns="http://www.idpf.org/2007/opf" version="3.0">
 <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
  <dc:title>{titulo}</dc:title><dc:creator>Machado de Assis</dc:creator>
 </metadata>
 <manifest><item id="c1" href="c1.xhtml" media-type="application/xhtml+xml"/>
 {('<item id="cover" href="capa.png" media-type="image/png" properties="cover-image"/>') if com_capa else ''}</manifest>
 <spine><itemref idref="c1"/></spine>
</package>""")
        z.writestr("OEBPS/c1.xhtml",
                   f"<html><body><h1>{titulo}</h1><p>{texto}</p></body></html>")
        if com_capa:
            z.writestr("OEBPS/capa.png", _png_capa())
    return buf.getvalue()


def test_o_formato_vem_do_CONTEUDO_e_nao_do_nome(tmp_path):
    """Nome e `content-type` são escolhidos por quem manda."""
    achado = inspeciona_upload(io.BytesIO(_epub()), "qualquer.txt",
                               "text/plain")
    assert achado.formato == "epub"

    achado = inspeciona_upload(io.BytesIO(b"so um texto\n"), "livro.epub",
                               "application/epub+zip")
    assert achado.formato == "txt"


def test_formato_que_nao_sabemos_ler_e_RECUSADO_com_motivo(tmp_path):
    """Recusa não é falha: é o arquivo que não serve, e a pessoa resolve
    mandando outro. A tela precisa da razão para dizer isso."""
    achado = inspeciona_upload(io.BytesIO(b"\x89PNG\r\n\x1a\n" + b"0" * 100),
                               "capa.png", "image/png")
    assert achado.recusa == Recusa.FORMATO
    assert achado.motivo


def test_arquivo_grande_demais_e_recusado_ANTES_de_ler(tmp_path):
    """O teto vale antes e depois de descomprimir."""
    achado = inspeciona_upload(io.BytesIO(b"0" * (60 * 1024 * 1024)),
                               "enorme.txt", "text/plain")
    assert achado.recusa == Recusa.TAMANHO


def test_pdf_sem_texto_e_recusado_e_NUNCA_vira_ocr(tmp_path):
    """§: "não executa OCR"."""
    caminho = tmp_path / "digitalizado.pdf"
    caminho.write_bytes(b"%PDF-1.4\n% sem texto nenhum aqui\n%%EOF\n")

    saida = extrai_texto(str(caminho), "pdf")
    assert saida.recusa == Recusa.SEM_TEXTO
    assert not saida.texto


def test_o_epub_vira_texto_com_titulo_e_autor(tmp_path):
    """O caminho feliz: o que sai é texto corrido, com os metadados que o
    arquivo trazia."""
    caminho = tmp_path / "livro.epub"
    caminho.write_bytes(_epub())

    saida = extrai_texto(str(caminho), "epub")
    assert saida.recusa is None
    assert "Capítulo Primeiro" in saida.texto
    assert saida.titulo == "Dom Casmurro"
    assert saida.autor == "Machado de Assis"

    # Repaginado, e não fotografado: a marcação não atravessa.
    assert "<p>" not in saida.texto
    assert "<html>" not in saida.texto


def test_o_epub_traz_capa_e_o_backend_prepara_os_dois_tamanhos(tmp_path):
    caminho = tmp_path / "ilustrado.epub"
    caminho.write_bytes(_epub(com_capa=True))

    saida = extrai_texto(str(caminho), "epub")
    assert saida.capa.startswith(b"\x89PNG")

    mini = prepara_capa(saida.capa, 42, 75)
    grande = prepara_capa(saida.capa, 76, 114)
    assert len(mini) == ((42 + 7) // 8) * 75
    assert len(grande) == ((76 + 7) // 8) * 114
    assert any(mini)                 # há tinta preta
    assert any(b != 0xFF for b in mini)
    assert prepara_capa_web(saida.capa).startswith(b"\x89PNG")


def test_miniaturas_nascem_diretamente_da_capa_original():
    original = _png_capa()
    biblioteca = prepara_capa_mini(original, 34, 49)
    destaque = prepara_capa_mini(original, 42, 63)

    assert len(biblioteca) == 5 * 49
    assert len(destaque) == 6 * 63
    assert any(biblioteca)
    assert any(destaque)


def test_obra_sem_capa_continua_sendo_obra(tmp_path):
    caminho = tmp_path / "sem-capa.epub"
    caminho.write_bytes(_epub(com_capa=False))

    saida = extrai_texto(str(caminho), "epub")
    assert saida.recusa is None
    assert saida.capa == b""


def test_conteudo_curto_demais_nao_e_obra(tmp_path):
    """Três palavras não são um livro nem um documento."""
    caminho = tmp_path / "vazio.txt"
    caminho.write_text("oi\n")

    saida = extrai_texto(str(caminho), "txt")
    assert saida.recusa == Recusa.SEM_TEXTO


def test_a_obra_normalizada_traz_amostra_hash_e_contagem(tmp_path):
    """O que a terceira tela do envio mostra: capa, título, autor, tipo
    sugerido e AMOSTRA do texto convertido."""
    texto = "Era uma vez. " * 500
    obra = normaliza_obra(texto, {"titulo": "Contos", "autor": "Alguém"})

    assert obra.titulo == "Contos"
    assert obra.caracteres == len(texto.strip())
    assert obra.amostra and len(obra.amostra) <= 600
    assert obra.amostra in texto
    assert len(obra.resumo_hash) == 64      # sha-256 em hex

    # O mesmo texto dá o mesmo hash: é ele que torna o reenvio idempotente.
    assert normaliza_obra(texto, {}).resumo_hash == obra.resumo_hash


def test_o_titulo_cai_no_nome_do_arquivo_quando_falta(tmp_path):
    """Obra sem título é uma linha em branco na estante."""
    obra = normaliza_obra("texto " * 200, {"arquivo": "relatorio-final.pdf"})
    assert obra.titulo == "relatorio-final"


def test_normalizacao_editorial_limpa_ligaturas_espacos_e_controles():
    texto = "A\u00a0ﬁcção\u2009ﬂui.\x00\n\n\nPróximo § 2 — € 10."
    assert normaliza_texto_editorial(texto) == \
        "A ficção flui.\n\nPróximo § 2 — € 10."


def test_pdf_reconstroi_paragrafos_e_remove_cabecalho_repetido():
    paginas = [
        "ROMANCE\nUma frase que conti-\nnua na linha seguinte.\n\nNovo parágrafo.\n1",
        "ROMANCE\nOutra frase que foi\nquebrada pela página.\n\n• Uma lista\n2",
    ]
    texto = reconstroi_paginas_pdf(paginas)
    assert "ROMANCE" not in texto
    assert "continua na linha seguinte." in texto
    assert "Outra frase que foi quebrada pela página." in texto
    assert "\n\nNovo parágrafo." in texto
    assert "\n• Uma lista" in texto
