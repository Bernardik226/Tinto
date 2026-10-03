/* Tinto — o PWA.
 *
 * Uma árvore de rotas só: `conta`, `acervo`, `acervo/adicionar`. Sem
 * framework, porque o que esta página faz cabe em duzentas linhas: ela
 * lê JSON, desenha lista e manda arquivo. Um framework aqui seria mais
 * código para manter que o código que ele substitui.
 *
 * O servidor é a autoridade: nada é decidido aqui que o backend não
 * confirme. Esta página desenha o que ele responde.
 */
(() => {
  "use strict";

  const $ = (sel) => document.querySelector(sel);
  const palco = $("#palco");

  // ── o tema ─────────────────────────────────────────────────────
  //
  // Sistema segue o aparelho; a escolha manual persiste. É a única
  // coisa que este arquivo guarda no navegador.
  const TEMA = "tinto-tema";
  const tema = (v) => {
    document.documentElement.dataset.tema = v;
    try { localStorage.setItem(TEMA, v); } catch (e) { /* modo privado */ }
  };
  try {
    const salvo = localStorage.getItem(TEMA);
    if (salvo) document.documentElement.dataset.tema = salvo;
  } catch (e) { /* segue no sistema */ }

  // A lua alterna claro e escuro. Sem escolha salva, parte do sistema.
  const escuro = () => {
    const t = document.documentElement.dataset.tema;
    return t === "escuro" ||
      (t !== "claro" && matchMedia("(prefers-color-scheme: dark)").matches);
  };
  const pinta_lua = () => document.querySelectorAll(".lua")
    .forEach((b) => b.setAttribute("aria-pressed", String(escuro())));
  document.querySelectorAll(".lua").forEach((b) => {
    b.onclick = () => { tema(escuro() ? "claro" : "escuro"); pinta_lua(); };
  });
  pinta_lua();

  const avisa = (texto) => {
    const el = $("#aviso");
    el.textContent = texto;
    el.hidden = false;
    clearTimeout(avisa._t);
    avisa._t = setTimeout(() => { el.hidden = true; }, 4000);
  };

  const pede = async (rota, opcoes) => {
    const r = await fetch(rota, { credentials: "same-origin", ...opcoes });
    if (r.status === 401) { entra(); throw new Error("sem sessão"); }
    return r;
  };

  const escapa = (s) => String(s ?? "").replace(/[&<>"']/g, (c) => (
    { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
  const capa = (o) => o.capa
    ? `<img src="/e/api/acervo/${encodeURIComponent(o.id)}/capa"
            alt="Capa de ${escapa(o.titulo)}">`
    : escapa((o.titulo || "?")[0]);
  const data_curta = (segundos) => segundos
    ? new Intl.DateTimeFormat("pt-BR", { dateStyle: "medium" })
        .format(new Date(segundos * 1000))
    : "Não informada";
  // Um item que abre NO LUGAR: o botão de resumo e, logo depois dele, o
  // detalhe. Anotações e envios de voz usam o mesmo.
  const liga_abre_fecha = () => palco.querySelectorAll(".anotacao-resumo")
    .forEach((b) => {
      b.onclick = () => {
        const d = b.nextElementSibling, abrir = d.hidden;
        d.hidden = !abrir;
        b.setAttribute("aria-expanded", String(abrir));
      };
    });

  // ── um envio de voz: o que foi dito e o que virou ──────────────
  //
  // O texto só existe nos CONFIRMADOS no aparelho. O que foi descartado
  // (ou ainda espera o OK) não fica guardado no servidor, e a linha diz
  // isso em vez de mostrar um vazio.
  const TIPOS = { 1: "Anotação", 2: "Tarefa", 3: "Lista", 4: "Evento" };
  const dia_mes = (d) => d ? `${d.slice(8, 10)}/${d.slice(5, 7)}` : "";
  const segundos_falados = (s) => {
    s = Math.max(0, Math.round(Number(s) || 0));
    return s < 60 ? `${s} s` : `${Math.floor(s / 60)} min ${s % 60} s`;
  };
  const o_que_virou = (a) => {
    const quando = [dia_mes(a.d), a.h ? (a.f ? `${a.h}–${a.f}` : a.h) : ""]
      .filter(Boolean).join(" ");
    const itens = a.itens ? `${a.itens} ${a.itens === 1 ? "item" : "itens"}` : "";
    return [a.t || "Sem título", quando, itens].filter(Boolean).join(" · ");
  };
  const envio_de_voz = (u) => {
    const acoes = u.acoes || [];
    const nome = !u.confirmada ? "Não confirmada"
      : (acoes[0] ? acoes[0].t || TIPOS[acoes[0].tp] : "Fala") +
        (acoes.length > 1 ? ` e mais ${acoes.length - 1}` : "");
    const detalhe = u.confirmada
      ? `<p class="sinopse">“${escapa(u.texto)}”</p>
         <dl>${acoes.map((a) => `<div><dt>${TIPOS[a.tp] || "Item"}</dt>
           <dd>${escapa(o_que_virou(a))}</dd></div>`).join("")}</dl>`
      : `<p class="sinopse">Não foi confirmada no Tinto: nada foi criado, e o
           que foi dito não fica guardado.</p>`;
    return `
      <article class="anotacao">
        <button class="anotacao-resumo" aria-expanded="false">
          <span class="nome">${escapa(nome)}</span>
          <span class="quando">${new Date(u.em * 1000).toLocaleString("pt-BR",
            { day: "2-digit", month: "2-digit", hour: "2-digit",
              minute: "2-digit" })} · ${segundos_falados(u.segundos)}</span>
        </button>
        <div class="obra-detalhes" hidden>${detalhe}</div>
      </article>`;
  };

  const tempo_leitura = (minutos) => {
    const n = Number(minutos || 0);
    if (!n) return "Não estimado";
    if (n < 60) return `${n} min`;
    const h = Math.floor(n / 60), m = n % 60;
    return m ? `${h} h ${m} min` : `${h} h`;
  };

  // ── a entrada ──────────────────────────────────────────────────
  function entra() {
    tela_viva = null;
    $("#app").hidden = true;
    $("#entrada").hidden = false;
  }

  // ── a conta na moldura ────────────────────────────────────────
  //
  // No celular, tocar nas iniciais abre um cartão pequeno com a conta
  // e a saída. No desktop, a mesma saída fica fixa no pé do menu. Os
  // dois caminhos encerram a MESMA sessão no servidor.
  const fecha_gaveta = () => {
    $("#gaveta").hidden = true;
    $("#iniciais").setAttribute("aria-expanded", "false");
  };

  $("#iniciais").onclick = (ev) => {
    ev.stopPropagation();
    const gaveta = $("#gaveta");
    const abre = gaveta.hidden;
    gaveta.hidden = !abre;
    $("#iniciais").setAttribute("aria-expanded", String(abre));
  };

  document.addEventListener("click", (ev) => {
    const gaveta = $("#gaveta");
    if (!gaveta.hidden && !gaveta.contains(ev.target)) fecha_gaveta();
  });
  document.addEventListener("keydown", (ev) => {
    if (ev.key === "Escape" && !$("#gaveta").hidden) {
      fecha_gaveta();
      $("#iniciais").focus();
    }
  });

  const sair = async () => {
    const botoes = [$("#sair-lado"), $("#sair-topo")];
    botoes.forEach((b) => { b.disabled = true; });
    try {
      const r = await fetch("/e/api/sair", {
        method: "POST", credentials: "same-origin",
      });
      if (!r.ok) throw new Error("não foi possível encerrar a sessão");

      fecha_gaveta();
      palco.replaceChildren();
      history.replaceState(null, "", location.pathname);
      entra();
    } catch (e) {
      avisa("Não foi possível sair. Tente novamente.");
    } finally {
      botoes.forEach((b) => { b.disabled = false; });
    }
  };

  $("#sair-lado").onclick = sair;
  $("#sair-topo").onclick = sair;

  // A instalação é OFERECIDA, nunca exigida: o navegador continua
  // servindo a página inteira para quem não instalar.
  let convite = null;
  window.addEventListener("beforeinstallprompt", (ev) => {
    ev.preventDefault();
    convite = ev;
    const b = $("#instalar");
    b.hidden = false;
    b.onclick = async () => { b.hidden = true; convite.prompt(); convite = null; };
  });

  // ── Minha Conta ────────────────────────────────────────────────
  async function tela_conta() {
    tela_viva = tela_conta;
    const conta = await (await pede("/e/api/conta")).json();

    const duracao = (s) => {
      s = Math.max(0, Number(s) || 0);
      const m = Math.floor(s / 60), resto = Math.floor(s % 60);
      return `${m} min ${String(resto).padStart(2, "0")} s`;
    };
    // Cinco à vista; o resto do mês atrás de um filete "mais N envios",
    // que abre e fecha no lugar. Vinte linhas de uma vez poluíam a conta.
    const envios = conta.voz.recentes || [];
    const resto = envios.length - 5;
    const recentes = envios.length ? `
      <div class="notas">${envios.slice(0, 5).map(envio_de_voz).join("")}</div>
      ${resto > 0 ? `
        <button class="mais-envios" aria-expanded="false"
          data-n="${resto} ${resto === 1 ? "envio" : "envios"}">mais ${resto}
          ${resto === 1 ? "envio" : "envios"}</button>
        <div class="notas resto-do-mes" hidden>${envios.slice(5).map(envio_de_voz).join("")}</div>`
      : ""}` : "";
    const aparelhos = conta.aparelhos.map((a) => `
      <button class="aparelho" data-device="${escapa(a.device_id)}"
              aria-expanded="false">
        <span class="mini-tinto" aria-hidden="true"></span>
        <div>
          <div class="nome">${escapa(a.nome || a.device_id)}</div>
          <div class="meta">${escapa(a.device_id)} ·
            ${a.online ? "conectado" : "sem contato"} ·
            sincronizou ${escapa(a.sincronizou || "ainda não")}</div>
        </div>
        <span aria-hidden="true">›</span>
      </button>
      <div class="detalhe" hidden data-de="${escapa(a.device_id)}">
        <label class="rotulo-codigo" for="n-${escapa(a.device_id)}">
          COMO VOCÊ CHAMA ESTE TINTO</label>
        <input class="campo" id="n-${escapa(a.device_id)}"
               value="${escapa(a.nome || "")}" maxlength="24"
               placeholder="o da mesa">
        <button class="botao salvar">Salvar o nome</button>
        <button class="botao risco soltar">Desvincular este Tinto</button>
        <p class="meta">Desvincular não apaga o que já está no cartão: a
           agenda que desceu continua legível sem rede e sem conta.</p>
      </div>`).join("");

    // O campo de seis caracteres é UM, e só aparece aqui — depois do
    // login. O código não atravessa o consentimento do Google, e nada
    // é reservado antes de se saber de quem é a conta.
    // As INICIAIS no cabeçalho: o quadrado do desenho aprovado.
    moldura(conta);

    palco.innerHTML = `
      <h1>Minha Conta</h1>
      <div class="email">${escapa(conta.email)}</div>

      <h2>Voz desta conta</h2>
      <div class="fatos">
        <div class="fato"><span class="rotulo">usada</span>
          <span class="valor">${duracao(conta.voz.usados)} de ${duracao(conta.voz.limite)}</span></div>
        <div class="fato"><span class="rotulo">renova em</span>
          <span class="valor">${conta.voz.dias} dias</span></div>
      </div>
      ${recentes ? `<h2>Envios deste mês</h2>${recentes}` : ""}

      <h2>Seus Tintos</h2>
      ${aparelhos || `<div class="sem-tinto">
        <div class="desenho-tinto" aria-hidden="true"></div>
        <h3>Nenhum dispositivo</h3>
        <p>No Tinto, abra Ajustes &gt; Minha conta e escolha Conectar. Ele
           mostra seis letras na tela — digite-as aqui.</p></div>`}

      <form id="vincular">
        <div class="rotulo-codigo">CÓDIGO DO APARELHO</div>
        <div class="codigo">
          ${[0, 1, 2, 3, 4, 5].map((i) => `
            <input id="c${i}" maxlength="1" autocomplete="off"
                   inputmode="latin" aria-label="letra ${i + 1} de 6">`).join("")}
        </div>
        <p class="erro" id="erro-codigo" hidden></p>
        <button class="botao forte">Vincular Tinto</button>
      </form>`;


    // O detalhe do aparelho abre NO LUGAR, e não numa terceira tela:
    // Dispositivos mora dentro de Minha Conta, e uma rota a mais faria
    // dele um destino permanente.
    liga_abre_fecha();
    const mais = palco.querySelector(".mais-envios");
    // O filete fica sempre logo abaixo dos cinco; o resto abre embaixo dele.
    if (mais) mais.onclick = () => {
      const lista = mais.nextElementSibling, abrir = lista.hidden;
      lista.hidden = !abrir;
      mais.setAttribute("aria-expanded", String(abrir));
      mais.textContent = `${abrir ? "esconder" : "mais"} ${mais.dataset.n}`;
    };

    palco.querySelectorAll(".aparelho").forEach((cartao) => {
      const id = cartao.dataset.device;
      const detalhe = palco.querySelector(`.detalhe[data-de="${id}"]`);

      cartao.onclick = () => {
        const abrindo = detalhe.hidden;
        detalhe.hidden = !abrindo;
        cartao.setAttribute("aria-expanded", String(abrindo));
      };

      detalhe.querySelector(".salvar").onclick = async () => {
        const nome = detalhe.querySelector(".campo").value;
        const r = await pede(`/e/api/aparelho/${id}`, {
          method: "PATCH", headers: { "content-type": "application/json" },
          body: JSON.stringify({ nome }),
        });
        if (r.ok) { avisa("Nome salvo."); tela_conta(); }
      };

      detalhe.querySelector(".soltar").onclick = async () => {
        if (!confirm("Desvincular este Tinto?\n\nO que já está no cartão "
                     + "continua lá.")) return;
        const r = await pede(`/e/api/soltar?device_id=${encodeURIComponent(id)}`,
                             { method: "POST" });
        if (r.ok) { avisa("Tinto desvinculado."); tela_conta(); }
      };
    });

    // Seis caixas, uma letra cada: a pessoa está copiando do vidro do
    // aparelho, e o dedo anda sozinho para a próxima. Colar o código
    // inteiro numa delas também funciona.
    const caixas = [...palco.querySelectorAll(".codigo input")];
    caixas.forEach((caixa, i) => {
      caixa.oninput = () => {
        caixa.value = caixa.value.replace(/[^A-Za-z]/g, "").toUpperCase();
        if (caixa.value && i < 5) caixas[i + 1].focus();
      };
      caixa.onkeydown = (ev) => {
        if (ev.key === "Backspace" && !caixa.value && i > 0) caixas[i - 1].focus();
      };
      caixa.onpaste = (ev) => {
        const texto = (ev.clipboardData.getData("text") || "")
          .replace(/[^A-Za-z]/g, "").toUpperCase().slice(0, 6);
        if (!texto) return;
        ev.preventDefault();
        texto.split("").forEach((letra, n) => { if (caixas[n]) caixas[n].value = letra; });
        caixas[Math.min(texto.length, 5)].focus();
      };
    });

    $("#vincular").onsubmit = async (ev) => {
      ev.preventDefault();
      const erro = $("#erro-codigo");
      const digitado = caixas.map((c) => c.value).join("");
      const r = await pede("/e/api/vincular", {
        method: "POST", headers: { "content-type": "application/json" },
        body: JSON.stringify({ codigo: digitado }),
      });
      if (r.ok) { avisa("Tinto vinculado."); tela_conta(); return; }

      // O erro aparece JUNTO do campo, e diz o que fazer: o código
      // expira em cinco minutos e o aparelho mostra outro.
      erro.textContent = (await r.json()).motivo
        || "Esse código não vale mais. Peça outro no aparelho.";
      erro.hidden = false;
      caixas[0].focus();
    };
  }

  // ── Meu Acervo ─────────────────────────────────────────────────
  const ESTADO = {
    recebido: "recebido", convertendo: "convertendo…",
    pronto: "pronto", recusado: "não deu para converter",
    falhou: "falhou aqui",
  };

  // As anotações faladas: uma faixa por nota, título cortado numa linha,
  // e o OK abre o texto inteiro e o dia em que foi registrada.
  async function tela_anotacoes() {
    tela_viva = tela_anotacoes;
    const { anotacoes } = await (await pede("/e/api/anotacoes")).json();
    const linhas = anotacoes.map((a) => `
      <article class="anotacao">
        <button class="anotacao-resumo" aria-expanded="false">
          <span class="nome">${escapa(a.titulo || "Anotação")}</span>
          <span class="quando">${data_curta(a.criada_em)}</span>
        </button>
        <div class="obra-detalhes" hidden>
          <p class="sinopse">${escapa(a.corpo || "Sem texto.")}</p>
          <dl><div><dt>Registrada em</dt><dd>${data_curta(a.criada_em)}</dd></div></dl>
        </div>
      </article>`).join("");

    palco.innerHTML = `
      <h1>Meu Acervo</h1>
      ${filtros_do_acervo("anotacao", false)}
      ${linhas ? `<div class="notas">${linhas}</div>`
               : `<div class="vazio">Nenhuma anotação ainda.<br>
                  Fale com o Tinto e ela aparece aqui.</div>`}`;
    liga_filtros();
    liga_abre_fecha();
  }

  const filtros_do_acervo = (filtro, com_mais) => `
      <div class="filtros" role="group" aria-label="Filtrar">
        ${[["", "Todos"], ["livro", "Livros"], ["documento", "Documentos"],
           ["anotacao", "Anotações"]]
          .map(([v, t]) => `<button data-filtro="${v}"
            aria-pressed="${filtro === v}">${t}</button>`).join("")}
        ${com_mais ? '<button class="direita" id="mais" aria-label="Adicionar">+<span class="so-largo"> ADICIONAR</span></button>' : ""}
      </div>`;

  const liga_filtros = () => palco.querySelectorAll("[data-filtro]").forEach((b) => {
    b.onclick = () => b.dataset.filtro === "anotacao"
      ? tela_anotacoes() : tela_acervo(b.dataset.filtro);
  });

  async function tela_acervo(filtro = "") {
    tela_viva = () => tela_acervo(filtro);
    const dados = await (await pede(
      `/e/api/acervo${filtro ? `?filtro=${filtro}` : ""}`)).json();

    const obras = dados.obras.map((o) => `
      <article class="obra" data-id="${escapa(o.id)}">
        <button class="obra-resumo" aria-expanded="false">
          <span class="capa">${capa(o)}</span>
          <span class="nome">${escapa(o.titulo)}</span>
          <span class="autor">${escapa(o.autor || "Autor não informado")}</span>
          <span class="estado">${ESTADO[o.estado] || o.estado}</span>
          ${o.dispositivos.length ? '<span class="aqui">NO DISPOSITIVO</span>' : ""}
          <span class="abrir-detalhes" aria-hidden="true">DETALHES ↓</span>
        </button>
        <div class="obra-detalhes" hidden>
          <p class="sinopse">${escapa(o.sinopse || "Sinopse não disponível.")}</p>
          <dl>
            <div><dt>Tipo</dt><dd>${o.tipo === "livro" ? "Livro" : "Documento"}</dd></div>
            <div><dt>Adicionado</dt><dd>${data_curta(o.criada_em)}</dd></div>
            <div><dt>Extensão</dt><dd>${Number(o.palavras || 0).toLocaleString("pt-BR")} palavras</dd></div>
            <div><dt>Tempo estimado</dt><dd>${tempo_leitura(o.minutos_leitura)}</dd></div>
          </dl>
          <button class="remover">Remover do Acervo</button>
        </div>
      </article>`).join("");

    palco.innerHTML = `
      <h1>Meu Acervo</h1>
      ${filtros_do_acervo(filtro, true)}
      ${obras ? `<div class="estante">${obras}</div>`
              : `<div class="vazio">Nada aqui ainda.<br>
                 Use o + para mandar um livro ou documento.</div>`}
      <form id="envio" hidden>
        <input type="file" id="arquivo" name="arquivo"
               accept=".epub,.pdf,.docx,.txt,.rtf,.md">
      </form>`;

    liga_filtros();

    // O `+` abre o ENVIO e NÃO vira destino de navegação: enviar é
    // evento, não hábito, e um terceiro item permanente na barra faria
    // dele um lugar onde se mora.
    $("#mais").onclick = () => envio(filtro);

    palco.querySelectorAll(".obra-resumo").forEach((b) => {
      b.onclick = () => {
        const detalhes = b.nextElementSibling;
        const abrir = detalhes.hidden;
        palco.querySelectorAll(".obra-detalhes:not([hidden])").forEach((d) => {
          d.hidden = true;
          d.previousElementSibling.setAttribute("aria-expanded", "false");
        });
        detalhes.hidden = !abrir;
        b.setAttribute("aria-expanded", String(abrir));
      };
    });

    palco.querySelectorAll(".remover").forEach((b) => {
      b.onclick = async () => {
        const id = b.closest(".obra").dataset.id;
        if (!confirm("Remover do Acervo?\n\nCópias já baixadas nos seus "
                     + "Tintos continuarão disponíveis.")) return;
        await pede(`/e/api/acervo/${id}`, { method: "DELETE" });
        avisa("Removida do Acervo.");
        tela_acervo(filtro);
      };
    });
  }

  // ── o ENVIO, em cinco passos ───────────────────────────────────
  //
  // Escolher · verificar · revisar · confirmar · adicionar. Os cinco
  // existem porque a conversão pode dar errado de três jeitos
  // diferentes, e cada um se resolve num lugar: o arquivo que não serve
  // (passo 2), o texto que saiu ruim (passo 3), e a classificação que a
  // máquina errou (passo 4).
  //
  // O servidor conserva o resultado da conversão como rascunho, mas ele
  // só entra no catálogo depois da confirmação final.
  const PASSOS = ["Escolher", "Verificar", "Revisar", "Confirmar", "Pronto"];

  function envio(filtro) {
    tela_viva = null;      // um envio em curso não é trocado por baixo
    const rascunho = { arquivo: null, obra: null, passo: 1, cancelado: false };

    const desenha = () => {
      const trilha = PASSOS.map((nome, i) => `
        <li aria-current="${i + 1 === rascunho.passo ? "step" : "false"}">
          <span class="numero">${i + 1}</span>${nome}</li>`).join("");

      palco.innerHTML = `
        <h1>Adicionar ao Acervo</h1>
        <ol class="trilha">${trilha}</ol>
        <div id="passo"></div>
        <div class="acoes">
          ${rascunho.passo > 1 && rascunho.passo < 5
            ? '<button class="botao" id="voltar">Voltar</button>' : ""}
          <button class="botao nu" id="cancelar">Cancelar</button>
        </div>`;

      // Cancelar existe em TODOS os passos, inclusive no último: a
      // pessoa pode desistir depois de ler a amostra e ver que a
      // conversão saiu ruim.
      $("#cancelar").onclick = async () => {
        rascunho.cancelado = true;
        if (rascunho.obra && !rascunho.obra.publicada) {
          await pede(`/e/api/acervo/${rascunho.obra.id}`, { method: "DELETE" });
        }
        tela_acervo(filtro);
      };
      const voltar = $("#voltar");
      if (voltar) voltar.onclick = () => { rascunho.passo--; desenha(); };

      ({ 1: passo_escolher, 2: passo_verificar, 3: passo_revisar,
         4: passo_confirmar, 5: passo_pronto }[rascunho.passo])();
    };

    const passo_escolher = () => {
      $("#passo").innerHTML = `
        <p class="ajuda">EPUB, PDF com texto, DOCX, TXT, RTF ou Markdown.
           O Tinto lê o TEXTO — a diagramação não atravessa, e é por isso
           que um PDF que é só imagem não serve.</p>
        <input type="file" id="arquivo"
               accept=".epub,.pdf,.docx,.txt,.rtf,.md">`;
      $("#arquivo").onchange = (ev) => {
        rascunho.arquivo = ev.target.files[0] || null;
        if (rascunho.arquivo) { rascunho.passo = 2; desenha(); }
      };
    };

    const passo_verificar = async () => {
      $("#passo").innerHTML = `
        <div class="carregando" role="status" aria-live="polite">
          <div>Preparando <b>${escapa(rascunho.arquivo.name)}</b></div>
          <span class="pontos" aria-hidden="true"><i></i><i></i><i></i></span>
          <small>Extraindo o texto para o leitor. Nenhuma IA é usada.</small>
        </div>`;

      const corpo = new FormData();
      corpo.append("arquivo", rascunho.arquivo);
      const r = await pede("/e/api/acervo", { method: "POST", body: corpo });
      const saida = await r.json();

      // A conversão pode terminar depois de a pessoa apertar Cancelar.
      // Nesse caso o id só passa a existir agora: descartamo-lo sem
      // reabrir o stepper nem deixar um rascunho órfão.
      if (rascunho.cancelado) {
        if (r.ok && saida.id && !saida.publicada) {
          await pede(`/e/api/acervo/${saida.id}`, { method: "DELETE" });
        }
        return;
      }

      if (!r.ok) {
        // RECUSA é do arquivo, e quem resolve é a pessoa mandando outro.
        // Ela volta para o passo 1 com o motivo na frente.
        $("#passo").innerHTML = `
          <div class="recusa">
            <div class="titulo">Este arquivo não serve</div>
            <p>${escapa(saida.motivo || "não deu para converter")}</p>
            <button class="botao forte" id="outro">Escolher outro arquivo</button>
          </div>`;
        $("#outro").onclick = () => { rascunho.passo = 1; desenha(); };
        return;
      }

      rascunho.obra = saida;
      rascunho.passo = 3;
      desenha();
    };

    const passo_revisar = () => {
      const o = rascunho.obra;
      $("#passo").innerHTML = `
        <p class="ajuda">Confira se o texto atravessou legível. Se a
           amostra estiver embaralhada, o arquivo não serve.</p>
        <div class="revisao">
          <div class="capa">${capa(o)}</div>
          <div>
            <div class="nome">${escapa(o.titulo)}</div>
            <div class="autor">${escapa(o.autor || "sem autor")}</div>
            <div class="meta">${o.caracteres.toLocaleString("pt-BR")} caracteres</div>
          </div>
        </div>
        <blockquote class="amostra">${escapa(o.amostra || "")}</blockquote>
        <button class="botao forte" id="seguir">O texto está legível</button>`;
      $("#seguir").onclick = () => { rascunho.passo = 4; desenha(); };
    };

    const passo_confirmar = () => {
      const o = rascunho.obra;
      $("#passo").innerHTML = `
        <p class="ajuda">Corrija o que a máquina errou. O tipo muda só
           onde a obra aparece — Livros ou Documentos.</p>
        <label class="rotulo-codigo" for="t">TÍTULO</label>
        <input class="campo" id="t" value="${escapa(o.titulo)}" maxlength="120">
        <label class="rotulo-codigo" for="a">AUTOR</label>
        <input class="campo" id="a" value="${escapa(o.autor || "")}" maxlength="80">
        <div class="filtros" role="group" aria-label="Tipo">
          ${[["livro", "Livro"], ["documento", "Documento"]].map(([v, n]) => `
            <button data-tipo="${v}" aria-pressed="${o.tipo === v}">${n}</button>`).join("")}
        </div>
        <button class="botao forte" id="adicionar">Adicionar ao meu Acervo</button>`;

      palco.querySelectorAll("[data-tipo]").forEach((b) => {
        b.onclick = () => { rascunho.obra.tipo = b.dataset.tipo; desenha(); };
      });

      $("#adicionar").onclick = async () => {
        const r = await pede(`/e/api/acervo/${o.id}`, {
          method: "PATCH", headers: { "content-type": "application/json" },
          body: JSON.stringify({ titulo: $("#t").value, autor: $("#a").value,
                                 tipo: rascunho.obra.tipo }),
        });
        if (!r.ok) return;
        rascunho.obra = await r.json();
        const confirma = await pede(`/e/api/acervo/${o.id}/confirmar`, {
          method: "POST",
        });
        if (confirma.ok) {
          rascunho.obra = await confirma.json();
          rascunho.passo = 5;
          desenha();
        }
      };
    };

    const passo_pronto = () => {
      // A frase é anunciada para quem usa leitor de tela, e é a mesma
      // que a especificação pede — com o nome da obra dentro.
      avisa(`${rascunho.obra.titulo} foi adicionada ao seu Acervo.`);
      $("#passo").innerHTML = `
        <div class="pronto">
          <div class="titulo">${escapa(rascunho.obra.titulo)}</div>
          <p>Está no seu Acervo. Abra a obra no Tinto para ele baixar a
             cópia — o aparelho decide o que fica no cartão dele.</p>
          <button class="botao forte" id="fim">Voltar ao Acervo</button>
        </div>`;
      $("#fim").onclick = () => tela_acervo(filtro);
    };

    desenha();
  }

  // ── a moldura sabe de quem é ───────────────────────────────────
  //
  // As iniciais no cabeçalho do celular e o bloco de conta no pé do menu
  // lateral dizem a mesma coisa, e por isso saem do mesmo lugar. Quem os
  // preenche é o BOOT: no desktop o menu está na tela desde o primeiro
  // instante, inclusive na rota do Acervo, que não pede a conta.
  function moldura(conta) {
    $("#iniciais").textContent = (conta.email || "?").slice(0, 2).toUpperCase();

    const nome = (conta.email || "").split("@")[0];
    const identidade = `<strong>${escapa(nome)}</strong><br>${escapa(conta.email || "")}`;
    $("#conta-lado").innerHTML = identidade;
    $("#gaveta-quem").innerHTML = identidade;
  }

  // ── as rotas ───────────────────────────────────────────────────
  async function anda() {
    const rota = (location.hash.replace(/^#\/?/, "") || "conta").split("/")[0];

    document.querySelectorAll(".destino").forEach((a) => {
      if (a.dataset.rota === rota) a.setAttribute("aria-current", "page");
      else a.removeAttribute("aria-current");
    });

    try {
      if (rota === "acervo") await tela_acervo();
      else await tela_conta();
      palco.scrollTop = 0;     // no celular quem rola é o palco
      palco.focus();
    } catch (e) { /* sem sessão: a entrada já apareceu */ }
  }

  window.addEventListener("hashchange", anda);

  // ── o conteúdo se atualiza sozinho ─────────────────────────────
  //
  // Ao voltar para o app e a cada 10 s com ele aberto — mas só a tela que
  // se ofereceu (`tela_viva`) e só com a pessoa PARADA. Parada é uma regra
  // só, que não depende de que tela é: nada tocado nos últimos 10 s, nada
  // aberto, nenhum campo em foco ou editado. Tela nova não precisa entrar
  // em lista nenhuma para não ser trocada por baixo de quem a usa.
  let tela_viva = null;
  let tocou_em = 0;
  for (const tipo of ["pointerdown", "keydown", "input"])
    document.addEventListener(tipo, () => { tocou_em = Date.now(); }, true);

  const parado = () => Date.now() - tocou_em > 10000 &&
    !document.querySelector('[aria-expanded="true"]') &&
    !(document.activeElement && document.activeElement.matches("input, textarea")) &&
    ![...palco.querySelectorAll("input, textarea")]
      .some((c) => c.value !== c.defaultValue);

  async function atualiza() {
    if (document.hidden || !tela_viva || !parado()) return;
    try { await tela_viva(); }
    catch (e) { /* sem sessão ou sem rede: fica o que estava */ }
  }
  document.addEventListener("visibilitychange", atualiza);
  setInterval(atualiza, 10000);

  // Quem manda dizer se há sessão é o SERVIDOR. Perguntar primeiro
  // evita desenhar a moldura para quem vai cair na entrada.
  (async () => {
    const r = await fetch("/e/api/conta", { credentials: "same-origin" });
    if (r.status === 401) { entra(); return; }
    $("#entrada").hidden = true;
    $("#app").hidden = false;
    try { moldura(await r.json()); } catch (e) { /* a tela pede de novo */ }
    anda();
  })();
})();
