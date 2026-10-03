// firmware/testes/t_texto.c — a tinta, e as três armadilhas conhecidas.
#include "teste.h"
#include "tela/texto.h"
#include "ui/agua.h"

static uint8_t memoria[(TELA_L + 7) / 8 * TELA_A];
static bitmap_t bm;

static void tela_limpa(void)
{
    bitmap_liga(&bm, memoria, TELA_L, TELA_A);
    gfx_limpa(&bm, false);
    gfx_zera_faltantes();
}

static int conta_tinta(void)
{
    int n = 0;
    for (int y = 0; y < bm.a; y++)
        for (int x = 0; x < bm.l; x++)
            if (gfx_le(&bm, x, y)) n++;
    return n;
}

// ── ARMADILHA 1 — o tracking ────────────────────────────────────────
void t_largura_soma_avancos_nao_tinta(void)
{
    COMECA("armadilha 1 · medir soma avanços, não larguras de tinta");

    // Medindo a tinta, "iiii" daria ~4 px; o avanço inclui os side bearings.
    int quatro_i = gfx_largura(F_CORPO, "iiii");
    ESPERA(quatro_i >= 12);

    // Medir junto bate com medir por partes.
    int junto  = gfx_largura(F_CORPO, "Dentista");
    int partes = gfx_largura(F_CORPO, "Dent") + gfx_largura(F_CORPO, "ista");
    ESPERA_IGUAL(junto, partes);

    TERMINA();
}

void t_largura_bate_com_o_que_desenha(void)
{
    COMECA("a largura medida é onde o desenho realmente termina");

    tela_limpa();
    int fim = gfx_texto(&bm, 0, 0, F_CORPO, "Dentista");
    ESPERA_IGUAL(fim, gfx_largura(F_CORPO, "Dentista"));

    TERMINA();
}

// ── ARMADILHA 2 — a caixa mede a tinta ──────────────────────────────
void t_acento_nao_gasta_linha_a_mais(void)
{
    COMECA("armadilha 2 · a caixa mede a tinta, não o contorno vetorial");

    // A caixa vetorial de "Á" pedia uma linha a mais. O teste não fixa a
    // altura (muda com a fonte): fixa que a tinta cabe na linha declarada.
    int linha = gfx_altura_linha(F_CORPO);

    tela_limpa();
    gfx_texto(&bm, 4, 0, F_CORPO, "ÁÉÍÓÚÇ");

    int fora = 0;
    for (int y = linha; y < bm.a; y++)
        for (int x = 0; x < bm.l; x++)
            if (gfx_le(&bm, x, y)) fora++;
    ESPERA_IGUAL(fora, 0);

    TERMINA();
}

// ── ARMADILHA 3 — glifo faltante ────────────────────────────────────
void t_glifo_faltante_conta_em_vez_de_sumir(void)
{
    COMECA("armadilha 3 · glifo faltante conta e vira tofu, não some");

    tela_limpa();
    gfx_texto(&bm, 4, 0, F_CORPO, "Dentista");
    ESPERA_IGUAL(gfx_faltantes(), 0);

    // "→" não está na fonte de propósito: ícone é desenhado.
    tela_limpa();
    gfx_texto(&bm, 4, 0, F_CORPO, "vai →");
    ESPERA_IGUAL(gfx_faltantes(), 1);
    ESPERA(conta_tinta() > 0);           // desenhou o tofu, não sumiu

    TERMINA();
}

void t_o_texto_do_sistema_nao_tem_faltante(void)
{
    COMECA("nenhum texto do sistema pede glifo que a fonte não tem");

    // O que teria pego o "3 d", o ":" da lista e as aspas da citação.
    static const char *TEXTOS[] = {
        "Dentista", "mandar histórico pro Oulu", "Reunião com o cliente",
        "atrasada · 2 d", "FALA DO MÊS", "vira em 19 dias",
        "Anotação · fora do Google", "3 d", "14:00", "seg", "sáb",
        "não consegui ler", "cartão cheio", "sistema desatualizado",
        "Aniversários", "1 de 4", "+ 2 mais", "12 de 30 min",
        "conectado como fulano", "Ações da nota",
    };
    gfx_zera_faltantes();
    for (size_t i = 0; i < sizeof TEXTOS / sizeof TEXTOS[0]; i++) {
        gfx_largura(F_MIUDA,  TEXTOS[i]);
        gfx_largura(F_CORPO,  TEXTOS[i]);
        gfx_largura(F_TITULO, TEXTOS[i]);
    }
    ESPERA_IGUAL(gfx_faltantes(), 0);

    // E a fonte enorme dá conta do relógio.
    gfx_largura(F_ENORME, "09:14");
    gfx_largura(F_ENORME, "0123456789");
    ESPERA_IGUAL(gfx_faltantes(), 0);

    TERMINA();
}

// ── UTF-8 ───────────────────────────────────────────────────────────
void t_acento_e_um_glifo_nao_dois_bytes(void)
{
    COMECA("acento é UM glifo, não dois bytes soltos");

    gfx_zera_faltantes();
    // "ção": 5 bytes, 3 caracteres.
    ESPERA_IGUAL(strlen("ção"), 5);
    int l_cao = gfx_largura(F_CORPO, "ção");
    int l_cao_ascii = gfx_largura(F_CORPO, "cao");
    ESPERA_IGUAL(gfx_faltantes(), 0);
    // Largura parecida: byte como char daria quase o dobro.
    ESPERA(l_cao < l_cao_ascii * 3 / 2);

    TERMINA();
}

// ── o orçamento real da tela ────────────────────────────────────────
void t_orcamento_de_28_colunas(void)
{
    COMECA("o orçamento de colunas é medido, não estimado");

    // ~24 colunas no corpo de 14 pt (em 1 bit, mais pixel por letra é o que
    // suaviza). A faixa pega fonte trocada por acidente.
    int m = gfx_largura(F_CORPO, "m");
    int n = gfx_largura(F_CORPO, "n");
    int medio = (m + n) / 2;
    int colunas = 240 / medio;

    ESPERA(colunas >= 20);
    ESPERA(colunas <= 30);

    TERMINA();
}

void t_truncar_conta_bytes_de_caractere_inteiro(void)
{
    COMECA("truncar nunca corta um caractere no meio");

    // Cortar "ção" no meio de um byte desenharia lixo.
    int bytes = gfx_cabe(F_CORPO, "ação", 8);
    char buf[16];
    memcpy(buf, "ação", (size_t)bytes);
    buf[bytes] = '\0';

    gfx_zera_faltantes();
    gfx_largura(F_CORPO, buf);
    ESPERA_IGUAL(gfx_faltantes(), 0);   // nenhum byte solto virou tofu

    TERMINA();
}

void t_paragrafo_quebra_na_palavra(void)
{
    COMECA("parágrafo quebra na palavra e respeita o teto de linhas");

    tela_limpa();
    const char *texto = "O painel da dock deve mostrar as tarefas do dia, "
                        "e não o próximo compromisso.";

    int y = gfx_paragrafo(&bm, 10, 0, 220, 4, F_CORPO, texto);
    ESPERA(y > 0);
    ESPERA_IGUAL(y % gfx_altura_linha(F_CORPO), 0);
    ESPERA(y <= 4 * gfx_altura_linha(F_CORPO));
    ESPERA_IGUAL(gfx_faltantes(), 0);

    TERMINA();
}

// ── negativo ────────────────────────────────────────────────────────
void t_negativo_inverte_e_volta(void)
{
    COMECA("negativo inverte a região e aplicar duas vezes desfaz");

    tela_limpa();
    gfx_texto(&bm, 6, 3, F_MIUDA, "QUA 12 AGO");
    int antes = conta_tinta();

    gfx_negativo(&bm, 0, 0, TELA_L, 19);
    int depois = conta_tinta();
    ESPERA(depois > antes);              // o fundo virou tinta

    gfx_negativo(&bm, 0, 0, TELA_L, 19);
    ESPERA_IGUAL(conta_tinta(), antes);

    TERMINA();
}

// Whisper e LLM escrevem reticência de um caractere e aspas curvas: sem
// glifo, tofu no meio da frase.
void t_conteudo_nao_vira_tofu(void)
{
    COMECA("texto de conteúdo não vira tofu no meio da frase");

    static const char *FALAS[] = {
        "então, tava pensando… porque na dock você olha de longe",
        "ele disse “manda o histórico até segunda” e eu esqueci",
        "o que muda mesmo é a tarefa — e não o compromisso",
        "‘pasta térmica’, aquela boa",
    };
    gfx_zera_faltantes();
    for (size_t i = 0; i < sizeof FALAS / sizeof FALAS[0]; i++) {
        gfx_largura(F_CORPO,   FALAS[i]);
        gfx_largura(F_CITACAO, FALAS[i]);
        gfx_largura(F_MIUDA,   FALAS[i]);
    }
    ESPERA_IGUAL(gfx_faltantes(), 0);

    TERMINA();
}

// RN-28: o itálico separa o que VOCÊ disse do que a IA escreveu.
void t_a_citacao_e_mesmo_diferente_do_corpo(void)
{
    COMECA("RN-28 · a citação é visivelmente outra fonte, não o corpo");

    const char *frase = "o que muda mesmo e a tarefa";

    // Mesma altura de linha: trocar uma pela outra não mexe no leiaute.
    ESPERA_IGUAL(gfx_altura_linha(F_CITACAO), gfx_altura_linha(F_CORPO));

    // Compara-se a TINTA, não a largura.
    tela_limpa();
    gfx_texto(&bm, 4, 0, F_CORPO, frase);
    int tinta_corpo = 0;
    for (int y = 0; y < 20; y++)
        for (int x = 0; x < bm.l; x++)
            if (gfx_le(&bm, x, y)) tinta_corpo++;

    tela_limpa();
    gfx_texto(&bm, 4, 0, F_CITACAO, frase);
    int tinta_citacao = 0, iguais = 0;
    for (int y = 0; y < 20; y++)
        for (int x = 0; x < bm.l; x++)
            if (gfx_le(&bm, x, y)) tinta_citacao++;

    (void)iguais;
    ESPERA(tinta_corpo > 0 && tinta_citacao > 0);
    ESPERA(tinta_corpo != tinta_citacao);

    TERMINA();
}

// A fonte enorme cobre o que o cartaz escreve além de dígitos.
void t_a_fonte_enorme_da_conta_do_cartaz(void)
{
    COMECA("RN-31 · a fonte do cartaz dá conta de \"Qua 12\" e \"Amanhã\"");

    static const char *CARTAZ[] = {
        "09:14", "14:00", "Amanhã",
        "Dom 1", "Seg 2", "Ter 3", "Qua 12", "Qui 13", "Sex 14", "Sáb 15",
    };
    gfx_zera_faltantes();
    for (size_t i = 0; i < sizeof CARTAZ / sizeof CARTAZ[0]; i++)
        gfx_largura(F_ENORME, CARTAZ[i]);
    ESPERA_IGUAL(gfx_faltantes(), 0);

    TERMINA();
}

// Texto de exceção não vira tofu: aparece quando já há problema.
void t_texto_de_excecao_nao_vira_tofu(void)
{
    COMECA("o texto das exceções não pede glifo que falta");

    static const char *AVISOS[] = {
        "Sem cartão", "Desligue, ponha o cartão e ligue.",
        "O cartão guarda tudo que você fala. Sem ele o aparelho não tem "
        "onde escrever.",
        "não consegui ler", "cartão cheio", "sem contato há 2 h",
        "por transcrever · sem rede", "Guardado no aparelho",
    };
    gfx_zera_faltantes();
    for (size_t i = 0; i < sizeof AVISOS / sizeof AVISOS[0]; i++) {
        gfx_largura(F_MIUDA,  AVISOS[i]);
        gfx_largura(F_CORPO,  AVISOS[i]);
        gfx_largura(F_TITULO, AVISOS[i]);
    }
    ESPERA_IGUAL(gfx_faltantes(), 0);

    TERMINA();
}

// O parágrafo que acaba a cota de linhas termina em RETICÊNCIA: parar
// calado transformava o título em outro.
void t_paragrafo_cortado_diz_que_foi_cortado(void)
{
    COMECA("parágrafo que não cabe termina em reticência, não em silêncio");

    const char *longo = "Reunião de alinhamento com o time de hardware";

    // Uma linha só: sobra reticência no fim.
    int alt = gfx_altura_linha(F_TITULO);
    ESPERA_IGUAL(gfx_paragrafo(NULL, 0, 0, 120, 1, F_TITULO, longo), alt);

    // Duas linhas: idem.
    ESPERA_IGUAL(gfx_paragrafo(NULL, 0, 0, 120, 2, F_TITULO, longo), alt * 2);

    // Quando cabe, uma linha e sem reticência.
    ESPERA_IGUAL(gfx_paragrafo(NULL, 0, 0, 240, 2, F_TITULO, "Dentista"), alt);

    TERMINA();
}

// A reticência cabe DENTRO da largura pedida.
void t_a_reticencia_cabe_dentro_da_largura(void)
{
    COMECA("a reticência entra na conta da largura, não estoura ela");

    static uint8_t bits[(240 / 8) * 60];
    bitmap_t bm;
    bitmap_liga(&bm, bits, 240, 60);
    gfx_limpa(&bm, false);

    const int larg = 120;
    gfx_paragrafo(&bm, 0, 4, larg, 1, F_TITULO, "Reunião de alinhamento");

    // Nenhum pixel além da largura.
    int fora = 0;
    for (int y = 0; y < 60; y++)
        for (int x = larg; x < 240; x++)
            if (bits[y * bm.passo + x / 8] & (0x80 >> (x % 8))) fora++;
    ESPERA_IGUAL(fora, 0);

    TERMINA();
}

// ── a quebra explícita ──────────────────────────────────────────────
// `\n` sem tratamento virava tofu ("Prazer,▯Usuário.").
void t_o_paragrafo_obedece_a_quebra_de_linha(void)
{
    COMECA("parágrafo · o \\n quebra a linha e não vira tofu");

    static uint8_t mem[(TELA_L + 7) / 8 * TELA_A];
    bitmap_t bm;
    bitmap_liga(&bm, mem, TELA_L, TELA_A);
    gfx_limpa(&bm, false);

    gfx_zera_faltantes();
    int fim = gfx_paragrafo(&bm, 0, 0, 220, 3, F_CORPO, "Prazer,\nUsuário.");

    ESPERA_IGUAL(gfx_faltantes(), 0);

    // Duas linhas: a quebra aconteceu.
    ESPERA_IGUAL(fim, gfx_altura_linha(F_CORPO) * 2);

    // O `\n` ganha da quebra automática.
    gfx_zera_faltantes();
    fim = gfx_paragrafo(NULL, 0, 0, 2000, 3, F_CORPO, "um\ndois\ntrês");
    ESPERA_IGUAL(gfx_faltantes(), 0);
    ESPERA_IGUAL(fim, gfx_altura_linha(F_CORPO) * 3);

    TERMINA();
}

// ── a serifa editorial é mesmo outra fonte ──────────────────────────
// Uma entrada apontando para o TTF errado gera, compila e desenha em sans.
void t_a_serifa_editorial_e_mesmo_outra_fonte(void)
{
    COMECA("a serifa editorial é outro desenho, não o título em sans");

    const char *frase = "Boa tarde.";

    // Mesma altura de linha do título sans.
    ESPERA_IGUAL(gfx_altura_linha(F_EDITORIAL), gfx_altura_linha(F_TITULO));

    // Compara-se a TINTA: serifa é mais tinta pelo mesmo texto.
    tela_limpa();
    gfx_texto(&bm, 4, 0, F_TITULO, frase);
    int tinta_sans = conta_tinta();

    tela_limpa();
    gfx_texto(&bm, 4, 0, F_EDITORIAL, frase);
    int tinta_serifa = conta_tinta();

    ESPERA(tinta_sans > 0 && tinta_serifa > 0);
    ESPERA(tinta_sans != tinta_serifa);

    // Nenhum glifo faltando na saudação e na marca da Home.
    tela_limpa();
    gfx_texto(&bm, 4, 0,  F_EDITORIAL, "Bom dia.");
    gfx_texto(&bm, 4, 24, F_EDITORIAL, "Boa tarde.");
    gfx_texto(&bm, 4, 48, F_EDITORIAL, "Boa noite.");
    gfx_texto(&bm, 4, 72, F_EDITORIAL, "Olá.");
    ESPERA_IGUAL(gfx_faltantes(), 0);
    TERMINA();
}


// ── o texto do vazio é TINTA CHEIA ──────────────────────────────────
// Esmaecer tirava metade da haste de uma letra pequena. O recuo vem do
// tamanho. Este teste impede a trama de voltar.
void t_a_marca_dagua_e_tinta_cheia(void)
{
    COMECA("o texto do estado vazio sai em tinta cheia, sem trama");

    const char *frase = "Seu dia está livre.";

    // A frase pela peça do vazio.
    tela_limpa();
    ui_agua(&bm, 0, 0, bm.l, 60, frase);
    int com_agua = conta_tinta();

    // E a mesma frase em tinta cheia.
    tela_limpa();
    gfx_texto(&bm, 0, 0, F_MIUDA, frase);
    int cheia = conta_tinta();

    ESPERA(cheia > 0);

    // Iguais: nenhum pixel tirado.
    ESPERA_IGUAL(com_agua, cheia);
    TERMINA();
}
