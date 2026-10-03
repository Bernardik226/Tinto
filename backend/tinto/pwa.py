"""O aplicativo da PESSOA, no navegador."""

import html
import logging
import secrets
import time

from pydantic import BaseModel
from fastapi import APIRouter, HTTPException, Request
from fastapi.responses import HTMLResponse, RedirectResponse, JSONResponse

from . import config, oauth, portaria
from .memoria import Sincronia, memoria, nome_limpo
from .modelo import notas

log = logging.getLogger("tinto")

pwa = APIRouter()

# Sobe de mão junto com o que muda no PWA. É o carimbo que responde
# "qual código está no ar" sem depender de ler arquivo estático.
PWA_VERSAO = "fase4-envio-5-passos-2"

# A sessão mora no VOLUME (`memoria.sessoes`), e não aqui.
_pessoas: dict[str, str] = {}
_indo: dict[str, str] = {}       # nonce → para onde voltar

BISCOITO = "tinto_pessoa"


def quem_e(request: Request) -> str:
    """Quem está logado — se ainda estiver em TINTO_CONTAS."""
    ficha = request.cookies.get(BISCOITO, "")
    email = memoria.dono_da_sessao(ficha, "app") or _pessoas.get(ficha, "")
    return email if email and config.entra_conta(email) else ""


def abre_sessao(request: Request, email: str, para: str = "/e"):
    ficha = memoria.abre_sessao(email, "app")
    r = RedirectResponse(para, status_code=303)
    # Um ano. A ficha não expira sozinha do lado do servidor — quem
    # encerra é o botão de sair —, e um cookie que morre antes dela
    # devolveria a tela de login sem que ninguém tivesse pedido.
    r.set_cookie(BISCOITO, ficha, httponly=True, samesite="lax",
                 max_age=365 * 86400, secure=request.url.scheme == "https")
    return r


@pwa.get("/e/google")
def entrar(request: Request, codigo: str = ""):
    """O consentimento. É AQUI que ele acontece, e não no aparelho."""
    from .app import exige_chaves
    exige_chaves(("GOOGLE_CLIENT_ID", config.GOOGLE_CLIENT_ID),
                 ("GOOGLE_REDIRECT", config.GOOGLE_REDIRECT))

    nonce = secrets.token_urlsafe(18)
    # ── o código NÃO atravessa o login ───────────────────────────────
    # Ele já atravessou, e a razão era boa: quem chega pelo QR está com
    # as seis letras na frente dos olhos e cinco minutos no relógio.
    del codigo   # lido e descartado de propósito
    _indo[nonce] = "/e"
    return RedirectResponse(oauth.url_de_consentimento(f"pwa:{nonce}"))


# Os nonces que já voltaram do Google sem refresh token e foram mandados
# de volta com `prompt=consent`. Um por login, e só um: se nem com a tela
# de permissão vier refresh, insistir seria pingue-pongue entre o Google e
# o servidor, com a pessoa no meio.
_ja_insisti: set[str] = set()


def _faz_tempo(quando: float) -> str:
    """"há 3 min". Nunca um timestamp: a pergunta é "ele está vivo?"."""
    s = max(0, int(time.time() - quando))
    if s < 60:
        return "agora"
    if s < 3600:
        return f"há {s // 60} min"
    if s < 86400:
        return f"há {s // 3600} h"
    return f"há {s // 86400} d"

def _limpa(codigo: str) -> str:
    """As seis letras, ou nada."""
    codigo = "".join(codigo.split()).upper()
    return codigo if len(codigo) == 6 and codigo.isalpha() else ""


async def volta_do_google(request: Request, estado: str, code: str):
    """O Google devolveu um login de PESSOA."""
    nonce = estado[4:]
    if nonce not in _indo:
        raise HTTPException(400, "este login não vale mais")
    para = _indo.pop(nonce, "/e")

    tokens = await oauth.trocar_codigo(code)
    email = await oauth.quem_e(tokens["access"])

    if not config.entra_conta(email):
        log.info("login recusado · %s não está em TINTO_CONTAS · ip %s",
                 email, portaria.de_quem(request))
        # A mesma capa da entrada, com o estilo do app.
        return HTMLResponse(status_code=403, content=(
            '<!doctype html><html lang="pt-BR" data-tema="sistema"><head>'
            '<meta charset="utf-8"><meta name="viewport" '
            'content="width=device-width,initial-scale=1">'
            '<title>Tinto</title><link rel="stylesheet" href="/e/app.css">'
            '</head><body><section id="entrada"><div class="capa">'
            '<span class="logo grande">Tinto</span>'
            '<h1>Esta conta não pode usar este servidor.</h1>'
            '<p>Peça a quem hospeda este Tinto para liberar o seu e-mail.</p>'
            '<a class="botao forte" href="/e">Voltar</a>'
            '</div></section></body></html>'))
    log.info("login · %s", email)

    # O refresh token fica GUARDADO com a pessoa, e não com o aparelho: é
    # ele que faz o segundo Tinto dela ser só seis letras.
    memoria.guarda_pessoa(email, tokens["refresh"])

    # ── a permissão que faltou ───────────────────────────────────────
    # Sem refresh token novo e sem nenhum guardado, esta conta não
    # sincroniza: o acesso morre em uma hora e o aparelho para no meio da
    # tarde. É o único caso em que a tela de permissão do Google se
    # justifica — e ela é pedida AQUI, uma vez, em vez de a cada login.
    if not memoria.refresh_de(email) and nonce not in _ja_insisti:
        outro = secrets.token_urlsafe(18)
        _indo[outro] = para
        _ja_insisti.add(outro)
        return RedirectResponse(
            oauth.url_de_consentimento(f"pwa:{outro}", forcar=True))

    _ja_insisti.discard(nonce)
    return abre_sessao(request, email, para)


@pwa.post("/e/api/soltar")
def api_soltar(request: Request, device_id: str = ""):
    """A pessoa desvincula um Tinto DELA."""
    email = quem_e(request)
    if not email:
        raise HTTPException(401, "entre com a sua conta")

    ap = memoria.por_device.get(device_id)
    if not ap or ap.pessoa != email:
        raise HTTPException(404, "não achei este aparelho")

    # A conta sai; o que já desceu para o cartão FICA. Mesma regra do
    # `POST /v1/desparear`, e de propósito: o aparelho
    # continua sendo um Tinto inteiro depois de desconectado — a agenda
    # que já está lá continua legível sem rede e sem conta.
    ap.pessoa = ""
    ap.google_refresh = ""
    ap.tz_nome = ""
    ap.sinc = Sincronia()
    ap.pendentes.clear()
    ap.saindo.clear()
    memoria.grava()
    log.info("desvínculo · %s ↔ %s", email, device_id)
    return {"ok": True}


def _do_pwa(nome: str):
    """Um arquivo da pasta `pwa/`, achado nos dois lugares onde ela mora."""
    from pathlib import Path

    aqui = Path(__file__).resolve()
    for base in (aqui.parents[1], aqui.parents[2]):
        c = base / "pwa" / nome
        if c.exists():
            return c
    return None


@pwa.get("/e/manifest.json")
def manifesto():
    from fastapi.responses import FileResponse

    m = _do_pwa("manifest.json")
    if not m:
        raise HTTPException(404, "sem manifesto")
    return FileResponse(m, media_type="application/manifest+json")


# Os quatro nomes que existem, e a lista é FECHADA de propósito.
ESTATICOS = {
    "icone-192.png": "image/png",
    "icone-512.png": "image/png",
    "icone-maskable-512.png": "image/png",
    "apple-touch-icon.png": "image/png",
    "logo.png": "image/png",
    "app.css": "text/css; charset=utf-8",
    "app.js": "text/javascript; charset=utf-8",
}

# Os ícones da gaveta, e SÓ eles: são os que ganham um ano de cache.
ICONES = {"icone-192.png", "icone-512.png",
          "icone-maskable-512.png", "apple-touch-icon.png"}


@pwa.get("/e/{nome}")
def estatico(nome: str):
    """Os ícones do aplicativo."""
    from fastapi.responses import FileResponse

    tipo = ESTATICOS.get(nome)
    if not tipo:
        raise HTTPException(404, "não encontrado")
    arquivo = _do_pwa(nome)
    if not arquivo:
        raise HTTPException(404, "não empacotado")

    # O ÍCONE é imutável por um ano: o desenho não muda sem o arquivo
    # mudar de conteúdo, e quem instalou o aplicativo não deve pedi-lo de
    # novo a cada abertura. Quando ele mudar, muda o nome.
    cache = ("public, max-age=31536000, immutable" if nome in ICONES
             else "no-cache")
    return FileResponse(arquivo, media_type=tipo,
                        headers={"cache-control": cache})


@pwa.get("/", include_in_schema=False)
def raiz():
    """O domínio sozinho abre o aplicativo: é o link que se manda."""
    return RedirectResponse("/e", status_code=307)


@pwa.get("/e", response_class=HTMLResponse)
def pagina(request: Request):
    """A moldura, como ela está no disco."""
    arquivo = _do_pwa("index.html")
    if not arquivo:
        return HTMLResponse("<h1>Tinto</h1><p>o aplicativo não foi "
                            "empacotado</p>")
    try:
        return HTMLResponse(arquivo.read_text(encoding="utf-8"))
    except OSError:
        return HTMLResponse("<h1>Tinto</h1><p>não consegui ler a página</p>")


class Vinculo(BaseModel):
    """As seis letras que a pessoa digita em Minha Conta."""

    codigo: str = ""


@pwa.post("/e/api/vincular")
def api_vincular(request: Request, corpo: Vinculo):
    """O aparelho se anexa à conta de quem já entrou."""
    email = quem_e(request)
    if not email:
        raise HTTPException(401, "entre com a sua conta")

    # ── seis letras são 300 milhões de tentativas, ou vinte por minuto ─
    # O código tem seis letras e vale cinco minutos. Sem limite, varrer o
    # espaço inteiro é trabalho de tarde — e cada acerto entrega o
    # aparelho de alguém a quem varreu.
    portaria.limita(request, "vincular", quantas=20, janela_s=60)

    refresh = memoria.pessoas.get(email, "")
    if not refresh:
        return JSONResponse(status_code=409, content={
            "motivo": "Entre de novo com o Google: falta a autorização "
                      "desta conta."})

    ap = memoria.confirma_pareamento(_limpa(corpo.codigo), email, refresh)
    if not ap:
        return JSONResponse(status_code=404, content={
            "motivo": "Esse código não vale mais. Ele expira em cinco "
                      "minutos e serve uma vez só — peça outro no "
                      "aparelho."})

    memoria.grava()
    log.info("vínculo · %s ↔ %s", email, ap.device_id)
    return {"ok": True, "device_id": ap.device_id,
            "nome": ap.nome or ap.device_id}


class NomeNovo(BaseModel):
    nome: str = ""


@pwa.patch("/e/api/aparelho/{device_id}")
def api_renomeia(device_id: str, corpo: NomeNovo, request: Request):
    """O nome do aparelho, que a pessoa escolhe."""
    email = quem_e(request)
    if not email:
        raise HTTPException(401, "entre com a sua conta")

    ap = memoria.por_device.get(device_id)
    if not ap or ap.pessoa != email:
        raise HTTPException(404, "não achei este aparelho")

    # Na próxima sincronização o aparelho recebe, e adota.
    ap.nome = nome_limpo(corpo.nome)[:24]
    memoria.grava()
    return {"ok": True, "nome": ap.nome}


@pwa.get("/e/api/versao")
def api_versao():
    """Qual código está no ar."""
    return {"pwa": PWA_VERSAO}


@pwa.post("/e/api/sair")
def api_sair(request: Request):
    """Sair é do BOTÃO, e de mais nada."""
    email = quem_e(request)
    ficha = request.cookies.get(BISCOITO, "")

    memoria.fecha_sessao(ficha)
    if email:
        memoria.fecha_sessoes_de(email, "app")
    _pessoas.pop(ficha, None)

    r = JSONResponse({"ok": True})
    r.delete_cookie(BISCOITO)
    return r


@pwa.get("/e/api/anotacoes")
def api_anotacoes(request: Request):
    """As anotações faladas da pessoa, as mais novas primeiro. Só leitura:
    elas nascem da voz, no Tinto."""
    email = quem_e(request)
    if not email:
        raise HTTPException(401, "entre com a sua conta")
    return {"anotacoes": [
        {"id": a.id, "titulo": a.titulo, "corpo": a.corpo, "dia": a.dia,
         "criada_em": a.criada_em}
        for a in notas.anotacoes_de(email, limite=200)]}


@pwa.get("/e/api/conta")
def api_conta(request: Request):
    """Quem é a pessoa, quanto de voz sobrou e quais Tintos são dela."""
    email = quem_e(request)
    if not email:
        raise HTTPException(401, "entre com a sua conta")

    meus = [a for a in memoria.por_device.values() if a.pessoa == email]

    usados, limite, dias = memoria.quota(meus[0]) if meus else (0, 0, 0)
    # Os envios do mês, para o app abrir cada um. O texto só existe nos
    # confirmados (ver `memoria.confirmou`); o resto é data e duração.
    recentes = [{"segundos": u["segundos"], "em": u["em"],
                 "confirmada": "texto" in u,
                 "texto": u.get("texto", ""), "acoes": u.get("acoes", [])}
                for u in (memoria.usos_recentes(meus[0], 20) if meus else [])]

    aparelhos = []
    for ap in meus:
        aparelhos.append({
            "device_id": ap.device_id,
            "nome": ap.nome or "",
            # Falou com o servidor há pouco: o pull mantém o aparelho
            # ligado sempre por perto, então 2 min de silêncio é sumiço.
            "online": time.time() - getattr(ap, "visto_em", 0) < 120,
            "sincronizou": _faz_tempo(getattr(ap, "visto_em", 0)),
        })

    return {"email": email,
            "voz": {"usados": usados, "limite": limite, "dias": dias,
                    "recentes": recentes},
            "aparelhos": aparelhos}

