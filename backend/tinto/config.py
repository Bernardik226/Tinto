"""O que vem de fora, e o que acontece quando não vem.

Nenhuma chave mora no código nem tem padrão que funcione. `FALTANDO`
deixa o servidor SUBIR sem elas e dizer o que falta em cada rota: recusar
a subir deixaria o aparelho dizendo "sem contato" por causa de uma
variável de ambiente.
"""

import logging
import os

FALTANDO = "(não configurado)"


def _var(nome: str) -> str:
    return os.environ.get(nome, "").strip() or FALTANDO


# ── Google ──────────────────────────────────────────────────────────
# O consentimento acontece no NAVEGADOR: um dump do flash do ESP32 revela,
# no pior caso, um token deste serviço, nunca a credencial Google.
GOOGLE_CLIENT_ID = _var("GOOGLE_CLIENT_ID")
GOOGLE_CLIENT_SECRET = _var("GOOGLE_CLIENT_SECRET")
GOOGLE_REDIRECT = _var("GOOGLE_REDIRECT")

# O `calendar` INTEIRO, e não `app.created`: com ele, apagar no Tinto um
# evento criado no celular dava 403 e o pull o ressuscitava. O preço é a
# verificação de escopo sensível do Google.
ESCOPOS = (
    "https://www.googleapis.com/auth/calendar "
    "https://www.googleapis.com/auth/tasks "
    "openid email"
)

# ── quem pode usar este servidor ────────────────────────────────────
# `TINTO_CONTAS`: os e-mails Google que podem entrar, cada um com o limite
# de voz por mês em minutos (opcional; sem número, `QUOTA_PADRAO_S`):
#
#     TINTO_CONTAS=voce@gmail.com:120,mae@gmail.com:30,pai@gmail.com
#
# É o único portão: quem hospeda paga a Groq e a Anthropic. **Vazia =
# ninguém entra.**


def _contas(nome: str) -> dict[str, int | None]:
    """"a@x.com:120,b@x.com" → {"a@x.com": 7200, "b@x.com": None}."""
    contas: dict[str, int | None] = {}
    for item in os.environ.get(nome, "").split(","):
        email, _, minutos = item.strip().partition(":")
        email = email.strip().lower()
        if not email:
            continue
        minutos = minutos.strip()
        if minutos and not minutos.isdigit():
            # Um limite mal escrito ("1h") não vira o padrão calado.
            logging.getLogger("tinto").warning(
                "TINTO_CONTAS: limite inválido para %s (%r) — vale o padrão; "
                "use minutos, como %s:120", email, minutos, email)
        contas[email] = int(minutos) * 60 if minutos.isdigit() else None
    return contas


CONTAS = _contas("TINTO_CONTAS")


def entra_conta(email: str) -> bool:
    return email.strip().lower() in CONTAS


# ── as duas IAs ──────────────────────────────────────────────────────
GROQ_API_KEY = _var("GROQ_API_KEY")
ANTHROPIC_API_KEY = _var("ANTHROPIC_API_KEY")

# ── o que a voz custa ───────────────────────────────────────────────
# Preços de tabela, ESTIMATIVA declarada: em variável para corrigir sem
# deploy. O teto de fábrica (segundos de fala por mês) é conservador: teto
# largo só se descobre na fatura.
QUOTA_PADRAO_S = int(os.environ.get("QUOTA_PADRAO_S", str(30 * 60)))


def limite_de(email: str) -> int:
    """Os segundos de voz por mês desta conta: o dela, ou o padrão."""
    proprio = CONTAS.get(email.strip().lower())
    return QUOTA_PADRAO_S if proprio is None else proprio


# Rápido o bastante para uma fala de 20 s, e bom em português.
MODELO_STT = os.environ.get("MODELO_STT", "whisper-large-v3-turbo")

# Classificar e extrair data é a tarefa mais barata de um LLM, e o custo
# por captura decide quantas vezes por dia o aparelho pode ser usado.
MODELO_LLM = os.environ.get("MODELO_LLM", "claude-haiku-4-5-20251001")


def falta(**chaves: str) -> list[str]:
    """Quais destas não estão configuradas, PELO NOME (pelo valor, a lista
    diria quantas faltam e nenhuma delas).
    """
    return [nome for nome, valor in chaves.items() if valor == FALTANDO]


# ── o aviso do Google, em vez da pergunta ───────────────────────────
# O endereço PÚBLICO que recebe o push do Calendar. Vazio desliga o recurso
# e o servidor pergunta de cinco em cinco segundos. O domínio precisa estar
# VERIFICADO (Search Console + Cloud Console), senão o `watch` responde 401
# e o polling segue valendo.
TINTO_WEBHOOK_URL = _var("TINTO_WEBHOOK_URL")

# Volta em todo aviso (`X-Goog-Channel-Token`): a rota é pública por
# obrigação, e sem isto qualquer um acordaria os pulls.
TINTO_WEBHOOK_SEGREDO = _var("TINTO_WEBHOOK_SEGREDO")

# O arquivo de verificação do Search Console, só o miolo: para
# `google1a2b3c4d.html`, `1a2b3c4d`. Variável, para não depender de deploy.
GOOGLE_SITE_VERIFICATION = _var("GOOGLE_SITE_VERIFICATION")


# ── um processo, e só um ─────────────────────────────────────────────
def confere_um_processo() -> None:
    """Recusa a partida se pedirem mais de um worker.

    O estado é um dicionário em memória espelhado num JSON: dois workers são
    dois dicionários, e a gravação de um apaga a do outro sem erro nenhum.
    Escalar é máquina maior, não mais processos.
    """
    quantos = os.environ.get("WEB_CONCURRENCY", "").strip()
    if quantos and quantos != "1":
        raise RuntimeError(
            f"WEB_CONCURRENCY={quantos}: o Tinto roda em um processo só. "
            "O estado é um dicionário em memória espelhado no volume, e "
            "dois workers perdem pareamento e cota sem dizer nada. "
            "Para aguentar mais gente: máquina maior, ou Postgres.")


# ── onde o estado mora em disco ─────────────────────────────────────
# O volume do servidor; sem a variável, tudo em memória (os testes).
ESTADO = os.environ.get("TINTO_ESTADO") or None


def estado_ao_lado(sufixo: str) -> str | None:
    """`/data/estado.json` → `/data/estado-<sufixo>.json`, ou None sem disco.

    Pelo STEM: com `TINTO_ESTADO=/data/estado` um `.replace(".json")` não
    teria efeito, e dois módulos escreveriam no MESMO arquivo.
    """
    if not ESTADO:
        return None
    from pathlib import Path
    p = Path(ESTADO)
    return str(p.with_name(f"{p.stem}-{sufixo}{p.suffix or '.json'}"))
