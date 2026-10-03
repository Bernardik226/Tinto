#!/usr/bin/env python3
"""Gera os ícones do Tinto a partir dos SVGs do Phosphor (MIT)."""
import io
import math
import os
import sys

try:
    import cairosvg
except ImportError:
    sys.exit("falta cairosvg — pip install cairosvg (só pra gerar; o .c "
             "gerado vai versionado)")
from PIL import Image

AQUI   = os.path.dirname(os.path.abspath(__file__))
RAIZ   = os.path.join(AQUI, "..", "..")   # a raiz do projeto
SVG    = os.path.join(RAIZ, "firmware", "assets", "icones")
SAIDA  = os.path.join(RAIZ, "firmware", "main", "tela", "icones.c")
CABECALHO = os.path.join(RAIZ, "firmware", "main", "tela", "icones.h")

# nome no C            arquivo                    px    onde aparece
ICONES = [
        # SILHUETA CHEIA no tamanho pequeno, traço no tamanho grande. Aos 38 px
    # o traço tem espaço pra desenhar a grade do microfone; aos 15 ele vira
    # um rabisco, e o que sobra legível é só o contorno externo. Mesma
    # regra dos marcadores do calendário: abaixo de certo tamanho, forma
    # cheia é a única que sobrevive.
    ("ICO_MIC",         "microphone-fill",        15, "captura crua, linha de capturas"),

    # Minha Conta. Desenhado para este conjunto e não trazido do
    # Phosphor: a família não tem um `user` na espessura que as outras
    # linhas usam, e um ícone de outro peso ao lado de "Wi-Fi" e
    # "Volume" salta como erro de alinhamento.
    ("ICO_PESSOA",      "user-fill",              15, "Minha Conta, o dono"),
    ("ICO_CAIXA",       "square-bold",            15, "tarefa por fazer"),
    ("ICO_CAIXA_ON",    "check-square-bold",      15, "tarefa feita"),
    ("ICO_NOTA",        "note-bold",              15, "anotação"),
    ("ICO_LISTA",       "list-bullets-bold",      15, "lista"),
    ("ICO_LEITOR_TAMANHO", "leitor-tamanho",      15, "leitor · tamanho"),
    ("ICO_LEITOR_FONTE", "leitor-fonte",          15, "leitor · família"),
    ("ICO_LEITOR_ALINHA", "leitor-alinha",        15, "leitor · alinhamento"),
    ("ICO_LIVRO_PEQUENO", "livro-pequeno",        15, "gaveta · livros"),
    ("ICO_CONTINUAR_LENDO", "continuar-lendo",    24, "Acervo · retomar a leitura"),
    ("ICO_BIBLIOTECA",      "biblioteca",         24, "Acervo · a estante"),
    ("ICO_TEMPO_LEITURA",   "tempo-leitura",     18, "obra · duração estimada"),
    ("ICO_DOCUMENTO_PEQUENO", "documento-pequeno",15, "gaveta · documentos"),
    ("ICO_EVENTO",      "calendar-blank-bold",    15, "evento"),
    ("ICO_RELOGIO",     "clock-bold",             15, "hora"),
    ("ICO_RELOGIO_24",  "relogio-digital",        15, "hora em 24 h"),
    ("ICO_LIXO",        "trash-bold",             15, "apagar"),
    ("ICO_AJUSTES",     "sliders-bold",           15, "ajustes"),
    ("ICO_ENTRA",       "caret-right-bold",       13, "▶ entra no item"),
    ("ICO_RENOMEAR",    "pencil-simple-bold",     15, "renomear"),

    # barra de título — 13 px, que é o que cabe nos 23 px de altura
    # ── o leque de Wi-Fi ─────────────────────────────────────────────
    ("ICO_WIFI",        "leque:3",                13, "barra · com rede"),
    ("ICO_SEM_REDE",    "leque:3/x",              13, "barra · sem rede"),

    ("ICO_WIFI_1",      "leque:1",                15, "rede · sinal fraco"),
    ("ICO_WIFI_2",      "leque:2",                15, "rede · sinal médio"),
    ("ICO_WIFI_3",      "leque:3",                15, "rede · sinal cheio"),
    ("ICO_CADEADO",     "cadeado",                13, "rede · pede senha"),
    ("ICO_SAIR",        "sair",                   13, "desconectar a conta"),

    ("ICO_CAT_CONEXAO2","leque:3",                22, "Ajustes · Conexão"),
    ("ICO_SUBINDO",     "arrow-up-bold",          13, "barra · a fila subindo"),
    ("ICO_DESCENDO",    "arrow-down-bold",        13, "barra · o pull chegando"),
    ("ICO_SINCRONIZA",  "arrows-clockwise-bold",  13, "barra · processando"),
    ("ICO_SYNC_ERRO",   "x-bold",                 13, "barra · deu errado"),


    # O visto e o xis. Não saem de vetor: eles aparecem em Conferir e
    # Resultado como PAR — "fazer as duas coisas" contra "descartar a
    # gravação" —, e um par só funciona se os dois tiverem o mesmo peso de
    # traço e a mesma caixa. Reduzidos de famílias diferentes, um sai mais
    # gordo que o outro e a escolha deixa de parecer simétrica.
    ("ICO_VISTO",       "visto",                  15, "feito · concluído · confirmado"),
    ("ICO_X",           "xis",                    15, "descartar — o par do visto"),

    # A marca do Tinto, pequena: ela fica ao lado do nome do dono no
    # cabeçalho da Home. Mesmo desenho do cartão de Ajustes sem a
    # engrenagem — é o aparelho, e não a configuração dele.
    ("ICO_TINTO",       "tinto_icon.jpeg",        20, "a marca, no cabeçalho da Home"),

    # ── os cinco destinos de Ajustes, 26 px ──────────────────────────
    # Vinte e dois e não quinze: a raiz de Ajustes deixou de ser uma lista
    # de linhas e virou uma pilha de DESTINOS, com título em serifa e
    # descrição embaixo. Um ícone de 15 px ao lado de um título de 17
    # parece um marcador de item; nesse tamanho ele é o par visual do
    # título, que é o que a fase 3 pede — e 22 é a medida do desenho.
    ("ICO_CAT_CONTA",   "user-fill",              22, "Ajustes · Minha conta"),
    ("ICO_CAT_CONEXAO", "wifi-high-bold",         22, "Ajustes · Conexão"),
    ("ICO_CAT_TELA",    "hora-tela",              22, "Ajustes · Hora e tela"),
    ("ICO_CAT_CAMERA",  "camera-bold",            22, "Ajustes · Câmera e voz"),
    ("ICO_CAT_CARTAO",  "note-bold",              22, "Ajustes · Armazenamento e sobre"),

    # ── marcadores do calendário ─────────────────────────────────────
    # Reduzir o microfone e o calendário a 9 px dava duas manchas iguais.
    # Aqui a FORMA é a única coisa que separa os dois marcadores, então
    # eles são desenhados na grade, e não reduzidos.
    ("ICO_MARCA_TAREFA", "marca-tarefa", 6, """calendário · o dia tem TAREFA — quadrado cheio, 6×6.

As duas formas são as do Google, e não invenção nossa: bolinha é evento
e quadrado é tarefa no aplicativo que a pessoa já usa no celular. O
aparelho mostra o dia DELA, e usar um vocabulário visual próprio para
dizer a mesma coisa seria obrigá-la a aprender duas legendas."""),
    ("ICO_MARCA_EVENTO", "marca-evento", 7, """calendário · o dia tem EVENTO — bolinha cheia, 7×7.

Cheia e não vazada: a 7 px, um anel de 1 px de traço vira um borrão
cinza no e-ink e deixa de se distinguir do quadrado a um palmo de
distância. A FORMA é a única coisa que separa os dois marcadores num
painel sem cor, e ela só funciona se sobreviver ao tamanho."""),

    # ── os quatro cartões da Home, 44 px ─────────────────────────────
    # Por que 44 e não os 42 do desenho: o desenho desenha num viewBox de 44.
    # Render a 44 é escala 1:1 — coordenada inteira cai em fronteira de
    # pixel, e traço de 2 px sobre eixo inteiro cobre exatamente duas
    # colunas, sem meio-tom nenhum. A 42 tudo vira 0,9545 e o mesmo traço
    # sai com franja cinza que o limiar corta torto, um lado com 2 px e o
    # outro com 3. Os quatro cartões medem 106×136 e sobra espaço de
    # sobra; o que não sobrava era nitidez.
    ("ICO_AREA_AGENDA",  "agendaapp_icon.jpeg",    44, "cartão da Home · Agenda"),
    ("ICO_AREA_ACERVO",  "acervo_icon.png",        44, "cartão da Home · Acervo"),
    ("ICO_AREA_JOGOS",   "gamesapp_icon.jpeg",     44, """cartão da Home · Jogos

O único dos quatro que é CHEIO, e não de traço. O cavalo do desenho é um
contorno aberto de 2 px: a 44 px ele sai rabisco — a crina, o focinho e
a orelha viram três diagonais soltas que não fecham forma nenhuma.
Cheio, a silhueta lê de um palmo e sobrevive invertida. É também o que a
spec §9 já manda para as peças do tabuleiro.

O preço é peso: o cartão de Jogos fica mais escuro que os outros três.
Aceito de olho aberto — ambiguidade custa mais que peso."""),
    ("ICO_XADREZ",       "chesshorse.png",         29, "linha de Jogos · cavalo isolado"),
    # O MESMO livro, grande. Ele é o desenho do Acervo vazio, e ali não
    # divide a tela com mais nada: 44 px no meio de 240×416 lê como
    # sujeira, e era por isso que o vazio tinha um retângulo no lugar.
    ("ICO_ACERVO_GRANDE", "acervo_icon.png",       76, "o livro do Acervo vazio, 76 px"),
    ("ICO_AREA_AJUSTES", "ajustesapp_icon.jpeg",   44, """cartão da Home · O Meu Tinto

Quadrado com ponto central, como a spec §6 manda. NÃO é engrenagem, e
não reaproveita ICO_AJUSTES (sliders, 15 px), que continua servindo às
linhas de menu."""),
]

# Comentário curto ao lado do enum, no .h. Só onde o nome sozinho não
# diz a forma — o resto se lê no .c, junto dos bytes.
LEGENDA = {
    "ICO_VISTO":        "o visto — feito, concluído, confirmado",
    "ICO_X":            "o xis — descartar, o par do visto",
    "ICO_TINTO":        "a marca, no cabeçalho da Home",
    "ICO_CAT_CONTA":    "os cinco destinos de Ajustes, 26 px",
    "ICO_MARCA_TAREFA": "quadrado cheio — o dia tem tarefa",
    "ICO_MARCA_EVENTO": "bolinha cheia  — o dia tem evento",
    "ICO_AREA_AGENDA":  "cartões da Home, 44 px — calendário",
    "ICO_AREA_ACERVO":  "livro aberto",
    "ICO_ACERVO_GRANDE": "o livro do Acervo vazio, 76 px",
    "ICO_AREA_JOGOS":   "cavalo de xadrez, silhueta cheia",
    "ICO_XADREZ":       "o mesmo cavalo, rasterizado para a linha",
    "ICO_AREA_AJUSTES": "quadrado com ponto central",
}

# ── os que são desenhados à mão ──────────────────────────────────────
# Reduzir vetor não serve pra tudo. Um microfone de 15 px sai rabisco se
# for de TRAÇO (aos 38 px o traço desenha a grade da cápsula; aos 15 sobra
# só o contorno) e sai sino se for CHEIO (a cápsula engorda até encostar
# no arco, e o arco some).
DESENHADOS = {
    # Cabeçalhos do Acervo: arte nativa 24×24. Os SVGs anteriores eram
    # reduzidos de 20 para 18 px; o antialiasing virava pixels indecisos
    # que o limiar de 1 bit ora apagava, ora engrossava.
    ("continuar-lendo", 24): [
        "........................",
        "..████████..████████....",
        ".██......████......██...",
        ".██.......██.......██...",
        ".██.......██.......██...",
        ".██.......██.......██...",
        ".██.......██..██...██...",
        ".██.......██..███..██...",
        ".██.......████████████...",
        ".██.......█████████████..",
        ".██.......████████████...",
        ".██.......██..███..██...",
        ".██.......██..██...██...",
        ".██.......██.......██...",
        ".██.......██.......██...",
        ".██.......██.......██...",
        ".██......████......██...",
        "..████████..████████....",
        "....██████..██████......",
        "........................",
        "........................",
        "........................",
        "........................",
        "........................",
    ],
    ("biblioteca", 24): [
        "........................",
        "......██████............",
        "......██..██............",
        "..██████..██..██████....",
        "..██..██..██..██..██....",
        "..██..██..██..██..██....",
        "..██..██..██..██..██....",
        "..██..██..██..██████....",
        "..██..██..██..██........",
        "..██..██..██..██████....",
        "..██..██..██..██..██....",
        "..██..██..██..██..██....",
        "..██..██..██..██..██....",
        "..██..██..██..██..██....",
        "..██..██..██..██..██....",
        "..██..██..██..██..██....",
        "..██..██..██..██..██....",
        "..██████..██..██████....",
        "........................",
        "..██████████████████....",
        "..██████████████████....",
        "........................",
        "........................",
        "........................",
    ],
    # A seta que desce para a bandeja: baixar. Desenhada, e não vetorizada
    # de um arquivo, porque em 15 px o que sobra de um ícone de download é
    # a SILHUETA — haste grossa, ponta larga e a bandeja aberta em cima.
    # Vetor fino vira mancha no espalhamento do e-ink.
    ("baixar", 15): [
        ".....███.....",
        ".....███.....",
        ".....███.....",
        ".....███.....",
        ".....███.....",
        "██.......██..",
        ".██.....██...",
        "..███████....",
        "...█████.....",
        "....███......",
        ".............",
        "█...........█",
        "█...........█",
        "█████████████",
        ".............",
    ],
    ("microphone-fill", 15): [
        "...█████...",
        "..███████..",
        "..███████..",
        "..███████..",
        "..███████..",
        "..███████..",
        "..███████..",
        ".█.......█.",
        ".█.......█.",
        ".██.....██.",
        "..███████..",
        ".....█.....",
        ".....█.....",
        "...███████.",
    ],
    # O G do Google. Não sai de arquivo nenhum: o logo é DEFINIDO pelas
    # quatro cores, e em 1 bit elas somem todas — o que resta identificando a
    # marca é só a silhueta, o anel aberto no alto à direita e a barra saindo
    # pela direita.
    ("google-g", 15): [
        ".....█████.....",
        "...█████████...",
        "..███.....███..",
        ".███.......██..",
        "██.............",
        "██.............",
        "██.............",
        "██.....████████",
        "██.....████████",
        "██.....████████",
        "██...........██",
        ".███.......███.",
        "..███.....███..",
        "...█████████...",
        ".....█████.....",
    ],
    # Os marcadores do calendário. Seis e sete pixels: aqui não há vetor
    # nenhum pra reduzir, a forma É a grade.
    ("visto", 15): [
        "...............",
        "...............",
        ".............██",
        "............███",
        "...........███.",
        "..█.......███..",
        ".███.....███...",
        ".████...███....",
        "..████.███.....",
        "...██████......",
        "....████.......",
        ".....██........",
        "...............",
        "...............",
        "...............",
    ],
    ("xis", 15): [
        "...............",
        "...............",
        ".██.........██.",
        ".███.......███.",
        "..███.....███..",
        "...███...███...",
        "....███.███....",
        ".....█████.....",
        "....███.███....",
        "...███...███...",
        "..███.....███..",
        ".███.......███.",
        ".██.........██.",
        "...............",
        "...............",
    ],
    # SAIR: a porta com a seta atravessando. Desenhado na grade porque a
    # 13 px o que identifica o símbolo é o VÃO na parede direita da porta
    # — reduzido de vetor, o vão fecha e sobra um retângulo com um risco.
    ("sair", 13): [
        ".............",
        ".██████......",
        ".█....█......",
        ".█....█..█...",
        ".█....█..██..",
        ".█....█...██.",
        ".█....███████",
        ".█....█...██.",
        ".█....█..██..",
        ".█....█..█...",
        ".█....█......",
        ".██████......",
        ".............",
    ],

    # O cadeado da rede protegida. Solto do leque de propósito: fundido,
    # ele só servia num tamanho e sumia quando o sinal era fraco.
    ("cadeado", 13): [
        ".............",
        "....█████....",
        "...██...██...",
        "...██...██...",
        "...██...██...",
        ".██████████..",
        ".██████████..",
        ".███.██.███..",
        ".███.██.███..",
        ".██████████..",
        ".██████████..",
        ".............",
        ".............",
    ],
    ("marca-tarefa", 6): [
        "██████",
        "██████",
        "██████",
        "██████",
        "██████",
        "██████",
    ],
    ("marca-evento", 7): [
        "..███..",
        ".█████.",
        "███████",
        "███████",
        "███████",
        ".█████.",
        "..███..",
    ],

    # O cavalo do cartão de Jogos. Este NÃO saiu de SVG, e as tentativas
    # estão registradas aqui pra ninguém refazê-las: o contorno do desenho,
    # reduzido, dá três diagonais soltas; a silhueta em polígono, rasterizada,
    # dá um borrão em forma de barbatana, porque o que separa cavalo de
    # barbatana são quatro detalhes que só existem NA GRADE — a orelha com
    # canto reto contra a testa (linha 8 pra 9), a boca aberta de 2 px
    # (linhas 22-23), o queixo saliente logo abaixo dela, e a queda seca da
    # papada pro pescoço (linhas 26-28).
    ("cavalo-de-xadrez", 44): [
        "............................................",
        "............................................",
        "............................................",
        "......................███...................",
        "......................████..................",
        "......................█████.................",
        "......................██████................",
        "......................███████...............",
        "......................████████..............",
        "...................███████████..............",
        ".................█████████████..............",
        "................███████████████.............",
        "..............█████...█████████.............",
        ".............██████...██████████............",
        "............███████...██████████............",
        "...........██████████████████████...........",
        "..........███████████████████████...........",
        ".........████████████████████████...........",
        ".........█████████████████████████..........",
        "........██████████████████████████..........",
        ".......███████████████████████████..........",
        ".......███████████████████████████..........",
        "............██████████████████████..........",
        "............██████████████████████..........",
        "........███████████████████████████.........",
        "........███████████████████████████.........",
        "...........████████████████████████.........",
        "...............████████████████████.........",
        "..................█████████████████.........",
        "..................█████████████████.........",
        "..................█████████████████.........",
        ".................███████████████████........",
        "................████████████████████........",
        "...............█████████████████████........",
        "..............███████████████████████.......",
        ".............████████████████████████.......",
        ".....██████████████████████████████████.....",
        ".....██████████████████████████████████.....",
        ".....██████████████████████████████████.....",
        "............................................",
        "............................................",
        "............................................",
        "............................................",
        "............................................",
    ],
}


def desenhado(chave):
    """Arte ASCII → bits empacotados. Devolve (bits, largura, altura)."""
    arte = DESENHADOS[chave]
    a = len(arte)
    l = max(len(linha) for linha in arte)
    passo = (l + 7) // 8
    bits = bytearray(passo * a)
    for y, linha in enumerate(arte):
        for x, c in enumerate(linha):
            if c != ".":
                bits[y * passo + x // 8] |= 0x80 >> (x % 8)
    return bits, l, a


# Onde moram os ícones que vieram como IMAGEM, e não como vetor.
LOGOS = os.path.join(RAIZ, "docs", "logos")


def leque_wifi(px, arcos, cortado=False, S=10):
    """O leque de Wi-Fi: um QUARTO de círculo, 90° exatos."""
    from PIL import ImageDraw

    # As bordas dos anéis, em fração do lado. Medidas, não estimadas.
    FAIXAS = [(0.000, 0.153), (0.255, 0.437), (0.539, 0.721), (0.823, 1.030)]

    W  = px * S
    im = Image.new("L", (W, W), 0)
    p  = im.load()

    # O vértice fica FORA da grade por meio pixel: é o que faz o canto
    # sair reto em vez de com um degrau — o quarto de círculo tem de
    # nascer exatamente na quina.
    ox, oy = -0.5 * S, W + 0.5 * S

    # Quantas faixas acendem. O miolo conta como uma: sinal fraco ainda é
    # um aparelho, e o vértice É o aparelho.
    quantos = max(2, min(arcos + 1, len(FAIXAS)))

    for y in range(W):
        for x in range(W):
            dx, dy = x - ox, oy - y
            if dx < 0 or dy < 0:
                continue
            d = math.hypot(dx, dy) / W
            for k in range(quantos):
                a, b = FAIXAS[k]
                if a <= d <= b:
                    p[x, y] = 255
                    break

    d = ImageDraw.Draw(im)

    if cortado:
        # A barra corta o leque de canto a canto, e ela APAGA um vão claro
        # antes de marcar: sem o vão ela some dentro da tinta e o ícone
        # volta a ser o de rede presente.
        d.line([(0, W), (W, 0)], fill=0,   width=int(S * 4.2))
        d.line([(0, W), (W, 0)], fill=255, width=int(S * 2.0))

    im = im.resize((px, px), Image.BOX)
    q  = im.load()

    passo = (px + 7) // 8
    bits  = bytearray(passo * px)
    for y in range(px):
        for x in range(px):
            if q[x, y] > 118:
                bits[y * passo + x // 8] |= 0x80 >> (x % 8)
    return bits


def do_raster(arquivo, px):
    """PNG/JPEG → bits, para os ícones que chegaram desenhados."""
    img = Image.open(os.path.join(LOGOS, arquivo))

    # O alfa manda quando ele DIZ alguma coisa. Um PNG pode ter canal alfa
    # e ser opaco de ponta a ponta — o desenho está no brilho, e não no
    # recorte —, e confiar no canal ali pinta o quadro inteiro de tinta.
    # Foi o que aconteceu com o livro do Acervo: saiu um retângulo preto.
    alfa = None
    if img.mode in ("RGBA", "LA") or "transparency" in img.info:
        a = img.convert("RGBA").split()[-1]
        if a.getextrema()[0] < 250:          # tem transparência de verdade
            alfa = a

    if alfa is not None:
        tinta = alfa.point(lambda v: 255 if v > 96 else 0)
    else:
        tinta = img.convert("L").point(lambda v: 255 if v < 128 else 0)

    caixa = tinta.getbbox()
    if caixa:
        tinta = tinta.crop(caixa)

    # QUADRADO antes de reduzir: esticar um desenho alto para um quadrado
    # de 44 deforma a silhueta, e silhueta deformada é o que faz um cavalo
    # virar barbatana.
    lado = max(tinta.size)
    fundo = Image.new("L", (lado, lado), 0)
    fundo.paste(tinta, ((lado - tinta.width) // 2, (lado - tinta.height) // 2))

    tinta = fundo.resize((px, px), Image.LANCZOS)

    passo = (px + 7) // 8
    bits = bytearray(passo * px)
    for y in range(px):
        for x in range(px):
            if tinta.getpixel((x, y)) > 110:
                bits[y * passo + x // 8] |= 0x80 >> (x % 8)
    return bits


def rasteriza(nome, px, girar=False):
    """SVG → bits empacotados, 1 bit por pixel, MSB primeiro."""
    caminho = os.path.join(SVG, nome + ".svg")
    png = cairosvg.svg2png(url=caminho, output_width=px, output_height=px)
    img = Image.open(io.BytesIO(png))

    # O que decide se o pixel tem tinta é o ALFA, não a cor: o SVG desenha
    # em preto sobre transparente, e converter para L primeiro faria o
    # fundo transparente virar preto.
    alfa = img.split()[-1]
    if girar:
        alfa = alfa.rotate(90, expand=True)
    passo = (px + 7) // 8
    bits = bytearray(passo * px)
    for y in range(px):
        for x in range(px):
            if alfa.getpixel((x, y)) > 96:
                bits[y * passo + x // 8] |= 0x80 >> (x % 8)
    return bits


def gera():
    linhas = []
    w = linhas.append
    w("// GERADO por firmware/ferramentas/icones.py — não edite à mão.")
    w("//")
    w("// Phosphor (MIT), rasterizados em 1 bit. Os SVGs de")
    w("// origem estão em firmware/assets/icones/, com a licença. Os quatro")
    w("// ICO_AREA_* da Home e os marcadores do calendário são nossos.")
    w('#include "icones.h"')
    w("")

    tabela = []
    total = 0
    for entrada in ICONES:
        cnome, arquivo, px, onde = entrada[:4]
        if (arquivo, px) in DESENHADOS:
            bits, larg, alt = desenhado((arquivo, px))
        elif arquivo.startswith("leque:"):
            spec   = arquivo.split(":", 1)[1]
            arcos  = int(spec[0])
            bits   = leque_wifi(px, arcos, cortado="/x" in spec)
            larg = alt = px
        elif arquivo.lower().endswith((".png", ".jpg", ".jpeg")):
            bits = do_raster(arquivo, px)
            larg = alt = px
        else:
            bits = rasteriza(arquivo, px, len(entrada) > 4 and entrada[4])
            larg = alt = px
        var = cnome.lower()
        # O "onde" pode ter várias linhas: o motivo de uma forma mora ao
        # lado dos bytes dela, não num documento que ninguém abre junto.
        for linha in onde.split("\n"):
            w(("// " + linha).rstrip())
        w(f"static const uint8_t {var}_bits[] = {{")
        for i in range(0, len(bits), 12):
            w("    " + ", ".join(f"0x{b:02X}" for b in bits[i:i + 12]) + ",")
        w("};")
        tabela.append((cnome, var, larg, alt))
        total += len(bits)
        w("")

    w("const icone_t ICONES[ICO_QUANTOS] = {")
    for cnome, var, larg, alt in tabela:
        w(f"    [{cnome}] = {{ {larg}, {alt}, {var}_bits }},")
    w("};")
    w("")
    # A função de desenho sai daqui junto com os dados, e não à mão num
    # arquivo ao lado: escrita à mão, ela some no dia em que alguém regera
    # os ícones — que foi exatamente o que aconteceu na primeira vez.
    w("int gfx_icone(bitmap_t *bm, int x, int y, icone_id id)")
    w("{")
    w("    if (id < 0 || id >= ICO_QUANTOS) return x;")
    w("    const icone_t *i = &ICONES[id];")
    w("    int passo = (i->l + 7) / 8;")
    w("")
    w("    for (int ly = 0; ly < i->a; ly++)")
    w("        for (int lx = 0; lx < i->l; lx++)")
    w("            if (i->bits[ly * passo + lx / 8] & (0x80 >> (lx % 8)))")
    w("                gfx_pixel(bm, x + lx, y + ly, true);")
    w("")
    w("    return x + i->l;")
    w("}")

    with open(SAIDA, "w") as f:
        f.write("\n".join(linhas) + "\n")

    with open(CABECALHO, "w") as f:
        f.write("""// tela/icones.h — o vocabulário de ícones. UI.md nível 1, junto das fontes.
//
// Mora em tela/ e não em ui/ porque ícone é BITMAP: mesma natureza de um
// glifo, mesma camada. E porque vista/ precisa dizer QUAL ícone uma linha
// leva — se o tipo morasse em ui/, vista/ dependeria da camada de cima.
//
// GERADO em parte por firmware/ferramentas/icones.py, a partir dos SVGs do Phosphor
// (MIT) rasterizados em 1 bit no tamanho de uso.
//
// Dois vocabulários, e eles não se misturam: os de LINHA (13/15 px) marcam
// um item de lista; os ICO_AREA_ (44 px) são os cartões da Home. Usar o
// ICO_EVENTO de 15 px num cartão de 106×136 deixa o cartão vazio.
#ifndef TELA_ICONES_H
#define TELA_ICONES_H

#include "bitmap.h"

typedef enum {
    ICO_NENHUM = 0,   // a linha não leva ícone
""" + "\n".join(
    (f"    {c},".ljust(24) + f"// {LEGENDA[c]}") if c in LEGENDA
    else f"    {c},"
    for c in [e[0] for e in ICONES]) + """
    ICO_QUANTOS
} icone_id;

typedef struct {
    int8_t l, a;
    const uint8_t *bits;
} icone_t;

extern const icone_t ICONES[ICO_QUANTOS];

// Pinta o ícone com o canto superior esquerdo em (x, y). Devolve o x de
// depois dele, pra encadear como o texto encadeia.
int gfx_icone(bitmap_t *bm, int x, int y, icone_id id);

#endif
""")
    print(f"  {SAIDA}")
    # Soma os bits que REALMENTE saíram, em vez de rasterizar tudo de novo
    # pra contar: a segunda passada não consultava DESENHADOS, então
    # qualquer ícone que só existisse na grade — sem SVG por trás — fazia o
    # gerador estourar DEPOIS de já ter escrito o .c certo.
    print(f"    {len(ICONES)} ícones · {total:,} bytes")


if __name__ == "__main__":
    gera()
