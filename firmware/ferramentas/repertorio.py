"""O repertório de caracteres das fontes do Tinto. SEM DEPENDÊNCIAS."""

# Acentos e sinais que o português exige, mais os travessões do sistema.
ACENTOS = "áàâãéêíóôõúûüçÁÀÂÃÉÊÍÓÔÕÚÛÜÇºª°·—–"

# O que o TEXTO DE CONTEÚDO traz e o do sistema nunca pediu: a transcrição
# vem do Whisper e o resumo da LLM, e os dois escrevem reticências de um
# caractere só e aspas curvas.
CONTEUDO = "…“”‘’"
LATIM_OCIDENTAL = "äåæèëìîïñòöùÿÄÅÆÈËÌÎÏÑÒÖÙŸœŒøØß¿¡"
SIMBOLOS = "€£¥©®™§±×÷•"

# As setas do rodapé são TEXTO, não ícone: aparecem dentro de frases como
# "◀ calendário" e precisam do mesmo avanço e linha de base das letras.
SETAS = "◀▶▲▼"

# As marcas do calendário aparecem DUAS vezes: desenhadas na grade, e
# escritas na legenda do rodapé. Se a legenda usasse outra forma, ela
# ensinaria um símbolo diferente do que está na tela — que é pior que não
# ter legenda.
MARCAS = "◦▪"

CORPO = ("".join(chr(c) for c in range(32, 127))
         + ACENTOS + LATIM_OCIDENTAL + SIMBOLOS + SETAS + MARCAS + CONTEUDO)
