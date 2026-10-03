// firmware/testes/t_dado.c — datas, JSON e o cartão.
#include "teste.h"
#include "vista/campos.h"
#include "nucleo/data.h"
#include "dado/json.h"
#include "dado/cartao.h"

static const data_t DIA = { .ano = 2026, .mes = 8, .dia = 14 };

// ── datas ───────────────────────────────────────────────────────────
void t_data_ida_e_volta(void)
{
    COMECA("data vira texto e volta igual");

    char t[DATA_TEXTO];
    data_para_texto(DIA, t, sizeof t);
    ESPERA_TEXTO(t, "2026-08-14");

    data_t d;
    ESPERA_IGUAL(data_de_texto("2026-08-14", &d), OK);
    ESPERA(data_igual(d, DIA));

    ESPERA_IGUAL(data_de_texto("2026-8-14",  &d), ERR_FORMATO);
    ESPERA_IGUAL(data_de_texto("2026-13-01", &d), ERR_FORMATO);
    ESPERA_IGUAL(data_de_texto("lixo",       &d), ERR_FORMATO);

    TERMINA();
}

void t_data_atravessa_o_mes(void)
{
    COMECA("somar dias atravessa mês, ano e bissexto");

    data_t d = data_soma_dias((data_t){2026,8,31}, 1);
    ESPERA(data_igual(d, (data_t){2026,9,1}));

    d = data_soma_dias((data_t){2026,12,31}, 1);
    ESPERA(data_igual(d, (data_t){2027,1,1}));

    d = data_soma_dias((data_t){2026,1,1}, -1);
    ESPERA(data_igual(d, (data_t){2025,12,31}));

    // 2028 é bissexto; 2026 não.
    d = data_soma_dias((data_t){2028,2,28}, 1);
    ESPERA(data_igual(d, (data_t){2028,2,29}));
    d = data_soma_dias((data_t){2026,2,28}, 1);
    ESPERA(data_igual(d, (data_t){2026,3,1}));

    ESPERA_IGUAL(data_dias_entre((data_t){2026,8,10}, (data_t){2026,8,14}), 4);
    ESPERA_IGUAL(data_dias_entre((data_t){2026,8,14}, (data_t){2026,8,10}), -4);

    TERMINA();
}

void t_dia_da_semana(void)
{
    COMECA("dia da semana — põe o 1 na coluna certa do calendário");

    ESPERA_TEXTO(data_semana_curta((data_t){2026,8,14}), "sex");
    ESPERA_TEXTO(data_semana_curta((data_t){2026,8,10}), "seg");
    ESPERA_TEXTO(data_semana_curta((data_t){2026,8,1}),  "sáb");
    ESPERA_TEXTO(data_mes_curto((data_t){2026,8,14}),    "ago");

    TERMINA();
}

// ── json ────────────────────────────────────────────────────────────
void t_json_le_o_contrato(void)
{
    COMECA("o leitor de JSON dá conta do contrato");

    const char *j = "{\"v\":1,\"t\":\"Dentista\",\"h\":\"14:00\","
                    "\"tp\":4,\"ok\":false,\"n\":-7}";
    char s[64]; int n; bool b;

    ESPERA(json_str(j, "t", s, sizeof s));  ESPERA_TEXTO(s, "Dentista");
    ESPERA(json_str(j, "h", s, sizeof s));  ESPERA_TEXTO(s, "14:00");
    ESPERA(json_int(j, "v", &n));           ESPERA_IGUAL(n, 1);
    ESPERA(json_int(j, "tp", &n));          ESPERA_IGUAL(n, 4);
    ESPERA(json_int(j, "n", &n));           ESPERA_IGUAL(n, -7);
    ESPERA(json_bool(j, "ok", &b));         ESPERA(!b);

    // RN-B8: campo que não existe devolve false.
    ESPERA(!json_str(j, "zzz", s, sizeof s));
    ESPERA(!json_int(j, "zzz", &n));

    TERMINA();
}

void t_json_nao_confunde_chave_com_valor(void)
{
    COMECA("chave não casa com pedaço de outra, nem com conteúdo");

    // "t" não casa com "titulo", e o valor "v" não vira chave.
    const char *j = "{\"titulo\":\"v\",\"t\":\"certo\",\"v\":9}";
    char s[32]; int n;

    ESPERA(json_str(j, "t", s, sizeof s));  ESPERA_TEXTO(s, "certo");
    ESPERA(json_int(j, "v", &n));           ESPERA_IGUAL(n, 9);
    ESPERA(json_str(j, "titulo", s, sizeof s)); ESPERA_TEXTO(s, "v");

    TERMINA();
}

// RN-B7: truncar, não rejeitar.
void t_json_trunca_em_vez_de_rejeitar(void)
{
    COMECA("RN-B7 · título comprido é truncado, não recusado");

    const char *j = "{\"t\":\"um titulo absurdamente comprido que nao cabe\"}";
    char s[11];

    ESPERA(json_str(j, "t", s, sizeof s));
    ESPERA_TEXTO(s, "um titulo ");   // 10 + terminador
    ESPERA_IGUAL(strlen(s), 10);

    TERMINA();
}

// ── cartão ──────────────────────────────────────────────────────────
static item_t item_de_teste(const char *id, const char *titulo)
{
    item_t it = { .tipo = TIPO_TAREFA, .origem = ORIGEM_AQUI };
    snprintf(it.id,     sizeof it.id,     "%s", id);
    snprintf(it.titulo, sizeof it.titulo, "%s", titulo);
    return it;
}

void t_cartao_grava_e_le_igual(void)
{
    COMECA("grava, lê de volta e vem igual");

    const hal_t *hal = pc_liga();
    cartao_prepara(hal);

    item_t it = item_de_teste("0914-oulu", "mandar histórico pro Oulu");
    it.vence = (data_t){2026,8,10};
    it.feita = false;
    ESPERA_IGUAL(cartao_grava_item(hal, DIA, &it), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, DIA, "0914-oulu", &lido), OK);
    ESPERA_TEXTO(lido.titulo, "mandar histórico pro Oulu");
    ESPERA_IGUAL(lido.tipo, TIPO_TAREFA);
    ESPERA(!lido.feita);
    ESPERA(data_igual(lido.vence, (data_t){2026,8,10}));

    TERMINA();
}

void t_cartao_tres_dias_doze_itens(void)
{
    COMECA("três dias, doze itens: lê, altera e relê igual");

    const hal_t *hal = pc_liga();
    cartao_prepara(hal);

    data_t dias[3] = { {2026,8,12}, {2026,8,13}, {2026,8,14} };
    for (int d = 0; d < 3; d++) {
        for (int i = 0; i < 4; i++) {
            char id[40], titulo[64];
            snprintf(id, sizeof id, "10%02d-item%d", i, i);
            snprintf(titulo, sizeof titulo, "item %d do dia %d", i, d);
            item_t it = item_de_teste(id, titulo);
            ESPERA_IGUAL(cartao_grava_item(hal, dias[d], &it), OK);
        }
    }

    data_t achados[8];
    int quantos = 0;
    ESPERA_IGUAL(cartao_lista_dias(hal, achados, 8, &quantos), OK);
    ESPERA_IGUAL(quantos, 3);

    item_t itens[8];
    ESPERA_IGUAL(cartao_lista_itens(hal, dias[1], 0, itens, 8, &quantos), OK);
    ESPERA_IGUAL(quantos, 4);

    // Altera um e relê.
    itens[2].feita = true;
    snprintf(itens[2].titulo, sizeof itens[2].titulo, "mudado");
    ESPERA_IGUAL(cartao_grava_item(hal, dias[1], &itens[2]), OK);

    item_t relido;
    ESPERA_IGUAL(cartao_le_item(hal, dias[1], itens[2].id, &relido), OK);
    ESPERA(relido.feita);
    ESPERA_TEXTO(relido.titulo, "mudado");

    // Os outros não foram tocados.
    ESPERA_IGUAL(cartao_lista_itens(hal, dias[1], 0, itens, 8, &quantos), OK);
    ESPERA_IGUAL(quantos, 4);

    TERMINA();
}

// RN-64: o corte de energia no meio da escrita.
void t_corte_de_energia_nao_deixa_meta_pela_metade(void)
{
    COMECA("RN-64 · corte de energia no meio da escrita não corrompe nada");

    const hal_t *hal = pc_liga();
    cartao_prepara(hal);

    item_t v1 = item_de_teste("1422-ipva", "pagar o IPVA");
    ESPERA_IGUAL(cartao_grava_item(hal, DIA, &v1), OK);

    // A bateria acaba entre escrever o .tmp e renomear.
    pc_falhar_renomear(true);
    item_t v2 = item_de_teste("1422-ipva", "ISTO NÃO PODE APARECER");
    ESPERA(cartao_grava_item(hal, DIA, &v2) != OK);
    pc_falhar_renomear(false);

    // O antigo está íntegro.
    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, DIA, "1422-ipva", &lido), OK);
    ESPERA_TEXTO(lido.titulo, "pagar o IPVA");

    // E o .tmp não ficou para trás.
    ESPERA(!pc_tem_arquivo(
        "/TINTO/itens/2026-08-14/1422-ipva/meta.json.tmp"));

    TERMINA();
}

// RN-62: sem meta.json o diretório não existe (sem lista de lixo).
void t_sem_meta_o_diretorio_nao_existe(void)
{
    COMECA("RN-62 · sem meta.json, o diretório não existe");

    const hal_t *hal = pc_liga();
    cartao_prepara(hal);

    item_t it = item_de_teste("0914-nota", "uma nota");
    cartao_grava_item(hal, DIA, &it);

    // O lixo que todo cartão acumula.
    pc_poe_arquivo("/TINTO/itens/2026-08-14/.Trash-1000/algo", "x");
    pc_poe_arquivo("/TINTO/itens/2026-08-14/.DS_Store", "x");
    pc_poe_arquivo("/TINTO/itens/2026-08-14/0914-nota/audio.wav", "RIFF");

    item_t itens[8];
    int quantos = 0;
    ESPERA_IGUAL(cartao_lista_itens(hal, DIA, 0, itens, 8, &quantos), OK);
    ESPERA_IGUAL(quantos, 1);
    ESPERA_TEXTO(itens[0].id, "0914-nota");

    TERMINA();
}

// RN-63: formato desconhecido é ignorado, nunca lido torto.
void t_versao_desconhecida_e_ignorada(void)
{
    COMECA("RN-63 · meta de versão futura é ignorado, não lido torto");

    const hal_t *hal = pc_liga();
    pc_poe_arquivo("/TINTO/itens/2026-08-14/9999-futuro/meta.json",
                     "{\"v\":99,\"t\":\"de outra versão\"}");
    pc_poe_arquivo("/TINTO/itens/2026-08-14/0001-ok/meta.json",
                     "{\"v\":1,\"t\":\"desta versão\"}");

    item_t it;
    ESPERA_IGUAL(cartao_le_item(hal, DIA, "9999-futuro", &it), ERR_FORMATO);

    item_t itens[8];
    int quantos = 0;
    ESPERA_IGUAL(cartao_lista_itens(hal, DIA, 0, itens, 8, &quantos), OK);
    ESPERA_IGUAL(quantos, 1);
    ESPERA_TEXTO(itens[0].titulo, "desta versão");

    TERMINA();
}

void t_listagem_do_cartao_distingue_vazio_de_falha(void)
{
    COMECA("listagem do cartão distingue vazio de falha");

    const hal_t *hal = pc_liga();
    ESPERA_IGUAL(cartao_prepara(hal), OK);

    item_t itens[2];
    int quantos = -1;
    ESPERA_IGUAL(cartao_lista_itens(hal, DIA, 0, itens, 2, &quantos), OK);
    ESPERA_IGUAL(quantos, 0);

    data_t dias[2];
    quantos = -1;
    ESPERA_IGUAL(cartao_lista_dias(hal, dias, 2, &quantos), OK);
    ESPERA_IGUAL(quantos, 0);

    pc_falhar_listar(true);
    quantos = -1;
    ESPERA_IGUAL(cartao_lista_itens(hal, DIA, 0, itens, 2, &quantos),
                 ERR_ARQUIVO);
    ESPERA_IGUAL(quantos, 0);

    quantos = -1;
    ESPERA_IGUAL(cartao_lista_dias(hal, dias, 2, &quantos), ERR_ARQUIVO);
    ESPERA_IGUAL(quantos, 0);

    TERMINA();
}

// Fim, local e dia inteiro (`f`, `l`, `di`) atravessam até o item.
void t_o_item_guarda_fim_local_e_dia_inteiro(void)
{
    COMECA("evento sobrevive ao cartão com fim, local e dia inteiro");

    const hal_t *hal = pc_liga();
    data_t dia = {2026, 8, 14};

    item_t it;
    memset(&it, 0, sizeof it);
    snprintf(it.id,     sizeof it.id,     "%s", "1400-dentista");
    snprintf(it.titulo, sizeof it.titulo, "%s", "Dentista");
    snprintf(it.hora,   sizeof it.hora,   "%s", "14:00");
    snprintf(it.fim,    sizeof it.fim,    "%s", "15:00");
    snprintf(it.local,  sizeof it.local,  "%s", "Rua Bahia, 210");
    it.tipo = TIPO_EVENTO;
    it.dia  = dia;

    ESPERA_IGUAL(cartao_grava_item(hal, dia, &it), OK);

    item_t lido;
    ESPERA_IGUAL(cartao_le_item(hal, dia, "1400-dentista", &lido), OK);
    ESPERA_TEXTO(lido.fim,   "15:00");
    ESPERA_TEXTO(lido.local, "Rua Bahia, 210");
    ESPERA(!lido.dia_inteiro);

    // RN-B8: meta velho, sem os campos novos, continua abrindo.
    pc_poe_arquivo("/TINTO/itens/2026-08-14/velho/meta.json",
                     "{\"v\":1,\"t\":\"Antigo\",\"h\":\"09:00\",\"tp\":4}");
    ESPERA_IGUAL(cartao_le_item(hal, dia, "velho", &lido), OK);
    ESPERA_TEXTO(lido.titulo, "Antigo");
    ESPERA_TEXTO(lido.fim,   "");
    ESPERA_TEXTO(lido.local, "");

    TERMINA();
}

// A faixa de horário: o dia inteiro é uma resposta, não uma falta.
void t_a_faixa_de_horario(void)
{
    COMECA("a faixa: início–fim, só início, ou o dia todo");

    item_t it;
    char faixa[20];

    memset(&it, 0, sizeof it);
    snprintf(it.hora, sizeof it.hora, "%s", "14:00");
    snprintf(it.fim,  sizeof it.fim,  "%s", "15:00");
    vista_faixa(&it, true, faixa, sizeof faixa);
    ESPERA_TEXTO(faixa, "14:00 – 15:00");

    // Sem fim, mostra o início e não inventa duração.
    it.fim[0] = '\0';
    vista_faixa(&it, true, faixa, sizeof faixa);
    ESPERA_TEXTO(faixa, "14:00");

    // Dia inteiro VENCE a hora.
    it.dia_inteiro = true;
    snprintf(it.hora, sizeof it.hora, "%s", "00:00");
    vista_faixa(&it, true, faixa, sizeof faixa);
    ESPERA_TEXTO(faixa, "o dia todo");

    memset(&it, 0, sizeof it);
    vista_faixa(&it, true, faixa, sizeof faixa);
    ESPERA_TEXTO(faixa, "");

    TERMINA();
}

void t_gravar_item_cria_as_pastas_do_caminho(void)
{
    COMECA("gravar item cria a pasta do dia e a do item");
    const hal_t *hal = pc_liga();
    pc_memoria_virgem();

    // A árvore do provisionamento para em /TINTO/itens.
    ESPERA_IGUAL(hal->criar_diretorio("/TINTO"), OK);
    ESPERA_IGUAL(hal->criar_diretorio("/TINTO/itens"), OK);

    data_t dia = { 2026, 8, 20 };
    item_t it = { .tipo = TIPO_ANOTACAO, .origem = ORIGEM_AQUI, .dia = dia };
    snprintf(it.id, sizeof it.id, "1030-nota");
    snprintf(it.titulo, sizeof it.titulo, "comprar pão");

    // O FatFs não cria caminho pelo meio: o dia e o item nascem na gravação
    // (o hal do PC antigo, com caminhos planos, não via isso).
    ESPERA_IGUAL(cartao_grava_item(hal, dia, &it), OK);
    ESPERA(pc_tem_arquivo("/TINTO/itens/2026-08-20/1030-nota/meta.json"));

    caminho_tipo_t tipo = CAMINHO_AUSENTE;
    ESPERA_IGUAL(hal->tipo_caminho("/TINTO/itens/2026-08-20", &tipo), OK);
    ESPERA_IGUAL(tipo, CAMINHO_DIRETORIO);
    ESPERA_IGUAL(hal->tipo_caminho("/TINTO/itens/2026-08-20/1030-nota", &tipo), OK);
    ESPERA_IGUAL(tipo, CAMINHO_DIRETORIO);
    TERMINA();
}

// Meta ilegível faz o item APARECER com marcador de defeito.
void t_meta_ilegivel_aparece_com_defeito(void)
{
    COMECA("Etapa 11 · meta ilegível aparece com defeito, e não some");

    const hal_t *hal = pc_liga();
    data_t dia = { 2026, 8, 21 };

    item_t bom;
    memset(&bom, 0, sizeof bom);
    snprintf(bom.id,     sizeof bom.id,     "%s", "0900-boa");
    snprintf(bom.titulo, sizeof bom.titulo, "%s", "esta se lê");
    bom.tipo = TIPO_ANOTACAO;
    bom.dia  = dia;
    ESPERA_IGUAL(cartao_grava_item(hal, dia, &bom), OK);

    // Sem "v" nenhum: é ilegível (RN-65), não versão futura (RN-63).
    pc_poe_arquivo("/TINTO/itens/2026-08-21/0901-ruim/meta.json",
                   "{\"t\":\"faltou tudo\",");

    item_t itens[8];
    int n = 0;
    ESPERA_IGUAL(cartao_lista_itens(hal, dia, 0, itens, 8, &n), OK);
    ESPERA_IGUAL(n, 2);

    bool achou_defeito = false;
    for (int i = 0; i < n; i++)
        if (strcmp(itens[i].id, "0901-ruim") == 0) achou_defeito = itens[i].defeito;

    ESPERA(achou_defeito);

    TERMINA();
}

// Todo ajuste tem chave no config: NULL num `%s` derruba o ESP32. Este
// teste reprova o build.
void t_dado_todo_ajuste_tem_chave_no_config(void)
{
    COMECA("nenhum ajuste entra no enum sem chave no config.json");
    ESPERA(cartao_chaves_completas());
    TERMINA();
}


// O `:` do id, que o FAT recusa, grava e volta inteiro (a falha era
// silenciosa: nada do Google chegava ao cartão).
void t_o_id_com_dois_pontos_grava_e_volta_inteiro(void)
{
    COMECA("id com `:` grava no cartão e volta inteiro da listagem");

    const hal_t *hal = pc_liga();
    data_t dia = { .ano = 2026, .mes = 8, .dia = 28 };

    // Os três formatos de id que existem.
    const char *IDS[3] = { "n:1", "e:5o8p2k1q3r", "t:MDI5MjkyNTE1MDI2" };

    for (int i = 0; i < 3; i++) {
        item_t it;
        memset(&it, 0, sizeof it);
        snprintf(it.id, sizeof it.id, "%s", IDS[i]);
        snprintf(it.titulo, sizeof it.titulo, "Item %d", i);
        it.dia  = dia;
        it.tipo = TIPO_EVENTO;

        ESPERA_IGUAL(cartao_grava_item(hal, dia, &it), OK);

        // Volta pelo id ORIGINAL.
        item_t lido;
        ESPERA_IGUAL(cartao_le_item(hal, dia, IDS[i], &lido), OK);
        ESPERA_TEXTO(lido.id, IDS[i]);
    }

    // A listagem devolve o id, não o nome da pasta.
    item_t itens[8];
    int n = 0;
    ESPERA_IGUAL(cartao_lista_itens(hal, dia, 0, itens, 8, &n), OK);
    ESPERA_IGUAL(n, 3);
    for (int i = 0; i < n; i++)
        ESPERA(strchr(itens[i].id, ':') != NULL);

    TERMINA();
}
