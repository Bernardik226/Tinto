#!/usr/bin/env python3
"""O ORÇAMENTO de RAM interna, conferido a cada build."""
import os
import re
import subprocess
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.join(AQUI, "..", "..")   # a raiz do projeto
ELF  = os.path.join(RAIZ, "firmware", "build", "tinto.elf")
if not os.path.exists(ELF):
    import glob
    achados = glob.glob(os.path.join(RAIZ, "firmware", "build", "*.elf"))
    if achados:
        ELF = achados[0]

# 128 KB. Hoje o `.bss` está em 107, e a folga é o que permite uma tela
# nova sem obrigar ninguém a otimizar no meio de outra coisa. Passou
# disso, é decisão — e decisão se toma com o número na frente.
TETO_BSS = 128 * 1024


def main():
    if not os.path.exists(ELF):
        sys.exit("compile o firmware antes: cd firmware && idf.py build")

    ferramenta = "xtensa-esp32s3-elf-size"
    try:
        saida = subprocess.check_output([ferramenta, "-A", ELF], text=True)
    except (FileNotFoundError, subprocess.CalledProcessError):
        sys.exit("falta %s no PATH — rode `. ~/esp/esp-idf/export.sh`" % ferramenta)

    # `.dram0.bss` e `.iram0.bss` — a RAM INTERNA. A `.ext_ram.bss` fica
    # de fora de propósito: ela é PSRAM, e são 8 MB. O teto existe para a
    # memória escassa, não para a que sobra.
    bss = sum(int(m.group(1))
              for m in re.finditer(r"^\.(?:dram0|iram0)\.bss\s+(\d+)",
                                   saida, re.M))

    print("  .bss  %6d bytes  ·  teto %d  ·  %s"
          % (bss, TETO_BSS, "ok" if bss <= TETO_BSS else "ESTOUROU"))

    if bss > TETO_BSS:
        sys.exit(
            "\nO .bss passou do teto de RAM interna.\n"
            "Ela e a unica que o DMA alcanca, e dela vivem o cartao, o\n"
            "Wi-Fi e as pilhas: faltando, quebra quem nao gastou.\n\n"
            "Antes de subir o teto, pergunte se o buffer novo precisa\n"
            "MESMO ser interno. Quase nunca precisa - so DMA, ISR e\n"
            "pilha. O resto vai para a PSRAM por `mem_emprestada`.\n")


if __name__ == "__main__":
    main()
