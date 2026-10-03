#!/usr/bin/env python3
"""Gera as fontes do Tinto a partir da Bitstream Vera Sans."""
import os
import sys
import unicodedata
from PIL import Image

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.join(AQUI, "..", "..")   # a raiz do projeto
ASSETS = os.path.join(RAIZ, "firmware", "assets", "fontes")

try:
    import freetype
except ImportError:
    sys.exit("falta freetype-py — pip install freetype-py.\n"
             "Só é preciso pra REGERAR as fontes; o fontes.c vai no repo.")

# DejaVu Sans.
def _f(nome):
    return os.path.join(ASSETS, nome)

TTF      = _f("DejaVuSans.ttf")
TTF_NEG  = _f("DejaVuSans-Bold.ttf")
TTF_ITA  = _f("DejaVuSans.ttf")     # sem Oblique proporcional: shear (§SHEAR)

# DejaVu Serif Bold — a serifa editorial.
TTF_SERIF = _f("DejaVuSerif-Bold.ttf")
TTF_SERIF_LEITURA = _f("DejaVuSerif.ttf")
TTF_SANS_LEITURA  = _f("PTSans.ttf")
TTF_MONO_LEITURA  = _f("FreeMono.ttf")
TTF_LITERATA = _f("Literata.ttf")
TTF_ATKINSON = _f("AtkinsonHyperlegible-Regular.ttf")
TTF_INTER = _f("Inter-Medium.ttf")
TTF_SOURCE_SERIF = _f("SourceSerif4.ttf")
TTF_SERIF_FORTE = _f("DejaVuSerif-Bold.ttf")
TTF_SANS_FORTE = _f("PTSans-Bold.ttf")
TTF_MONO_FORTE = _f("FreeMonoBold.ttf")
TTF_ATKINSON_FORTE = _f("AtkinsonHyperlegible-Bold.ttf")
TTF_INTER_FORTE = _f("Inter-SemiBold.ttf")

# ── espaçamento ──────────────────────────────────────────────────────
# +1 px entre letras e +3 na entrelinha. Em tela com antialiasing o olho
# separa letras pela borda cinza; aqui não há borda, então letra vizinha
# encosta e o par "rn" lê como "m". O tracking é o que devolve a
# separação, e custa ~1 coluna de texto por linha.
# A mono não precisa de tracking: a caixa de cada letra já é uniforme, e
# somar 1 px aqui só gastaria coluna. A entrelinha continua, porque ela
# resolve outra coisa — descendente encostando na ascendente da linha de
# baixo, que acontece em qualquer fonte sem meio-tom.
TRACKING   = 1
ENTRELINHA = 2

SHEAR = 0.25
SAIDA = os.path.join(RAIZ, "firmware", "main", "tela", "fontes.c")

# ASCII imprimível + o que o português exige.
from repertorio import ACENTOS, CONTEUDO, SETAS, CORPO   # noqa: F401

# A fonte enorme era "só dígitos" e o cartaz escreve nela também "Qua 12" e
# "Amanhã" — que saíam como tofu, e a prova da home mostrou isso por três
# etapas sem ninguém olhar. As letras aqui são exatamente as dos dias da
# semana abreviados e da palavra "Amanhã": nada além disso, porque cada
# glifo a 42 pt custa flash de verdade.
SEMANA  = "DomSegTerQuaQuiSexSábAmanhã"
DIGITOS = "0123456789:h –½" + "".join(sorted(set(SEMANA)))

# Vazio: o itálico agora vem da fonte, não do shear.
ITALICAS = {"F_CITACAO"}

FONTES = [
    # nome        arquivo  tam  caracteres  comentário
    # O orçamento de ~28 colunas por linha foi medido com
    # a DejaVu em 12 pt, e em 1 bit tamanho pequeno não tem para onde
    # esconder o serrilhado: cada haste ou cai num pixel ou no outro. Mais
    # pixel por letra é a única coisa que suaviza sem antialiasing — que
    # não existe em painel de 1 bit.
    ("F_MIUDA",   TTF,     11,  CORPO,   "rótulos, selos, rodapé"),
    ("F_CORPO",   TTF,     14,  CORPO,   "o texto do sistema"),
    # A tarefa é menor que o compromisso, e isso é hierarquia e não
    # economia: o compromisso tem hora marcada e não espera, a tarefa
    # espera. Dar o mesmo peso aos dois faz a tela dizer que são a mesma
    # urgência, e eles não são.
    ("F_CORPO_P", TTF,     12,  CORPO,   "as linhas de tarefa"),
    ("F_TITULO",  TTF_NEG, 17,  CORPO,   "título de item, cartaz"),
    ("F_ENORME",  TTF_NEG, 46,  DIGITOS, "relógio e o cartaz — só dígitos"),
    # RN-28: a transcrição crua aparece SEMPRE, em itálico. O itálico não é
    # enfeite: ele é o que separa, sem cor e sem tom, o que VOCÊ disse do que
    # a IA escreveu. Sem essa distinção visível, a nota deixa de ser
    # verificável — e verificabilidade é a promessa do produto inteiro.
    ("F_CITACAO", TTF_ITA, 14,  CORPO,   "a transcrição crua, em itálico"),

    # ── a serifa editorial ───────────────────────────────────────────
    # 17 px, o mesmo corpo do F_TITULO, e é de propósito: ela SUBSTITUI o
    # título sans em alguns lugares, e duas fontes com alturas diferentes
    # no mesmo papel fariam a composição pular ao trocar uma pela outra.
    ("F_EDITORIAL", TTF_SERIF, 17, CORPO, "a saudação da Home — serifa"),
    ("F_XADREZ", TTF, 27, "♔♕♖♗♘♙♚♛♜♝♞♟", "as peças do xadrez"),

    # Três famílias x três corpos do leitor. São fontes prontas no
    # firmware: trocar uma delas não depende de rede nem repagina no backend.
    ("F_LEITOR_SERIF_P", TTF_SERIF_LEITURA, 12, CORPO, "leitor serifado pequeno"),
    ("F_LEITOR_SERIF_M", TTF_SERIF_LEITURA, 14, CORPO, "leitor serifado médio"),
    ("F_LEITOR_SERIF_G", TTF_SERIF_LEITURA, 17, CORPO, "leitor serifado grande"),
    ("F_LEITOR_SANS_P",  TTF_SANS_LEITURA,  12, CORPO, "leitor sem serifa pequeno"),
    ("F_LEITOR_SANS_M",  TTF_SANS_LEITURA,  14, CORPO, "leitor sem serifa médio"),
    ("F_LEITOR_SANS_G",  TTF_SANS_LEITURA,  17, CORPO, "leitor sem serifa grande"),
    ("F_LEITOR_MONO_P",  TTF_MONO_LEITURA,  12, CORPO, "leitor mono pequeno"),
    ("F_LEITOR_MONO_M",  TTF_MONO_LEITURA,  14, CORPO, "leitor mono médio"),
    ("F_LEITOR_MONO_G",  TTF_MONO_LEITURA,  17, CORPO, "leitor mono grande"),
    ("F_LEITOR_LITERATA_P", TTF_LITERATA, 12, CORPO, "leitor Literata pequeno"),
    ("F_LEITOR_LITERATA_M", TTF_LITERATA, 14, CORPO, "leitor Literata médio"),
    ("F_LEITOR_LITERATA_G", TTF_LITERATA, 17, CORPO, "leitor Literata grande"),
    ("F_LEITOR_ATKINSON_P", TTF_ATKINSON, 12, CORPO, "leitor Atkinson pequeno"),
    ("F_LEITOR_ATKINSON_M", TTF_ATKINSON, 14, CORPO, "leitor Atkinson médio"),
    ("F_LEITOR_ATKINSON_G", TTF_ATKINSON, 17, CORPO, "leitor Atkinson grande"),
    ("F_LEITOR_INTER_P", TTF_INTER, 12, CORPO, "leitor Inter pequeno"),
    ("F_LEITOR_INTER_M", TTF_INTER, 14, CORPO, "leitor Inter médio"),
    ("F_LEITOR_INTER_G", TTF_INTER, 17, CORPO, "leitor Inter grande"),
    ("F_LEITOR_SOURCE_P", TTF_SOURCE_SERIF, 12, CORPO, "leitor Source Serif pequeno"),
    ("F_LEITOR_SOURCE_M", TTF_SOURCE_SERIF, 14, CORPO, "leitor Source Serif médio"),
    ("F_LEITOR_SOURCE_G", TTF_SOURCE_SERIF, 17, CORPO, "leitor Source Serif grande"),
    ("F_LEITOR_SERIF_FORTE_P", TTF_SERIF_FORTE, 12, CORPO, "DejaVu Serif forte pequeno"),
    ("F_LEITOR_SERIF_FORTE_M", TTF_SERIF_FORTE, 14, CORPO, "DejaVu Serif forte médio"),
    ("F_LEITOR_SERIF_FORTE_G", TTF_SERIF_FORTE, 17, CORPO, "DejaVu Serif forte grande"),
    ("F_LEITOR_SANS_FORTE_P", TTF_SANS_FORTE, 12, CORPO, "PT Sans forte pequeno"),
    ("F_LEITOR_SANS_FORTE_M", TTF_SANS_FORTE, 14, CORPO, "PT Sans forte médio"),
    ("F_LEITOR_SANS_FORTE_G", TTF_SANS_FORTE, 17, CORPO, "PT Sans forte grande"),
    ("F_LEITOR_MONO_FORTE_P", TTF_MONO_FORTE, 12, CORPO, "FreeMono forte pequeno"),
    ("F_LEITOR_MONO_FORTE_M", TTF_MONO_FORTE, 14, CORPO, "FreeMono forte médio"),
    ("F_LEITOR_MONO_FORTE_G", TTF_MONO_FORTE, 17, CORPO, "FreeMono forte grande"),
    ("F_LEITOR_LITERATA_FORTE_P", TTF_LITERATA, 12, CORPO, "Literata forte pequeno", 700),
    ("F_LEITOR_LITERATA_FORTE_M", TTF_LITERATA, 14, CORPO, "Literata forte médio", 700),
    ("F_LEITOR_LITERATA_FORTE_G", TTF_LITERATA, 17, CORPO, "Literata forte grande", 700),
    ("F_LEITOR_ATKINSON_FORTE_P", TTF_ATKINSON_FORTE, 12, CORPO, "Atkinson forte pequeno"),
    ("F_LEITOR_ATKINSON_FORTE_M", TTF_ATKINSON_FORTE, 14, CORPO, "Atkinson forte médio"),
    ("F_LEITOR_ATKINSON_FORTE_G", TTF_ATKINSON_FORTE, 17, CORPO, "Atkinson forte grande"),
    ("F_LEITOR_INTER_FORTE_P", TTF_INTER_FORTE, 12, CORPO, "Inter forte pequeno"),
    ("F_LEITOR_INTER_FORTE_M", TTF_INTER_FORTE, 14, CORPO, "Inter forte médio"),
    ("F_LEITOR_INTER_FORTE_G", TTF_INTER_FORTE, 17, CORPO, "Inter forte grande"),
    ("F_LEITOR_SOURCE_FORTE_P", TTF_SOURCE_SERIF, 12, CORPO, "Source Serif forte pequeno", 700),
    ("F_LEITOR_SOURCE_FORTE_M", TTF_SOURCE_SERIF, 14, CORPO, "Source Serif forte médio", 700),
    ("F_LEITOR_SOURCE_FORTE_G", TTF_SOURCE_SERIF, 17, CORPO, "Source Serif forte grande", 700),
]


def inclina(img, ascent, pad):
    """Shear pra direita, com o eixo na linha de base."""
    base = pad + ascent
    saida = Image.new("1", img.size, 0)
    px, po = img.load(), saida.load()
    for y in range(img.size[1]):
        dx = round((base - y) * SHEAR)
        for x in range(img.size[0]):
            if px[x, y]:
                nx = x + dx
                if 0 <= nx < img.size[0]:
                    po[nx, y] = 1
    return saida


# As setas do rodapé são DESENHADAS, não rasterizadas da fonte.
def seta_desenhada(ch, altura_x):
    """Triângulo cheio, na altura de x, apontando pro lado do caractere."""
    h = altura_x if altura_x % 2 else altura_x - 1     # ímpar: tem ponta
    if h < 3: h = 3
    w = (h + 1) // 2 + 1

    tinta = [[False] * w for _ in range(h)]
    for linha in range(h):
        # distância da linha até o meio decide a largura daquela fatia
        d = abs(linha - h // 2)
        n = w - d
        for c in range(n):
            if   ch == "◀": tinta[linha][w - 1 - c] = True
            elif ch == "▶": tinta[linha][c] = True
    if ch in "▲▼":
        # As verticais são as horizontais GIRADAS, e não um segundo desenho:
        # girar em grade de pixel é exato, e dois desenhos independentes
        # dariam pontas diferentes lado a lado no mesmo rodapé.
        base = [[False] * w for _ in range(h)]
        for linha in range(h):
            d = abs(linha - h // 2)
            for c in range(w - d):
                base[linha][c] = True            # ▶, ponta à direita
        nh, nw = w, h
        tinta = [[False] * nw for _ in range(nh)]
        for r in range(nh):
            for c in range(nw):
                # ▼ é o ▶ girado 90°; o ▲ é o ▼ espelhado na vertical
                tinta[r][c] = base[h - 1 - c][r if ch == "▼" else nh - 1 - r]
        h, w = nh, nw

    passo = (w + 7) // 8
    bits = bytearray(passo * h)
    for y in range(h):
        for x in range(w):
            if tinta[y][x]: bits[y * passo + x // 8] |= 0x80 >> (x % 8)

    # topo = quanto acima da linha de base ela começa; centrada na altura de x
    topo = altura_x - (altura_x - h) // 2
    return (w, h, 0, topo, w + 2, list(bits))


def rasteriza(caminho_ttf, tamanho, texto, italica=False, peso=None):
    """Devolve (glifos, altura_linha, altura_x, ascent)."""
    face = freetype.Face(caminho_ttf)
    if peso is not None and face.has_multiple_masters:
        info = face.get_variation_info()
        face.set_var_design_coords([
            peso if "Weight" in eixo.name else eixo.default
            for eixo in info.axes])
    face.set_pixel_sizes(0, tamanho)

    # Sem FORCE_AUTOHINT: a FreeType tem hinting manual, e o autohinter
    # SUBSTITUI o do desenhista por um genérico — que é justamente o que
    # produz haste de larguras diferentes na mesma palavra.
    flags = freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_MONO

    ascent  = face.size.ascender >> 6
    descent = -(face.size.descender >> 6)
    glifos  = []

    for ch in sorted(set(texto), key=ord):
        face.load_char(ch, flags)
        g  = face.glyph
        bm = g.bitmap
        avanco = g.advance.x >> 6

        if ch == " ":
            avanco += 2
        avanco += TRACKING

        if bm.rows == 0 or bm.width == 0:      # espaço e afins
            if ch != " " and unicodedata.category(ch) not in ("Zs", "Cf"):
                sys.exit(f"ERRO: '{ch}' (U+{ord(ch):04X}) não existe em "
                         f"{caminho_ttf} — e glifo faltante já mordeu "
                         f"três vezes neste projeto.")
            glifos.append((ord(ch), 0, 0, 0, 0, avanco, []))
            continue

        # O FreeType já entrega empacotado, mas com `pitch` próprio; aqui
        # reempacota no passo que o firmware espera.
        passo = (bm.width + 7) // 8
        bits = bytearray(passo * bm.rows)
        for y in range(bm.rows):
            for x in range(bm.width):
                if bm.buffer[y * bm.pitch + (x >> 3)] & (0x80 >> (x & 7)):
                    bits[y * passo + x // 8] |= 0x80 >> (x % 8)

        if italica:
            bits, bm_w = _inclina(bits, bm.width, bm.rows, g.bitmap_top)
        else:
            bm_w = bm.width

        if ch in SETAS:
            face.load_char("x", flags)
            ax = face.glyph.bitmap.rows
            face.load_char(ch, flags)
            w2, h2, esq2, topo2, av2, b2 = seta_desenhada(ch, ax)
            glifos.append((ord(ch), w2, h2, esq2, topo2, av2, b2))
        else:
            glifos.append((ord(ch), bm_w, bm.rows, g.bitmap_left, g.bitmap_top,
                           avanco, list(bits)))

    altura_linha = ascent + descent + ENTRELINHA

    # altura de x medida na TINTA do "x" — é ela que decide se a fonte
    # "parece" grande, não o tamanho nominal
    face.load_char("x", flags)
    altura_x = face.glyph.bitmap.rows

    return glifos, altura_linha, altura_x, ascent


def _inclina(bits, w, h, topo):
    """Shear pra direita, com o eixo na linha de base."""
    novo_w = w + int(h * SHEAR) + 1
    passo_n = (novo_w + 7) // 8
    passo_v = (w + 7) // 8
    saida = bytearray(passo_n * h)
    for y in range(h):
        dx = round((topo - y) * SHEAR)
        for x in range(w):
            if bits[y * passo_v + (x >> 3)] & (0x80 >> (x & 7)):
                nx = x + dx
                if 0 <= nx < novo_w:
                    saida[y * passo_n + nx // 8] |= 0x80 >> (nx % 8)
    return saida, novo_w


def gera():
    out = []
    w = out.append
    w("// GERADO POR firmware/ferramentas/fontes.py — não editar à mão.")
    w("//")
    w("// As caixas medem a TINTA (não o contorno vetorial) e o avanço inclui")
    w("// os side bearings, então somar avanços preserva o tracking.")
    w('#include "fontes.h"')
    w("")

    resumo = []
    for item in FONTES:
        nome, ttf, tam, chars, comentario, *opcoes = item
        peso = opcoes[0] if opcoes else None
        glifos, altura_linha, altura_x, ascent = rasteriza(
            ttf, tam, chars, italica=(nome in ITALICAS), peso=peso)

        # bitmaps num blob só, com índice — evita 400 arrays soltos
        blob, indices = [], []
        for (_c, _l, _a, _e, _t, _av, bits) in glifos:
            indices.append(len(blob))
            blob.extend(bits)

        curto = nome.lower()
        w(f"// {nome} — {tam} pt · {comentario}")
        w(f"//   altura de linha {altura_linha} px · altura de x {altura_x} px "
          f"· {len(glifos)} glifos · {len(blob)} bytes")
        w(f"static const uint8_t {curto}_bits[] = {{")
        for i in range(0, len(blob), 16):
            w("    " + ", ".join(f"0x{b:02X}" for b in blob[i:i+16]) + ",")
        w("};")
        w(f"static const glifo_t {curto}_glifos[] = {{")
        for (c, l, a, e, t, av, _b), idx in zip(glifos, indices):
            ch = chr(c)
            nome_ch = ch if 33 <= c <= 126 and ch not in '\\"' else f"U+{c:04X}"
            w(f"    {{ {c:5d}, {l:2d}, {a:2d}, {e:3d}, {t:3d}, {av:2d}, "
              f"{idx:5d} }},  // {nome_ch}")
        w("};")
        w("")
        resumo.append((nome, curto, len(glifos), altura_linha, altura_x, ascent, tam))

    w("const fonte_dados_t FONTES[] = {")
    for nome, curto, n, altura_linha, altura_x, ascent, tam in resumo:
        w(f"    [{nome}] = {{ {curto}_glifos, {curto}_bits, {n}, "
          f"{altura_linha}, {altura_x}, {ascent} }},")
    w("};")
    w("")

    texto = "\n".join(out) + "\n"
    with open(SAIDA, "w") as f:
        f.write(texto)

    print(f"  {SAIDA}")
    for nome, _c, n, al, ax, _asc, tam in resumo:
        print(f"    {nome:10s} {tam:2d} pt · linha {al:2d} px · "
              f"altura de x {ax} px · {n} glifos")
    print(f"    {len(texto):,} bytes de C")


if __name__ == "__main__":
    gera()
