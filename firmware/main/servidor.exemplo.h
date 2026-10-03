// servidor.exemplo.h — o endereço do SEU servidor. Copie e preencha:
//
//     cp firmware/main/servidor.exemplo.h firmware/main/servidor.h
//     make qr        # o QR do aplicativo, mesmo endereço em bitmap
//
// `servidor.h` está no `.gitignore`: um endereço no repositório apontaria o
// Tinto de quem clonar para a máquina de quem publicou.
//
// O endereço do backend, com `https://` e sem barra no fim. É só o padrão
// de fábrica: o aparelho lê `/TINTO/sistema/nuvem.json` no cartão e só cai
// aqui quando ele não diz nada. Trocar de servidor depois é editar o JSON.
#ifndef TINTO_SERVIDOR_H
#define TINTO_SERVIDOR_H

#define TINTO_SERVIDOR "https://mude-isto.exemplo.com"

#endif
