# Hardware

A placa, as peças, a pinagem e a energia do Tinto 1.0.

O firmware nunca fala com componente, fala com **papel**: `entrada` entrega
`IN_CIMA`, não "GPIO 5 em LOW". Trocar uma peça é trocar o `hal/`, e o resto
do sistema não fica sabendo.

---

## 1. A placa

**Freenove ESP32-S3-WROOM CAM**, com o módulo **ESP32-S3-WROOM-1 N16R8**
(16 MB de flash, 8 MB de PSRAM octal) e dois USB-C (nativo e UART).

A câmera da placa **não é usada**: a FPC fica desconectada e o driver nunca
inicializa. Isso libera os GPIO 4–18 para o microfone e os botões.

---

## 2. Lista de peças

| Bloco | Peça | Observação |
|---|---|---|
| Placa | Freenove ESP32-S3-WROOM CAM N16R8 | SD-MMC no próprio slot da placa |
| Tela | E-ink 3.7" P&B, 240×416 — **WeAct GDEY037T03** (controlador UC8253) | refresh parcial, sem backlight |
| Microfone | **INMP441** (I2S) | ver §4 |
| Navegação | joystick de 5 vias | cima, baixo, esquerda, direita, OK |
| Botões | voz · power · MENU · BACK | ver §3 |
| Expansor | **PCF8575** (16 bits, I2C) | todos os botões menos o power |
| Memória | microSD no slot da placa | SD-MMC 1-bit |
| Energia | USB-C da placa | bateria fica para a v2 (§12) |

---

## 3. Pinagem

A única fonte de número de pino é [`firmware/main/pins.h`](../firmware/main/pins.h).
Fora dele, um `grep GPIO_NUM_` tem que voltar vazio.

### Proibidos

| GPIO | Motivo |
|---|---|
| 26–37 | flash SPI e PSRAM octal do N16R8 — usar gera falha intermitente |
| 19, 20 | USB nativo |
| 43, 44 | UART0, a saída de log |
| 0, 3, 45, 46 | strapping de boot |

### Alocação

| Função | GPIO |
|---|---|
| E-ink SCK / MOSI | 41 / 42 |
| E-ink CS / DC / RST / BUSY | 1 / 21 / 14 / 47 |
| SD-MMC CMD / CLK / D0 | 38 / 39 / 40 |
| INMP441 SCK / WS / SD | 5 / 6 / 4 (`L/R` em GND) |
| Botão power | 2 (RTC-GPIO: acorda e desbloqueia) |
| I2C SDA / SCL | 12 / 13 |
| INT do PCF8575 | 9 (RTC-GPIO, pull-up externo de 10 kΩ) |
| Reservados ao cristal de 32.768 kHz | 15 / 16 (fixos no silício) |
| Livres | 7, 8, 10, 11, 17, 18, 48 |

### Os botões no PCF8575

São nove entradas, e não há GPIO para todas. A divisão é por **quem pode
desbloquear**: só o power tem pino próprio e acorda o aparelho já
desbloqueado. Os outros oito ficam no expansor, cujo INT comum acorda o chip
sem desbloquear.

| Porta | Botão |
|---|---|
| P00 – P04 | cima / baixo / esquerda / direita / OK |
| P05 / P06 | MENU / BACK |
| P10 | voz |
| P07, P11 – P17 | livres (P13 reservado ao sensor da dock) |

O endereço I2C do PCF8575 é `0x20` nesta montagem (A0–A2 em GND). Se o seu for
outro, ajuste `ENDERECO_PCF8575` no `pins.h`.

O expansor é **um CI e zero resistores**, lê botões simultâneos e avisa por
interrupção, sem laço de leitura. Num aparelho que passa o dia parado, isso
é energia.

---

## 4. O microfone

**Um INMP441, 16 kHz, mono, 16 bits** (32 KB/s), gravado cru no cartão como
WAV. Nada de ganho automático ou noise gate na captura: processamento
destrutivo na gravação não tem volta.

O que mais melhora a transcrição, em ordem:

1. distância do mic à boca — o gesto de segurar o botão já resolve;
2. ganho certo, sem clipar;
3. **isolamento mecânico** — o INMP441 é MEMS e escuta vibração de
   estrutura. Montado rígido, o clique do botão vira um "toc" no começo de
   toda nota. Monte em espuma ou silicone, longe do botão de voz.

O pino `L/R` permite um segundo INMP441 no mesmo barramento, sem custar
GPIO.

---

## 5. Áudio: só entra, não sai

```
INMP441 --[I2S0 RX, mono]--> ESP32-S3 --> WAV no cartão --> servidor
```

O Tinto grava e envia; não reproduz. Uma nota falada é lida na tela, e o
ESP32-S3 não tem DAC — saída de som exigiria um CI externo no I2S1.

---

## 6. O relógio

O ESP32-S3 tem RTC interno, mas o oscilador RC padrão erra cerca de uma hora
por dia. Na 1.0 a hora vem do **NTP** a cada conexão, o que basta para um
aparelho que sincroniza a agenda.

Os GPIO 15 e 16 ficam reservados para um **cristal de 32.768 kHz** (erro de
~1,7 s/dia), necessário quando houver bateria e deep sleep longo.

---

## 7. O cartão SD

O slot da própria placa, em SD-MMC 1-bit, num barramento só dele — sem
dividir SPI com o e-ink e sem componente a mais. Trocar o cartão exige abrir
o aparelho; com o aplicativo web enviando arquivos, isso é raro.

A estrutura de pastas e a escrita atômica estão no [SISTEMA §7](SISTEMA.md).

---

## 10. Memória

| Memória | Tamanho | Uso |
|---|---|---|
| RAM interna | 512 KB | DMA, ISR, pilhas, TLS, driver do cartão, Wi-Fi |
| PSRAM | 8 MB | buffers grandes e temporários |
| Flash | 16 MB, `ota_0` e `ota_1` de 3 MB | código |
| Cartão | o que couber | os dados da pessoa |

**Buffer grande e temporário mora na PSRAM.** A RAM interna é de quem não
tem escolha: um buffer `static` de 16 KB já foi suficiente para deixar o
driver do cartão sem DMA. `mem_emprestada` (`hal/memoria_hal.c`) tenta a
PSRAM primeiro.

`make ram` soma `.dram0.bss` e `.iram0.bss` e falha acima de 128 KB.

As duas partições de app têm o mesmo tamanho de propósito: é o que permite
atualização OTA com volta.

---

## 11. Energia e bloqueio

| Estado | Consumo aproximado |
|---|---|
| Acordado, tela parada | ~40–50 mA |
| Refresh de e-ink | picos de menos de 1 s |
| Wi-Fi transmitindo | ~150–300 mA em picos |
| Gravando (I2S + cartão) | ~60–90 mA |

E-ink parado é praticamente de graça, e **segura a imagem sem energia**. O
que domina o consumo é quantas vezes o rádio acorda, e por isso a
sincronização é por pull periódico, não por conexão aberta.

Na 1.0, alimentada por USB, **o aparelho não dorme**: o power **trava e
destrava a tela**. Travada, ela mostra a hora, a data e o dia, e só o power
destrava; o botão de voz avisa para desbloquear. Sem tocar em nada por
**N minutos** (Ajustes → Hora e tela → "Bloquear após"), a tela trava
sozinha — nunca com uma fala aberta e nunca no primeiro uso.

No primeiro uso o power também só trava e destrava, e a etapa em que se
estava continua ali.

Três regras: nada de polling apertado; escrever no cartão custa energia, então
se escreve por lote; e o Wi-Fi liga, faz o que tem que fazer, e desliga.

---

## 12. O que fica para a v2

- **Bateria** LiPo com carregador com power-path (BQ24074 ou MCP73871 — o
  TP4056 comum carrega e descarrega a célula ao mesmo tempo) e **fuel
  gauge** MAX17048 no mesmo I2C. O firmware já lê o gauge se ele responder
  em `0x36`, e mostra a barra vazia se não.
- **Energia na bateria:** segurar o power ~3 s desliga (deep sleep); travar
  na bateria passa a dormir de verdade, acordando pelo power e pelo INT do
  PCF8575; aviso de bateria baixa e tela de bateria esgotada antes de
  desligar. Opcional: modos da tela de bloqueio (simples ou completa).
- **Cristal de 32.768 kHz** nos GPIO 15/16.
- **Dock magnética** com contatos de carga e sensor hall (PCF8575 P13).
- **LED** de gravação, usando o LED endereçável da própria placa.
- **Câmera** com dithering para o e-ink e **impressora térmica** na dock.
- **Buzzer**.
- Case impresso e **PCB própria** com o módulo WROOM-1 soldado.
