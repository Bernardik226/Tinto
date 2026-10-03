// A ponte do navegador: o mesmo firmware que a janela carrega por ctypes,
// aqui como WebAssembly. `t.tinto_x(...)` chama a função C de mesmo nome;
// string vira char* no heap do módulo e é liberada na volta.

export const NADA = 0, ANOTACAO = 1, TAREFA = 2, LISTA = 3, EVENTO = 4;
// entrada_t do hal/hal.h: a ordem importa.
export const CIMA = 1, BAIXO = 2, ESQ = 3, DIR = 4, OK = 5, MENU = 6,
             VOLTAR = 7, VOZ = 8, POWER = 9;

export async function carrega(bytes) {
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const x = instance.exports;
  x._initialize();
  const enc = new TextEncoder();

  const t = {};
  for (const nome of Object.keys(x).filter(n => n.startsWith('tinto_')))
    t[nome] = (...args) => {
      const soltos = [];
      const a = args.map(v => {
        if (typeof v !== 'string') return v;
        const b = enc.encode(v + '\0'), p = x.malloc(b.length);
        new Uint8Array(x.memory.buffer, p, b.length).set(b);
        soltos.push(p);
        return p;
      });
      try { return x[nome](...a); } finally { soltos.forEach(x.free); }
    };

  const la = x.malloc(8);
  t.quadro = () => {
    const p = x.tinto_tela(la, la + 4);
    const [l, a] = new Int32Array(x.memory.buffer, la, 2);
    return { l, a, bits: new Uint8Array(x.memory.buffer, p, ((l + 7) >> 3) * a) };
  };
  return t;
}

// ── os cenários ─────────────────────────────────────────────────────
// Recorte de cenas.py: o que mostra o aparelho em 10 segundos. Os mesmos
// dados, para a demo e a janela contarem o mesmo dia.
const SEM = [0, 0, 0];

function monta(t, ano, mes, dia, hora, minuto, itens) {
  t.tinto_liga();
  t.tinto_limpa_itens();
  t.tinto_relogio(ano, mes, dia, hora, minuto);
  for (const [titulo, tipo, h, venc, feita, fora] of itens)
    t.tinto_poe_item(titulo, tipo, h || '', venc[0], venc[1], venc[2], feita, fora);
  t.tinto_dono('Usuário');
  t.tinto_rede(1);     // falar exige rede: a demo finge que tem
  t.tinto_redesenha();
}

function dia_comum(t) {
  monta(t, 2026, 8, 12, 9, 14, [
    ['Dentista',                 EVENTO, '14:00', SEM,          0, 1],
    ['Aula de finlandês',        EVENTO, '19:30', SEM,          0, 1],
    ['mandar histórico p/ Oulu', TAREFA, null, [2026, 8, 10],   0, 1],
    ['revisar PR do expansor',   TAREFA, null, SEM,             0, 0],
    ['comprar pasta térmica',    TAREFA, null, SEM,             0, 1],
    ['pagar internet',           TAREFA, null, SEM,             1, 1],
    ['ideia do painel',          ANOTACAO, null, SEM,           0, 0],
    ['pasta térmica',            ANOTACAO, null, SEM,           0, 0],
  ]);
}

function dia_cheio(t) {
  monta(t, 2026, 8, 14, 8, 15, [
    ['Daily do time',            EVENTO, '09:00', SEM,          0, 1],
    ['Revisão de arquitetura',   EVENTO, '11:00', SEM,          0, 1],
    ['Reunião com o cliente',    EVENTO, '14:30', SEM,          0, 1],
    ['Academia',                 EVENTO, '18:00', SEM,          0, 1],
    ['mandar histórico p/ Oulu', TAREFA, null, [2026, 8, 10],   0, 1],
    ['responder o convite',      TAREFA, null, [2026, 8, 13],   0, 1],
    ['revisar PR do expansor',   TAREFA, null, SEM,             0, 0],
    ['comprar pasta térmica',    TAREFA, null, SEM,             0, 0],
    ['trocar o óleo',            TAREFA, null, SEM,             0, 0],
    ['ler o datasheet do PCF',   TAREFA, null, SEM,             0, 0],
    ['uma captura',              ANOTACAO, null, SEM,           0, 0],
  ]);
  t.tinto_evento(OK, 0);
}

function sem_compromisso(t) {
  monta(t, 2026, 8, 13, 9, 2, [
    ['mandar histórico p/ Oulu', TAREFA, null, [2026, 8, 10], 0, 1],
    ['revisar PR do expansor',   TAREFA, null, SEM,           0, 0],
    ['comprar pasta térmica',    TAREFA, null, SEM,           0, 0],
  ]);
  t.tinto_evento(OK, 0);
}

function calendario(t) {
  monta(t, 2026, 8, 12, 7, 41, [
    ['Dentista',               EVENTO,   '14:00', SEM, 0, 1],
    ['Aula de finlandês',      EVENTO,   '19:30', SEM, 0, 1],
    ['ideia do painel',        ANOTACAO, '07:12', SEM, 0, 0],
    ['o que ficou da reunião', ANOTACAO, '09:41', SEM, 0, 0],
  ]);
  for (const [dia, tipo, fora] of [[3, EVENTO, 1], [5, ANOTACAO, 0], [6, EVENTO, 1],
      [6, ANOTACAO, 0], [10, EVENTO, 1], [11, ANOTACAO, 0], [13, EVENTO, 1],
      [18, EVENTO, 1], [21, EVENTO, 1]])
    t.tinto_poe_item_em(2026, 8, dia, 'x', tipo, fora);
  t.tinto_evento(OK, 0);
  t.tinto_evento(MENU, 0);
  t.tinto_evento(OK, 0);
}

// A voz, até o Conferir. Sem microfone nem IA: o "áudio" são os segundos
// que passam, e a nuvem responde sempre a mesma frase, pelo mesmo parser
// que lê o servidor de verdade.
const ENTENDEU = JSON.stringify({
  falou: 'marca reunião com o cliente quinta às três', nota: 'n1',
  acoes: [{ v: 'criou', id: 'n:1', tp: EVENTO, t: 'Reunião com o cliente',
            d: '2026-08-14', h: '15:00' }],
});

export function fala(t) {
  t.tinto_evento(VOZ, 0);
  t.tinto_nuvem_responde(ENTENDEU);
}

function voz(t) {
  dia_comum(t);
  t.tinto_evento(OK, 0);
  fala(t);
  for (let i = 0; i < 6; i++) t.tinto_tick();
}

const TRECHO = ('Continuei até a esquina. O céu estava límpido e a rua, ' +
  'silenciosa. Pensei em voltar, mas a inquietação não me deixava parar. ' +
  'Caminhei mais um quarteirão, contando os passos, até a avenida ' +
  'terminar. ').repeat(6);
const LIVRO = 0, DOCUMENTO = 1, SO_ONLINE = 0, BAIXANDO = 1, AQUI = 2;

function acervo(t, obras, passos = []) {
  monta(t, 2026, 9, 5, 9, 14, []);
  for (const o of obras) t.tinto_poe_obra(...o);
  t.tinto_evento(DIR, 0);
  t.tinto_evento(OK, 0);
  for (const p of passos) t.tinto_evento(p, 0);
  t.tinto_redesenha();
}

const OBRAS = [
  ['ob:a', 'O estrangeiro', 'Camus', LIVRO, AQUI, TRECHO.length, 0, TRECHO],
  ['ob:b', 'Manual de campo', '', LIVRO, SO_ONLINE, 4200, 0, ''],
  ['ob:c', 'Contos escolhidos', '', LIVRO, AQUI, 1000, 120, 'Um conto curto. '],
  ['ob:d', 'Notas sobre o tempo', '', DOCUMENTO, BAIXANDO, 3000, 0, ''],
];

function xadrez(t) {
  monta(t, 2026, 8, 12, 9, 14, []);
  for (const p of [BAIXO, OK, OK, BAIXO, OK, BAIXO, OK]) t.tinto_evento(p, 0);
}

export const CENAS = [
  ['home',             t => monta(t, 2026, 8, 12, 9, 14, [
                         ['Dentista', EVENTO, '14:00', SEM, 0, 1],
                         ['revisar PR do expansor', TAREFA, null, SEM, 0, 0]])],
  ['dia comum',        t => { dia_comum(t); t.tinto_evento(OK, 0); }],
  ['dia cheio',        dia_cheio],
  ['sem compromisso',  sem_compromisso],
  ['calendário',       calendario],
  ['voz',              voz],
  ['acervo',           t => acervo(t, OBRAS)],
  ['leitor',           t => acervo(t, OBRAS.slice(0, 1), [OK])],
  ['xadrez',           xadrez],
];
