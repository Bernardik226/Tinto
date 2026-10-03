# Para agentes de IA

Você está ajudando alguém a montar o próprio Tinto. Guie em passos curtos,
um de cada vez, e confirme cada um antes do próximo. O passo a passo completo
é o [MONTAR](MONTAR.md); isto é o mapa.

## A ordem

1. **Google Cloud:** projeto, Calendar API e Tasks API ativas, tela de
   consentimento em *In Production*, cliente OAuth *Aplicativo da Web* com o
   redirect `https://SEU-SERVIDOR/oauth/retorno`. É a parte mais demorada:
   vá tela por tela.
2. **Chaves:** uma da Groq e uma da Anthropic.
3. **Servidor:** o `Dockerfile` da raiz (o contexto de build é a raiz), com
   as variáveis de [`backend/.env.exemplo`](../backend/.env.exemplo) e
   `TINTO_ESTADO` num volume persistente.
4. **Firmware:** copiar `firmware/main/servidor.exemplo.h` para
   `servidor.h`, pôr o endereço do servidor e rodar `make qr firmware`.
5. **Primeiro uso:** nome, Wi-Fi, e o QR. No celular, entrar com o Google e
   digitar o código de 6 dígitos que o Tinto mostra.

## Onde costuma travar

- `GOOGLE_REDIRECT` tem que ser **igual**, letra por letra, ao redirect
  cadastrado no Google Cloud.
- `TINTO_CONTAS` vazia = ninguém entra. O e-mail de quem vai usar precisa
  estar lá.
- *Testing* expira o login em 7 dias; use *In Production*.
- O servidor roda com **um worker só**; não aumente.
- O QR sai do `servidor.h` por `make qr`, que precisa do pacote Python
  `segno`. Sem o `servidor.h`, o aparelho não sabe com quem falar.
- O que acontece na porta (aparelho novo, login recusado) aparece no **log
  do servidor**.

## Sem placa

`make janela` roda o aparelho inteiro no navegador, sem servidor nem Google.
Bom para a pessoa conhecer antes de comprar as peças.
