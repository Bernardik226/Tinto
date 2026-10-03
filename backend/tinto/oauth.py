"""O consentimento do Google, que acontece no navegador.

O aparelho não tem teclado nem navegador, e um dump do flash do ESP32
revelaria no pior caso um token deste serviço, nunca a credencial Google
(`SISTEMA.md` §3).

1. no Tinto: Ajustes → Conexões mostra seis letras
2. a pessoa abre um endereço curto no celular, entra com o Google ALI e
   digita o código
3. o servidor amarra a conta ao aparelho e guarda o refresh token
4. o Tinto diz "conectado como fulano" e nunca mais pergunta
"""

import httpx

from . import config

class SemConta(Exception):
    """O Google não reconhece mais este refresh token.

    Diferente de "o Google está fora do ar": um é esperar, o outro é pedir
    para reconectar. Acontece quando a pessoa revoga o acesso, o app é
    apagado, ou passam seis meses sem sincronizar.
    """


AUTORIZAR = "https://accounts.google.com/o/oauth2/v2/auth"
TROCAR = "https://oauth2.googleapis.com/token"
QUEM_SOU = "https://openidconnect.googleapis.com/v1/userinfo"

# O mesmo buraco de teste de `agenda.py` e `ia.py`: sem ele, provar o
# `pull` exigiria um monkeypatch em `renovar`, que deixaria de ser provada.
transporte = None


def url_de_consentimento(estado: str, forcar: bool = False) -> str:
    """Para onde mandar o navegador.

    `access_type=offline` pede o REFRESH TOKEN; sem ele o acesso morre em uma
    hora. `prompt=consent` só quando `forcar`: o Google só entrega o refresh
    na PRIMEIRA autorização, então quem volta sem ele (e sem um guardado) é
    mandado de novo uma vez; quem já tem entra direto. `select_account`
    deixa escolher entre duas contas no navegador.
    """
    from urllib.parse import urlencode

    return AUTORIZAR + "?" + urlencode({
        "client_id": config.GOOGLE_CLIENT_ID,
        "redirect_uri": config.GOOGLE_REDIRECT,
        "response_type": "code",
        "scope": config.ESCOPOS,
        "access_type": "offline",
        "prompt": "consent" if forcar else "select_account",
        "include_granted_scopes": "true",
        # O código de seis letras vai e volta aqui: amarra este navegador àquele
        # aparelho sem sessão no servidor nem cookie (que não atravessa o QR).
        "state": estado,
    })


async def trocar_codigo(code: str) -> dict:
    """O `code` do redirect vira `{refresh, access}`. O refresh se guarda; o
    access dura uma hora.
    """
    async with httpx.AsyncClient(timeout=10, transport=transporte) as cliente:
        r = await cliente.post(TROCAR, data={
            "code": code,
            "client_id": config.GOOGLE_CLIENT_ID,
            "client_secret": config.GOOGLE_CLIENT_SECRET,
            "redirect_uri": config.GOOGLE_REDIRECT,
            "grant_type": "authorization_code",
        })
        r.raise_for_status()
        d = r.json()

    return {"refresh": d.get("refresh_token", ""),
            "access": d.get("access_token", "")}


async def renovar(refresh: str) -> str:
    """Um access token novo, a partir do refresh. Não se guarda o access: ele
    vale uma hora.
    """
    async with httpx.AsyncClient(timeout=10, transport=transporte) as cliente:
        r = await cliente.post(TROCAR, data={
            "refresh_token": refresh,
            "client_id": config.GOOGLE_CLIENT_ID,
            "client_secret": config.GOOGLE_CLIENT_SECRET,
            "grant_type": "refresh_token",
        })

        # `invalid_grant` com 400: a concessão acabou, tentar de novo não adianta.
        # Separar isto de um 503 separa "espere" de "reconecte".
        if r.status_code == 400:
            erro = ""
            try:
                erro = r.json().get("error", "")
            except ValueError:
                pass
            # Só `invalid_grant`. `invalid_client` é o CLIENT_ID/SECRET deste servidor
            # errado: tratá-lo como SemConta mandaria todo mundo reconectar por causa
            # de uma variável de ambiente.
            if erro == "invalid_grant":
                raise SemConta(erro)

        r.raise_for_status()
        return r.json().get("access_token", "")


async def quem_e(access: str) -> str:
    """O e-mail da conta: é só isso que se guarda de identidade. Guardar o que
    não se usa é o que vaza.
    """
    async with httpx.AsyncClient(timeout=10, transport=transporte) as cliente:
        r = await cliente.get(QUEM_SOU,
                              headers={"Authorization": f"Bearer {access}"})
        r.raise_for_status()
        return r.json().get("email", "")
