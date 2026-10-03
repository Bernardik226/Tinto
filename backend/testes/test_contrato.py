"""O contrato, provado sem Google e sem IA de pé."""

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tinto.contrato import Acao, Item, RespostaCaptura, RespostaPull, Tipo
from tinto.google import evento_para_item, tarefa_para_item, vem_desligada
from tinto.ia import _acoes_de, _lista_do_que_existe
from tinto.memoria import Memoria


# ── o formato que o firmware lê ──────────────────────────────────────
def test_nenhum_campo_vira_null():
    """`null` onde o parser espera string é a falha silenciosa."""
    bruto = json.loads(RespostaPull(itens=[Item(id="g:1")]).model_dump_json())
    item = bruto["itens"][0]

    for campo in ("t", "h", "f", "l", "d", "o"):
        assert item[campo] is not None
        assert isinstance(item[campo], str)


def test_o_tipo_atravessa_como_inteiro():
    """`tp` é lido com `json_int` do outro lado."""
    bruto = json.loads(Item(id="g:1", tp=Tipo.EVENTO).model_dump_json())
    assert bruto["tp"] == 4
    assert isinstance(bruto["tp"], int)


def test_o_limite_de_acoes_e_tres():
    """Três é o teto do device (`RESULTADOS_MAX`)."""
    muitas = [Acao(id=f"n:{i}") for i in range(5)]
    try:
        RespostaCaptura(falou="x", acoes=muitas)
        assert False, "aceitou mais de três ações"
    except Exception:
        pass


# ── a tradução do Google ─────────────────────────────────────────────
def test_evento_recusado_nao_chega_ao_aparelho():
    """Quem recusou não vai."""
    ev = {
        "id": "a1", "summary": "Reunião que eu recusei",
        "start": {"dateTime": "2026-08-27T15:00:00-03:00"},
        "end": {"dateTime": "2026-08-27T16:00:00-03:00"},
        "attendees": [{"self": True, "responseStatus": "declined"}],
    }
    assert evento_para_item(ev) is None


def test_evento_de_dia_inteiro_nao_tem_hora():
    """`di` separa "o dia todo" de "não sei a hora"."""
    ev = {"id": "a2", "summary": "Entrega do documento",
          "start": {"date": "2026-08-27"}, "end": {"date": "2026-08-28"}}

    it = evento_para_item(ev)
    assert it.di is True
    assert it.h == ""          # e não "00:00", que seria uma hora falsa
    assert it.d == "2026-08-27"


def test_endereco_comprido_e_truncado_e_nao_recusado():
    """RN-B7: truncar, não rejeitar."""
    ev = {"id": "a3", "summary": "Dentista",
          "location": "R. " + "x" * 200,
          "start": {"dateTime": "2026-08-27T15:00:00-03:00"},
          "end": {"dateTime": "2026-08-27T16:00:00-03:00"}}

    it = evento_para_item(ev)
    assert it is not None
    assert len(it.l) <= 95


def test_tarefa_completa_vem_marcada():
    tk = {"id": "t9", "title": "Pagar IPVA", "status": "completed",
          "due": "2026-08-20T00:00:00.000Z"}
    it = tarefa_para_item(tk)
    assert it.ok is True
    assert it.d == "2026-08-20"
    assert it.tp == Tipo.TAREFA


def test_agenda_de_feriado_vem_desligada():
    """Ela tem ~15 eventos por ano e nenhum é compromisso."""
    assert vem_desligada("pt.brazilian#holiday@group.v.calendar.google.com")
    assert not vem_desligada("alguem@exemplo.com")


# ── a LLM ────────────────────────────────────────────────────────────
def test_json_dentro_de_markdown_ainda_vale():
    """Um modelo que embrulha o JSON não pode derrubar a captura."""
    resposta = ('Claro! Aqui está:\n```json\n'
                '{"acoes":[{"v":"criou","t":"Dentista","h":"15:00",'
                '"d":"2026-08-27","tp":4}]}\n```')
    acoes = _acoes_de(resposta)
    assert len(acoes) == 1
    assert acoes[0].t == "Dentista"
    assert acoes[0].tp == Tipo.EVENTO


def test_lixo_da_llm_nao_derruba_nada():
    assert _acoes_de("desculpe, não entendi") == []
    assert _acoes_de("") == []
    assert _acoes_de("{quebrado") == []


def test_acao_invalida_e_descartada_e_nao_corrigida():
    """Inventar o que faltou é o que o prompt proíbe a LLM de fazer."""
    resposta = ('{"acoes":[{"t":"' + "x" * 500 + '","tp":4},'
                '{"t":"Boa","tp":2}]}')
    acoes = _acoes_de(resposta)
    assert len(acoes) == 1
    assert acoes[0].t == "Boa"


def test_a_llm_distingue_itens_iguais_pelo_tipo_e_pela_casa():
    """Título e data não bastam para escolher o alvo de uma fala."""
    existentes = [
        Item(id="t:1", t="Academia", d="2026-09-05", h="07:00",
             tp=Tipo.TAREFA, l="Rotina"),
        Item(id="g:1", t="Academia", d="2026-09-05", h="18:00",
             tp=Tipo.EVENTO, a="Pessoal"),
    ]

    lista = _lista_do_que_existe(existentes)

    assert "tarefa" in lista and "lista Rotina" in lista
    assert "evento" in lista and "agenda Pessoal" in lista


# ── o isolamento ─────────────────────────────────────────────────────
def test_registrar_duas_vezes_nao_cria_dois_aparelhos():
    """Um boot repetido não pode deixar órfão o pareamento que existe."""
    m = Memoria()
    m.provisiona("AA:BB:CC")
    a = m.registra("AA:BB:CC")
    b = m.registra("AA:BB:CC")
    assert a.token == b.token
    assert len(m.aparelhos) == 1


def test_codigo_de_pareamento_e_de_uso_unico():
    """Um código que serve duas vezes é um código que, vazado, pareia o
    aparelho de outra pessoa à sua conta."""
    m = Memoria()
    m.provisiona("AA:BB:CC")
    p = m.novo_codigo("AA:BB:CC")

    assert m.confirma_pareamento(p.codigo, "eu@x.com", "ref") is not None
    assert m.confirma_pareamento(p.codigo, "outro@x.com", "ref") is None


def test_o_codigo_nao_tem_letra_ambigua():
    """`0/O` e `1/I` são o par que mais erra na travessia da tela de
    e-ink para o teclado do celular."""
    m = Memoria()
    m.provisiona("AA:BB:CC")
    for _ in range(50):
        codigo = m.novo_codigo("AA:BB:CC").codigo
        assert "I" not in codigo and "O" not in codigo
        assert codigo.isalpha() and len(codigo) == 6


def test_a_mesma_operacao_devolve_a_mesma_resposta():
    """O pior bug possível deste sistema: o device manda a captura, a
    resposta se perde, ele tenta de novo, e nascem DOIS eventos na agenda
    de verdade da pessoa."""
    m = Memoria()
    assert m.ja_respondeu("eu@x.com", "op-1") is None

    m.guarda_resposta("eu@x.com", "op-1", {"ok": True, "id": "uma-vez"})
    assert m.ja_respondeu("eu@x.com", "op-1")["id"] == "uma-vez"


def test_a_operacao_de_uma_conta_nao_responde_a_outra():
    """RN-91 num lugar que não parece consulta, e é."""
    m = Memoria()
    m.guarda_resposta("eu@x.com", "op-1", {"falou": "minha vida"})

    assert m.ja_respondeu("outro@x.com", "op-1") is None
    assert m.ja_respondeu("eu@x.com", "op-1")["falou"] == "minha vida"


def test_saber_o_device_id_nao_da_o_token_de_ninguem():
    """`ENGENHARIA.md` §7, fronteira 4: id forjado."""
    m = Memoria()
    m.provisiona("AA:BB:CC")
    meu = m.registra("AA:BB:CC", "segredo-do-aparelho")

    assert m.registra("AA:BB:CC", "chute") is None
    assert m.registra("AA:BB:CC", "") is None
    assert m.registra("AA:BB:CC", "segredo-do-aparelho").token == meu.token


def test_a_prova_nao_fica_guardada_em_claro():
    """Um dump do banco não deve entregar a credencial de ninguém — nem a
    de um aparelho."""
    m = Memoria()
    m.provisiona("AA:BB:CC")
    m.registra("AA:BB:CC", "segredo-do-aparelho")

    guardado = m.por_device["AA:BB:CC"].segredo
    assert guardado and "segredo-do-aparelho" not in guardado


# ── o modelo: a NOTA é a unidade, não o item ─────────────────────────
def test_a_nota_guarda_a_transcricao_e_as_acoes():
    """Uma sessão de fala: uma transcrição, de uma a três ações."""
    from tinto.modelo import Notas

    n = Notas()
    nota = n.abre("eu@x.com", "marca dentista quinta e compra pasta", [
        Acao(id="n:1", t="Dentista", h="15:00", tp=Tipo.EVENTO),
        Acao(id="n:2", t="Comprar pasta", tp=Tipo.TAREFA),
    ])

    assert nota.id.startswith("nt:")
    assert len(nota.acoes) == 2
    assert "dentista" in nota.transcricao
    assert nota.estado == "aberta"          # ninguém confirmou ainda


def test_descartar_nao_deixa_transcricao_orfa():
    """Descartar é a linha que promete que nada daquilo ficou."""
    from tinto.modelo import Notas

    n = Notas()
    nota = n.abre("eu@x.com", "deixa pra lá", [Acao(id="n:1", t="X")])
    nota.descartar()

    assert nota.estado == "descartada"
    assert nota.transcricao == ""
    assert nota.acoes == []


def test_a_nota_de_outra_pessoa_nao_existe():
    """RN-92: 404 e não 403."""
    from tinto.modelo import Notas

    n = Notas()
    minha = n.abre("eu@x.com", "x", [])

    assert n.de("eu@x.com", minha.id) is not None
    assert n.de("outro@x.com", minha.id) is None


def test_so_evento_tarefa_e_lista_vao_pro_google():
    """A anotação é a única coisa nossa, e é de propósito."""
    from tinto.modelo import e_do_google

    assert e_do_google(Tipo.EVENTO)
    assert e_do_google(Tipo.TAREFA)
    assert e_do_google(Tipo.LISTA)
    assert not e_do_google(Tipo.ANOTACAO)


def test_anotacoes_saem_por_recencia():
    """A pessoa não lembra em que terça falou aquilo, lembra que falou."""
    from tinto.modelo import Notas

    n = Notas()
    for i in range(3):
        n.anota("eu@x.com", Acao(id=f"n:{i}", t=f"nota {i}", tp=Tipo.ANOTACAO),
                "nt:1", "2026-08-26")

    lista = n.anotacoes_de("eu@x.com")
    assert len(lista) == 3
    assert lista[0].titulo == "nota 2"       # a mais nova primeiro
    assert all(a.nota_id == "nt:1" for a in lista)


def test_a_resposta_da_captura_devolve_o_id_da_nota():
    """É ele que fecha o vínculo do lado do device."""
    r = RespostaCaptura(falou="x", acoes=[Acao(id="n:1")], nota="nt:abc")
    bruto = json.loads(r.model_dump_json())
    assert bruto["nota"] == "nt:abc"


def test_tarefa_concluida_no_google_desce_como_concluida():
    """Concluir no celular tem de chegar ao Tinto."""
    tk = {"id": "abc", "title": "Levar os exames",
          "status": "completed", "hidden": True,
          "completed": "2026-09-02T10:00:00.000Z"}

    it = tarefa_para_item(tk, "Minhas tarefas")
    assert it is not None
    assert it.ok is True
    assert it.t == "Levar os exames"


def test_tarefa_apagada_no_google_nao_vira_item():
    """Apagada é outra coisa: ela não existe mais, e não desce como item."""
    assert tarefa_para_item({"id": "x", "title": "t", "deleted": True}) is None


def test_tarefa_traz_a_data_em_que_foi_concluida():
    """O Google sabe QUANDO ela foi concluída, e o Tinto mostra por essa data."""
    tk = {"id": "abc", "title": "Comprar componentes",
          "status": "completed", "hidden": True,
          "completed": "2026-09-02T13:45:00.000Z"}

    it = tarefa_para_item(tk, "My Tasks")
    assert it is not None
    assert it.ok is True
    assert it.c == "2026-09-02"


def test_tarefa_aberta_nao_tem_data_de_conclusao():
    """Aberta não tem `completed`, e o campo desce vazio."""
    it = tarefa_para_item({"id": "x", "title": "t", "status": "needsAction"})
    assert it is not None
    assert it.c == ""


def test_evento_traz_o_nome_da_agenda_em_que_ele_mora():
    """O detalhe do item mostra "AGENDA · Pessoal", e o nome vem daqui."""
    ev = {"id": "a1", "summary": "Reunião de produto",
          "start": {"dateTime": "2026-09-04T09:30:00-03:00"},
          "end": {"dateTime": "2026-09-04T10:15:00-03:00"}}

    it = evento_para_item(ev, agenda="Trabalho")
    assert it is not None
    assert it.a == "Trabalho"


def test_evento_sem_nome_de_agenda_desce_vazio():
    """Vazio é "não sei", e a tela some com a linha."""
    ev = {"id": "a2", "summary": "Dentista",
          "start": {"dateTime": "2026-09-04T09:30:00-03:00"},
          "end": {"dateTime": "2026-09-04T10:15:00-03:00"}}
    assert evento_para_item(ev).a == ""
