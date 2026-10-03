#!/usr/bin/env python3
"""Tinto — a prova em PNG.

    make provas   →  docs/provas/*.png

Renderiza cada cena no tamanho exato do painel, com o código REAL de
desenho: compara com os desenhos sem gravar a placa. As cenas moram em
cenas.py, as mesmas da janela.
"""
import os
import struct
import sys
import zlib

from cenas import CENAS, carrega, quadro

SAIDA = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     "..", "..", "docs", "provas")


def png(caminho, bits, l, a, escala=1):
    """PNG sem Pillow — zlib é biblioteca padrão."""
    passo = (l + 7) // 8
    linhas = bytearray()
    for y in range(a):
        linha = bytearray()
        for x in range(l):
            aceso = (bits[y * passo + (x >> 3)] >> (7 - (x & 7))) & 1
            linha += bytes([0 if aceso else 255] * 3) * escala
        for _ in range(escala):
            linhas += b"\x00" + linha

    def pedaco(tipo, dados):
        c = tipo + dados
        return struct.pack(">I", len(dados)) + c + struct.pack(">I", zlib.crc32(c))

    dados = (b"\x89PNG\r\n\x1a\n"
             + pedaco(b"IHDR", struct.pack(">IIBBBBB", l * escala, a * escala,
                                           8, 2, 0, 0, 0))
             + pedaco(b"IDAT", zlib.compress(bytes(linhas), 9))
             + pedaco(b"IEND", b""))
    with open(caminho, "wb") as f:
        f.write(dados)
    return len(dados)


def main():
    t = carrega()
    os.makedirs(SAIDA, exist_ok=True)
    escala = int(sys.argv[1]) if len(sys.argv) > 1 else 1

    print()
    for nome, _rotulo, monta in CENAS:
        monta(t)
        bits, l, a = quadro(t)
        n = png(os.path.join(SAIDA, f"{nome}.png"), bits, l, a, escala)
        tinta = sum(bin(b).count("1") for b in bits)
        print(f"  {nome:22s} {l}×{a}  ·  {tinta:6,} px de tinta  ·  {n:,} B")

    print(f"\n  {len(CENAS)} provas em docs/provas/\n")


if __name__ == "__main__":
    main()
