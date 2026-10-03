#!/usr/bin/env python3
"""Tinto — o aparelho, navegável no navegador.

Carrega o firmware de verdade (libtinto.so) com ctypes e serve uma página
que pinta o bitmap num canvas. Não é mímica: as funções são as mesmas que
os testes chamam.

    make janela      →  http://localhost:8080
"""
import ctypes
import http.server
import json
import socketserver
import sys
import webbrowser

from cenas import CENAS, carrega, _T

NOME_DA_TELA = {i: n[len("TELA_"):] for n, i in _T.items()}

t = carrega()
cena_atual = 0
CENAS[cena_atual][2](t)      # a janela abre num dia de verdade, não vazia


def quadro():
    l, a = ctypes.c_int(0), ctypes.c_int(0)
    p = t.tinto_tela(ctypes.byref(l), ctypes.byref(a))
    n = ((l.value + 7) // 8) * a.value
    return {
        "cena": cena_atual,
        "l": l.value, "a": a.value,
        "bits": bytes(p[:n]).hex(),
        "cursor": t.tinto_cursor(),
        "nivel": t.tinto_profundidade(),
        "eventos": t.tinto_eventos(),
        "docado": t.tinto_docado(),
        "tela":   NOME_DA_TELA.get(t.tinto_tela_id(), "?"),
    }


PAGINA = """<!doctype html><meta charset=utf-8>
<title>Tinto — aparelho</title>
<style>
 :root{ --papel:#dcd9d0; --tinta:#16150f; --fundo:#131614; --fraco:#8b9390; }
 html,body{ height:100%; overflow:hidden; }
 body{ background:var(--fundo); color:#dce1de; font:14px/1.5 ui-monospace,monospace;
       margin:0; display:flex; gap:34px; align-items:center;
       justify-content:center; padding:20px; box-sizing:border-box; }
 canvas{ image-rendering:pixelated; border:1px solid rgba(255,255,255,.35);
         background:var(--papel); }
 .col{ display:flex; flex-direction:column; gap:12px; }
 h1{ font-size:13px; letter-spacing:.2em; text-transform:uppercase;
     color:var(--fraco); margin:0; font-weight:400; }
 table{ border-collapse:collapse; font-size:12.5px; }
 td{ padding:3px 14px 3px 0; }
 td:first-child{ color:var(--fraco); }
 kbd{ background:#222725; border:1px solid #3a423e; border-bottom-width:2px;
      border-radius:3px; padding:1px 6px; font:inherit; font-size:11px; }
 .est{ color:var(--fraco); font-size:12px; }
 .est b{ color:#dce1de; font-weight:700; }
 button{ background:transparent; color:#dce1de; border:1px solid #3a423e;
         padding:5px 11px; font:inherit; font-size:11.5px; cursor:pointer; }
 button:hover{ border-color:#6a746f; }
 button.on{ background:#dce1de; color:#131614; border-color:#dce1de; }
</style>
<div class=col>
  <h1>Tinto · 240×416</h1>
  <canvas id=t width=240 height=416></canvas>
  <div class=est id=est></div>
</div>
<div class=col>
  <h1>Cenário</h1>
  <div id=cenas style="display:flex;flex-direction:column;gap:4px"></div>
  <h1 style="margin-top:14px">Controles</h1>
  <table>
   <tr><td><kbd>↑</kbd> <kbd>↓</kbd> <kbd>←</kbd> <kbd>→</kbd></td><td>5-vias</td></tr>
   <tr><td><kbd>Enter</kbd></td><td>OK — a ação da linha</td></tr>
   <tr><td><kbd>Backspace</kbd></td><td>BACK — volta um nível</td></tr>
   <tr><td><kbd>Shift</kbd>+<kbd>Backspace</kbd></td><td>BACK segurado — volta pra home</td></tr>
   <tr><td><kbd>Tab</kbd></td><td>MENU — a gaveta</td></tr>
   <tr><td><kbd>Espaço</kbd></td><td>voz</td></tr>
   <tr><td><kbd>0</kbd></td><td>power</td></tr>
   <tr><td><kbd>D</kbd></td><td>docar / tirar da dock</td></tr>
   <tr><td><kbd>T</kbd></td><td>passar 1 minuto</td></tr>
   <tr><td><kbd>P</kbd></td><td>salvar PNG</td></tr>
  </table>
  <div style="display:flex;gap:8px;margin-top:6px">
    <button onclick="baixa()">salvar PNG</button>
  </div>
  <div class=est style="max-width:32ch;line-height:1.6;margin-top:10px">
    Este é o firmware de verdade rodando por <b>ctypes</b>. As mesmas funções
    que os testes chamam.
  </div>
</div>
<script>
const cv = document.getElementById('t'), cx = cv.getContext('2d');
const PAPEL = [220,217,208], TINTA = [22,21,15];

function escala(l, a){
  // passo inteiro: 2x, 1x… fracionário borraria o pixel de 1 bit
  const cabe = Math.min((innerHeight - 130) / a, (innerWidth * 0.55) / l);
  return Math.max(1, Math.floor(cabe));
}

let ultimo = null;

let pinta = function(q){
  ultimo = q;
  if (cv.width !== q.l || cv.height !== q.a){ cv.width = q.l; cv.height = q.a; }
  const e = escala(q.l, q.a);
  cv.style.width = (q.l*e)+'px'; cv.style.height = (q.a*e)+'px';
  const bytes = new Uint8Array(q.bits.match(/../g).map(h=>parseInt(h,16)));
  const img = cx.createImageData(q.l, q.a), passo = (q.l+7)>>3;
  for (let y=0; y<q.a; y++) for (let x=0; x<q.l; x++){
    const bit = (bytes[y*passo + (x>>3)] >> (7-(x&7))) & 1;
    const c = bit ? TINTA : PAPEL, o = (y*q.l+x)*4;
    img.data[o]=c[0]; img.data[o+1]=c[1]; img.data[o+2]=c[2]; img.data[o+3]=255;
  }
  cx.putImageData(img,0,0);
  document.getElementById('est').innerHTML =
    `<b>${q.tela}</b> · cursor <b>${q.cursor}</b> · ` +
    `nível <b>${q.nivel}</b> · ` +
    `${q.docado ? '<b>na dock</b>' : 'na mão'}`;
}

async function manda(corpo){
  const r = await fetch('/evento', {method:'POST', body:JSON.stringify(corpo)});
  pinta(await r.json());
}

const NAVEGA = ['ArrowUp','ArrowDown','ArrowLeft','ArrowRight',' ','Tab','Enter',
                'Backspace'];

// Os números são entrada_t de hal/hal.h, na ordem do enum — se a janela
// mentir sobre qual botão foi apertado, ela deixa de ser o aparelho e vira
// desenho.
const CIMA=1, BAIXO=2, ESQ=3, DIR=4, OK=5, MENU=6, VOLTAR=7, VOZ=8, POWER=9;

addEventListener('keydown', e => {
  if (NAVEGA.includes(e.key)) e.preventDefault();   // nada de rolar a página
  const k = e.key.length === 1 ? e.key.toLowerCase() : e.key;
  if (k === 'd') return manda({docar:1});
  if (k === 't') return manda({tick:1});
  if (k === 'p') return baixa();
  if (k === '0')              return manda({tecla:POWER,     ms:0});
  if (e.key === 'Tab')        return manda({tecla:MENU,      ms:0});
  if (e.key === ' ')          return manda({tecla:VOZ,       ms:0});
  // segurado: 800 ms, acima do limiar de 600 que o app usa pro "volta pra home"
  if (e.key === 'Backspace')  return manda({tecla:VOLTAR, ms: e.shiftKey ? 800 : 0});
  manda({tecla: ({ArrowUp:CIMA,ArrowDown:BAIXO,ArrowLeft:ESQ,
                  ArrowRight:DIR,Enter:OK})[e.key] || 0, ms:0});
});

function baixa(){
  cv.toBlob(b => {
    const a = document.createElement('a');
    a.href = URL.createObjectURL(b);
    a.download = 'tinto.png'; a.click();
  });
}

addEventListener('resize', () => { if (ultimo) pinta(ultimo); });

const CENAS = __CENAS__;
const div = document.getElementById('cenas');
CENAS.forEach((nome, i) => {
  const b = document.createElement('button');
  b.textContent = nome;
  b.onclick = () => manda({cena:i});
  div.appendChild(b);
});

const _antes = pinta;
pinta = q => {
  _antes(q);
  [...div.children].forEach((b, i) => b.classList.toggle('on', i === q.cena));
};

fetch('/quadro').then(r=>r.json()).then(pinta);
</script>
"""


class Servidor(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def _json(self, obj):
        corpo = json.dumps(obj).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(corpo)))
        self.end_headers()
        self.wfile.write(corpo)

    def do_GET(self):
        if self.path == "/quadro":
            return self._json(quadro())
        corpo = PAGINA.replace("__CENAS__",
        json.dumps([r for _n, r, _f in CENAS])).encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(corpo)))
        self.end_headers()
        self.wfile.write(corpo)

    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0))
        d = json.loads(self.rfile.read(n) or "{}")
        global cena_atual
        if "cena" in d:
            cena_atual = int(d["cena"]) % len(CENAS)
            CENAS[cena_atual][2](t)
        elif d.get("docar"):
            t.tinto_docar(0 if t.tinto_docado() else 1)
        elif d.get("tick"):
            t.tinto_tick()
        else:
            t.tinto_evento(int(d.get("tecla", 0)), int(d.get("ms", 0)))
        self._json(quadro())


if __name__ == "__main__":
    porta = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("", porta), Servidor) as s:
        url = f"http://localhost:{porta}"
        print(f"\n  Tinto rodando em \033[1m{url}\033[0m")
        print("  ctrl-c pra parar\n")
        try:
            webbrowser.open(url)
        except Exception:
            pass
        s.serve_forever()
