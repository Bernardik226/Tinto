#!/usr/bin/env python3
"""Gera o bitmap 1-bit da logo oficial do Tinto."""
import hashlib
import os
import sys

try:
    from PIL import Image
except ImportError:      # pragma: no cover
    # O `--check` roda dentro de `make test`, e o shell de quem vai gravar a
    # placa é o do ESP-IDF, cujo Python não tem Pillow. Sem ele não dá para
    # reproduzir o bitmap — mas o teste em C continua conferindo o SHA da
    # fonte, que é o que impede trocar a marca sem regerar.
    if "--check" in sys.argv:
        print("  logo: Pillow ausente, procedência conferida pelo teste em C")
        raise SystemExit(0)
    sys.exit("falta Pillow — pip install Pillow (só pra gerar; o .c vai versionado)")

AQUI   = os.path.dirname(os.path.abspath(__file__))
RAIZ   = os.path.join(AQUI, "..", "..")
FONTE  = os.path.join(RAIZ, "docs", "logos", "Tinto_Logo_New.jpeg")
SAIDA_C = os.path.join(RAIZ, "firmware", "main", "tela", "logo_tinto.c")
SAIDA_H = os.path.join(RAIZ, "firmware", "main", "tela", "logo_tinto.h")

# O asset oficial, e nenhum outro.
SHA_ESPERADO = "7400f8129cd26e55c63e4b1ab469af4fbccb35ce696012d44973057d503939f4"

# A caixa do desenho: 240 de tela, menos 11 de margem do corpo, menos 3 do
# bloco e 7 do respiro interno, dos dois lados.
CAIXA_L = 198
CAIXA_A = 150
LIMIAR  = 160


def sha_da_fonte():
    with open(FONTE, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


# Uma linha ou coluna precisa de pelo menos este tanto de tinta para ser
# considerada parte da marca. Meio por cento da dimensão: sobra folga para
# a haste mais fina do "t" e não sobra para pixel solto de JPEG.
PISO_RUIDO = 0.005


def bbox_da_marca(img):
    l, a = img.size
    px = img.load()
    colunas = [0] * l
    linhas  = [0] * a
    for y in range(a):
        for x in range(l):
            if px[x, y] == 0:
                colunas[x] += 1
                linhas[y]  += 1

    piso_col = max(2, int(a * PISO_RUIDO))
    piso_lin = max(2, int(l * PISO_RUIDO))
    xs = corpo([x for x, n in enumerate(colunas) if n >= piso_col],
               colunas, int(l * VAO_MAXIMO))
    ys = corpo([y for y, n in enumerate(linhas)  if n >= piso_lin],
               linhas,  int(a * VAO_MAXIMO))
    if not xs or not ys:
        return None
    return (xs[0], ys[0], xs[-1] + 1, ys[-1] + 1)


# O maior vão que ainda é ESPAÇO ENTRE LETRAS. Acima dele, o que vem
# depois não é mais a marca: é outra coisa no mesmo arquivo.
VAO_MAXIMO = 0.05


def corpo(indices, tinta, vao):
    """O bloco de índices que carrega a marca, sem a poeira do export."""
    if not indices:
        return indices

    blocos = [[indices[0]]]
    for i in indices[1:]:
        if i - blocos[-1][-1] > vao:
            blocos.append([])
        blocos[-1].append(i)

    return max(blocos, key=lambda b: sum(tinta[i] for i in b))


def gera():
    sha = sha_da_fonte()
    if sha != SHA_ESPERADO:
        sys.exit(f"logo não confere: {sha}\n"
                 f"           esperado: {SHA_ESPERADO}\n"
                 f"Trocar a marca exige atualizar SHA_ESPERADO.")

    im = Image.open(FONTE)
    # Composição sobre branco. O asset é opaco, então isto é identidade —
    # o passo fica para que um asset com alfa real volte a funcionar sem
    # reescrever a ferramenta.
    if im.mode in ("RGBA", "LA"):
        fundo = Image.new("RGB", im.size, (255, 255, 255))
        fundo.paste(im, mask=im.split()[-1])
        im = fundo
    im = im.convert("L")

    # 1 bit ANTES de escalar: é o que apaga o quadriculado do export.
    tinta = im.point(lambda p: 0 if p <= LIMIAR else 255, mode="L")

    # O bounding box da MARCA, e não do ruído. `getbbox()` sozinho pega os
    # artefatos de compressão espalhados pelo quadriculado — com ele a
    # marca vinha com 23 linhas brancas no topo, encolhida no meio de uma
    # margem que não é dela. A projeção com piso descarta a poeira: uma
    # linha só conta se tiver tinta de verdade.
    caixa = bbox_da_marca(tinta)
    if not caixa:
        sys.exit("a logo saiu inteira branca no limiar — confira o asset")
    tinta = tinta.crop(caixa)

    tinta.thumbnail((CAIXA_L, CAIXA_A), Image.LANCZOS)
    tinta = tinta.point(lambda p: 0 if p <= LIMIAR else 255, mode="1")

    l, a = tinta.size
    px = tinta.load()
    passo = (l + 7) // 8
    bits = bytearray(passo * a)
    for y in range(a):
        for x in range(l):
            if px[x, y] == 0:                    # preto = tinta
                bits[y * passo + (x >> 3)] |= 0x80 >> (x & 7)

    h = f"""// GERADO por firmware/ferramentas/logo_tinto.py — não edite à mão.
//
// A marca reduzida ao painel, direto do asset oficial. O SHA da fonte vai
// junto para que trocar o arquivo sem regerar falhe alto.
#ifndef TELA_LOGO_TINTO_H
#define TELA_LOGO_TINTO_H

#include <stdint.h>

#define LOGO_TINTO_L {l}
#define LOGO_TINTO_A {a}

extern const uint8_t LOGO_TINTO_BITS[];
extern const char    LOGO_TINTO_FONTE_SHA256[];

#endif
"""

    linhas = []
    for i in range(0, len(bits), 12):
        linhas.append("    " + " ".join(f"0x{b:02x}," for b in bits[i:i + 12]))
    c = ("// GERADO por firmware/ferramentas/logo_tinto.py — não edite à mão.\n"
         '#include "logo_tinto.h"\n\n'
         "const uint8_t LOGO_TINTO_BITS[] = {\n"
         + "\n".join(linhas)
         + "\n};\n\nconst char LOGO_TINTO_FONTE_SHA256[] =\n"
         + f'    "{sha}";\n')
    return h, c, l, a, sum(bin(b).count("1") for b in bits)


def main():
    conferir = "--check" in sys.argv
    h, c, l, a, tinta = gera()
    for caminho, novo in ((SAIDA_H, h), (SAIDA_C, c)):
        if conferir:
            try:
                with open(caminho, encoding="utf-8") as f:
                    atual = f.read()
            except FileNotFoundError:
                sys.exit(f"falta {os.path.relpath(caminho, RAIZ)} — rode `make logos`")
            if atual != novo:
                sys.exit(f"{os.path.relpath(caminho, RAIZ)} está fora de data — "
                         f"rode `make logos`")
        else:
            with open(caminho, "w", encoding="utf-8") as f:
                f.write(novo)
    if not conferir:
        print(f"  LOGO_TINTO  {l}×{a}  ·  {tinta:,} px de tinta")


if __name__ == "__main__":
    main()
