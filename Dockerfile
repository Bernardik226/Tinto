# O backend e o aplicativo, do repositório inteiro.
#
# ── o contexto de build é a RAIZ, e isto não é detalhe ───────────────
#
# O PWA mora em `pwa/`, fora de `backend/`, e um contexto que começa em
# `backend/` não alcança um irmão. Havia um segundo Dockerfile lá dentro
# que compilava sem erro e subia um servidor SEM o aplicativo: `/e`
# respondia "o aplicativo não foi empacotado", e nada no build avisava.
#
# Quem hospeda: aponte o serviço para a raiz do repositório, não para
# `backend/`. É este arquivo que ele deve usar.
FROM python:3.12-slim

WORKDIR /app

# As dependências antes do código: elas mudam raramente, e esta camada
# fica em cache entre deploys.
COPY backend/requisitos.txt .
RUN pip install --no-cache-dir -r requisitos.txt

COPY backend/tinto/ ./tinto/
COPY pwa/ ./pwa/

# Nenhuma chave aqui dentro. Elas moram nas variáveis de ambiente da
# hospedagem, e o `config.py` é escrito para isso: nenhuma tem valor padrão
# que funcione, porque chave com padrão silencioso é a que vai pra
# produção sem ninguém perceber.
#
# `/saude` diz quais faltam, e cada rota devolve 503 nomeando a sua.
ENV PORT=8000
EXPOSE 8000

# `$PORT` porque a hospedagem escolhe a porta e a injeta. Fixar 8000 aqui
# faria o container subir e o roteador nunca achar ninguém escutando.
# UM WORKER. Não é omissão: o estado é um dicionário em memória
# espelhado no volume, e dois processos são dois estados — o pareamento
# nasce num, a confirmação chega no outro, e quem gravar por último apaga
# o outro. `config.confere_um_processo` recusa a partida se alguém puser
# `WEB_CONCURRENCY`, para que isso falhe alto em vez de virar conta que
# some. Para aguentar mais gente: máquina maior, ou Postgres.
CMD uvicorn tinto.app:app --host 0.0.0.0 --port ${PORT} --workers 1
