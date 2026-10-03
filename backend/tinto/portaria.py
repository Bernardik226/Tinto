"""O que protege as portas, antes de qualquer rota decidir alguma coisa.

· **cabeçalhos** que dizem ao navegador o que ele NÃO deve deixar esta
  página fazer
· **limite de tentativas** nas portas que aceitam chute

Nenhuma substitui autenticação: encurtam o que sobra quando o endereço
público é descoberto.
"""

import time

from fastapi import HTTPException, Request
from fastapi.responses import Response


# ── cabeçalhos ──────────────────────────────────────────────────────
CABECALHOS = {
    # **Clickjacking.** Sem isto, um site embute o aplicativo num `<iframe>`
    # invisível e colhe o clique de quem está logado ("desconectar a conta").
    "X-Frame-Options": "DENY",

    # O mesmo na forma moderna (`frame-ancestors`). `default-src 'self'`:
    # nada de script, fonte ou imagem de fora, e uma injeção de `<script
    # src=...>` não consegue buscá-lo.
    "Content-Security-Policy":
        "default-src 'self'; style-src 'self' 'unsafe-inline'; "
        "img-src 'self' data:; frame-ancestors 'none'; base-uri 'none'",

    # Sem adivinhar tipo: texto não vira script porque o começo parecia um.
    "X-Content-Type-Options": "nosniff",

    # O endereço do aplicativo não vaza no `Referer`.
    "Referrer-Policy": "same-origin",

    # A página web não precisa de câmera, microfone nem localização; negar por
    # escrito impede que um dia precise sem ninguém decidir.
    "Permissions-Policy": "camera=(), microphone=(), geolocation=()",
}

# HSTS só sob HTTPS: mandado em `http://localhost`, ensinaria o navegador a
# exigir HTTPS de localhost para sempre.
HSTS = "max-age=31536000; includeSubDomains"


async def cabecalhos(request: Request, seguir):
    resposta: Response = await seguir(request)

    for chave, valor in CABECALHOS.items():
        resposta.headers.setdefault(chave, valor)

    if _veio_por_https(request):
        resposta.headers.setdefault("Strict-Transport-Security", HSTS)

    return resposta


def _veio_por_https(request: Request) -> bool:
    """A conexão do NAVEGADOR foi cifrada?

    Atrás de um balanceador, `request.url.scheme` é `http` (o proxy termina o
    TLS), e o HSTS nunca saía em produção. `x-forwarded-proto` é forjável,
    mas o pior que um forjador ganha é um cabeçalho a mais.
    """
    proto = request.headers.get("x-forwarded-proto", "")
    if proto:
        return proto.split(",")[0].strip().lower() == "https"
    return request.url.scheme == "https"


# ── limite de tentativas ────────────────────────────────────────────
# Em memória, por IP. Não protege senha (não há): protege da ENUMERAÇÃO de
# MACs em `/v1/registrar`, onde 403 × 200 contaria quais aparelhos
# existem.
_tentativas: dict[str, list[float]] = {}


def de_quem(request: Request) -> str:
    """O IP de quem pediu, atrás de um proxy.

    Só `request.client` faria o mundo inteiro dividir um contador. O PRIMEIRO
    de `x-forwarded-for` é o cliente; forjável, por isso o limite é um degrau
    e não uma fechadura.
    """
    encaminhado = request.headers.get("x-forwarded-for", "")
    if encaminhado:
        return encaminhado.split(",")[0].strip()[:45]
    return request.client.host if request.client else "?"


def bateu(chave: str, quantas: int, janela_s: float) -> bool:
    """Registra uma tentativa; `True` quando passou do teto. A limpeza é aqui,
    na leitura: um varredor agendado seria máquina a mais.
    """
    agora = time.time()

    recentes = [t for t in _tentativas.get(chave, []) if agora - t < janela_s]
    recentes.append(agora)
    _tentativas[chave] = recentes

    # Os endereços que pararam de tentar somem junto.
    if len(_tentativas) > 512:
        for k in [k for k, v in _tentativas.items()
                  if not v or agora - v[-1] > janela_s]:
            del _tentativas[k]

    return len(recentes) > quantas


def limita(request: Request, porta: str, quantas: int, janela_s: float):
    """Recusa com 429: é sobre RITMO, não permissão, e não conta nada sobre o
    que existe do outro lado.
    """
    if bateu(f"{porta}\x00{de_quem(request)}", quantas, janela_s):
        raise HTTPException(429, "tentativas demais; espere um pouco",
                            headers={"Retry-After": str(int(janela_s))})


def esquece_tudo() -> None:
    """Zera os contadores. Só para os testes."""
    _tentativas.clear()
