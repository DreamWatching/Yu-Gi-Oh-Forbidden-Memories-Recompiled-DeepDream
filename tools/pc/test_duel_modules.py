"""Exercise actual independent portraits and option registry without game data."""
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'tmp/pc/duel-modules-test'
OUT.mkdir(parents=True, exist_ok=True)
MODS = ROOT / 'examples/mods'
fixture = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "duel-portraits/duel_portraits.c"
#include "duel-options/option_registry.h"
#include "duel-options/builtin_options.c"
void Duel_InitScene(void) {}
void Duel_DrawLifePointsAndDeckCounts(DisplayObject *p) {}
u8 D_8009B26C=0x43, D_8009B1D5=0, D_8009B26E=0x81;
s8 gDuel_bOpponentID=2;
DuelSideState D_800E9FF0[2];
u8 D_801D0000[0x4000], D_801D1200[0x2000];
static int enabled=1, contributor_on=1, reads, writes, settings=8000, provider_on, pixel_calls;
static uint64_t clock_us;
const char *Tables_DuelistShortName(int id) { return id==2 ? "Teana" : "Heishin"; }
void SaveSlots_StateName(const unsigned char *state,char *out,size_t size)
{ snprintf(out,size,"%s",state==D_801D1200+0x1000 ? "Guest" : "Yugi"); }
static int is_on(const MemoriesModHost *h) { return strcmp(h->id,"duel-portraits") ? contributor_on : enabled; }
static int setting(const MemoriesModHost *h,const char *key,int fallback)
{ (void)h;(void)key;(void)fallback;return settings; }
static void set(const MemoriesModHost *h,const char *key,int value)
{ (void)h;(void)key;writes++;settings=value; }
static uint64_t now(const MemoriesModHost *h) { (void)h;return clock_us; }
static int start(const MemoriesModHost *h,const char *path) { (void)h;(void)path;return 100; }
static int read_disc(const MemoriesModHost *h,int lba,int n,void *out)
{ (void)h;(void)lba;reads++;memset(out,0,n*2048);return n; }
static int tag_provider(DuelParticipants *p,size_t size)
{
    if(!provider_on)return 0;
    assert(size==sizeof(*p));memset(p,0,sizeof(*p));p->abi=1;p->size=sizeof(*p);
    p->active_side=1;p->transition_side=-1;p->transition_progress=255;
    strcpy(p->side[0].name,"Heishin");p->side[0].portrait=6;
    strcpy(p->side[1].name,"Seto");p->side[1].portrait=7;return 1;
}
static const DuelOptionsService test_service={1,sizeof(DuelOptionsService),DuelOptions_Add,DuelOptions_Rows,DuelOptions_Set,DuelOptions_Snapshot,DuelOptions_Remove};
static void *find(const MemoriesModHost *h,const char *key)
{
    (void)h;
    if(!strcmp(key,"duel-options:options_v1"))return (void *)&test_service;
    assert(!strcmp(key,"tag-duel-menu:participants_v1"));return (void *)tag_provider;
}
static void pixels(const MemoriesModHost *h,int x,int y,int w,int ht,const uint32_t *p,int sw,int sh,unsigned b,unsigned a)
{ (void)h;(void)x;(void)y;(void)w;(void)ht;(void)p;(void)sw;(void)sh;(void)b;(void)a;pixel_calls++; }
static void fill(const MemoriesModHost *h,int x,int y,int w,int ht,uint32_t c,unsigned a)
{ (void)h;(void)x;(void)y;(void)w;(void)ht;(void)c;(void)a; }
static void text(const MemoriesModHost *h,int x,int y,const char *t,uint32_t c,int s)
{ (void)h;(void)x;(void)y;(void)t;(void)c;(void)s; }
static int width(const MemoriesModHost *h,const char *t,int s) { (void)h;return strlen(t)*8*s; }
static void size(const MemoriesModHost *h,int *w,int *ht,int *s) { (void)h;*w=1366;*ht=768;*s=1; }
int main(void)
{
    MemoriesModHost h={0},owner={0};DuelParticipants saved;
    DuelOption option={0};DuelOptionRow rows[2];
    static DuelOptionSnapshot snapshot;
    h.id="duel-portraits";h.applied=is_on;h.now_us=now;h.find=find;
    h.disc_file_start=start;h.disc_read=read_disc;h.overlay_size=size;
    h.fill=fill;h.draw_text=text;h.text_width=width;host=&h;draw_pixels=pixels;duel_started=1;
    D_800E9FF0[0].deck_draw_cursor=5;D_800E9FF0[1].deck_draw_cursor=8;
    memset(D_800E9FF0[0].hand,-1,5);D_800E9FF0[0].hand[0]=0;
    original_init_scene=(void*)Duel_InitScene;original_draw_lp=(void*)Duel_DrawLifePointsAndDeckCounts;
    init_scene();assert(!duel_started && !query());
    D_8009B26E=0x80;draw_lp(NULL);assert(!duel_started && !query());
    D_8009B26E=0x81;draw_lp(NULL);assert(duel_started);
    assert(query());assert(!strcmp(view.side[0].name,"Yugi"));
    assert(!strcmp(view.side[1].name,"Teana"));assert(view.side[0].portrait==-1);
    assert(view.side[0].hand==1 && view.side[1].deck==32);
    saved=view;overlay();assert(pixel_calls==2 && reads==2);
    overlay();assert(reads==2);assert(!memcmp(&saved,&view,sizeof(saved)));
    provider_on=1;assert(query());assert(!strcmp(view.side[0].name,"Heishin"));
    provider_on=0;assert(query());assert(!strcmp(view.side[0].name,"Yugi"));
    gDuel_bOpponentID=-1;assert(query());assert(!strcmp(view.side[1].name,"Guest"));
    assert(!strcmp(view.side[1].control,"PLAYER 2") && view.side[1].portrait==-1);
    enabled=0;assert(!query() && !available());enabled=1;
    D_8009B26C=8;assert(!query());D_8009B26C=0x43;
    assert(colour(0x7fff)==0xffffffffu && colour(0)==0);clear_cache();
    owner.id="test-mod";owner.applied=is_on;owner.setting=setting;owner.set_setting=set;
    option.abi=1;option.size=sizeof(option);option.modes=3;
    strcpy(option.key,"starting_lp");strcpy(option.tab,"Battle Rules");strcpy(option.label,"Starting Life");
    option.default_value=8000;option.choice_count=2;
    option.choices[0].value=4000;strcpy(option.choices[0].label,"4000 LP");
    option.choices[1].value=8000;strcpy(option.choices[1].label,"8000 LP");
    assert(DuelOptions_Add(&owner,&option));assert(!DuelOptions_Add(&owner,&option));
    assert(DuelOptions_Rows(1,rows,2)==1 && rows[0].value==8000);
    strcpy(option.label,"Changed contributor memory");
    assert(DuelOptions_Rows(1,rows,2)==1 && !strcmp(rows[0].option.label,"Starting Life"));
    assert(DuelOptions_Rows(1,NULL,0)==1 && DuelOptions_Rows(1,NULL,1)==-1);
    assert(DuelOptions_Rows(4,rows,2)==0);assert(DuelOptions_Rows(3,rows,2)==-1);
    assert(!DuelOptions_Set("test-mod","starting_lp",20000) && writes==0);
    assert(DuelOptions_Set("test-mod","starting_lp",4000) && writes==1);
    assert(DuelOptions_Snapshot(1,&snapshot,sizeof(snapshot)) && snapshot.rows[0].value==4000);
    assert(DuelOptions_Freeze(1));
    settings=8000;assert(snapshot.rows[0].value==4000);
    { int value=-1;
      assert(DuelOptions_MatchValue("test-mod","starting_lp",&value) && value==4000);
      assert(!DuelOptions_MatchValue("other-mod","starting_lp",&value));
      assert(!DuelOptions_MatchValue("test-mod","starting_lp",NULL));
      DuelOptions_ClearMatch();
      assert(!DuelOptions_MatchValue("test-mod","starting_lp",&value));
    }
    settings=999;assert(DuelOptions_Rows(1,rows,2)==1 && rows[0].value==8000);
    contributor_on=0;assert(DuelOptions_Rows(1,rows,2)==0);
    assert(!DuelOptions_Set("test-mod","starting_lp",4000));contributor_on=1;
    DuelOptions_Remove(&owner);assert(DuelOptions_Rows(1,rows,2)==0);
    strcpy(option.key,"order");assert(!DuelOptions_Add(&owner,&option));
    strcpy(option.key,"starting_lp");option.choices[1].value=4000;assert(!DuelOptions_Add(&owner,&option));
    option.choices[1].value=8000;option.abi=99;assert(!DuelOptions_Add(&owner,&option));
    option.abi=1;memset(option.label,'x',sizeof(option.label));assert(!DuelOptions_Add(&owner,&option));
    strcpy(option.label,"Starting Life");
    for(int i=0;i<DUEL_OPTION_MAX;i++) {
        snprintf(option.key,sizeof(option.key),"rule_%d",i);assert(DuelOptions_Add(&owner,&option));
    }
    strcpy(option.key,"overflow");assert(!DuelOptions_Add(&owner,&option));
    assert(DuelOptions_Rows(1,rows,2)==DUEL_OPTION_MAX);DuelOptions_Clear();
    owner.find=find;DuelOptions_RegisterBuiltins(&owner);
    assert(DuelOptions_Snapshot(1,&snapshot,sizeof(snapshot)) && snapshot.count==4);
    assert(!strcmp(snapshot.rows[0].option.key,"starting_life") && snapshot.rows[0].value==8000);
    assert(snapshot.rows[1].option.choice_count==9 && snapshot.rows[2].option.choice_count==2);
    DuelOptions_RegisterBuiltins(&owner);assert(DuelOptions_Rows(1,NULL,0)==4);
    DuelOptions_RemoveBuiltins(&owner);assert(DuelOptions_Rows(1,NULL,0)==0);
    puts("Independent portraits and duel option registry checks passed");return 0;
}
'''
c = OUT / 'test.c'
c.write_text(fixture)
exe = OUT / 'test.exe'
subprocess.run([shutil.which('i686-w64-mingw32-clang'), '-std=gnu11', '-O2', '-Wno-gnu-folding-constant','-Wno-incompatible-library-redeclaration',
                '-I'+str(ROOT/'src'), '-I'+str(MODS), str(c),
                str(MODS/'duel-options/option_registry.c'), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
