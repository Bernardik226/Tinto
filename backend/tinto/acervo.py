"""O Acervo da pessoa: o catálogo online, e de mais ninguém.

**O Acervo é da CONTA**: todos os Tintos vinculados veem o mesmo
catálogo, e cada aparelho decide o que baixa. Separado da Agenda: obra
não é evento. Toda operação recebe `pessoa`, e a obra de outra conta
responde como se não existisse.
"""

from __future__ import annotations

import hashlib
import json
import secrets
import threading
import time
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path

from . import config
from .memoria import serializa

# Doze por lote: o servidor fatia, o PWA só pede o próximo.
LOTE = 12


class ObraTipo(str, Enum):
    """Livro ou documento: a diferença é do FILTRO da tela, e a pessoa corrige
    o palpite no envio.
    """

    LIVRO = "livro"
    DOCUMENTO = "documento"


class ConversaoEstado(str, Enum):
    """Os cinco estados do envio, e nenhum outro (cada um tem uma cara na tela).

    `RECUSADO` não é `FALHOU`: recusado é o arquivo que não serve (PDF só
    imagem, formato não aceito) e a pessoa manda outro; falhou é nosso.
    """

    RECEBIDO = "recebido"
    CONVERTENDO = "convertendo"
    PRONTO = "pronto"
    RECUSADO = "recusado"
    FALHOU = "falhou"


@dataclass
class Obra:
    """Uma obra do catálogo de alguém."""

    id: str
    pessoa: str
    titulo: str
    tipo: ObraTipo = ObraTipo.LIVRO
    estado: ConversaoEstado = ConversaoEstado.RECEBIDO

    autor: str = ""
    sinopse: str = ""
    capa: str = ""
    amostra: str = ""
    caracteres: int = 0
    palavras: int = 0
    minutos_leitura: int = 0
    resumo_hash: str = ""
    motivo: str = ""          # por que foi recusada, quando foi
    # Upload e conversão criam um rascunho; só a confirmação da pessoa torna a
    # obra visível no PWA e nos Tintos.
    publicada: bool = True

    criada_em: float = field(default_factory=time.time)

    # Em quais aparelhos ela já está BAIXADA. Informação, não comando: o PWA
    # mostra "No dispositivo", e quem baixa é o aparelho.
    dispositivos: list[str] = field(default_factory=list)

    def no_dispositivo(self, device_id: str) -> bool:
        return device_id in self.dispositivos


@dataclass
class Pagina:
    """Um lote do catálogo, e o cursor para pedir o próximo.

    O cursor é OPACO e aponta para uma OBRA, não uma posição: um índice
    pularia uma obra quando outra é removida entre dois lotes.
    """

    obras: list[Obra]
    cursor: str = ""


class Acervo:
    """As obras de todo mundo, cada uma com o dono junto."""

    def __init__(self) -> None:
        self._trava = threading.RLock()
        self._obras: dict[str, Obra] = {}
        self._textos: dict[str, str] = {}
        self._capas: dict[tuple[str, str], bytes] = {}
        self.arquivo: Path | None = None

    # ── escrever ─────────────────────────────────────────────────────
    @serializa
    def cria_obra(self, pessoa: str, titulo: str,
                  tipo: ObraTipo = ObraTipo.LIVRO, **campos) -> Obra:
        obra = Obra(id=f"ob:{secrets.token_urlsafe(9)}", pessoa=pessoa,
                    titulo=titulo, tipo=tipo, **campos)
        self._obras[obra.id] = obra
        return obra

    @serializa
    def atualiza_obra(self, pessoa: str, obra_id: str, **campos) -> bool:
        """Muda o que foi pedido. Devolve se a obra era mesmo dela."""
        obra = self.obra_de(pessoa, obra_id)
        if not obra:
            return False
        for chave, valor in campos.items():
            if hasattr(obra, chave):
                setattr(obra, chave, valor)
        return True

    @serializa
    def remove_obra(self, pessoa: str, obra_id: str) -> bool:
        """Tira do CATÁLOGO, e nada mais: quem já baixou continua lendo. Nenhum
        comando sai daqui para aparelho nenhum.
        """
        if not self.obra_de(pessoa, obra_id):
            return False
        del self._obras[obra_id]
        self._textos.pop(obra_id, None)
        for chave in [k for k in self._capas if k[0] == obra_id]:
            self._capas.pop(chave, None)
        if self.arquivo:
            pasta = self.arquivo.with_suffix("") / "obras"
            for tipo in ("web", "mini", "destaque", "grande"):
                try:
                    (pasta / f"{obra_id}.capa-{tipo}").unlink()
                except OSError:
                    pass
        return True

    @serializa
    def marca_no_dispositivo(self, pessoa: str, obra_id: str,
                             device_id: str, tem: bool) -> bool:
        obra = self.obra_de(pessoa, obra_id)
        if not obra:
            return False
        if tem and device_id not in obra.dispositivos:
            obra.dispositivos.append(device_id)
        elif not tem and device_id in obra.dispositivos:
            obra.dispositivos.remove(device_id)
        return True

    # ── o TEXTO da obra ─────────────────────────────────────────────
    # FORA do catálogo: um livro tem megabytes, e cada gravação do catálogo
    # reescreveria a biblioteca inteira.
    @serializa
    def guarda_texto(self, pessoa: str, obra_id: str, texto: str) -> bool:
        obra = self.obra_de(pessoa, obra_id)
        if not obra:
            return False

        self._textos[obra_id] = texto
        obra.caracteres = len(texto)
        obra.resumo_hash = hashlib.sha256(texto.encode("utf-8")).hexdigest()

        if self.arquivo:
            try:
                pasta = self.arquivo.with_suffix("") / "obras"
                pasta.mkdir(parents=True, exist_ok=True)
                (pasta / f"{obra_id}.txt").write_text(texto, encoding="utf-8")
            except OSError:
                pass
        return True

    @serializa
    def texto_de(self, pessoa: str, obra_id: str) -> str | None:
        if not self.obra_de(pessoa, obra_id):
            return None
        if obra_id in self._textos:
            return self._textos[obra_id]
        if self.arquivo:
            try:
                caminho = self.arquivo.with_suffix("") / "obras" / f"{obra_id}.txt"
                return caminho.read_text(encoding="utf-8")
            except OSError:
                return None
        return None

    @serializa
    def guarda_capa(self, pessoa: str, obra_id: str, dados: bytes,
                    tipo: str) -> bool:
        obra = self.obra_de(pessoa, obra_id)
        if not obra or tipo not in ("web", "mini", "destaque", "grande") or not dados:
            return False
        self._capas[(obra_id, tipo)] = bytes(dados)
        obra.capa = "sim"
        if self.arquivo:
            try:
                pasta = self.arquivo.with_suffix("") / "obras"
                pasta.mkdir(parents=True, exist_ok=True)
                (pasta / f"{obra_id}.capa-{tipo}").write_bytes(dados)
            except OSError:
                pass
        return True

    @serializa
    def capa_de(self, pessoa: str, obra_id: str, tipo: str) -> bytes | None:
        if (not self.obra_de(pessoa, obra_id) or
                tipo not in ("web", "mini", "destaque", "grande")):
            return None
        chave = (obra_id, tipo)
        if chave in self._capas:
            return self._capas[chave]
        if self.arquivo:
            try:
                dados = (self.arquivo.with_suffix("") / "obras" /
                         f"{obra_id}.capa-{tipo}").read_bytes()
                self._capas[chave] = dados
                return dados
            except OSError:
                pass
        return None

    @serializa
    def obra_com_hash(self, pessoa: str, resumo_hash: str) -> Obra | None:
        """A obra que já tem este conteúdo, se houver: o envio é idempotente (dois
        toques ou a conexão caindo não fazem duas cópias).
        """
        for obra in self._obras.values():
            if obra.pessoa == pessoa and obra.resumo_hash == resumo_hash:
                return obra
        return None

    # ── ler ──────────────────────────────────────────────────────────
    @serializa
    def obra_de(self, pessoa: str, obra_id: str) -> Obra | None:
        obra = self._obras.get(obra_id)
        return obra if obra and obra.pessoa == pessoa else None

    @serializa
    def lista_obras(self, pessoa: str, tipo: ObraTipo | None = None,
                    cursor: str = "") -> Pagina:
        """O catálogo dela, em lotes de doze. O FILTRO vem antes de fatiar:
        filtrar dentro do lote esconderia um livro que está lá.
        """
        minhas = [o for o in self._obras.values()
                  if o.pessoa == pessoa and o.publicada]
        if tipo is not None:
            minhas = [o for o in minhas if o.tipo == tipo]

        # A mais nova primeiro: é a que a pessoa acabou de mandar.
        minhas.sort(key=lambda o: (-o.criada_em, o.id))

        comeco = 0
        if cursor:
            for i, obra in enumerate(minhas):
                if obra.id == cursor:
                    comeco = i
                    break

        pedaco = minhas[comeco:comeco + LOTE]
        resto = minhas[comeco + LOTE:]
        return Pagina(obras=pedaco, cursor=resto[0].id if resto else "")

    # ── o instantâneo em disco ───────────────────────────────────────
    @serializa
    def grava(self) -> None:
        if not self.arquivo:
            return
        try:
            dados = {"v": 1, "obras": [
                {**vars(o), "tipo": o.tipo.value, "estado": o.estado.value}
                for o in self._obras.values()]}
            tmp = self.arquivo.with_suffix(".tmp")
            tmp.write_text(json.dumps(dados, ensure_ascii=False))
            tmp.replace(self.arquivo)
        except OSError:
            # Disco cheio não derruba o servidor; o que se perde é o próximo boot.
            pass

    @serializa
    def carrega(self) -> None:
        if not self.arquivo or not self.arquivo.exists():
            return
        try:
            dados = json.loads(self.arquivo.read_text())
        except (OSError, ValueError):
            return
        if dados.get("v") != 1:
            return

        for bruto in dados.get("obras", []):
            try:
                bruto["tipo"] = ObraTipo(bruto.get("tipo", "livro"))
                bruto["estado"] = ConversaoEstado(
                    bruto.get("estado", "recebido"))
                obra = Obra(**bruto)
            except (TypeError, ValueError):
                # Chave desconhecida ignora a OBRA, nunca derruba o servidor (como o
                # `meta.json` do cartão, RN-63).
                continue
            self._obras[obra.id] = obra

    @serializa
    def esquece_tudo(self) -> None:
        self._obras.clear()
        self._textos.clear()
        self._capas.clear()


acervo = Acervo()
if config.estado_ao_lado("acervo"):
    acervo.arquivo = Path(config.estado_ao_lado("acervo"))
    acervo.carrega()
