# Tinto — comandos de desenvolvimento, a partir da raiz.
#
# ENGENHARIA §5: dois builds, do MESMO código. Este é o que roda o aparelho
# inteiro no PC — testes, provas e a janela. O build de placa é `idf.py`.
#
#   make test     roda o aparelho e verifica
#   make janela   abre localhost:8080, navegável no teclado
#   make backend  a suíte do backend, contra um Google de mentira
#   make servidor sobe o backend em localhost:8000
#   make lib      só compila libtinto.so
#   make web      a demo do navegador em build/web (precisa de zig)
#   make limpa

CC      := gcc
# -Wno-format-truncation: TRUNCAR É A POLÍTICA (RN-B7), não um acidente.
# "Rejeitar por tamanho faz o dia da pessoa sumir da tela por causa de um
# nome comprido." O snprintf já trunca com segurança e sempre termina a
# string; o aviso existe pra quem NÃO quer truncar, que não é o caso aqui.
# `-fstack-usage`: o GCC escreve um `.su` por objeto dizendo quantos bytes
# de quadro cada função reserva. É o que `firmware/ferramentas/pilha.py` lê — o
# orçamento da memória AUTOMÁTICA, que faltava no dia em que uma vista de
# 5,8 KB na pilha pôs o aparelho em boot loop com o `make ram` verde.
CFLAGS  := -std=c11 -Wall -Wextra -Werror -Wno-format-truncation -O1 -g -MMD -MP -fstack-usage
INC     := -Ifirmware/main -Ifirmware/simulador

BIN     := build/pc/bin
OBJ     := build/pc/obj
PORTA   ?= 8080

# PURO — compila com gcc, sem uma linha de ESP-IDF
FONTES_FIRMWARE := \
  firmware/main/nucleo/tipos.c \
  firmware/main/nucleo/data.c \
  firmware/main/nucleo/leitor.c \
  firmware/main/nucleo/rotina.c \
  firmware/main/nucleo/inicializacao.c \
  firmware/main/jogos/xadrez.c \
  firmware/main/jogos/xadrez_maquina.c \
  firmware/main/dado/xadrez.c \
  firmware/main/dado/json.c \
  firmware/main/dado/cartao.c \
  firmware/main/dado/indice.c \
  firmware/main/dado/acervo.c \
  firmware/main/dado/memoria.c \
  firmware/main/dado/perfil.c \
  firmware/main/dado/log.c \
  firmware/main/dado/rede.c \
  firmware/main/dado/contrato.c \
  firmware/main/dado/nuvem.c \
  firmware/main/tela/bitmap.c \
  firmware/main/tela/motor.c \
  firmware/main/tela/mapa.c \
  firmware/main/tela/fontes.c \
  firmware/main/tela/texto.c \
  firmware/main/tela/icones.c \
  firmware/main/tela/logo_tinto.c \
  firmware/main/tela/qr.c \
  firmware/main/hal/uc8253.c \
  firmware/main/hal/refresco.c \
  firmware/main/uso/uso.c \
  firmware/main/uso/inicializacao.c \
  firmware/main/uso/nuvem.c \
  firmware/main/uso/acervo.c \
  firmware/main/vista/campos.c \
  firmware/main/vista/agenda.c \
  firmware/main/vista/lancador.c \
  firmware/main/vista/gravador.c \
  firmware/main/vista/vazia.c \
  firmware/main/vista/nota.c \
  firmware/main/vista/resultado.c \
  firmware/main/vista/dia.c \
  firmware/main/vista/calendario.c \
  firmware/main/vista/menu.c \
  firmware/main/vista/sobre.c \
  firmware/main/vista/bloqueio.c \
  firmware/main/vista/dock.c \
  firmware/main/vista/anotacoes.c \
  firmware/main/vista/conferir.c \
  firmware/main/vista/quando.c \
  firmware/main/vista/acervo.c \
  firmware/main/vista/obra.c \
  firmware/main/vista/leitor.c \
  firmware/main/vista/teclado.c \
  firmware/main/vista/fala.c \
  firmware/main/vista/wifi.c \
  firmware/main/vista/conexao.c \
  firmware/main/vista/armazenamento.c \
  firmware/main/vista/conta.c \
  firmware/main/vista/cartao.c \
  firmware/main/vista/vincular.c \
  firmware/main/vista/confirma.c \
  firmware/main/vista/relogio.c \
  firmware/main/vista/inicializacao.c \
  firmware/main/vista/xadrez.c \
  firmware/main/ui/agua.c \
  firmware/main/ui/chrome.c \
  firmware/main/ui/faixa.c \
  firmware/main/ui/voz.c \
  firmware/main/ui/gravador.c \
  firmware/main/ui/agenda.c \
  firmware/main/ui/lancador.c \
  firmware/main/ui/vazia.c \
  firmware/main/ui/rolagem.c \
  firmware/main/ui/pagina.c \
  firmware/main/ui/nota.c \
  firmware/main/ui/resultado.c \
  firmware/main/ui/blocos.c \
  firmware/main/ui/dia.c \
  firmware/main/ui/calendario.c \
  firmware/main/ui/ajustes.c \
  firmware/main/ui/cartao.c \
  firmware/main/ui/menu.c \
  firmware/main/ui/bloqueio.c \
  firmware/main/ui/dock.c \
  firmware/main/ui/anotacoes.c \
  firmware/main/ui/vincular.c \
  firmware/main/ui/confirma.c \
  firmware/main/ui/conferir.c \
  firmware/main/ui/hora.c \
  firmware/main/ui/acervo.c \
  firmware/main/ui/obra.c \
  firmware/main/ui/leitor.c \
  firmware/main/ui/teclado.c \
  firmware/main/ui/fala.c \
  firmware/main/ui/relogio.c \
  firmware/main/ui/inicializacao.c \
  firmware/main/ui/xadrez.c \
  firmware/main/app/app.c

FONTES_SIMULADOR := firmware/simulador/hal_pc.c
FONTES_TESTE := firmware/testes/main_teste.c firmware/testes/t_liga.c firmware/testes/t_dado.c firmware/testes/t_texto.c firmware/testes/t_agenda.c firmware/testes/t_uso.c firmware/testes/t_navega.c firmware/testes/t_nota.c firmware/testes/t_voz.c firmware/testes/t_pinos.c firmware/testes/t_eink.c firmware/testes/t_motor.c firmware/testes/t_memoria.c firmware/testes/t_perfil.c firmware/testes/t_onboarding.c firmware/testes/t_fala.c firmware/testes/t_espaco.c firmware/testes/t_wifi.c firmware/testes/t_armazenamento.c firmware/testes/t_conta.c firmware/testes/t_relogio.c firmware/testes/t_rede_salva.c firmware/testes/t_contrato.c firmware/testes/t_rotina.c firmware/testes/t_indice.c firmware/testes/t_nuvem.c firmware/testes/t_acervo.c firmware/testes/t_acervo_nuvem.c firmware/testes/t_acervo_vista.c firmware/testes/t_acervo_ui.c firmware/testes/t_leitor.c firmware/testes/t_acervo_navega.c firmware/testes/t_acervo_contrato.c firmware/testes/t_grid.c firmware/testes/t_mapa_telas.c firmware/testes/t_lancador.c firmware/testes/t_ui_chrome.c firmware/testes/t_faixa.c firmware/testes/t_vazio.c firmware/testes/t_xadrez.c firmware/testes/t_xadrez_maquina.c firmware/testes/t_xadrez_dado.c firmware/testes/t_xadrez_fluxo.c firmware/testes/t_xadrez_ui.c
FONTES_API   := firmware/simulador/api.c

O_FIRMWARE  := $(FONTES_FIRMWARE:%.c=$(OBJ)/%.o)
O_SIMULADOR := $(FONTES_SIMULADOR:%.c=$(OBJ)/%.o)
O_TESTE     := $(FONTES_TESTE:%.c=$(OBJ)/%.o)

CAMADAS_PURAS := firmware/main/nucleo firmware/main/tela firmware/main/dado \
  firmware/main/vista firmware/main/ui firmware/main/uso

.PHONY: all test janela lib web limpa camadas fontes icones logos gerados_conferidos provas firmware backend servidor config sd_sem_vfs

# As fontes são geradas, não escritas. Regerar é barato e o resultado é
# determinístico — então elas ficam no repo pra quem clonar não precisar
# de Pillow só pra compilar.
fontes:
	@python3 firmware/ferramentas/fontes.py

# Os ícones vêm do Pixelarticons (MIT), dos SVGs versionados em assets/.
# Gerar de novo não depende de baixar nada — e o .c gerado vai no repo,
# pra que clonar não exija cairosvg.
icones:
	@python3 firmware/ferramentas/icones.py

# A marca sai do asset oficial, nunca redesenhada. O SHA da fonte vai no .c
# gerado: trocar o arquivo sem regerar falha alto em vez de o aparelho
# desenhar uma marca que não é a marca.
logos:
	@python3 firmware/ferramentas/logo_tinto.py

# ── o QR do aplicativo ───────────────────────────────────────────────
#
# Ele NÃO vai versionado, e a razão é que o bitmap não é genérico: ele é o
# endereço do seu servidor, codificado. Publicá-lo é publicar o endereço.
#
# Quem clona copia `servidor.exemplo.h`, preenche, e roda isto uma vez.
qr: firmware/main/tela/qr.c

firmware/main/tela/qr.c: firmware/main/servidor.h firmware/ferramentas/qr.py
	@backend/.venv/bin/python firmware/ferramentas/qr.py 2>/dev/null \
	  || python3 firmware/ferramentas/qr.py

firmware/main/servidor.h:
	@echo "  falta firmware/main/servidor.h — o endereço do SEU servidor."
	@echo "    cp firmware/main/servidor.exemplo.h firmware/main/servidor.h"
	@echo "  e preencha o endereço. Ver README.md."
	@false

# Os ícones do Tinto App. Não são desenho novo: são o pingo do 'i' da
# marca — que já é um quadrado com um ponto dentro — sobre o papel de um
# e-ink. Pillow mora no venv do backend, e não no Python do ESP-IDF.
icones_pwa:
	@backend/.venv/bin/python pwa/ferramentas/icone_pwa.py
	@backend/.venv/bin/python pwa/ferramentas/logo_pwa.py

all: test

# ── testes ───────────────────────────────────────────────────────────
$(BIN)/testes: $(O_FIRMWARE) $(O_SIMULADOR) $(O_TESTE) | $(BIN)
	$(CC) $(CFLAGS) $^ -o $@

test: config gerados_conferidos sd_sem_vfs qr $(BIN)/testes camadas pilha hora_num_lugar_so
	@./$(BIN)/testes

gerados_conferidos:
	@python3 firmware/ferramentas/logo_tinto.py --check
	@backend/.venv/bin/python pwa/ferramentas/icone_pwa.py --check 2>/dev/null \
	  || python3 pwa/ferramentas/icone_pwa.py --check
	@backend/.venv/bin/python pwa/ferramentas/logo_pwa.py --check 2>/dev/null \
	  || python3 pwa/ferramentas/logo_pwa.py --check

config:
	@grep -qx 'CONFIG_FATFS_LFN_STACK=y' firmware/sdkconfig.defaults
	@grep -qx 'CONFIG_FATFS_MAX_LFN=64' firmware/sdkconfig.defaults
	@grep -qx 'CONFIG_FATFS_API_ENCODING_UTF_8=y' firmware/sdkconfig.defaults
	@grep -qx '\# CONFIG_FATFS_LFN_HEAP is not set' firmware/sdkconfig.defaults
	@grep -qx '\# CONFIG_FATFS_LFN_NONE is not set' firmware/sdkconfig.defaults
	@grep -qx '\# CONFIG_ESP_WIFI_IRAM_OPT is not set' firmware/sdkconfig.defaults
	@grep -qx '\# CONFIG_ESP_WIFI_RX_IRAM_OPT is not set' firmware/sdkconfig.defaults
	@if [ -f firmware/sdkconfig ]; then \
		grep -qx 'CONFIG_FATFS_LFN_STACK=y' firmware/sdkconfig && \
		grep -qx 'CONFIG_FATFS_MAX_LFN=64' firmware/sdkconfig && \
		grep -qx 'CONFIG_FATFS_API_ENCODING_UTF_8=y' firmware/sdkconfig && \
		grep -qx '\# CONFIG_FATFS_LFN_HEAP is not set' firmware/sdkconfig && \
		grep -qx '\# CONFIG_FATFS_LFN_NONE is not set' firmware/sdkconfig && \
		grep -qx '\# CONFIG_ESP_WIFI_IRAM_OPT is not set' firmware/sdkconfig && \
		grep -qx '\# CONFIG_ESP_WIFI_RX_IRAM_OPT is not set' firmware/sdkconfig; \
	fi

# ENGENHARIA §3: stdio e o VFS FatFs alocam durante fopen/opendir. Depois do
# boot, o SD usa FatFs direto com FIL/FF_DIR/FILINFO locais.
sd_sem_vfs:
	@! grep -nE '\<(fopen|fread|fwrite|fflush|fclose|fsync|fileno|opendir|readdir|closedir|stat|mkdir|rename|remove)\>[[:space:]]*\(' \
		firmware/main/hal/hal_sd.c \
		|| (echo "  ✗ I/O do SD voltou a passar por stdio/VFS"; exit 1)

# ── a biblioteca que a janela carrega ────────────────────────────────
# Os headers entram como dependência: a lib é compilada num comando só, sem
# .d, então mudar um .h não disparava rebuild — e a prova em PNG mostrava
# código velho enquanto `make test` já mostrava o novo.
CABECALHOS := $(shell find firmware/main firmware/simulador -name '*.h' 2>/dev/null)

lib: $(BIN)/libtinto.so

# Os cabeçalhos são DEPENDÊNCIA, não entrada do compilador: passá-los ao
# gcc faz ele pré-compilar cada um, e um header que não compila sozinho
# derruba a saída inteira em silêncio — a .so saía com "invalid ELF
# header", que não diz nada sobre a causa.
$(BIN)/libtinto.so: $(CABECALHOS) $(FONTES_FIRMWARE) $(FONTES_SIMULADOR) $(FONTES_API) | $(BIN)
	$(CC) $(CFLAGS) -fPIC -shared $(INC) $(filter %.c,$^) -o $@

janela: lib
	@python3 firmware/simulador/janela.py $(PORTA)

# ── a demo do navegador ──────────────────────────────────────────────
# O mesmo C da libtinto.so, para WebAssembly. Nenhuma função de WASI sobra
# no binário (o firmware não abre arquivo nem lê relógio do sistema), então
# a página o carrega sem shim. O QR e o servidor saem do servidor.h de quem
# compila: a demo pública é gerada no CI, com o endereço de mentira.
ZIG      ?= zig
WEB      := build/web
EXPORTA  := $(shell grep -oE '^[a-z][a-z ]* \**tinto_[a-z_0-9]+' $(FONTES_API) \
              | grep -oE 'tinto_[a-z_0-9]+' | sed 's/^/-Wl,--export=/')

web: $(WEB)/tinto.wasm
	cp firmware/simulador/web/index.html firmware/simulador/web/tinto.js $(WEB)/
	@echo "  $(WEB) pronto — python3 -m http.server -d $(WEB) $(PORTA)"

$(WEB)/tinto.wasm: $(CABECALHOS) $(FONTES_FIRMWARE) $(FONTES_SIMULADOR) $(FONTES_API)
	@mkdir -p $(WEB)
	$(ZIG) cc -target wasm32-wasi -mexec-model=reactor -O2 -std=c11 \
	  -Wno-format-truncation $(INC) \
	  $(filter %.c,$^) $(EXPORTA) -Wl,--export=malloc -Wl,--export=free -o $@

# A prova em PNG: comparar com o desenho SEM gravar a placa.
provas: lib
	@python3 firmware/simulador/prova.py

# O build embarcado continua inteiramente dentro de firmware/.
firmware:
	idf.py -C firmware build

# ── o backend ────────────────────────────────────────────────────────
# Outra base de código, com as camadas dela. A regra de ouro é a mesma:
# nada toca o Google de verdade num teste. `backend/testes/fora.py` é o mundo lá
# fora — Calendar, Tasks, Whisper e a LLM —, escrito a partir da
# documentação, e é lá que se conserta quando o Google mudar.
PY_BACKEND ?= backend/.venv/bin/python

backend:
	@$(PY_BACKEND) -m pytest backend/testes -q

# Com `--reload`, e o pareamento sobrevive: `TINTO_ESTADO` guarda conta e
# refresh token em disco. Sem ele, cada linha de Python salva mandava
# você pegar o celular de novo.
servidor:
	@cd backend && TINTO_ESTADO=$${TINTO_ESTADO:-estado.json} \
	  .venv/bin/python -m uvicorn tinto.app:app --reload --port 8000

# ── a fronteira das camadas, verificada pelo build ───────────────────
# CAMADAS §1: nucleo, tela e vista não conhecem ESP-IDF nem o cartão.
# Isto é o que transforma a regra em erro de build em vez de combinado.
camadas:
	@! grep -rlE '#include *[<"](esp_|freertos|driver/)' \
	    $(CAMADAS_PURAS) 2>/dev/null \
	  || (echo "  ✗ camada pura incluindo ESP-IDF"; exit 1)
	@! grep -rn '^static ' firmware/main/nucleo firmware/main/tela 2>/dev/null | grep -v '\.c:' \
	  || true
	@! grep -rn '#include *"\.\./app/' \
	    $(CAMADAS_PURAS) 2>/dev/null \
	  || (echo "  ✗ camada de baixo incluindo app/ — a seta aponta pra cima"; exit 1)

# A hora tem UM formatador (`nucleo/data.c`), e o ajuste 12/24 h só vale no
# sistema todo enquanto isso for verdade. Vinte e seis telas escreveram
# "%02d:%02d" na unha por meses, e o interruptor de Ajustes não fazia nada.
#
# O fuso é a exceção declarada: "-03:00" é deslocamento, não hora do dia.
hora_num_lugar_so:
	@! grep -rn '%02d:%02d' firmware/main/vista firmware/main/ui 2>/dev/null \
	    | grep -v 'fuso' \
	  || (echo "  ✗ hora escrita na unha — use vista_hora_da_barra ou vista_hora_do_item"; exit 1)

# ── plumbing ─────────────────────────────────────────────────────────
$(OBJ)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(BIN):
	@mkdir -p $(BIN)

limpa:
	rm -rf $(OBJ) $(BIN) $(WEB)

-include $(shell find $(OBJ) -name '*.d' 2>/dev/null)

# ── o orçamento de RAM interna ───────────────────────────────────────
#
# Ela é a única que o DMA alcança, e a falta dela não quebra quem gastou:
# um buffer estático a mais deixou o driver do cartão sem DMA, e o
# aparelho abriu a tela de reparo da memória com o cartão intacto.
# ── tudo o que precisa passar antes de um push ───────────────────────
#
# Os 528 testes do PC ficaram verdes sobre um
# código que NÃO compilava para a placa: `snprintf` com buffers
# sobrepostos, que o GCC do ESP recusa e o do PC deixa passar. A CI pegou;
# eu é que empurrei sem olhar.
#
# O PC e a placa não compilam com o mesmo compilador nem com a mesma
# otimização, e nenhuma flag faz os dois concordarem sobre tudo. O que faz
# é compilar os dois — e é isto.
# BASH explícito: o `export.sh` do IDF não roda no `sh` que o make usa por
# padrão, e o alvo falhava dizendo que o firmware não compila quando o que
# não compilava era o próprio comando.
#
# E `reconfigure` antes do build: o CMakeLists pega as camadas puras por
# GLOB, e glob não nota arquivo novo sozinho. Sem isto, uma vista nova
# compila no PC, passa nos testes, e falha no LINK da placa — que é o
# lugar mais chato de descobrir qualquer coisa.
tudo: test
	@bash -c '. $$HOME/esp/esp-idf/export.sh >/dev/null 2>&1; idf.py -C firmware reconfigure >/dev/null 2>&1; idf.py -C firmware build >/dev/null 2>&1' && echo "  firmware  ok" || (echo "  ✗ o firmware NÃO compila — rode: idf.py -C firmware build"; exit 1)
	@bash -c '. $$HOME/esp/esp-idf/export.sh >/dev/null 2>&1; python3 firmware/ferramentas/ram.py'
	@$(MAKE) -s pilha

ram:
	@python3 firmware/ferramentas/ram.py

# ── e o orçamento da PILHA ───────────────────────────────────────────
#
# O `ram` mede a memória ESTÁTICA. Este mede a AUTOMÁTICA, que era o ponto
# cego: uma struct que cresce não aparecia em lugar nenhum até o aparelho
# reiniciar no vidro. Ver firmware/ferramentas/pilha.py.
pilha:
	@python3 firmware/ferramentas/pilha.py

.PHONY: ram pilha tudo
