# Montar o seu

O Tinto não tem serviço central: cada pessoa sobe o próprio servidor, com a
própria conta Google e as próprias chaves. A lista de peças e a pinagem estão
no [HARDWARE](HARDWARE.md). Montando com um agente de IA? Ele começa por
[AGENTES](AGENTES.md).

## 1. O servidor

O servidor é um container Docker e roda em qualquer lugar que rode um
container (um PaaS, um VPS, um Raspberry Pi em casa). **O contexto de build é
a raiz do repositório**, porque o aplicativo web mora em `pwa/`.

**Google Cloud:**

1. Crie um projeto e ative a **Google Calendar API** e a **Google Tasks API**.
2. Configure a tela de consentimento OAuth e publique-a como **In
   Production** (veja a nota abaixo).
3. Crie um **ID do cliente OAuth** do tipo *Aplicativo da Web*, com o
   redirect `https://SEU-SERVIDOR/oauth/retorno`.

> **Testing ou In Production?** É o estado do seu projeto no Google Cloud.
> Em *Testing*, só entram os e-mails da lista de teste do Google, e o login
> expira a cada 7 dias. Em *In Production*, qualquer conta Google passa pela
> tela do Google (com um aviso de "app não verificado", normal para uso
> pessoal), mas o seu servidor só deixa entrar quem está em `TINTO_CONTAS`.
> Se alguém achar o endereço do seu app, entra com o Google e é recusado.

**Chaves de IA:** uma chave da [Groq](https://console.groq.com) (transcrição)
e uma da [Anthropic](https://console.anthropic.com) (estruturação).

**Variáveis de ambiente** — o modelo comentado está em
[`backend/.env.exemplo`](../backend/.env.exemplo):

| Variável | O quê |
|---|---|
| `GOOGLE_CLIENT_ID`, `GOOGLE_CLIENT_SECRET`, `GOOGLE_REDIRECT` | o cliente OAuth |
| `GROQ_API_KEY`, `ANTHROPIC_API_KEY` | as chaves de IA |
| `TINTO_CONTAS` | quem pode usar o servidor, com o limite de voz de cada um: `voce@gmail.com:120,mae@gmail.com:30` (minutos/mês). **Vazia = ninguém entra** |
| `QUOTA_PADRAO_S` | o limite de voz para quem não tem número na lista (padrão: 30 min) |
| `TINTO_ESTADO` | caminho do arquivo de estado, **dentro de um volume persistente** |

Não há painel de administração: o que acontece na porta (aparelhos novos,
logins recusados, vínculos) aparece no **log do servidor**.

O servidor roda com **um worker só**: o estado é um arquivo JSON, e o
refresh token do Google fica nele. Proteja o volume e configure backup.

Localmente:

```bash
cd backend
python3 -m venv .venv && .venv/bin/pip install -r requisitos.txt
cp .env.exemplo .env      # e preencha
cd .. && make servidor
```

## 2. O firmware

Com o [ESP-IDF 5.3](https://docs.espressif.com/projects/esp-idf/en/v5.3/esp32s3/get-started/)
instalado:

```bash
cp firmware/main/servidor.exemplo.h firmware/main/servidor.h
#   e preencha com o endereço do SEU servidor

make qr firmware                            # gera o QR e compila
idf.py -C firmware -p /dev/ttyACM0 flash monitor
```

O endereço do `servidor.h` é gravado no firmware e vira o **QR** que o
aparelho mostra na hora de vincular a conta.

## 3. O primeiro uso

1. Ligue o aparelho com um microSD. Ele prepara o cartão sozinho.
2. Dê um nome ao aparelho e conecte o Wi-Fi. Ele se registra no seu servidor
   sozinho.
3. Na tela de conta, leia o QR com o celular, entre com o Google (um e-mail
   de `TINTO_CONTAS`) e digite o código que aparece no Tinto. A agenda desce.

Wi-Fi e conta podem ser pulados (▶) e feitos depois em Ajustes; sem Wi-Fi, o
aparelho pede a data e a hora.
