"""A moldura do PWA: estático servido por lista fechada, e a entrada."""

import pytest

from tinto import pwa as app_pessoa


@pytest.fixture
def pessoa(cliente):
    sessao = "s-shell"
    app_pessoa._pessoas[sessao] = "eu@x.com"
    cliente.cookies.set("tinto_pessoa", sessao)
    return cliente


def test_a_moldura_e_estatica_e_nao_tem_html_injetado(cliente):
    """`index.html` sai como está no disco."""
    r = cliente.get("/e")
    assert r.status_code == 200
    assert "<!--CORPO-->" not in r.text
    assert "app.js" in r.text and "app.css" in r.text


def test_os_estaticos_saem_por_lista_fechada(cliente):
    """Nome que veio de fora nunca vira caminho."""
    assert cliente.get("/e/app.css").status_code == 200
    assert cliente.get("/e/app.js").status_code == 200

    assert cliente.get("/e/pwa.py").status_code == 404
    assert cliente.get("/e/qualquer-coisa.txt").status_code == 404


def test_o_codigo_nao_atravessa_o_login(cliente, fora, monkeypatch):
    """§: "Código de vínculo é digitado após login e nunca atravessa ou é
    reservado no OAuth".
    """
    from tinto import config

    monkeypatch.setattr(config, "GOOGLE_REDIRECT", "https://x/e/volta")

    r = cliente.get("/e/google?codigo=KXPT4M", follow_redirects=False)
    assert r.status_code in (302, 303, 307)
    assert "KXPT4M" not in r.headers.get("location", "")


def test_a_conta_responde_em_json_com_identidade_cota_e_aparelhos(pessoa):
    """A tela de Minha Conta lê isto, e nada mais."""
    r = pessoa.get("/e/api/conta")
    assert r.status_code == 200

    conta = r.json()
    assert conta["email"] == "eu@x.com"
    assert "voz" in conta and "usados" in conta["voz"] and "limite" in conta["voz"]
    assert conta["aparelhos"] == []


def test_sem_sessao_a_conta_nao_responde(cliente):
    cliente.cookies.clear()
    assert cliente.get("/e/api/conta").status_code == 401


def test_a_moldura_traz_o_logotipo_e_os_dois_destinos(cliente):
    """O desenho aprovado, no que ele tem de estrutural."""
    pagina = cliente.get("/e").text
    assert 'class="logo' in pagina

    css = cliente.get("/e/app.css").text
    assert "mask:" in css and "logo.png" in css

    # Dois destinos, desenhados duas vezes: barra no celular, lado no
    # desktop. Mesmos rótulos nos dois.
    assert pagina.count('data-rota="conta"') == 2
    assert pagina.count('data-rota="acervo"') == 2
    assert "dispositivos" not in pagina.lower()


def test_o_movimento_respeita_quem_pediu_para_nao_ter(cliente):
    """A animação do papel é enfeite, e enfeite não atrapalha."""
    css = cliente.get("/e/app.css").text
    assert "prefers-reduced-motion" in css
    assert "pointer-events: none" in css


def test_o_hidden_vence_o_display(cliente):
    """As duas telas não podem aparecer juntas."""
    css = cliente.get("/e/app.css").text
    assert "[hidden]" in css and "display: none !important" in css


def test_a_marca_e_o_asset_oficial_e_nao_um_desenho_em_css(cliente):
    """O logo do aplicativo é a marca, e não uma imitação."""
    r = cliente.get("/e/logo.png")
    assert r.status_code == 200
    assert r.headers["content-type"] == "image/png"
    assert r.content[:8] == b"\x89PNG\r\n\x1a\n"

    # A marca PODE mudar — acabou de mudar. Um ano de cache imutável
    # deixaria quem já abriu o aplicativo com a versão velha até 2027.
    assert "immutable" not in r.headers.get("cache-control", "")

    css = cliente.get("/e/app.css").text
    assert ".logo::before" not in css and ".logo::after" not in css


def test_os_icones_da_navegacao_sao_desenho_e_nao_caractere(cliente):
    """A pessoa e o livro aberto, como no desenho aprovado."""
    pagina = cliente.get("/e").text
    assert "◉" not in pagina and "▤" not in pagina

    # Os dois destinos são desenhados na barra e no lado. Outros ícones
    # podem existir na moldura (por exemplo, Sair), então não contamos
    # todo SVG da página como se fosse navegação.
    assert pagina.count('cx="12" cy="7" r="4"') == 2      # a pessoa
    assert pagina.count("M12 20V6c-3-2-6-3-10-2v14") == 2  # o livro aberto


def test_as_duas_saidas_do_app_encerram_a_sessao(cliente):
    """Desktop e celular oferecem a mesma saída real."""
    pagina = cliente.get("/e").text
    codigo = cliente.get("/e/app.js").text

    assert 'id="sair-lado"' in pagina
    assert 'id="sair-topo"' in pagina
    assert '$("#sair-lado")' in codigo
    assert '$("#sair-topo")' in codigo
    assert 'fetch("/e/api/sair"' in codigo
    assert "entra();" in codigo


def test_a_pagina_nao_fica_mais_larga_que_o_celular(cliente):
    """Nada de rolagem lateral."""
    css = cliente.get("/e/app.css").text

    assert "grid-template-columns: minmax(0, 1fr)" in css        # a moldura
    assert "repeat(6, minmax(0, 1fr))" in css                    # o código
    assert "grid-template-columns: 40px minmax(0, 1fr) 18px" in css  # o aparelho
    assert "16rem minmax(0, 1fr)" in css                         # o desktop

    # E o campo cabe na coluna em vez de mandar nela.
    assert "min-width: 0" in css


def test_mobile_reserva_espaco_para_as_duas_barras_fixas(cliente):
    """Conteúdo e botões não podem terminar sob a navegação."""
    css = cliente.get("/e/app.css").text

    assert "--topo-altura: 66px" in css
    assert "--barra-altura: 64px" in css
    assert "padding-top: var(--topo-altura)" in css
    assert "padding-bottom: calc(var(--barra-altura)" in css
    assert ".topo {" in css and "position: fixed" in css
    assert ".barra {" in css and "z-index: 5" in css


def test_envio_tem_loading_de_tres_pontos_e_acoes_responsivas(cliente):
    js = cliente.get("/e/app.js").text
    css = cliente.get("/e/app.css").text

    assert 'class="carregando"' in js
    assert 'class="pontos"' in js
    assert "@keyframes ponto" in css
    assert ".acoes { display: grid" in css


def test_css_responsivo_nao_perde_o_menu_desktop(cliente):
    """Uma regra sem fechar não pode engolir o restante do breakpoint."""
    css = cliente.get("/e/app.css").text

    assert css.count("{") == css.count("}")
    desktop = css[css.index("@media (min-width: 62rem)") :]
    assert ".lado {" in desktop
    assert ".topo { display: none; }" in desktop


def test_o_dominio_sozinho_abre_o_aplicativo(cliente):
    r = cliente.get("/", follow_redirects=False)
    assert r.status_code == 307
    assert r.headers["location"] == "/e"


def test_anotacoes_sao_so_da_pessoa_e_as_novas_primeiro(pessoa):
    """A aba Anotações do Meu Acervo lê isto. RN-91: só as desta pessoa."""
    from tinto.modelo import Acao, notas
    notas.anota("outro@x.com", Acao(id="a", t="De outro"), "", "2026-10-01")
    notas.anota("eu@x.com", Acao(id="b", t="Velha", l="texto velho"), "", "2026-09-30")
    notas.anota("eu@x.com", Acao(id="c", t="Nova", l="texto novo"), "", "2026-10-01")

    lista = pessoa.get("/e/api/anotacoes").json()["anotacoes"]
    titulos = [a["titulo"] for a in lista]
    assert "De outro" not in titulos
    assert titulos.index("Nova") < titulos.index("Velha")
    assert lista[titulos.index("Nova")]["corpo"] == "texto novo"


def test_sem_sessao_as_anotacoes_nao_respondem(cliente):
    cliente.cookies.clear()
    assert cliente.get("/e/api/anotacoes").status_code == 401


def test_aparelho_parado_ha_tempo_nao_aparece_conectado(pessoa):
    import time
    from tinto.memoria import memoria
    ap = memoria.registra("AA:BB:CC:11:22:33", "prova-teste")
    ap.pessoa = "eu@x.com"
    ap.visto_em = time.time()
    assert pessoa.get("/e/api/conta").json()["aparelhos"][0]["online"] is True
    ap.visto_em = time.time() - 600
    assert pessoa.get("/e/api/conta").json()["aparelhos"][0]["online"] is False
