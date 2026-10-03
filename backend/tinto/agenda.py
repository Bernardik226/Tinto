"""As chamadas ao Google, e o estado que elas exigem."""

import asyncio
import json
import logging
from datetime import datetime, timedelta, timezone
from urllib.parse import quote
from zoneinfo import ZoneInfo

import httpx

from .contrato import Item, Removido, Tipo
from .google import (AGENDA_TINTO, _dia, _prazo_da_tarefa, com_excecoes,
                     dia_feito, corpo_de_evento, corpo_de_tarefa, duracao_s,
                     casa_marca, evento_para_item, id_real,
                     lista_para_item, marca, regra_do_evento,
                     tarefa_para_item, vem_desligada)

CALENDAR = "https://www.googleapis.com/calendar/v3"
TASKS = "https://tasks.googleapis.com/tasks/v1"

# O buraco por onde o teste entra.
transporte = None

log = logging.getLogger("tinto.agenda")


def recusou(r: httpx.Response, onde: str) -> bool:
    """O Google disse não? Então DIGA POR QUÊ, alto."""
    if r.status_code == 200:
        return False

    motivo = ""
    try:
        motivo = r.json().get("error", {}).get("message", "")
    except ValueError:
        motivo = (r.text or "")[:200]

    log.warning("google recusou %s: %s %s", onde, r.status_code, motivo)
    return True


def _cliente(access: str) -> httpx.AsyncClient:
    return httpx.AsyncClient(
        headers={"Authorization": f"Bearer {access}"},
        timeout=15,
        transport=transporte,
    )


def _url(pedaco: str) -> str:
    """Um id do Google dentro de uma URL, codificado."""
    return quote(pedaco, safe="@")


def _agora() -> datetime:
    return datetime.now(timezone.utc)


def _iso(quando: datetime) -> str:
    """RFC 3339 com `Z`, que é o que as duas APIs aceitam sem discutir."""
    return quando.astimezone(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def _inicio_de_ontem(tz_nome: str) -> datetime:
    """Meia-noite de ontem para a pessoa, devolvida como instante."""
    try:
        zona = ZoneInfo(tz_nome) if tz_nome else timezone.utc
    except Exception:
        zona = timezone.utc
    local = _agora().astimezone(zona)
    ontem = (local.date() - timedelta(days=1))
    return datetime.combine(ontem, datetime.min.time(), tzinfo=zona)


# ── a JANELA, e por que ela é de três dias ───────────────────────────
# O Tinto é **ontem, hoje e amanhã**. É o que uma tela de tinta na mesa faz
# melhor que um celular, e é o escopo inteiro do aparelho.
JANELA_DIAS = 1


def _fim_de_amanha(tz_nome: str) -> datetime:
    """O último instante que a janela cobre."""
    try:
        zona = ZoneInfo(tz_nome) if tz_nome else timezone.utc
    except Exception:
        zona = timezone.utc
    local = _agora().astimezone(zona)
    depois = local.date() + timedelta(days=JANELA_DIAS + 1)
    return datetime.combine(depois, datetime.min.time(), tzinfo=zona)


# ── as agendas ───────────────────────────────────────────────────────
async def todas_as_agendas(cliente: httpx.AsyncClient) -> list[dict]:
    """A lista crua do Google, sem curadoria. É o que T-32 mostra."""
    r = await cliente.get(f"{CALENDAR}/users/me/calendarList")
    return [] if recusou(r, "calendarList") else r.json().get("items", [])


def esta_ligada(cal: dict, escolhas: dict) -> bool:
    """Esta agenda entra no `pull`?"""
    cal_id = cal.get("id", "")
    if cal_id in escolhas:
        return escolhas[cal_id]
    if cal.get("selected") is False:
        return False
    return not vem_desligada(cal_id)


async def _agendas(cliente: httpx.AsyncClient,
                   sinc) -> tuple[list[dict], str, set | None]:
    """As agendas que valem a tela, o fuso da conta, e TODAS as que existem."""
    dados = await todas_as_agendas(cliente)
    if not dados:
        # Sem a lista ainda dá para mostrar o dia. `primary` sempre existe,
        # e um pull pobre é melhor que um pull vazio.
        return [{"id": "primary"}], "", None

    fuso = ""
    escolhidas = []

    for cal in dados:
        if cal.get("primary"):
            fuso = cal.get("timeZone", "") or fuso
        if esta_ligada(cal, sinc.escolhas):
            escolhidas.append(cal)

    existem = {cal.get("id", "") for cal in dados} | {"primary"}
    return escolhidas or [{"id": "primary"}], fuso, existem


async def agenda_do_tinto(cliente: httpx.AsyncClient, sinc,
                          tz_nome: str = "") -> str:
    """O id da agenda "Tinto", criando-a se ela não existir."""
    if sinc.agenda_tinto:
        return sinc.agenda_tinto

    r = await cliente.get(f"{CALENDAR}/users/me/calendarList")
    if r.status_code == 200:
        for cal in r.json().get("items", []):
            if cal.get("summary") == AGENDA_TINTO:
                sinc.agenda_tinto = cal.get("id", "")
                return sinc.agenda_tinto

    # COM FUSO. Uma agenda criada sem `timeZone` nasce em UTC, e aí todo
    # evento que o Tinto escrever volta expresso em UTC — três horas
    # adiantado para quem está em Brasília, num aparelho cuja única
    # promessa é mostrar a que horas as coisas são.
    corpo = {"summary": AGENDA_TINTO}
    if tz_nome:
        corpo["timeZone"] = tz_nome

    r = await cliente.post(f"{CALENDAR}/calendars", json=corpo)
    if r.status_code in (200, 201):
        sinc.agenda_tinto = r.json().get("id", "")
    return sinc.agenda_tinto


# ── o delta ──────────────────────────────────────────────────────────
def chave_de_tarefa(titulo: str, dia: str) -> str:
    """Como uma tarefa e o evento dela se reconhecem: título + dia."""
    return f"{(titulo or '').strip().casefold()}|{(dia or '')[:10]}"


def _buraco_na_serie(sinc, serie: str, ev: dict, tz_nome: str) -> None:
    """Aquele dia deixou de ser da série."""
    original = ev.get("originalStartTime") or {}
    dia = original.get("date") or _dia(original, tz_nome)
    if not dia:
        return

    buracos = sinc.buracos.setdefault(serie, [])
    if dia not in buracos:
        buracos.append(dia)
        # ponytail: a lista cresce com o uso e nada a poda. Uma série com
        # centenas de buracos é conversa para o dia em que existir — o
        # campo do contrato corta bem antes disso.


async def _regra_com_fim(cliente: httpx.AsyncClient, cal_id: str,
                         ev: dict, sinc, tz_nome: str) -> str:
    """A regra pronta para o device: com os buracos e com uma DATA de fim."""
    regra = regra_do_evento(ev)
    if not regra:
        return ""

    corte = regra.find("|c=")
    if corte >= 0:
        quantas = int(regra[corte + 3:].split("|")[0] or 0)
        ultima = await _ultima_ocorrencia(cliente, cal_id, ev.get("id", ""),
                                          quantas, tz_nome)
        regra = regra[:corte] + (f"|u={ultima.replace('-', '')}"
                                 if ultima else "")

    return com_excecoes(regra, sinc.buracos.get(ev.get("id", ""), []))


async def _ultima_ocorrencia(cliente: httpx.AsyncClient, cal_id: str,
                             ev_id: str, quantas: int, tz_nome: str) -> str:
    """O dia da última vez que a rotina acontece. Vazio se não der para saber."""
    if quantas <= 0 or quantas > 250:
        return ""

    r = await cliente.get(
        f"{CALENDAR}/calendars/{_url(cal_id)}/events/{_url(ev_id)}/instances",
        params={"maxResults": quantas, "showDeleted": "false"})
    if recusou(r, f"fim da série {ev_id}"):
        return ""

    itens = r.json().get("items", [])
    if not itens:
        return ""
    return _dia(itens[-1].get("start", {}) or {}, tz_nome)


async def _proxima_ocorrencia(cliente: httpx.AsyncClient, cal_id: str,
                              ev_id: str, tz_nome: str) -> str:
    """O primeiro dia em que a rotina ainda acontece. Vazio se acabou."""
    r = await cliente.get(
        f"{CALENDAR}/calendars/{_url(cal_id)}/events/{_url(ev_id)}/instances",
        params={"maxResults": 1, "showDeleted": "false",
                "timeMin": _iso(_inicio_de_ontem(tz_nome))})
    if recusou(r, f"instâncias de {ev_id}"):
        return ""

    for instancia in r.json().get("items", []):
        return _dia(instancia.get("start", {}) or {}, tz_nome)
    return ""


async def _eventos(cliente: httpx.AsyncClient, cal_id: str, sinc,
                   tz_nome: str = "",
                   agenda: str = "",
                   tarefas: dict | None = None,
                   absorvidas: set | None = None) -> tuple[list[Item], list[str]]:
    """O que mudou numa agenda desde a última vez."""
    token = sinc.tokens.get(cal_id, "")
    itens: list[Item] = []
    fora: list[str] = []
    pagina = ""

    # As séries que ganharam um buraco neste delta. O mestre delas pode
    # não ter vindo — apagar uma ocorrência não mexe no mestre —, e sem
    # reemitir a regra o device continuaria mostrando o dia apagado.
    remendar: set[str] = set()

    while True:
        # ── a série vem como REGRA, não como noventa ocorrências ─────
        # Com `singleEvents=true` o Google expande tudo: "todo dia" chega
        # como uma ocorrência por dia, e cada uma virava um item, uma
        # escrita no cartão e uma repintura de tinta no vidro.
        params: dict = {"singleEvents": "false", "maxResults": 250}
        if token:
            params["syncToken"] = token
            params["showDeleted"] = "true"
        else:
            # **Sem `orderBy`.** O Google não devolve `nextSyncToken` para
            # um pedido que ordena — e sem token não existe sync
            # incremental: todo pull refaz a leitura inteira, e o aparelho
            # reescreve o cartão a cada quinze minutos, para sempre.
            params["showDeleted"] = "false"
            params["timeMin"] = _iso(_inicio_de_ontem(tz_nome))
            params["timeMax"] = _iso(_fim_de_amanha(tz_nome))
        if pagina:
            params["pageToken"] = pagina

        r = await cliente.get(f"{CALENDAR}/calendars/{_url(cal_id)}/events",
                              params=params)

        if r.status_code == 410:
            # Token expirado. Zera e recomeça do zero, uma vez só — o
            # `token = ""` faz a volta cair no ramo da janela de tempo.
            sinc.tokens.pop(cal_id, None)
            token, pagina = "", ""
            # E o que já tinha vindo desta agenda se perde de propósito:
            # a leitura vai recomeçar do zero, e manter a metade anterior
            # seria entregar o mesmo evento duas vezes.
            itens, fora = [], []
            continue

        if recusou(r, f"events de {cal_id}"):
            return itens, fora

        corpo = r.json()
        vistos_crus = 0
        for ev in corpo.get("items", []):
            # O evento INTEIRO, os primeiros de cada colheita.
            if vistos_crus < 3:
                vistos_crus += 1
                log.info("evento cru: %s", json.dumps(ev)[:700])

            marcado = marca("g", ev.get("id", ""))

            # ── esta é uma ocorrência SOLTA de uma série? ────────────
            # O Google não guarda "a terça que eu apaguei" dentro da
            # regra: ele manda um evento à parte, com `recurringEventId`
            # apontando para a série e `originalStartTime` dizendo qual
            # dia era. Cancelado quer dizer apagado; confirmado quer
            # dizer movido ou editado só naquele dia.
            serie = ev.get("recurringEventId", "")
            if serie:
                _buraco_na_serie(sinc, serie, ev, tz_nome)
                remendar.add(serie)

            if ev.get("status") == "cancelled":
                fora.append(marcado)
                sinc.onde.pop(marcado, None)
                sinc.reais.pop(marcado, None)

                # Se ele era o GÊMEO de uma tarefa, o par se desfaz: a
                # tarefa perdeu a hora — virou "dia inteiro" no Google —
                # e continua existindo sozinha. Guardar o endereço de um
                # evento morto faria a próxima mudança de hora escrever
                # num id que já não existe.
                for tid, endereco in list(sinc.gemeos.items()):
                    if endereco.endswith(f"|{ev.get('id', '')}"):
                        sinc.gemeos.pop(tid, None)
                continue

            # A REGRA da rotina, com os buracos e com o fim em data.
            regra = await _regra_com_fim(cliente, cal_id, ev, sinc, tz_nome)

            it = evento_para_item(ev, tz_nome, agenda, regra)
            if not it:
                continue

            # ── a ÂNCORA da rotina anda com o tempo ──────────────────
            # O mestre de "toda segunda" pode ter começado em 2019, e o
            # device grava o item na pasta do dia dele: a rotina moraria
            # numa pasta de sete anos atrás, longe de qualquer varredura.
            if regra:
                proxima = await _proxima_ocorrencia(
                    cliente, cal_id, ev.get("id", ""), tz_nome)
                if proxima:
                    it.d = proxima

            # ── é uma TAREFA vestida de evento? ──────────────────────
            # A tarefa com data e hora aparece no Google Agenda, e é por
            # aqui que a hora dela chega: arrastá-la lá muda o item aqui.
            # Ela TAMBÉM desce pelo Tasks, sem hora — a mesma coisa,
            # duas portas.
            endereco = f"{cal_id}|{ev.get('id', '')}"
            tid = next((tid for tid, salvo in sinc.gemeos.items()
                        if salvo == endereco), "")
            gemea = next((t for t in (tarefas or {}).values()
                          if t["id"] == tid), None) if tid else None
            if not gemea:
                gemea = (tarefas or {}).get(chave_de_tarefa(it.t, it.d))
            if gemea:
                prazo = gemea["d"]
                it.id = gemea["id"]
                it.tp = Tipo.TAREFA
                it.ok = gemea["ok"]
                it.c  = gemea["c"]
                it.p  = prazo if prazo != it.d else ""
                it.l  = gemea["lista_nome"][:47]
                if absorvidas is not None:
                    absorvidas.add(gemea["id"])
                sinc.onde[it.id]  = gemea["lista"]
                sinc.reais[it.id] = gemea["real"]

                # O endereço do GÊMEO: é ele que sabe de hora. Mudar o
                # horário no Tinto tem de chegar AQUI — o Tasks descarta
                # o horário, e mandar para lá muda no vidro e não no
                # Google.
                sinc.gemeos[it.id] = f"{cal_id}|{ev.get('id', '')}"
                log.info("híbrido: %r · %s %s", it.t, it.d, it.h)

            else:
                # De que agenda ele veio. É o que permite mudar a data de
                # um evento sem adivinhar onde ele mora — e adivinhar,
                # aqui, é um 404 que a pessoa lê como "não deu".
                sinc.onde[it.id] = cal_id
                # E o id INTEIRO: `it.id` cabe em 39 caracteres, o do
                # Google não. É por esta tabela que o apagar acha o evento.
                sinc.reais[it.id] = ev.get("id", "")

            itens.append(it)

        pagina = corpo.get("nextPageToken", "")
        if not pagina:
            novo = corpo.get("nextSyncToken", "")
            if not novo:
                # Sem token não há incremental, e o próximo pull traria
                # tudo de novo. Dizer isso alto é o que impede o defeito
                # de virar "o aparelho é lento" em vez de "falta um
                # parâmetro".
                log.warning("google não devolveu nextSyncToken para %s — "
                            "o próximo pull vai reler tudo", cal_id)
            sinc.tokens_novos[cal_id] = novo

            # ── as séries que ficaram com um buraco ──────────────────
            # Apagar a terça que vem não mexe no evento mestre, então ele
            # não volta no delta — e a regra que o device tem continua
            # dizendo "toda terça". Reemitir o mestre é o que faz o
            # buraco chegar lá.
            ja = {it.id for it in itens}
            for serie in remendar:
                if marca("g", serie) in ja:
                    continue

                rm = await cliente.get(
                    f"{CALENDAR}/calendars/{_url(cal_id)}/events/{_url(serie)}")
                if recusou(rm, f"mestre {serie}"):
                    continue

                mestre = rm.json()
                if mestre.get("status") == "cancelled":
                    # A série inteira acabou: o mestre já desce como
                    # `cancelled` pelo delta, e quem apaga é o `fora`.
                    continue

                regra = await _regra_com_fim(cliente, cal_id, mestre,
                                             sinc, tz_nome)
                remendado = evento_para_item(mestre, tz_nome, agenda, regra)
                if not remendado:
                    continue

                if regra:
                    proxima = await _proxima_ocorrencia(
                        cliente, cal_id, serie, tz_nome)
                    if proxima:
                        remendado.d = proxima
                    else:
                        # Não sobrou ocorrência nenhuma: a rotina acabou,
                        # e o que resta no cartão tem de sair.
                        fora.append(remendado.id)
                        continue

                sinc.onde[remendado.id] = cal_id
                sinc.reais[remendado.id] = serie
                itens.append(remendado)

            return itens, fora


# ── o que SUMIU do Google sai do aparelho ────────────────────────────
# O Google é a fonte da verdade. Uma lista do Tasks ou uma agenda que foi
# apagada (ou deixada) some da resposta dele sem deixar rastro — e o que
# estava dentro dela ficava no aparelho até o resync diário.
async def _o_que_sumiu(cliente, sinc, existem: set,
                      acompanhados: dict) -> list[str]:
    fora: list[str] = []
    for origem, no_aparelho in acompanhados.items():
        if not origem or origem in existem:
            continue
        if no_aparelho:
            fora.append(no_aparelho)
        for iid in [i for i, onde in sinc.onde.items()
                    if onde == origem and i != no_aparelho]:
            fora.append(iid)
            sinc.onde.pop(iid, None)
            sinc.reais.pop(iid, None)
            await _apaga_gemeo(cliente, sinc, iid)
        for mapa in (sinc.contagens, sinc.onde, sinc.reais):
            mapa.pop(no_aparelho, None)
        sinc.tokens.pop(origem, None)
        log.info("sumiu do Google: %s", no_aparelho or origem)
    return fora


async def _tarefas(cliente: httpx.AsyncClient, sinc, tz_nome: str = "",
                   indice: dict | None = None
                   ) -> tuple[list[Item], list[str], list[Item]]:
    """As listas e as tarefas, e o que saiu de dentro delas."""
    itens: list[Item] = []
    fora: list[str] = []

    # Todas as listas, tenham mudado ou não. Quem decide se elas pegam
    # carona é o `delta`.
    listas: list[Item] = []

    r = await cliente.get(f"{TASKS}/users/@me/lists")
    if recusou(r, "tasklists"):
        return itens, fora, listas

    desde = sinc.tarefas_desde
    mais_novo = desde

    for tl in r.json().get("items", []):
        lista_id = tl.get("id", "")
        cruas: list[dict] = []
        pagina = ""
        falhou = False
        while True:
            params = {"showCompleted": "true", "showHidden": "true",
                      "showDeleted": "true", "maxResults": 100}
            if pagina:
                params["pageToken"] = pagina
            rt = await cliente.get(f"{TASKS}/lists/{_url(lista_id)}/tasks",
                                   params=params)
            if recusou(rt, f"tasks de {lista_id}"):
                falhou = True
                break
            corpo = rt.json()
            cruas.extend(corpo.get("items", []))
            pagina = corpo.get("nextPageToken", "")
            if not pagina:
                break

        # Falha de rede não é lista vazia, e a diferença é cara.
        if falhou:
            continue

        # ── quantas vieram, e quantas passaram ───────────────────────
        # "0 itens" no log do pull não distingue "esta conta não tem
        # tarefa" de "as tarefas foram descartadas no caminho" — e é
        # exatamente essa distinção que decide se o defeito é aqui ou no
        # vidro. O `due` da primeira vai junto: é ele que diz se o Google
        # manda hora, que é a pergunta aberta do híbrido.
        antes_de_descer = len(itens)

        # A contagem é sobre o que EXISTE, e apagada não existe.
        vivas = [t for t in cruas if not t.get("deleted")]
        feitas = sum(1 for t in vivas if t.get("status") == "completed")

        marcada = marca("l", lista_id)
        sinc.onde[marcada] = lista_id
        sinc.reais[marcada] = lista_id

        # A lista só desce quando a contagem MUDA. É o que faz um pull sem
        # novidade voltar de fato vazio — e um pull vazio é o que deixa o
        # vidro parado quando nada aconteceu.
        listas.append(lista_para_item(tl, len(vivas), feitas))
        agora = f"{len(vivas)},{feitas},{tl.get('title', '')}"
        antes = sinc.contagens.get(marcada, "")

        # RENOMEADA no Google: o Tasks não mexe nas tarefas quando só a
        # lista muda de nome, e cada uma leva o nome dela — que é por onde
        # o aparelho agrupa. Elas descem de novo, com o nome novo.
        renomeou = bool(antes) and \
            antes.split(",", 2)[-1] != tl.get("title", "")
        if antes != agora:
            sinc.contagens[marcada] = agora
            itens.append(lista_para_item(tl, len(vivas), feitas))

        for t in cruas:
            quando = t.get("updated") or ""
            mais_novo = max(mais_novo, quando)

            id_marcado = marca("t", t.get("id", ""))

            # ── o ÍNDICE, para o evento se reconhecer ────────────────
            # Ele é montado ANTES do filtro de novidade, e é por isso que
            # ele funciona: a tarefa que não mudou hoje continua sendo a
            # gêmea do evento que mudou. Fechar o par depende de conhecer
            # TODAS as tarefas, não só as que o delta vai mandar.
            if indice is not None and t.get("due") and not t.get("deleted"):
                dia_t, _ = _prazo_da_tarefa(t.get("due") or "", tz_nome)
                indice[chave_de_tarefa(t.get("title", ""), dia_t)] = {
                    "id": id_marcado,
                    "real": t.get("id", ""),
                    "lista": lista_id,
                    "lista_nome": tl.get("title", ""),
                    "ok": t.get("status") == "completed",
                    "c": dia_feito(t.get("completed") or "", tz_nome),
                    "d": dia_t,
                }

            # Mudou depois da última visita? Se não, o device já tem.
            if desde and quando <= desde and \
                    (not renomeou or t.get("deleted")):
                continue

            if t.get("deleted"):
                fora.append(id_marcado)
                sinc.onde.pop(id_marcado, None)
                sinc.reais.pop(id_marcado, None)
                continue

            # O FUSO vai junto: tarefa com hora é o híbrido, e a hora
            # tem de chegar ao vidro já em hora local — o device nunca
            # converte (SISTEMA §2).
            it = tarefa_para_item(t, tl.get("title", ""), tz_nome)
            if it:
                itens.append(it)
                sinc.onde[id_marcado] = lista_id
                sinc.reais[id_marcado] = t.get("id", "")

        # Quantas VIERAM e quantas passaram. "0 itens" no log do pull não
        # distingue "esta conta não tem tarefa" de "as tarefas foram
        # descartadas no caminho" — e é essa distinção que decide se o
        # defeito é do servidor ou do vidro.
        log.info("tarefas de %s: %d cruas · %d desceram · desde=%r",
                 tl.get("title", lista_id), len(cruas),
                 len(itens) - antes_de_descer, desde)

    # O marcador é o `updated` mais novo que se VIU, e não a hora de
    # agora. São dois formatos diferentes de RFC 3339 — o Google manda
    # milissegundos, `_iso` não —, e comparar texto entre eles erra por um
    # segundo em silêncio. Comparar o do Google com o do Google não erra.
    vistas = {tl.get("id", "") for tl in r.json().get("items", [])}
    fora += await _o_que_sumiu(cliente, sinc, vistas, {
        sinc.onde.get(k, ""): k for k in sinc.contagens if k.startswith("l:")})

    sinc.tarefas_novo = mais_novo
    return itens, fora, listas


async def marcas_do_mes(access: str, sinc, ano: int, mes: int,
                        tz_nome: str = "") -> tuple[int, int]:
    """Quais dias do mês têm evento e quais têm tarefa. Dois inteiros."""
    from calendar import monthrange

    try:
        zona = ZoneInfo(tz_nome) if tz_nome else timezone.utc
    except Exception:
        zona = timezone.utc

    ultimo = monthrange(ano, mes)[1]
    inicio = datetime(ano, mes, 1, tzinfo=zona)
    fim = datetime(ano, mes, ultimo, 23, 59, 59, tzinfo=zona)

    eventos = tarefas = 0

    async with _cliente(access) as cliente:
        # As agendas que a PESSOA escolheu, em Sincronização. A grade do
        # mês mostra o que ela ligou — nem mais, nem menos.
        agendas, fuso_cal, _ = await _agendas(cliente, sinc)
        fuso = tz_nome or fuso_cal or await _fuso_da_conta(cliente)

        pedidos = [cliente.get(
            f"{CALENDAR}/calendars/{_url(cal.get('id', 'primary'))}/events",
            params={"singleEvents": "true", "maxResults": 250,
                    "showDeleted": "false",
                    "timeMin": _iso(inicio), "timeMax": _iso(fim)})
            for cal in agendas]

        for r in await asyncio.gather(*pedidos):
            if recusou(r, "marcas do mês"):
                continue
            for ev in r.json().get("items", []):
                it = evento_para_item(ev, fuso)
                if not it or not it.d:
                    continue
                # O intervalo inteiro do que dura mais de um dia: a viagem
                # de 14 a 17 ocupa quatro dias na grade, não um.
                primeiro = it.d
                ultimo_dia = it.p or it.d
                dia = datetime.fromisoformat(primeiro).date()
                fim_dia = datetime.fromisoformat(ultimo_dia).date()
                while dia <= fim_dia:
                    if dia.year == ano and dia.month == mes:
                        eventos |= 1 << (dia.day - 1)
                    dia += timedelta(days=1)

        r = await cliente.get(f"{TASKS}/users/@me/lists")
        if not recusou(r, "tasklists das marcas"):
            listas = r.json().get("items", [])
            pedidos = [cliente.get(f"{TASKS}/lists/{_url(tl.get('id', ''))}/tasks",
                                   params={"showCompleted": "true",
                                           "showHidden": "true",
                                           "maxResults": 100})
                       for tl in listas]
            for rt in await asyncio.gather(*pedidos):
                if recusou(rt, "tarefas das marcas"):
                    continue
                for t in rt.json().get("items", []):
                    dia_t, _ = _prazo_da_tarefa(t.get("due") or "", fuso)
                    feito = dia_feito(t.get("completed") or "", fuso)
                    for quando in (dia_t, feito):
                        if len(quando) == 10 and quando[:7] == f"{ano:04d}-{mes:02d}":
                            tarefas |= 1 << (int(quando[8:10]) - 1)

    # Quantas agendas e quantos dias acenderam. Sem isto, "o feriado não
    # apareceu" não distingue agenda de fora da curadoria, mês sem nada e
    # resposta recusada pelo Google.
    log.info("marcas %04d-%02d · %d agendas · %d dias com evento, %d com tarefa",
             ano, mes, len(agendas), bin(eventos).count("1"),
             bin(tarefas).count("1"))

    return eventos, tarefas


async def itens_do_dia(access: str, sinc, dia: str,
                       tz_nome: str = "") -> list[Item]:
    """O que acontece num dia FORA da janela — buscado quando a pessoa abre."""
    try:
        zona = ZoneInfo(tz_nome) if tz_nome else timezone.utc
    except Exception:
        zona = timezone.utc

    try:
        quando = datetime.fromisoformat(dia).replace(tzinfo=zona)
    except ValueError:
        return []

    itens: list[Item] = []

    async with _cliente(access) as cliente:
        # As mesmas das marcas, pelo mesmo motivo: o pontinho que a grade
        # mostrou tem de abrir com o que ele prometeu, e some junto quando
        # a pessoa desliga aquela agenda.
        agendas, fuso_cal, _ = await _agendas(cliente, sinc)
        fuso = tz_nome or fuso_cal or await _fuso_da_conta(cliente)

        pedidos = [cliente.get(
            f"{CALENDAR}/calendars/{_url(cal.get('id', 'primary'))}/events",
            params={"singleEvents": "true", "maxResults": 50,
                    "orderBy": "startTime", "showDeleted": "false",
                    "timeMin": _iso(quando),
                    "timeMax": _iso(quando + timedelta(days=1))})
            for cal in agendas]

        for cal, r in zip(agendas, await asyncio.gather(*pedidos)):
            if recusou(r, f"dia {dia}"):
                continue
            for ev in r.json().get("items", []):
                it = evento_para_item(ev, fuso, cal.get("summary", ""))
                if it:
                    itens.append(it)

        # ── e as TAREFAS daquele dia ─────────────────────────────────
        # O calendário marca o dia com dois marcadores — evento e tarefa —,
        # e abrir um dia que só tinha tarefa mostrava uma tela vazia. Marca
        # que promete conteúdo tem de entregá-lo, e o Tasks é metade do que
        # o Google chama de agenda.
        r = await cliente.get(f"{TASKS}/users/@me/lists")
        if not recusou(r, "tasklists do dia"):
            listas = r.json().get("items", [])
            pedidos = [cliente.get(f"{TASKS}/lists/{_url(tl.get('id', ''))}/tasks",
                                   params={"showCompleted": "true",
                                           "showHidden": "true",
                                           "maxResults": 100})
                       for tl in listas]
            for tl, rt in zip(listas, await asyncio.gather(*pedidos)):
                if recusou(rt, "tarefas do dia"):
                    continue
                for t in rt.json().get("items", []):
                    if t.get("deleted"):
                        continue
                    prazo, _ = _prazo_da_tarefa(t.get("due") or "", fuso)
                    feito = dia_feito(t.get("completed") or "", fuso)
                    # Pelo PRAZO ou pela CONCLUSÃO — as duas põem a tarefa
                    # naquele dia, e são as duas perguntas que se faz
                    # olhando um dia: o que eu tinha de fazer, e o que fiz.
                    if dia not in (prazo, feito):
                        continue
                    it = tarefa_para_item(t, tl.get("title", ""), fuso)
                    if it:
                        itens.append(it)

    return itens[:20]


async def panorama(access: str, sinc, tz_nome: str = "",
                   teto: int = 40) -> list[Item]:
    """O que EXISTE agora, para a LLM poder apontar em vez de adivinhar."""
    itens: list[Item] = []

    async with _cliente(access) as cliente:
        agendas, fuso, _ = await _agendas(cliente, sinc)
        if not fuso:
            fuso = await _fuso_da_conta(cliente)
        if tz_nome:
            fuso = tz_nome

        pedidos_eventos = []
        for cal in agendas:
            cal_id = cal.get("id", "primary")
            pedidos_eventos.append(cliente.get(
                f"{CALENDAR}/calendars/{_url(cal_id)}/events",
                params={"singleEvents": "true", "maxResults": 100,
                        "orderBy": "startTime", "showDeleted": "false",
                        "timeMin": _iso(_inicio_de_ontem(fuso)),
                        "timeMax": _iso(_agora() + timedelta(days=30))}))
        lista_de_tarefas = asyncio.create_task(
            cliente.get(f"{TASKS}/users/@me/lists"))

        for cal, r in zip(agendas, await asyncio.gather(*pedidos_eventos)):
            cal_id = cal.get("id", "primary")
            if recusou(r, f"panorama de {cal_id}"):
                continue

            for ev in r.json().get("items", []):
                it = evento_para_item(ev, fuso, cal.get("summary", ""))
                if not it:
                    continue
                itens.append(it)
                # A tabela aprende aqui também: é a mesma leitura, e é o
                # que faz o apagar por voz achar o id inteiro depois.
                sinc.onde[it.id] = cal_id
                sinc.reais[it.id] = ev.get("id", "")

        r = await lista_de_tarefas
        if not recusou(r, "tasklists"):
            listas = r.json().get("items", [])
            pedidos = []
            for tl in listas:
                lista_id = tl.get("id", "")
                pedidos.append(cliente.get(
                    f"{TASKS}/lists/{_url(lista_id)}/tasks",
                    params={"showCompleted": "false", "maxResults": 50}))
            for tl, rt in zip(listas, await asyncio.gather(*pedidos)):
                lista_id = tl.get("id", "")
                if recusou(rt, f"panorama de {lista_id}"):
                    continue
                for t in rt.json().get("items", []):
                    it = tarefa_para_item(t, tl.get("title", ""), fuso)
                    if not it:
                        continue
                    itens.append(it)
                    sinc.onde[it.id] = lista_id
                    sinc.reais[it.id] = t.get("id", "")

    return itens[:teto]


async def delta(access: str, sinc) -> tuple[list[Item], list[Removido], str]:
    """Tudo o que mudou: eventos, tarefas e listas. E o fuso."""
    itens: list[Item] = []
    fora: list[str] = []

    async with _cliente(access) as cliente:
        agendas, fuso, existem = await _agendas(cliente, sinc)

        # O fuso primeiro, e depois os eventos: eles são convertidos para
        # ele na tradução, e sem o nome em mãos a conversão não acontece.
        if not fuso:
            fuso = await _fuso_da_conta(cliente)

        # ── as TAREFAS primeiro, e a ordem é o conserto ──────────────
        # A tarefa com data e hora aparece nas DUAS pontas: é uma `task`
        # no Tasks — sem hora, porque a API descarta o horário — e um
        # item no Calendar, com hora. Colhidas separadas, ela chegava ao
        # vidro como duas coisas: um evento que não se marca e uma tarefa
        # que não tem hora.
        indice: dict = {}
        absorvidas: set = set()

        tarefas_itens, f, listas = await _tarefas(cliente, sinc, fuso, indice)
        fora += f

        for cal in agendas:
            # O NOME da agenda desce junto: o detalhe do item mostra
            # "AGENDA · Trabalho", e o id é um e-mail de sessenta
            # caracteres que não diz nada a ninguém.
            i, f = await _eventos(cliente, cal.get("id", "primary"), sinc,
                                  fuso, cal.get("summary", ""),
                                  indice, absorvidas)
            itens += i
            fora += f

        if existem is not None:
            agendas_vistas = set(sinc.tokens) | {
                c for i, c in sinc.onde.items() if i.startswith("g:")}
            fora += await _o_que_sumiu(cliente, sinc, existem,
                                      {c: "" for c in agendas_vistas})

        # O Calendar é incremental e o Tasks não. Marcar ou desmarcar uma
        # tarefa altera só a metade Tasks; o evento-gêmeo que guarda a hora
        # não volta pelo syncToken porque não mudou. Se deixarmos a versão
        # crua do Tasks descer, ela sobrescreve o híbrido no cartão sem `h`
        # e a linha cai da régua para o bloco de tarefas.
        for it in tarefas_itens:
            if it.id in absorvidas:
                continue
            endereco = sinc.gemeos.get(it.id, "")
            if not endereco or "|" not in endereco:
                continue

            cal_id, ev_id = endereco.split("|", 1)
            r = await cliente.get(
                f"{CALENDAR}/calendars/{_url(cal_id)}/events/{_url(ev_id)}")
            if r.status_code == 404:
                sinc.gemeos.pop(it.id, None)
                continue
            if recusou(r, f"gêmeo de {it.id}"):
                continue

            metade_hora = evento_para_item(r.json(), fuso)
            if not metade_hora:
                continue
            it.h = metade_hora.h
            it.f = metade_hora.f
            if it.d != metade_hora.d:
                it.p = it.d
            it.d = metade_hora.d
            it.di = metade_hora.di
            itens.append(it)
            absorvidas.add(it.id)

        # A tarefa que virou híbrido não desce também sozinha: seria a
        # mesma coisa duas vezes na tela, e marcar uma deixaria a outra
        # aberta — que é exatamente o defeito que isto conserta.
        itens += [it for it in tarefas_itens if it.id not in absorvidas]

        # ── a carona das listas ──────────────────────────────────────
        # Se já há novidade para mandar, as listas vão junto — custam dois
        # ou três itens e o device ignora o que não mudou. Sozinhas elas
        # não acordam ninguém: um pull sem novidade continua voltando
        # vazio, que é o que deixa o vidro parado quando nada aconteceu.
        if itens:
            ja = {it.id for it in itens}
            itens += [li for li in listas if li.id not in ja]

    # Sem duplicata: um id que entrou e saiu no mesmo delta saiu.
    entrando = {it.id for it in itens}
    return itens, [Removido(id=x) for x in dict.fromkeys(fora)
                   if x not in entrando], fuso


async def catalogo(access: str, sinc) -> tuple[list[dict], int]:
    """As agendas da conta, com o interruptor de cada uma, e quantas a
    conta tem — o corte em doze não pode sumir com as outras caladas. É T-32.
    """
    async with _cliente(access) as cliente:
        # Doze é o teto do device (`AGENDAS_MAX`), e o corte é aqui pelo
        # mesmo motivo de sempre: o que ele recebe e joga fora é RAM que
        # faltou pro que importa. A principal nunca fica de fora do corte.
        cruas = await todas_as_agendas(cliente)

    cruas.sort(key=lambda c: (not c.get("primary"), c.get("summary", "")))

    # A PRINCIPAL não se chama pelo e-mail.
    def nome_de(c):
        if c.get("summaryOverride"):
            return c["summaryOverride"]
        if c.get("primary"):
            return "Agenda principal"
        return c.get("summary") or ""

    lista = [{"id": c.get("id", ""), "t": nome_de(c)[:31],
              "on": esta_ligada(c, sinc.escolhas), "py": 0}
             for c in cruas[:12]]
    return lista, len(cruas)


async def _fuso_da_conta(cliente: httpx.AsyncClient) -> str:
    """O nome IANA do fuso, para o dia em que a agenda não disser."""
    try:
        r = await cliente.get(f"{CALENDAR}/users/me/settings/timezone")
        if not recusou(r, "fuso"):
            return r.json().get("value", "") or ""
    except httpx.HTTPError:
        pass
    return ""


def fuso_em_minutos(nome: str) -> int:
    """"America/Sao_Paulo" → -180.
    """
    if not nome:
        return 0
    try:
        from zoneinfo import ZoneInfo

        desloc = datetime.now(ZoneInfo(nome)).utcoffset()
        return int(desloc.total_seconds() // 60) if desloc else 0
    except Exception:
        return 0


# ── um gesto ─────────────────────────────────────────────────────────
def _resposta(r: httpx.Response, esperados: tuple[int, ...]) -> dict:
    """A resposta do Google → o `{ok}` que o device lê."""
    ok = r.status_code in esperados
    try:
        corpo = r.json() if r.content else {}
    except ValueError:
        corpo = {}

    saida = {"ok": ok, "id": corpo.get("id", "") if ok else ""}
    if not ok:
        saida["motivo"] = (corpo.get("error", {}).get("message")
                           or f"http {r.status_code}")[:120]
    return saida


async def aplica_gesto(access: str, g, sinc, tz_nome: str = "") -> dict:
    """Um gesto do device → uma chamada ao Google."""
    async with _cliente(access) as cliente:
        if g.tp == Tipo.LISTA:
            return await _gesto_de_lista(cliente, g, sinc)
        if g.tp == Tipo.TAREFA:
            return await _gesto_de_tarefa(cliente, g, sinc, tz_nome)
        return await _gesto_de_evento(cliente, g, sinc, tz_nome)


def _real_conhecido(g, sinc) -> str:
    """O id do Google por inteiro, pela tabela que o pull escreveu."""
    return sinc.reais.get(g.id) or id_real(g.id)


def _cortado(marcado: str) -> bool:
    """O id encostou no teto do contrato? Então pode ter sido cortado."""
    return len(marcado) >= 39


def _instante(texto: str, tz_nome: str = "") -> datetime | None:
    if not texto:
        return None
    try:
        valor = datetime.fromisoformat(texto.replace("Z", "+00:00"))
        if valor.tzinfo is None:
            valor = valor.replace(tzinfo=ZoneInfo(tz_nome or "UTC"))
        return valor.astimezone(timezone.utc).replace(second=0, microsecond=0)
    except (ValueError, TypeError, KeyError):
        return None


async def _google_mudou_depois(cliente, alvo: str, g,
                               tz_nome: str = "") -> bool:
    """O estado remoto é posterior ao gesto guardado no cartão?"""
    gesto = _instante(g.em, tz_nome)
    if not gesto:
        return False
    r = await cliente.get(alvo)
    if r.status_code != 200:
        return False
    remoto = _instante(r.json().get("updated", ""))
    return bool(remoto and remoto > gesto)


async def _gemeo_mudou_depois(cliente, g, sinc, tz_nome: str) -> bool:
    endereco = sinc.gemeos.get(g.id, "")
    if not endereco or "|" not in endereco:
        return False
    cal, evento = endereco.split("|", 1)
    return await _google_mudou_depois(
        cliente,
        f"{CALENDAR}/calendars/{_url(cal)}/events/{_url(evento)}",
        g, tz_nome)


async def _acha_id_comprido(cliente, cal: str, pedaco: str, g) -> str:
    """Procura, na agenda, o evento cujo id COMEÇA com este pedaço."""
    quando = None
    if g.d:
        try:
            quando = datetime.strptime(g.d, "%Y-%m-%d")
        except ValueError:
            quando = None

    inicio = (quando - timedelta(days=1)) if quando else (_agora() - timedelta(days=32))
    fim = (quando + timedelta(days=2)) if quando else (_agora() + timedelta(days=90))

    r = await cliente.get(f"{CALENDAR}/calendars/{_url(cal)}/events",
                          params={"singleEvents": "true", "maxResults": 250,
                                  "showDeleted": "false",
                                  "timeMin": _iso(inicio), "timeMax": _iso(fim)})
    if recusou(r, f"procurar {pedaco} em {cal}"):
        return ""

    for ev in r.json().get("items", []):
        # Pela MARCA, e não pelo começo do id: o encurtado troca a cauda
        # por um resumo, e duas ocorrências do mesmo evento têm o mesmo
        # começo. Comparar prefixo devolveria a primeira que aparecesse.
        if casa_marca(g.id, str(ev.get("id", ""))):
            return ev["id"]
    return ""


async def _gesto_de_evento(cliente, g, sinc, tz_nome: str) -> dict:
    """Evento: NASCE na agenda do Tinto; MEXER alcança qualquer uma."""
    minha = await agenda_do_tinto(cliente, sinc, tz_nome)
    if not minha:
        return {"ok": False, "motivo": "sem a agenda Tinto"}

    novo = not g.id or g.id.startswith("n:")
    onde = minha if novo else sinc.onde.get(g.id, minha)

    alvo = f"{CALENDAR}/calendars/{_url(onde)}/events"
    real = _real_conhecido(g, sinc)

    # Cortado e desconhecido: procura antes de agir. Sem isto o pedido vai
    # para um id que não existe, e um DELETE que responde 404 é lido como
    # sucesso — o evento sai do cartão e fica no Google para sempre.
    if not novo and _cortado(g.id) and g.id not in sinc.reais:
        achado = await _acha_id_comprido(cliente, onde, real, g)
        if achado:
            real = achado
            sinc.reais[g.id] = achado

    recurso = f"{alvo}/{_url(real)}"
    if not novo and await _google_mudou_depois(cliente, recurso, g, tz_nome):
        log.info("gesto %s ignorado: Google mudou depois", g.id)
        return {"ok": True, "id": g.id, "ignorado": True}

    if g.v == "apagou":
        r = await cliente.delete(recurso)
        # 404 é sucesso: o que se queria era que ele não estivesse lá.
        # Insistir num apagar que já aconteceu é o aparelho brigando com
        # um estado que já é o desejado.
        return _resposta(r, (200, 204, 404))

    # Editar a hora mantém a DURAÇÃO que o evento já tinha: mudar o início
    # não pode encolher para uma hora uma reunião de duas.
    dura = 3600
    if not novo and g.h and not g.f:
        ra = await cliente.get(recurso)
        if ra.status_code == 200:
            dura = duracao_s(ra.json())
    corpo = corpo_de_evento(g, tz_nome, dura)
    if novo:
        r = await cliente.post(alvo, json=corpo)
        saida = _resposta(r, (200, 201))
        if saida["ok"]:
            # O id volta JÁ MARCADO, igual ao que o `pull` vai mandar.
            bruto = saida["id"]
            saida["id"] = marca("g", bruto)
            sinc.onde[saida["id"]] = onde
            sinc.reais[saida["id"]] = bruto
        return saida

    # Editar não muda o id, e a resposta do Google traz o id cru. Devolver
    # o que CHEGOU mantém a promessa de uma linha só: o `id` da resposta é
    # sempre o id pelo qual o device conhece a coisa.
    r = await cliente.patch(recurso, json=corpo)
    saida = _resposta(r, (200,))
    saida["id"] = g.id
    return saida


async def _cria_gemeo(cliente, g, sinc, item_id: str,
                      tz_nome: str) -> bool:
    cal = await agenda_do_tinto(cliente, sinc, tz_nome)
    if not cal:
        return False
    r = await cliente.post(
        f"{CALENDAR}/calendars/{_url(cal)}/events",
        json=corpo_de_evento(g, tz_nome))
    if r.status_code not in (200, 201) or not r.json().get("id"):
        return False
    sinc.gemeos[item_id] = f"{cal}|{r.json()['id']}"
    return True


async def _apaga_gemeo(cliente, sinc, item_id: str) -> bool:
    endereco = sinc.gemeos.get(item_id, "")
    if not endereco or "|" not in endereco:
        return True
    cal, evento = endereco.split("|", 1)
    r = await cliente.delete(
        f"{CALENDAR}/calendars/{_url(cal)}/events/{_url(evento)}")
    if r.status_code not in (200, 204, 404):
        recusou(r, f"apagar gêmeo de {item_id}")
        return False
    sinc.gemeos.pop(item_id, None)
    return True


async def _hora_no_gemeo(cliente, g, sinc, tz_nome: str) -> bool:
    """A hora da tarefa vai para o item do Calendar, que é quem a guarda."""
    endereco = sinc.gemeos.get(g.id, "")
    if not endereco or "|" not in endereco:
        return False

    cal_id, ev_id = endereco.split("|", 1)
    alvo = f"{CALENDAR}/calendars/{_url(cal_id)}/events/{_url(ev_id)}"

    r = await cliente.get(alvo)
    if recusou(r, f"gêmeo de {g.id}"):
        return False

    atual = r.json()
    dura = duracao_s(atual)

    fuso = tz_nome or "UTC"
    dia = g.d or (atual.get("start", {}).get("dateTime") or "")[:10]
    if not dia or not g.h:
        return False

    try:
        from zoneinfo import ZoneInfo

        comeca = datetime.fromisoformat(f"{dia}T{g.h}:00").replace(
            tzinfo=ZoneInfo(fuso))
    except Exception:
        return False

    termina = comeca + timedelta(seconds=dura)
    corpo = {
        "start": {"dateTime": comeca.isoformat(), "timeZone": fuso},
        "end":   {"dateTime": termina.isoformat(), "timeZone": fuso},
    }
    rp = await cliente.patch(alvo, json=corpo)
    if not recusou(rp, f"hora do gêmeo {g.id}"):
        log.info("hora do híbrido no Calendar: %s %s %s", g.id, dia, g.h)
        return True
    return False


async def _gesto_de_tarefa(cliente, g, sinc, tz_nome: str = "") -> dict:
    """Tarefa: na lista de onde ela veio; a nova vai para `@default`."""
    novo = not g.id or g.id.startswith("n:")
    lista = "@default" if novo else sinc.onde.get(g.id, "@default")
    alvo = f"{TASKS}/lists/{_url(lista)}/tasks"
    real = _real_conhecido(g, sinc)
    recurso = f"{alvo}/{_url(real)}"

    if not novo and (await _google_mudou_depois(
            cliente, recurso, g, tz_nome)
            or await _gemeo_mudou_depois(cliente, g, sinc, tz_nome)):
        log.info("gesto %s ignorado: Google mudou depois", g.id)
        return {"ok": True, "id": g.id, "ignorado": True}

    if g.v == "apagou":
        r = await cliente.delete(recurso)
        saida = _resposta(r, (200, 204, 404))
        if saida["ok"] and not await _apaga_gemeo(cliente, sinc, g.id):
            return {"ok": False, "motivo": "não foi possível apagar o horário"}
        return saida

    corpo = corpo_de_tarefa(g)
    if novo:
        r = await cliente.post(alvo, json=corpo)
        saida = _resposta(r, (200, 201))
        if saida["ok"]:
            # Marcado como o `pull` manda: é assim que o device troca o id
            # provisório pelo definitivo e não fica com a tarefa em dobro.
            bruto = saida["id"]
            saida["id"] = marca("t", bruto)
            sinc.onde[saida["id"]] = lista
            sinc.reais[saida["id"]] = bruto

            # A API do Tasks descarta a HORA do `due`. Uma tarefa com
            # horário precisa também de um evento no Calendar: o Tasks é
            # dono do concluído, o evento é dono da faixa horária, e o
            # pull casa os dois numa única linha no Tinto.
            if g.h:
                if not await _cria_gemeo(
                        cliente, g, sinc, saida["id"], tz_nome):
                    # Não publique meia tarefa: sem o gêmeo ela perderia
                    # a hora no pull seguinte, exatamente o defeito que
                    # esta operação prometeu não produzir.
                    await cliente.delete(f"{alvo}/{_url(bruto)}")
                    return {"ok": False,
                            "motivo": "não foi possível guardar o horário"}
        return saida

    # PATCH e não PUT: o `position`, o `parent` e as notas que o celular
    # escreveu não vêm no gesto, e um PUT os apagaria — o aparelho
    # destruindo o que não sabe que existe.
    r = await cliente.patch(recurso, json=corpo)
    saida = _resposta(r, (200,))
    saida["id"] = g.id
    if not saida["ok"]:
        return saida

    # Só mexe na metade Calendar depois que a metade Tasks aceitou a
    # edição. Assim adicionar, alterar ou retirar hora converge como uma
    # operação única para quem usa, sem evento órfão.
    if g.h:
        guardou = (await _hora_no_gemeo(cliente, g, sinc, tz_nome)
                   if g.id in sinc.gemeos else
                   await _cria_gemeo(cliente, g, sinc, g.id, tz_nome))
    else:
        guardou = await _apaga_gemeo(cliente, sinc, g.id)
    if not guardou:
        return {"ok": False, "id": g.id,
                "motivo": "não foi possível atualizar o horário"}
    return saida


async def enche_lista(access: str, lista_id: str, itens: list[str]) -> int:
    """O que foi falado DENTRO da lista, criado dentro dela no Google."""
    if not lista_id or not itens:
        return 0

    postos = 0
    anterior = ""
    async with _cliente(access) as cliente:
        alvo = f"{TASKS}/lists/{_url(lista_id)}/tasks"
        for titulo in itens[:20]:
            titulo = (titulo or "").strip()[:127]
            if not titulo:
                continue
            r = await cliente.post(alvo, params={"previous": anterior} if anterior else None,
                                   json={"title": titulo})
            if recusou(r, f"item de {lista_id}"):
                continue
            anterior = r.json().get("id", "")
            postos += 1
    return postos


def _mesmo_nome(a: str, b: str) -> bool:
    """"Compras" e "compras " são a mesma lista para quem fala."""
    return a.strip().casefold() == b.strip().casefold()


async def _lista_com_o_nome(cliente, titulo: str) -> str:
    """O id da lista que já se chama assim, ou vazio."""
    if not titulo.strip():
        return ""
    r = await cliente.get(f"{TASKS}/users/@me/lists")
    if recusou(r, "tasklists"):
        return ""
    for tl in r.json().get("items", []):
        if _mesmo_nome(tl.get("title", ""), titulo):
            return tl.get("id", "")
    return ""


async def _gesto_de_lista(cliente, g, sinc) -> dict:
    """Lista: renomear e apagar. O device não abre lista item a item."""
    real = _real_conhecido(g, sinc)
    alvo = f"{TASKS}/users/@me/lists/{_url(real)}"

    if g.v == "apagou":
        r = await cliente.delete(alvo)
        return _resposta(r, (200, 204, 404))

    if not g.id or g.id.startswith("n:"):
        # Uma lista com este NOME já existe? Então é ela.
        ja = await _lista_com_o_nome(cliente, g.t)
        if ja:
            marcada = marca("l", ja)
            sinc.onde[marcada] = ja
            sinc.reais[marcada] = ja
            return {"ok": True, "id": marcada}

        r = await cliente.post(f"{TASKS}/users/@me/lists",
                               json={"title": g.t[:63]})
        saida = _resposta(r, (200, 201))
        if saida["ok"]:
            bruto = saida["id"]
            saida["id"] = marca("l", bruto)
            sinc.onde[saida["id"]] = bruto
            sinc.reais[saida["id"]] = bruto
        return saida

    r = await cliente.patch(alvo, json={"title": g.t[:63]})
    return _resposta(r, (200,))


# ── o Google avisando, em vez de nós perguntando ─────────────────────
# `events.watch` registra um canal: toda mudança naquela agenda vira um
# POST na nossa porta. É o que troca "perguntar de cinco em cinco
# segundos" por "ser avisado".
async def vigia(access: str, cal_id: str, canal_id: str,
                endereco: str, segredo: str) -> dict | None:
    corpo = {
        "id": canal_id,
        "type": "web_hook",
        "address": endereco,
        # Volta em `X-Goog-Channel-Token` em todo aviso. É o que separa "o
        # Google avisou" de "alguém achou a rota", que é pública por
        # obrigação — o Google precisa alcançá-la sem credencial nossa.
        "token": segredo,
    }

    async with _cliente(access) as c:
        try:
            r = await c.post(f"{CALENDAR}/calendars/{_url(cal_id)}/events/watch",
                             json=corpo)
        except httpx.HTTPError as erro:
            log.warning("watch %s: %s", cal_id, erro)
            return None

        if r.status_code >= 300:
            # O motivo INTEIRO, e não só o código. Foi o texto que revelou
            # que o `channelIdInvalid` era a nossa própria string, e não
            # uma pendência de console — sem ele, quem lê o log conserta o
            # lugar errado.
            log.warning("watch %s recusado: %s %s",
                        cal_id, r.status_code, r.text[:200])

            # ── a recusa DEFINITIVA ──
            # Calendário público e read-only — feriados, fases da lua —
            # não suporta push, e nunca vai suportar. Tratá-la como falha
            # temporária faz o servidor tentar de novo a cada pull, para
            # sempre: uma chamada de API por ciclo que já se sabe que
            # falha.
            if "pushNotSupportedForRequestedResource" in r.text:
                return {"sem_push": True}

            return None

        dados = r.json()

    return {
        "id": dados.get("id", canal_id),
        "recurso": dados.get("resourceId", ""),
        # Em milissegundos, e o Google pode nem mandar. Sem prazo, trata
        # como já vencido: renovar à toa custa uma chamada; não renovar
        # custa a agenda parar de chegar rápido, calada.
        "expira": float(dados.get("expiration", 0)) / 1000.0,
    }


async def para_de_vigiar(access: str, canal_id: str, recurso: str) -> None:
    """Fecha um canal. Sem ele, o Google segue batendo numa porta morta."""
    if not canal_id or not recurso:
        return

    async with _cliente(access) as c:
        try:
            await c.post(f"{CALENDAR}/channels/stop",
                         json={"id": canal_id, "resourceId": recurso})
        except httpx.HTTPError as erro:
            log.info("channels/stop %s: %s", canal_id, erro)
