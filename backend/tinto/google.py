"""A tradução do Google para o contrato do device."""

import hashlib
import logging
from datetime import datetime

log = logging.getLogger("tinto.google")

from .contrato import Item, Tipo


# Quantos caracteres o id do contrato comporta — `item_t.id[40]` do device,
# menos o terminador.
MARCA_MAX = 39


def marca(prefixo: str, bruto: str) -> str:
    """`g:` + o id do Google, encurtado SEM colidir quando não cabe."""
    inteiro = f"{prefixo}:{bruto}"
    if len(inteiro) <= MARCA_MAX:
        return inteiro

    resumo = hashlib.sha1(bruto.encode()).hexdigest()[:6]
    cabe = MARCA_MAX - len(prefixo) - 1 - 1 - len(resumo)
    return f"{prefixo}:{bruto[:cabe]}~{resumo}"


def casa_marca(marcado: str, bruto: str) -> bool:
    """Este id do Google é o que está por trás daquela marca?"""
    prefixo = marcado.split(":", 1)[0] if ":" in marcado else ""
    return marca(prefixo, bruto) == marcado


def _na_hora_da_pessoa(dt: str, tz_nome: str) -> datetime | None:
    """Um `dateTime` do Google → o mesmo instante, no fuso da CONTA."""
    if not dt:
        return None
    try:
        quando = datetime.fromisoformat(dt)
    except ValueError:
        return None

    if quando.tzinfo is None or not tz_nome:
        return quando
    try:
        from zoneinfo import ZoneInfo

        return quando.astimezone(ZoneInfo(tz_nome))
    except Exception:
        return quando


def _hora(quando: dict, tz_nome: str = "") -> str:
    """"14:00" a partir de um `start`/`end` do Google, no fuso da conta.
    """
    local = _na_hora_da_pessoa(quando.get("dateTime") or "", tz_nome)
    return local.strftime("%H:%M") if local else ""


def _dia(quando: dict, tz_nome: str = "") -> str:
    """"2026-08-27", venha de `date` ou de `dateTime`.
    """
    if quando.get("date"):
        return quando["date"]
    local = _na_hora_da_pessoa(quando.get("dateTime") or "", tz_nome)
    return local.strftime("%Y-%m-%d") if local else ""


def dia_feito(completed: str, tz_nome: str = "") -> str:
    """O dia da conclusão no fuso da CONTA, como o Google Tasks mostra.

    `completed` vem em UTC: marcar às 21h50 em Brasília é 00:50 do dia
    seguinte, e cortar o texto dava amanhã.
    """
    local = _na_hora_da_pessoa(completed, tz_nome)
    return local.strftime("%Y-%m-%d") if local else ""


# ── a ROTINA, e por que ela não desce ocorrência por ocorrência ──────
# "Todo dia às sete" é UMA coisa na cabeça da pessoa, e era noventa no
# cartão: o Google, com `singleEvents=true`, devolve cada ocorrência
# expandida, e cada uma virava um item, uma escrita e uma repintura de
# tinta. O vidro passava minutos enchendo a agenda de um em um — e mudar
# para "toda hora" seriam dois mil.
REGRA_MAX = 48

_FREQ = {"DAILY": "d", "WEEKLY": "s", "MONTHLY": "m", "YEARLY": "a"}
_DIAS = {"SU": "0", "MO": "1", "TU": "2", "WE": "3",
         "TH": "4", "FR": "5", "SA": "6"}


def regra_do_evento(ev: dict) -> str:
    """O `recurrence` do Google → a regra curta do contrato."""
    linhas = ev.get("recurrence") or []
    rrule = next((l for l in linhas if l.upper().startswith("RRULE:")), "")
    if not rrule:
        return ""

    partes = {}
    for pedaco in rrule.split(":", 1)[1].split(";"):
        if "=" in pedaco:
            chave, valor = pedaco.split("=", 1)
            partes[chave.upper()] = valor

    freq = _FREQ.get(partes.get("FREQ", "").upper(), "")
    if not freq:
        return ""

    intervalo = partes.get("INTERVAL", "1")
    if not intervalo.isdigit() or not 1 <= int(intervalo) <= 99:
        intervalo = "1"

    regra = f"{freq}:{intervalo}"

    if freq == "s":
        dias = "".join(_DIAS[d.strip().upper()[-2:]]
                       for d in partes.get("BYDAY", "").split(",")
                       if d.strip().upper()[-2:] in _DIAS)
        if dias:
            regra += f":{''.join(sorted(set(dias)))}"

    # O fim, quando ele existe. Sem isto, "toda terça até dezembro" seria
    # para sempre no aparelho — e o Google pararia sozinho, deixando as
    # duas telas em desacordo a partir de janeiro.
    until = partes.get("UNTIL", "")
    if until:
        regra += f"|u={until[:8]}"
    elif partes.get("COUNT", "").isdigit():
        regra += f"|c={partes['COUNT']}"

    return regra[:REGRA_MAX]


def _mmdd(dia: str) -> str:
    """"2026-09-15" → "0915". É como a exceção cabe no campo."""
    return dia.replace("-", "")[4:8] if len(dia) >= 10 else ""


def com_excecoes(regra: str, dias: list[str]) -> str:
    """A regra mais os dias que NÃO acontecem."""
    if not dias:
        return regra

    for dia in sorted(set(dias)):
        mmdd = _mmdd(dia)
        if not mmdd:
            continue
        emenda = (f"{regra}|x={mmdd}" if "|x=" not in regra
                  else f"{regra},{mmdd}")
        if len(emenda) > REGRA_MAX:
            # ponytail: as exceções que não couberam viram um dia mostrado
            # a mais. Se isso doer, o caminho é o device pedir a lista de
            # buracos da série por um campo próprio.
            log.info("exceções não couberam na regra: %s", regra)
            break
        regra = emenda
    return regra


def _ultimo_dia(inicio: dict, fim: dict, tz_nome: str = "") -> str:
    """O último dia que o evento OCUPA — vazio quando ele cabe num dia só."""
    from datetime import date, timedelta

    comeco = _dia(inicio, tz_nome)
    if not comeco:
        return ""

    if fim.get("date"):
        try:
            ultimo = (date.fromisoformat(fim["date"])
                      - timedelta(days=1)).isoformat()
        except ValueError:
            return ""
    else:
        local = _na_hora_da_pessoa(fim.get("dateTime") or "", tz_nome)
        if not local:
            return ""
        ultimo = local.strftime("%Y-%m-%d")
        if local.strftime("%H:%M") == "00:00":
            ultimo = (local.date() - timedelta(days=1)).isoformat()

    return ultimo if ultimo > comeco else ""


def evento_para_item(ev: dict, tz_nome: str = "", agenda: str = "",
                     regra: str = "") -> Item | None:
    """Um `Event` do Calendar → um item do contrato."""
    if ev.get("status") == "cancelled":
        return None

    # RN: quem recusou não vai. `self` é a pessoa dona do calendário — a
    # resposta dos outros convidados não é da nossa conta.
    for quem in ev.get("attendees", []):
        if quem.get("self") and quem.get("responseStatus") == "declined":
            return None

    inicio = ev.get("start", {}) or {}
    fim = ev.get("end", {}) or {}
    dia_inteiro = bool(inicio.get("date"))

    return Item(
        id=marca("g", ev.get("id", "")),
        t=(ev.get("summary") or "")[:127],
        h="" if dia_inteiro else _hora(inicio, tz_nome),
        f="" if dia_inteiro else _hora(fim, tz_nome),
        # RN-B7: truncar, não recusar. Endereço comprido some da tela, e
        # recusar faria o compromisso inteiro sumir por causa do endereço.
        l=(ev.get("location") or "")[:95],
        di=dia_inteiro,
        # `recurringEventId` é a instância; `recurrence` é o mestre. Os
        # dois querem dizer a mesma coisa para quem olha a tela: isto se
        # repete, e o que você faz aqui vale para este dia.
        r=bool(ev.get("recurringEventId") or ev.get("recurrence")),
        rr=regra[:47],
        d=_dia(inicio, tz_nome),
        # Até quando ele vai, quando não acaba no mesmo dia. O device usa
        # o par `d`..`p` como intervalo — é o mesmo campo com que a tarefa
        # diz o prazo.
        p=_ultimo_dia(inicio, fim, tz_nome),
        tp=Tipo.EVENTO,
        o="g",
        # De qual agenda ele veio, pelo NOME. É a linha do detalhe que
        # manda a pessoa procurar no lugar certo do celular.
        a=agenda[:31],
    )


def _prazo_da_tarefa(due: str, tz_nome: str) -> tuple[str, str]:
    """O `due` do Tasks → (dia, hora). A hora quase sempre é vazia."""
    if not due:
        return "", ""

    local = _na_hora_da_pessoa(due, tz_nome)
    if not local:
        return due[:10], ""

    # Meia-noite em UTC é o jeito do Tasks dizer "só a data".
    if due.endswith(("Z", "+00:00")) and due[11:16] == "00:00":
        return due[:10], ""

    return local.strftime("%Y-%m-%d"), local.strftime("%H:%M")


def tarefa_para_item(tk: dict, lista: str = "", tz_nome: str = "") -> Item | None:
    """Um `Task` do Google Tasks → um item do contrato."""
    # APAGADA não desce como item: ela não existe mais, e quem cuida dela
    # é a lista de removidos do pull. Descer como item faria o device
    # gravar no cartão uma tarefa que o Google acabou de apagar.
    if tk.get("deleted"):
        return None

    dia, hora = _prazo_da_tarefa(tk.get("due") or "", tz_nome)

    if hora:
        # Alto, porque é raro e porque foi ele que faltou: uma tarefa com
        # hora é o híbrido, e saber que ela chegou assim é o que separa
        # "o Google não manda" de "nós jogamos fora".
        log.info("tarefa com hora: %s · %s %s", tk.get("title", ""), dia, hora)

    return Item(
        id=marca("t", tk.get("id", "")),
        t=(tk.get("title") or "")[:127],
        d=dia,
        h=hora,
        ok=tk.get("status") == "completed",
        c=dia_feito(tk.get("completed") or "", tz_nome),
        tp=Tipo.TAREFA,
        o="g",
        # O NOME da lista a que ela pertence viaja no `l`, que em tarefa
        # não é local — é onde ela mora. Nome e não id: é ele que a home
        # usa como título do grupo, e um id de tasklist na tela não diz
        # nada a ninguém.
        l=lista[:47],
    )


def lista_para_item(tasklist: dict, total: int, feitas: int) -> Item:
    """Uma `TaskList` → um item do contrato."""
    return Item(
        id=marca("l", tasklist.get("id", "")),
        t=(tasklist.get("title") or "")[:63],
        tp=Tipo.LISTA,
        o="g",
        n=total,
        k=feitas,
    )


# ── as agendas, e por que a curadoria é uma tela ─────────────────────
# Uma conta Google típica tem seis a dez calendários inscritos, e a maior
# parte não é compromisso: feriados, fases da lua, aniversários do
# contatos. Numa tela de 240 px isso não é ruído — é a tela inteira.
DESLIGADAS_POR_PADRAO = (
    "holiday@group.v.calendar.google.com",
    "#contacts@group.v.calendar.google.com",
    "#weeknum@group.v.calendar.google.com",
)


def vem_desligada(calendario_id: str) -> bool:
    """Agenda que enche a tela sem informar o dia."""
    return any(marca in calendario_id for marca in DESLIGADAS_POR_PADRAO)


# ── a agenda em que o Tinto escreve ─────────────────────────────────
# Uma agenda PRÓPRIA, e agora por escolha e não por limite: desde 02/09 o
# escopo é o `calendar` inteiro, e escrever em `primary` seria possível.
AGENDA_TINTO = "Tinto"


def _mais_um_dia(dia: str) -> str:
    """"2026-08-14" → "2026-08-15".
    """
    from datetime import date, timedelta

    try:
        return (date.fromisoformat(dia) + timedelta(days=1)).isoformat()
    except ValueError:
        return dia


def duracao_s(ev: dict) -> int:
    """Quanto um evento do Calendar dura, em segundos. Uma hora quando
    não se sabe (dia inteiro, formato estranho), e nunca menos de 5 min."""
    try:
        de = datetime.fromisoformat(ev["start"]["dateTime"])
        ate = datetime.fromisoformat(ev["end"]["dateTime"])
        return max(int((ate - de).total_seconds()), 300)
    except (KeyError, ValueError, TypeError):
        return 3600


def _fim_do_evento(dia: str, hora: str, fim: str = "",
                   dura_s: int = 3600) -> tuple[str, str]:
    """Onde o evento acaba: ("2026-08-27", "23:30") → ("2026-08-28", "00:30")."""
    from datetime import timedelta

    try:
        comeca = datetime.fromisoformat(f"{dia}T{hora}")
    except ValueError:
        return dia, hora

    try:
        termina = datetime.fromisoformat(f"{dia}T{fim}") if fim else None
    except ValueError:
        termina = None
    if termina is None:
        termina = comeca + timedelta(seconds=dura_s)
    elif termina <= comeca:
        termina += timedelta(days=1)
    return termina.date().isoformat(), termina.strftime("%H:%M")


def corpo_de_evento(g, tz_nome: str = "", dura_s: int = 3600) -> dict:
    """Um gesto → o corpo de um `event` do Calendar."""
    corpo: dict = {"summary": g.t[:127]}

    if g.h and g.d:
        corpo["start"] = {"dateTime": f"{g.d}T{g.h}:00"}
        fim_d, fim_h = _fim_do_evento(g.d, g.h, getattr(g, "f", ""), dura_s)
        corpo["end"] = {"dateTime": f"{fim_d}T{fim_h}:00"}
        if tz_nome:
            corpo["start"]["timeZone"] = tz_nome
            corpo["end"]["timeZone"] = tz_nome
    elif g.d:
        corpo["start"] = {"date": g.d}
        corpo["end"] = {"date": _mais_um_dia(g.d)}

    return corpo


def corpo_de_tarefa(g) -> dict:
    """Um gesto → o corpo de uma `task` do Tasks."""
    corpo: dict = {"title": g.t[:127],
                   "status": "completed" if g.ok else "needsAction"}

    # ── tirar o prazo é MANDAR `null`, não calar ──────────────────────
    # O gesto sobe como PATCH, e PATCH ignora o que não vem. Omitir o
    # `due` quando a pessoa tira o prazo no Tinto deixava o prazo intacto
    # do outro lado: a tela dizia "sem data", o cartão obedecia, e o pull
    # seguinte trazia a data de volta — o aparelho desfazendo sozinho o
    # que a pessoa acabou de fazer.
    prazo = g.p or g.d
    corpo["due"] = f"{prazo}T00:00:00.000Z" if prazo else None
    return corpo


def id_real(item_id: str) -> str:
    """`g:a1b2c3` → `a1b2c3`. O que o Google conhece."""
    return item_id.split(":", 1)[-1]
