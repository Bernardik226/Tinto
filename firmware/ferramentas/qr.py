#!/usr/bin/env python3
"""Gera o QR que leva ao aplicativo, como bitmap embutido no firmware."""
import os
import re
import sys

try:
    import segno
except ImportError:
    sys.exit("falta segno:  backend/.venv/bin/pip install segno")

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.join(AQUI, "..", "..")
SAIDA = os.path.join(RAIZ, "firmware", "main", "tela", "qr.c")
SAIDA_H = os.path.join(RAIZ, "firmware", "main", "tela", "qr.h")

# ── o endereço vem de `firmware/main/servidor.h` ─────────────────────
# Uma fonte só, e ela NÃO está no repositório: o servidor é de quem monta
# o aparelho. Este arquivo lia um endereço fixo, e o `qr.c` gerado é esse
# endereço em bitmap — publicar o `.c` era publicar o servidor de quem
# publicou, codificado.
SERVIDOR_H = os.path.join(RAIZ, "firmware", "main", "servidor.h")


def endereco():
    """A URL do aplicativo: o servidor configurado, mais `/e`."""
    if not os.path.exists(SERVIDOR_H):
        sys.exit(
            "falta firmware/main/servidor.h — o endereço do SEU servidor.\n"
            "  cp firmware/main/servidor.exemplo.h firmware/main/servidor.h\n"
            "  e preencha o endereço antes de gerar o QR.")

    with open(SERVIDOR_H, encoding="utf-8") as f:
        achado = re.search(r'#define\s+TINTO_SERVIDOR\s+"([^"]*)"', f.read())

    if not achado or not achado.group(1):
        sys.exit("servidor.h existe e não diz endereço nenhum.")

    url = achado.group(1).rstrip("/")
    if "mude-isto" in url:
        sys.exit("servidor.h ainda está com o endereço do exemplo.")
    return url + "/e"

# Correção de erro M (~15%). L leria mais rápido e perdoaria menos sujeira;
# H perdoaria muito e cresceria o bloco. Num vidro que não risca e não
# amassa, M é o meio certo.
NIVEL = "m"

# Quantos pixels da tela por módulo do QR.
ESCALA = 4

# A zona quieta. Quatro módulos é o que a norma pede; menos que isso e o
# leitor não acha a borda do símbolo contra o fundo da tela.
QUIETA = 4


def gera():
    ENDERECO = endereco()
    qr = segno.make(ENDERECO, error=NIVEL)
    matriz = [list(linha) for linha in qr.matrix]
    n = len(matriz)

    lado = (n + 2 * QUIETA) * ESCALA
    if lado > 240:
        sys.exit(f"o QR ficou com {lado}px e a tela tem 240. "
                 f"Encurte o endereço ou baixe a escala.")

    # Uma linha de bits por linha de PIXEL, empacotada em bytes — o mesmo
    # formato dos ícones, para o `tela_` desenhar sem caso especial.
    bytes_por_linha = (lado + 7) // 8
    linhas = []
    for y in range(lado):
        bits = bytearray(bytes_por_linha)
        my = y // ESCALA - QUIETA
        for x in range(lado):
            mx = x // ESCALA - QUIETA
            preto = (0 <= my < n and 0 <= mx < n and matriz[my][mx])
            if preto:
                bits[x // 8] |= 0x80 >> (x % 8)
        linhas.append(bits)

    corpo = ",\n    ".join(
        ", ".join(f"0x{b:02x}" for b in linha) for linha in linhas)

    with open(SAIDA, "w", encoding="utf-8") as f:
        f.write(f'''// GERADO por firmware/ferramentas/qr.py — não editar à mão.
//
// O QR do aplicativo, em bitmap de 1 bit. Ele é CONSTANTE porque o
// endereço não muda: o firmware não codifica QR nenhum em tempo de
// execução, e não redesenha o bloco a cada código de pareamento.
//
//   endereço: {ENDERECO}
//   correção: {NIVEL.upper()} · {n}x{n} módulos · {ESCALA}px por módulo
#include "qr.h"

const uint8_t QR_BITS[QR_LADO * QR_BYTES_LINHA] = {{
    {corpo}
}};
''')

    with open(SAIDA_H, "w", encoding="utf-8") as f:
        f.write(f'''// GERADO por firmware/ferramentas/qr.py — não editar à mão.
#ifndef TELA_QR_H
#define TELA_QR_H

#include <stdint.h>

#define QR_LADO        {lado}
#define QR_BYTES_LINHA {bytes_por_linha}
#define QR_ESCALA      {ESCALA}
#define QR_MODULOS     {n + 2 * QUIETA}

// Um bit por pixel, 1 = preto. Mesma forma dos ícones.
extern const uint8_t QR_BITS[QR_LADO * QR_BYTES_LINHA];

#endif
''')

    print(f"{n}x{n} módulos · {lado}x{lado} px · "
          f"{lado * bytes_por_linha} bytes  →  tela/qr.c")


if __name__ == "__main__":
    gera()
