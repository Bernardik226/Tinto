// nucleo/prazos.h — quanto se espera, e por quê.
//
// Separado de `tipos.h` porque o hal também precisa destes números e não
// inclui o núcleo (`OK`/`ERR_TIMEOUT` colidem com o ESP-IDF). Só números.
#ifndef NUCLEO_PRAZOS_H
#define NUCLEO_PRAZOS_H

// Quanto o socket espera pela nuvem. A captura (transcrever e entender) é o
// pedido mais lento, e o backend responde perto de 60 s.
#define HTTP_PRAZO_MS   (90u * 1000u)

// Quanto a TELA espera: sempre mais que o socket. Ao contrário, ela dizia
// "Sem resposta" (e apagava o áudio) com a requisição ainda viva. A folga é
// o caminho do hal até o quadro.
#define ESTRUTURA_PRAZO_MS (HTTP_PRAZO_MS + 10u * 1000u)

// Quanto o recibo de um gesto ("Remarcado · sex 12 set") fica na tela.
// Qualquer botão o dispensa antes.
#define FEITO_PRAZO_MS  (6u * 1000u)

#endif
