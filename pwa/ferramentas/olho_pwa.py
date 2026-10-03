#!/usr/bin/env python3
"""Tira uma foto do PWA, no tamanho de um celular ou de um monitor."""
import base64
import json
import subprocess
import sys
import time
import urllib.request

PORTA = 4444


def rpc(metodo, caminho, corpo=None):
    dados = json.dumps(corpo).encode() if corpo is not None else None
    req = urllib.request.Request(f"http://127.0.0.1:{PORTA}{caminho}",
                                 data=dados, method=metodo,
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=90) as r:
        return json.loads(r.read())


def foto(url, largura, altura, saida):
    gecko = subprocess.Popen(["geckodriver", "--port", str(PORTA)],
                             stdout=subprocess.DEVNULL,
                             stderr=subprocess.DEVNULL)
    try:
        for _ in range(40):
            try:
                urllib.request.urlopen(f"http://127.0.0.1:{PORTA}/status",
                                       timeout=2)
                break
            except Exception:
                time.sleep(0.5)

        s = rpc("POST", "/session", {"capabilities": {"alwaysMatch": {
            "moz:firefoxOptions": {"args": ["-headless"]}}}})["value"]["sessionId"]

        rpc("POST", f"/session/{s}/window/rect",
            {"width": largura, "height": altura, "x": 0, "y": 0})
        rpc("POST", f"/session/{s}/url", {"url": url})
        time.sleep(1.2)      # a fonte carrega, o papel do fundo assenta

        png = rpc("GET", f"/session/{s}/screenshot")["value"]
        with open(saida, "wb") as f:
            f.write(base64.b64decode(png))
        rpc("DELETE", f"/session/{s}")
        return saida
    finally:
        gecko.terminate()


if __name__ == "__main__":
    if len(sys.argv) != 5:
        sys.exit(__doc__)
    print(foto(sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]))
