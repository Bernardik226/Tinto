// A demo antes de publicar: cada cena desenha, e a voz chega ao Conferir
// e ao Resultado pelo caminho da nuvem.
//
//     node firmware/simulador/web/prova.mjs build/web/tinto.wasm
import fs from 'fs';
import assert from 'assert';
import { carrega, CENAS, fala, OK, BAIXO } from './tinto.js';

// Os números vêm do HEADER, como em cenas.py: enum copiado à mão erra calado.
const h = fs.readFileSync(new URL('../../main/nucleo/estado.h', import.meta.url), 'utf8');
const bloco = h.slice(h.indexOf('TELA_AGENDA = 0,'), h.indexOf('} tela_id;'));
const TELA = Object.fromEntries(
  [...bloco.matchAll(/^\s*(TELA_[A-Z_0-9]+)\s*(?:=\s*\d+)?\s*,/gm)].map((m, i) => [m[1], i]));

const t = await carrega(fs.readFileSync(process.argv[2]));

for (const [nome, f] of CENAS) {
  f(t);
  assert(t.quadro().bits.some(b => b), `cena "${nome}" em branco`);
}

CENAS[0][1](t);
t.tinto_evento(OK, 0);                       // a Agenda
fala(t);
for (let i = 0; i < 5; i++) t.tinto_tick();
t.tinto_evento(OK, 0);                       // terminei
assert.equal(t.tinto_tela_id(), TELA.TELA_CONFERIR);
t.tinto_evento(BAIXO, 0);
t.tinto_evento(OK, 0);                       // Marcar na agenda
assert.equal(t.tinto_tela_id(), TELA.TELA_RESULTADO);

console.log(`demo ok · ${CENAS.length} cenas · voz até o Resultado`);
