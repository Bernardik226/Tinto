#include "obra.h"
#include "chrome.h"
#include "grid.h"
#include "rolagem.h"
#include "../tela/texto.h"

#define CX 10
#define CL 72
#define CA 108
#define TOPO (GRID_BARRA_A + 8)
#define FUNDO (GRID_RODAPE_Y - 4)

static int corpo(bitmap_t *bm,const vista_obra_t*v,int y)
{
    gfx_ret(bm,CX,y,CL,CA,false);
    int tx=CX+CL+9,tw=TELA_L-GRID_MARGEM-tx,ty=y;
    ty=gfx_paragrafo(bm,tx,ty,tw,4,F_EDITORIAL,v->titulo)+5;
    ty=gfx_paragrafo(bm,tx,ty,tw,3,F_MIUDA,v->autor)+7;
    gfx_texto(bm,tx,ty,F_MIUDA,v->tipo);
    y+=CA+12;
    gfx_texto(bm,GRID_MARGEM,y,F_TITULO,"Sobre");
    y+=gfx_altura_linha(F_TITULO)+5;
    // Limite acima do que a sinopse pode ocupar: o excesso vira rolagem.
    y=gfx_paragrafo(bm,GRID_MARGEM,y,GRID_UTIL-8,32,F_CORPO_P,v->sinopse)+10;
    gfx_icone(bm,GRID_MARGEM,y,ICO_PESSOA);
    gfx_texto_ate(bm,GRID_MARGEM+22,y,F_MIUDA,v->autor,GRID_UTIL-22); y+=22;
    gfx_icone(bm,GRID_MARGEM,y,ICO_TEMPO_LEITURA);
    gfx_texto(bm,GRID_MARGEM+22,y,F_MIUDA,v->tempo); y+=22;
    gfx_texto(bm,GRID_MARGEM,y,F_MIUDA,v->adicionada); y+=18;
    gfx_texto(bm,GRID_MARGEM,y,F_MIUDA,v->progresso); y+=gfx_altura_linha(F_MIUDA);
    return y;
}

static int altura(const bitmap_t *bm,const vista_obra_t*v)
{
    bitmap_t *r=rolagem_rascunho(bm);
    return r ? corpo(r,v,0) : 0;
}

int tela_obra_paradas(bitmap_t *bm,const vista_obra_t*v)
{
    return rolagem_suave_paradas(altura(bm,v),FUNDO-TOPO);
}

bool tela_obra_capa_area(const vista_obra_t*v,int*x,int*y,int*l,int*a)
{
    if(v->cursor>0)return false;
    if(x)*x=CX;
    if(y)*y=TOPO;
    if(l)*l=CL;
    if(a)*a=CA;
    return true;
}

void tela_obra(bitmap_t *bm,const vista_obra_t*v)
{
    gfx_limpa(bm,false);
    int alto=altura(bm,v),area=FUNDO-TOPO;
    int d=rolagem_suave_desloc(v->cursor,alto,area);
    corpo(bm,v,TOPO-d);
    gfx_limpa_ret(bm,0,0,TELA_L,GRID_BARRA_A+3);
    gfx_limpa_ret(bm,0,FUNDO,TELA_L,TELA_A-FUNDO);
    barra_t b={v->barra,v->hora,v->bateria,v->wifi,false,v->sinc};chrome_barra(bm,&b);
    rolagem_setas_laterais(bm,d,alto,area,TOPO,FUNDO);
    chrome_rodape(bm,"BACK voltar","OK ler",false);
}
