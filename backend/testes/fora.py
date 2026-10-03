"""O mundo lá fora, de mentira e com estado: Google, Whisper e a LLM."""

import json
import re
from urllib.parse import parse_qs, unquote, urlparse

import time

import httpx


class Fora:
    def __init__(self) -> None:
        self.canais: list[dict] = []
        self.canais_fechados: list[str] = []
        self.versao = 0

        self.agendas = {
            "primary": {"id": "primary", "summary": "Usuário",
                        "primary": True, "timeZone": "America/Sao_Paulo"},
            "pt.brazilian#holiday@group.v.calendar.google.com": {
                "id": "pt.brazilian#holiday@group.v.calendar.google.com",
                "summary": "Feriados no Brasil"},
            "trabalho@x.com": {"id": "trabalho@x.com", "summary": "Trabalho",
                               "selected": False},
            # Um `#` no meio do id, que é o que uma agenda importada de
            # um .ics tem. Ela NÃO está na lista das que vêm desligadas —
            # está aqui para provar que a URL aguenta o caractere.
            "casa#compartilhada@group.calendar.google.com": {
                "id": "casa#compartilhada@group.calendar.google.com",
                "summary": "Casa"},
        }
        self.eventos: dict[str, dict] = {}

        self.listas = {
            "@default": {"id": "@default", "title": "Minhas tarefas"},
            "casa": {"id": "casa", "title": "Casa"},
        }
        self.tarefas: dict[str, dict] = {}

        # O que o Whisper vai transcrever e o que a LLM vai entender. O
        # teste dita os dois: a graça de um falso é poder escrever a fala
        # difícil, não a fácil.
        # Quando ligado, o Google recusa o refresh token — é a conta
        # revogada, que é diferente de o Google estar fora do ar.
        self.concessao_morta = False
        self.sem_refresh = False

        # De quem é a conta que volta do Google.
        self.email = "eu@x.com"

        # Os `code` já trocados por token. Um teste pode semear um aqui
        # para dizer "este já foi usado".
        self.codigos_gastos: set[str] = set()

        # O Google recusando a leitura de UMA lista de tarefas — um 429 de
        # cota, um 500 momentâneo. Existe porque falha e lista vazia são
        # coisas diferentes, e o código que as tratava igual fazia um
        # tropeço de rede virar "0 itens" no vidro.
        self.recusa_tasks = False

        self.fala = "marca dentista quinta às três"
        self.acoes = [{"v": "criou", "t": "Dentista", "h": "15:00",
                       "d": "2026-08-27", "tp": 4}]

        # Cada chamada que o backend fez, para o teste poder afirmar sobre
        # o que NÃO foi chamado — que é metade do que interessa aqui.
        self.chamadas: list[tuple[str, str]] = []

    # ── o que os testes montam ───────────────────────────────────────
    def poe_evento(self, cal: str, **campos) -> str:
        self.versao += 1
        ev = {"id": f"ev{len(self.eventos) + 1}", "status": "confirmed",
              "_cal": cal, "_v": self.versao}
        ev.update(campos)
        self.eventos[ev["id"]] = ev
        return ev["id"]

    def poe_tarefa(self, lista: str, **campos) -> str:
        self.versao += 1
        tk = {"id": f"tk{len(self.tarefas) + 1}", "status": "needsAction",
              "_lista": lista, "updated": f"2026-08-25T00:00:{self.versao:02d}Z"}
        tk.update(campos)
        self.tarefas[tk["id"]] = tk
        return tk["id"]

    # ── o transporte ─────────────────────────────────────────────────
    def transporte(self) -> httpx.MockTransport:
        return httpx.MockTransport(self._responde)

    def _responde(self, pedido: httpx.Request) -> httpx.Response:
        url = urlparse(str(pedido.url))
        # Decodifica como um servidor de verdade decodifica: o `%23` que o
        # backend escreveu volta a ser o `#` do id da agenda. Sem isto o
        # falso seria mais exigente que o Google, e o teste passaria a
        # cobrar uma coisa que ninguém cobra.
        caminho, metodo = unquote(url.path), pedido.method
        q = {k: v[0] for k, v in parse_qs(url.query).items()}
        self.chamadas.append((metodo, caminho))

        corpo = {}
        if pedido.content:
            try:
                corpo = json.loads(pedido.content)
            except ValueError:
                # Formulário, e não JSON: é assim que o OAuth troca o
                # código por token. Sem esta linha o corpo chegava vazio, e
                # o falso não tinha como saber QUAL código estava sendo
                # trocado.
                corpo = {k: v[0] for k, v in
                         parse_qs(pedido.content.decode("utf8",
                                                        "replace")).items()}

        for padrao, funcao in self._rotas():
            m = re.fullmatch(padrao, f"{metodo} {caminho}")
            if m:
                return funcao(m, q, corpo)

        return httpx.Response(404, json={"error": {"message": caminho}})

    def _rotas(self):
        return [
            (r"POST /token", self._token),
            (r"POST /openai/v1/audio/transcriptions", self._whisper),
            (r"POST /v1/messages", self._llm),
            (r"GET /calendar/v3/users/me/calendarList", self._lista_agendas),
            (r"GET /calendar/v3/users/me/settings/timezone", self._fuso),
            (r"POST /calendar/v3/calendars", self._cria_agenda),
            (r"GET /calendar/v3/calendars/(?P<cal>[^/]+)/events",
             self._lista_eventos),
            # UM evento. O gêmeo da tarefa com hora é lido antes de ser
            # reescrito — é dele que sai a DURAÇÃO, que ninguém pediu
            # para mudar quando pediu outro horário.
            # As INSTÂNCIAS de uma série. É por aqui que o backend
            # descobre quando a rotina acontece de novo — e é o Google
            # quem faz essa conta, porque é ele que conhece os buracos.
            (r"GET /calendar/v3/calendars/(?P<cal>[^/]+)/events/"
             r"(?P<ev>[^/]+)/instances",
             self._instancias),
            (r"GET /calendar/v3/calendars/(?P<cal>[^/]+)/events/(?P<ev>[^/]+)",
             self._le_evento),
            (r"POST /calendar/v3/calendars/(?P<cal>[^/]+)/events/watch",
             self._vigia),
            (r"POST /calendar/v3/channels/stop", self._para_de_vigiar),
            (r"POST /calendar/v3/calendars/(?P<cal>[^/]+)/events",
             self._cria_evento),
            (r"PATCH /calendar/v3/calendars/(?P<cal>[^/]+)/events/(?P<ev>[^/]+)",
             self._muda_evento),
            (r"DELETE /calendar/v3/calendars/(?P<cal>[^/]+)/events/(?P<ev>[^/]+)",
             self._apaga_evento),
            (r"GET /tasks/v1/users/@me/lists", self._lista_listas),
            (r"POST /tasks/v1/users/@me/lists", self._cria_lista),
            (r"PATCH /tasks/v1/users/@me/lists/(?P<l>[^/]+)", self._muda_lista),
            (r"DELETE /tasks/v1/users/@me/lists/(?P<l>[^/]+)", self._apaga_lista),
            (r"GET /tasks/v1/lists/(?P<l>[^/]+)/tasks", self._lista_tarefas),
            (r"GET /tasks/v1/lists/(?P<l>[^/]+)/tasks/(?P<t>[^/]+)",
             self._le_tarefa),
            (r"POST /tasks/v1/lists/(?P<l>[^/]+)/tasks", self._cria_tarefa),
            (r"PATCH /tasks/v1/lists/(?P<l>[^/]+)/tasks/(?P<t>[^/]+)",
             self._muda_tarefa),
            (r"DELETE /tasks/v1/lists/(?P<l>[^/]+)/tasks/(?P<t>[^/]+)",
             self._apaga_tarefa),
            (r"GET /v1/userinfo", self._quem_sou),
        ]

    # ── OAuth ────────────────────────────────────────────────────────
    def _quem_sou(self, m, q, corpo):
        """O `userinfo`, que faltava aqui."""
        return httpx.Response(200, json={"email": self.email})

    def _token(self, m, q, corpo):
        if self.concessao_morta:
            return httpx.Response(400, json={"error": "invalid_grant"})

        # O `code` vale UMA vez. Recarregar a página da volta é o jeito
        # mais comum de usá-lo duas, e o Google responde `invalid_grant` —
        # que é o que faz a tela dizer "este endereço já foi usado" em vez
        # de dar 500.
        codigo = (corpo or {}).get("code", "")
        if codigo and codigo in self.codigos_gastos:
            return httpx.Response(400, json={"error": "invalid_grant"})
        if codigo:
            self.codigos_gastos.add(codigo)

        # O SEGUNDO login da mesma pessoa não traz refresh token. É o
        # comportamento do Google, e não um caso de erro: ele só o entrega
        # na primeira autorização, e depois assume que quem pediu guardou.
        if self.sem_refresh:
            return httpx.Response(200, json={"access_token": "acc-1"})

        return httpx.Response(200, json={"access_token": "acc-1",
                                         "refresh_token": "ref-1"})

    # ── Whisper e a LLM ──────────────────────────────────────────────
    def _whisper(self, m, q, corpo):
        return httpx.Response(200, json={"text": self.fala})

    def _llm(self, m, q, corpo):
        # Dentro de um bloco de markdown de propósito: é assim que um
        # modelo responde metade das vezes, e o parser tem que aguentar.
        texto = "```json\n" + json.dumps({"acoes": self.acoes}) + "\n```"
        return httpx.Response(200, json={"content": [{"text": texto}]})

    # ── Calendar ─────────────────────────────────────────────────────
    def _lista_agendas(self, m, q, corpo):
        return httpx.Response(200, json={"items": list(self.agendas.values())})

    def _fuso(self, m, q, corpo):
        return httpx.Response(200, json={"value": "America/Sao_Paulo"})

    def _cria_agenda(self, m, q, corpo):
        novo = {"id": "tinto@grupo", **corpo}
        self.agendas[novo["id"]] = novo
        return httpx.Response(200, json=novo)

    def _lista_eventos(self, m, q, corpo):
        cal = m.group("cal")
        token = q.get("syncToken")

        desde = int(token) if token and token.isdigit() else -1

        # ── a JANELA, como o Google aplica ──────────────────────────
        # `timeMin`/`timeMax` filtram de verdade lá, e o falso ignorava:
        # um evento de outubro descia numa colheita de setembro e nenhum
        # teste percebia. Harness mais permissivo que o alvo aprova o que
        # a placa reprova.
        de = (q.get("timeMin") or "")[:10]
        ate = (q.get("timeMax") or "")[:10]

        saida = []
        for ev in self.eventos.values():
            if ev["_cal"] != cal:
                continue
            if ev["_v"] <= desde:
                continue

            quando = (ev.get("start", {}).get("date")
                      or (ev.get("start", {}).get("dateTime") or "")[:10])
            # Série sem data de início não se filtra por janela: quem
            # decide se ela vale é a regra, e ela vale sempre.
            if quando and ev.get("status") != "cancelled":
                if de and quando < de:
                    continue
                if ate and quando > ate:
                    continue
            # Sem token, o cancelado não vem — é o `showDeleted=false` do
            # primeiro pull, que traria lápides de eventos que o aparelho
            # nunca viu.
            if desde < 0 and ev["status"] == "cancelled":
                continue
            saida.append({k: v for k, v in ev.items() if not k.startswith("_")})

        # **Como o Google de verdade:** um pedido que ordena não ganha
        # `nextSyncToken`. Devolver o token sempre foi o que escondeu, num
        # falso verde, um backend que relia a agenda inteira a cada pull.
        corpo = {"items": saida}
        if "orderBy" not in q:
            corpo["nextSyncToken"] = str(self.versao)
        return httpx.Response(200, json=corpo)

    def _instancias(self, m, q, corpo):
        """As ocorrências de uma série, como o Google as calcula."""
        from datetime import date, timedelta

        mestre = self.eventos.get(m.group("ev"))
        if not mestre:
            return httpx.Response(404, json={"error": {"message": "Not Found"}})

        regras = {}
        for linha in mestre.get("recurrence") or []:
            if linha.upper().startswith("RRULE:"):
                for pedaco in linha.split(":", 1)[1].split(";"):
                    if "=" in pedaco:
                        k, v = pedaco.split("=", 1)
                        regras[k.upper()] = v

        inicio_txt = (mestre.get("start", {}).get("date")
                      or (mestre.get("start", {}).get("dateTime") or "")[:10])
        if not inicio_txt:
            return httpx.Response(200, json={"items": []})

        comeco = date.fromisoformat(inicio_txt)
        freq = regras.get("FREQ", "").upper()
        passo = int(regras.get("INTERVAL", "1") or 1)
        conta = int(regras["COUNT"]) if regras.get("COUNT", "").isdigit() else None
        ate = None
        if regras.get("UNTIL"):
            ate = date.fromisoformat(f"{regras['UNTIL'][:4]}-"
                                     f"{regras['UNTIL'][4:6]}-"
                                     f"{regras['UNTIL'][6:8]}")

        semana = {"SU": 6, "MO": 0, "TU": 1, "WE": 2, "TH": 3, "FR": 4, "SA": 5}
        dias_da_semana = {semana[d.strip().upper()[-2:]]
                          for d in regras.get("BYDAY", "").split(",")
                          if d.strip().upper()[-2:] in semana}

        # Os dias que saíram da série: apagados e movidos.
        buracos = set()
        for ev in self.eventos.values():
            if ev.get("recurringEventId") != mestre["id"]:
                continue
            original = ev.get("originalStartTime") or {}
            dia = original.get("date") or (original.get("dateTime") or "")[:10]
            if dia:
                buracos.add(dia)

        desde_txt = (q.get("timeMin") or "")[:10]
        desde = date.fromisoformat(desde_txt) if desde_txt else comeco
        teto = int(q.get("maxResults", "250"))

        saida, vistas, dia = [], 0, comeco
        limite = comeco + timedelta(days=800)
        while dia <= limite and len(saida) < teto:
            vale = (freq == "DAILY" and (dia - comeco).days % passo == 0) or (
                freq == "WEEKLY"
                and ((dia - comeco).days // 7) % passo == 0
                and (not dias_da_semana or dia.weekday() in dias_da_semana))

            if vale:
                vistas += 1
                if conta is not None and vistas > conta:
                    break
                if ate and dia > ate:
                    break
                if dia.isoformat() not in buracos and dia >= desde:
                    saida.append({
                        "id": f"{mestre['id']}_{dia.isoformat().replace('-', '')}",
                        "status": "confirmed",
                        "recurringEventId": mestre["id"],
                        "summary": mestre.get("summary", ""),
                        "start": {"date": dia.isoformat()},
                        "end": {"date": (dia + timedelta(days=1)).isoformat()},
                    })
            dia += timedelta(days=1)

        return httpx.Response(200, json={"items": saida})

    # ── o canal de aviso ──
    # O Google devolve o `resourceId` — que é o que fecha o canal depois —
    # e o vencimento em MILISSEGUNDOS. O prazo real dele varia; aqui é uma
    # semana, que é o teto documentado para eventos.
    def _vigia(self, m, q, corpo):
        # Calendário público e read-only não suporta push, e nunca vai:
        # é recusa DEFINITIVA, e o servidor precisa distingui-la de uma
        # falha de rede para não tentar de novo a cada pull.
        if "holiday" in unquote(m.group("cal")):
            return httpx.Response(400, json={"error": {"errors": [{
                "domain": "calendar",
                "reason": "pushNotSupportedForRequestedResource",
                "message": "Push notifications are not supported by this "
                           "resource.",
            }], "code": 400}})

        self.canais.append(corpo)
        return httpx.Response(200, json={
            "kind": "api#channel",
            "id": corpo.get("id", ""),
            "resourceId": "recurso-" + corpo.get("id", ""),
            "expiration": str(int((time.time() + 7 * 24 * 3600) * 1000)),
        })

    def _para_de_vigiar(self, m, q, corpo):
        self.canais_fechados.append(corpo.get("id", ""))
        return httpx.Response(204)

    def _cria_evento(self, m, q, corpo):
        if "end" not in corpo:
            return httpx.Response(
                400, json={"error": {"message": "Missing end time."}})
        eid = self.poe_evento(m.group("cal"), **corpo)
        return httpx.Response(200, json={"id": eid, **corpo})

    def _le_evento(self, m, q, corpo):
        ev = self.eventos.get(m.group("ev"))
        if not ev:
            return httpx.Response(404, json={"error": {"message": "Not Found"}})
        return httpx.Response(
            200, json={k: v for k, v in ev.items() if not k.startswith("_")})

    def _muda_evento(self, m, q, corpo):
        ev = self.eventos.get(m.group("ev"))
        if not ev or ev["_cal"] != m.group("cal"):
            return httpx.Response(404, json={"error": {"message": "Not Found"}})
        self.versao += 1
        ev.update(corpo)
        ev["_v"] = self.versao
        return httpx.Response(200, json={"id": ev["id"]})

    def _apaga_evento(self, m, q, corpo):
        ev = self.eventos.get(m.group("ev"))
        if not ev:
            return httpx.Response(404, json={"error": {"message": "Not Found"}})
        self.versao += 1
        ev["status"] = "cancelled"
        ev["_v"] = self.versao
        return httpx.Response(204)

    # ── Tasks ────────────────────────────────────────────────────────
    def _lista_listas(self, m, q, corpo):
        return httpx.Response(200, json={"items": list(self.listas.values())})

    def _cria_lista(self, m, q, corpo):
        novo = {"id": f"ls{len(self.listas)}", "title": corpo.get("title", "")}
        self.listas[novo["id"]] = novo
        return httpx.Response(200, json=novo)

    def _muda_lista(self, m, q, corpo):
        tl = self.listas.get(m.group("l"))
        if not tl:
            return httpx.Response(404, json={"error": {"message": "Not Found"}})
        tl.update(corpo)
        return httpx.Response(200, json=tl)

    def _apaga_lista(self, m, q, corpo):
        return httpx.Response(204 if self.listas.pop(m.group("l"), None)
                              else 404)

    def _lista_tarefas(self, m, q, corpo):
        if self.recusa_tasks:
            return httpx.Response(429, json={"error": {"message": "slow down"}})

        lista = m.group("l")
        saida = [{k: v for k, v in t.items() if not k.startswith("_")}
                 for t in self.tarefas.values() if t["_lista"] == lista]
        inicio = int(q.get("pageToken", "0"))
        tamanho = int(q.get("maxResults", "100"))
        fim = inicio + tamanho
        corpo = {"items": saida[inicio:fim]}
        if fim < len(saida):
            corpo["nextPageToken"] = str(fim)
        return httpx.Response(200, json=corpo)

    def _cria_tarefa(self, m, q, corpo):
        # Campo nulo no POST é campo ausente: o Google não guarda null.
        corpo = {k: v for k, v in corpo.items() if v is not None}
        tid = self.poe_tarefa(m.group("l"), **corpo)
        return httpx.Response(200, json={"id": tid, **corpo})

    def _le_tarefa(self, m, q, corpo):
        tk = self.tarefas.get(m.group("t"))
        if not tk or tk["_lista"] != m.group("l"):
            return httpx.Response(404, json={"error": {"message": "Not Found"}})
        return httpx.Response(200, json={
            k: v for k, v in tk.items() if not k.startswith("_")})

    def _muda_tarefa(self, m, q, corpo):
        tk = self.tarefas.get(m.group("t"))
        # É AQUI que o teste da lista errada mora: `@default` não conhece
        # uma tarefa de "casa", e o Google devolve 404 — não um sucesso
        # silencioso.
        if not tk or tk["_lista"] != m.group("l"):
            return httpx.Response(404, json={"error": {"message": "Not Found"}})
        self.versao += 1
        # `null` APAGA o campo, como no Google — guardar o None faria o
        # falso aceitar um "tirar o prazo" que o de verdade recusa.
        for chave, valor in corpo.items():
            if valor is None:
                tk.pop(chave, None)
            else:
                tk[chave] = valor
        tk["updated"] = f"2026-08-25T00:00:{self.versao:02d}Z"
        return httpx.Response(200, json={"id": tk["id"]})

    def _apaga_tarefa(self, m, q, corpo):
        tk = self.tarefas.get(m.group("t"))
        if not tk or tk["_lista"] != m.group("l"):
            return httpx.Response(404, json={"error": {"message": "Not Found"}})
        self.versao += 1
        tk["deleted"] = True
        tk["updated"] = f"2026-08-25T00:00:{self.versao:02d}Z"
        return httpx.Response(204)
