"""A sessão sobrevive ao deploy."""

from tinto import pwa as app_pwa
from tinto.memoria import Memoria, memoria


def test_a_sessao_do_app_sobrevive_a_um_reinicio(tmp_path):
    arquivo = tmp_path / "estado.json"

    m = Memoria(str(arquivo))
    fichinha = m.abre_sessao("usuario@exemplo.com", "app")

    outra = Memoria(str(arquivo))
    assert outra.dono_da_sessao(fichinha, "app") == "usuario@exemplo.com"


def test_sair_encerra_a_sessao_de_verdade(tmp_path):
    arquivo = tmp_path / "estado.json"

    m = Memoria(str(arquivo))
    fichinha = m.abre_sessao("usuario@exemplo.com", "app")
    m.fecha_sessao(fichinha)

    assert m.dono_da_sessao(fichinha, "app") == ""
    assert Memoria(str(arquivo)).dono_da_sessao(fichinha, "app") == ""


def test_a_ficha_nao_e_guardada_em_texto_claro(tmp_path):
    """O arquivo do volume não pode conter a ficha que abre a porta."""
    arquivo = tmp_path / "estado.json"
    m = Memoria(str(arquivo))
    fichinha = m.abre_sessao("usuario@exemplo.com", "app")

    assert fichinha not in arquivo.read_text()


def test_uma_ficha_inventada_nao_entra():
    m = Memoria()
    assert m.dono_da_sessao("nao-sou-ficha-de-ninguem", "app") == ""
    assert m.dono_da_sessao("", "app") == ""


def test_o_aplicativo_continua_logado_depois_do_deploy(cliente):
    """O caminho inteiro: entra, o processo reinicia, e ele continua lá."""
    fichinha = memoria.abre_sessao("usuario@exemplo.com", "app")
    cliente.cookies.set("tinto_pessoa", fichinha)

    # O reinício: as tabelas em memória dos módulos somem.
    app_pwa._pessoas.clear()

    assert cliente.get("/e/api/conta").status_code == 200


def test_o_botao_de_sair_do_app_derruba_a_sessao(cliente):
    fichinha = memoria.abre_sessao("usuario@exemplo.com", "app")
    cliente.cookies.set("tinto_pessoa", fichinha)

    assert cliente.post("/e/api/sair").status_code == 200
    assert memoria.dono_da_sessao(fichinha, "app") == ""

    cliente.cookies.set("tinto_pessoa", fichinha)
    assert cliente.get("/e/api/conta").status_code == 401


def test_sair_do_app_fecha_as_outras_sessoes_da_mesma_conta(cliente):
    """Sair da conta vale no celular e no computador."""
    celular = memoria.abre_sessao("usuario@exemplo.com", "app")
    computador = memoria.abre_sessao("usuario@exemplo.com", "app")
    cliente.cookies.set("tinto_pessoa", celular)

    assert cliente.post("/e/api/sair").status_code == 200

    assert memoria.dono_da_sessao(celular, "app") == ""
    assert memoria.dono_da_sessao(computador, "app") == ""


# ── e a tela de permissão do Google, que aparecia toda vez ───────────
def test_o_login_normal_nao_pede_permissao_de_novo():
    """"Toda hora eu tenho que permitir de novo."
    """
    from tinto import oauth

    normal = oauth.url_de_consentimento("pwa:n1")
    assert "prompt=consent" not in normal
    assert "access_type=offline" in normal      # o refresh continua sendo pedido

    forcado = oauth.url_de_consentimento("pwa:n1", forcar=True)
    assert "prompt=consent" in forcado


def test_sem_refresh_guardado_a_permissao_e_pedida_uma_vez(
        cliente, fora, monkeypatch):
    """Quem nunca autorizou volta para o Google, com o consentimento."""
    monkeypatch.setattr(app_pwa.config, "GOOGLE_REDIRECT", "https://x/oauth")
    fora.sem_refresh = True

    r = cliente.get("/e/google", follow_redirects=False)
    estado = r.headers["location"].split("state=")[1].split("&")[0]

    r = cliente.get(f"/oauth/retorno?state={estado}&code=c1",
                    follow_redirects=False)
    assert r.status_code in (302, 303, 307)
    assert "prompt=consent" in r.headers["location"]

    # A segunda volta não insiste: ela entra, e quem cuida do que falta é
    # a tela de conta.
    estado2 = r.headers["location"].split("state=")[1].split("&")[0]
    r = cliente.get(f"/oauth/retorno?state={estado2}&code=c2",
                    follow_redirects=False)
    assert "accounts.google" not in r.headers.get("location", "")


def test_quem_ja_autorizou_entra_direto_mesmo_sem_refresh_novo(
        cliente, fora, monkeypatch):
    """O segundo login não traz refresh — e não precisa: ele está guardado."""
    memoria.pessoas["eu@x.com"] = "ref-guardado"
    monkeypatch.setattr(app_pwa.config, "GOOGLE_REDIRECT", "https://x/oauth")
    fora.sem_refresh = True

    r = cliente.get("/e/google", follow_redirects=False)
    estado = r.headers["location"].split("state=")[1].split("&")[0]

    r = cliente.get(f"/oauth/retorno?state={estado}&code=c1",
                    follow_redirects=False)
    assert "prompt=consent" not in r.headers.get("location", "")
    assert memoria.pessoas["eu@x.com"] == "ref-guardado"
