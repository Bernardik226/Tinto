"""O Acervo pelas duas portas: a da pessoa (PWA, sessão) e a do aparelho
(token). As duas resolvem a mesma pessoa: o Acervo é da CONTA.

Não existe comando de download: o PWA mostra "No dispositivo", e quem
baixa é o aparelho.
"""

from __future__ import annotations

import io
from datetime import datetime, timezone

from fastapi import APIRouter, Depends, HTTPException, Request, UploadFile
from fastapi.responses import JSONResponse, Response
from pydantic import BaseModel

from . import conversao
from .acervo import ConversaoEstado, ObraTipo, acervo
from .memoria import Aparelho
from .pwa import quem_e

rotas_acervo = APIRouter()

# A ROTA precisa parar de ler: conferir só em `inspeciona_upload` seria
# tarde, com o arquivo inteiro já na memória.
ENTRADA_MAX = conversao.ENTRADA_MAX
LEITURA_BLOCO = 64 * 1024


async def _le_upload(arquivo: UploadFile) -> bytes | None:
    """Lê até o teto e um byte; `None` significa que passou dele."""
    partes: list[bytes] = []
    total = 0
    while True:
        bloco = await arquivo.read(min(LEITURA_BLOCO,
                                       ENTRADA_MAX - total + 1))
        if not bloco:
            return b"".join(partes)
        partes.append(bloco)
        total += len(bloco)
        if total > ENTRADA_MAX:
            return None


def exige_pessoa(request: Request) -> str:
    """Sem login não há catálogo, nem vazio: lista vazia ensinaria que a conta
    existe.
    """
    pessoa = quem_e(request)
    if not pessoa:
        raise HTTPException(401, "entre com a sua conta")
    return pessoa


def _completa_metadados(obra):
    if not obra.palavras:
        texto = acervo.texto_de(obra.pessoa, obra.id) or ""
        if texto:
            dados = conversao.normaliza_obra(texto, {
                "titulo": obra.titulo, "autor": obra.autor,
                "descricao": obra.sinopse})
            obra.sinopse = dados.sinopse
            obra.palavras = dados.palavras
            obra.minutos_leitura = dados.minutos_leitura
    return obra


def _publica(obra) -> dict:
    """A obra como a tela a lê. Nunca o caminho do arquivo."""
    obra = _completa_metadados(obra)
    return {
        "id": obra.id,
        "titulo": obra.titulo,
        "autor": obra.autor,
        "sinopse": obra.sinopse,
        "tipo": obra.tipo.value,
        "estado": obra.estado.value,
        "capa": obra.capa,
        "amostra": obra.amostra,
        "caracteres": obra.caracteres,
        "palavras": obra.palavras,
        "minutos_leitura": obra.minutos_leitura,
        "criada_em": obra.criada_em,
        "motivo": obra.motivo,
        "dispositivos": list(obra.dispositivos),
        "publicada": obra.publicada,
    }


# ── a porta da PESSOA ───────────────────────────────────────────────
@rotas_acervo.get("/e/api/acervo")
def catalogo(cursor: str = "", filtro: str = "",
             pessoa: str = Depends(exige_pessoa)):
    tipo = None
    if filtro in (ObraTipo.LIVRO.value, ObraTipo.DOCUMENTO.value):
        tipo = ObraTipo(filtro)

    pagina = acervo.lista_obras(pessoa, tipo=tipo, cursor=cursor)
    return {"obras": [_publica(o) for o in pagina.obras],
            "cursor": pagina.cursor}


@rotas_acervo.get("/e/api/acervo/{obra_id}")
def uma_obra(obra_id: str, pessoa: str = Depends(exige_pessoa)):
    obra = acervo.obra_de(pessoa, obra_id)
    if not obra:
        raise HTTPException(404, "não achei esta obra")
    return _publica(obra)


@rotas_acervo.get("/e/api/acervo/{obra_id}/capa")
def capa_web(obra_id: str, pessoa: str = Depends(exige_pessoa)):
    dados = acervo.capa_de(pessoa, obra_id, "web")
    if dados is None:
        raise HTTPException(404, "esta obra não tem capa")
    return Response(content=dados, media_type="image/png",
                    headers={"cache-control": "private, max-age=86400"})


@rotas_acervo.post("/e/api/acervo")
async def envia(arquivo: UploadFile, pessoa: str = Depends(exige_pessoa)):
    """O envio inteiro numa chamada: inspeciona, converte e cataloga. Sem fila:
    extrair texto de um EPUB de 3 MB leva milissegundos.
    """
    bruto = await _le_upload(arquivo)
    if bruto is None:
        return JSONResponse(
            status_code=413,
            content={"recusa": conversao.Recusa.TAMANHO.value,
                     "motivo": "O arquivo é grande demais para o Acervo."})

    achado = conversao.inspeciona_upload(io.BytesIO(bruto),
                                         arquivo.filename or "",
                                         arquivo.content_type or "")
    if achado.recusa:
        return JSONResponse(status_code=415,
                            content={"recusa": achado.recusa.value,
                                     "motivo": achado.motivo})

    # Temporário com nome NOSSO: nome escolhido de fora que vira caminho local
    # escreve fora de onde devia.
    import tempfile

    with tempfile.NamedTemporaryFile(suffix=f".{achado.formato}",
                                     delete=False) as tmp:
        tmp.write(bruto)
        caminho = tmp.name

    try:
        extraido = conversao.extrai_texto(caminho, achado.formato)
    finally:
        import os as _os

        try:
            _os.unlink(caminho)
        except OSError:
            pass

    if extraido.recusa:
        return JSONResponse(status_code=415,
                            content={"recusa": extraido.recusa.value,
                                     "motivo": extraido.motivo})

    dados = conversao.normaliza_obra(
        extraido.texto,
        {"titulo": extraido.titulo, "autor": extraido.autor,
         "descricao": extraido.descricao,
         "arquivo": arquivo.filename or ""})

    # Repetir o envio reaproveita o rascunho (ou a obra publicada).
    ja = acervo.obra_com_hash(pessoa, dados.resumo_hash)
    if ja:
        return _publica(ja)

    obra = acervo.cria_obra(
        pessoa, titulo=dados.titulo, autor=dados.autor,
        sinopse=dados.sinopse, palavras=dados.palavras,
        minutos_leitura=dados.minutos_leitura,
        tipo=ObraTipo.LIVRO if achado.formato == "epub" else ObraTipo.DOCUMENTO,
        estado=ConversaoEstado.PRONTO, amostra=dados.amostra,
        publicada=False)
    acervo.guarda_texto(pessoa, obra.id,
                        conversao.normaliza_texto_editorial(extraido.texto))
    if extraido.capa:
        acervo.guarda_capa(pessoa, obra.id,
                           conversao.prepara_capa_web(extraido.capa), "web")
        acervo.guarda_capa(pessoa, obra.id,
                           conversao.prepara_capa_mini(extraido.capa, 34, 49), "mini")
        acervo.guarda_capa(pessoa, obra.id,
                           conversao.prepara_capa_mini(extraido.capa, 42, 63),
                           "destaque")
        acervo.guarda_capa(pessoa, obra.id,
                           conversao.prepara_capa(extraido.capa, 240, 360), "grande")
    acervo.grava()
    return _publica(obra)


@rotas_acervo.post("/e/api/acervo/{obra_id}/confirmar")
def confirma(obra_id: str, pessoa: str = Depends(exige_pessoa)):
    """Publica o rascunho somente após o último aceite do stepper."""
    obra = acervo.obra_de(pessoa, obra_id)
    if not obra:
        raise HTTPException(404, "não achei esta obra")
    if obra.estado != ConversaoEstado.PRONTO:
        raise HTTPException(409, "esta obra ainda não está pronta")
    acervo.atualiza_obra(pessoa, obra_id, publicada=True)
    acervo.grava()
    return _publica(acervo.obra_de(pessoa, obra_id))


class Presenca(BaseModel):
    """O aparelho dizendo se tem a cópia. No escopo do módulo: classe definida
    a cada chamada é um tipo novo que o FastAPI não resolve.
    """

    tem: bool = True


class Retoque(BaseModel):
    """O que a pessoa pode corrigir no quarto passo do envio."""

    titulo: str | None = None
    autor: str | None = None
    tipo: str | None = None


@rotas_acervo.patch("/e/api/acervo/{obra_id}")
def corrige(obra_id: str, mudanca: Retoque,
            pessoa: str = Depends(exige_pessoa)):
    campos = {}
    if mudanca.titulo is not None:
        campos["titulo"] = mudanca.titulo[:120]
    if mudanca.autor is not None:
        campos["autor"] = mudanca.autor[:80]
    if mudanca.tipo in (ObraTipo.LIVRO.value, ObraTipo.DOCUMENTO.value):
        campos["tipo"] = ObraTipo(mudanca.tipo)

    if not acervo.atualiza_obra(pessoa, obra_id, **campos):
        raise HTTPException(404, "não achei esta obra")
    acervo.grava()
    return _publica(acervo.obra_de(pessoa, obra_id))


@rotas_acervo.delete("/e/api/acervo/{obra_id}")
def remove(obra_id: str, pessoa: str = Depends(exige_pessoa)):
    """Tira do catálogo online; as cópias baixadas continuam (a frase da tela é
    literal: nenhum comando sai daqui).
    """
    if not acervo.remove_obra(pessoa, obra_id):
        raise HTTPException(404, "não achei esta obra")
    acervo.grava()
    return {"ok": True,
            "aviso": "Cópias já baixadas nos seus Tintos continuarão "
                     "disponíveis."}


# ── a porta do APARELHO ─────────────────────────────────────────────
def _registra_rotas_do_device(app, exige_conta):
    """As rotas do Tinto, com a dependência de token do `app`. Aqui, junto das
    da pessoa, para a regra de posse ficar num arquivo só.
    """

    @app.get("/v1/acervo")
    def acervo_do_aparelho(cursor: str = "", filtro: str = "",
                           ap: Aparelho = Depends(exige_conta)):
        tipo = None
        if filtro in (ObraTipo.LIVRO.value, ObraTipo.DOCUMENTO.value):
            tipo = ObraTipo(filtro)

        pagina = acervo.lista_obras(ap.pessoa, tipo=tipo, cursor=cursor)
        def bytes_da_obra(o):
            texto = acervo.texto_de(ap.pessoa, o.id)
            return len(texto.encode("utf-8")) if texto is not None else 0

        for o in pagina.obras:
            _completa_metadados(o)
        return {"obras": [{"id": o.id, "t": o.titulo, "a": o.autor,
                           "tp": o.tipo.value, "n": bytes_da_obra(o),
                           "s": o.sinopse[:240], "pl": o.palavras,
                           "ml": o.minutos_leitura,
                           "cr": datetime.fromtimestamp(
                               o.criada_em, timezone.utc).date().isoformat(),
                           "c": bool(o.capa),
                           "aqui": o.no_dispositivo(ap.device_id)}
                          for o in pagina.obras],
                "cursor": pagina.cursor}

    @app.get("/v1/acervo/{obra_id}/capa/{tamanho}")
    def capa_do_aparelho(obra_id: str, tamanho: str, offset: int | None = None,
                         ap: Aparelho = Depends(exige_conta)):
        if tamanho not in ("mini", "destaque", "grande"):
            raise HTTPException(404, "tamanho de capa desconhecido")

        # `destaque` serve também de versão das miniaturas: obras antigas são
        # refeitas do PNG original na primeira leitura.
        if tamanho in ("mini", "destaque") and not acervo.capa_de(
                ap.pessoa, obra_id, "destaque"):
            original = acervo.capa_de(ap.pessoa, obra_id, "web")
            if original:
                acervo.guarda_capa(ap.pessoa, obra_id,
                    conversao.prepara_capa_mini(original, 34, 49), "mini")
                acervo.guarda_capa(ap.pessoa, obra_id,
                    conversao.prepara_capa_mini(original, 42, 63), "destaque")
        dados = acervo.capa_de(ap.pessoa, obra_id, tamanho)
        if dados is None:
            raise HTTPException(404, "esta obra não tem capa")
        # Capas anteriores à folha 120×180 não têm resolução para o leitor:
        # recompõe do PNG original, sem reenvio.
        esperados = {"grande": 10800, "mini": 245, "destaque": 378}
        esperado = esperados[tamanho]
        if len(dados) != esperado:
            original = acervo.capa_de(ap.pessoa, obra_id, "web")
            if original:
                largura, altura = {"grande": (240, 360),
                                   "mini": (34, 49),
                                   "destaque": (42, 63)}[tamanho]
                novo = (conversao.prepara_capa(original, largura, altura)
                        if tamanho == "grande" else
                        conversao.prepara_capa_mini(original, largura, altura))
                if novo:
                    acervo.guarda_capa(ap.pessoa, obra_id, novo, tamanho)
                    dados = novo
        # O HAL recebe corpos terminados em NUL, e capa tem bytes zero: atravessa
        # em hexadecimal e vira bitmap só ao desenhar.
        if offset is not None:
            if offset < 0 or offset > len(dados):
                raise HTTPException(416, "posição fora da capa")
            dados = dados[offset:offset + 7000]
        return Response(content=dados.hex(), media_type="text/plain",
                        headers={"cache-control": "private, max-age=86400"})

    @app.get("/v1/acervo/{obra_id}/conteudo")
    @app.get("/v1/acervo/{obra_id}/conteudo/bloco")
    def conteudo(obra_id: str, offset: int | None = None,
                 ap: Aparelho = Depends(exige_conta)):
        """O texto, com TAMANHO e HASH: cópia incompleta que abre é pior que cópia
        que não abre.
        """
        obra = acervo.obra_de(ap.pessoa, obra_id)
        texto = acervo.texto_de(ap.pessoa, obra_id) if obra else None
        if obra is None or texto is None:
            raise HTTPException(404, "não achei esta obra")

        corpo = texto.encode("utf-8")
        if offset is None:
            trecho = corpo                 # compatibilidade com clientes antigos
            proximo = len(corpo)
        else:
            if offset < 0 or offset > len(corpo):
                raise HTTPException(416, "posição fora da obra")
            # 128 KiB por pedido: TLS por 768 bytes eram centenas de handshakes no
            # ESP32 para um livro pequeno.
            trecho = corpo[offset:offset + 128 * 1024]
            proximo = offset + len(trecho)
        return Response(content=trecho, media_type="text/plain; charset=utf-8",
                        headers={"x-tinto-hash": obra.resumo_hash,
                                 "x-tinto-tamanho": str(len(corpo)),
                                 "x-tinto-proximo": str(proximo)})

    @app.post("/v1/acervo/{obra_id}/presenca")
    def presenca(obra_id: str, corpo: Presenca,
                 ap: Aparelho = Depends(exige_conta)):
        """O aparelho anuncia que baixou ou apagou: "No dispositivo" no PWA é o eco
        disto, e não um palpite do servidor.
        """
        if not acervo.marca_no_dispositivo(ap.pessoa, obra_id, ap.device_id,
                                           corpo.tem):
            raise HTTPException(404, "não achei esta obra")
        acervo.grava()
        return {"ok": True}
