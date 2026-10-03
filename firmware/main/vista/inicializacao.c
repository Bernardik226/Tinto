#include "inicializacao.h"
#include "campos.h"
#include "../tela/texto.h"
#include <stdio.h>
#include <string.h>

// O texto vem do desenho normativo da inicialização; frase que não está
// lá não existe.

static void poe(char *destino, size_t max, const char *texto)
{
    snprintf(destino, max, "%s", texto);
}

static void opcao(vista_inicializacao_t *v, int i, const char *texto,
                  const char *nota)
{
    poe(v->opcoes[i], sizeof v->opcoes[i], texto);
    poe(v->notas[i],  sizeof v->notas[i],  nota);
    if (i + 1 > v->n_opcoes) v->n_opcoes = i + 1;
}

void vista_inicializacao(const estado_t *e, vista_inicializacao_t *out)
{
    memset(out, 0, sizeof *out);
    out->pontos = -1;
    poe(out->barra, sizeof out->barra, "MEMÓRIA");
    out->selecionada = e->inicio.cursor;

    // A HORA NÃO APARECE: sem NTP nem ajuste, o RTC é um contador, e dado
    // inventado numa tela de erro faz duvidar do resto. A bateria fica, é
    // medida. Volta quando houver hora confiável.
    out->hora[0]  = '\0';
    out->bateria  = e->bateria;
    out->wifi = vista_wifi_da_barra(e);
    out->sinc = vista_sinc_da_barra(e);

    switch (e->inicio.fase) {
    case INICIO_MEMORIA_AUSENTE:
        // Sem rodapé nem opção: nenhum botão faz nada aqui.
        out->icone_sd = true;
        out->icone_x  = true;
        poe(out->titulo, sizeof out->titulo, "Memória interna ausente");
        poe(out->corpo, sizeof out->corpo,
            "O microSD interno pode estar solto, ausente ou danificado.");
        poe(out->alerta, sizeof out->alerta,
            "Desligue o aparelho, abra a tampa e verifique o cartão em um "
            "computador.");
        break;

    case INICIO_MEMORIA_COMUNICACAO:
        out->icone_sd = true;
        poe(out->titulo, sizeof out->titulo, "Memória não responde");
        poe(out->corpo, sizeof out->corpo,
            "O cartão está no lugar, mas as leituras falharam. Contato ruim "
            "é a causa mais comum.");
        poe(out->alerta, sizeof out->alerta,
            "Desligue, reencaixe o cartão e ligue de novo.");
        break;

    case INICIO_MEMORIA_REPARO:
        poe(out->rotulo, sizeof out->rotulo, "Recuperação");
        poe(out->titulo, sizeof out->titulo, "Memória precisa de reparo");
        poe(out->corpo, sizeof out->corpo,
            "O cartão respondeu, mas os arquivos não puderam ser lidos com "
            "segurança.");
        // RN-6C: a saída não destrutiva vem primeiro E selecionada.
        opcao(out, 0, "verificar no computador",
              "mantém os arquivos para tentativa de reparo");
        opcao(out, 1, "formatar como novo", "apaga todo o cartão");
        // O rodapé diz o botão que existe (quem desliga é o power, não ◀).
        poe(out->rodape_esq, sizeof out->rodape_esq, "▲▼ escolher");
        poe(out->rodape_dir, sizeof out->rodape_dir, "OK confirmar");
        break;

    case INICIO_CONFIRMAR_FORMATAR:
        poe(out->rotulo, sizeof out->rotulo, "Confirmação");
        poe(out->titulo, sizeof out->titulo, "Apagar e configurar de novo?");

        // Os TRÊS grupos, não "todos os arquivos".
        poe(out->corpo, sizeof out->corpo,
            "Serão removidos deste aparelho: nome, notas e gravações; "
            "livros e progresso de leitura; Wi-Fi, conta e preferências.");

        // E o que NÃO sai: a agenda não vai junto.
        poe(out->alerta, sizeof out->alerta,
            "Nada será apagado da sua conta Google.");
        opcao(out, 0, "Não, voltar", "");
        opcao(out, 1, "Sim, restaurar aparelho", "");
        poe(out->rodape_esq, sizeof out->rodape_esq, "▲▼ escolher");
        poe(out->rodape_dir, sizeof out->rodape_dir, "OK confirmar");
        break;

    case INICIO_PREPARANDO:
        poe(out->barra, sizeof out->barra, "PREPARANDO");
        poe(out->titulo, sizeof out->titulo, "Preparando o Tinto");
        poe(out->corpo, sizeof out->corpo,
            "Não desligue nem retire a memória durante esta etapa.");

        // As três etapas. Sem rodapé: nenhum botão faz nada.
        poe(out->etapas[0], sizeof out->etapas[0], "Apagando conteúdo local");
        poe(out->etapas[1], sizeof out->etapas[1], "Preparando a memória");
        poe(out->etapas[2], sizeof out->etapas[2], "Reiniciando o aparelho");
        out->n_etapas    = 3;
        out->etapa_atual = e->inicio.etapa;
        break;

    case INICIO_MEMORIA_SOMENTE_LEITURA:
        out->icone_sd = true;
        poe(out->titulo, sizeof out->titulo, "Memória protegida");
        poe(out->corpo, sizeof out->corpo,
            "O cartão só permite leitura, então nada do que você falar pode "
            "ser guardado.");
        poe(out->alerta, sizeof out->alerta,
            "Desligue e verifique a trava do adaptador.");
        break;

    case INICIO_MEMORIA_CHEIA:
        out->icone_sd = true;
        poe(out->titulo, sizeof out->titulo, "Memória cheia");
        poe(out->corpo, sizeof out->corpo,
            "Não há espaço para guardar nada novo neste cartão.");
        poe(out->alerta, sizeof out->alerta,
            "Desligue e libere espaço em um computador.");
        break;

    case INICIO_FORMATO_FUTURO:
        poe(out->rotulo, sizeof out->rotulo, "Versão");
        poe(out->titulo, sizeof out->titulo, "Cartão de versão mais nova");
        poe(out->corpo, sizeof out->corpo,
            "Este cartão foi preparado por uma versão do Tinto que este "
            "aparelho ainda não conhece. Nada será alterado.");
        poe(out->alerta, sizeof out->alerta,
            "Atualize o aparelho ou use outro cartão.");
        break;

    case INICIO_BOAS_VINDAS:
        poe(out->barra, sizeof out->barra, "PRIMEIRO USO");
        out->logo = true;
        poe(out->titulo, sizeof out->titulo, "Configure seu Tinto");

        poe(out->corpo, sizeof out->corpo,
            "Vamos dar um nome ao aparelho e, se você quiser, conectar o "
            "Wi-Fi e a sua agenda do Google.");
        poe(out->rodape_esq, sizeof out->rodape_esq, "BACK voltar");
        poe(out->rodape_dir, sizeof out->rodape_dir, "OK continuar");
        break;

    // ── a rede ───────────────────────────────────────────────────────────
    // Pular é o ▶; o BACK da etapa seguinte reabre esta. O texto diz o que se
    // perde: o aparelho funciona sem rede.
    case INICIO_WIFI:
        poe(out->barra, sizeof out->barra, "REDE");

        // Voltando da conta com Wi-Fi já conectado: o ▶ segue em vez de "pular".
        if (e->rede == REDE_LIGADA) {
            poe(out->titulo, sizeof out->titulo, "Wi-Fi conectado");
            snprintf(out->corpo, sizeof out->corpo,
                     "O Tinto está na rede %s. Para usar outra, escolha "
                     "trocar.", e->wifi_atual[0] ? e->wifi_atual : "atual");
            poe(out->rodape_esq, sizeof out->rodape_esq, "▶ continuar");
            poe(out->rodape_dir, sizeof out->rodape_dir, "OK trocar rede");
            break;
        }

        poe(out->titulo, sizeof out->titulo, "Conecte-se ao Wi-Fi");
        poe(out->corpo, sizeof out->corpo,
            "Com rede, o Tinto traz a sua agenda Google, entende comandos "
            "de voz e baixa livros do acervo. Sem rede, você ainda pode "
            "jogar e visualizar o que já está no aparelho.");
        poe(out->rodape_esq, sizeof out->rodape_esq, "▶ pular");
        poe(out->rodape_dir, sizeof out->rodape_dir, "OK escolher rede");
        break;

    // ── a conta ──────────────────────────────────────────────────────────
    // Logo depois do Wi-Fi o aparelho ainda se registra: segundos.
    case INICIO_CONTA:
        poe(out->barra, sizeof out->barra, "CONTA");

        if (estado_servidor_sem_resposta(e)) {
            poe(out->titulo, sizeof out->titulo, "Sem resposta do servidor");
            poe(out->corpo, sizeof out->corpo,
                "O Tinto não conseguiu falar com o seu servidor. Confira a "
                "rede e tente de novo, ou conecte a conta depois em Ajustes.");
            poe(out->rodape_esq, sizeof out->rodape_esq, "▶ pular");
            poe(out->rodape_dir, sizeof out->rodape_dir, "OK tentar de novo");
            break;
        }

        if (!e->tem_token) {
            // Espera: sem pular e sem rodapé, só os pontinhos.
            poe(out->titulo, sizeof out->titulo, "Conectando\nao servidor");
            out->pontos = (int)((e->agora_ms / 1000u) % 4u);
            poe(out->corpo, sizeof out->corpo,
                "O Tinto está se apresentando ao seu servidor. Esta tela "
                "continua sozinha.");
            break;
        }

        // Conta já vinculada: segue em vez de oferecer o vínculo de novo.
        if (e->nome[0]) {
            poe(out->titulo, sizeof out->titulo, "Conta conectada");
            snprintf(out->corpo, sizeof out->corpo,
                     "O Tinto está vinculado a %s, e a sua agenda já "
                     "sincroniza.", e->nome);
            poe(out->rodape_esq, sizeof out->rodape_esq, "▶ continuar");
            poe(out->rodape_dir, sizeof out->rodape_dir, "OK continuar");
            break;
        }

        // O servidor respondeu: tela própria, e só o OK segue.
        if (!e->inicio.servidor_visto) {
            poe(out->titulo, sizeof out->titulo, "Conectado ao servidor");
            poe(out->corpo, sizeof out->corpo,
                "O Tinto se apresentou ao seu servidor com sucesso. Agora "
                "falta a sua conta Google.");
            poe(out->rodape_dir, sizeof out->rodape_dir, "OK continuar");
            break;
        }

        poe(out->titulo, sizeof out->titulo, "Conecte a sua conta");
        poe(out->corpo, sizeof out->corpo,
            "O Tinto lê a sua agenda do Google e escreve nela o que você "
            "falar. A conta se conecta pelo celular — o aparelho mostra um "
            "código e você digita no aplicativo.");
        poe(out->rodape_esq, sizeof out->rodape_esq, "▶ pular");
        poe(out->rodape_dir, sizeof out->rodape_dir, "OK conectar");
        break;

    case INICIO_CONFIRMAR_DONO:
        poe(out->barra, sizeof out->barra, "PROPRIETÁRIO");
        snprintf(out->nome, sizeof out->nome, "Prazer,\n%s.",
                 e->inicio.nome_pendente);
        poe(out->nota, sizeof out->nota,
            "Este nome e o identificador estável acompanham a memória "
            "interna. Mudar o nome depois não troca o proprietário.");
        poe(out->rodape_esq, sizeof out->rodape_esq, "BACK corrigir");
        poe(out->rodape_dir, sizeof out->rodape_dir, "OK confirmar");
        break;

    case INICIO_DATA_HORA:
        poe(out->barra, sizeof out->barra, "DATA E HORA");
        poe(out->titulo, sizeof out->titulo, "Que dia é hoje?");
        poe(out->corpo, sizeof out->corpo,
            "Sem internet o Tinto não descobre a hora sozinho. "
            "◀ ▶ escolhe o campo, ▲ ▼ muda o valor.");
        snprintf(out->campos[0], sizeof out->campos[0], "%02d", e->inicio.dia);
        snprintf(out->campos[1], sizeof out->campos[1], "%02d", e->inicio.mes);
        snprintf(out->campos[2], sizeof out->campos[2], "%04d", e->inicio.ano);
        snprintf(out->campos[4], sizeof out->campos[4], "%02d", e->inicio.minuto);
        poe(out->etiquetas[0], sizeof out->etiquetas[0], "dia");
        poe(out->etiquetas[1], sizeof out->etiquetas[1], "mês");
        poe(out->etiquetas[2], sizeof out->etiquetas[2], "ano");
        // Em 12 h o am/pm desce para a etiqueta: na linha dos números não cabe.
        if (e->config.valor[AJUSTE_HORA24]) {
            snprintf(out->campos[3], sizeof out->campos[3], "%02d",
                     e->inicio.hora);
            poe(out->etiquetas[3], sizeof out->etiquetas[3], "hora");
        } else {
            int doze = e->inicio.hora % 12;
            snprintf(out->campos[3], sizeof out->campos[3], "%02d",
                     doze ? doze : 12);
            poe(out->etiquetas[3], sizeof out->etiquetas[3],
                e->inicio.hora < 12 ? "am" : "pm");
        }
        poe(out->etiquetas[4], sizeof out->etiquetas[4], "min");
        out->n_campos    = 5;
        out->campo_ativo = e->inicio.campo;
        poe(out->rodape_esq, sizeof out->rodape_esq, "▲▼ valor");
        poe(out->rodape_dir, sizeof out->rodape_dir, "OK confirmar");
        break;

    // ── pronto, e em que estado ──────────────────────────────────────────
    // Os dois passos finais se pulam: diz o que ficou, o que falta, o que se
    // perde e ONDE resolver.
    case INICIO_CONCLUSAO: {
        bool tem_rede  = e->rede == REDE_LIGADA;
        bool tem_conta = e->nome[0] != '\0';

        poe(out->barra, sizeof out->barra, "PRIMEIRO USO");

        if (tem_rede && tem_conta) {
            snprintf(out->titulo, sizeof out->titulo,
                     "Tudo pronto%s%s.",
                     e->inicio.nome_pendente[0] ? ", " : "",
                     e->inicio.nome_pendente);
            poe(out->corpo, sizeof out->corpo,
                "Rede conectada e conta vinculada. O seu dia já está aqui.");
        } else if (tem_rede) {
            poe(out->titulo, sizeof out->titulo, "Quase lá");
            poe(out->corpo, sizeof out->corpo,
                "A rede está conectada, mas falta a sua conta Google. Sem "
                "ela o Tinto não mostra a sua agenda, e o botão de voz "
                "fica travado. Ajustes > Minha conta.");
        } else {
            // Uma pendência só: a conta nem foi oferecida.
            poe(out->titulo, sizeof out->titulo, "Quase lá");
            poe(out->corpo, sizeof out->corpo,
                "Falta conectar à rede. Sem ela não dá para vincular a "
                "conta, e o Tinto fica só com o que estiver no cartão. "
                "Ajustes > Wi-Fi.");
        }

        // O kicker diz que a jornada ACABOU.
        poe(out->rotulo, sizeof out->rotulo, "configuração concluída");

        poe(out->estados[0], sizeof out->estados[0], "Memória · pronta");
        snprintf(out->estados[1], sizeof out->estados[1], "Internet · %s",
                 tem_rede ? "conectada" : "fazer depois");
        snprintf(out->estados[2], sizeof out->estados[2], "Google · %s",
                 tem_conta ? "vinculada" : "fazer depois");
        out->n_estados = 3;
        poe(out->rodape_esq, sizeof out->rodape_esq, "power desligar");
        poe(out->rodape_dir, sizeof out->rodape_dir, "OK começar");
        break;
    }

    default:
        break;
    }

    // O rodapé diz o que o BACK faz agora: a volta entre etapas evita que
    // errar o nome custe reiniciar. Nas telas de mídia, sem volta, a saída é o
    // power.
    if (inicio_pode_voltar(e->inicio.fase) &&
        strcmp(out->rodape_esq, "power desligar") == 0)
        poe(out->rodape_esq, sizeof out->rodape_esq, "BACK voltar");

}
