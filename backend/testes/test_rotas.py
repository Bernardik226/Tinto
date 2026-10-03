"""As rotas, do jeito que o aparelho as usa."""

import json
import time
from datetime import date, timedelta

# A janela do backend é ontem, hoje e amanhã. Quem testa colheita marca
# dentro dela — data fixa de agosto não desce mais, e é o certo.
HOJE = date.today().isoformat()
AMANHA = (date.today() + timedelta(days=1)).isoformat()

import pytest

from tinto import app as rotas
from tinto.memoria import memoria
from tinto import portaria
from tinto.memoria import Memoria
from tinto.modelo import Notas, notas


WAV = b"RIFF$\x00\x00\x00WAVEfmt "        # cabeçalho de mentira: ninguém lê


# ── a fala inteira, do ● ao Google ───────────────────────────────────
def test_a_fala_vira_evento_no_google(pareado, fora):
    """O caminho feliz inteiro, e é o que o aparelho existe para fazer."""
    cliente, cab, ap = pareado

    r = cliente.post("/v1/captura", headers={**cab, "operacao": "cap-1"},
                     files={"audio": ("fala.wav", WAV, "audio/wav")})
    proposta = r.json()

    assert proposta["falou"] == "marca dentista quinta às três"
    assert proposta["acoes"][0]["t"] == "Dentista"
    assert proposta["nota"].startswith("nt:")
    # Propôs, e só. Nada existe no Google ainda.
    assert not fora.eventos

    acao = proposta["acoes"][0]
    r = cliente.post("/v1/push", headers={**cab, "operacao": "push-1"},
                     json={**acao, "nota": proposta["nota"]})

    assert r.json()["ok"], r.json()
    assert len(fora.eventos) == 1


def test_a_captura_repetida_nao_cria_dois_eventos(pareado, fora):
    """O pior bug possível deste sistema."""
    cliente, cab, ap = pareado

    um = cliente.post("/v1/captura", headers={**cab, "operacao": "cap-1"},
                      files={"audio": ("fala.wav", WAV, "audio/wav")}).json()
    dois = cliente.post("/v1/captura", headers={**cab, "operacao": "cap-1"},
                        files={"audio": ("fala.wav", WAV, "audio/wav")}).json()

    assert um["nota"] == dois["nota"]

    corpo = {**um["acoes"][0], "nota": um["nota"]}
    cliente.post("/v1/push", headers={**cab, "operacao": "p-1"}, json=corpo)
    cliente.post("/v1/push", headers={**cab, "operacao": "p-1"}, json=corpo)

    assert len(fora.eventos) == 1


def test_a_nota_deixa_de_estar_aberta_quando_o_gesto_sobe(pareado, fora):
    """Uma nota em `aberta` é uma fala que a pessoa não decidiu."""
    cliente, cab, ap = pareado
    proposta = cliente.post("/v1/captura", headers={**cab, "operacao": "c-1"},
                            files={"audio": ("f.wav", WAV, "audio/wav")}).json()

    assert notas.de(ap.pessoa, proposta["nota"]).estado == "aberta"

    cliente.post("/v1/push", headers={**cab, "operacao": "p-1"},
                 json={**proposta["acoes"][0], "nota": proposta["nota"]})

    assert notas.de(ap.pessoa, proposta["nota"]).estado == "confirmada"


# ── anotação: a única coisa que é nossa ──────────────────────────────
def test_anotacao_nao_toca_o_google_e_fica_no_servidor(pareado, fora):
    """RN-25. Ela não tem equivalente do outro lado, e mandar geraria
    erro eterno — mora aqui, e é onde o Tinto não compete com ninguém."""
    cliente, cab, ap = pareado
    fora.fala = "hoje foi um dia estranho no trabalho"
    fora.acoes = [{"v": "anotou", "t": "Dia estranho", "tp": 1}]

    proposta = cliente.post("/v1/captura", headers={**cab, "operacao": "c-1"},
                            files={"audio": ("f.wav", WAV, "audio/wav")}).json()

    r = cliente.post("/v1/push", headers={**cab, "operacao": "p-1"},
                     json={**proposta["acoes"][0], "nota": proposta["nota"],
                           "d": "2026-08-25"})
    assert r.json()["ok"]
    assert not fora.eventos and not fora.tarefas

    guardadas = cliente.get("/v1/anotacoes", headers=cab).json()["anotacoes"]
    assert guardadas[0]["t"] == "Dia estranho"
    # O CORPO veio da nota: o gesto tem 320 bytes e não carrega texto
    # longo, e sem isto a anotação chegaria só com o cabeçalho.
    assert guardadas[0]["c"] == "hoje foi um dia estranho no trabalho"


def test_renomear_anotacao_nao_cria_uma_segunda(pareado, fora):
    """Sem o ramo do renomear, `editou` cai no criar e a pessoa fica com
    duas onde havia uma — a nova certa e a velha com o nome errado."""
    cliente, cab, ap = pareado
    a = notas.anota(ap.pessoa, _acao("Nome torto"), "nt:1", "2026-08-25")

    r = cliente.post("/v1/push", headers={**cab, "operacao": "p-1"},
                     json={"v": "editou", "id": a.id, "t": "Nome certo",
                           "tp": 1})

    assert r.json()["ok"]
    guardadas = cliente.get("/v1/anotacoes", headers=cab).json()["anotacoes"]
    assert len(guardadas) == 1
    assert guardadas[0]["t"] == "Nome certo"


def test_gesto_sem_tipo_e_recusado_em_vez_de_virar_anotacao(pareado, fora):
    """Escolhendo por ele, o gesto viraria uma anotação silenciosa — e a
    pessoa procuraria no Google para sempre."""
    cliente, cab, ap = pareado
    r = cliente.post("/v1/push", headers={**cab, "operacao": "p-1"},
                     json={"v": "criou", "id": "n:1", "t": "?", "tp": 0})

    assert r.json()["ok"] is False
    assert not cliente.get("/v1/anotacoes", headers=cab).json()["anotacoes"]


def test_anotacao_sobrevive_ao_cartao_e_ao_deploy(tmp_path):
    """*"Apagar o cartão e parear de novo devolve as anotações"* — é a
    promessa que justifica a anotação ter casa própria no servidor.
    """
    arquivo = tmp_path / "notas.json"

    guarda = Notas(str(arquivo))
    guarda.anota("eu@x.com", _acao("O que eu pensei"), "nt:1", "2026-08-25")

    devolta = Notas(str(arquivo))
    minhas = devolta.anotacoes_de("eu@x.com")

    assert len(minhas) == 1
    assert minhas[0].titulo == "O que eu pensei"


def _acao(titulo: str):
    from tinto.contrato import Acao, Tipo
    return Acao(id="n:1", t=titulo, tp=Tipo.ANOTACAO)


# ── o pull ───────────────────────────────────────────────────────────
def test_nenhuma_resposta_do_pull_estoura_o_buffer_do_device(pareado, fora):
    """O device recebe a resposta num `char json[1024]`."""
    cliente, cab, ap = pareado
    for i in range(30):
        fora.poe_tarefa("@default", title=f"Tarefa comprida número {i}" * 2)

    vistos, voltas = 0, 0
    while True:
        r = cliente.get("/v1/pull", headers=cab)
        assert len(r.content) < 1024, f"resposta de {len(r.content)} bytes"
        vistos += len(r.json()["itens"])
        voltas += 1
        if not r.json()["mais"] or voltas > 40:
            break

    assert vistos >= 30          # nada se perdeu no caminho
    assert voltas > 1            # e não coube tudo de uma vez


def test_o_marcador_do_google_so_anda_quando_o_buffer_esvazia(pareado, fora):
    """Avançar antes seria dizer ao Google "já recebi tudo" com metade
    ainda na fila daqui — e o `syncToken` nunca traz de novo o que já deu.
    """
    cliente, cab, ap = pareado
    for i in range(20):
        fora.poe_tarefa("@default", title=f"Tarefa {i}" * 5)

    cliente.get("/v1/pull", headers=cab)
    assert ap.pendentes                      # sobrou coisa
    assert not ap.sinc.tarefas_desde         # e o marcador não andou

    for _ in range(40):
        if not cliente.get("/v1/pull", headers=cab).json()["mais"]:
            break

    assert ap.sinc.tarefas_desde             # agora sim


def test_o_pull_de_novo_nao_repete_o_que_ja_desceu(pareado, fora):
    """A segunda leitura traz só o que mudou. Repetir tudo é o aparelho
    reescrevendo o cartão inteiro a cada quinze minutos."""
    cliente, cab, ap = pareado
    fora.poe_tarefa("@default", title="Comprar pasta")

    primeiro = cliente.get("/v1/pull", headers=cab).json()
    assert any(i["t"] == "Comprar pasta" for i in primeiro["itens"])

    segundo = cliente.get("/v1/pull", headers=cab).json()
    assert not any(i["t"] == "Comprar pasta" for i in segundo["itens"])


def test_a_anotacao_desce_pelo_pull_como_qualquer_item(pareado, fora):
    """UM modelo só. Evento, tarefa, lista e anotação chegam pela mesma
    rota, no mesmo formato e pelo mesmo parser — muda o `tp` e muda a
    origem."""
    cliente, cab, ap = pareado
    notas.anota(ap.pessoa, _acao("Dia estranho"), "nt:abc", "2026-08-25")

    r = cliente.get("/v1/pull", headers=cab).json()
    minha = [i for i in r["itens"] if i["id"].startswith("an:")]

    assert len(minha) == 1
    assert minha[0]["tp"] == 1              # ANOTACAO
    assert minha[0]["o"] == "n"             # nasceu aqui, não veio do Google
    assert minha[0]["t"] == "Dia estranho"
    assert minha[0]["nota"] == "nt:abc"     # o vínculo com a fala


def test_a_anotacao_nao_desce_duas_vezes(pareado, fora):
    """Repetir é o aparelho reescrevendo o cartão a cada quinze minutos."""
    cliente, cab, ap = pareado
    notas.anota(ap.pessoa, _acao("Uma vez só"), "nt:1", "2026-08-25")

    primeiro = cliente.get("/v1/pull", headers=cab).json()["itens"]
    segundo = cliente.get("/v1/pull", headers=cab).json()["itens"]

    assert any(i["id"].startswith("an:") for i in primeiro)
    assert not any(i["id"].startswith("an:") for i in segundo)


def test_cartao_novo_traz_as_anotacoes_de_volta(pareado, fora):
    """*"Apagar o cartão e parear de novo devolve as anotações"* — a
    promessa da Etapa 10, e a razão de a anotação ter casa própria."""
    cliente, cab, ap = pareado
    notas.anota(ap.pessoa, _acao("O que eu pensei"), "nt:1", "2026-08-25")
    cliente.get("/v1/pull", headers=cab)          # já desceu uma vez

    # O resync completo é o que um cartão novo provoca — e ele tem que
    # trazer de volta o que só existe deste lado.
    ap.sinc.recomecar()
    ap.pendentes.clear()

    r = cliente.get("/v1/pull", headers=cab).json()
    assert any(i["t"] == "O que eu pensei" for i in r["itens"])


# ── T-32 · as agendas ────────────────────────────────────────────────
def test_a_tela_de_sincronizacao_recebe_as_agendas(pareado, fora):
    """`estado_t.agendas[]` existe no firmware e a tela desenha; sem esta
    rota ela mostra "nenhuma" para sempre."""
    cliente, cab, ap = pareado
    lista = cliente.get("/v1/agendas", headers=cab).json()["agendas"]

    nomes = {a["t"]: a["on"] for a in lista}
    # A principal se chama "Agenda principal", e não pelo e-mail nem pelo
    # nome que o Google põe no `summary` dela: no app do Google o que se
    # vê é o nome do PERFIL, que o device não tem — e o que ela é, é a
    # agenda principal.
    assert nomes["Agenda principal"] is True
    assert nomes["Feriados no Brasil"] is False      # enche sem informar
    assert nomes["Trabalho"] is False                # já desligada no celular


def test_mais_de_doze_agendas_vem_doze_e_o_total(pareado, fora):
    """O device guarda doze; o servidor diz quantas a conta tem, para a
    tela avisar das que não cabem em vez de sumir com elas."""
    cliente, cab, ap = pareado
    for i in range(14):
        fora.agendas[f"extra{i:02d}@x.com"] = {"id": f"extra{i:02d}@x.com",
                                               "summary": f"Extra {i:02d}"}
    r = cliente.get("/v1/agendas", headers=cab).json()
    assert len(r["agendas"]) == 12
    assert r["total"] == 18
    assert r["agendas"][0]["t"] == "Agenda principal"   # nunca fica de fora


def test_religar_feriados_e_uma_escolha_que_fica(pareado, fora):
    """O palpite do sistema e a resposta da pessoa não são a mesma lista."""
    cliente, cab, ap = pareado
    feriados = "pt.brazilian#holiday@group.v.calendar.google.com"
    fora.poe_evento(feriados, summary="Independência",
                    start={"date": HOJE}, end={"date": AMANHA})

    r = cliente.post("/v1/agendas", headers=cab,
                     json={"id": feriados, "on": True})
    assert r.json()["ok"]

    itens = []
    r = cliente.get("/v1/pull", headers=cab)
    itens += r.json()["itens"]
    while r.json()["mais"]:
        r = cliente.get("/v1/pull", headers=cab)
        itens += r.json()["itens"]

    assert any(i["t"] == "Independência" for i in itens)


def test_desligar_uma_agenda_tira_ela_do_pull(pareado, fora):
    cliente, cab, ap = pareado
    fora.poe_evento("primary", summary="Reunião do time",
                    start={"date": HOJE}, end={"date": AMANHA})

    cliente.post("/v1/agendas", headers=cab,
                 json={"id": "primary", "on": False})

    r = cliente.get("/v1/pull", headers=cab).json()
    assert not any(i["t"] == "Reunião do time" for i in r["itens"])


def test_desligar_manda_o_device_apagar_o_que_ja_desceu(pareado, fora):
    """**O que fecha a Etapa 12.4.**"""
    cliente, cab, ap = pareado
    fora.poe_evento("primary", summary="Reunião do time",
                    start={"date": HOJE}, end={"date": AMANHA})

    # Um pull comum NÃO manda zerar: é o delta de todo dia, e apagar a cada
    # quinze minutos o que acabou de descer é o aparelho piscando sozinho.
    cliente.get("/v1/pull", headers=cab)
    assert cliente.get("/v1/pull", headers=cab).json()["zerar"] is False

    cliente.post("/v1/agendas", headers=cab,
                 json={"id": "primary", "on": False})

    assert cliente.get("/v1/pull", headers=cab).json()["zerar"] is True

    # E UMA vez só. O bit viaja com a colheita; repeti-lo no pull seguinte
    # faria o device apagar o que a colheita acabou de entregar.
    assert cliente.get("/v1/pull", headers=cab).json()["zerar"] is False


def test_o_resync_diario_tambem_manda_zerar(pareado, fora):
    """As duas causas de recomeço são diferentes e igualmente válidas."""
    cliente, cab, ap = pareado

    cliente.get("/v1/pull", headers=cab)
    assert cliente.get("/v1/pull", headers=cab).json()["zerar"] is False

    # Um dia inteiro sem resync completo.
    ap.sinc.cheio_em = 0.0

    assert cliente.get("/v1/pull", headers=cab).json()["zerar"] is True


def test_agenda_que_nao_e_da_conta_nao_existe(pareado, fora):
    cliente, cab, ap = pareado
    r = cliente.post("/v1/agendas", headers=cab,
                     json={"id": "a-agenda-de-outro@x.com", "on": False})
    assert r.status_code == 404


# ── quem é quem ──────────────────────────────────────────────────────
def test_sem_token_nao_se_lê_nada(cliente):
    assert cliente.get("/v1/pull").status_code == 401
    assert cliente.post("/v1/push", json={"id": "x"}).status_code == 401
    assert cliente.get("/v1/anotacoes").status_code == 401


def test_saber_o_device_id_nao_da_o_token_do_aparelho(cliente):
    """A fronteira 4 do `ENGENHARIA.md` §7, na rota."""
    from tinto.memoria import memoria
    memoria.provisiona("AA:BB:CC")

    meu = cliente.post("/v1/registrar",
                       json={"device_id": "AA:BB:CC", "prova": "meu-segredo"})
    assert meu.status_code == 200

    ladrao = cliente.post("/v1/registrar",
                          json={"device_id": "AA:BB:CC", "prova": "chute"})
    assert ladrao.status_code == 401

    de_novo = cliente.post("/v1/registrar",
                           json={"device_id": "AA:BB:CC",
                                 "prova": "meu-segredo"})
    assert de_novo.json()["device_token"] == meu.json()["device_token"]


def test_aparelho_sem_conta_recebe_409_e_nao_401(cliente):
    """O token é válido, e o que falta é outra coisa. O device distingue
    os dois: sem conta ele mostra a tela de parear, e com token inválido
    ele se registra de novo."""
    from tinto.memoria import memoria
    memoria.provisiona("AA:BB:CC")

    r = cliente.post("/v1/registrar", json={"device_id": "AA:BB:CC"})
    cab = {"Authorization": f"Bearer {r.json()['device_token']}"}

    assert cliente.get("/v1/pull", headers=cab).status_code == 409


def test_a_anotacao_de_outra_conta_nao_existe(pareado, fora):
    """RN-92: 404 e não 403. Negar a existência não vaza nem o fato de
    existir."""
    cliente, cab, ap = pareado
    minha = notas.anota("outro@x.com", _acao("Segredo"), "nt:1", "2026-08-25")

    r = cliente.post("/v1/push", headers={**cab, "operacao": "p-1"},
                     json={"v": "apagou", "id": minha.id, "tp": 1})

    assert r.json()["ok"] is False
    assert notas.anotacao_de("outro@x.com", minha.id) is not None


def test_parear_de_novo_esquece_a_sincronizacao_da_conta_anterior(tmp_path):
    """Os `syncToken` da conta anterior fariam o primeiro pull vir vazio —
    e o aparelho diria que a agenda da pessoa não tem nada."""
    m = Memoria(str(tmp_path / "e.json"))
    ap = m.provisiona("AA:BB:CC")
    ap.sinc.tokens["primary"] = "velho"

    p = m.novo_codigo("AA:BB:CC")
    m.confirma_pareamento(p.codigo, "outra@x.com", "ref-2")

    assert ap.sinc.tokens == {}


def test_o_pareamento_sobrevive_a_um_reload(tmp_path):
    """Refazer o pareamento a cada linha de Python alterada não é
    inconveniência: é o que faz parar de testar."""
    arquivo = str(tmp_path / "estado.json")

    m = Memoria(arquivo)
    ap = m.provisiona("AA:BB:CC")
    p = m.novo_codigo("AA:BB:CC")
    m.confirma_pareamento(p.codigo, "eu@x.com", "ref-1")

    depois = Memoria(arquivo)
    volta = depois.por_token(ap.token)

    assert volta is not None
    assert volta.pessoa == "eu@x.com"
    assert volta.google_refresh == "ref-1"


# ── quem eu deixo entrar ─────────────────────────────────────────────
def test_registrar_sozinho_nao_da_acesso_a_nada(cliente):
    """O aparelho se registra sem aprovação, e isso não abre nada."""
    from tinto.memoria import memoria

    r = cliente.post("/v1/registrar", json={"device_id": "AA:BB:CC",
                                            "prova": "segredo-do-aparelho"})
    assert r.status_code == 200
    cab = {"Authorization": f"Bearer {r.json()['device_token']}"}

    assert cliente.get("/v1/pull", headers=cab).status_code == 409
    assert memoria.por_device["AA:BB:CC"].pessoa == ""
    assert "segredo-do-aparelho" not in repr(memoria.por_device["AA:BB:CC"])


def test_registro_sem_conta_expira_em_sete_dias(cliente):
    """Registrar é aberto; o lixo de quem nunca vinculou não se acumula."""
    from tinto.memoria import memoria

    cliente.post("/v1/registrar", json={"device_id": "AA:BB:01", "prova": "a"})
    cliente.post("/v1/registrar", json={"device_id": "AA:BB:02", "prova": "b"})
    memoria.por_device["AA:BB:02"].pessoa = "eu@x.com"
    for ap in memoria.por_device.values():
        ap.visto_em -= 8 * 86400

    cliente.post("/v1/registrar", json={"device_id": "AA:BB:03", "prova": "c"})

    assert "AA:BB:01" not in memoria.por_device      # sem conta: saiu
    assert "AA:BB:02" in memoria.por_device          # com conta: fica


def test_conta_fora_da_lista_e_recusada_sem_dizer_qual_e_a_lista(
        cliente, fora, monkeypatch):
    """Dizer "seu e-mail não está entre os convidados" confirma que existe
    uma lista e convida a tentar outro. A recusa seca não convida nada.
    """
    from tinto import config, oauth, pwa as app_pessoa
    from tinto.memoria import memoria

    monkeypatch.setattr(config, "CONTAS", {"eu@x.com": None})
    monkeypatch.setattr(rotas.config, "GOOGLE_REDIRECT", "https://x/oauth")

    async def quem_e(_):
        return "estranho@x.com"

    monkeypatch.setattr(oauth, "quem_e", quem_e)

    app_pessoa._indo["n1"] = "/e"
    r = cliente.get("/oauth/retorno", params={"code": "c", "state": "pwa:n1"},
                    follow_redirects=False)

    assert r.status_code == 403
    assert "eu@x.com" not in r.text
    assert memoria.refresh_de("estranho@x.com") == ""


# ── desvincular ──────────────────────────────────────────────────────
def test_desparear_tira_a_conta_e_deixa_as_anotacoes(pareado, fora):
    """*"O aparelho continua sendo um Tinto inteiro depois de removido"*."""
    cliente, cab, ap = pareado
    notas.anota(ap.pessoa, _acao("O que eu pensei"), "nt:1", "2026-08-25")
    ap.sinc.tokens["primary"] = "um-token-velho"

    r = cliente.post("/v1/desparear", headers=cab)
    assert r.json() == {"ok": True, "pareado": False}

    assert ap.pessoa == "" and ap.google_refresh == ""
    # O `syncToken` da conta que saiu não pode sobreviver: com ele, o
    # primeiro pull da conta seguinte viria vazio.
    assert ap.sinc.tokens == {}
    assert cliente.get("/v1/pull", headers=cab).status_code == 409
    assert len(notas.anotacoes_de("eu@x.com")) == 1


def test_reparear_depois_de_desvincular_traz_a_agenda_de_novo(pareado, fora):
    cliente, cab, ap = pareado
    cliente.post("/v1/desparear", headers=cab)

    fora.poe_evento("primary", summary="Reunião",
                    start={"date": HOJE}, end={"date": AMANHA})

    ap.pessoa, ap.google_refresh = "eu@x.com", "ref-1"
    r = cliente.get("/v1/pull", headers=cab).json()

    assert any(i["t"] == "Reunião" for i in r["itens"])


# ── o áudio ──────────────────────────────────────────────────────────
def test_caminho_de_arquivo_e_recusado_sem_a_raiz_declarada(pareado, fora):
    """Um endpoint que abre o arquivo que o cliente pedir é um endpoint
    que lê `/etc/passwd` — e o cliente aqui é um aparelho de mesa com um
    token que pode vazar num dump de flash."""
    cliente, cab, ap = pareado
    r = cliente.post("/v1/captura", headers={**cab, "operacao": "c-1"},
                     json={"wav": "/etc/passwd"})

    assert r.status_code == 415


def test_caminho_de_arquivo_funciona_na_bancada(pareado, fora, tmp_path,
                                                monkeypatch):
    """O simulador roda na mesma máquina que o backend, e mandar 300 KB
    de WAV para `localhost` que vai abrir o mesmo arquivo é trabalho por
    nada."""
    cliente, cab, ap = pareado
    (tmp_path / "fala.wav").write_bytes(WAV)
    monkeypatch.setattr(rotas, "AUDIO_RAIZ", str(tmp_path))

    r = cliente.post("/v1/captura", headers={**cab, "operacao": "c-1"},
                     json={"wav": "fala.wav"})

    assert r.status_code == 200
    assert r.json()["falou"]


def test_a_raiz_da_bancada_nao_deixa_subir(pareado, fora, tmp_path,
                                           monkeypatch):
    cliente, cab, ap = pareado
    monkeypatch.setattr(rotas, "AUDIO_RAIZ", str(tmp_path))

    r = cliente.post("/v1/captura", headers={**cab, "operacao": "c-1"},
                     json={"wav": "../../etc/passwd"})

    assert r.status_code == 404


# ── configuração ─────────────────────────────────────────────────────
def test_chave_faltando_diz_qual(pareado):
    """A diferença entre um deploy que se conserta em um minuto e uma
    tarde lendo log."""
    cliente, cab, ap = pareado
    r = cliente.get("/v1/pull", headers=cab)

    assert r.status_code == 503
    assert "GOOGLE_CLIENT_ID" in r.json()["detail"]


def test_saude_lista_o_que_falta(cliente):
    corpo = cliente.get("/saude").json()
    assert corpo["ok"] is True
    assert isinstance(corpo["falta"], list)
    assert json.dumps(corpo)          # serializa sem susto


# ── a saúde ──────────────────────────────────────────────────────────
def test_saude_diz_o_NOME_da_variavel_que_falta(cliente, monkeypatch):
    """Devolver o valor sentinela dá `["(não configurado)", "(não
    configurado)"]` — a mesma resposta para quatro problemas diferentes,
    num lugar que existe exatamente para dizer qual é."""
    monkeypatch.setattr(rotas.config, "GOOGLE_CLIENT_ID", rotas.config.FALTANDO)
    monkeypatch.setattr(rotas.config, "GROQ_API_KEY", "tem")

    falta = cliente.get("/saude").json()["falta"]
    assert "GOOGLE_CLIENT_ID" in falta
    assert "GROQ_API_KEY" not in falta


# ── a conta que caiu ─────────────────────────────────────────────────
def test_conta_revogada_pede_para_reconectar_em_vez_de_dar_500(pareado, fora):
    """`invalid_grant` é o Google dizendo "esta concessão acabou"."""
    cliente, cab, ap = pareado
    fora.concessao_morta = True

    r = cliente.get("/v1/pull", headers=cab)
    assert r.status_code == 428
    assert ap.reconectar is True


def test_todas_as_rotas_do_google_dizem_a_mesma_coisa(pareado, fora):
    """Um lugar só. Sem isso, uma concessão morta dá 500 no pull e 500 no
    push."""
    cliente, cab, ap = pareado
    fora.concessao_morta = True

    assert cliente.get("/v1/pull", headers=cab).status_code == 428
    assert cliente.get("/v1/agendas", headers=cab).status_code == 428
    r = cliente.post("/v1/push", headers={**cab, "operacao": "p-1"},
                     json={"v": "criou", "id": "n:1", "t": "X", "tp": 2})
    assert r.status_code == 428


def test_reconectar_limpa_a_marca(pareado, fora):
    """A marca é sobre o token que morreu. Um pareamento novo traz um
    token novo, e carregar a bandeira do anterior faria o aparelho pedir
    para reconectar uma conta que acabou de conectar."""
    cliente, cab, ap = pareado
    fora.concessao_morta = True
    cliente.get("/v1/pull", headers=cab)
    assert ap.reconectar is True

    from tinto.memoria import memoria
    p = memoria.novo_codigo(ap.device_id)
    memoria.confirma_pareamento(p.codigo, "eu@x.com", "ref-2")

    assert ap.reconectar is False




# ── o código de pareamento ───────────────────────────────────────────
def test_o_endereco_antigo_leva_ao_aplicativo_com_o_codigo_no_bolso(
        cliente):
    """`/conectar` disparava o consentimento POR APARELHO, com as seis
    letras no `state`. Ele continua de pé porque endereço que alguém já
    digitou uma vez não se apaga — mas hoje só aponta para `/e`."""
    r = cliente.get("/conectar", params={"codigo": "abcdef"},
                    follow_redirects=False)

    assert r.status_code == 303
    assert r.headers["location"] == "/e?codigo=ABCDEF"

    # E o que não parece um código não vira parâmetro de URL.
    r = cliente.get("/conectar", params={"codigo": "<script>"},
                    follow_redirects=False)
    assert r.headers["location"] == "/e"


def test_o_codigo_nao_e_reservado_antes_do_login(cliente, monkeypatch):
    """O código é digitado DEPOIS de entrar, e nunca atravessa o OAuth."""
    from tinto import pwa as app_pessoa

    monkeypatch.setattr(rotas.config, "GOOGLE_CLIENT_ID", "cid")
    monkeypatch.setattr(rotas.config, "GOOGLE_REDIRECT", "https://x/oauth")

    r = cliente.get("/e/google", params={"codigo": "ABCDEF"},
                    follow_redirects=False)

    # Nem no `state`, nem guardado ao lado do nonce.
    assert "ABCDEF" not in r.headers["location"]
    assert list(app_pessoa._indo.values()) == ["/e"]


def test_vincular_sem_credencial_do_google_nao_deixa_o_aparelho_verde(
        cliente):
    """O pior estado deste sistema: o Tinto diz "conectado como fulano", e
    todo `pull` volta vazio — a pessoa conclui que a
    agenda dela está vazia.
    """
    from tinto import pwa as app_pessoa
    from tinto.memoria import memoria

    memoria.provisiona("AA:BB:CC")
    p = memoria.novo_codigo("AA:BB:CC")

    cliente.cookies.set("tinto_pessoa", "s1")
    app_pessoa._pessoas["s1"] = "eu@x.com"          # sessão sem refresh

    r = cliente.post("/e/api/vincular", json={"codigo": p.codigo})

    # 409: o problema não é o código, é a sessão sem credencial — e a
    # resposta diz isso junto do campo, em vez de mandar a pessoa para
    # outra página descobrir.
    assert r.status_code == 409
    assert "Entre de novo" in r.json()["motivo"]
    assert memoria.por_device["AA:BB:CC"].pessoa == ""
    # E o código NÃO foi gasto: o erro não é dela, e ele ainda serve.
    assert p.codigo in memoria.pareamentos


def test_os_dois_erros_de_vincular_pedem_coisas_diferentes(cliente):
    """Um código que não vale se resolve pedindo outro no aparelho. Uma
    sessão sem credencial se resolve entrando de novo. São cinco segundos
    de diferença entre saber qual dos dois é e tentar o mesmo código três
    vezes."""
    from tinto import pwa as app_pessoa
    from tinto.memoria import memoria

    cliente.cookies.set("tinto_pessoa", "s1")
    app_pessoa._pessoas["s1"] = "eu@x.com"
    memoria.guarda_pessoa("eu@x.com", "ref-1")

    # O erro volta JUNTO da resposta, e não numa página nova: o código
    # expira em cinco minutos, e mandar a pessoa para outra tela para ler
    # "não vale mais" gasta parte do prazo que ela ainda tinha.
    r = cliente.post("/e/api/vincular", json={"codigo": "ZZZZZZ"})
    assert r.status_code == 404
    assert "peça outro no aparelho" in r.json()["motivo"]

    memoria.pessoas.pop("eu@x.com", None)
    r = cliente.post("/e/api/vincular", json={"codigo": "ZZZZZZ"})
    assert r.status_code == 409
    assert "Entre de novo" in r.json()["motivo"]


def test_codigo_expirado_devolve_pagina_com_o_caminho_de_volta(cliente,
                                                               monkeypatch):
    """Quem lê isto é uma pessoa num navegador, no fim de um consentimento
    que ela acabou de dar. Um JSON de 400 a deixa sem saber se perdeu a
    conta ou o que fazer agora."""
    from tinto import oauth

    async def trocar(_):
        return {"refresh": "r", "access": "a"}

    async def quem_e(_):
        return "eu@x.com"

    monkeypatch.setattr(rotas.config, "GOOGLE_CLIENT_SECRET", "s")
    monkeypatch.setattr(oauth, "trocar_codigo", trocar)
    monkeypatch.setattr(oauth, "quem_e", quem_e)

    r = cliente.get("/oauth/retorno",
                    params={"code": "c", "state": "JAMORREU"})

    assert r.status_code == 400
    # E o caminho de volta é o aplicativo.
    assert 'href="/e"' in r.text


def test_o_pareamento_aberto_sobrevive_a_um_deploy(tmp_path):
    """Ele se perde justamente no intervalo em que a pessoa está numa
    página que o servidor não controla — entre clicar em conectar e o
    Google devolver. Um deploy nesse intervalo devolveria "código
    inválido" sem nada de errado ter acontecido."""
    arquivo = str(tmp_path / "estado.json")

    m = Memoria(arquivo)
    m.provisiona("AA:BB:CC")
    p = m.novo_codigo("AA:BB:CC")

    depois = Memoria(arquivo)
    ap = depois.confirma_pareamento(p.codigo, "eu@x.com", "ref-1")

    assert ap is not None
    assert ap.pessoa == "eu@x.com"


def test_recarregar_a_volta_do_google_nao_da_500(cliente, fora, monkeypatch):
    """O `code` do Google vale UMA vez. Recarregar a página é o jeito mais
    comum de cair aqui, e um 500 parece defeito do servidor quando é o
    protocolo funcionando como devia."""
    from tinto import pwa as app_pessoa

    monkeypatch.setattr(rotas.config, "GOOGLE_CLIENT_SECRET", "s")
    app_pessoa._indo["n9"] = "/e"
    fora.codigos_gastos.add("ja-usado")   # o Google já trocou este por token

    r = cliente.get("/oauth/retorno", params={"code": "ja-usado",
                                              "state": "pwa:n9"})

    assert r.status_code == 400
    assert "já foi usado" in r.text


def test_cancelar_o_consentimento_nao_parece_defeito(cliente, monkeypatch):
    monkeypatch.setattr(rotas.config, "GOOGLE_CLIENT_SECRET", "s")

    r = cliente.get("/oauth/retorno", params={"error": "access_denied"})

    assert r.status_code == 400
    assert "não foi concluído" in r.text


# ── quem é desenvolvedor ─────────────────────────────────────────────
def _pedido_falso():
    """O mínimo que `abre_sessao` lê do request: o esquema da URL."""
    from types import SimpleNamespace
    return SimpleNamespace(url=SimpleNamespace(scheme="https"), cookies={})


# ── o aplicativo da pessoa ───────────────────────────────────────────
def test_o_aplicativo_pede_login_antes_de_tudo(cliente):
    """O consentimento acontece no NAVEGADOR, e é lá porque é lá que ele
    PODE acontecer: um dump do flash do ESP32 revela, no pior caso, um
    token deste serviço — nunca a credencial Google de ninguém."""
    pagina = cliente.get("/e").text
    assert "Entrar com o Google" in pagina


def test_a_pessoa_anexa_aparelhos_com_as_seis_letras(cliente, fora):
    """A inversão: o consentimento é da PESSOA, e o aparelho se anexa."""
    from tinto import pwa as app_pessoa
    from tinto.memoria import memoria

    memoria.provisiona("AA:BB:CC", "o da mesa")
    memoria.guarda_pessoa("eu@x.com", "ref-1")

    # A sessão da pessoa, como se ela tivesse acabado de consentir.
    cliente.cookies.set("tinto_pessoa", "s1")
    app_pessoa._pessoas["s1"] = "eu@x.com"

    p = memoria.novo_codigo("AA:BB:CC")
    r = cliente.post("/e/api/vincular", json={"codigo": p.codigo.lower()})
    assert r.status_code == 200

    ap = memoria.por_device["AA:BB:CC"]
    assert ap.pessoa == "eu@x.com"
    assert ap.google_refresh == "ref-1"

    # E o aparelho aparece na conta DELA, com o nome dele.
    # Pela API: o PWA é estático desde a fase 4, e quem responde os
    # dados é o JSON.
    conta = cliente.get("/e/api/conta").json()
    assert [a["nome"] for a in conta["aparelhos"]] == ["o da mesa"]


def test_a_pessoa_so_ve_e_solta_o_que_e_dela(cliente, fora):
    """RN-92: 404 e não 403 para o que é de outro — negar a existência não
    vaza nem o fato de existir."""
    from tinto import pwa as app_pessoa
    from tinto.memoria import memoria

    outro = memoria.provisiona("FF:FF:FF", "o de outra pessoa")
    outro.pessoa = "outra@x.com"

    cliente.cookies.set("tinto_pessoa", "s1")
    app_pessoa._pessoas["s1"] = "eu@x.com"

    assert "o de outra pessoa" not in cliente.get("/e").text
    r = cliente.post("/e/api/soltar", params={"device_id": "FF:FF:FF"})
    assert r.status_code == 404
    assert memoria.por_device["FF:FF:FF"].pessoa == "outra@x.com"


def test_desvincular_pelo_aplicativo_solta_o_aparelho(cliente, fora):
    """O mesmo vínculo que o botão do Tinto desfaz, visto do outro lado."""
    from tinto import pwa as app_pessoa
    from tinto.memoria import memoria

    ap = memoria.provisiona("AA:BB:CC")
    ap.pessoa = "eu@x.com"
    ap.google_refresh = "ref-1"
    ap.sinc.tokens["primary"] = "velho"

    cliente.cookies.set("tinto_pessoa", "s1")
    app_pessoa._pessoas["s1"] = "eu@x.com"

    cliente.post("/e/api/soltar", params={"device_id": "AA:BB:CC"})

    assert ap.pessoa == "" and ap.google_refresh == ""
    assert ap.sinc.tokens == {}


def test_segundo_login_sem_refresh_nao_desliga_os_aparelhos(cliente):
    """O Google só devolve o refresh token na PRIMEIRA autorização."""
    from tinto.memoria import memoria

    memoria.guarda_pessoa("eu@x.com", "ref-1")
    memoria.guarda_pessoa("eu@x.com", "")

    assert memoria.refresh_de("eu@x.com") == "ref-1"


def test_novo_login_renova_credencial_dos_aparelhos_da_pessoa(cliente):
    """O token pertence à pessoa; uma cópia velha no aparelho não pode
    sobreviver ao login que acabou de renovar a concessão."""
    from tinto.memoria import memoria

    ap = memoria.provisiona("AA:BB:CC")
    ap.pessoa = "eu@x.com"
    ap.google_refresh = "ref-antigo"
    ap.reconectar = True

    memoria.guarda_pessoa("eu@x.com", "ref-novo")

    assert memoria.refresh_de("eu@x.com") == "ref-novo"
    assert ap.google_refresh == "ref-novo"
    assert ap.reconectar is False


def test_os_icones_do_aplicativo_sao_servidos_e_nada_mais(cliente):
    """A rota é `/e/{nome}`, e ela mora DEPOIS das rotas com nome próprio."""
    r = cliente.get("/e/icone-192.png")
    assert r.status_code == 200
    assert r.headers["content-type"] == "image/png"
    assert "immutable" in r.headers["cache-control"]

    assert cliente.get("/e/manifest.json").json()["name"] == "Tinto App"

    for fora in ("index.html", "../backend/.env", "qualquer.png"):
        assert cliente.get(f"/e/{fora}").status_code == 404

    # E a rota nomeada continua ganhando do curinga.
    r = cliente.get("/e/google", follow_redirects=False)
    assert r.status_code != 404


def test_a_saude_avisa_quando_ninguem_pode_entrar(cliente, monkeypatch):
    """TINTO_CONTAS vazia fecha o servidor para todos: é o primeiro passo
    de quem sobe um servidor novo, e o mais fácil de esquecer."""
    from tinto import config

    monkeypatch.setattr(config, "CONTAS", {})
    assert "TINTO_CONTAS" in cliente.get("/saude").json()["aviso"]

    monkeypatch.setattr(config, "CONTAS", {"eu@x.com": None})
    saude = cliente.get("/saude").json()
    assert saude["aviso"] == ""
    assert saude["contas"] == 1
    # Nenhum e-mail no corpo: a contagem não vaza quem é.
    assert "eu@x.com" not in cliente.get("/saude").text


# ── a portaria ───────────────────────────────────────────────────────
def test_cabecalhos_de_seguranca_em_toda_resposta(cliente):
    """Middleware e não decorador: uma rota nova nasce protegida sem
    ninguém lembrar de anotá-la.
    """
    for rota in ("/saude", "/e"):
        h = cliente.get(rota).headers
        assert h["X-Frame-Options"] == "DENY", rota
        assert "frame-ancestors 'none'" in h["Content-Security-Policy"], rota
        assert h["X-Content-Type-Options"] == "nosniff", rota
        assert h["Referrer-Policy"] == "same-origin", rota

    # HSTS só sob HTTPS: mandado em `http://localhost`, ele ensinaria o
    # navegador a exigir HTTPS de localhost para sempre — e isso não se
    # desfaz sem limpar o estado do navegador.
    assert "Strict-Transport-Security" not in cliente.get("/saude").headers

    # E atrás de um balanceador ele PRECISA sair, que é onde serve para
    # alguma coisa. O proxy termina o TLS e fala com o app em texto puro:
    # perguntar só a `request.url.scheme` fazia o HSTS nunca ser mandado
    # justamente em produção.
    h = cliente.get("/saude", headers={"x-forwarded-proto": "https"}).headers
    assert "max-age=" in h["Strict-Transport-Security"]


def test_registrar_para_de_responder_a_quem_varre(cliente):
    """Não há senha aqui, e o que se protege não é senha: é a ENUMERAÇÃO."""
    for i in range(20):
        r = cliente.post("/v1/registrar",
                         json={"device_id": f"AA:BB:{i:02d}", "prova": "x"})
        assert r.status_code == 200, i

    r = cliente.post("/v1/registrar", json={"device_id": "AA:BB:99",
                                            "prova": "x"})
    assert r.status_code == 429
    # 429 é sobre RITMO, não sobre permissão: ele não conta nada do que
    # existe do outro lado, e quem estourou por acidente lê "espere".
    assert r.headers.get("Retry-After")

    # O device de verdade não sente: ele tenta uma vez a cada trinta
    # segundos, e o teto é por minuto.
    portaria.esquece_tudo()
    assert cliente.post("/v1/registrar",
                        json={"device_id": "AA:BB:CC",
                              "prova": "p"}).status_code == 200


def test_vincular_para_de_aceitar_chute_de_seis_letras(cliente, fora):
    """24 letras em 6 posições: 191 milhões de combinações, e um código
    vale cinco minutos. Chutar dá muito trabalho — mas dá, e quem
    acertasse amarraria o Tinto de outra pessoa à própria conta.
    """
    from tinto import pwa as app_pessoa

    cliente.cookies.set("tinto_pessoa", "s1")
    app_pessoa._pessoas["s1"] = "eu@x.com"
    memoria.guarda_pessoa("eu@x.com", "ref-1")

    for _ in range(20):
        r = cliente.post("/e/api/vincular", json={"codigo": "ZZZZZZ"})
        assert r.status_code == 404

    assert cliente.post("/e/api/vincular", json={"codigo": "ZZZZZZ"},
                        follow_redirects=False).status_code == 429


def test_o_limite_e_por_endereco_e_nao_global(cliente):
    """Um contador só faria o primeiro a estourar trancar todo mundo."""
    for i in range(21):
        cliente.post("/v1/registrar", json={"device_id": f"CC:DD:{i:02d}"},
                     headers={"x-forwarded-for": "203.0.113.1"})

    # Aquele endereço estourou.
    assert cliente.post("/v1/registrar", json={"device_id": "CC:DD:99"},
                        headers={"x-forwarded-for": "203.0.113.1"}
                        ).status_code == 429

    # E o vizinho não paga por isso.
    assert cliente.post("/v1/registrar", json={"device_id": "CC:DD:98"},
                        headers={"x-forwarded-for": "198.51.100.7"}
                        ).status_code == 200


# ── long polling ─────────────────────────────────────────────────────
def test_o_pull_com_espera_segura_a_resposta_ate_ter_o_que_dizer(
        pareado, fora, monkeypatch):
    """A latência deixa de ser "meio ciclo do device"."""
    cliente, cab, ap = pareado
    monkeypatch.setattr(rotas, "ESPERA_MAX_S", 0.6)
    monkeypatch.setattr(rotas, "ESPERA_PASSO_S", 0.1)

    # Esvazia o que houver, para a próxima colheita começar do zero.
    cliente.get("/v1/pull", headers=cab)

    # Nada novo: ele segura até o teto e volta vazio — sem erro.
    inicio = time.monotonic()
    r = cliente.get("/v1/pull?esperar=1", headers=cab)
    demorou = time.monotonic() - inicio

    assert r.status_code == 200
    assert r.json()["itens"] == []
    assert demorou >= 0.5, "não esperou"

    # E com novidade, ele responde SEM esperar o teto.
    fora.poe_evento("primary", summary="Dentista",
                    start={"dateTime": f"{HOJE}T15:00:00-03:00"},
                    end={"dateTime": f"{HOJE}T16:00:00-03:00"})

    inicio = time.monotonic()
    r = cliente.get("/v1/pull?esperar=1", headers=cab)
    demorou = time.monotonic() - inicio

    assert any(i["t"] == "Dentista" for i in r.json()["itens"])
    assert demorou < 0.5, "esperou o teto mesmo tendo o que dizer"


def test_sem_esperar_a_resposta_sai_na_hora(pareado, fora, monkeypatch):
    """`esperar` é OPT-IN, e é isso que mantém o resto funcionando."""
    cliente, cab, ap = pareado
    monkeypatch.setattr(rotas, "ESPERA_MAX_S", 5.0)

    cliente.get("/v1/pull", headers=cab)

    inicio = time.monotonic()
    r = cliente.get("/v1/pull", headers=cab)

    assert r.status_code == 200
    assert time.monotonic() - inicio < 1.0


def test_com_buffer_cheio_ele_nao_espera(pareado, fora, monkeypatch):
    """Segurar com algo na mão seria atrasar de propósito o que já está
    pronto — e o buffer existe justamente porque a colheita não coube numa
    resposta só."""
    cliente, cab, ap = pareado
    monkeypatch.setattr(rotas, "ESPERA_MAX_S", 5.0)

    for i in range(30):
        fora.poe_evento("primary", summary=f"Evento {i}",
                        start={"date": HOJE},
                        end={"date": AMANHA})

    r = cliente.get("/v1/pull?esperar=1", headers=cab).json()
    assert r["mais"] is True, "a colheita deveria ter sobrado no buffer"

    inicio = time.monotonic()
    r = cliente.get("/v1/pull?esperar=1", headers=cab)
    assert time.monotonic() - inicio < 1.0
    assert r.json()["itens"]


def test_o_device_pode_pedir_a_colheita_inteira(pareado, fora):
    """`tudo=1`: o aparelho diz "não tenho nada, me manda tudo"."""
    cliente, cab, ap = pareado
    fora.poe_evento("primary", summary="Dentista",
                    start={"dateTime": f"{HOJE}T15:00:00-03:00"},
                    end={"dateTime": f"{HOJE}T16:00:00-03:00"})

    # A primeira colheita entrega, e o token avança.
    r = cliente.get("/v1/pull", headers=cab).json()
    assert any(i["t"] == "Dentista" for i in r["itens"])
    assert ap.sinc.tokens, "o token deveria ter avançado"

    # Daqui em diante o Google não repete: pull vazio, para sempre.
    assert cliente.get("/v1/pull", headers=cab).json()["itens"] == []

    # Foi o que aconteceu de verdade: o dado se perdeu no caminho, e o
    # aparelho ficou com a agenda em branco sem nada explicando.
    r = cliente.get("/v1/pull?tudo=1", headers=cab).json()
    assert any(i["t"] == "Dentista" for i in r["itens"])

    # E vem com `zerar`: é a verdade INTEIRA, e o que estiver no cartão de
    # uma colheita anterior precisa sair antes.
    assert r["zerar"] is True


def test_uma_fala_sem_comando_vira_anotacao(pareado, fora, monkeypatch):
    """Zero ações ainda é uma FALA."""
    cliente, cab, ap = pareado

    async def sem_comando(falou, tz_min=0, existentes=None):
        return []

    monkeypatch.setattr(rotas.ia, "estruturar", sem_comando)

    r = cliente.post("/v1/captura", headers={**cab, "operacao": "op-1"},
                     files={"audio": ("fala.wav", b"RIFF....WAVEfmt ",
                                      "audio/wav")}).json()

    assert len(r["acoes"]) == 1
    acao = r["acoes"][0]
    assert acao["tp"] == 1, "TIPO_ANOTACAO"
    assert acao["v"] == "anotou"
    assert acao["t"], "uma anotação sem título é uma linha em branco"

    # E a transcrição vem inteira, que é o conteúdo dela.
    assert r["falou"]
    assert r["nota"]


def test_o_titulo_da_anotacao_corta_onde_a_frase_respira():
    """Meia palavra num título é pior que um título curto."""
    from tinto.app import _titulo_da_fala

    # Corta na pontuação.
    assert _titulo_da_fala(
        "comprar pasta térmica. depois ver o datasheet"
    ) == "comprar pasta térmica"

    # Sem pontuação e cabendo, vai inteiro.
    assert _titulo_da_fala("ideia do encoder") == "ideia do encoder"

    # Comprido demais corta na PALAVRA, nunca no meio dela.
    longo = _titulo_da_fala("a" * 20 + " " + "b" * 20 + " " + "c" * 40)
    assert longo.endswith("…")
    assert "c" not in longo, "cortou no meio de uma palavra"

    # Fala vazia ainda tem título: a linha não pode ficar em branco.
    assert _titulo_da_fala("") == "Anotação"
    assert _titulo_da_fala("   ") == "Anotação"


def test_o_push_devolve_o_id_como_o_pull_manda(pareado, fora):
    """O device conhece a coisa por UM id, e o servidor fala esse id."""
    cliente, cab, ap = pareado

    r = cliente.post("/v1/push", headers={**cab, "operacao": "g-1"},
                     json={"v": "criou", "id": "n:1", "tp": 4,
                           "t": "Dentista", "d": "2026-08-29",
                           "h": "15:00"}).json()

    assert r["ok"]
    assert r["id"].startswith("g:"), r["id"]

    # E é exatamente o id que o pull usaria para a mesma coisa.
    criado = list(fora.eventos)[-1]
    assert r["id"] == f"g:{criado}"[:39]


# ── o Google avisando, em vez de nós perguntando ─────────────────────
# Enquanto o `pull` fica pendurado, o servidor tinha uma saída só:
# PERGUNTAR ao Google de cinco em cinco segundos. Com o canal de aviso, o
# Google bate na nossa porta e o pull acorda na hora.
def test_o_aviso_do_google_acorda_quem_espera(cliente, monkeypatch):
    from tinto import avisos, config as cfg

    monkeypatch.setattr(cfg, "TINTO_WEBHOOK_SEGREDO", "segredo-1")
    avisos.registra_canal("canal-1", "eu@x.com")
    espera = avisos.espera_de("eu@x.com")
    espera.clear()

    r = cliente.post("/v1/google/aviso", headers={
        "X-Goog-Channel-ID": "canal-1",
        "X-Goog-Channel-Token": "segredo-1",
        "X-Goog-Resource-State": "exists",
    })

    assert r.status_code == 200
    assert espera.is_set()


def test_o_aviso_sem_o_segredo_nao_acorda_ninguem(cliente, monkeypatch):
    """A rota é PÚBLICA por obrigação — quem bate é o Google, sem
    credencial nossa. O que a protege é o token que nós mesmos pusemos no
    canal; sem ele, qualquer um acordaria os pulls de qualquer aparelho.
    """
    from tinto import avisos, config as cfg

    monkeypatch.setattr(cfg, "TINTO_WEBHOOK_SEGREDO", "segredo-1")
    avisos.registra_canal("canal-2", "outro@x.com")
    espera = avisos.espera_de("outro@x.com")
    espera.clear()

    r = cliente.post("/v1/google/aviso", headers={
        "X-Goog-Channel-ID": "canal-2",
        "X-Goog-Channel-Token": "chute",
        "X-Goog-Resource-State": "exists",
    })

    assert r.status_code == 200
    assert not espera.is_set()


def test_o_sync_de_boas_vindas_nao_conta_como_mudanca(cliente, monkeypatch):
    """`sync` é o aviso que o Google manda ao CRIAR o canal: ele diz
    "estou de pé", não "algo mudou". Acordar por ele faria todo canal novo
    custar uma colheita.
    """
    from tinto import avisos, config as cfg

    monkeypatch.setattr(cfg, "TINTO_WEBHOOK_SEGREDO", "segredo-1")
    avisos.registra_canal("canal-3", "terceiro@x.com")
    espera = avisos.espera_de("terceiro@x.com")
    espera.clear()

    r = cliente.post("/v1/google/aviso", headers={
        "X-Goog-Channel-ID": "canal-3",
        "X-Goog-Channel-Token": "segredo-1",
        "X-Goog-Resource-State": "sync",
    })

    assert r.status_code == 200
    assert not espera.is_set()


def test_sem_webhook_configurado_o_pull_continua_igual(pareado, fora):
    """A prova de que o recurso é ACELERADOR, e não dependência."""
    cliente, cab, ap = pareado

    r = cliente.get("/v1/pull", headers=cab)

    assert r.status_code == 200
    assert ap.sinc.canais == {}


def test_a_prova_do_dominio_so_responde_ao_arquivo_certo(cliente, monkeypatch):
    """O Search Console pede um arquivo com nome sorteado. Ele vem de
    variável de ambiente e não de commit: a verificação é passo manual do
    dono, e prendê-la a um deploy seria travar o console numa fila de CI.
    """
    from tinto import config as cfg

    monkeypatch.setattr(cfg, "GOOGLE_SITE_VERIFICATION", "1a2b3c4d")

    r = cliente.get("/google1a2b3c4d.html")
    assert r.status_code == 200
    assert "google-site-verification: google1a2b3c4d.html" in r.text

    assert cliente.get("/google9z9z9z9z.html").status_code == 404


def test_o_pull_abre_canal_de_aviso_e_o_reaproveita(pareado, fora, monkeypatch):
    """Com o webhook configurado, o pull registra o canal — uma vez."""
    from tinto import config as cfg

    cliente, cab, ap = pareado
    monkeypatch.setattr(cfg, "TINTO_WEBHOOK_URL",
                        "https://exemplo.test/v1/google/aviso")
    monkeypatch.setattr(cfg, "TINTO_WEBHOOK_SEGREDO", "segredo-1")

    cliente.get("/v1/pull", headers=cab)

    # Um canal por agenda SINCRONIZADA — a conta de teste tem duas.
    abertos = len(fora.canais)
    assert abertos > 0
    assert abertos == len(ap.sinc.tokens)
    assert fora.canais[0]["address"] == "https://exemplo.test/v1/google/aviso"
    assert fora.canais[0]["token"] == "segredo-1"

    # O segundo pull encontra os canais vivos e não abre outros.
    cliente.get("/v1/pull", headers=cab)
    assert len(fora.canais) == abertos


def test_canal_recusado_nao_derruba_a_sincronizacao(pareado, fora, monkeypatch):
    """O caso REAL enquanto o domínio não está verificado no console: o
    Google responde 401 ao `watch`. A agenda tem de continuar chegando.
    """
    from tinto import agenda as ag, config as cfg

    cliente, cab, ap = pareado
    monkeypatch.setattr(cfg, "TINTO_WEBHOOK_URL",
                        "https://exemplo.test/v1/google/aviso")

    async def recusa(*a, **k):
        return None
    monkeypatch.setattr(ag, "vigia", recusa)

    r = cliente.get("/v1/pull", headers=cab)

    assert r.status_code == 200
    assert ap.sinc.canais == {}     # nenhum canal, e nenhum estrago


def test_o_id_do_canal_cabe_no_alfabeto_do_google(pareado, fora, monkeypatch):
    """`[A-Za-z0-9\\-_+/=]+`, e o `device_id` é um MAC."""
    import re

    from tinto import config as cfg

    cliente, cab, ap = pareado
    monkeypatch.setattr(cfg, "TINTO_WEBHOOK_URL",
                        "https://exemplo.test/v1/google/aviso")

    cliente.get("/v1/pull", headers=cab)

    assert fora.canais, "nenhum canal foi aberto"
    for canal in fora.canais:
        assert re.fullmatch(r"[A-Za-z0-9\-_+/=]+", canal["id"]), canal["id"]

    # E o MAC continua legível dentro dele: é o que liga um canal a um
    # aparelho quando se lê o log meses depois.
    assert "AA-BB-CC" in fora.canais[0]["id"]


def test_com_canal_o_servidor_para_de_perguntar_ao_google(pareado, fora,
                                                          monkeypatch):
    """O canal só paga se o polling encolher — senão é complexidade de
    graça.
    """
    from tinto import app as rotas

    cliente, cab, ap = pareado

    # Sem canal, o passo é o curto: é ele que sustenta a latência quando
    # não há quem avise.
    assert rotas.ESPERA_PASSO_S < rotas.ESPERA_PASSO_COM_CANAL_S

    # E o longo cabe DENTRO da janela: um passo maior que ela faria a
    # conferida de segurança nunca acontecer.
    assert rotas.ESPERA_PASSO_COM_CANAL_S < rotas.ESPERA_MAX_S


def test_calendario_sem_push_nao_e_tentado_de_novo(pareado, fora, monkeypatch):
    """Feriados é público e read-only: o Google recusa o `watch` com
    `pushNotSupportedForRequestedResource`, e essa resposta não muda.
    """
    from tinto import config as cfg

    cliente, cab, ap = pareado
    monkeypatch.setattr(cfg, "TINTO_WEBHOOK_URL",
                        "https://exemplo.test/v1/google/aviso")

    # Feriados vem desligada por palpite; aqui a pessoa a ligou — que é o
    # caso real que descobriu este defeito.
    ap.sinc.escolhas["pt.brazilian#holiday@group.v.calendar.google.com"] = True

    cliente.get("/v1/pull", headers=cab)
    tentativas = sum(1 for _, c in fora.chamadas if c.endswith("/events/watch"))

    # O segundo pull não tenta de novo NADA: os que abriram estão vivos, e
    # o que recusou está marcado.
    cliente.get("/v1/pull", headers=cab)
    assert sum(1 for _, c in fora.chamadas
               if c.endswith("/events/watch")) == tentativas

    # E a marca não conta como canal: ela é a memória de que não pode
    # haver um.
    sem_push = [c for c in ap.sinc.canais.values() if c.get("sem_push")]
    assert sem_push
    assert not any(c.get("id") for c in sem_push)


def test_a_lista_nasce_com_o_que_foi_falado_dentro(pareado, fora):
    """"Lista de compras: arroz, feijão e macarrão" — as três dentro.
    """
    cliente, cab, ap = pareado
    fora.acoes = [{"v": "criou", "t": "Compras", "tp": 3,
                   "itens": ["arroz", "feijão", "macarrão"]}]

    proposta = cliente.post("/v1/captura", headers={**cab, "operacao": "c-1"},
                            files={"audio": ("f.wav", WAV, "audio/wav")}).json()

    # Os itens NÃO viajam para o device: a resposta da captura é lida para
    # um `char json[1024]`, e a lista de compras estoura o buffer.
    assert "itens" not in proposta["acoes"][0]

    r = cliente.post("/v1/push", headers={**cab, "operacao": "p-1"},
                     json={**proposta["acoes"][0], "nota": proposta["nota"]})
    assert r.json()["ok"], r.json()

    nova = [l for l in fora.listas.values() if l["title"] == "Compras"]
    assert nova, fora.listas
    dentro = [t["title"] for t in fora.tarefas.values()
              if t["_lista"] == nova[0]["id"]]
    assert dentro == ["arroz", "feijão", "macarrão"], dentro


def test_o_fim_falado_chega_ao_google(pareado, fora):
    """"Reunião das 14 às 16": o fim fica na nota e entra no OK."""
    cliente, cab, ap = pareado
    fora.acoes = [{"v": "criou", "t": "Reunião", "h": "14:00", "f": "16:00",
                   "d": "2026-09-11", "tp": 4}]
    proposta = cliente.post("/v1/captura", headers={**cab, "operacao": "c-f"},
                            files={"audio": ("f.wav", WAV, "audio/wav")}).json()
    assert "f" not in proposta["acoes"][0]       # não viaja ao device

    r = cliente.post("/v1/push", headers={**cab, "operacao": "p-f"},
                     json={**proposta["acoes"][0], "nota": proposta["nota"]})
    assert r.json()["ok"], r.json()
    ev = [e for e in fora.eventos.values() if e.get("summary") == "Reunião"]
    assert ev[0]["end"]["dateTime"] == "2026-09-11T16:00:00", ev


def test_editar_a_hora_no_aparelho_mantem_a_duracao(pareado, fora):
    """Mudar a hora no Tinto não encolhe a reunião de 2 h para 1 h."""
    cliente, cab, ap = pareado
    r = cliente.post("/v1/push", headers={**cab, "operacao": "e-1"},
                     json={"v": "criou", "id": "n:1", "t": "Aula",
                           "h": "14:00", "d": "2026-09-11", "tp": 4})
    gid = r.json()["id"]
    ev = next(e for e in fora.eventos.values() if e.get("summary") == "Aula")
    ev["end"] = {"dateTime": "2026-09-11T16:00:00",
                 "timeZone": ev["end"].get("timeZone", "")}

    r = cliente.post("/v1/push", headers={**cab, "operacao": "e-2"},
                     json={"v": "editou", "id": gid, "t": "Aula",
                           "h": "15:00", "d": "2026-09-11", "tp": 4})
    assert r.json()["ok"], r.json()
    assert ev["end"]["dateTime"].startswith("2026-09-11T17:00"), ev["end"]


def _mesma(a: str, b: str) -> bool:
    return a.strip().casefold() == b.strip().casefold()


def test_por_na_lista_que_ja_existe_nao_cria_uma_segunda(pareado, fora):
    """"Põe leite e pão na lista de compras" — na lista que já está lá.
    """
    cliente, cab, ap = pareado
    velha = fora.listas["@default"] = {"id": "@default", "title": "Compras"}
    fora.acoes = [{"v": "adicionou", "t": "compras", "tp": 3,
                   "itens": ["leite", "pão"]}]

    proposta = cliente.post("/v1/captura", headers={**cab, "operacao": "c-1"},
                            files={"audio": ("f.wav", WAV, "audio/wav")}).json()

    # "adicionou" não existe no vocabulário do device: vira "criou", e a
    # ação chega inteira em vez de se perder calada na validação.
    assert proposta["acoes"][0]["v"] == "criou"

    r = cliente.post("/v1/push", headers={**cab, "operacao": "p-1"},
                     json={**proposta["acoes"][0], "nota": proposta["nota"]})
    assert r.json()["ok"], r.json()

    compras = [l for l in fora.listas.values() if _mesma("Compras", l["title"])]
    assert len(compras) == 1, fora.listas
    dentro = [t["title"] for t in fora.tarefas.values()
              if t["_lista"] == velha["id"]]
    assert dentro == ["leite", "pão"], dentro


def test_a_memoria_do_reenvio_sobrevive_a_um_deploy(tmp_path, pareado, fora):
    """O reenvio depois de um restart não pode criar o evento duas vezes."""
    cliente, cab, ap = pareado
    memoria.arquivo = tmp_path / "estado.json"

    acao = {"v": "criou", "id": "n:1", "t": "Dentista", "h": "15:00",
            "d": "2026-09-11", "tp": 4}
    r = cliente.post("/v1/push", headers={**cab, "operacao": "op-x"},
                     json=acao)
    assert r.json()["ok"], r.json()
    assert len(fora.eventos) == 1

    # O deploy: a RAM some, o volume fica.
    memoria.operacoes.clear()
    memoria._carrega()

    # E o aparelho reenvia o mesmo gesto, com a mesma operação.
    de_novo = cliente.post("/v1/push", headers={**cab, "operacao": "op-x"},
                           json=acao)

    assert de_novo.json() == r.json()
    assert len(fora.eventos) == 1, fora.eventos


def test_hora_sem_dia_cai_em_hoje_ou_amanha(pareado, fora):
    """A hora põe a coisa na régua, e régua sem dia não existe."""
    from datetime import datetime, timedelta, timezone

    # Um instante FIXO: o teste não pode depender da hora em que roda.
    agora = datetime(2026, 9, 4, 12, 0, tzinfo=timezone(timedelta(hours=-3)))

    # Ainda vai acontecer hoje.
    acoes = rotas.ia._acoes_de(
        '{"acoes":[{"v":"criou","t":"Remédio","tp":2,"h":"22:00"}]}',
        None, agora)
    assert acoes and acoes[0].d == "2026-09-04", acoes

    # Já passou: é de amanhã. Marcar para uma hora que ficou para trás
    # poria a coisa num dia que já acabou.
    acoes = rotas.ia._acoes_de(
        '{"acoes":[{"v":"criou","t":"Remédio","tp":2,"h":"07:00"}]}',
        None, agora)
    assert acoes and acoes[0].d == "2026-09-05", acoes


def test_dois_workers_sao_recusados_na_partida(monkeypatch):
    """Um processo só, e ele grita se mandarem dois."""
    from tinto import config

    monkeypatch.setenv("WEB_CONCURRENCY", "2")
    with pytest.raises(RuntimeError, match="um processo"):
        config.confere_um_processo()

    for valor in ("1", ""):
        monkeypatch.setenv("WEB_CONCURRENCY", valor)
        config.confere_um_processo()          # não levanta

    monkeypatch.delenv("WEB_CONCURRENCY")
    config.confere_um_processo()


def test_consulta_do_calendario_nao_fica_pendurada(pareado, fora, monkeypatch):
    """Abrir um dia não pode esperar o long polling."""
    cliente, cab, ap = pareado
    monkeypatch.setattr(rotas, "ESPERA_MAX_S", 5.0)

    # Sem novidade nenhuma: um pull normal com `esperar` fica pendurado.
    cliente.get("/v1/pull", headers=cab)

    inicio = time.monotonic()
    r = cliente.get("/v1/olhar?dia=2026-09-27", headers=cab)
    gasto = time.monotonic() - inicio

    assert r.status_code == 200
    assert gasto < 2.0, f"a consulta esperou {gasto:.1f}s"

    # E o pull não aceita mais a carona: o `dia` sobrando na query é
    # ignorado, e a resposta dele não fala de dia nenhum.
    assert "dia" not in cliente.get("/v1/pull", headers=cab).json()


# ── o nome do aparelho é um só, nos dois lados ───────────────────────
def test_o_nome_trocado_no_aparelho_sobe_no_pull(pareado, fora):
    cliente, cab, ap = pareado

    r = cliente.get("/v1/pull", params={"nome": 'Tinto "da" família'},
                    headers=cab)
    assert r.json()["aparelho_nome"] == "Tinto da família"
    assert ap.nome == "Tinto da família"

    # Sem nome no pedido (nada trocado no aparelho), o servidor não mexe.
    r = cliente.get("/v1/pull", headers=cab)
    assert r.json()["aparelho_nome"] == "Tinto da família"


def test_renomear_no_aplicativo_chega_ao_aparelho(pareado, fora):
    from tinto import pwa as app_pessoa

    cliente, cab, ap = pareado
    cliente.get("/v1/pull", params={"nome": "Mesa"}, headers=cab)

    app_pessoa._pessoas["s-nome"] = ap.pessoa
    cliente.cookies.set("tinto_pessoa", "s-nome")
    assert cliente.patch(f"/e/api/aparelho/{ap.device_id}",
                         json={"nome": "Cozinha"}).status_code == 200

    # O aparelho não trocou nada desde então: pede sem nome, e recebe.
    r = cliente.get("/v1/pull", headers=cab)
    assert r.json()["aparelho_nome"] == "Cozinha"


# ── tirar um e-mail de TINTO_CONTAS corta o acesso na hora ───────────
def test_conta_removida_da_lista_perde_o_acesso_na_proxima_chamada(
        pareado, fora, monkeypatch):
    from tinto import config, pwa as app_pessoa

    cliente, cab, ap = pareado
    assert cliente.get("/v1/pull", headers=cab).status_code == 200

    app_pessoa._pessoas["s-fora"] = ap.pessoa
    cliente.cookies.set("tinto_pessoa", "s-fora")
    assert cliente.get("/e/api/conta").status_code == 200

    # Quem hospeda tirou o e-mail da lista.
    monkeypatch.setattr(config, "CONTAS", {"outra@x.com": None})

    assert cliente.get("/v1/pull", headers=cab).status_code == 409
    estado = cliente.get("/v1/parear/estado", headers=cab).json()
    assert estado["pareado"] is False and estado["conta"] == ""
    assert cliente.get("/e/api/conta").status_code == 401

    # O vínculo continua guardado: voltar à lista o devolve.
    monkeypatch.setattr(config, "CONTAS", {ap.pessoa: None})
    assert cliente.get("/v1/pull", headers=cab).status_code == 200


def test_limite_mal_escrito_avisa_no_log(monkeypatch, caplog):
    from tinto import config

    monkeypatch.setenv("TINTO_CONTAS", "voce@gmail.com:1h")
    with caplog.at_level("WARNING", logger="tinto"):
        contas = config._contas("TINTO_CONTAS")
    assert contas == {"voce@gmail.com": None}
    assert "limite inválido" in caplog.text


def test_registro_aberto_tem_teto_de_aparelhos_sem_conta(cliente, monkeypatch):
    """Sem teto, quem achasse o endereço encheria o estado de registros."""
    from tinto import portaria
    from tinto.memoria import memoria

    monkeypatch.setattr(memoria, "SEM_CONTA_MAX", 3)
    for i in range(3):
        assert cliente.post("/v1/registrar", json={
            "device_id": f"AA:00:{i:02d}", "prova": "p"}).status_code == 200
    portaria.esquece_tudo()

    r = cliente.post("/v1/registrar", json={"device_id": "AA:00:99",
                                            "prova": "p"})
    assert r.status_code == 429

    # Quem já está registrado continua se reapresentando.
    assert cliente.post("/v1/registrar", json={
        "device_id": "AA:00:01", "prova": "p"}).status_code == 200

    # E um aparelho que ganha conta libera a vaga.
    memoria.por_device["AA:00:00"].pessoa = "eu@x.com"
    assert cliente.post("/v1/registrar", json={
        "device_id": "AA:00:99", "prova": "p"}).status_code == 200


def test_o_que_o_aparelho_criou_volta_no_resync(pareado, fora):
    """No vidro: shopping criado por voz sumiu do Tinto e ficou no Google."""
    cliente, cab, ap = pareado
    cliente.get("/v1/pull", headers=cab)
    r = cliente.post("/v1/push", headers={**cab, "operacao": "shop-1"},
                     json={"v": "criou", "id": "n:1", "t": "Shopping",
                           "h": "20:00", "d": HOJE, "tp": 4})
    assert r.json()["ok"]
    cliente.get("/v1/pull", headers=cab)

    ap.sinc.cheio_em = 0.0                  # o resync diário
    lotes, zerou = [], False
    for _ in range(5):
        p = cliente.get("/v1/pull", headers=cab).json()
        zerou = zerou or p["zerar"]
        lotes += [i["t"] for i in p["itens"]]
        if not p["mais"]:
            break
    assert zerou
    assert "Shopping" in lotes


def test_o_app_mostra_o_que_a_fala_virou_so_depois_de_confirmada(pareado, fora):
    """Cada envio de voz abre no app com o que foi dito e o que virou."""
    from tinto import pwa as app_pessoa

    cliente, cab, ap = pareado
    app_pessoa._pessoas["s-fala"] = "eu@x.com"
    cliente.cookies.set("tinto_pessoa", "s-fala")
    fora.fala = "lista de compras arroz e feijão"
    fora.acoes = [{"v": "criou", "t": "Compras", "tp": 3,
                   "itens": ["arroz", "feijão"]}]

    proposta = cliente.post("/v1/captura", headers={**cab, "operacao": "c-v"},
                            files={"audio": ("f.wav", WAV, "audio/wav")}).json()

    envio = cliente.get("/e/api/conta").json()["voz"]["recentes"][0]
    assert envio["confirmada"] is False
    assert envio["texto"] == "" and envio["acoes"] == []

    r = cliente.post("/v1/push", headers={**cab, "operacao": "p-v"},
                     json={**proposta["acoes"][0], "nota": proposta["nota"]})
    assert r.json()["ok"], r.json()

    envio = cliente.get("/e/api/conta").json()["voz"]["recentes"][0]
    assert envio["confirmada"] is True
    assert envio["texto"] == "lista de compras arroz e feijão"
    assert envio["acoes"] == [{"tp": 3, "t": "Compras", "d": "", "h": "",
                               "f": "", "itens": 2}]
    assert "nota" not in envio and "tokens" not in envio
