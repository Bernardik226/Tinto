"""O arquivo que chega → o texto que o Tinto lê.

**O que sai é texto repaginado, não uma foto da página**: 240 px e uma
fonte só não sobrevivem a diagramação. E o que ENTRA é hostil:

· o FORMATO vem dos bytes, nunca do nome nem do cabeçalho
· o TETO é conferido antes de ler e de novo depois de descomprimir
· nada do que chegou vira caminho de arquivo local

RECUSA não é FALHA: recusado é o arquivo que não serve (PDF só imagem,
formato que não lemos) e a pessoa manda outro. Falha é nossa.
"""

from __future__ import annotations

import hashlib
import io
import math
import posixpath
import re
import unicodedata
import zipfile
from dataclasses import dataclass
from enum import Enum
from pathlib import Path

# 50 MB no que chega, 20 milhões de caracteres no que sai: um zip de 2 MB
# vira 8 GB, e medir só na saída é medir depois de gastar a memória.
ENTRADA_MAX = 50 * 1024 * 1024
TEXTO_MAX = 20 * 1000 * 1000

# Menos que isto não é obra: um arquivo vazio abriria no leitor sem nada.
TEXTO_MIN = 40


def normaliza_texto_editorial(texto: str) -> str:
    """Unicode editorial previsível para uma fonte embarcada finita."""
    texto = unicodedata.normalize("NFKC", texto or "")
    texto = texto.translate(str.maketrans({
        "‣": "•", "⁃": "•", "∙": "•",
        "☐": "[ ]", "☑": "[x]", "✓": "[x]", "✔": "[x]",
    }))
    texto = texto.translate({
        0x00A0: " ", 0x2007: " ", 0x2009: " ", 0x202F: " ",
        0xFEFF: "", 0x00AD: "", 0x0000: "",
    })
    texto = re.sub(r"[\t\v\f\r ]+", " ", texto)
    texto = re.sub(r" *\n *", "\n", texto)
    texto = re.sub(r"\n{3,}", "\n\n", texto)
    return texto.strip()


def reconstroi_paginas_pdf(paginas: list[str]) -> str:
    """Transforma linhas visuais do PDF em parágrafos repagináveis."""
    limpas = [normaliza_texto_editorial(p) for p in paginas if p.strip()]
    if not limpas:
        return ""

    # Primeira/última linha repetida em mais da metade das páginas é
    # cabeçalho/rodapé, não conteúdo. Números isolados também são rodapé.
    bordas: dict[str, int] = {}
    por_pagina = []
    for pagina in limpas:
        linhas = pagina.splitlines()
        por_pagina.append(linhas)
        for linha in ([linhas[0], linhas[-1]] if linhas else []):
            chave = linha.strip()
            if chave: bordas[chave] = bordas.get(chave, 0) + 1
    repetidas = {s for s, n in bordas.items() if n > len(por_pagina) / 2}

    saida: list[str] = []
    for linhas in por_pagina:
        paragrafos: list[str] = []
        atual = ""
        for pos, linha in enumerate(linhas):
            linha = linha.strip()
            if (linha in repetidas and pos in (0, len(linhas) - 1)) or \
               (pos == len(linhas) - 1 and re.fullmatch(r"[–—-]?\s*\d+", linha)):
                continue
            if not linha:
                if atual: paragrafos.append(atual); atual = ""
                continue
            lista = bool(re.match(r"(?:[•◦▪*-]|\d+[.)])\s+", linha))
            if lista:
                if atual: paragrafos.append(atual); atual = ""
                paragrafos.append(linha)
                continue
            if atual.endswith("-") and linha[:1].islower():
                atual = atual[:-1] + linha
            elif atual:
                atual += " " + linha
            else:
                atual = linha
        if atual: paragrafos.append(atual)
        if paragrafos: saida.append("\n\n".join(paragrafos))
    return normaliza_texto_editorial("\n\n".join(saida))


class Recusa(str, Enum):
    FORMATO = "formato"
    TAMANHO = "tamanho"
    SEM_TEXTO = "sem_texto"


@dataclass
class Achado:
    """O que a inspeção descobriu sobre o que chegou."""

    formato: str = ""
    tamanho: int = 0
    recusa: Recusa | None = None
    motivo: str = ""


@dataclass
class Extraido:
    """O texto, e o que veio junto dele."""

    texto: str = ""
    titulo: str = ""
    autor: str = ""
    descricao: str = ""
    capa: bytes = b""
    recusa: Recusa | None = None
    motivo: str = ""


@dataclass
class Normalizada:
    """A obra pronta para a tela de revisão do envio."""

    titulo: str = ""
    autor: str = ""
    sinopse: str = ""
    amostra: str = ""
    caracteres: int = 0
    palavras: int = 0
    minutos_leitura: int = 0
    resumo_hash: str = ""


# ── o formato vem dos BYTES ────────────────────────────────────────
# Na ordem em que se conferem. `zip` é ambíguo (EPUB e DOCX), e se desempata
# pelo conteúdo.
_ASSINATURAS = (
    (b"%PDF-", "pdf"),
    (b"{\\rtf", "rtf"),
)


def _formato_do_zip(dados: bytes) -> str:
    try:
        with zipfile.ZipFile(__import__("io").BytesIO(dados)) as z:
            nomes = z.namelist()
    except (zipfile.BadZipFile, OSError):
        return ""

    if any(n == "mimetype" or n.endswith(".opf") for n in nomes):
        return "epub"
    if any(n.startswith("word/") for n in nomes):
        return "docx"
    return ""


# Os únicos bytes de controle que texto tem: tabulação, quebra de linha,
# retorno de carro e alimentação de formulário.
_CONTROLE_OK = {0x09, 0x0A, 0x0D, 0x0C}


def _texto_puro(dados: bytes) -> bool:
    """Parece texto? Byte de controle já responde que não: `latin-1` decodifica
    qualquer byte, e um PNG passava por texto.
    """
    amostra = dados[:4096]
    if not amostra:
        return False
    return not any(b < 0x20 and b not in _CONTROLE_OK for b in amostra)


def inspeciona_upload(stream, nome: str = "", content_type: str = "") -> Achado:
    """O que é isto que chegou, pelos BYTES. `nome` e `content_type` servem só
    para a mensagem.
    """
    dados = stream.read(ENTRADA_MAX + 1)
    if len(dados) > ENTRADA_MAX:
        return Achado(tamanho=len(dados), recusa=Recusa.TAMANHO,
                      motivo="o arquivo passa de 50 MB")

    for assinatura, formato in _ASSINATURAS:
        if dados.startswith(assinatura):
            return Achado(formato=formato, tamanho=len(dados))

    if dados[:2] == b"PK":
        formato = _formato_do_zip(dados)
        if formato:
            return Achado(formato=formato, tamanho=len(dados))
        return Achado(tamanho=len(dados), recusa=Recusa.FORMATO,
                      motivo="pacote que não é EPUB nem DOCX")

    if _texto_puro(dados):
        formato = "md" if nome.lower().endswith((".md", ".markdown")) else "txt"
        return Achado(formato=formato, tamanho=len(dados))

    return Achado(tamanho=len(dados), recusa=Recusa.FORMATO,
                  motivo="formato que o Tinto não sabe ler")


# ── e o texto sai ───────────────────────────────────────────────────
def _sem_marcacao(html: str) -> str:
    """A marcação cai; o texto fica."""
    html = re.sub(r"(?is)<(script|style).*?</\1>", " ", html)
    html = re.sub(r"(?i)<br\s*/?>|</p>|</div>|</h\d>", "\n", html)
    texto = re.sub(r"(?s)<[^>]+>", " ", html)
    texto = (texto.replace("&nbsp;", " ").replace("&amp;", "&")
                  .replace("&lt;", "<").replace("&gt;", ">")
                  .replace("&quot;", '"').replace("&#39;", "'"))
    texto = re.sub(r"[ \t]+", " ", texto)
    return re.sub(r"\n{3,}", "\n\n", texto).strip()


def prepara_capa(original: bytes, largura: int, altura: int) -> bytes:
    """Imagem hostil → quadro exato de 1 bit, com 1 significando preto."""
    if not original or largura <= 0 or altura <= 0:
        return b""
    try:
        from PIL import Image, ImageFilter, ImageOps

        with Image.open(io.BytesIO(original)) as fonte:
            fonte.load()
            if fonte.width * fonte.height > 40_000_000:
                return b""
            imagem = ImageOps.autocontrast(fonte.convert("L"), cutoff=1)
            imagem.thumbnail((largura, altura), Image.Resampling.LANCZOS)
            quadro = Image.new("L", (largura, altura), 255)
            imagem = imagem.filter(ImageFilter.UnsharpMask(radius=0.7,
                                                            percent=115,
                                                            threshold=3))
            quadro.paste(imagem, ((largura - imagem.width) // 2,
                                  (altura - imagem.height) // 2))

            # Bayer 4×4: retícula regular no e-ink (difusão de erro parecia ruído
            # numa capa pequena).
            bayer = ((0, 8, 2, 10), (12, 4, 14, 6),
                     (3, 11, 1, 9), (15, 7, 13, 5))
            mono = Image.new("1", (largura, altura), 1)
            for y in range(altura):
                for x in range(largura):
                    limiar = (bayer[y & 3][x & 3] + 0.5) * 255 / 16
                    mono.putpixel((x, y), 255 if quadro.getpixel((x, y)) > limiar else 0)
            quadro = mono
    except (OSError, ValueError, MemoryError):
        return b""

    passo = (largura + 7) // 8
    bits = bytearray(passo * altura)
    for y in range(altura):
        for x in range(largura):
            if quadro.getpixel((x, y)) == 0:
                bits[y * passo + x // 8] |= 0x80 >> (x % 8)
    return bytes(bits)


def prepara_capa_mini(original: bytes, largura: int, altura: int) -> bytes:
    """Miniatura de 1 bit feita da imagem original, nunca de outro bitmap.
    Atkinson preserva massas e contornos sem a retícula regular, visível
    demais nos cartões pequenos.
    """
    if not original or largura <= 0 or altura <= 0:
        return b""
    try:
        from PIL import Image, ImageFilter, ImageOps

        with Image.open(io.BytesIO(original)) as fonte:
            fonte.load()
            if fonte.width * fonte.height > 40_000_000:
                return b""
            imagem = ImageOps.autocontrast(fonte.convert("L"), cutoff=1)
            imagem.thumbnail((largura, altura), Image.Resampling.LANCZOS)
            imagem = imagem.filter(ImageFilter.UnsharpMask(radius=0.55,
                                                             percent=105,
                                                             threshold=4))
            quadro = Image.new("L", (largura, altura), 255)
            quadro.paste(imagem, ((largura - imagem.width) // 2,
                                  (altura - imagem.height) // 2))
            pixels = [float(v) for v in quadro.getdata()]
            vizinhos = ((1, 0), (2, 0), (-1, 1), (0, 1), (1, 1), (0, 2))
            for y in range(altura):
                for x in range(largura):
                    i = y * largura + x
                    novo = 255.0 if pixels[i] >= 128.0 else 0.0
                    erro = (pixels[i] - novo) / 8.0
                    pixels[i] = novo
                    for dx, dy in vizinhos:
                        nx, ny = x + dx, y + dy
                        if 0 <= nx < largura and ny < altura:
                            j = ny * largura + nx
                            pixels[j] = min(255.0, max(0.0, pixels[j] + erro))
    except (OSError, ValueError, MemoryError):
        return b""

    passo = (largura + 7) // 8
    bits = bytearray(passo * altura)
    for y in range(altura):
        for x in range(largura):
            if pixels[y * largura + x] < 128:
                bits[y * passo + x // 8] |= 0x80 >> (x % 8)
    return bytes(bits)


def prepara_capa_web(original: bytes) -> bytes:
    """Reabre, limita e reencoda uma capa como PNG seguro para o PWA."""
    if not original:
        return b""
    try:
        from PIL import Image

        with Image.open(io.BytesIO(original)) as fonte:
            fonte.load()
            if fonte.width * fonte.height > 40_000_000:
                return b""
            imagem = fonte.convert("RGB")
            imagem.thumbnail((900, 1350), Image.Resampling.LANCZOS)
            saida = io.BytesIO()
            imagem.save(saida, format="PNG", optimize=True)
            return saida.getvalue()
    except (OSError, ValueError, MemoryError):
        return b""


def _do_epub(caminho: str) -> Extraido:
    partes: list[str] = []
    titulo = autor = descricao = ""
    capa = b""
    try:
        with zipfile.ZipFile(caminho) as z:
            for nome in z.namelist():
                if nome.endswith(".opf"):
                    opf = z.read(nome).decode("utf-8", "replace")
                    t = re.search(r"(?is)<dc:title[^>]*>(.*?)</dc:title>", opf)
                    a = re.search(r"(?is)<dc:creator[^>]*>(.*?)</dc:creator>",
                                  opf)
                    d = re.search(r"(?is)<dc:description[^>]*>(.*?)</dc:description>",
                                  opf)
                    titulo = _sem_marcacao(t.group(1)) if t else ""
                    autor = _sem_marcacao(a.group(1)) if a else ""
                    descricao = _sem_marcacao(d.group(1)) if d else ""

                    # EPUB 3 marca a capa com `properties=cover-image`.
                    # O caminho nasce relativo ao OPF e nunca pode subir
                    # para fora do pacote.
                    item = re.search(
                        r'(?is)<item\b(?=[^>]*\bproperties=["\'][^"\']*cover-image)'
                        r'[^>]*\bhref=["\']([^"\']+)["\'][^>]*/?>', opf)
                    if item:
                        relativo = posixpath.normpath(posixpath.join(
                            posixpath.dirname(nome), item.group(1)))
                        if not relativo.startswith("../") and relativo in z.namelist():
                            info = z.getinfo(relativo)
                            if info.file_size <= 10 * 1024 * 1024:
                                capa = z.read(relativo)

            # ── o teto é conferido ANTES de descomprimir ────────
            # `z.read()` descomprime tudo na memória antes de medir. O cabeçalho do zip
            # não é confiado, é USADO PARA RECUSAR: quem declara demais é barrado sem
            # custo, e quem declara de menos passa pela soma do que foi lido.
            declarado = sum(i.file_size for i in z.infolist())
            if declarado > TEXTO_MAX * 4:
                return Extraido(recusa=Recusa.TAMANHO,
                                motivo="o livro passa do teto de texto")

            lidos = 0
            for nome in sorted(z.namelist()):
                if not nome.lower().endswith((".xhtml", ".html", ".htm")):
                    continue
                if z.getinfo(nome).file_size > TEXTO_MAX:
                    return Extraido(recusa=Recusa.TAMANHO,
                                    motivo="o livro passa do teto de texto")

                bruto = z.read(nome)
                lidos += len(bruto)
                if lidos > TEXTO_MAX:
                    return Extraido(recusa=Recusa.TAMANHO,
                                    motivo="o livro passa do teto de texto")
                partes.append(_sem_marcacao(bruto.decode("utf-8", "replace")))
    except (zipfile.BadZipFile, OSError, KeyError):
        return Extraido(recusa=Recusa.FORMATO, motivo="EPUB ilegível")

    return Extraido(texto="\n\n".join(p for p in partes if p),
                    titulo=titulo, autor=autor, descricao=descricao, capa=capa)


def _extrai_pagina_pdf(pagina) -> str:
    """Texto corrido; o modo `layout` injeta espaços de coordenadas."""
    return pagina.extract_text() or ""


def _do_pdf(caminho: str) -> Extraido:
    """O texto do PDF, e NUNCA OCR: adivinhar as letras seria entregar um livro
    que a máquina inventou. Só imagem é recusado com o motivo.
    """
    try:
        from pypdf import PdfReader
    except ImportError:
        return Extraido(recusa=Recusa.SEM_TEXTO,
                        motivo="não consegui ler este PDF")

    try:
        leitor = PdfReader(caminho)
        meta = leitor.metadata or {}
        titulo = str(meta.get("/Title") or "").strip()
        autor = str(meta.get("/Author") or "").strip()
        descricao = str(meta.get("/Subject") or "").strip()
        partes = [_extrai_pagina_pdf(p) for p in leitor.pages]
        capa = b""
        if leitor.pages:
            for imagem in leitor.pages[0].images:
                dados = imagem.data
                if dados and len(dados) <= 10 * 1024 * 1024:
                    capa = dados
                    break
    except Exception:
        return Extraido(recusa=Recusa.SEM_TEXTO, motivo="PDF ilegível")

    texto = reconstroi_paginas_pdf(partes)
    if len(texto) < TEXTO_MIN:
        return Extraido(recusa=Recusa.SEM_TEXTO,
                        motivo="este PDF é só imagem: não há texto para ler")
    return Extraido(texto=texto, titulo=titulo, autor=autor,
                    descricao=descricao, capa=capa)


def _do_docx(caminho: str) -> Extraido:
    titulo = autor = descricao = ""
    try:
        with zipfile.ZipFile(caminho) as z:
            bruto = z.read("word/document.xml").decode("utf-8", "replace")
            try:
                props = z.read("docProps/core.xml").decode("utf-8", "replace")
                def campo(nome):
                    m = re.search(rf"(?is)<(?:dc|dcterms):{nome}[^>]*>"
                                  rf"(.*?)</(?:dc|dcterms):{nome}>", props)
                    return _sem_marcacao(m.group(1)) if m else ""
                titulo, autor = campo("title"), campo("creator")
                descricao = campo("description") or campo("subject")
            except KeyError:
                pass
    except (zipfile.BadZipFile, OSError, KeyError):
        return Extraido(recusa=Recusa.FORMATO, motivo="DOCX ilegível")

    bruto = re.sub(r"(?i)</w:p>", "\n", bruto)
    return Extraido(texto=_sem_marcacao(bruto), titulo=titulo, autor=autor,
                    descricao=descricao)


def _do_texto(caminho: str) -> Extraido:
    try:
        bruto = Path(caminho).read_bytes()
    except OSError:
        return Extraido(recusa=Recusa.FORMATO, motivo="arquivo ilegível")
    if len(bruto) > TEXTO_MAX:
        return Extraido(recusa=Recusa.TAMANHO, motivo="passa do teto de texto")
    return Extraido(texto=bruto.decode("utf-8", "replace").strip())


def extrai_texto(caminho: str, formato: str) -> Extraido:
    """O arquivo no disco → o texto corrido."""
    saida = {
        "epub": _do_epub,
        "pdf": _do_pdf,
        "docx": _do_docx,
    }.get(formato, _do_texto)(caminho)

    if saida.recusa is None and len(saida.texto.strip()) < TEXTO_MIN:
        return Extraido(recusa=Recusa.SEM_TEXTO, titulo=saida.titulo,
                        autor=saida.autor,
                        motivo="não há texto suficiente para ler")
    return saida


def normaliza_obra(texto: str, metadados: dict) -> Normalizada:
    """O texto → o que a tela de revisão do envio mostra.

    A AMOSTRA deixa conferir que virou texto legível antes de entrar no
    acervo. O HASH do texto convertido torna o reenvio idempotente.
    """
    texto = normaliza_texto_editorial(texto)
    titulo = normaliza_texto_editorial(metadados.get("titulo") or "")
    if not titulo:
        arquivo = (metadados.get("arquivo") or "").strip()
        titulo = Path(arquivo).stem if arquivo else "Sem título"

    autor = normaliza_texto_editorial(metadados.get("autor") or "")
    if not autor:
        inicio = "\n".join(texto.splitlines()[:20])
        achado = re.search(r"(?im)^\s*(?:autor(?:a)?\s*:|por\s+)\s*"
                           r"([^\n]{2,80})\s*$", inicio)
        autor = achado.group(1).strip(" .–—-") if achado else ""

    descricao = normaliza_texto_editorial(metadados.get("descricao") or "")
    if not descricao:
        paragrafos = [p.strip() for p in texto.split("\n\n") if p.strip()]
        paragrafos = [p for p in paragrafos
                      if not re.match(r"(?i)^(?:autor(?:a)?\s*:|por\s+)", p)]
        descricao = paragrafos[0] if paragrafos else ""
    palavras = len(re.findall(r"\b[^\W_]+(?:['’-][^\W_]+)*\b", texto,
                              flags=re.UNICODE))

    return Normalizada(
        titulo=titulo[:120],
        autor=autor[:80],
        sinopse=descricao[:480],
        amostra=texto[:600].strip(),
        caracteres=len(texto),
        palavras=palavras,
        minutos_leitura=max(1, math.ceil(palavras / 220)),
        resumo_hash=hashlib.sha256(texto.encode("utf-8")).hexdigest(),
    )
