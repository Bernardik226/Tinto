#!/usr/bin/env python3
"""Gera os ícones do Tinto App, a partir do pingo do 'i' da marca."""
import hashlib
import os
import sys

try:
    from PIL import Image, ImageDraw
except ImportError:      # pragma: no cover
    if "--check" in sys.argv:
        print("  ícone: Pillow ausente, geração não conferida")
        raise SystemExit(0)
    sys.exit("falta Pillow — backend/.venv/bin/pip install Pillow")

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.join(AQUI, "..", "..")
SAIDA = os.path.join(RAIZ, "pwa")

# ── as proporções, medidas no asset oficial ──────────────────────────
# Frações do LADO do quadrado, e não pixels: o mesmo desenho serve a 180 e
# a 512 sem uma tabela de tamanhos que se desatualiza no primeiro tamanho
# novo. Elas somam 1,0 — borda, folga, ponto, folga, borda.
BORDA = 15 / 127        # 0,118 — a espessura da moldura
FOLGA = 13 / 127        # 0,102 — o ar entre a moldura e o ponto
PONTO = 71 / 127        # 0,559 — o diâmetro do ponto

# ── as cores ─────────────────────────────────────────────────────────
# O papel de um e-ink iluminado, e a tinta que ele consegue mesmo: preto de
# e-ink é ~#1a1a1a, nunca `#000`. Usar preto puro aqui faria o ícone ser a
# única coisa do sistema mais escura que o próprio aparelho.
PAPEL = (0xEC, 0xE8, 0xDF)
TINTA = (0x1A, 0x1A, 0x1A)

# Quanto do lado da arte o pingo ocupa, por formato.
OCUPACAO = {"any": 0.68, "maskable": 0.56}

# Desenha grande e reduz. Um círculo de 71 px traçado direto sai com a
# borda serrilhada, e serrilhado é o que se enxerga primeiro num ícone.
SUPER = 8

ARQUIVOS = [
    ("icone-192.png", 192, "any"),
    ("icone-512.png", 512, "any"),
    ("icone-maskable-512.png", 512, "maskable"),
    # O iOS ignora o manifesto e lê esta tag. Ele também NÃO recorta nem
    # arredonda o que vem por ela — o cantinho quadrado é por conta de
    # quem gera, e por isso este usa a arte do `any`, com margem própria.
    ("apple-touch-icon.png", 180, "any"),
]


def desenha(lado: int, formato: str) -> "Image.Image":
    """O pingo do 'i', num quadrado de `lado` pixels."""
    s = lado * SUPER
    im = Image.new("RGB", (s, s), PAPEL)
    d = ImageDraw.Draw(im)

    # O pingo, centrado. `round` no fim e não no meio: arredondar cada
    # medida separado faz a borda de cima sair um pixel mais grossa que a
    # de baixo, e num quadrado isso se vê.
    caixa = s * OCUPACAO[formato]
    x0 = (s - caixa) / 2
    y0 = (s - caixa) / 2

    # A moldura: um quadrado cheio com outro vazado por dentro.
    b = caixa * BORDA
    d.rectangle([round(x0), round(y0), round(x0 + caixa) - 1,
                 round(y0 + caixa) - 1], fill=TINTA)
    d.rectangle([round(x0 + b), round(y0 + b), round(x0 + caixa - b) - 1,
                 round(y0 + caixa - b) - 1], fill=PAPEL)

    # O ponto. Ele é o que sobrevive ao ícone de 48 px na lista de
    # aplicativos — a moldura vira uma borda, e o ponto continua um ponto.
    p = caixa * PONTO
    px0 = x0 + caixa * (BORDA + FOLGA)
    d.ellipse([round(px0), round(y0 + caixa * (BORDA + FOLGA)),
               round(px0 + p) - 1,
               round(y0 + caixa * (BORDA + FOLGA) + p) - 1], fill=TINTA)

    return im.resize((lado, lado), Image.LANCZOS)


def bytes_do_png(im: "Image.Image") -> bytes:
    import io
    buf = io.BytesIO()
    # `optimize` porque estes arquivos vão versionados, e um PNG que muda
    # de tamanho sem o desenho mudar é um diff que não diz nada.
    im.save(buf, "PNG", optimize=True)
    return buf.getvalue()


def main() -> int:
    checar = "--check" in sys.argv
    divergiu = False

    for nome, lado, formato in ARQUIVOS:
        novo = bytes_do_png(desenha(lado, formato))
        caminho = os.path.join(SAIDA, nome)

        if checar:
            try:
                with open(caminho, "rb") as f:
                    velho = f.read()
            except OSError:
                print(f"  ícone: {nome} não existe")
                divergiu = True
                continue
            if hashlib.sha256(velho).digest() != hashlib.sha256(novo).digest():
                print(f"  ícone: {nome} difere do que este arquivo gera")
                divergiu = True
            continue

        with open(caminho, "wb") as f:
            f.write(novo)
        print(f"  {nome}  {lado}×{lado}  {formato}  {len(novo)} bytes")

    if checar and not divergiu:
        print("  ícone: os quatro conferem")
    return 1 if divergiu else 0


if __name__ == "__main__":
    raise SystemExit(main())
