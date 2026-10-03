"""O estado do servidor."""

import hashlib
import json
import os
import secrets
import threading
import logging
import time
from dataclasses import asdict, dataclass, field
from functools import wraps

from . import config
from pathlib import Path

log = logging.getLogger("tinto")

# ── quanto tempo um delta pode ser incremental ───────────────────────
# O `syncToken` do Google é barato e nunca traz o que não mudou — e é
# exatamente por isso que um item perdido no caminho fica perdido para
# sempre. Um resync completo por dia é o teto do estrago: o pior caso vira
# "o evento apareceu no dia seguinte" em vez de "o evento nunca apareceu".
RESYNC_S = 24 * 3600

# A forma da leitura. A série desce como REGRA desde 11/09/2026, e um
# aparelho que sincronizou antes disso tem o cartão cheio de ocorrências
# soltas que o Google nunca vai mandar apagar — elas deixaram de existir
# como eventos autônomos. Subir este número recomeça a leitura UMA vez.
FORMATO_REGRA = 2


def serializa(metodo):
    """Uma operação inteira por vez dentro do único processo suportado."""
    @wraps(metodo)
    def chamada(self, *args, **kwargs):
        with self._trava:
            return metodo(self, *args, **kwargs)
    return chamada


@dataclass
class Sincronia:
    """O que o backend precisa lembrar entre dois `pull`."""

    # calendário → `syncToken`. Um por agenda, porque o Google dá um por
    # agenda: um token só para todas não existe.
    tokens: dict[str, str] = field(default_factory=dict)

    # Os tokens da colheita que ainda está sendo entregue ao device. Eles
    # só viram `tokens` quando o buffer esvazia — avançar antes seria
    # dizer ao Google "já recebi tudo" com metade ainda na fila daqui.
    tokens_novos: dict[str, str] = field(default_factory=dict)

    tarefas_desde: str = ""
    tarefas_novo: str = ""

    # ── os BURACOS de cada série ─────────────────────────────────────
    # id do evento mestre → dias em que a rotina NÃO acontece. Apagar uma
    # terça e mover uma quarta produzem a mesma coisa aqui: aquele dia
    # deixou de ser da série.
    buracos: dict[str, list[str]] = field(default_factory=dict)

    # A leitura mudou de forma em 11/09/2026: a série desce como REGRA, e
    # não mais ocorrência por ocorrência. O cartão de quem já sincronizou
    # está cheio de ocorrências soltas que o Google nunca vai mandar
    # apagar — elas não existem mais como eventos autônomos.
    formato: int = 0

    # id do item → agenda ou lista em que ele mora. Sem isto, todo PATCH
    # de tarefa vai para `@default` e devolve 404 para quem tem mais de
    # uma lista.
    onde: dict[str, str] = field(default_factory=dict)

    # id do item → o id INTEIRO do Google. O contrato do device carrega 39
    # caracteres, e instância de série (`<mestre>_20260910T130000Z`) passa
    # disso: o que chega de volta num gesto é um PEDAÇO do id. Sem esta
    # tabela o DELETE ia para um id que não existe, o Google respondia 404
    # — que aqui é lido como sucesso — e o evento saía do cartão e ficava
    # no Google Agenda para sempre.
    reais: dict[str, str] = field(default_factory=dict)

    # ── o GÊMEO da tarefa com hora ───────────────────────────────────
    # id do item → "<agenda>|<id do evento>".
    gemeos: dict[str, str] = field(default_factory=dict)

    # lista de tarefas → "n,k", a contagem que já desceu.
    contagens: dict[str, str] = field(default_factory=dict)

    agenda_tinto: str = ""
    cheio_em: float = 0.0        # quando foi o último resync completo

    # Versão da janela inicial já entregue. Quando a definição de “ontem”
    # muda, isto força uma única colheita completa nos aparelhos existentes
    # em vez de esperar o resync diário ou pedir que a pessoa limpe dados.
    janela_v: int = 0

    # O que a PESSOA respondeu em T-32: id da agenda → ligada.
    escolhas: dict[str, bool] = field(default_factory=dict)

    # ── os canais de aviso do Google ─────────────────────────────────
    # calendário → {"id", "recurso", "expira"}.
    canais: dict[str, dict] = field(default_factory=dict)

    # Até quando as ANOTAÇÕES já desceram. Elas não são delta do Google —
    # o Google não as conhece —, então o marcador delas é o relógio de
    # quando foram criadas aqui.
    anotacoes_desde: float = 0.0
    anotacoes_novo: float = 0.0

    # Esqueci tudo, e o device ainda não sabe.
    recomecou: bool = False

    def precisa_de_tudo(self) -> bool:
        return time.time() - self.cheio_em > RESYNC_S

    def recomecar(self) -> None:
        """Esquece onde parou. A próxima leitura traz tudo de novo."""
        self.tokens.clear()
        self.tokens_novos.clear()
        self.buracos.clear()

        # Quem recomeça já nasce no formato de agora: sem isto, todo
        # segundo pull de um aparelho novo viraria outro recomeço.
        self.formato = FORMATO_REGRA
        self.tarefas_desde = ""
        self.tarefas_novo = ""
        self.contagens.clear()
        self.anotacoes_desde = 0.0
        self.anotacoes_novo = 0.0
        self.cheio_em = time.time()

        # O device precisa saber. Marcada aqui, e não em quem chama, para
        # que nenhum caminho de recomeço fique sem contar: quem chama
        # `recomecar()` não precisa lembrar de avisar o device.
        self.recomecou = True

    def concluiu(self) -> None:
        """A colheita foi entregue inteira. Agora sim o marcador anda."""
        self.tokens.update(self.tokens_novos)
        self.tokens_novos.clear()
        if self.tarefas_novo:
            self.tarefas_desde = self.tarefas_novo
            self.tarefas_novo = ""
        if self.anotacoes_novo:
            self.anotacoes_desde = self.anotacoes_novo
            self.anotacoes_novo = 0.0


def nome_limpo(nome: str) -> str:
    """Sem aspas, barra invertida e controle: o aparelho grava o nome num
    JSON sem escape, e os dois lados precisam mostrar o mesmo texto."""
    return "".join(c for c in nome if c not in '"\\' and c >= " ").strip()


@dataclass
class Aparelho:
    device_id: str
    token: str

    # O SHA-256 da prova que o aparelho apresenta ao registrar
    # (`ENGENHARIA.md` §9: `secret_hash`). Ela é o que separa "eu sou este
    # aparelho" de "eu sei o número de série deste aparelho" — e o
    # `device_id` é um MAC, que se lê de fora.
    segredo: str = ""
    pessoa: str = ""          # o e-mail da conta Google, vazio = não pareado
    google_refresh: str = ""
    tz_min: int = 0
    tz_nome: str = ""         # "America/Sao_Paulo" — o evento criado precisa
    visto_em: float = field(default_factory=time.time)

    # A voz NÃO mora aqui. Ela mora em `Cota`, e a chave é a pessoa —
    # veja a classe logo abaixo.
    # O nome do aparelho ("Tinto da família"). É UM campo, o mesmo no
    # aparelho e no aplicativo, e muda dos dois lados: o aparelho manda o
    # dele no pull quando o trocou, e adota este quando não trocou.
    nome: str = ""

    # O Google não reconhece mais a concessão. O refresh token continua
    # guardado e não vale mais nada — some no próximo pareamento.
    reconectar: bool = False

    sinc: Sincronia = field(default_factory=Sincronia)

    # O que já foi colhido do Google e ainda não coube numa resposta.
    pendentes: list[dict] = field(default_factory=list)
    saindo: list[dict] = field(default_factory=list)
    cursor: str = ""


CICLO_S = 30 * 86400


@dataclass
class Cota:
    """O que a VOZ gastou, e de quem é o gasto."""
    usados_s: int = 0
    capturas: int = 0
    tokens: int = 0
    ciclo_em: float = field(default_factory=time.time)
    usos: list[dict] = field(default_factory=list)


@dataclass
class Sessao:
    """Quem está logado no aplicativo web."""
    dono: str
    porta: str = "app"
    criada_em: float = field(default_factory=time.time)


@dataclass
class Pareamento:
    """Um código de seis letras esperando alguém do outro lado."""

    codigo: str
    device_id: str
    criado_em: float = field(default_factory=time.time)
    pessoa: str = ""

    # CINCO minutos é o prazo de um código LIDO NUMA TELA e digitado num
    # celular. A pessoa está com o aparelho na frente e
    # o telefone na mão: o código não espera, ele acompanha um gesto que já
    # está acontecendo. Prazo longo num segredo curto é o pior dos dois.
    validade_s: float = 300

    def expirou(self) -> bool:
        return time.time() - self.criado_em > self.validade_s


class Memoria:
    def __init__(self, arquivo: str | None = None) -> None:
        # FastAPI executa rotas síncronas em threads mesmo com um worker.
        # A trava é reentrante porque uma mutação chama `grava()` antes de
        # devolver; a operação e o instantâneo precisam ser indivisíveis.
        self._trava = threading.RLock()
        self.aparelhos: dict[str, Aparelho] = {}      # token → aparelho
        self.por_device: dict[str, Aparelho] = {}     # device_id → aparelho
        self.pareamentos: dict[str, Pareamento] = {}  # código → pareamento

        # Os ids de operação já vistos, com a resposta que deram.
        self.operacoes: dict[str, dict] = {}

        # SHA-256 da ficha → a sessão dela.
        self.sessoes: dict[str, Sessao] = {}

        # chave da conta → o que a voz dela gastou.
        self.cotas: dict[str, Cota] = {}

        # e-mail → refresh token do Google.
        self.pessoas: dict[str, str] = {}

        # Sem arquivo = memória volátil, que é o que os testes querem: um
        # teste que grava em disco é um teste que depende do teste
        # anterior.
        self.arquivo = Path(arquivo) if arquivo else None
        self._carrega()

    # ── aparelhos ────────────────────────────────────────────────────
    @serializa
    def provisiona(self, device_id: str, nome: str = "") -> Aparelho:
        """O aparelho passa a existir neste servidor."""
        ap = self.por_device.get(device_id)
        if ap:
            if nome:
                ap.nome = nome[:31]
                self.grava()
            return ap

        # Nasce sem prova: a primeira que chegar no `registra` fica.
        ap = Aparelho(device_id=device_id, token=secrets.token_urlsafe(24),
                      nome=nome[:31])
        self.aparelhos[ap.token] = ap
        self.por_device[device_id] = ap
        self.grava()
        return ap

    # Quantos aparelhos sem conta o servidor aceita guardar. Registrar é
    # aberto, e sem teto quem achasse o endereço poderia encher o estado —
    # um arquivo reescrito inteiro a cada gravação. Uso real nunca chega
    # perto: quem registra vincula em minutos.
    SEM_CONTA_MAX = 32

    @serializa
    def cabe_registro_novo(self) -> bool:
        self._expira_sem_conta()
        return sum(1 for ap in self.por_device.values()
                   if not ap.pessoa) < self.SEM_CONTA_MAX

    def _expira_sem_conta(self) -> list[str]:
        """Aparelhos registrados que nunca ganharam conta, e sumiram."""
        corte = time.time() - 7 * 86400
        velhos = [d for d, ap in self.por_device.items()
                  if not ap.pessoa and ap.visto_em < corte]
        for d in velhos:
            ap = self.por_device.pop(d)
            self.aparelhos.pop(ap.token, None)
        return velhos

    @serializa
    def registra(self, device_id: str, prova: str = "") -> Aparelho | None:
        """O APARELHO se apresenta. Na primeira vez, passa a existir."""
        expirados = self._expira_sem_conta()
        for d in expirados:
            log.info("registro expirado · %s (sem conta há 7 dias)", d)
        if expirados:
            self.grava()

        ap = self.por_device.get(device_id)
        if not ap:
            ap = self.provisiona(device_id)

        if ap.segredo:
            if not prova or _digere(prova) != ap.segredo:
                return None
        elif prova:
            # A PRIMEIRA prova que chegar fica, e fecha a janela para
            # sempre no primeiro boot com rede.
            ap.segredo = _digere(prova)
            self.grava()
        return ap

    # ── as sessões do navegador ──────────────────────────────────────
    # Uma ficha vale ATÉ A PESSOA MANDAR SAIR. Sem prazo: o Tinto é um
    # aparelho de casa, o aplicativo é onde se manda livro e se confere a
    # cota, e expirar sozinho só ensina a pessoa a odiar a tela de login.
    # Quem encerra é o botão de sair.
    def _ficha(self, ficha: str) -> str:
        return hashlib.sha256(ficha.encode()).hexdigest()

    @serializa
    def abre_sessao(self, email: str, porta: str = "app") -> str:
        """Devolve a ficha. Ela existe UMA vez, aqui — depois só o SHA."""
        ficha = secrets.token_urlsafe(24)
        self.sessoes[self._ficha(ficha)] = Sessao(dono=email, porta=porta)
        self.grava()
        return ficha

    @serializa
    def dono_da_sessao(self, ficha: str, porta: str = "app") -> str:
        if not ficha:
            return ""
        s = self.sessoes.get(self._ficha(ficha))
        return s.dono if s and s.porta == porta else ""

    @serializa
    def fecha_sessao(self, ficha: str) -> None:
        if ficha and self.sessoes.pop(self._ficha(ficha), None):
            self.grava()

    @serializa
    def fecha_sessoes_de(self, email: str, porta: str = "app") -> None:
        """Todas as fichas desta pessoa nesta porta."""
        mortas = [k for k, s in self.sessoes.items()
                  if s.dono == email and s.porta == porta]
        for k in mortas:
            del self.sessoes[k]
        if mortas:
            self.grava()

    # ── a quota ──────────────────────────────────────────────────────
    def chave_da_cota(self, ap: Aparelho) -> str:
        """De quem é o gasto deste aparelho."""
        return ap.pessoa or f"aparelho:{ap.device_id}"

    @serializa
    def cota(self, ap: Aparelho) -> Cota:
        chave = self.chave_da_cota(ap)
        c = self.cotas.get(chave)
        if not c:
            c = self.cotas[chave] = Cota()
        return c

    @serializa
    def quota(self, ap: Aparelho) -> tuple[int, int, int]:
        """Usados, limite e dias até virar — em segundos e dias."""
        c = self.cota(ap)
        if time.time() - c.ciclo_em > CICLO_S:
            c.usados_s = 0
            c.capturas = 0
            c.tokens = 0
            c.usos.clear()
            c.ciclo_em = time.time()
            self.grava()

        limite = config.limite_de(ap.pessoa) if ap.pessoa \
            else config.QUOTA_PADRAO_S
        faltam = int((c.ciclo_em + CICLO_S - time.time()) // 86400)
        return c.usados_s, limite, max(0, faltam)

    @serializa
    def capturas(self, ap: Aparelho) -> int:
        """Quantas vezes a LLM foi chamada no ciclo, nesta conta."""
        return self.cota(ap).capturas

    @serializa
    def usos_recentes(self, ap: Aparelho, limite: int = 5) -> list[dict]:
        return list(reversed(self.cota(ap).usos[-max(0, limite):]))

    @serializa
    def cabe_falar(self, ap: Aparelho, segundos: int) -> bool:
        usados, limite, _ = self.quota(ap)
        return usados + segundos <= limite

    @serializa
    def gastou(self, ap: Aparelho, segundos: int, capturou: bool,
               tokens: int = 0, nota: str = "") -> None:
        """O consumo entra DEPOIS de a chamada dar certo."""
        c = self.cota(ap)
        c.usados_s += max(0, segundos)
        if capturou:
            c.capturas += 1
            c.tokens += max(0, tokens)
            c.usos.append({"segundos": max(0, segundos), "em": time.time(),
                           "tokens": max(0, tokens), "nota": nota})
            del c.usos[:-20]
        self.grava()

    @serializa
    def confirmou(self, ap: Aparelho, nota: str, texto: str,
                  acoes: list[dict]) -> None:
        """A fala foi confirmada no aparelho: o que se disse e o que virou."""
        for u in self.cota(ap).usos:
            if nota and u.get("nota") == nota:
                u["texto"] = texto
                u["acoes"] = acoes
                self.grava()
                return

    @serializa
    def por_token(self, token: str) -> Aparelho | None:
        ap = self.aparelhos.get(token)
        if ap:
            ap.visto_em = time.time()
        return ap

    # ── pareamento ───────────────────────────────────────────────────
    @serializa
    def novo_codigo(self, device_id: str,
                    validade_s: float = 300) -> Pareamento:
        """Seis letras, sem números."""
        # Limpa os expirados aqui, e não num varredor: são poucos, e um
        # processo de limpeza para dez aparelhos é máquina a mais para
        # manter de pé.
        for c in [c for c, p in self.pareamentos.items() if p.expirou()]:
            del self.pareamentos[c]

        letras = "ABCDEFGHJKLMNPQRSTUVWXYZ"     # sem I e O
        codigo = "".join(secrets.choice(letras) for _ in range(6))
        p = Pareamento(codigo=codigo, device_id=device_id,
                       validade_s=validade_s)
        self.pareamentos[codigo] = p
        self.grava()
        return p

    @serializa
    def confirma_pareamento(self, codigo: str, pessoa: str,
                            refresh: str) -> Aparelho | None:
        """O navegador terminou. O aparelho passa a ter dono."""
        if not refresh:
            return None

        p = self.pareamentos.pop(codigo.upper(), None)
        if not p or p.expirou():
            return None

        ap = self.por_device.get(p.device_id)
        if not ap:
            return None

        ap.pessoa = pessoa
        ap.google_refresh = refresh
        ap.reconectar = False

        # Conta nova, leitura do zero. O aparelho pode ter sido pareado
        # antes com outra conta, e os `syncToken` da anterior não valem
        # nada aqui — pior: fariam o primeiro pull vir vazio, e o aparelho
        # diria que a agenda da pessoa não tem nada.
        ap.sinc = Sincronia()
        ap.pendentes.clear()
        ap.saindo.clear()
        self.grava()
        return ap

    # ── as pessoas ───────────────────────────────────────────────────
    @serializa
    def guarda_pessoa(self, email: str, refresh: str) -> None:
        """A credencial da pessoa. Refresh vazio NÃO apaga o que existe."""
        if refresh:
            self.pessoas[email] = refresh
            for ap in self.por_device.values():
                if ap.pessoa == email:
                    ap.google_refresh = refresh
                    ap.reconectar = False
            self.grava()

    @serializa
    def refresh_de(self, email: str) -> str:
        return self.pessoas.get(email, "")

    # ── idempotência ─────────────────────────────────────────────────
    # **Com dono.** O id da operação nasce no device, e um device escolhe o
    # que quiser — um contador que reinicia no boot, um `op-1`. Guardá-los
    # num dicionário só faz o `op-1` de um aparelho devolver a resposta do
    # `op-1` de outro: a captura de uma pessoa aparecendo na tela de outra,
    # que é exatamente o que RN-91 existe para impedir.
    @serializa
    def ja_respondeu(self, dono: str, operacao: str) -> dict | None:
        if not operacao:
            return None
        guardada = self.operacoes.get(f"{dono}\x00{operacao}")
        return guardada.get("r") if guardada else None

    @serializa
    def guarda_resposta(self, dono: str, operacao: str,
                        resposta: dict) -> None:
        if operacao:
            self.operacoes[f"{dono}\x00{operacao}"] = {
                "em": time.time(), "r": resposta}

    # ── e ela precisa SOBREVIVER a um deploy ─────────────────────────
    # Esta lembrança é o que impede a duplicata: um gesto cuja resposta se
    # perdeu no caminho é reenviado pelo aparelho, e o que faz o reenvio
    # ser inofensivo é achar aqui a resposta de antes em vez de executar
    # de novo.
    OPERACOES_S = 24 * 3600

    def _operacoes_vivas(self) -> dict:
        corte = time.time() - self.OPERACOES_S
        return {k: v for k, v in self.operacoes.items()
                if v.get("em", 0) >= corte}

    # ── o instantâneo em disco ───────────────────────────────────────
    # Só o que dói perder: aparelho, conta, refresh token, onde a
    # sincronização parou — e o pareamento ABERTO.
    @serializa
    def grava(self) -> None:
        if not self.arquivo:
            return
        try:
            dados = {"v": 1, "aparelhos": [
                {k: v for k, v in asdict(ap).items()
                 if k not in ("pendentes", "saindo", "cursor")}
                for ap in self.por_device.values()
            ], "pareamentos": [asdict(p) for p in self.pareamentos.values()
                               if not p.expirou()],
                    "pessoas": self.pessoas,
                    "cotas": {k: asdict(c) for k, c in self.cotas.items()},
                    "sessoes": {k: asdict(v)
                                for k, v in self.sessoes.items()},
                    "operacoes": [
                        {"k": k, "em": v.get("em", 0), "r": v.get("r", {})}
                        for k, v in self._operacoes_vivas().items()]}
            # Escrita atômica, pelo mesmo motivo do cartão: desligar no
            # meio é *quando*, não *se*, e meio arquivo JSON é um backend
            # que não sobe.
            tmp = self.arquivo.with_suffix(".tmp")
            tmp.write_text(json.dumps(dados, ensure_ascii=False))
            tmp.replace(self.arquivo)
        except OSError:
            # Disco cheio ou volume somente leitura não pode derrubar o
            # servidor: o estado em memória continua certo, e é ele que
            # atende. O que se perde é o próximo boot.
            pass

    @serializa
    def _carrega(self) -> None:
        if not self.arquivo or not self.arquivo.exists():
            return
        try:
            dados = json.loads(self.arquivo.read_text())
        except (OSError, ValueError):
            return
        if dados.get("v") != 1:
            # Formato desconhecido é IGNORADO, nunca interpretado na
            # marra — a mesma regra do `meta.json` do cartão (RN-63).
            return

        antigas: list[tuple[Aparelho, dict]] = []
        for bruto in dados.get("aparelhos", []):
            # Chave que este código não conhece IGNORA o aparelho, nunca
            # derruba o servidor.
            velha = {k: bruto.pop(k) for k in
                     ("limite_s", "usados_s", "capturas", "ciclo_em")
                     if k in bruto}

            try:
                sinc = Sincronia(**bruto.pop("sinc", {}) or {})
                ap = Aparelho(**bruto)
            except TypeError:
                continue
            ap.sinc = sinc
            self.aparelhos[ap.token] = ap
            self.por_device[ap.device_id] = ap
            if velha:
                antigas.append((ap, velha))

        self.pessoas.update(dados.get("pessoas", {}))

        for chave, bruto in (dados.get("sessoes") or {}).items():
            try:
                self.sessoes[chave] = Sessao(**bruto)
            except TypeError:
                continue

        for chave, bruto in (dados.get("cotas") or {}).items():
            # O limite não mora mais aqui: vem de TINTO_CONTAS.
            bruto.pop("limite_s", None)
            try:
                self.cotas[chave] = Cota(**bruto)
            except TypeError:
                continue

        # A migração só vale para quem AINDA não tem cota de conta: um
        # arquivo já migrado tem as duas coisas por um deploy ou dois, e
        # a antiga não pode ressuscitar por cima da nova.
        for ap, velha in antigas:
            chave = self.chave_da_cota(ap)
            if chave in self.cotas and dados.get("cotas"):
                continue
            c = self.cota(ap)
            c.usados_s = max(c.usados_s, int(velha.get("usados_s", 0)))
            c.capturas = max(c.capturas, int(velha.get("capturas", 0)))
            if velha.get("ciclo_em"):
                c.ciclo_em = min(c.ciclo_em, float(velha["ciclo_em"]))

        corte = time.time() - self.OPERACOES_S
        for bruto in dados.get("operacoes", []):
            if bruto.get("em", 0) >= corte:
                self.operacoes[bruto["k"]] = {"em": bruto["em"],
                                              "r": bruto.get("r", {})}

        for bruto in dados.get("pareamentos", []):
            try:
                p = Pareamento(**bruto)
            except TypeError:
                continue
            if not p.expirou():
                self.pareamentos[p.codigo] = p


def _digere(prova: str) -> str:
    """SHA-256 do que o aparelho apresentou."""
    return hashlib.sha256(prova.encode()).hexdigest()


# Em produção o caminho vem do ambiente, e ele aponta para o
# volume — sem volume, o container reinicia e o pareamento some junto.
memoria = Memoria(config.ESTADO)
