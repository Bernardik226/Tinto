// firmware/testes/t_contrato.c — a fronteira com o backend.
// Chaves curtas, um nível, nenhum campo que mude de tipo.
#include "teste.h"
#include "dado/nuvem.h"
#include "dado/contrato.h"
#include <string.h>

void t_o_contrato_le_um_evento(void)
{
    COMECA("contrato · um evento vira item, com local e fim");

    item_t it;
    ESPERA(contrato_item("{\"id\":\"g:a1b2c3\",\"t\":\"Dentista\","
                         "\"h\":\"14:00\",\"f\":\"15:00\","
                         "\"l\":\"Rua Bahia, 210\",\"o\":\"g\"}", &it));

    ESPERA_TEXTO(it.id, "g:a1b2c3");
    ESPERA_TEXTO(it.titulo, "Dentista");
    ESPERA_TEXTO(it.hora, "14:00");
    ESPERA_TEXTO(it.fim, "15:00");
    ESPERA_TEXTO(it.local, "Rua Bahia, 210");
    ESPERA_IGUAL(it.tipo, TIPO_EVENTO);
    ESPERA_IGUAL(it.origem, ORIGEM_GOOGLE);

    TERMINA();
}

// `di` VENCE a hora: quem limpa o dado sujo é a fronteira.
void t_dia_inteiro_vence_a_hora(void)
{
    COMECA("contrato · dia inteiro ignora a hora, mesmo se ela vier");

    item_t it;
    ESPERA(contrato_item("{\"id\":\"g:f6g7h8\",\"t\":\"Entrega\","
                         "\"di\":true,\"h\":\"09:00\",\"o\":\"g\"}", &it));

    ESPERA(it.dia_inteiro);
    ESPERA_TEXTO(it.hora, "");

    TERMINA();
}

void t_a_tarefa_traz_data_e_estado(void)
{
    COMECA("contrato · a tarefa traz a data e se está feita");

    item_t it;
    ESPERA(contrato_item("{\"id\":\"t:MTk4Nz\",\"t\":\"Comprar fita kapton\","
                         "\"d\":\"2026-08-05\",\"ok\":false,\"o\":\"g\"}", &it));

    ESPERA_IGUAL(it.tipo, TIPO_TAREFA);
    ESPERA_IGUAL(it.vence.ano, 2026);
    ESPERA_IGUAL(it.vence.mes, 8);
    ESPERA_IGUAL(it.vence.dia, 5);
    ESPERA(!it.feita);

    TERMINA();
}

// Item sem id não entra: não há como gravá-lo nem achá-lo de volta.
void t_item_sem_id_nao_entra(void)
{
    COMECA("contrato · item sem id não entra");

    item_t it;
    ESPERA(!contrato_item("{\"t\":\"sem id nenhum\"}", &it));

    TERMINA();
}

void t_o_pull_percorre_a_lista_inteira(void)
{
    COMECA("contrato · o cursor percorre todos os itens da resposta");

    const char *json =
        "{\"itens\":[{\"id\":\"g:1\",\"t\":\"um\"},"
        "{\"id\":\"t:2\",\"t\":\"dois\"},"
        "{\"id\":\"g:3\",\"t\":\"três\"}],\"cursor\":\"abc\"}";

    const char *p = strchr(json, '[');
    char objeto[256];
    int n = 0;
    while (contrato_proximo(&p, objeto, sizeof objeto)) {
        item_t it;
        if (contrato_item(objeto, &it)) n++;
    }

    ESPERA_IGUAL(n, 3);

    TERMINA();
}


// A lista traz `n` e `k`: "Compras · 2 de 7".
void t_a_lista_traz_quantos_tem_e_quantos_foram(void)
{
    COMECA("contrato · a lista diz quanto falta ali dentro");

    item_t it;
    ESPERA(contrato_item(
        "{\"id\":\"l:compras\",\"t\":\"Compras da semana\",\"n\":7,\"k\":2}",
        &it));

    ESPERA_IGUAL(it.tipo, TIPO_LISTA);
    ESPERA_IGUAL(it.lista_n, 7);
    ESPERA_IGUAL(it.lista_k, 2);

    // Sem os campos, zero: "não medida", e a Agenda não desenha contagem.
    ESPERA(contrato_item("{\"id\":\"l:outra\",\"t\":\"Outra\"}", &it));
    ESPERA_IGUAL(it.lista_n, 0);
    ESPERA_IGUAL(it.lista_k, 0);

    TERMINA();
}

// O item diz de que fala veio, pelo pull também (RN-48).
void t_o_item_diz_de_que_fala_ele_veio(void)
{
    COMECA("contrato · o item traz a nota de onde veio");

    item_t it;
    ESPERA(contrato_item(
        "{\"id\":\"t:x1\",\"t\":\"comprar pasta\",\"nota\":\"nt:0912\"}", &it));
    ESPERA_TEXTO(it.nota, "nt:0912");

    // Sem a chave, vazio: "veio do Google".
    ESPERA(contrato_item("{\"id\":\"g:a1\",\"t\":\"Dentista\"}", &it));
    ESPERA_TEXTO(it.nota, "");

    TERMINA();
}

// O servidor só pode ser HTTPS: o endereço vem do cartão, e `http://`
// entregaria o token em claro.
void t_o_servidor_so_pode_ser_https(void)
{
    COMECA("nuvem · endereço que não é https é recusado, e cai no padrão");

    const hal_t *hal = pc_liga();

    // Um cartão adulterado.
    pc_poe_arquivo("/TINTO/sistema/nuvem.json",
                   "{\"v\":1,\"servidor\":\"http://intruso.exemplo.com\","
                   "\"device_id\":\"abc\",\"prova\":\"xyz\"}");

    nuvem_cred_t c;
    ESPERA_IGUAL(nuvem_carrega(hal, &c), OK);

    // O endereço inseguro não é usado: volta o de fábrica.
    ESPERA(strncmp(c.servidor, "https://", 8) == 0);
    ESPERA(strstr(c.servidor, "intruso") == NULL);

    TERMINA();
}

// Os buffers do device são o `max_length` do backend + 1 (o terminador):
// menos corta no meio de UTF-8, mais desperdiça RAM.
void t_o_contrato_cabe_nos_buffers_do_device(void)
{
    COMECA("contrato · o que o servidor aceita cabe no que o device guarda");

    item_t it;

    // Os campos que atravessam o contrato.
    ESPERA_IGUAL((int)sizeof it.id,     39 + 1);
    ESPERA_IGUAL((int)sizeof it.titulo, 127 + 1);
    ESPERA_IGUAL((int)sizeof it.hora,   5 + 1);
    ESPERA_IGUAL((int)sizeof it.fim,    5 + 1);
    ESPERA_IGUAL((int)sizeof it.local,  95 + 1);
    ESPERA_IGUAL((int)sizeof it.agenda, 31 + 1);

    TERMINA();
}


// A ocorrência de uma série chega marcada, e a marca sobrevive ao cartão.
void t_o_contrato_sabe_o_que_se_repete(void)
{
    COMECA("contrato · a ocorrência de uma série chega marcada");

    item_t it;
    ESPERA(contrato_item("{\"id\":\"g:serie_20260911\",\"t\":\"Academia\","
                         "\"h\":\"07:00\",\"r\":true,\"o\":\"g\"}", &it));
    ESPERA(it.repete);

    // Campo ausente é "não", nunca "não sei".
    item_t sozinho;
    ESPERA(contrato_item("{\"id\":\"g:unico\",\"t\":\"Dentista\"}", &sozinho));
    ESPERA(!sozinho.repete);

    TERMINA();
}

// Dois arrays na mesma resposta: o cursor não atravessa de um para o
// outro (o dia de consulta ia parar no cartão).
void t_o_cursor_nao_atravessa_o_fim_do_array(void)
{
    COMECA("contrato · o cursor para no ] do array, e não invade o próximo");

    const char *json =
        "{\"itens\":[],\"dia\":[{\"id\":\"g:festa\",\"t\":\"Festa\"}]}";

    const char *itens = strchr(strstr(json, "\"itens\""), '[');
    const char *fim = contrato_fim_do_array(itens);
    ESPERA(fim != NULL);

    char objeto[320];
    const char *p = itens;
    ESPERA(!contrato_proximo_ate(&p, fim, objeto, sizeof objeto));

    // E o array seguinte é lido sozinho.
    const char *do_dia = strchr(strstr(json, "\"dia\""), '[');
    const char *fim_dia = contrato_fim_do_array(do_dia);
    p = do_dia;
    ESPERA(contrato_proximo_ate(&p, fim_dia, objeto, sizeof objeto));

    item_t it;
    ESPERA(contrato_item(objeto, &it));
    ESPERA_TEXTO(it.titulo, "Festa");
    ESPERA(!contrato_proximo_ate(&p, fim_dia, objeto, sizeof objeto));

    TERMINA();
}
