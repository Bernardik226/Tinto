#!/usr/bin/env python3
"""O orçamento da PILHA, e por que ele existe."""
import re
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[2]   # a raiz do projeto
OBJ = RAIZ / "build/pc/obj"

# 12 KB, contra os 48 KB de pilha da task APP (`main.c`).
TETO = 12288

# `dynamic` e `bounded` são VLA e alloca: o GCC não sabe o tamanho. Não há
# nenhum no firmware, e se aparecer um, ele é reportado.
LINHA = re.compile(r"^(.+?):(\d+):(\d+):(.+?)\t(\d+)\t(\w+)$")


def main() -> int:
    arquivos = sorted(OBJ.rglob("*.su"))
    if not arquivos:
        print("  ✗ nenhum .su — compile com -fstack-usage antes")
        return 1

    gordas = []
    for su in arquivos:
        for linha in su.read_text().splitlines():
            m = LINHA.match(linha)
            if not m:
                continue
            arquivo, _, _, funcao, bytes_, tipo = m.groups()
            # Só o código do aparelho: teste e simulador podem gastar
            # pilha à vontade, que a máquina deles tem de sobra.
            if "firmware/main" not in arquivo:
                continue
            n = int(bytes_)
            if n > TETO or tipo != "static":
                gordas.append((n, funcao.strip(), Path(arquivo).name, tipo))

    if gordas:
        gordas.sort(reverse=True)
        print(f"  ✗ quadro de pilha acima de {TETO} bytes:")
        for n, funcao, arquivo, tipo in gordas[:12]:
            extra = "" if tipo == "static" else f"  ({tipo})"
            print(f"      {n:6d} B  {arquivo}  {funcao}{extra}")
        print("      → mova a struct grande para `static` (vai pra .bss,")
        print("        que o `make ram` mede) ou reduza o teto dela.")
        return 1

    pior = 0
    nome = ""
    for su in arquivos:
        for linha in su.read_text().splitlines():
            m = LINHA.match(linha)
            if m and "firmware/main" in m.group(1):
                if int(m.group(5)) > pior:
                    pior = int(m.group(5))
                    nome = m.group(4).strip()
    print(f"  pilha  maior quadro {pior} bytes  ·  teto {TETO}  ·  ok")
    if pior:
        print(f"         ({nome})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
