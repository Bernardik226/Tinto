"""As rotas."""

import asyncio
import json
import logging
import os
import re
import time
import uuid
from pathlib import Path

import httpx
from fastapi import (Depends, FastAPI, Header, HTTPException,
                     Request)
from fastapi.responses import HTMLResponse, RedirectResponse
from pydantic import BaseModel, Field

from . import agenda, avisos, config, ia, oauth, portaria
from .pwa import pwa
from .acervo_rotas import rotas_acervo, _registra_rotas_do_device
from .pwa import volta_do_google as volta_pessoa
from .contrato import (Acao, Gesto, Item, Removido, RespostaCaptura,
                       RespostaOlhar, RespostaPull, Tipo)
from .memoria import FORMATO_REGRA, Aparelho, Sincronia, memoria, nome_limpo
from .modelo import anotacao_para_item, e_do_google, notas

# O uvicorn configura o logger DELE, não o nosso: sem esta linha, todo
# `log.info` deste projeto é escrito para lugar nenhum — e o diagnóstico
# some justamente onde não há como pôr um depurador.
# `force=True` porque o uvicorn configura o root logger ANTES de importar
# este módulo, e `basicConfig` sem ele é um no-op silencioso: as linhas
# são escritas para lugar nenhum, e a ausência delas parece "o código não
# rodou" — que é a conclusão errada, e cara.
logging.basicConfig(
    level=os.environ.get("TINTO_LOG", "INFO").upper(),
    format="%(levelname)s %(name)s · %(message)s",
    force=True)

log = logging.getLogger("tinto")

# Antes de qualquer rota existir: dois workers são dois estados, e a
# conta de alguém some sem nada no log. Ver `config.confere_um_processo`.
config.confere_um_processo()

app = FastAPI(title="Tinto", version="1")

# Os cabeçalhos de segurança, em TODA resposta.
app.middleware("http")(portaria.cabecalhos)

# A outra superfície: o aplicativo da pessoa, num navegador. As rotas
# `/v1/` atendem o ESP32 com um token de aparelho.
app.include_router(pwa)
app.include_router(rotas_acervo)


# ── quem está falando ────────────────────────────────────────────────
def aparelho(authorization: str = Header(default="")) -> Aparelho:
    """O token do header vira o aparelho, ou 401."""
    token = authorization.removeprefix("Bearer ").strip()
    ap = memoria.por_token(token)
    if not ap:
        raise HTTPException(401, "token desconhecido")
    return ap


def exige_conta(ap: Aparelho = Depends(aparelho)) -> Aparelho:
    """O aparelho precisa estar pareado."""
    # A conta precisa continuar em TINTO_CONTAS: tirar um e-mail da lista
    # corta o acesso dos aparelhos dele na próxima chamada, e não só o
    # próximo login. O vínculo fica guardado — voltar à lista o devolve.
    if not ap.pessoa or not config.entra_conta(ap.pessoa):
        raise HTTPException(409, "aparelho sem conta")
    return ap


def exige_chaves(*nomes_e_valores) -> None:
    """503 dizendo QUAL chave falta."""
    faltando = [n for n, v in nomes_e_valores if v == config.FALTANDO]
    if faltando:
        raise HTTPException(503, f"falta configurar: {', '.join(faltando)}")


def exige_google() -> None:
    """As duas do OAuth, e as duas juntas."""
    exige_chaves(("GOOGLE_CLIENT_ID", config.GOOGLE_CLIENT_ID),
                 ("GOOGLE_CLIENT_SECRET", config.GOOGLE_CLIENT_SECRET))


# As rotas do Acervo do lado do APARELHO. Elas moram em
# `acervo_rotas.py`, junto das da pessoa — as duas falam do mesmo
# catálogo, e separá-las por porta espalharia a regra de posse por dois
# arquivos. Registradas aqui porque a dependência de token é daqui.
_registra_rotas_do_device(app, exige_conta)


async def acesso(ap: Aparelho) -> str:
    """O access token desta conta, ou 409 pedindo para reconectar."""
    exige_google()
    try:
        # A concessão pertence à pessoa, não a um aparelho. A cópia no
        # aparelho fica como migração para estados antigos; depois que a
        # pessoa entra novamente no app, o token novo central vence.
        refresh = memoria.refresh_de(ap.pessoa) or ap.google_refresh
        return await oauth.renovar(refresh)
    except oauth.SemConta:
        # Marcado, e não apagado. O `google_refresh` some no próximo
        # pareamento; guardar o fato de que ELE morreu é o que deixa o
        # aparelho dizer "reconecte" em vez de "conecte uma conta".
        ap.reconectar = True
        ap.pendentes.clear()
        ap.saindo.clear()
        memoria.grava()
        raise HTTPException(428, "a conta Google precisa ser reconectada")


# ── registro ─────────────────────────────────────────────────────────
class PedidoRegistrar(BaseModel):
    device_id: str = Field(max_length=64)
    prova: str = Field(default="", max_length=128)


@app.post("/v1/registrar")
def registrar(p: PedidoRegistrar, request: Request):
    """`device_id` + prova → token. Uma vez na vida do aparelho."""
    # ── o teto de tentativas ─────────────────────────────────────────
    # Registrar é aberto, e o que se protege é a ENUMERAÇÃO e o lixo no
    # estado. Vinte por minuto é folgado para o device, que tenta uma vez
    # a cada trinta segundos, e apertado para quem está varrendo.
    portaria.limita(request, "registrar", quantas=20, janela_s=60)

    # O aparelho se registra sozinho: existir aqui não dá acesso a nada
    # sem uma conta de TINTO_CONTAS vinculada. Quem hospeda vê cada
    # registro novo no log.
    novo = p.device_id not in memoria.por_device
    if novo and not memoria.cabe_registro_novo():
        log.warning("registro recusado · teto de aparelhos sem conta · %s · "
                    "ip %s", p.device_id, portaria.de_quem(request))
        raise HTTPException(429, "muitos aparelhos sem conta neste servidor")
    ap = memoria.registra(p.device_id, p.prova)
    if not ap:
        log.info("registro recusado · prova inválida para %s · ip %s",
                 p.device_id, portaria.de_quem(request))
        raise HTTPException(401, "prova inválida")
    if novo:
        log.info("registro · aparelho novo %s · ip %s", p.device_id,
                 portaria.de_quem(request))
    return {"device_token": ap.token}


# ── pareamento ───────────────────────────────────────────────────────
@app.post("/v1/parear/iniciar")
def parear_iniciar(ap: Aparelho = Depends(aparelho)):
    """Seis letras, dez minutos, uso único."""
    p = memoria.novo_codigo(ap.device_id)
    return {"codigo": p.codigo, "validade_s": p.validade_s}


@app.get("/v1/parear/estado")
def parear_estado(ap: Aparelho = Depends(aparelho)):
    """O device pergunta até virar."""
    # A mesma regra do `exige_conta`: uma conta que saiu de TINTO_CONTAS
    # não é vínculo, senão o aparelho alternaria entre "vinculado" aqui e
    # "sem conta" no pull.
    valida = bool(ap.pessoa) and config.entra_conta(ap.pessoa)
    return {"pareado": valida, "conta": ap.pessoa if valida else "",
            "reconectar": bool(valida and ap.reconectar)}


@app.get("/conectar")
def conectar(codigo: str = ""):
    """O endereço antigo. Hoje ele só aponta para o aplicativo."""
    codigo = codigo.strip().upper()
    para = f"/e?codigo={codigo}" if codigo.isalpha() and codigo else "/e"
    return RedirectResponse(para, status_code=303)


def _recado(titulo: str, corpo: str, status: int = 400) -> HTMLResponse:
    """Uma página, não um JSON."""
    return HTMLResponse(status_code=status, content=(
        f"<title>Tinto</title>"
        "<style>body{font:16px/1.6 system-ui,sans-serif;max-width:32rem;"
        "margin:15vh auto;padding:0 1.5rem}"
        "@media(prefers-color-scheme:dark){body{background:#16150f;"
        "color:#ede8e0}}</style>"
        f"<h1>{titulo}</h1>{corpo}"
        '<p>Volte ao <a href="/e">aplicativo</a> e comece de novo.</p>'))


@app.get("/oauth/retorno")
async def oauth_retorno(request: Request, code: str = "", state: str = "",
                        error: str = ""):
    """O Google devolve aqui. É onde o aparelho ganha dono."""
    exige_chaves(("GOOGLE_CLIENT_SECRET", config.GOOGLE_CLIENT_SECRET))

    if error:
        # A pessoa clicou em cancelar, ou negou um dos escopos. Não é
        # defeito de ninguém e não deve parecer um.
        return _recado("O consentimento não foi concluído",
                       "<p>Nada mudou: o aparelho continua sem conta.</p>")

    if not code or not state:
        return _recado("Faltou informação na volta do Google",
                       "<p>O endereço chegou incompleto.</p>")

    # O `state` diz de onde o pedido saiu: só o aplicativo (`pwa:`).
    if state.startswith("pwa:"):
        try:
            return await volta_pessoa(request, state, code)
        except httpx.HTTPError:
            # O `code` do Google vale UMA vez e poucos minutos, e recarregar
            # esta página é o jeito mais comum de gastá-lo.
            return _recado("Este endereço já foi usado",
                           "<p>O código que o Google devolve vale uma vez "
                           "só. Nada foi perdido.</p>")

    # Um `state` que não é nosso não veio deste servidor.
    return _recado("Esta volta não é de um pedido nosso",
                   "<p>Comece de novo pelo aplicativo.</p>")


@app.post("/v1/desparear")
def desparear(ap: Aparelho = Depends(aparelho)):
    """A conta sai do aparelho. O que já desceu FICA."""
    ap.pessoa = ""
    ap.google_refresh = ""
    ap.tz_nome = ""
    ap.reconectar = False
    ap.sinc = Sincronia()
    ap.pendentes.clear()
    ap.saindo.clear()
    ap.cursor = ""
    memoria.grava()

    return {"ok": True, "pareado": False}


# ── o delta ──────────────────────────────────────────────────────────
# **O teto é de BYTES, não de itens**, e ele é o número mais importante
# desta rota.
PULL_BYTES = 760


def _cabe(pendentes: list[dict], n: int, orcamento: int) -> tuple[int, int]:
    """Quantos dos primeiros cabem em `orcamento` bytes, e quanto gastam."""
    usado = cabem = 0
    for bruto in pendentes[:n]:
        custo = len(json.dumps(bruto, ensure_ascii=False))
        if usado + custo > orcamento and cabem:
            break
        usado += custo
        cabem += 1
    return cabem, usado


# ── quanto tempo o servidor SEGURA um pull vazio ─────────────────────
# É o long polling, e ele existe porque uma agenda de mesa é olhada o dia
# inteiro: um evento marcado no celular que leva um minuto para aparecer é
# o aparelho parecendo lento justamente enquanto alguém olha para ele.
ESPERA_MAX_S = 25.0

# De quanto em quanto tempo o servidor olha de novo para o Google enquanto
# segura. Cinco segundos é o bastante para a latência ficar em segundos, e
# raro o bastante para não virar um pull por segundo do lado de lá.
ESPERA_PASSO_S = 5.0

# ── e o passo de quem TEM canal de aviso ─────────────────────────────
# Com o Google avisando, perguntar de cinco em cinco segundos é
# exatamente o desperdício que o canal existe para acabar: doze chamadas
# por minuto por aparelho pendurado, o dia inteiro, quase todas voltando
# vazias. Com um aparelho é ruído; com dezenas é a diferença entre caber e
# não caber na cota.
ESPERA_PASSO_COM_CANAL_S = 20.0


# ── os canais de aviso, criados e renovados ──────────────────────────
# Uma conferida por pull, e ela quase sempre não faz nada: comparar
# `expira` com o relógio é barato, e a criação só acontece na primeira vez
# e a cada renovação.
CANAL_FOLGA_S = 30 * 60


async def _garante_canais(ap, access: str) -> None:
    if config.TINTO_WEBHOOK_URL == config.FALTANDO:
        return
    if not ap.pessoa:
        return

    agora = time.time()

    # As agendas que ESTE aparelho sincroniza. Vigiar as outras seria
    # pagar canal por calendário que a pessoa desligou, e receber aviso de
    # coisa que nunca vai descer.
    alvos = set(ap.sinc.tokens) | set(ap.sinc.tokens_novos)

    for cal_id in sorted(alvos):
        canal = ap.sinc.canais.get(cal_id)

        # Este calendário já disse que não suporta push. Não se pergunta
        # de novo: a resposta não muda, e perguntar custa uma chamada de
        # API por pull.
        if canal and canal.get("sem_push"):
            continue

        if canal and canal.get("expira", 0) - agora > CANAL_FOLGA_S:
            # Vivo. Só garante que o mapa em memória conhece o canal — ele
            # se perde a cada reinício do servidor, e sem isto o aviso
            # chegaria sem dono.
            avisos.registra_canal(canal["id"], ap.pessoa)
            continue

        # Vencido ou perto disso: fecha o antigo antes de abrir o novo,
        # senão o Google mantém os dois e o mesmo evento vira dois avisos.
        if canal:
            avisos.esquece_canal(canal.get("id", ""))
            await agenda.para_de_vigiar(access, canal.get("id", ""),
                                        canal.get("recurso", ""))

        # ── o id do canal tem alfabeto próprio ───────────────────
        # `[A-Za-z0-9\-_+/=]+`, e o `device_id` é um MAC: os dois-pontos
        # dele derrubavam TODA criação de canal com 400 `channelIdInvalid`.
        limpo = re.sub(r"[^A-Za-z0-9\-_]", "-", ap.device_id)
        canal_id = f"tinto-{limpo}-{uuid.uuid4().hex[:12]}"
        novo = await agenda.vigia(access, cal_id, canal_id,
                                  config.TINTO_WEBHOOK_URL,
                                  config.TINTO_WEBHOOK_SEGREDO)
        if not novo:
            # Falha temporária — rede, token, servidor. Não insiste neste
            # pull: o próximo tenta de novo, e até lá a pergunta periódica
            # sustenta a sincronização.
            continue

        ap.sinc.canais[cal_id] = novo

        if novo.get("sem_push"):
            # Guardado para NÃO tentar mais. É a única entrada em `canais`
            # que não é um canal: é a memória de que não pode haver um.
            memoria.grava()
            log.info("%s não suporta aviso; segue pela pergunta", cal_id)
            continue
        avisos.registra_canal(novo["id"], ap.pessoa)
        memoria.grava()
        log.info("canal de aviso %s para %s", novo["id"], cal_id)


@app.get("/v1/pull", response_model=RespostaPull)
async def pull(desde: str = "", n: int = 20, esperar: int = 0,
               tudo: int = 0, nome: str = "",
               ap: Aparelho = Depends(exige_conta)):
    """A ÚNICA via de entrada de dado no aparelho."""
    # O NOME do aparelho vem de carona: o device só o manda quando foi
    # trocado nele, e aí ele vale. A resposta devolve o nome atual, e é
    # assim que um nome trocado no aplicativo chega ao aparelho.
    nome = nome_limpo(nome)[:24]
    if nome and nome != ap.nome:
        ap.nome = nome
        memoria.grava()

    # O device precisa saber que esta colheita é a verdade INTEIRA, e não
    # um delta. Ver `RespostaPull.zerar`: sem isto, o que veio de uma
    # agenda que foi desligada fica no cartão para sempre.
    zerar = False

    # O token de acesso, quando houve colheita. Ele é o que a espera usa
    # para perguntar de novo ao Google — e só existe se o buffer estava
    # vazio, que é exatamente a condição da espera.
    access = ""

    if not ap.pendentes and not ap.saindo:
        # ── `tudo=1`: o DEVICE pede a verdade inteira ────────────────
        # O `syncToken` do Google nunca reenvia o que já deu. Um lote que
        # se perde entre o servidor e o cartão — um reinício no meio, um
        # firmware com defeito, um cartão que falhou na escrita — se perde
        # PARA SEMPRE, e todo pull seguinte volta vazio com o token
        # apontando para o futuro. O aparelho fica com a agenda em branco
        # e o servidor achando que entregou.
        if ap.sinc.janela_v < 1:
            ap.sinc.recomecar()

        # ── a leitura mudou de forma, então recomeça UMA vez ──────────
        # A série passou a descer como regra. O cartão de quem já
        # sincronizou está cheio das ocorrências soltas do modelo antigo,
        # e o Google nunca vai mandar apagá-las: elas não existem mais
        # como eventos autônomos, então não há `cancelled` para elas.
        elif ap.sinc.formato < FORMATO_REGRA:
            ap.sinc.recomecar()
            log.info("formato %d: recomeço para a série virar regra",
                     FORMATO_REGRA)

        elif tudo:
            ap.sinc.recomecar()

        # Um resync completo por dia é o teto do estrago de uma resposta
        # perdida: o pior caso vira "apareceu amanhã" em vez de "nunca
        # apareceu".
        elif ap.sinc.precisa_de_tudo():
            ap.sinc.recomecar()

        # `recomecar()` zera os tokens, e é isso que define um recomeço —
        # tanto o do resync diário acima quanto o do `POST /v1/agendas`.
        zerar = ap.sinc.recomecou

        access = await acesso(ap)
        itens, fora, tz_nome = await agenda.delta(access, ap.sinc)
        ap.sinc.janela_v = 1
        memoria.grava()

        # ── o que a colheita trouxe, e de onde ───────────────────────
        # Um `pull` que volta vazio tem três causas que se parecem: não há
        # nada novo, as agendas estão desligadas, ou o `syncToken` já
        # entregou tudo antes. Sem este log, as três chegam ao device como
        # o mesmo silêncio — e o vidro vazio não distingue nenhuma.
        # O QUE desceu, e não só quantos. "11 itens" não distingue onze
        # eventos de uma lista com dez tarefas — e quando alguém diz que a
        # lista não apareceu, é exatamente essa distinção que decide se o
        # problema é daqui ou do vidro.
        quais = {}
        for it in itens:
            quais[int(it.tp)] = quais.get(int(it.tp), 0) + 1
        tipos = " ".join(f"{Tipo(k).name.lower()}={v}"
                         for k, v in sorted(quais.items()))

        log.info("pull %s · %d itens (%s), %d fora · %d escolhas, "
                 "%d tokens · zerar=%s", ap.device_id, len(itens),
                 tipos or "nada", len(fora),
                 len(ap.sinc.escolhas), len(ap.sinc.tokens), zerar)

        # A bandeira cai só DEPOIS de a colheita chegar.
        ap.sinc.recomecou = False

        # As ANOTAÇÕES entram no mesmo lote, e vão na FRENTE.
        novas = notas.desceram_depois(ap.pessoa, ap.sinc.anotacoes_desde)
        if novas:
            ap.sinc.anotacoes_novo = novas[-1].criada_em

        ap.pendentes = ([anotacao_para_item(a).model_dump() for a in novas]
                        + [it.model_dump() for it in itens])
        ap.saindo = [x.model_dump() for x in fora]

        if tz_nome:
            # O fuso vem do Google Agenda, e é a fonte certa: é nele que
            # os eventos da pessoa foram marcados. Um evento das 15:00 é
            # às 15:00 do calendário dela, não do lugar onde ela abriu o
            # Tinto — e por isso não se pergunta a geolocalização por IP.
            ap.tz_nome = tz_nome
            ap.tz_min = agenda.fuso_em_minutos(tz_nome)

        memoria.grava()

    # ── e aqui ele ESPERA, se o device pediu ─────────────────────────
    # Só quando a colheita veio VAZIA: com algo na mão, segurar a resposta
    # seria atrasar de propósito o que já está pronto.
    if access:
        await _garante_canais(ap, access)

    if esperar and access and not ap.pendentes and not ap.saindo:
        limite = time.monotonic() + ESPERA_MAX_S

        # Com canal, o laço abaixo dorme ESPERANDO SER ACORDADO em vez de
        # dormir um prazo fixo — e a latência do que chega deixa de ser o
        # intervalo da pergunta e passa a ser o tempo de rede.
        espera = avisos.espera_de(ap.pessoa)
        espera.clear()

        while time.monotonic() < limite:
            # Acordado pelo Google, ou pelo prazo. Os dois caminhos caem na
            # mesma pergunta abaixo: o aviso não diz O QUE mudou, ele diz
            # QUE mudou — quem descobre o quê continua sendo o delta.
            passo = (ESPERA_PASSO_COM_CANAL_S if ap.sinc.canais
                     else ESPERA_PASSO_S)
            try:
                await asyncio.wait_for(espera.wait(), timeout=passo)
                espera.clear()
            except asyncio.TimeoutError:
                pass

            # A pergunta nova ao Google. Se ele tiver algo, a resposta sai
            # na hora.
            try:
                itens, fora, _ = await agenda.delta(access, ap.sinc)
            except (httpx.HTTPError, oauth.SemConta):
                # Falhar no meio da espera não é diferente de falhar no
                # começo: devolve o que há (nada) e o device tenta de novo.
                # Levantar aqui trocaria um pull vazio por um erro.
                break

            novas = notas.desceram_depois(ap.pessoa, ap.sinc.anotacoes_desde)
            if novas:
                ap.sinc.anotacoes_novo = novas[-1].criada_em

            if itens or fora or novas:
                ap.pendentes = ([anotacao_para_item(a).model_dump()
                                 for a in novas]
                                + [it.model_dump() for it in itens])
                ap.saindo = [x.model_dump() for x in fora]
                memoria.grava()
                break

    n = max(1, min(n, 20))

    # O estado inteiro numa linha, e FORA do `if` da colheita: um pull que
    # não colhe nada porque o buffer ainda tem coisa é indistinguível, de
    # fora, de um que colheu zero — e as duas situações pedem coisas
    # opostas de quem está diagnosticando.
    log.info("pull %s · buffer %d+%d · %d tokens · tz %d",
             ap.device_id, len(ap.pendentes), len(ap.saindo),
             len(ap.sinc.tokens), ap.tz_min)

    quantos, gasto = _cabe(ap.pendentes, n, PULL_BYTES)
    lote, ap.pendentes = ap.pendentes[:quantos], ap.pendentes[quantos:]

    # O que sobrou do orçamento vai para os removidos. Eles são baratos —
    # um `{"id":"g:…"}` são ~20 bytes — mas cabem no mesmo buffer de 1023,
    # e um removido cortado no meio é um objeto que o parser lê torto.
    quantos_fora, _ = _cabe(ap.saindo, n, max(PULL_BYTES - gasto, 40))
    fora, ap.saindo = ap.saindo[:quantos_fora], ap.saindo[quantos_fora:]

    mais = bool(ap.pendentes or ap.saindo)
    if not mais:
        ap.sinc.concluiu()
        memoria.grava()

    # O cursor existe para o contrato e para o log. Ele não escolhe o que
    # vem — quem escolhe é o buffer —, e por isso é um número de lote e
    # não um ponteiro: o device pode desligar no meio, e um ponteiro que
    # ele não devolve é um ponteiro que trava a fila.
    ap.cursor = str(int(ap.cursor or 0) + 1) if mais else ""

    # A quota vem de carona (RN-52): o aparelho só EXIBE o número que
    # recebeu, e nunca calcula. Ele já lê estes três campos — eles estavam
    # zerados desde sempre, dizendo "sem limite" para um serviço que tem.
    usados, limite, dias = memoria.quota(ap)

    return RespostaPull(
        itens=[Item(**x) for x in lote],
        removidos=[Removido(**x) for x in fora],
        cursor=ap.cursor, mais=mais, tz_min=ap.tz_min, zerar=zerar,
        quota_usados=usados, quota_limite=limite, quota_dias=dias,
        aparelho_nome=ap.nome)


# ── o que a pessoa está OLHANDO ──────────────────────────────────────
@app.get("/v1/olhar", response_model=RespostaOlhar)
async def olhar(mes: str = "", dia: str = "",
                ap: Aparelho = Depends(exige_conta)):
    """O mês que está na grade e o dia que ela acabou de abrir."""
    try:
        olhando = await acesso(ap)
    except Exception:
        olhando = ""

    if not olhando:
        return RespostaOlhar()

    mcm = ""
    mce = mct = 0
    do_dia: list = []

    if mes and len(mes) == 7:
        try:
            mce, mct = await agenda.marcas_do_mes(
                olhando, ap.sinc, int(mes[:4]), int(mes[5:7]), ap.tz_nome)
            mcm = mes
        except (ValueError, KeyError):
            mcm = ""

    if dia and len(dia) == 10:
        do_dia = await agenda.itens_do_dia(olhando, ap.sinc, dia, ap.tz_nome)

    log.info("olhar %s · mes=%s · dia=%s · %d itens",
             ap.device_id, mcm or "-", dia or "-", len(do_dia))

    return RespostaOlhar(mcm=mcm, mce=mce, mct=mct,
                         dd=dia if len(dia) == 10 else "", dia=do_dia)


# ── um gesto ─────────────────────────────────────────────────────────
@app.post("/v1/push")
async def push(g: Gesto, operacao: str = Header(default=""),
               ap: Aparelho = Depends(exige_conta)):
    """UM gesto. Sem lote — a fila saiu do device em 25/08."""
    ja = memoria.ja_respondeu(ap.pessoa, operacao)
    if ja:
        return ja

    if g.tp == Tipo.ANOTACAO:
        # ANOTAÇÃO não vai pro Google (RN-25). Ela não tem equivalente do
        # outro lado, e mandar geraria erro eterno — ela mora aqui, e é a
        # única coisa do sistema que é nossa.
        resultado = _gesto_de_anotacao(ap, g)
    elif not e_do_google(g.tp):
        # `tp:0` é gesto sem tipo, e um gesto sem tipo não sabe para onde
        # vai. Recusar é melhor que escolher: escolhendo, ele viraria uma
        # anotação silenciosa e a pessoa procuraria no Google para sempre.
        resultado = {"ok": False, "motivo": "gesto sem tipo"}
    else:
        access = await acesso(ap)

        # O que a fala disse e o aparelho não carrega — o fim do evento e
        # o que vai dentro da lista — volta da NOTA que gerou a ação.
        acao = _acao_da_nota(ap, g)
        if acao and acao.f and g.v == "criou":
            g.f = acao.f
        resultado = await agenda.aplica_gesto(access, g, ap.sinc, ap.tz_nome)

        # A lista acabou de nascer: o que foi falado DENTRO dela entra
        # agora. O gesto carrega só o título — `Gesto` não tem onde pôr o
        # conteúdo, e o device não abre lista item a item —, então os
        # itens vêm da nota que gerou a ação. Sem isto, "lista de compras:
        # arroz e feijão" virava uma lista vazia no celular.
        if resultado.get("ok") and g.tp == Tipo.LISTA and acao:
            dentro = list(acao.itens)
            if dentro:
                postos = await agenda.enche_lista(
                    access, ap.sinc.reais.get(resultado["id"], ""), dentro)
                log.info("lista %s · %d de %d itens dentro",
                         resultado["id"], postos, len(dentro))

        # A gravação em disco é UMA, no fim da rota: `sinc.onde` acabou
        # de aprender onde a coisa nova mora, e a resposta guardada logo
        # abaixo é o que impede a duplicata num reenvio. Gravar aqui, e
        # não lá, deixava a segunda de fora do arquivo — e era ela que
        # mais fazia falta.

    # A NOTA foi decidida. Ela deixa de estar "aberta" no momento em que
    # a primeira ação dela sobe — que é o OK da pessoa em Conferir
    # chegando aqui (RN-16).
    if g.nota:
        nota = notas.de(ap.pessoa, g.nota)
        if nota and nota.estado != "confirmada":
            nota.confirmar()
            memoria.confirmou(ap, nota.id, nota.transcricao,
                              [_o_que_virou(a) for a in nota.acoes])

    # O gesto no log, e é a única forma de saber de fora que ele chegou.
    log.info("push %s · %s %s tp=%d · %s", ap.device_id, g.v, g.id or "-",
             int(g.tp),
             "ok" if resultado.get("ok") else
             f"NAO: {resultado.get('motivo', 'sem motivo')}")

    memoria.guarda_resposta(ap.pessoa, operacao, resultado)
    memoria.grava()
    return resultado


def _o_que_virou(a: Acao) -> dict:
    """Uma ação, como o app a mostra: tipo, nome, quando e quanto."""
    return {"tp": int(a.tp), "t": a.t, "d": a.d, "h": a.h, "f": a.f,
            "itens": len(a.itens)}


def _acao_da_nota(ap: Aparelho, g: Gesto) -> Acao | None:
    """A ação da fala que virou este gesto."""
    if not g.nota:
        return None
    nota = notas.de(ap.pessoa, g.nota)
    if not nota:
        return None
    for a in nota.acoes:
        if a.tp == g.tp and (a.t == g.t or not g.t):
            return a
    return None


def _gesto_de_anotacao(ap: Aparelho, g: Gesto) -> dict:
    """Apagar, renomear, ou criar. Nesta ordem, e ela importa."""
    if g.v == "apagou":
        return {"ok": notas.apaga_anotacao(ap.pessoa, g.id), "id": g.id}

    if g.id.startswith("an:"):
        return {"ok": notas.renomeia_anotacao(ap.pessoa, g.id, g.t),
                "id": g.id}

    a = notas.anota(ap.pessoa, Acao(id=g.id, t=g.t, h=g.h, d=g.d,
                                    tp=g.tp, v=g.v), g.nota, g.d)
    return {"ok": True, "id": a.id}


# ── a voz ────────────────────────────────────────────────────────────
# O áudio chega de dois jeitos, e os dois são de propósito.
AUDIO_RAIZ = os.environ.get("TINTO_AUDIO_RAIZ", "")


async def audio_do_pedido(request: Request) -> bytes:
    tipo = request.headers.get("content-type", "")

    if tipo.startswith("multipart/form-data"):
        form = await request.form()
        parte = form.get("audio") or form.get("file") or form.get("wav")
        if parte is None or isinstance(parte, str):
            raise HTTPException(400, "faltou o arquivo de áudio")
        return await parte.read()

    try:
        corpo = await request.json()
    except ValueError:
        corpo = {}

    caminho = str(corpo.get("wav", "")).strip()
    if not caminho:
        raise HTTPException(400, "faltou o áudio")

    if not AUDIO_RAIZ:
        raise HTTPException(
            415, "mande o áudio em multipart; caminho só com TINTO_AUDIO_RAIZ")

    raiz = Path(AUDIO_RAIZ).resolve()
    alvo = (raiz / caminho.lstrip("/")).resolve()
    # `..` não sobe. A conferência é sobre o caminho RESOLVIDO porque é o
    # que o sistema de arquivos vai usar — conferir a string crua deixa
    # passar link simbólico.
    if not alvo.is_relative_to(raiz) or not alvo.is_file():
        raise HTTPException(404, "áudio não encontrado")

    return alvo.read_bytes()


# `/v1/transcrever` saiu em 28/08.


def _titulo_da_fala(falou: str) -> str:
    """As primeiras palavras, cortadas onde a frase respira."""
    limpo = " ".join((falou or "").split())
    if not limpo:
        return "Anotação"

    for marca in (". ", "? ", "! ", "; ", ", "):
        antes = limpo.split(marca)[0]
        if 8 <= len(antes) <= 60:
            return antes

    if len(limpo) <= 60:
        return limpo

    return limpo[:60].rsplit(" ", 1)[0] + "…"


@app.post("/v1/captura", response_model=RespostaCaptura)
async def captura(request: Request, operacao: str = Header(default=""),
                  ap: Aparelho = Depends(exige_conta)):
    """A fala inteira → o que a IA entendeu."""
    ja = memoria.ja_respondeu(ap.pessoa, operacao)
    if ja:
        return ja

    exige_chaves(("GROQ_API_KEY", config.GROQ_API_KEY),
                 ("ANTHROPIC_API_KEY", config.ANTHROPIC_API_KEY))

    audio = await audio_do_pedido(request)

    segundos = ia.duracao_s(audio)
    if not memoria.cabe_falar(ap, segundos):
        raise HTTPException(402, "sem minutos de fala neste ciclo")

    # STT e leitura da agenda não dependem um do outro. Rodá-los em fila
    # fazia a pessoa pagar os dois tempos inteiros antes de a LLM começar.
    captura_inicio = time.monotonic()
    transcricao = asyncio.create_task(ia.transcrever(audio))

    # ── o que JÁ EXISTE vai junto com a fala ─────────────────────────
    # Sem isto a LLM não tem em que apontar: "muda o dentista pra sexta"
    # devolvia uma ação `editou` com id inventado, o backend a tratava
    # como nova, e nascia um SEGUNDO dentista na sexta com o de quinta
    # ainda lá. A tela de Conferir chegava a dizer "Confirmar a mudança"
    # — e o que subia era uma criação.
    existentes = []
    panorama_inicio = time.monotonic()
    try:
        existentes = await agenda.panorama(await acesso(ap), ap.sinc,
                                           ap.tz_nome)
    except Exception as erro:
        # Falhar aqui não pode custar a FALA. Sem a lista, a LLM continua
        # criando e anotando — só deixa de alcançar o que já existe.
        log.warning("panorama falhou, a fala segue sem ele: %s", erro)

    panorama_ms = int((time.monotonic() - panorama_inicio) * 1000)
    falou = await transcricao
    estrutura_inicio = time.monotonic()
    acoes = await ia.estruturar(falou, ap.tz_min, existentes)
    log.info("captura: panorama=%d ms · estrutura=%d ms · total=%d ms · "
             "%d existentes", panorama_ms,
             int((time.monotonic() - estrutura_inicio) * 1000),
             int((time.monotonic() - captura_inicio) * 1000),
             len(existentes))

    # ── zero ações ainda é uma FALA ──────────────────────────────────
    # Ela vira ANOTAÇÃO, com a transcrição como conteúdo.
    if not acoes:
        acoes = [Acao(v="anotou", id="n:1", tp=Tipo.ANOTACAO,
                      t=_titulo_da_fala(falou))]

    # Cobra DEPOIS de as duas darem certo. Uma falha do Whisper descontando
    # minutos seria a pessoa perdendo saldo por um erro que não foi dela —
    # e sem ter como saber que perdeu.
    # A NOTA nasce aqui, e é ela a unidade do sistema — não o item. Uma
    # sessão de fala, uma transcrição, de uma a três ações.
    nota = notas.abre(ap.pessoa, falou, acoes)

    memoria.gastou(ap, segundos, capturou=True,
                   tokens=ia.tokens_da_ultima_estrutura(), nota=nota.id)

    # O aparelho guarda a frase em 640 bytes (FALA_MAX): corta por BYTE,
    # sem partir um acento no meio.
    curta = falou.encode()[:636].decode("utf-8", "ignore")
    r = RespostaCaptura(falou=curta, acoes=acoes[:3], nota=nota.id)
    # A mesma proteção do gesto, e aqui ela vale ainda mais: o pior bug
    # possível deste sistema é a resposta da captura se perder, a pessoa
    # apertar de novo, e a fala inteira ser transcrita e cobrada duas
    # vezes. Em disco, ela sobrevive ao deploy.
    memoria.guarda_resposta(ap.pessoa, operacao, r.model_dump())
    memoria.grava()
    return r


# ── as agendas (T-32 · Sincronização) ────────────────────────────────
class EscolhaDeAgenda(BaseModel):
    """Qual agenda, e ligada ou não."""

    id: str = Field(default="", max_length=128)
    t: str = Field(default="", max_length=63)
    on: bool = True


# ── o firmware, e por que o device é quem compara ────────────────────
@app.get("/v1/agendas")
async def agendas(ap: Aparelho = Depends(exige_conta)):
    """As agendas da conta e o interruptor de cada uma."""
    access = await acesso(ap)
    lista, total = await agenda.catalogo(access, ap.sinc)
    return {"agendas": lista, "total": total}


@app.post("/v1/agendas")
async def escolher_agenda(e: EscolhaDeAgenda,
                          ap: Aparelho = Depends(exige_conta)):
    """Liga ou desliga uma agenda, e força a releitura."""
    access = await acesso(ap)

    alvo = ""
    lista, _ = await agenda.catalogo(access, ap.sinc)
    for cal in lista:
        if (e.id and cal["id"] == e.id) or (e.t and cal["t"] == e.t):
            alvo = cal["id"]
            break

    if not alvo:
        # 404 e não 400: negar a existência é a mesma regra do resto
        # (RN-92), e uma agenda que não é desta conta não é desta conta.
        raise HTTPException(404, "agenda desconhecida")

    ap.sinc.escolhas[alvo] = e.on

    ap.sinc.recomecar()
    ap.pendentes.clear()
    ap.saindo.clear()
    memoria.grava()

    return {"ok": True, "id": alvo, "on": e.on}


# ── saúde ────────────────────────────────────────────────────────────
# ── o Google avisando ────────────────────────────────────────────────
@app.post("/v1/google/aviso")
async def google_aviso(
    canal: str = Header(default="", alias="X-Goog-Channel-ID"),
    token: str = Header(default="", alias="X-Goog-Channel-Token"),
    estado: str = Header(default="", alias="X-Goog-Resource-State"),
):
    esperado = config.TINTO_WEBHOOK_SEGREDO
    if esperado != config.FALTANDO and token != esperado:
        # 200, e não 403. Um canal antigo — de antes de o segredo mudar —
        # continuaria batendo aqui para sempre se recusássemos, porque o
        # Google só desiste com 4xx repetido e nós não sabemos qual canal
        # é. Ignorar em silêncio é o que faz ele expirar em paz.
        log.warning("aviso com token errado no canal %s", canal)
        return {"ok": True}

    # `sync` é o aviso de boas-vindas que o Google manda ao criar o canal:
    # ele diz "estou de pé", e não "algo mudou". Acordar por ele faria todo
    # canal novo custar uma colheita.
    if estado == "sync":
        return {"ok": True}

    quem = avisos.acorda(canal)
    if not quem:
        # Canal que não se reconhece: o servidor reiniciou e perdeu o mapa,
        # mas o canal continua vivo do lado de lá. Acordar todo mundo custa
        # uma pergunta por aparelho pendurado e a maioria volta vazia — e o
        # contrário é uma agenda que só atualiza no ciclo seguinte, sem
        # nada na tela explicando.
        avisos.acorda_todos()

    return {"ok": True}


# ── a prova de que o domínio é seu ───────────────────────────────────
# O Search Console pede um arquivo com um nome que ele sorteia. Ele vem de
# variável de ambiente, e não de um commit, porque a verificação é um
# passo manual do dono do projeto: prendê-la a um deploy seria travar o
# console numa fila de CI.
@app.get("/google{miolo}.html")
async def verificacao_do_google(miolo: str):
    valor = config.GOOGLE_SITE_VERIFICATION
    if valor == config.FALTANDO or miolo != valor:
        raise HTTPException(status_code=404, detail="não é este arquivo")
    return HTMLResponse(f"google-site-verification: google{valor}.html")


@app.get("/saude")
def saude():
    """O que está de pé e o que falta configurar."""
    return {
        "ok": True,
        "falta": config.falta(GOOGLE_CLIENT_ID=config.GOOGLE_CLIENT_ID,
                              GOOGLE_CLIENT_SECRET=config.GOOGLE_CLIENT_SECRET,
                              GOOGLE_REDIRECT=config.GOOGLE_REDIRECT,
                              GROQ_API_KEY=config.GROQ_API_KEY,
                              ANTHROPIC_API_KEY=config.ANTHROPIC_API_KEY),
        "aparelhos": len(memoria.aparelhos),
        "pareados": sum(1 for a in memoria.aparelhos.values() if a.pessoa),

        # Sem nenhuma conta liberada, ninguém entra: é o primeiro passo de
        # quem sobe um servidor novo, e o mais fácil de esquecer.
        "contas": len(config.CONTAS),
        "aviso": "" if config.CONTAS else
                 "TINTO_CONTAS está vazia: ninguém consegue entrar",

        # ── o aviso do Google está de pé? ────────────────────────────
        # Duas perguntas que só o servidor sabe responder, e que sem isto
        # exigiam ler o log do provedor para descobrir: a variável chegou,
        # e o Google aceitou abrir canal.
        "webhook": config.TINTO_WEBHOOK_URL != config.FALTANDO,
        # Só os canais DE VERDADE: as marcas de "não suporta push" moram
        # no mesmo mapa e não são canal nenhum. Contá-las faria o número
        # dizer que está ligado quando não está.
        "canais": sum(1 for a in memoria.aparelhos.values()
                      for c in a.sinc.canais.values() if c.get("id")),
    }


# ── anotações ────────────────────────────────────────────────────────
@app.get("/v1/anotacoes")
def anotacoes(ap: Aparelho = Depends(exige_conta)):
    """As anotações da pessoa, COM O CORPO. Esta rota é do PWA."""
    return {"anotacoes": [
        {"id": a.id, "t": a.titulo, "c": a.corpo,
         "d": a.dia, "nota": a.nota_id}
        for a in notas.anotacoes_de(ap.pessoa)
    ]}
