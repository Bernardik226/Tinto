"""Os cenários do Tinto — um dia inteiro, montado sem recompilar.

Uma fonte só para a prova (PNG) e a janela (navegável).
"""
import ctypes
import os
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.join(AQUI, "..", "..")   # a raiz do projeto
LIB = os.path.join(RAIZ, "build", "pc", "bin", "libtinto.so")

# tipo_t do nucleo/tipos.h
NADA, ANOTACAO, TAREFA, LISTA, EVENTO = 0, 1, 2, 3, 4
SEM_DATA = (0, 0, 0)

# entrada_t do hal/hal.h: a ordem importa.
CIMA, BAIXO, ESQ, DIR, OK, MENU, VOLTAR, VOZ, POWER = 1, 2, 3, 4, 5, 6, 7, 8, 9


def carrega():
    if not os.path.exists(LIB):
        sys.exit("libtinto.so não existe. Rode `make lib` primeiro.")
    t = ctypes.CDLL(LIB)
    t.tinto_tela.restype = ctypes.POINTER(ctypes.c_ubyte)
    t.tinto_abre_tela.argtypes = [ctypes.POINTER(ctypes.c_int)] * 2
    t.tinto_poe_arquivo.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    t.tinto_poe_item.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p,
                                 ctypes.c_int, ctypes.c_int, ctypes.c_int,
                                 ctypes.c_int, ctypes.c_int]
    t.tinto_poe_texto.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_char_p]
    t.tinto_poe_obra.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p,
                                 ctypes.c_int, ctypes.c_int, ctypes.c_int,
                                 ctypes.c_int, ctypes.c_char_p]
    t.tinto_poe_dur.argtypes = [ctypes.c_int, ctypes.c_int]
    t.tinto_sem_cartao.argtypes = [ctypes.c_int]
    t.tinto_agenda.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int]
    t.tinto_wifi_salva.argtypes = [ctypes.c_char_p]
    t.tinto_wifi_lista.argtypes = [ctypes.c_int]
    t.tinto_preparando.argtypes = [ctypes.c_int]
    t.tinto_senha_de.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    t.tinto_wifi_falha.argtypes = [ctypes.c_int]
    t.tinto_memoria_estado.argtypes = [ctypes.c_int]
    t.tinto_poe_item_em.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int,
                                    ctypes.c_char_p, ctypes.c_int, ctypes.c_int]
    t.tinto_abre.argtypes = [ctypes.c_int]
    t.tinto_fase_inicio.argtypes = [ctypes.c_int]
    t.tinto_tem_token.argtypes = [ctypes.c_int]
    t.tinto_rede.argtypes = [ctypes.c_int]
    t.tinto_wifi_salva.argtypes = [ctypes.c_char_p]
    t.tinto_wifi.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_char_p,
                             ctypes.c_char_p]
    t.tinto_rede_no_ar.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int]
    t.tinto_conta.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
    t.tinto_espaco.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
    t.tinto_abre_tela.argtypes = [ctypes.c_int]
    t.tinto_nuvem_responde.argtypes = [ctypes.c_char_p]
    t.tinto_anota.argtypes = [ctypes.c_int] * 3 + [ctypes.c_char_p] * 3
    t.tinto_poe_evento.argtypes = [ctypes.c_int, ctypes.c_char_p,
                                   ctypes.c_char_p, ctypes.c_int]
    t.tinto_propoe_n.argtypes = [ctypes.c_int]
    t.tinto_propoe.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p,
                               ctypes.c_int, ctypes.c_int, ctypes.c_int,
                               ctypes.c_char_p]
    for f in ("tinto_largura", "tinto_altura", "tinto_cursor",
              "tinto_profundidade", "tinto_eventos", "tinto_docado"):
        getattr(t, f).restype = ctypes.c_int
    return t


def monta(t, ano, mes, dia, hora, minuto, itens):
    t.tinto_liga()
    t.tinto_limpa_itens()
    t.tinto_relogio(ano, mes, dia, hora, minuto)
    for titulo, tipo, h, venc, feita, fora in itens:
        t.tinto_poe_item(titulo.encode(), tipo, (h or "").encode(),
                         venc[0], venc[1], venc[2], feita, fora)
    t.tinto_redesenha()


# ── entrar nas áreas ────────────────────────────────────────────────
# O aparelho liga na Home 2x2; a agenda é o primeiro cartão, Ajustes o
# quarto. Aqui, e não em cada cena, para mudar num lugar só.
def entra_na_agenda(t):
    t.tinto_evento(OK, 0)


def entra_nos_ajustes(t):
    """▶ e ▼ chegam no quarto cartão; o OK entra.

    Era pela GAVETA — MENU, desce até "Ajustes", OK —, e a gaveta saiu da
    Home porque a Home JÁ é o espaço de opções. Dentro das áreas ela
    continua existindo, como ação contextual da tela aberta.
    """
    t.tinto_evento(DIR, 0)
    t.tinto_evento(BAIXO, 0)
    t.tinto_evento(OK, 0)


# ── a Home ───────────────────────────────────────────────────────────
def _home(t, passos=()):
    monta(t, 2026, 8, 12, 9, 14, [
        ("Dentista",               EVENTO, "14:00", SEM_DATA, 0, 1),
        ("revisar PR do expansor", TAREFA, None,    SEM_DATA, 0, 0),
    ])
    t.tinto_dono("Usuário".encode())
    for p in passos:
        t.tinto_evento(p, 0)


def home(t):                 _home(t)
def home_acervo(t):          _home(t, (DIR,))
def home_jogos(t):           _home(t, (BAIXO,))
def home_ajustes(t):         _home(t, (DIR, BAIXO))


def xadrez_jogos(t):
    _home(t, (BAIXO, OK))


def xadrez_modos(t):
    _home(t, (BAIXO, OK, OK))


def xadrez_preparar_maquina(t):
    _home(t, (BAIXO, OK, OK, OK))


def _xadrez(t, seleciona=False):
    _home(t, (BAIXO, OK, OK, BAIXO, OK, BAIXO, OK))
    if seleciona:
        t.tinto_evento(OK, 0)


def xadrez_tabuleiro(t): _xadrez(t)
def xadrez_selecao(t):   _xadrez(t, True)


def xadrez_ultima_jogada(t):
    _xadrez(t)
    for passo in (OK, CIMA, OK):
        t.tinto_evento(passo, 0)


def xadrez_ajuda(t):
    _xadrez(t)
    for passo in (MENU, BAIXO, BAIXO, OK, OK):
        t.tinto_evento(passo, 0)


def xadrez_opcoes(t):
    _home(t, (BAIXO, OK, OK, BAIXO, BAIXO, OK))


def xadrez_menu(t):
    _xadrez(t)
    t.tinto_evento(MENU, 0)


def xadrez_historico(t):
    _xadrez(t)
    t.tinto_evento(OK, 0)
    t.tinto_evento(CIMA, 0)
    t.tinto_evento(OK, 0)
    t.tinto_evento(MENU, 0)
    t.tinto_evento(OK, 0)


def xadrez_horizontal(t):
    _home(t, (BAIXO, OK, OK, BAIXO, BAIXO, OK, BAIXO, OK,
              BAIXO, OK, BAIXO, OK))


def xadrez_resultado(t):
    _home(t)
    base = ('{"v":5,"modo":1,"cor":0,"baixo":0,"dificuldade":1,'
            '"orientacao":0,"cursor":31,"origem":-1,"resultado":2,'
            '"ajuda":0,"placar_a2":2,"placar_b2":0,'
            '"lances":"f2f3 e7e5 g2g4 d8h4"')
    soma = 2166136261
    for byte in base.encode():
        soma = ((soma ^ byte) * 16777619) & 0xffffffff
    salvo = f'{base},"checksum":"{soma:08x}"}}'
    t.tinto_poe_arquivo(b"/TINTO/jogos/xadrez.json", salvo.encode())
    for passo in (BAIXO, OK, OK, OK):
        t.tinto_evento(passo, 0)


def xadrez_resultado_local(t):
    _home(t)
    base = ('{"v":5,"modo":0,"cor":0,"baixo":0,"dificuldade":1,'
            '"orientacao":0,"cursor":31,"origem":-1,"resultado":2,'
            '"ajuda":0,"placar_a2":2,"placar_b2":0,'
            '"lances":"f2f3 e7e5 g2g4 d8h4"')
    soma = 2166136261
    for byte in base.encode():
        soma = ((soma ^ byte) * 16777619) & 0xffffffff
    salvo = f'{base},"checksum":"{soma:08x}"}}'
    t.tinto_poe_arquivo(b"/TINTO/jogos/xadrez.json", salvo.encode())
    for passo in (BAIXO, OK, OK, OK):
        t.tinto_evento(passo, 0)


# ── os cenários ──────────────────────────────────────────────────────
def dia_comum(t):
    monta(t, 2026, 8, 12, 9, 14, [
        ("Dentista",                 EVENTO, "14:00", SEM_DATA,      0, 1),
        ("Aula de finlandês",        EVENTO, "19:30", SEM_DATA,      0, 1),
        ("mandar histórico p/ Oulu", TAREFA, None, (2026, 8, 10),    0, 1),
        ("revisar PR do expansor",   TAREFA, None, SEM_DATA,         0, 0),
        ("comprar pasta térmica",    TAREFA, None, SEM_DATA,         0, 1),
        ("pagar internet",           TAREFA, None, SEM_DATA,         1, 1),
        ("ideia do painel",          ANOTACAO, None, SEM_DATA,       0, 0),
        ("pasta térmica",            ANOTACAO, None, SEM_DATA,       0, 0),
    ])


def sem_compromisso(t):
    monta(t, 2026, 8, 13, 9, 2, [
        ("mandar histórico p/ Oulu", TAREFA, None, (2026, 8, 10), 0, 1),
        ("revisar PR do expansor",   TAREFA, None, SEM_DATA,      0, 0),
        ("comprar pasta térmica",    TAREFA, None, SEM_DATA,      0, 0),
    ])


# Evento de dia inteiro vem primeiro e sem "em N h": já está acontecendo.
def home_dia_inteiro(t):
    monta(t, 2026, 8, 12, 9, 14, [
        ("Entrega do documento",     EVENTO, "", SEM_DATA,      0, 1),
        ("Dentista",                 EVENTO, "14:00", SEM_DATA, 0, 1),
        ("revisar PR do expansor",   TAREFA, None, SEM_DATA,    0, 0),
        ("comprar pasta térmica",    TAREFA, None, SEM_DATA,    0, 1),
    ])
    t.tinto_poe_evento(1, b"", b"", 1)


def dia_cheio(t):
    monta(t, 2026, 8, 14, 8, 15, [
        ("Daily do time",            EVENTO, "09:00", SEM_DATA,      0, 1),
        ("Revisão de arquitetura",   EVENTO, "11:00", SEM_DATA,      0, 1),
        ("Reunião com o cliente",    EVENTO, "14:30", SEM_DATA,      0, 1),
        ("Academia",                 EVENTO, "18:00", SEM_DATA,      0, 1),
        ("mandar histórico p/ Oulu", TAREFA, None, (2026, 8, 10),    0, 1),
        ("responder o convite",      TAREFA, None, (2026, 8, 13),    0, 1),
        ("revisar PR do expansor",   TAREFA, None, SEM_DATA,         0, 0),
        ("comprar pasta térmica",    TAREFA, None, SEM_DATA,         0, 0),
        ("trocar o óleo",            TAREFA, None, SEM_DATA,         0, 0),
        ("ler o datasheet do PCF",   TAREFA, None, SEM_DATA,         0, 0),
        ("uma captura",              ANOTACAO, None, SEM_DATA,       0, 0),
    ])


def fim_do_dia(t):
    monta(t, 2026, 8, 12, 21, 38, [
        ("Dentista",              EVENTO, "14:00", SEM_DATA, 0, 1),
        ("comprar pasta térmica", TAREFA, None, SEM_DATA,    0, 0),
        ("mandar histórico",      TAREFA, None, SEM_DATA,    1, 1),
        ("revisar PR",            TAREFA, None, SEM_DATA,    1, 0),
        ("ideia do encoder",      ANOTACAO, None, SEM_DATA,  0, 0),
    ])


def vazia(t):
    monta(t, 2026, 8, 15, 10, 30, [])


# ── T-16 · a nota ───────────────────────────────────────────────────
# Resumo em cima, transcrição crua embaixo, com os "né": prova que a IA
# estruturou e não inventou (RN-28).
def nota(t):
    monta(t, 2026, 8, 12, 7, 44, [
        ("ideia do painel da dock", ANOTACAO, "07:12", SEM_DATA, 0, 0),
    ])
    t.tinto_poe_dur(1, 23)
    t.tinto_poe_texto(
        1,
        "então, tava pensando aqui que o painel da dock talvez não devesse "
        "mostrar o próximo compromisso, né, porque… porque na dock você "
        "olha de longe, e o que muda mesmo é a tarefa".encode(),
        "O painel da dock deve mostrar as tarefas do dia, e não o próximo "
        "compromisso — na dock o aparelho é olhado de longe e por muito "
        "tempo, e o que muda ao longo do dia são as tarefas".encode())
    t.tinto_abre(0)


# Antes da rede: sem resumo, o aviso ocupa o lugar do bloco e a
# transcrição continua ali.
def nota_sem_ia(t):
    monta(t, 2026, 8, 12, 7, 44, [
        ("", NADA, "14:20", SEM_DATA, 0, 0),
    ])
    t.tinto_poe_dur(1, 41)
    t.tinto_poe_texto(1, "".encode(), "".encode())
    t.tinto_abre(0)


# ── o destino: onde isto foi parar do lado de fora ──────────────────
# A que NASCEU aqui diz em qual agenda foi; a que VEIO do Google diz que
# ali é só leitura.
def nota_evento_daqui(t):
    monta(t, 2026, 8, 12, 7, 44, [
        ("Reunião com o cliente", EVENTO, "15:00", SEM_DATA, 0, 0),
    ])
    t.tinto_poe_dur(1, 38)
    t.tinto_poe_texto(
        1,
        "marca reunião com o cliente quinta às três".encode(),
        "Reunião com o cliente, quinta 14 de agosto às 15:00.".encode())
    t.tinto_abre(0)


def mudar_a_hora(t):
    """O mostrador: a casa sob o cursor em negativo, número branco."""
    nota_evento_daqui(t)
    t.tinto_abre_tela(_T["TELA_ESCOLHER_HORA"])
    t.tinto_redesenha()


def nota_evento_do_google(t):
    monta(t, 2026, 8, 12, 7, 44, [
        ("Dentista", EVENTO, "14:00", SEM_DATA, 0, 1),
    ])
    t.tinto_poe_evento(1, b"15:00", "Rua Bahia, 210 · Funcionarios".encode(), 0)
    t.tinto_poe_agenda(1, "Pessoal".encode())
    t.tinto_abre(0)


# ── T-31 · as capturas do dia ───────────────────────────────────────
# Em ordem de fala, com o que aquilo virou. A última é offline: o título
# ainda é a hora.
def capturas(t):
    monta(t, 2026, 8, 12, 7, 41, [
        ("ideia do painel da dock", ANOTACAO, "07:12", SEM_DATA, 0, 0),
        ("comprar pasta térmica",   TAREFA,   "07:41", SEM_DATA, 0, 0),
        ("reunião com o cliente",   EVENTO,   "09:30", SEM_DATA, 0, 0),
        ("lista do mercado",        LISTA,    "11:05", SEM_DATA, 0, 0),
        ("",                        NADA,     "14:20", SEM_DATA, 0, 0),
    ])
    for i, seg in ((1, 23), (2, 9), (3, 14), (4, 31), (5, 41)):
        t.tinto_poe_dur(i, seg)

    entra_na_agenda(t)
    # A primeira linha da agenda: o OK abre o que foi falado hoje.
    t.tinto_evento(OK, 0)


# ── T-06 · o dia ────────────────────────────────────────────────────
# EVENTO e TAREFA juntos: onde o rótulo e a contagem se encontravam.
def calendario_cheio(t):
    monta(t, 2026, 8, 12, 9, 14, [
        ("Dentista",          EVENTO, "14:00", SEM_DATA,      0, 1),
        ("Aula de finlandês", EVENTO, "19:30", SEM_DATA,      0, 1),
        ("Reunião do time",   EVENTO, "16:00", SEM_DATA,      0, 1),
        ("pagar a internet",  TAREFA, None,    (2026, 8, 12), 0, 1),
        ("comprar pasta",     TAREFA, None,    (2026, 8, 12), 0, 1),
        ("responder o Oulu",  TAREFA, None,    (2026, 8, 12), 0, 1),
    ])
    entra_na_agenda(t)
    t.tinto_evento(MENU, 0)
    t.tinto_evento(OK, 0)


def dia_passado(t):
    monta(t, 2026, 8, 12, 7, 42, [
        ("Reunião de alinhamento", EVENTO, "09:00", SEM_DATA, 0, 1),
        ("Academia",               EVENTO, "18:30", SEM_DATA, 0, 1),
        # Com PRAZO no dia aberto: é o que faz uma tarefa pertencer a um dia. Sem
        # prazo ela fica em "POR FAZER".
        ("pagar a internet",       TAREFA, None,    (2026, 8, 12), 1, 0),
        ("responder o Oulu",       TAREFA, None,    (2026, 8, 12), 0, 1),
        ("o que ficou da reunião", ANOTACAO, "09:41", SEM_DATA, 0, 0),
        ("ideia do encoder",       ANOTACAO, "21:02", SEM_DATA, 0, 0),
    ])
    t.tinto_poe_dur(5, 72)
    t.tinto_poe_dur(6, 34)
    # As listas de destino: o `local` é onde o nome da lista viaja.
    t.tinto_poe_evento(3, b"", "Casa".encode(), 0)
    t.tinto_poe_evento(4, b"", "Minhas tarefas".encode(), 0)
    # Pelo CALENDÁRIO, a porta de verdade do dia (com o cursor num compromisso,
    # o OK entraria nele).
    entra_na_agenda(t)
    t.tinto_evento(MENU, 0)
    t.tinto_evento(OK, 0)      # Calendário
    t.tinto_evento(OK, 0)      # o dia sob o cursor — hoje


# ── a Agenda em páginas ─────────────────────────────────────────────
# Dia cheio: a primeira página, e a segunda com o rodapé "2/2".
def _agenda_cheia(t):
    monta(t, 2026, 8, 12, 9, 14, [
        ("Daily do time",            EVENTO, "10:00", SEM_DATA,      0, 1),
        ("Cliente",                  EVENTO, "14:30", SEM_DATA,      0, 1),
        ("Academia",                 TAREFA, "18:00", (2026, 8, 12), 0, 0),
        ("mandar histórico p/ Oulu", TAREFA, None, (2026, 8, 10), 0, 1),
        ("responder o convite",      TAREFA, None, (2026, 8, 11), 0, 1),
        ("revisar PR do expansor",   TAREFA, None, SEM_DATA,      0, 0),
        ("comprar pasta térmica",    TAREFA, None, SEM_DATA,      0, 0),
        ("trocar o óleo",            TAREFA, None, SEM_DATA,      0, 0),
        ("ler o datasheet do PCF",   TAREFA, None, SEM_DATA,      0, 0),
        ("renovar o passaporte",     TAREFA, None, SEM_DATA,      0, 0),
        ("marcar o dentista",        TAREFA, None, SEM_DATA,      0, 0),
        ("devolver o livro",         TAREFA, None, SEM_DATA,      0, 0),
        ("consertar a torneira",     TAREFA, None, SEM_DATA,      0, 0),
    ])
    entra_na_agenda(t)


def agenda_paginas(t):
    _agenda_cheia(t)
    t.tinto_redesenha()


def agenda_paginas_fim(t):
    _agenda_cheia(t)
    for _ in range(12):
        t.tinto_evento(BAIXO, 0)


# ── T-20 · o calendário ──────────────────────────────────────────────
def calendario(t):
    monta(t, 2026, 8, 12, 7, 41, [
        ("Dentista",              EVENTO,   "14:00", SEM_DATA, 0, 1),
        ("Aula de finlandês",     EVENTO,   "19:30", SEM_DATA, 0, 1),
        ("ideia do painel",       ANOTACAO, "07:12", SEM_DATA, 0, 0),
        ("o que ficou da reunião", ANOTACAO, "09:41", SEM_DATA, 0, 0),
    ])
    # Itens espalhados pelo mês: o calendário é um mapa do mês, não do dia.
    for dia, tipo, fora in ((3, EVENTO, 1), (5, ANOTACAO, 0), (6, EVENTO, 1),
                            (6, ANOTACAO, 0), (10, EVENTO, 1), (11, ANOTACAO, 0),
                            (13, EVENTO, 1), (18, EVENTO, 1), (21, EVENTO, 1)):
        t.tinto_poe_item_em(2026, 8, dia, "x".encode(), tipo, fora)
    # Pelo MENU: na home o ◀ anda de DIA.
    entra_na_agenda(t)
    t.tinto_evento(MENU, 0)
    t.tinto_evento(OK, 0)      # Calendário é a primeira linha da gaveta


# ── T-14 · a gaveta, por cima da tela ────────────────────────────────
def gaveta(t):
    dia_comum(t)
    entra_na_agenda(t)
    t.tinto_evento(MENU, 0)


# ── T-25 · ajustes ───────────────────────────────────────────────────
def ajustes(t):
    dia_comum(t)
    entra_nos_ajustes(t)
    # Desce até o fim: prova que "Sobre" é alcançável.
    for _ in range(12):
        t.tinto_evento(BAIXO, 0)


# ── a voz ───────────────────────────────────────────────────────────
# Falar EXIGE rede (o ● recusa no gesto). Estas cenas ligam a rede; a
# recusa tem cena própria (`voz-sem-rede`).
def gravando(t):
    dia_comum(t)
    t.tinto_rede(1)
    t.tinto_evento(VOZ, 0)
    for _ in range(14):
        t.tinto_tick()


def gravando_pausado(t):
    dia_comum(t)
    t.tinto_rede(1)
    t.tinto_evento(VOZ, 0)
    for _ in range(20):
        t.tinto_tick()
    t.tinto_evento(VOZ, 0)      # pausa
    t.tinto_evento(VOZ, 0)      # retoma: dois trechos
    for _ in range(18):
        t.tinto_tick()
    t.tinto_evento(VOZ, 0)      # pausa de novo


def gravando_fora_da_home(t):
    dia_comum(t)
    t.tinto_rede(1)
    entra_na_agenda(t)
    t.tinto_evento(MENU, 0)     # entra numa tela pela gaveta da Agenda
    t.tinto_evento(OK, 0)
    t.tinto_evento(VOZ, 0)
    for _ in range(9):
        t.tinto_tick()


def anotacoes(t):
    """A lista vazia: o convite inteiro."""
    monta(t, 2026, 8, 12, 9, 14, [])
    t.tinto_abre_anotacoes()
    t.tinto_redesenha()


FALA_DA_IDEIA = (
    "pensei que o case podia ter um encaixe magnético na base, com dois ímãs "
    "pequenos de neodímio, e o botão de voz um pouco mais para a direita, "
    "porque com a mão esquerda segurando o aparelho o polegar alcança melhor "
    "ali")


def anotacoes_aberta(t):
    """A anotação aberta: o título e o que foi dito, inteiro."""
    anotacoes_lista(t)
    t.tinto_evento(OK, 0)


def anotacoes_lista(t):
    """Oito anotações: a primeira página e o "1 / 2"."""
    dia_comum(t)
    for dia, hora, titulo in [
            (12, "18:40", "Ideia para o encaixe do case"),
            (12, "09:15", "O que a Ana falou do projeto"),
            (11, "21:02", "Livro que quero ler: O estrangeiro"),
            (11, "07:30", "Sonho estranho com o mar"),
            (10, "14:10", "Testar o encoder no lugar do 5-vias"),
            (9,  "19:00", "Receita da torta de limão"),
            (8,  "12:00", "Lembrar do aniversário do pai"),
            (6,  "10:00", "Lista de presentes")]:
        t.tinto_anota(2026, 8, dia, hora.encode(), titulo.encode(),
                      FALA_DA_IDEIA.encode() if dia == 12 and hora == "18:40"
                      else b"")
    t.tinto_abre_anotacoes()
    t.tinto_redesenha()


# Sem rede: o dia continua na tela, e o rodapé diz que nada muda até
# conectar.
def home_sem_rede(t):
    dia_comum(t)


# A recusa: sem rede o ● não grava, e a faixa diz por quê na hora.
def voz_sem_rede(t):
    dia_comum(t)
    t.tinto_evento(VOZ, 0)


def descartar(t):
    dia_comum(t)
    t.tinto_evento(VOZ, 0)
    for _ in range(38):
        t.tinto_tick()
    t.tinto_evento(VOZ, 0)
    t.tinto_evento(VOZ, 0)
    for _ in range(1):
        t.tinto_tick()
    t.tinto_evento(VOLTAR, 0)   # ◀ gravando = descartar


# ── T-01 · a tela BLOQUEADA ─────────────────────────────────────────
# E-ink segura a imagem em deep sleep: o bloqueio é a cara do objeto em cima
# da mesa. Deitado na dock é a única tela horizontal, e não se opera.
def dock(t):
    HOJE = (2026, 9, 1)
    monta(t, *HOJE, 11, 17,
          [("Dentista", EVENTO, "14:00", HOJE, 0, 0),
           ("Aula de finlandês", EVENTO, "19:30", HOJE, 0, 0)])
    t.tinto_docar(1)
    t.tinto_abre_tela(_T["TELA_BLOQUEADA"])
    t.tinto_redesenha()


def bloqueio(t):
    dia_comum(t)
    t.tinto_evento(POWER, 0)


def bloqueio_mes_longo(t):
    """A data mais comprida do ano: o mês não pode cortar na borda."""
    monta(t, 2026, 9, 30, 9, 14, [("Dentista", EVENTO, "14:00", SEM_DATA, 0, 1)])
    t.tinto_evento(POWER, 0)


def bloqueio_voz(t):
    bloqueio(t)
    t.tinto_evento(VOZ, 0)


# ── a tela de diagnóstico ───────────────────────────────────────────
# Direto pela pilha: contar descidas quebra em silêncio quando um item entra
# ou sai do menu.
def sobre(t):
    dia_comum(t)
    t.tinto_abre_tela(_T["TELA_SOBRE"])
    t.tinto_redesenha()


# ── T-26 · a fala do mês, nos três estados ──────────────────────────
# Pelo caminho de verdade: a prova também prova a PORTA.
def _abre_fala(t):
    t.tinto_conta(b"Convidado", b"")
    t.tinto_rede(2)                      # REDE_LIGADA
    # Direto pela pilha: contar descidas em Minha conta já levou a prova à
    # tela errada.
    t.tinto_abre_tela(_T["TELA_FALA"])
    t.tinto_redesenha()


# O limite de verdade, 120 min: o número de três dígitos precisa caber no
# card.
def fala_folgada(t):
    dia_comum(t)
    t.tinto_quota(12, 120, 19)
    _abre_fala(t)


def fala_no_limite(t):
    dia_comum(t)
    t.tinto_quota(118, 120, 19)
    _abre_fala(t)


def fala_estourada(t):
    # Duas capturas sem tipo: é o que "espera transcrição" quer dizer.
    monta(t, 2026, 8, 12, 9, 14, [
        ("", NADA, "08:47", SEM_DATA, 0, 0),
        ("", NADA, "07:03", SEM_DATA, 0, 0),
    ])
    t.tinto_poe_dur(0, 74)
    t.tinto_poe_dur(1, 22)
    t.tinto_quota(120, 120, 1)
    _abre_fala(t)


# ── as telas de rede e de conta ─────────────────────────────────────
# Os números vêm do HEADER: índice de enum copiado à mão abria a tela errada
# sem nenhuma falha.
def _telas():
    import re
    from pathlib import Path
    h = (Path(RAIZ) / "firmware/main/nucleo/estado.h").read_text()
    bloco = h[h.index("TELA_AGENDA = 0,"):h.index("} tela_id;")]
    nomes = re.findall(r"^\s*(TELA_[A-Z_0-9]+)\s*(?:=\s*\d+)?\s*,",
                       bloco, re.M)
    return {n: i for i, n in enumerate(nomes)}


_T = _telas()


def _fase(nome):
    """O índice de uma fase de `inicio_fase_t`, lido do header.

    Número de enum copiado à mão manda a cena para a tela errada sem nada
    acusar quando o enum muda.
    """
    import re
    from pathlib import Path
    h = (Path(RAIZ) / "firmware/main/nucleo/inicializacao.h").read_text()
    bloco = h[h.index("INICIO_HOME = 0"):h.index("} inicio_fase_t;")]
    nomes = re.findall(r"^\s*(INICIO_[A-Z_0-9]+)", bloco, re.M)
    return nomes.index(nome)


_FASE_CONCLUSAO = _fase("INICIO_CONCLUSAO")
WIFI          = _T["TELA_WIFI"]
ARMAZENAMENTO = _T["TELA_ARMAZENAMENTO"]
CONEXAO       = _T["TELA_CONTA"]
SINCRONIZACAO = _T["TELA_SINCRONIZACAO"]
DATA_HORA     = _T["TELA_DATA_HORA"]


def wifi_lista(t):
    dia_comum(t)
    t.tinto_wifi_lista(1)
    t.tinto_rede_no_ar(b"casa", 82, 1)
    t.tinto_rede_no_ar(b"casa_5G", 74, 0)
    t.tinto_rede_no_ar(b"VIVO-A24C", 51, 0)
    t.tinto_rede_no_ar(b"NET_2G_9BC1", 18, 0)
    t.tinto_abre_tela(WIFI)
    t.tinto_redesenha()


# Muitas redes: é onde a PÁGINA e o contador aparecem.
def wifi_lista_cheia(t):
    dia_comum(t)
    t.tinto_wifi_lista(1)
    for nome, forca, salva in [(b"casa", 82, 1), (b"casa_5G", 74, 0),
                               (b"VIVO-A24C", 51, 0), (b"NET_2G_9BC1", 18, 0),
                               (b"Escritorio", 66, 0), (b"Cafe do centro", 40, 0),
                               (b"Vizinho_5G", 22, 0), (b"TP-Link_9C2A", 35, 0)]:
        t.tinto_rede_no_ar(nome, forca, salva)
    t.tinto_abre_tela(WIFI)
    t.tinto_redesenha()


def wifi_senha(t):
    dia_comum(t)
    t.tinto_wifi(0, 0, b"", b"")
    t.tinto_senha_de(b"Escritorio", b"abc12345")
    t.tinto_redesenha()


def wifi_conectado(t):
    dia_comum(t)
    t.tinto_wifi(2, 82, b"casa", b"192.168.0.42")
    t.tinto_wifi_salva(b"casa")
    t.tinto_rede_no_ar(b"casa_5G", 74, 0)
    t.tinto_rede_no_ar(b"VIVO-A24C", 51, 0)
    t.tinto_abre_tela(WIFI)
    t.tinto_redesenha()


# As duas falhas são telas diferentes: uma se resolve digitando de novo, a
# outra não.
def _conexao_erro(t, qual):
    dia_comum(t)
    t.tinto_wifi(0, 0, "Escritório".encode(), b"")
    t.tinto_wifi_falha(qual)
    t.tinto_abre_tela(WIFI)
    t.tinto_redesenha()


# Dois ticks pegam o meio da animação dos pontinhos.
def conexao_conectando(t):
    dia_comum(t)
    t.tinto_wifi(1, 0, b"Escritorio", b"")
    t.tinto_abre_tela(WIFI)
    t.tinto_tick()
    t.tinto_tick()


def conexao_senha_recusada(t): _conexao_erro(t, 1)
def conexao_sem_resposta(t):   _conexao_erro(t, 2)


def conexao_sem_rede(t):
    dia_comum(t)
    t.tinto_abre_tela(CONEXAO)
    t.tinto_redesenha()


def conexao_parear(t):
    dia_comum(t)
    t.tinto_wifi(2, 82, b"casa", b"192.168.0.42")
    t.tinto_conta(b"", b"KXPT4M")
    t.tinto_abre_tela(CONEXAO)
    t.tinto_redesenha()


def conexao_pareada(t):
    dia_comum(t)
    t.tinto_wifi(2, 82, b"casa", b"192.168.0.42")
    t.tinto_conta(b"Convidado", b"")
    t.tinto_abre_tela(CONEXAO)
    t.tinto_redesenha()


def sincronizacao(t):
    dia_comum(t)
    t.tinto_wifi(2, 82, b"casa", b"192.168.0.42")
    t.tinto_conta(b"Convidado", b"")
    t.tinto_abre_tela(SINCRONIZACAO)
    t.tinto_redesenha()


def data_e_hora_ajustes(t):
    dia_comum(t)
    t.tinto_abre_tela(DATA_HORA)
    t.tinto_redesenha()


def armazenamento(t):
    dia_comum(t)
    t.tinto_espaco(2150, 7380, 1900)
    t.tinto_abre_tela(ARMAZENAMENTO)
    t.tinto_redesenha()


# ── T-30 · o recibo, nos dois caminhos ──────────────────────────────
# O MESMO gesto, duas telas: lado a lado se confere que só muda o que deve.
def _fala_e_confirma(t):
    dia_comum(t)

    # A REDE: sem ela o ● recusa (RN-15) e a cena mostraria outra tela.
    t.tinto_rede(1)

    entra_na_agenda(t)
    t.tinto_evento(VOZ, 0)
    for _ in range(41):
        t.tinto_tick()
    t.tinto_evento(OK, 0)


def recibo(t):
    _fala_e_confirma(t)


# A confirmação: o que o aparelho entendeu, antes de virar verdade.
def confirma_fala_longa(t):
    """A frase crua inteira, mesmo longa: a tela rola em vez de cortar."""
    dia_comum(t)
    t.tinto_rede(1)
    t.tinto_propoe("Ideia para o case".encode(), ANOTACAO, b"", 2026, 8, 12,
                   ("pensei que o case podia ter um encaixe magnético na base, "
                    "com dois ímãs pequenos de neodímio, e o botão de voz um "
                    "pouco mais para a direita, porque com a mão esquerda "
                    "segurando o aparelho o polegar alcança melhor ali; e a "
                    "entrada USB podia ficar atrás, escondida, para o cabo não "
                    "aparecer na mesa").encode())


def confirma_evento(t):
    dia_comum(t)
    t.tinto_rede(1)
    t.tinto_propoe("Reunião com o cliente".encode(), EVENTO, b"15:00",
                   2026, 8, 14,
                   "marca reunião com o cliente quinta às três".encode())


def evento_dia_inteiro(t):
    monta(t, 2026, 8, 12, 7, 44, [
        ("Entrega do documento", EVENTO, "", SEM_DATA, 0, 1),
    ])
    t.tinto_poe_evento(1, b"", b"", 1)
    t.tinto_abre(0)


def confirma_tarefa(t):
    dia_comum(t)
    t.tinto_rede(1)
    t.tinto_propoe("comprar pasta térmica".encode(), TAREFA, b"",
                   0, 0, 0,
                   "me lembra de comprar pasta térmica quando eu sair".encode())


# O pior caso: três resultados, títulos compridos, endereço, frase longa.
def confirma_tres(t):
    dia_comum(t)
    t.tinto_rede(1)
    t.tinto_propoe_n(3)


# Rolado para cima: dá para reler o que ele entendeu quando os três não
# cabem.
def confirma_tres_rolado(t):
    confirma_tres(t)
    for _ in range(3):
        t.tinto_evento(CIMA, 0)


# O RESULTADO só entra depois de a última ação voltar do servidor:
# "concluído" com ações em voo promete o que pode falhar.
def resultado(t):
    dia_comum(t)
    t.tinto_rede(2)              # ligada: as ações sobem
    t.tinto_propoe_n(2)
    t.tinto_evento(OK, 0)        # "Fazer as duas coisas"
    for _ in range(2):           # as duas ações voltam do servidor
        t.tinto_nuvem_responde(b'{"ok":true,"id":"t:1"}')
    t.tinto_redesenha()


# ── as cinco falhas da voz ──────────────────────────────────────────
# Tudo o que pode dar errado entre apertar o ● e ver o que o aparelho
# entendeu.

# Sem cena para "Não gravei": cartão ausente ou cheio têm TELA PRÓPRIA. A
# faixa fica para o microfone que não abre.

# O áudio que NÃO saiu: a rede cai entre falar e soltar o OK.
def voz_nao_enviei(t):
    dia_comum(t)
    t.tinto_rede(1)
    t.tinto_evento(VOZ, 0)
    for _ in range(10):
        t.tinto_tick()
    t.tinto_rede(0)
    t.tinto_evento(OK, 0)


# A resposta que não vem: o áudio subiu e o prazo estourou. A fala está no
# cartão, e a frase diz isso.
PRAZO_S = 100   # ESTRUTURA_PRAZO_MS, em segundos


def voz_sem_resposta(t):
    dia_comum(t)
    t.tinto_rede(1)
    t.tinto_nuvem_demora(1)
    t.tinto_evento(VOZ, 0)
    for _ in range(10):
        t.tinto_tick()
    t.tinto_evento(OK, 0)
    # O prazo vem da constante do firmware: número fixo deixava de alcançá-lo
    # e a cena mostrava a espera, calada.
    for _ in range(PRAZO_S + 3):
        t.tinto_tick()


# ENVIANDO: as ações em voo, sem o botão para apertar de novo (o toque
# repetido duplicava o evento).
def conferir_enviando(t):
    dia_comum(t)
    t.tinto_rede(1)
    t.tinto_propoe_n(2)
    t.tinto_nuvem_demora(1)
    t.tinto_evento(OK, 0)
    t.tinto_tick()


# ESTRUTURANDO: a resposta é segurada, senão o simulador responde no mesmo
# tick e este quadro não existe.
def recibo_estruturando(t):
    dia_comum(t)
    t.tinto_rede(1)
    t.tinto_nuvem_demora(1)
    t.tinto_evento(VOZ, 0)
    for _ in range(41):
        t.tinto_tick()
    t.tinto_evento(OK, 0)
    t.tinto_tick()


# ── T-28 · o teclado ─────────────────────────────────────────────────
def teclado(t):
    dia_comum(t)
    entra_na_agenda(t)
    # Linha 0 é a captura e 1-2 são a agenda: a primeira tarefa é a 3.
    for _ in range(3):
        t.tinto_evento(BAIXO, 0)
    t.tinto_evento(DIR, 0)       # entra no detalhe do item
    t.tinto_evento(DIR, 0)       # abre a gaveta de ações
    t.tinto_evento(OK, 0)        # "renomear", que é a primeira
    for _ in range(3):
        t.tinto_evento(DIR, 0)


# ── as ações da nota ────────────────────────────────────────────────
# A FALA ORIGINAL: aberta pelo OK no detalhe; dois itens da mesma fala vêm
# parar aqui.
def fala_original(t):
    nota(t)
    t.tinto_evento(OK, 0)


# Concluir é a ação principal; ver a fala fica logo abaixo.
def tarefa_detalhe(t):
    monta(t, 2026, 8, 12, 7, 44, [
        ("Levar os exames", TAREFA, None, (2026, 8, 14), 0, 1),
    ])
    t.tinto_rede(1)
    t.tinto_poe_dur(1, 23)
    t.tinto_poe_texto(1, "leva os exames pra consulta".encode(),
                         "Levar os exames para a consulta.".encode())
    t.tinto_abre(0)


# CONCLUÍDA: a data em que foi fechada, como o Google mostra, e o botão
# "Desmarcar".
def tarefa_detalhe_feita(t):
    tarefa_detalhe(t)
    t.tinto_evento(OK, 0)


# Rolado até o fim: a descrição inteira, e os botões com foco.
def tarefa_detalhe_rolado(t):
    tarefa_detalhe_feita(t)
    for _ in range(3):
        t.tinto_evento(BAIXO, 0)


# Pelo MENU: a gaveta é o submenu da página; o ▶ entra no item (RN-37).
def nota_acoes(t):
    nota(t)
    t.tinto_evento(MENU, 0)


# ── RN-A3 · sem cartão ───────────────────────────────────────────────
def sem_cartao(t):
    t.tinto_liga()
    t.tinto_sem_cartao(1)     # tirar o cartão reinicia o aparelho


# ── aparência ────────────────────────────────────────────────────────
def _conta(t, rede=1, google=1, cadastrado=1):
    dia_comum(t)
    t.tinto_dono("Usuário".encode())
    t.tinto_rede(rede)
    if google:
        t.tinto_quota(18, 60, 12)
    entra_nos_ajustes(t)
    t.tinto_evento(OK, 0)


def conta_conectada(t):   _conta(t)


# O cursor no ÍCONE DE SAIR, como o "◀ hoje ▶" da Agenda.
def conta_desconectar(t):
    _conta(t)
    t.tinto_evento(CIMA, 0)
def conta_sem_rede(t):    _conta(t, rede=0)


# COM rede: sem `tinto_rede(1)` o leque não é desenhado.
def barra_com_rede(t):
    dia_comum(t)
    t.tinto_rede(1)


# A confirmação de desconectar diz o que se perde e o que fica.
def conta_confirma(t):
    _conta(t)
    t.tinto_evento(CIMA, 0)
    t.tinto_evento(OK, 0)


def _sinc(t, rede=1):
    dia_comum(t)
    t.tinto_dono("Usuário".encode())
    t.tinto_quota(18, 60, 12)
    t.tinto_rede(rede)
    entra_nos_ajustes(t)
    t.tinto_evento(OK, 0)          # Minha conta
    for _ in range(2):
        t.tinto_evento(BAIXO, 0)   # Sincronizar agendas, o terceiro destino
    t.tinto_evento(OK, 0)
    # DEPOIS de entrar: abrir a tela pede o catálogo, e a resposta vazia
    # zeraria as agendas postas antes.
    t.tinto_agenda("Pessoal".encode(), 1, 0)
    t.tinto_agenda("Trabalho".encode(), 1, 0)
    t.tinto_agenda("Aniversários".encode(), 0, 213)
    t.tinto_agenda("Feriados no Brasil".encode(), 0, 14)
    t.tinto_redesenha()


# Coisa demais no dia: é onde a página do DIA aparece.
def dia_tela_cheia(t):
    HOJE = (2026, 9, 2)
    monta(t, *HOJE, 9, 14,
          [(f"compromisso número {i}", EVENTO, f"{7+i:02d}:00", HOJE, 0, 0)
           for i in range(12)])
    t.tinto_abre_tela(_T["TELA_DIA"])
    t.tinto_redesenha()


# A operação bloqueante no meio: uma etapa feita, uma em curso, uma por
# vir. O único quadro sem rodapé.
def preparando(t):
    dia_comum(t)
    t.tinto_preparando(1)
    t.tinto_redesenha()


def sinc_lista(t):     _sinc(t)


# Doze agendas, mais três que não couberam: a última das três páginas, com
# o nome mais comprido que o servidor manda (31 letras).
def sinc_paginas(t):
    _sinc(t)
    for i in range(7):
        t.tinto_agenda(f"Compartilhada {i + 1}".encode(), i % 2, 0)
    t.tinto_agenda("Aniversários da família Melo 31".encode(), 1, 0)
    t.tinto_agendas_fora(3)
    for _ in range(8):
        t.tinto_evento(BAIXO, 0)


# BUSCANDO: sem catálogo e com o servidor mudo, o que se vê enquanto a
# lista não chega.
def sinc_buscando(t):
    dia_comum(t)
    t.tinto_dono("Usuário".encode())
    t.tinto_quota(18, 60, 12)
    t.tinto_rede(1)
    t.tinto_nuvem_demora(1)
    entra_nos_ajustes(t)
    t.tinto_evento(OK, 0)
    for _ in range(2):
        t.tinto_evento(BAIXO, 0)
    t.tinto_evento(OK, 0)
    # Dois ticks: o meio da animação.
    t.tinto_tick()
    t.tinto_tick()


# SEM REDE com catálogo salvo: as agendas continuam visíveis (a spec diz
# que ficam disponíveis); o erro só tira o MEXER.
def sinc_sem_rede(t):
    _sinc(t)
    t.tinto_rede(0)
    t.tinto_redesenha()


def ajustes_raiz(t):
    dia_comum(t)
    entra_nos_ajustes(t)


def som(t):
    dia_comum(t)
    entra_nos_ajustes(t)
    for _ in range(3):
        t.tinto_evento(BAIXO, 0)
    t.tinto_evento(OK, 0)


def aparencia(t):
    dia_comum(t)
    entra_nos_ajustes(t)
    for _ in range(2):
        t.tinto_evento(BAIXO, 0)   # a terceira categoria
    t.tinto_evento(OK, 0)



# ── a inicialização offline ────────────────────────────────────────
# Pede o cenário de mídia e deixa o app decidir a fase, como na placa.
def memoria_ausente(t):
    t.tinto_liga()
    t.tinto_memoria_estado(1)          # MEMORIA_AUSENTE


def memoria_comunicacao(t):
    t.tinto_liga()
    t.tinto_memoria_estado(2)          # MEMORIA_COMUNICACAO


def memoria_reparo(t):
    t.tinto_liga()
    t.tinto_memoria_danificada()


def memoria_confirmar_formatar(t):
    t.tinto_liga()
    t.tinto_memoria_danificada()
    t.tinto_evento(BAIXO, 0)           # "formatar como novo"
    t.tinto_evento(OK, 0)


def memoria_confirmar_no_sim(t):
    memoria_confirmar_formatar(t)
    t.tinto_evento(BAIXO, 0)           # cursor movido para "sim, apagar tudo"


def _primeiro_uso(t):
    t.tinto_liga()
    t.tinto_memoria_virgem()


def boas_vindas(t):
    _primeiro_uso(t)


def nome_proprietario(t):
    boas_vindas(t)
    t.tinto_evento(OK, 0)
    for tecla in (OK,):          # digita a primeira letra do teclado
        t.tinto_evento(tecla, 0)


def data_e_hora(t):
    _primeiro_uso(t)
    t.tinto_pula_para_data_hora()
    t.tinto_evento(DIR, 0)        # pula o Wi-Fi, que vem antes da hora
    t.tinto_evento(DIR, 0)        # e seleciona o mês


# ── a conta: primeiro ele se registra, depois a pessoa vincula ──────
def conta_conectando(t):
    t.tinto_pula_para_conclusao()
    t.tinto_fase_inicio(_fase("INICIO_CONTA"))
    t.tinto_tem_token(0)
    t.tinto_tick(); t.tinto_tick()   # meio da animação


def conta_registrado(t):
    t.tinto_pula_para_conclusao()
    t.tinto_fase_inicio(_fase("INICIO_CONTA"))
    t.tinto_tem_token(1)


def rede_do_primeiro_uso(t):
    t.tinto_pula_para_conclusao()
    t.tinto_fase_inicio(_fase("INICIO_WIFI"))


# Pela FASE, e não pelo caminho: prova de tela é sobre o DESENHO; o
# caminho tem os testes do app.
def conclusao(t):
    _primeiro_uso(t)
    t.tinto_pula_para_conclusao()
    t.tinto_dono("Usuário".encode())
    t.tinto_rede(2)
    t.tinto_conta(b"usuario@exemplo.com", b"")
    t.tinto_fase_inicio(_FASE_CONCLUSAO)


def memoria_somente_leitura(t):
    t.tinto_liga()
    t.tinto_memoria_estado(5)          # MEMORIA_SOMENTE_LEITURA


def memoria_cheia(t):
    t.tinto_liga()
    t.tinto_memoria_estado(6)          # MEMORIA_CHEIA


# ── as cenas da AGENDA ──────────────────────────────────────────────
# As de cima param na Home (onde o aparelho liga) e servem de base. Estas
# entram na agenda e viram a prova dela.
def agenda_dia_comum(t):        dia_comum(t);       entra_na_agenda(t)


# O cursor SOLTO: o ◀▶ troca o dia, e a moldura em "◀ hoje ▶" diz isso.
def agenda_nav_em_foco(t):
    dia_comum(t)
    entra_na_agenda(t)
    t.tinto_evento(BAIXO, 0)     # desce para uma linha
    t.tinto_evento(ESQ, 0)       # e solta o cursor
def agenda_sem_compromisso(t):  sem_compromisso(t); entra_na_agenda(t)
def agenda_dia_cheio(t):        dia_cheio(t);       entra_na_agenda(t)
def agenda_fim_do_dia(t):       fim_do_dia(t);      entra_na_agenda(t)
def agenda_dia_vazio(t):        vazia(t);           entra_na_agenda(t)
def agenda_sem_rede(t):         home_sem_rede(t);   entra_na_agenda(t)
def agenda_dia_inteiro(t):      home_dia_inteiro(t); entra_na_agenda(t)


# ── o Acervo e o leitor ─────────────────────────────────────────────
# Os três estados da estante, o leitor com texto de verdade, e o vazio.
SO_ONLINE, BAIXANDO, AQUI = 0, 1, 2
LIVRO, DOCUMENTO = 0, 1

TRECHO = ("Continuei até a esquina. O céu estava límpido e a rua, "
          "silenciosa. Pensei em voltar, mas a inquietação não me "
          "deixava parar. Caminhei mais um quarteirão, contando os "
          "passos, até a avenida terminar. ") * 6


def _acervo(t, obras, passos=()):
    monta(t, 2026, 9, 5, 9, 14, [])
    t.tinto_dono("Usuário".encode())
    for o in obras:
        t.tinto_poe_obra(*o)
    # Home › Acervo, pelos botões: a cena entra como a pessoa entra.
    t.tinto_evento(DIR, 0)
    t.tinto_evento(OK, 0)
    for p in passos:
        t.tinto_evento(p, 0)
    t.tinto_redesenha()


def acervo_vazio(t):
    _acervo(t, [])


def acervo_cheio(t):
    _acervo(t, [
        (b"ob:a", "O estrangeiro".encode(), b"Camus", LIVRO, AQUI,
         len(TRECHO), int(len(TRECHO) * 0.38), TRECHO.encode()),
        (b"ob:b", "Manual de campo".encode(), b"", LIVRO, SO_ONLINE,
         4200, 0, b""),
        (b"ob:c", "Contos escolhidos".encode(), b"", LIVRO, AQUI,
         1000, 120, b"Um conto curto. "),
        (b"ob:d", "Notas sobre o tempo".encode(), b"", DOCUMENTO, BAIXANDO,
         3000, 0, b""),
    ])


def acervo_baixando(t):
    _acervo(t, [
        (b"ob:d", "Notas sobre o tempo".encode(), b"", DOCUMENTO, BAIXANDO,
         3000, 0, b""),
        (b"ob:b", "Manual de campo".encode(), b"", LIVRO, SO_ONLINE,
         4200, 0, b""),
    ])


def leitor(t):
    """A obra aberta: o OK sobre a primeira linha entra no texto."""
    _acervo(t, [
        (b"ob:a", "O estrangeiro".encode(), b"Camus", LIVRO, AQUI,
         len(TRECHO), 0, TRECHO.encode()),
    ], (OK,))


def leitor_no_fim(t):
    """A última página PERGUNTA se conclui — nunca conclui sozinha."""
    _acervo(t, [
        (b"ob:a", "O estrangeiro".encode(), b"Camus", LIVRO, AQUI,
         len(TRECHO), 0, TRECHO.encode()),
    ], (OK,) + (DIR,) * 12)


CENAS = [
    ("acervo-vazio",               "acervo vazio",         acervo_vazio),
    ("acervo-cheio",               "estante com os três estados", acervo_cheio),
    ("acervo-baixando",            "download em curso",    acervo_baixando),
    ("leitor",                     "a página",             leitor),
    ("leitor-fim",                 "fim da obra",          leitor_no_fim),
    ("inicio-memoria-ausente",     "memória ausente",      memoria_ausente),
    ("inicio-memoria-comunicacao", "memória não responde", memoria_comunicacao),
    ("inicio-memoria-reparo",      "reparo",               memoria_reparo),
    ("inicio-confirmar-formatar",  "confirmação no não",   memoria_confirmar_formatar),
    ("inicio-confirmar-no-sim",    "confirmação no sim",   memoria_confirmar_no_sim),
    ("inicio-somente-leitura",     "memória protegida",    memoria_somente_leitura),
    ("inicio-memoria-cheia",       "memória cheia",        memoria_cheia),
    ("inicio-boas-vindas",         "boas-vindas",          boas_vindas),
    ("inicio-nome",                "nome do proprietário", nome_proprietario),
    ("inicio-data-hora",           "data e hora",          data_e_hora),
    ("inicio-conclusao",           "conclusão offline",    conclusao),
    ("inicio-rede",                "rede, no primeiro uso", rede_do_primeiro_uso),
    ("inicio-conta-conectando",    "registrando no servidor", conta_conectando),
    ("inicio-conta",               "conectar a conta",      conta_registrado),
    # ── a Home 2x2, e cada cartão em foco ──
    ("home",                 "home · Agenda",   home),
    ("home-acervo",          "home · Acervo",   home_acervo),
    ("home-jogos",           "home · Jogos",    home_jogos),
    ("home-ajustes",         "home · Ajustes",  home_ajustes),
    ("xadrez-jogos",          "Jogos · Xadrez",  xadrez_jogos),
    ("xadrez-modos",          "xadrez · modos", xadrez_modos),
    ("xadrez-preparar-maquina", "xadrez · preparar máquina", xadrez_preparar_maquina),
    ("xadrez-tabuleiro",      "xadrez · tabuleiro", xadrez_tabuleiro),
    ("xadrez-selecao",        "xadrez · peça escolhida", xadrez_selecao),
    ("xadrez-ultima-jogada",  "xadrez · última jogada", xadrez_ultima_jogada),
    ("xadrez-ajuda",          "xadrez · ajuda ativada", xadrez_ajuda),
    ("xadrez-opcoes",         "xadrez · opções", xadrez_opcoes),
    ("xadrez-menu",           "xadrez · menu da partida", xadrez_menu),
    ("xadrez-historico",      "xadrez · histórico", xadrez_historico),
    ("xadrez-horizontal",     "xadrez · horizontal", xadrez_horizontal),
    ("xadrez-resultado",      "xadrez · resultado", xadrez_resultado),
    ("xadrez-resultado-local", "xadrez · resultado local", xadrez_resultado_local),

    # ── a AGENDA ──
    ("agenda-dia-comum",       "agenda · dia comum",       agenda_dia_comum),
    ("agenda-nav-em-foco",     "agenda · navegando o dia", agenda_nav_em_foco),
    ("agenda-sem-compromisso", "agenda · sem compromisso", agenda_sem_compromisso),
    ("agenda-dia-cheio",       "agenda · dia cheio",       agenda_dia_cheio),
    ("agenda-fim-do-dia",      "agenda · fim do dia",      agenda_fim_do_dia),
    ("agenda-dia-vazio",       "agenda · dia vazio",       agenda_dia_vazio),
    ("nota",                 "nota",            nota),
    ("nota-sem-ia",          "nota sem rede",   nota_sem_ia),
    ("capturas",             "capturas do dia", capturas),
    ("dia",                  "o dia",           dia_passado),
    ("agenda-paginas",       "agenda · página 1", agenda_paginas),
    ("agenda-paginas-fim",   "agenda · página 2", agenda_paginas_fim),
    ("calendario",           "calendário",      calendario),
    ("calendario-cheio",     "calendário · dia com os dois", calendario_cheio),
    ("gaveta",               "a gaveta",        gaveta),
    ("ajustes",              "ajustes",         ajustes),
    ("gravando",             "gravando",        gravando),
    ("voz-sem-rede",         "sem rede",        voz_sem_rede),
    ("agenda-sem-rede",      "agenda · sem rede", agenda_sem_rede),
    ("anotacoes",            "anotações · vazia",  anotacoes),
    ("anotacoes-lista",      "anotações · lista",  anotacoes_lista),
    ("anotacoes-aberta",     "anotações · aberta", anotacoes_aberta),
    ("gravando-pausado",     "pausado",         gravando_pausado),
    ("gravando-fora",        "gravando fora",   gravando_fora_da_home),
    ("descartar",            "descartar",       descartar),
    ("dock",                 "o tinto deitado",    dock),
    ("bloqueio",              "repouso",         bloqueio),
    ("bloqueio-mes-longo",    "repouso · setembro", bloqueio_mes_longo),
    ("bloqueio-voz", "voz bloqueada",   bloqueio_voz),
    ("sobre",                "sobre",           sobre),
    ("fala-folgada",         "fala · folgada",  fala_folgada),
    ("fala-no-limite",       "fala · no limite", fala_no_limite),
    ("fala-estourada",       "fala · estourada", fala_estourada),
    ("wifi-senha",           "senha do wi-fi",   wifi_senha),
    ("wifi-lista-cheia",     "redes · paginado", wifi_lista_cheia),
    ("wifi-lista",           "wi-fi · redes",     wifi_lista),
    ("wifi-conectado",       "wi-fi · conectado", wifi_conectado),
    ("conexao-conectando",    "conexão · conectando", conexao_conectando),
    ("conexao-senha-recusada", "conexão · senha recusada", conexao_senha_recusada),
    ("conexao-sem-resposta",  "conexão · sem resposta",  conexao_sem_resposta),
    ("conexao-sem-rede",     "conexões · sem rede", conexao_sem_rede),
    ("conexao-parear",       "conexões · parear",   conexao_parear),
    ("conexao-pareada",      "conexões · pareada",  conexao_pareada),
    ("sincronizacao",        "sincronização",       sincronizacao),
    ("data-e-hora",          "data e hora",         data_e_hora_ajustes),
    ("armazenamento",        "armazenamento",       armazenamento),
    ("recibo",               "recibo offline",  recibo),
    ("recibo-estruturando",  "recibo com rede", recibo_estruturando),
    ("confirma-evento",      "confirmar · evento", confirma_evento),
    ("confirma-fala-longa",  "confirmar · fala longa", confirma_fala_longa),
    ("confirma-tarefa",      "confirmar · tarefa", confirma_tarefa),
    ("confirma-tres",        "confirmar · 3 coisas", confirma_tres),
    ("confirma-tres-topo",   "confirmar · rolado",   confirma_tres_rolado),
    ("resultado",            "resultado · 2 ações",  resultado),
    ("conferir-enviando",    "conferir · enviando",  conferir_enviando),
    ("voz-nao-enviei",       "voz · não enviou",     voz_nao_enviei),
    ("voz-sem-resposta",     "voz · sem resposta",   voz_sem_resposta),
    ("teclado",              "teclado",         teclado),
    ("nota-acoes",           "ações da nota",   nota_acoes),
    ("fala-original",        "a fala que originou",  fala_original),
    ("tarefa-detalhe",       "detalhe da tarefa",    tarefa_detalhe),
    ("tarefa-detalhe-feita", "detalhe · concluída",  tarefa_detalhe_feita),
    ("tarefa-detalhe-rolado","detalhe · rolado",     tarefa_detalhe_rolado),
    ("destino-daqui",        "destino · nasceu aqui",  nota_evento_daqui),
    ("mudar-a-hora",         "mudar a hora",           mudar_a_hora),
    ("destino-google",       "destino · veio do Google", nota_evento_do_google),
    ("evento-dia-inteiro",   "evento de dia inteiro", evento_dia_inteiro),
    ("agenda-dia-inteiro",   "agenda · dia inteiro",  agenda_dia_inteiro),
    ("sem-cartao",           "sem cartão",      sem_cartao),
    ("aparencia",            "aparência",       aparencia),
    ("som",                  "som",             som),
    ("ajustes-raiz",         "ajustes · a raiz", ajustes_raiz),
    ("dia-cheio",            "dia · paginado", dia_tela_cheia),
    ("preparando",           "operação bloqueante", preparando),
    ("sinc-lista",           "sincronização · agendas", sinc_lista),
    ("sinc-paginas",         "sincronização · páginas", sinc_paginas),
    ("sinc-buscando",        "sincronização · buscando", sinc_buscando),
    ("sinc-sem-rede",        "sincronização · sem internet", sinc_sem_rede),
    ("barra-com-rede",       "barra · o leque",  barra_com_rede),
    ("conta-conectada",      "minha conta · conectada", conta_conectada),
    ("conta-sem-rede",       "minha conta · sem rede",  conta_sem_rede),
    ("conta-desconectar",    "minha conta · sair",      conta_desconectar),
    ("conta-confirma",       "desconectar · confirmar", conta_confirma),
]


def quadro(t):
    l, a = ctypes.c_int(0), ctypes.c_int(0)
    p = t.tinto_tela(ctypes.byref(l), ctypes.byref(a))
    n = ((l.value + 7) // 8) * a.value
    return bytes(p[:n]), l.value, a.value
