"""A agenda do Google, de ponta a ponta, sem Google."""

import asyncio
from datetime import date, datetime, timedelta, timezone

# ── a JANELA é de três dias, então os testes vivem nela ──────────────
# O backend colhe ontem, hoje e amanhã — é o escopo do aparelho. Data
# fixa de agosto num teste de colheita passou a não descer, e com razão:
# ela está fora do que o Tinto guarda. Quem testa a colheita usa estas.
HOJE = date.today().isoformat()
AMANHA = (date.today() + timedelta(days=1)).isoformat()
ONTEM = (date.today() - timedelta(days=1)).isoformat()

import httpx
import pytest

from tinto import agenda
from tinto.contrato import Gesto, Tipo
from tinto.google import (corpo_de_evento, corpo_de_tarefa,
                          evento_para_item, marca)
from tinto.memoria import Sincronia


@pytest.fixture
def dez_de_setembro(monkeypatch):
    """Os testes com data escrita por extenso vivem em 10/09/2026."""
    monkeypatch.setattr(agenda, "_agora", lambda: datetime(
        2026, 9, 10, 12, 0, tzinfo=timezone.utc))


def test_janela_inicial_inclui_o_dia_civil_inteiro_de_ontem(monkeypatch):
    """Ontem não significa as últimas 24 horas."""
    monkeypatch.setattr(agenda, "_agora", lambda: datetime(
        2026, 9, 5, 23, 30, tzinfo=timezone.utc))

    inicio = agenda._inicio_de_ontem("America/Sao_Paulo")

    # 00:00 de 04/09 em São Paulo = 03:00 UTC.
    assert agenda._iso(inicio) == "2026-09-04T03:00:00Z"


def test_panorama_nao_espera_cada_agenda_em_fila(monkeypatch):
    """A janela da fala paga a rede mais lenta, não a soma de todas."""
    ativas = 0
    maximo = 0

    async def google(pedido: httpx.Request) -> httpx.Response:
        nonlocal ativas, maximo
        caminho = pedido.url.path
        if caminho.endswith("/calendarList"):
            return httpx.Response(200, json={"items": [
                {"id": "primary", "summary": "Pessoal", "primary": True,
                 "timeZone": "America/Sao_Paulo"},
                {"id": "trabalho", "summary": "Trabalho"},
            ]})
        if caminho.endswith("/users/@me/lists"):
            return httpx.Response(200, json={"items": [
                {"id": "pessoal", "title": "Pessoal"},
                {"id": "trabalho", "title": "Trabalho"},
            ]})

        ativas += 1
        maximo = max(maximo, ativas)
        await asyncio.sleep(0.01)
        ativas -= 1
        return httpx.Response(200, json={"items": []})

    monkeypatch.setattr(agenda, "transporte", httpx.MockTransport(google))
    asyncio.run(agenda.panorama("token", Sincronia(),
                                "America/Sao_Paulo"))

    assert maximo >= 2


# ── os formatos que erram sozinhos ───────────────────────────────────
def test_evento_de_dia_inteiro_termina_no_dia_seguinte():
    """`end.date` é EXCLUSIVO."""
    g = Gesto(id="n:1", t="Entrega", d="2026-08-14", tp=Tipo.EVENTO)
    corpo = corpo_de_evento(g)

    assert corpo["start"] == {"date": "2026-08-14"}
    assert corpo["end"] == {"date": "2026-08-15"}


def test_evento_com_hora_tem_fim_e_fuso():
    """Sem `end` o Google devolve 400; sem `timeZone` ele adivinha."""
    g = Gesto(id="n:1", t="Dentista", d="2026-08-27", h="15:00",
              tp=Tipo.EVENTO)
    corpo = corpo_de_evento(g, "America/Sao_Paulo")

    assert corpo["start"]["dateTime"] == "2026-08-27T15:00:00"
    assert corpo["end"]["dateTime"] == "2026-08-27T16:00:00"
    assert corpo["start"]["timeZone"] == "America/Sao_Paulo"


def test_evento_que_atravessa_a_meia_noite_termina_no_dia_seguinte():
    """Este teste EXIGIA o defeito, e é por isso que ele passava."""
    g = Gesto(id="n:1", t="Virada", d="2026-08-27", h="23:30", tp=Tipo.EVENTO)
    corpo = corpo_de_evento(g)

    assert corpo["start"]["dateTime"] == "2026-08-27T23:30:00"
    assert corpo["end"]["dateTime"] == "2026-08-28T00:30:00"


def test_evento_comum_nao_muda_de_dia():
    g = Gesto(id="n:1", t="Dentista", d="2026-08-27", h="15:00", tp=Tipo.EVENTO)
    corpo = corpo_de_evento(g)

    assert corpo["start"]["dateTime"] == "2026-08-27T15:00:00"
    assert corpo["end"]["dateTime"] == "2026-08-27T16:00:00"


def test_a_virada_no_fim_do_mes_anda_o_mes():
    """31/08 23:30 → 01/09 00:30. É `date` que faz a conta, e não nós."""
    g = Gesto(id="n:1", t="Virada", d="2026-08-31", h="23:10", tp=Tipo.EVENTO)
    assert corpo_de_evento(g)["end"]["dateTime"] == "2026-09-01T00:10:00"


def test_due_de_tarefa_e_meia_noite_utc():
    """Mandar com fuso local desloca o dia pra trás de madrugada."""
    g = Gesto(id="n:1", t="Pagar IPVA", d="2026-08-20", tp=Tipo.TAREFA)
    corpo = corpo_de_tarefa(g)

    assert corpo["due"] == "2026-08-20T00:00:00.000Z"
    assert corpo["status"] == "needsAction"


# ── o ciclo fecha ────────────────────────────────────────────────────
def test_evento_criado_volta_no_pull(pareado, fora):
    """O teste que resume o backend inteiro."""
    cliente, cab, ap = pareado

    r = cliente.post("/v1/push", headers={**cab, "operacao": "op-1"},
                     json={"v": "criou", "id": "n:1", "t": "Dentista",
                           "d": HOJE, "h": "15:00", "tp": 4})
    assert r.json()["ok"], r.json()

    r = cliente.get("/v1/pull", headers=cab)
    titulos = [i["t"] for i in r.json()["itens"]]
    assert "Dentista" in titulos


def test_o_evento_nasce_na_agenda_do_tinto_e_nao_na_principal(pareado, fora):
    """`calendar.app.created` alcança só o que o aplicativo criou."""
    cliente, cab, ap = pareado
    cliente.post("/v1/push", headers={**cab, "operacao": "op-1"},
                 json={"v": "criou", "id": "n:1", "t": "Dentista",
                       "d": "2026-08-27", "h": "15:00", "tp": 4})

    criados = [ev for ev in fora.eventos.values()]
    assert len(criados) == 1
    assert criados[0]["_cal"] == "tinto@grupo"
    assert fora.agendas["tinto@grupo"]["summary"] == "Tinto"


def test_tarefa_e_alterada_na_lista_de_onde_veio(pareado, fora):
    """`@default` não conhece uma tarefa de "Casa", e devolve 404."""
    cliente, cab, ap = pareado
    tid = fora.poe_tarefa("casa", title="Trocar a lâmpada")

    cliente.get("/v1/pull", headers=cab)          # é o pull que ensina onde

    r = cliente.post("/v1/push", headers={**cab, "operacao": "op-2"},
                     json={"v": "editou", "id": f"t:{tid}",
                           "t": "Trocar a lâmpada", "tp": 2, "ok": True})

    assert r.json()["ok"], r.json()
    assert fora.tarefas[tid]["status"] == "completed"


def test_tarefa_apagada_no_celular_some_do_aparelho(pareado, fora):
    """`removidos`, e é a única forma de o Tinto saber que ela saiu."""
    cliente, cab, ap = pareado
    tid = fora.poe_tarefa("@default", title="Comprar pasta")

    r = cliente.get("/v1/pull", headers=cab)
    assert f"t:{tid}" in [i["id"] for i in r.json()["itens"]]

    fora.tarefas[tid]["deleted"] = True
    fora.versao += 1
    fora.tarefas[tid]["updated"] = f"2026-08-26T00:00:{fora.versao:02d}Z"

    r = cliente.get("/v1/pull", headers=cab)
    assert {"id": f"t:{tid}"} in r.json()["removidos"]


def test_tarefa_depois_da_primeira_pagina_tambem_some(pareado, fora):
    """O Google Tasks corta cada resposta; exclusão na página 2 também vale."""
    cliente, cab, ap = pareado
    ids = [fora.poe_tarefa("@default", title=f"Tarefa {i}")
           for i in range(105)]

    r = cliente.get("/v1/pull", headers=cab)
    while r.json()["mais"]:
        r = cliente.get("/v1/pull", headers=cab)

    tid = ids[-1]
    fora.versao += 1
    fora.tarefas[tid].update(
        deleted=True, updated=f"2026-08-26T00:01:{fora.versao:02d}Z")

    r = cliente.get("/v1/pull", headers=cab)
    assert {"id": f"t:{tid}"} in r.json()["removidos"]


def test_evento_apagado_no_celular_some_do_aparelho(pareado, fora):
    cliente, cab, ap = pareado
    eid = fora.poe_evento("primary", summary="Reunião",
                          start={"dateTime": "2026-08-27T10:00:00-03:00"},
                          end={"dateTime": "2026-08-27T11:00:00-03:00"})

    cliente.get("/v1/pull", headers=cab)
    fora.eventos[eid]["status"] = "cancelled"
    fora.versao += 1
    fora.eventos[eid]["_v"] = fora.versao

    r = cliente.get("/v1/pull", headers=cab)
    assert {"id": f"g:{eid}"} in r.json()["removidos"]


# ── a curadoria ──────────────────────────────────────────────────────
def test_feriados_e_agenda_desligada_nao_chegam(pareado, fora):
    """Uma conta comum tem seis a dez agendas, e a maior parte não é
    compromisso. Numa tela de 240 px isso não é ruído — é a tela inteira.
    """
    cliente, cab, ap = pareado
    fora.poe_evento("pt.brazilian#holiday@group.v.calendar.google.com",
                    summary="Independência", start={"date": "2026-09-07"},
                    end={"date": "2026-09-08"})
    fora.poe_evento("trabalho@x.com", summary="Daily",
                    start={"date": "2026-08-27"},
                    end={"date": "2026-08-28"})

    cliente.get("/v1/pull", headers=cab)

    pediu = [c for c in fora.chamadas if "/events" in c[1]]
    assert not any("holiday" in c[1] for c in pediu)
    assert not any("trabalho" in c[1] for c in pediu)


def test_agenda_com_cerquilha_no_id_nao_perde_a_url(pareado, fora):
    """`#` numa URL não é um caractere: é o começo do fragmento."""
    cliente, cab, ap = pareado
    fora.poe_evento("casa#compartilhada@group.calendar.google.com",
                    summary="Almoço de domingo",
                    start={"date": HOJE}, end={"date": AMANHA})

    itens = []
    r = cliente.get("/v1/pull", headers=cab)
    itens += r.json()["itens"]
    while r.json()["mais"]:
        r = cliente.get("/v1/pull", headers=cab)
        itens += r.json()["itens"]

    assert any(i["t"] == "Almoço de domingo" for i in itens)


def test_o_fuso_vem_do_google_e_ja_resolvido(pareado, fora):
    """A tabela de horário de verão muda por decreto e não cabe num
    firmware que se atualiza a cada meses."""
    cliente, cab, ap = pareado
    r = cliente.get("/v1/pull", headers=cab)

    assert r.json()["tz_min"] == -180
    assert ap.tz_nome == "America/Sao_Paulo"


# ── a lista ──────────────────────────────────────────────────────────
def test_a_lista_conta_o_todo_e_nao_o_delta(pareado, fora):
    """`n` e `k` são sobre o que existe DENTRO, não sobre o que mudou."""
    cliente, cab, ap = pareado
    for i in range(7):
        fora.poe_tarefa("casa", title=f"item {i}",
                        status="completed" if i < 2 else "needsAction")

    itens = []
    r = cliente.get("/v1/pull", headers=cab)
    itens += r.json()["itens"]
    while r.json()["mais"]:
        r = cliente.get("/v1/pull", headers=cab)
        itens += r.json()["itens"]

    casa = [i for i in itens if i["id"] == "l:casa"][0]
    assert casa["n"] == 7 and casa["k"] == 2
    assert casa["tp"] == Tipo.LISTA


def test_gesto_de_lista_nao_vai_parar_no_calendar(pareado, fora):
    """Uma lista tem `tp:3` e id `l:`, e o despachante antigo mandava as
    duas coisas para o mesmo lugar: `PATCH /calendars/primary/events/casa`.
    """
    cliente, cab, ap = pareado
    r = cliente.post("/v1/push", headers={**cab, "operacao": "op-3"},
                     json={"v": "editou", "id": "l:casa", "t": "Da casa",
                           "tp": 3})

    assert r.json()["ok"], r.json()
    assert fora.listas["casa"]["title"] == "Da casa"


# ── o que o Google recusa ────────────────────────────────────────────
def test_erro_do_google_vira_ok_falso_com_motivo(pareado, fora):
    """O device lê só `ok`. O `motivo` é para quem lê o log — e é a
    diferença entre consertar em um minuto e passar a tarde adivinhando.
    """
    cliente, cab, ap = pareado
    r = cliente.post("/v1/push", headers={**cab, "operacao": "op-4"},
                     json={"v": "editou", "id": "t:naoexiste", "t": "X",
                           "tp": 2})

    assert r.json()["ok"] is False
    assert r.json()["motivo"]


def test_evento_de_outra_agenda_e_editado_onde_ele_mora(pareado, fora):
    """Mexer alcança qualquer agenda; o gesto vai para a de ORIGEM."""
    cliente, cab, ap = pareado
    eid = fora.poe_evento("primary", summary="Reunião do time",
                          start={"date": HOJE},
                          end={"date": "2026-08-28"})
    cliente.get("/v1/pull", headers=cab)          # é o pull que ensina onde

    r = cliente.post("/v1/push", headers={**cab, "operacao": "op-5"},
                     json={"v": "editou", "id": f"g:{eid}",
                           "t": "Reunião do time — adiada", "tp": 4,
                           "d": HOJE})

    assert r.json()["ok"], r.json()
    assert fora.eventos[eid]["_cal"] == "primary"
    assert fora.eventos[eid]["summary"] == "Reunião do time — adiada"


def test_gesto_offline_antigo_nao_sobrescreve_google_mais_novo(pareado, fora):
    """Ao reconectar, vence quem realmente mudou por último."""
    cliente, cab, ap = pareado
    eid = fora.poe_evento(
        "primary", summary="Título do celular",
        updated="2026-09-10T12:05:00Z",
        start={"date": "2026-09-11"}, end={"date": "2026-09-12"})
    cliente.get("/v1/pull", headers=cab)

    r = cliente.post("/v1/push", headers={**cab, "operacao": "op-antiga"},
                     json={"v": "editou", "id": f"g:{eid}",
                           "t": "Título offline", "tp": 4,
                           "d": "2026-09-11",
                           "em": "2026-09-10T09:00:00-03:00"})

    assert r.json()["ok"], r.json()
    assert r.json()["ignorado"] is True
    assert fora.eventos[eid]["summary"] == "Título do celular"


def test_tarefa_offline_antiga_nao_reabre_a_concluida_no_google(pareado, fora):
    cliente, cab, ap = pareado
    tid = fora.poe_tarefa(
        "@default", title="Entregar relatório", status="completed",
        completed="2026-09-10T12:05:00Z",
        updated="2026-09-10T12:05:00Z", due="2026-09-11T00:00:00Z")
    cliente.get("/v1/pull", headers=cab)

    r = cliente.post("/v1/push", headers={**cab, "operacao": "op-t-antiga"},
                     json={"v": "editou", "id": f"t:{tid}",
                           "t": "Entregar relatório", "tp": 2,
                           "d": "2026-09-11", "ok": False,
                           "em": "2026-09-10T09:00:00-03:00"})

    assert r.json().get("ignorado") is True, r.json()
    assert fora.tarefas[tid]["status"] == "completed"


def test_tarefa_nova_com_hora_volta_do_google_com_a_mesma_hora(pareado, fora):
    """Tasks descarta hora; o gêmeo no Calendar precisa preservá-la."""
    cliente, cab, ap = pareado

    criada = cliente.post(
        "/v1/push", headers={**cab, "operacao": "op-hibrida"},
        json={"v": "criou", "id": "n:1", "t": "Tomar remédio",
              "tp": 2, "d": "2026-09-11", "h": "08:30"}).json()
    assert criada["ok"], criada

    itens = cliente.get("/v1/pull", headers=cab).json()["itens"]
    remedio = [i for i in itens if i["t"] == "Tomar remédio"]

    assert len(remedio) == 1, remedio
    assert remedio[0]["tp"] == Tipo.TAREFA
    assert remedio[0]["h"] == "08:30"


def test_tirar_hora_da_tarefa_remove_o_gemeo_do_calendar(pareado, fora):
    cliente, cab, ap = pareado
    criada = cliente.post(
        "/v1/push", headers={**cab, "operacao": "op-com-hora"},
        json={"v": "criou", "id": "n:1", "t": "Tomar remédio",
              "tp": 2, "d": "2026-09-11", "h": "08:30"}).json()

    alterada = cliente.post(
        "/v1/push", headers={**cab, "operacao": "op-sem-hora"},
        json={"v": "editou", "id": criada["id"], "t": "Tomar remédio",
              "tp": 2, "d": "2026-09-11", "h": ""}).json()

    assert alterada["ok"], alterada
    assert all(e.get("status") == "cancelled"
               for e in fora.eventos.values()
               if e.get("summary") == "Tomar remédio")


def test_apagar_tarefa_com_hora_apaga_as_duas_metades(pareado, fora):
    cliente, cab, ap = pareado
    criada = cliente.post(
        "/v1/push", headers={**cab, "operacao": "op-com-hora"},
        json={"v": "criou", "id": "n:1", "t": "Tomar remédio",
              "tp": 2, "d": "2026-09-11", "h": "08:30"}).json()

    apagada = cliente.post(
        "/v1/push", headers={**cab, "operacao": "op-apaga"},
        json={"v": "apagou", "id": criada["id"], "t": "Tomar remédio",
              "tp": 2, "d": "2026-09-11", "h": "08:30"}).json()

    assert apagada["ok"], apagada
    assert all(t.get("deleted") for t in fora.tarefas.values())
    assert all(e.get("status") == "cancelled" for e in fora.eventos.values())


def test_adicionar_hora_a_tarefa_cria_gemeo_no_calendar(pareado, fora):
    cliente, cab, ap = pareado
    criada = cliente.post(
        "/v1/push", headers={**cab, "operacao": "op-sem-hora"},
        json={"v": "criou", "id": "n:1", "t": "Tomar remédio",
              "tp": 2, "d": "2026-09-11", "h": ""}).json()

    alterada = cliente.post(
        "/v1/push", headers={**cab, "operacao": "op-ganha-hora"},
        json={"v": "editou", "id": criada["id"], "t": "Tomar remédio",
              "tp": 2, "d": "2026-09-11", "h": "08:30"}).json()

    assert alterada["ok"], alterada
    gemeos = [e for e in fora.eventos.values()
              if e.get("summary") == "Tomar remédio"
              and e.get("status") != "cancelled"]
    assert len(gemeos) == 1
    assert gemeos[0]["start"]["dateTime"].startswith("2026-09-11T08:30")


def test_gesto_antigo_nao_desfaz_hora_mais_nova_do_google(pareado, fora):
    cliente, cab, ap = pareado
    criada = cliente.post(
        "/v1/push", headers={**cab, "operacao": "op-com-hora"},
        json={"v": "criou", "id": "n:1", "t": "Tomar remédio",
              "tp": 2, "d": "2026-09-11", "h": "08:30"}).json()
    evento = next(iter(fora.eventos.values()))
    evento["start"]["dateTime"] = "2026-09-11T10:00:00-03:00"
    evento["end"]["dateTime"] = "2026-09-11T11:00:00-03:00"
    evento["updated"] = "2026-09-10T12:05:00Z"

    antiga = cliente.post(
        "/v1/push", headers={**cab, "operacao": "op-hora-antiga"},
        json={"v": "editou", "id": criada["id"], "t": "Tomar remédio",
              "tp": 2, "d": "2026-09-11", "h": "09:00",
              "em": "2026-09-10T09:00:00-03:00"}).json()

    assert antiga.get("ignorado") is True, antiga
    assert evento["start"]["dateTime"] == "2026-09-11T10:00:00-03:00"


def test_api_desligada_no_projeto_aparece_no_log(pareado, fora, caplog):
    """"A API não está ligada", "o escopo não foi concedido" e "a pessoa
    não tem eventos" viram, os três, um dia em branco no vidro.
    """
    import httpx

    def recusa_tudo(pedido):
        if "oauth2" in str(pedido.url):
            return httpx.Response(200, json={"access_token": "a"})
        return httpx.Response(403, json={"error": {
            "message": "Google Calendar API has not been used in project "
                       "123 before or it is disabled."}})

    from tinto import agenda
    agenda.transporte = httpx.MockTransport(recusa_tudo)

    cliente, cab, ap = pareado
    with caplog.at_level("WARNING"):
        r = cliente.get("/v1/pull", headers=cab)

    assert r.status_code == 200          # o device não vê erro
    assert r.json()["itens"] == []
    assert "has not been used in project" in caplog.text


# ── o fuso, que é a coisa que o aparelho não pode errar ──────────────
def test_evento_em_utc_chega_na_hora_da_pessoa():
    """O defeito que apareceu na primeira agenda de verdade."""
    ev = {"id": "a1", "summary": "Dentista",
          "start": {"dateTime": "2026-08-27T18:00:00Z"},
          "end": {"dateTime": "2026-08-27T19:00:00Z"}}

    it = evento_para_item(ev, "America/Sao_Paulo")
    assert it.h == "15:00"
    assert it.f == "16:00"
    assert it.d == "2026-08-27"


def test_a_conversao_do_dia_acompanha_a_da_hora():
    """Um evento às 22:00 de Brasília é `01:00Z` do dia seguinte."""
    ev = {"id": "a2", "summary": "Virada",
          "start": {"dateTime": "2026-08-28T01:00:00Z"},
          "end": {"dateTime": "2026-08-28T02:00:00Z"}}

    it = evento_para_item(ev, "America/Sao_Paulo")
    assert it.h == "22:00"
    assert it.d == "2026-08-27"          # e não 28


def test_dia_inteiro_nao_e_convertido():
    """`start.date` não tem hora nem fuso. Convertê-lo inventaria um
    instante que ninguém marcou, e o dia poderia andar."""
    ev = {"id": "a3", "summary": "Entrega",
          "start": {"date": "2026-08-27"}, "end": {"date": "2026-08-28"}}

    it = evento_para_item(ev, "America/Sao_Paulo")
    assert it.di is True and it.d == "2026-08-27" and it.h == ""


def test_a_agenda_do_tinto_nasce_no_fuso_da_conta(pareado, fora):
    """Sem `timeZone`, ela nasce em UTC — e todo evento que o Tinto
    escrever volta três horas adiantado para quem está em Brasília."""
    cliente, cab, ap = pareado
    cliente.get("/v1/pull", headers=cab)          # aprende o fuso
    cliente.post("/v1/push", headers={**cab, "operacao": "op-1"},
                 json={"v": "criou", "id": "n:1", "t": "Dentista",
                       "d": "2026-08-27", "h": "15:00", "tp": 4})

    assert fora.agendas["tinto@grupo"].get("timeZone") == "America/Sao_Paulo"


def test_o_primeiro_sync_ganha_token_e_o_segundo_pull_vem_vazio(pareado, fora):
    """O defeito que só a agenda de verdade revelou."""
    cliente, cab, ap = pareado
    fora.poe_evento("primary", summary="Reunião do time",
                    start={"date": HOJE}, end={"date": AMANHA})

    primeiro = cliente.get("/v1/pull", headers=cab).json()
    assert any(i["t"] == "Reunião do time" for i in primeiro["itens"])
    assert ap.sinc.tokens.get("primary"), "não guardou o syncToken"

    segundo = cliente.get("/v1/pull", headers=cab).json()
    assert not any(i["t"] == "Reunião do time" for i in segundo["itens"])


def test_pull_sem_novidade_volta_vazio(pareado, fora):
    """Um pull vazio é o que deixa o vidro parado quando nada aconteceu."""
    cliente, cab, ap = pareado
    fora.poe_tarefa("@default", title="Comprar pasta")

    assert cliente.get("/v1/pull", headers=cab).json()["itens"]
    assert cliente.get("/v1/pull", headers=cab).json()["itens"] == []

    # E volta a descer quando a contagem muda de verdade.
    fora.poe_tarefa("@default", title="Mais uma")
    nomes = [i["t"] for i in cliente.get("/v1/pull", headers=cab).json()["itens"]]
    assert "Minhas tarefas" in nomes and "Mais uma" in nomes


def test_lista_que_falhou_nao_vira_lista_vazia(pareado, fora):
    """Falha de rede não é lista vazia, e tratá-las igual é caro."""
    cliente, cab, ap = pareado
    fora.poe_tarefa("@default", title="Comprar pasta")
    fora.poe_tarefa("@default", title="Pagar IPVA")

    # A primeira leitura desce a lista com a contagem certa.
    r = cliente.get("/v1/pull", headers=cab).json()
    listas = [i for i in r["itens"] if i["id"].startswith("l:")]
    assert listas and listas[0]["n"] == 2

    # O buffer do servidor tem teto de bytes: o que não coube sai nos
    # pulls seguintes. Esvaziar antes é o que faz o próximo pull COLHER —
    # senão ele entrega o que já estava guardado, e o teste falaria sobre
    # a colheita anterior.
    for _ in range(5):
        if not cliente.get("/v1/pull", headers=cab).json()["itens"]:
            break

    # Agora o Google recusa a leitura DAQUELA lista.
    fora.recusa_tasks = True
    ap.sinc.contagens.clear()

    r = cliente.get("/v1/pull", headers=cab).json()
    listas = [i for i in r["itens"] if i["id"].startswith("l:")]

    # Ela não desce dizendo zero: ela simplesmente não desce.
    assert not listas, listas
    # E o cache não guardou a contagem errada — a certa ainda pode vir.
    assert not ap.sinc.contagens


def test_evento_de_outra_agenda_se_apaga(pareado, fora):
    """Criar é na agenda do Tinto; MEXER alcança qualquer uma."""
    cliente, cab, ap = pareado
    eid = fora.poe_evento("primary", summary="Reunião do time")

    cliente.get("/v1/pull", headers=cab)          # é o pull que ensina onde

    r = cliente.post("/v1/push", headers={**cab, "operacao": "op-9"},
                     json={"v": "apagou", "id": f"g:{eid}",
                           "t": "Reunião do time", "tp": 4})

    assert r.json()["ok"], r.json()

    # `cancelled` e não sumido: é o que o Google faz com evento apagado, e
    # é por esse estado que o pull seguinte sabe tirá-lo do aparelho.
    assert fora.eventos[eid]["status"] == "cancelled"


def test_evento_de_serie_e_apagado_no_google(pareado, fora):
    """Id comprido: o que o device carrega é um PEDAÇO do id do Google."""
    cliente, cab, ap = pareado
    eid = fora.poe_evento("primary",
                          id="4f1ep2bhm5plnqmtqvgs4o9rag_20260910T130000Z",
                          summary="Reunião semanal")

    cliente.get("/v1/pull", headers=cab)          # é o pull que ensina onde

    r = cliente.post("/v1/push", headers={**cab, "operacao": "op-serie"},
                     json={"v": "apagou", "id": marca("g", eid),
                           "t": "Reunião semanal", "tp": 4})

    assert r.json()["ok"], r.json()
    assert fora.eventos[eid]["status"] == "cancelled"


@pytest.mark.usefixtures("dez_de_setembro")
def test_id_cortado_sem_tabela_e_procurado_na_agenda(pareado, fora):
    """A tabela `reais` não cobre o que desceu ANTES dela existir."""
    cliente, cab, ap = pareado
    eid = fora.poe_evento("primary",
                          id="4f1ep2bhm5plnqmtqvgs4o9rag_20260910T130000Z",
                          summary="Reunião semanal",
                          start={"dateTime": "2026-09-10T13:00:00Z"},
                          end={"dateTime": "2026-09-10T14:00:00Z"})

    cliente.get("/v1/pull", headers=cab)
    marcado = marca("g", eid)
    ap.sinc.reais.clear()                    # o backend reiniciou

    r = cliente.post("/v1/push", headers={**cab, "operacao": "op-cortado"},
                     json={"v": "apagou", "id": marcado, "d": "2026-09-10",
                           "t": "Reunião semanal", "tp": 4})

    assert r.json()["ok"], r.json()
    assert fora.eventos[eid]["status"] == "cancelled"


def test_tarefa_com_hora_desce_como_hibrido(pareado, fora):
    """O `due` com hora vira o híbrido; a meia-noite continua sendo prazo."""
    cliente, cab, ap = pareado
    fora.poe_tarefa("@default", title="Academia",
                    due="2026-09-11T10:00:00.000Z")
    fora.poe_tarefa("@default", title="Entregar relatório",
                    due="2026-09-12T00:00:00.000Z")

    r = cliente.get("/v1/pull", headers=cab).json()
    por_titulo = {i["t"]: i for i in r["itens"]}

    academia = por_titulo["Academia"]
    assert academia["h"] == "07:00", academia      # -03:00, mesmo dia
    assert academia["d"] == "2026-09-11", academia

    prazo = por_titulo["Entregar relatório"]
    assert prazo["h"] == "", prazo
    assert prazo["d"] == "2026-09-12", prazo


@pytest.mark.usefixtures("dez_de_setembro")
def test_tarefa_com_hora_e_o_evento_dela_viram_um_item_so(pareado, fora):
    """As duas pontas da mesma coisa, casadas numa só."""
    cliente, cab, ap = pareado

    tid = fora.poe_tarefa("@default", title="Academia",
                          due="2026-09-11T00:00:00.000Z")
    fora.poe_evento("primary", summary="Academia",
                    start={"dateTime": "2026-09-11T07:00:00-03:00"},
                    end={"dateTime": "2026-09-11T08:00:00-03:00"})

    itens = cliente.get("/v1/pull", headers=cab).json()["itens"]
    academias = [i for i in itens if i["t"] == "Academia"]

    # UMA, e não duas.
    assert len(academias) == 1, academias

    it = academias[0]
    assert it["tp"] == 2, it                 # tarefa: ela se marca
    assert it["h"] == "07:00", it            # a hora veio do Calendar
    # E o FIM junto: é ele que diz se o resto do dia cabe, e some se o
    # casamento copiar só o começo.
    assert it["f"] == "08:00", it
    assert it["d"] == "2026-09-11", it
    assert it["id"] == f"t:{tid}", it        # o id é o da TAREFA

    # E marcar vai para o Tasks, que é onde "concluída" existe.
    r = cliente.post("/v1/push", headers={**cab, "operacao": "op-h"},
                     json={"v": "editou", "id": it["id"], "t": "Academia",
                           "h": "07:00", "d": "2026-09-11", "tp": 2,
                           "ok": True})
    assert r.json()["ok"], r.json()
    assert fora.tarefas[tid]["status"] == "completed", fora.tarefas[tid]


@pytest.mark.usefixtures("dez_de_setembro")
def test_hibrido_separa_data_prazo_e_conclusao(pareado, fora):
    """Calendar diz quando começa; Tasks diz prazo e conclusão."""
    cliente, cab, ap = pareado
    tid = fora.poe_tarefa("@default", title="Relatório",
                          due="2026-09-14T00:00:00.000Z")
    fora.poe_evento("primary", summary="Relatório",
                    start={"dateTime": "2026-09-11T07:00:00-03:00"},
                    end={"dateTime": "2026-09-11T08:00:00-03:00"})

    # O primeiro casamento ainda precisa de uma pista comum. Depois de
    # aprendido, o vínculo sobrevive a datas diferentes.
    fora.tarefas[tid]["due"] = "2026-09-11T00:00:00.000Z"
    cliente.get("/v1/pull", headers=cab)
    fora.versao += 1
    fora.tarefas[tid].update(
        due="2026-09-14T00:00:00.000Z", status="completed", hidden=True,
        completed="2026-09-12T10:00:00.000Z",
        updated="2026-09-12T10:00:00.000Z", _v=fora.versao)

    item = next(i for i in cliente.get("/v1/pull", headers=cab).json()["itens"]
                if i["id"] == f"t:{tid}")
    assert item["d"] == "2026-09-11"       # data agendada
    assert item["p"] == "2026-09-14"       # prazo
    assert item["c"] == "2026-09-12"       # conclusão


@pytest.mark.usefixtures("dez_de_setembro")
def test_vinculo_do_hibrido_sobrevive_a_edicao_de_titulo_e_data(pareado, fora):
    cliente, cab, ap = pareado
    tid = fora.poe_tarefa("@default", title="Academia",
                          due="2026-09-11T00:00:00.000Z")
    eid = fora.poe_evento("primary", summary="Academia",
                          start={"dateTime": "2026-09-11T07:00:00-03:00"},
                          end={"dateTime": "2026-09-11T08:00:00-03:00"})
    cliente.get("/v1/pull", headers=cab)

    fora.versao += 1
    fora.eventos[eid].update(
        summary="Treino", _v=fora.versao,
        start={"dateTime": "2026-09-12T08:00:00-03:00"},
        end={"dateTime": "2026-09-12T09:00:00-03:00"})
    itens = cliente.get("/v1/pull", headers=cab).json()["itens"]

    item = next(i for i in itens if i["id"] == f"t:{tid}")
    assert item["t"] == "Treino"
    assert item["d"] == "2026-09-12"
    assert item["p"] == "2026-09-11"


@pytest.mark.usefixtures("dez_de_setembro")
def test_marcar_e_desmarcar_hibrido_nao_apaga_a_hora(pareado, fora):
    """O estado muda no Tasks; a hora continua morando no Calendar."""
    cliente, cab, ap = pareado

    tid = fora.poe_tarefa("@default", title="Academia",
                          due="2026-09-11T00:00:00.000Z")
    fora.poe_evento("primary", summary="Academia",
                    start={"dateTime": "2026-09-11T07:00:00-03:00"},
                    end={"dateTime": "2026-09-11T08:00:00-03:00"})

    primeiro = cliente.get("/v1/pull", headers=cab).json()["itens"]
    assert [i for i in primeiro if i["id"] == f"t:{tid}" and i["h"] == "07:00"]

    # Só a metade Tasks mudou. O evento-gêmeo fica intocado e não aparece
    # no delta incremental do Calendar.
    fora.versao += 1
    fora.tarefas[tid].update(status="completed", hidden=True,
                             completed="2026-09-11T10:00:00.000Z",
                             updated="2026-09-11T10:00:00.000Z",
                             _v=fora.versao)
    marcado = cliente.get("/v1/pull", headers=cab).json()["itens"]
    item = next(i for i in marcado if i["id"] == f"t:{tid}")
    assert item["ok"] is True
    assert item["h"] == "07:00", item
    assert item["d"] == "2026-09-11", item

    fora.versao += 1
    fora.tarefas[tid].update(status="needsAction", hidden=False,
                             completed="", updated="2026-09-11T11:00:00.000Z",
                             _v=fora.versao)
    aberto = cliente.get("/v1/pull", headers=cab).json()["itens"]
    item = next(i for i in aberto if i["id"] == f"t:{tid}")
    assert item["ok"] is False
    assert item["h"] == "07:00", item


@pytest.mark.usefixtures("dez_de_setembro")
def test_mudar_a_hora_do_hibrido_vai_para_o_calendar(pareado, fora):
    """A hora mora no gêmeo, e é lá que ela tem de ser escrita."""
    cliente, cab, ap = pareado

    tid = fora.poe_tarefa("@default", title="Academia",
                          due="2026-09-11T00:00:00.000Z")
    eid = fora.poe_evento("primary", summary="Academia",
                          start={"dateTime": "2026-09-11T07:00:00-03:00"},
                          end={"dateTime": "2026-09-11T08:00:00-03:00"})

    cliente.get("/v1/pull", headers=cab)      # é o pull que casa os dois

    r = cliente.post("/v1/push", headers={**cab, "operacao": "op-hora"},
                     json={"v": "editou", "id": f"t:{tid}", "t": "Academia",
                           "h": "09:00", "d": "2026-09-11", "tp": 2})
    assert r.json()["ok"], r.json()

    # O EVENTO mudou de hora — e manteve a duração de uma hora, que
    # ninguém pediu para mudar.
    inicio = fora.eventos[eid]["start"]["dateTime"]
    fim = fora.eventos[eid]["end"]["dateTime"]
    assert inicio.startswith("2026-09-11T09:00"), inicio
    assert fim.startswith("2026-09-11T10:00"), fim


@pytest.mark.usefixtures("dez_de_setembro")
def test_tarefa_que_vira_dia_inteiro_perde_a_hora(pareado, fora):
    """"Dia inteiro" no Google tira a tarefa da régua do Tinto.
    """
    cliente, cab, ap = pareado

    tid = fora.poe_tarefa("@default", title="Academia",
                          due="2026-09-11T00:00:00.000Z")
    eid = fora.poe_evento("primary", summary="Academia",
                          start={"dateTime": "2026-09-11T07:00:00-03:00"},
                          end={"dateTime": "2026-09-11T08:00:00-03:00"})

    itens = cliente.get("/v1/pull", headers=cab).json()["itens"]
    assert [i for i in itens if i["t"] == "Academia" and i["h"] == "07:00"]

    # No Google, ela vira "dia inteiro": o gêmeo some e a tarefa fica.
    fora.versao += 1
    fora.eventos[eid]["status"] = "cancelled"
    fora.eventos[eid]["_v"] = fora.versao
    fora.tarefas[tid]["updated"] = "2026-09-11T12:00:00.000Z"

    r = cliente.get("/v1/pull", headers=cab).json()
    academia = [i for i in r["itens"] if i["t"] == "Academia"]

    assert academia, r
    assert academia[0]["h"] == "", academia      # sem hora: sai da régua
    assert academia[0]["d"] == "2026-09-11", academia
    assert academia[0]["tp"] == 2, academia      # e continua tarefa


def test_ids_compridos_nao_viram_o_mesmo_item():
    """Dois eventos, dois ids — mesmo quando o id não cabe no contrato."""
    from tinto.google import casa_marca, marca

    comeco = "040000008200E00074C5B7101A82E008" + "0" * 10
    um, outro = comeco + "-primeiro", comeco + "-segundo"

    a, b = marca("g", um), marca("g", outro)

    assert a != b
    assert len(a) <= 39 and len(b) <= 39

    # E o caminho de volta continua exato: a marca reconhece o SEU id.
    assert casa_marca(a, um)
    assert not casa_marca(a, outro)

    # O que cabe passa inteiro, como sempre passou.
    assert marca("t", "curto") == "t:curto"


def test_a_lista_volta_junto_de_qualquer_novidade(pareado, fora):
    """A lista perdida no caminho não espera o resync do dia seguinte."""
    cliente, cab, ap = pareado

    fora.poe_tarefa("@default", title="Comprar pão",
                    due="2026-09-11T00:00:00.000Z")
    primeiro = cliente.get("/v1/pull", headers=cab).json()["itens"]
    assert [i for i in primeiro if i["tp"] == 3], primeiro   # a lista desceu

    # Nada mudou: o pull volta vazio, e é o que segura o vidro parado.
    assert cliente.get("/v1/pull", headers=cab).json()["itens"] == []

    # Uma novidade qualquer — e a lista pega carona, sem ter mudado.
    fora.poe_evento("primary", summary="Dentista",
                    start={"dateTime": "2026-09-12T15:00:00-03:00"},
                    end={"dateTime": "2026-09-12T16:00:00-03:00"})
    depois = cliente.get("/v1/pull", headers=cab).json()["itens"]

    assert [i for i in depois if i["t"] == "Dentista"], depois
    assert [i for i in depois if i["tp"] == 3], depois


def test_tirar_o_prazo_no_tinto_tira_o_prazo_no_google(pareado, fora):
    """"Sem data" tem de chegar lá. Antes não chegava.
    """
    cliente, cab, ap = pareado

    tid = fora.poe_tarefa("@default", title="Pagar IPVA",
                          due="2026-09-20T00:00:00.000Z")
    itens = cliente.get("/v1/pull", headers=cab).json()["itens"]
    ipva = next(i for i in itens if i["t"] == "Pagar IPVA")
    assert ipva["d"] == "2026-09-20"

    r = cliente.post("/v1/push", headers={**cab, "operacao": "tira-prazo"},
                     json={"v": "mudou", "id": ipva["id"], "t": "Pagar IPVA",
                           "d": "", "tp": 2})
    assert r.json()["ok"], r.json()

    assert "due" not in fora.tarefas[tid], fora.tarefas[tid]

    # E o pull seguinte não ressuscita a data.
    depois = cliente.get("/v1/pull", headers=cab).json()["itens"]
    voltou = [i for i in depois if i["t"] == "Pagar IPVA"]
    assert not voltou or voltou[0]["d"] == "", voltou


def test_evento_de_varios_dias_diz_ate_quando_vai():
    """A viagem de 14 a 17 ocupa quatro dias, e o device precisa saber."""
    from tinto.google import evento_para_item

    viagem = evento_para_item({
        "id": "v1", "summary": "Viagem",
        "start": {"date": "2026-09-14"},
        "end": {"date": "2026-09-18"},          # exclusivo: acaba no 17
    })
    assert viagem.d == "2026-09-14"
    assert viagem.p == "2026-09-17"

    # Um dia só continua sem `p` — nada a dizer.
    um_dia = evento_para_item({
        "id": "v2", "summary": "Feriado",
        "start": {"date": "2026-09-07"},
        "end": {"date": "2026-09-08"},
    })
    assert um_dia.p == ""

    # E a noite que atravessa a meia-noite é UMA noite, não dois dias.
    noite = evento_para_item({
        "id": "v3", "summary": "Plantão",
        "start": {"dateTime": "2026-09-14T22:00:00-03:00"},
        "end": {"dateTime": "2026-09-15T00:00:00-03:00"},
    }, "America/Sao_Paulo")
    assert noite.p == "", noite

    # Mas a madrugada que entra pelo dia seguinte ocupa os dois.
    virada = evento_para_item({
        "id": "v4", "summary": "Festa",
        "start": {"dateTime": "2026-09-14T22:00:00-03:00"},
        "end": {"dateTime": "2026-09-15T02:00:00-03:00"},
    }, "America/Sao_Paulo")
    assert virada.p == "2026-09-15", virada


@pytest.mark.usefixtures("dez_de_setembro")
def test_e2e_a_rotina_desce_como_regra(pareado, fora):
    """PONTA A PONTA: "todos os dias" vira UMA linha, e ela se mantém fiel."""
    cliente, cab, ap = pareado

    mestre = fora.poe_evento(
        "primary", summary="Academia",
        start={"date": "2026-09-11"}, end={"date": "2026-09-12"},
        recurrence=["RRULE:FREQ=DAILY"])

    itens = cliente.get("/v1/pull", headers=cab).json()["itens"]
    academia = [i for i in itens if i["t"] == "Academia"]

    # UMA linha para a rotina inteira.
    assert len(academia) == 1, academia
    assert academia[0]["rr"] == "d:1", academia[0]
    assert academia[0]["r"] is True

    # ── apagar UM dia no celular ─────────────────────────────────────
    # O Google não mexe no mestre: ele cria um evento cancelado apontando
    # para a série. A regra continua dizendo "todo dia", e é o buraco que
    # tira aquele dia dela.
    fora.poe_evento("primary", status="cancelled",
                    recurringEventId=mestre,
                    originalStartTime={"date": "2026-09-15"})

    depois = cliente.get("/v1/pull", headers=cab).json()["itens"]
    remendada = [i for i in depois if i["t"] == "Academia"]
    assert remendada, depois
    assert remendada[0]["rr"] == "d:1|x=0915", remendada[0]

    # ── tirar a repetição ────────────────────────────────────────────
    # Ela deixa de ser rotina e vira um compromisso de um dia só. Sem
    # isso, o Tinto continuaria desenhando a série para sempre.
    fora.versao += 1
    fora.eventos[mestre].pop("recurrence")
    fora.eventos[mestre]["_v"] = fora.versao

    sem_regra = cliente.get("/v1/pull", headers=cab).json()["itens"]
    sozinha = [i for i in sem_regra if i["t"] == "Academia"]
    assert sozinha, sem_regra
    assert sozinha[0]["rr"] == "", sozinha[0]
    assert sozinha[0]["d"] == "2026-09-11", sozinha[0]


def test_serie_com_contagem_vira_data_de_fim(pareado, fora):
    """"Dez vezes" chega ao aparelho como "até tal dia".
    """
    cliente, cab, ap = pareado

    # A série começa HOJE — a janela é de três dias, e uma série que
    # começa fora dela não desce (é o que o aparelho quer: ele mostra o
    # que está por vir, não o que começa em três semanas).
    dia = date.fromisoformat(HOJE)
    fora.poe_evento("primary", summary="Fisioterapia",
                    start={"date": HOJE}, end={"date": AMANHA},
                    recurrence=[f"RRULE:FREQ=WEEKLY;BYDAY="
                                f"{['MO','TU','WE','TH','FR','SA','SU'][dia.weekday()]};"
                                f"COUNT=3"])

    itens = cliente.get("/v1/pull", headers=cab).json()["itens"]
    fisio = next(i for i in itens if i["t"] == "Fisioterapia")

    # Três segundas a partir de 14/09: 14, 21 e 28.
    fim = (dia + timedelta(days=14)).isoformat().replace("-", "")
    esperado = f"s:1:{(dia.weekday() + 1) % 7}|u={fim}"
    assert fisio["rr"] == esperado, fisio


def test_marcas_do_mes_cabem_em_dois_inteiros(pareado, fora):
    """O calendário mostra o mês sem o aparelho guardar o mês."""
    cliente, cab, ap = pareado

    fora.poe_evento("primary", summary="Dentista",
                    start={"dateTime": "2026-09-17T15:00:00-03:00"},
                    end={"dateTime": "2026-09-17T16:00:00-03:00"})
    fora.poe_evento("primary", summary="Viagem",
                    start={"date": "2026-09-21"}, end={"date": "2026-09-24"})
    fora.poe_tarefa("@default", title="Pagar IPVA",
                    due="2026-09-29T00:00:00.000Z")

    r = cliente.get("/v1/olhar?mes=2026-09", headers=cab).json()

    assert r["mcm"] == "2026-09"
    assert r["mce"] & (1 << 16), r["mce"]          # dia 17
    for dia in (21, 22, 23):                        # a viagem ocupa os três
        assert r["mce"] & (1 << (dia - 1)), dia
    assert not r["mce"] & (1 << 23)                 # o 24 é o fim exclusivo
    assert r["mct"] & (1 << 28), r["mct"]           # dia 29, a tarefa


def test_o_dia_distante_e_consulta_e_nao_vai_para_o_cartao(pareado, fora):
    """Clicou no dia 27, ele busca o dia 27."""
    cliente, cab, ap = pareado

    fora.poe_evento("primary", summary="Casamento",
                    start={"dateTime": "2026-09-27T18:00:00-03:00"},
                    end={"dateTime": "2026-09-27T23:00:00-03:00"})

    # E uma TAREFA que vence no mesmo dia: o calendário marca o dia com
    # os dois marcadores, e abrir um dia que só tinha tarefa mostrava uma
    # tela vazia.
    fora.poe_tarefa("@default", title="Levar presente",
                    due="2026-09-27T00:00:00.000Z")

    r = cliente.get("/v1/olhar?dia=2026-09-27", headers=cab).json()

    assert [i for i in r["dia"] if i["t"] == "Casamento"], r["dia"]
    assert [i for i in r["dia"] if i["t"] == "Levar presente"], r["dia"]

    # O ECO: uma resposta só vale para o dia que ela diz responder. Sem
    # ele, o aparelho aceitava como consulta qualquer resposta que tivesse
    # a chave `dia` — inclusive a vazia que todo pull carregava.
    assert r["dd"] == "2026-09-27", r

    # E NÃO entra na colheita: o cartão continua guardando três dias.
    pull = cliente.get("/v1/pull", headers=cab).json()
    assert not [i for i in pull["itens"] if i["t"] == "Casamento"], pull


def test_o_fim_que_a_fala_disse_vale():
    """"Reunião das 14 às 16" é 14–16, não 14–15."""
    g = Gesto(id="n:1", t="Reunião", d="2026-08-27", h="14:00", f="16:00",
              tp=Tipo.EVENTO)
    assert corpo_de_evento(g)["end"]["dateTime"] == "2026-08-27T16:00:00"


def test_fim_dito_antes_do_inicio_e_do_dia_seguinte():
    """"Das 22 à 1" acaba no dia seguinte, não antes de começar."""
    g = Gesto(id="n:1", t="Festa", d="2026-08-27", h="22:00", f="01:00",
              tp=Tipo.EVENTO)
    assert corpo_de_evento(g)["end"]["dateTime"] == "2026-08-28T01:00:00"


def test_editar_a_hora_mantem_a_duracao():
    """Mudar o início de uma reunião de 2 h não a encolhe para 1 h."""
    g = Gesto(v="editou", id="g:x", t="Reunião", d="2026-08-27", h="14:10",
              tp=Tipo.EVENTO)
    assert corpo_de_evento(g, dura_s=7200)["end"]["dateTime"] == \
        "2026-08-27T16:10:00"


def _puxa_tudo(cliente, cab):
    itens = []
    r = cliente.get("/v1/pull", headers=cab)
    itens += r.json()["itens"]
    while r.json()["mais"]:
        r = cliente.get("/v1/pull", headers=cab)
        itens += r.json()["itens"]
    return itens


def test_renomear_a_lista_no_google_leva_o_nome_novo_as_tarefas(pareado, fora):
    """No vidro: a lista renomeada no Google Tasks continuava com o nome
    velho na Agenda. O Tasks não mexe nas tarefas quando a LISTA muda de
    nome, e elas não desciam de novo — cada uma levava o nome antigo, que
    é por onde o aparelho agrupa."""
    cliente, cab, ap = pareado
    fora.poe_tarefa("casa", title="Arroz")
    fora.poe_tarefa("casa", title="Feijão")
    _puxa_tudo(cliente, cab)

    nome_velho = fora.listas["casa"]["title"]
    fora.listas["casa"]["title"] = "Mercado"
    itens = _puxa_tudo(cliente, cab)

    tarefas = [i for i in itens if i["tp"] == Tipo.TAREFA]
    assert sorted(i["t"] for i in tarefas) == ["Arroz", "Feijão"]
    assert all(i["l"] == "Mercado" for i in tarefas), (nome_velho, tarefas)
    assert [i["t"] for i in itens if i["id"] == "l:casa"] == ["Mercado"]

    # E o pull seguinte, sem mudança nenhuma, não reenvia nada.
    assert not [i for i in _puxa_tudo(cliente, cab) if i["tp"] == Tipo.TAREFA]


def test_lista_apagada_no_google_sai_do_aparelho(pareado, fora):
    """A lista apagada no Google Tasks some da resposta sem deixar rastro,
    e ficava no aparelho até o resync diário — com as tarefas dentro."""
    cliente, cab, ap = pareado
    fora.poe_tarefa("casa", title="Arroz")
    itens = _puxa_tudo(cliente, cab)
    tarefa = [i["id"] for i in itens if i["t"] == "Arroz"][0]

    del fora.listas["casa"]
    for k in [k for k, t in fora.tarefas.items() if t["_lista"] == "casa"]:
        del fora.tarefas[k]

    removidos = []
    r = cliente.get("/v1/pull", headers=cab)
    removidos += [x["id"] for x in r.json()["removidos"]]
    while r.json()["mais"]:
        r = cliente.get("/v1/pull", headers=cab)
        removidos += [x["id"] for x in r.json()["removidos"]]
    assert sorted(removidos) == sorted(["l:casa", tarefa])

    # E não volta a ser anunciada no pull seguinte.
    r = cliente.get("/v1/pull", headers=cab)
    assert r.json()["removidos"] == []


def _removidos(cliente, cab):
    fora_ = []
    r = cliente.get("/v1/pull", headers=cab)
    fora_ += [x["id"] for x in r.json()["removidos"]]
    while r.json()["mais"]:
        r = cliente.get("/v1/pull", headers=cab)
        fora_ += [x["id"] for x in r.json()["removidos"]]
    return fora_


def test_agenda_apagada_no_google_tira_os_eventos_dela(pareado, fora):
    """A agenda apagada (ou deixada) no Google some da lista sem rastro, e
    os eventos dela ficavam no aparelho até o resync diário."""
    cliente, cab, ap = pareado
    casa = "casa#compartilhada@group.calendar.google.com"
    fora.poe_evento(casa, summary="Faxina",
                    start={"dateTime": f"{AMANHA}T10:00:00-03:00"},
                    end={"dateTime": f"{AMANHA}T11:00:00-03:00"})
    itens = _puxa_tudo(cliente, cab)
    faxina = [i["id"] for i in itens if i["t"] == "Faxina"]
    assert faxina

    del fora.agendas[casa]
    assert _removidos(cliente, cab) == faxina
    assert _removidos(cliente, cab) == []          # e não repete


def test_lista_apagada_leva_o_gemeo_da_tarefa_com_hora(pareado, fora):
    """A tarefa com hora tem um evento-gêmeo no Google Agenda, que o Google
    não apaga junto com a lista — ele seguiria na régua do dia."""
    cliente, cab, ap = pareado
    criada = cliente.post(
        "/v1/push", headers={**cab, "operacao": "op-gemeo"},
        json={"v": "criou", "id": "n:1", "t": "Academia",
              "tp": 2, "d": AMANHA, "h": "08:00"}).json()
    assert criada["ok"], criada
    _puxa_tudo(cliente, cab)
    gemeo = [e for e in fora.eventos.values() if e.get("summary") == "Academia"]
    assert len(gemeo) == 1

    del fora.listas["@default"]
    for k in [k for k, t in fora.tarefas.items() if t["_lista"] == "@default"]:
        del fora.tarefas[k]

    assert criada["id"] in _removidos(cliente, cab)
    vivos = [e for e in fora.eventos.values()
             if e.get("summary") == "Academia" and e.get("status") != "cancelled"]
    assert vivos == []


def test_conclusao_a_noite_fica_no_dia_local():
    """21h50 em Brasília já é amanhã em UTC: o dia é o que o Google mostra."""
    from tinto.google import dia_feito, tarefa_para_item
    feito = "2026-10-03T00:50:00.000Z"
    assert dia_feito(feito, "America/Sao_Paulo") == "2026-10-02"
    assert dia_feito(feito, "Asia/Tokyo") == "2026-10-03"
    it = tarefa_para_item({"id": "a", "title": "arroz", "status": "completed",
                           "completed": feito}, "Compras", "America/Sao_Paulo")
    assert it.c == "2026-10-02"
