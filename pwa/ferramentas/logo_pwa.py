#!/usr/bin/env python3
"""Gera a marca do Tinto App a partir do asset oficial."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "..", "firmware", "ferramentas"))
from logo_tinto import FONTE, LIMIAR, SHA_ESPERADO, bbox_da_marca, sha_da_fonte

try:
    from PIL import Image
except ImportError:      # pragma: no cover
    if "--check" in sys.argv:
        print("  logo do app: Pillow ausente, procedência conferida no C")
        raise SystemExit(0)
    sys.exit("falta Pillow — pip install Pillow")

RAIZ  = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
SAIDA = os.path.join(RAIZ, "pwa", "logo.png")

# 128 px de altura: o maior uso é a capa da entrada, a 64 px de corpo, e o
# dobro disso cobre a tela retina sem virar um arquivo que pesa no primeiro
# carregamento. A largura sai da marca, e não de um número escolhido aqui.
ALTURA = 128


def gera() -> bytes:
    sha = sha_da_fonte()
    if sha != SHA_ESPERADO:
        sys.exit(f"logo não confere: {sha}\n           esperado: {SHA_ESPERADO}")

    im = Image.open(FONTE).convert("L")
    tinta = im.point(lambda p: 0 if p <= LIMIAR else 255, mode="L")

    caixa = bbox_da_marca(tinta)
    if not caixa:
        sys.exit("a logo saiu inteira branca no limiar — confira o asset")
    tinta = tinta.crop(caixa)

    larg = max(1, round(tinta.width * ALTURA / tinta.height))
    tinta = tinta.resize((larg, ALTURA), Image.LANCZOS)

    # Preto onde há tinta, transparente onde não há. O alfa é a máscara: o
    # meio-tom do LANCZOS vira meia opacidade, e é ele que segura a curva
    # do "o" lisa num tamanho pequeno.
    marca = Image.new("RGBA", tinta.size, (0, 0, 0, 0))
    marca.putalpha(tinta.point(lambda p: 255 - p))

    import io
    saco = io.BytesIO()
    marca.save(saco, "PNG", optimize=True)
    return saco.getvalue()


if __name__ == "__main__":
    novo = gera()
    if "--check" in sys.argv:
        try:
            with open(SAIDA, "rb") as f:
                velho = f.read()
        except FileNotFoundError:
            sys.exit("pwa/logo.png não existe — rode a ferramenta sem --check")
        if velho != novo:
            sys.exit("pwa/logo.png está desatualizado — rode a ferramenta")
        print("  logo do app: confere com o asset oficial")
    else:
        with open(SAIDA, "wb") as f:
            f.write(novo)
        from PIL import Image as I
        l, a = I.open(SAIDA).size
        print(f"  pwa/logo.png · {l}×{a} · {len(novo)} bytes")
