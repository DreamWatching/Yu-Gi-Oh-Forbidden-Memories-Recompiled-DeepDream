"""Actual separated lifecycle: all starters, LP, field cadence and solo AI."""
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'tmp/pc/duel-match-test'
OUT.mkdir(parents=True,exist_ok=True)
source=(ROOT/'examples/mods/duel-options/match_runtime.c').read_text()
def function(decl):
    start=source.index(decl);brace=source.index('{',start);end=brace+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
start=source.index('typedef struct {');end=source.index('} TagMatchRules;',start)+len('} TagMatchRules;')
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pc/mods/modapi.h"
#include "game/duel_side_state.h"
#include "shared/duel_match.h"
#include "shared/tag_duel_service.h"
#include "tag-duel-menu/duel_rules.h"
#include "tag-duel-menu/tag_rank.h"
#define MAIN_MODE_DUEL 3
#define TRIG_ANGLE_HALF_TURN 2048
#define TRIG_ANGLE_QUARTER_TURN 1024
#define DUEL_OPTION_TAG 2
#define DUEL_OPTION_SOLO 1
typedef int s32;
static u8 D_8009B26C=3,gDuel_bTerrain;
u8 D_8009B1D5;
DuelSideState D_800E9FF0[2];
DuelSideState *D_8009B1C8;
static unsigned D_8009B16C;
static u8 grid[20];
u8 *D_8009B22C;
#define D_800907D8 grid
static struct {int angle;} D_800F2848;
static int frames,rolls,attack_calls,enabled=1,freeze_mode;
static unsigned char D_801A8000[1],D_801A9800[1];
static struct {unsigned char *script_base;} gAiScript_State;
static int on(const MemoriesModHost *h) { (void)h;return enabled; }
static void *find(const MemoriesModHost *h,const char *s) { (void)h;(void)s;return NULL; }
static void log_line(const MemoriesModHost *h,const char *s,...) { (void)h;(void)s; }
static MemoriesModHost host={0};
static const MemoriesModHost *tag_host=&host;
static void init_base(void) { D_8009B1D5=0; }
static void sides_base(void) {
    memset(D_800E9FF0,0,sizeof(D_800E9FF0));
    for(int i=0;i<2;i++){D_800E9FF0[i].life_points.signed_value=8000;D_800E9FF0[i].max_life_points=8000;}
}
static void switch_base(void) { D_8009B1D5^=1; }
static int ai_base(void) { attack_calls++;return 2; }
static void *original_init_scene=(void *)init_base,*original_init_sides=(void *)sides_base;
static void *original_turn_switch=(void *)switch_base,*original_ai_run=(void *)ai_base;
static void ViewState_ApplyOrbit(void) { frames++; }
static void reroll_terrain(void) { rolls++; }
static int DuelOptions_Freeze(unsigned mode) { freeze_mode=mode;return 1; }
static void DuelOptions_ClearMatch(void) {}
static int DuelOptions_MatchValue(const char *a,const char *b,int *out) {*out=1;return 1;}
'''
tests=r'''
int main(void)
{
    static const int changes[]={4,9,12,17,20,25};DuelMatchView view;
    host.applied=on;host.find=find;host.log=log_line;
    for(int side=0;side<2;side++)for(int member=0;member<2;member++) {
        DuelMatch_Configure(20000,3,side,side ? 0 : member,side ? member : 0,4);
        assert(freeze_mode==2 && match_rules.pending && !match_rules.active);
        assert(match_rules.start_side==side);
        assert(side ? match_rules.ally_starter==(member^1) : match_rules.enemy_starter==member);
        init_sides();assert(D_800E9FF0[0].max_life_points==20000 && D_800E9FF0[1].life_points.signed_value==20000);
        init_scene();assert(D_8009B1D5==side && match_rules.active && !match_rules.pending);
        assert(gDuel_bTerrain==3 && (D_8009B16C&0x1000));
        assert(query(&view,sizeof(view)) && view.seats==4 && view.start_side==side);
        assert(!query(&view,sizeof(view)-1));
    }
    DuelMatch_Configure(12000,8,1,0,1,4);init_scene();rolls=0;
    for(int turn=1;turn<=25;turn++) {
        int before=rolls;turn_switch();int expected=0;
        for(unsigned i=0;i<sizeof(changes)/sizeof(*changes);i++)expected|=turn==changes[i];
        assert(rolls-before==expected && match_rules.completed_turns==turn);
    }
    DuelMatch_Configure(4000,0,1,0,0,2);init_scene();assert(freeze_mode==1 && D_8009B1D5==1);
    attack_calls=0;gAiScript_State.script_base=D_801A9800;
    assert(ai_run()==3 && !attack_calls);
    gAiScript_State.script_base=D_801A8000;assert(ai_run()==2 && attack_calls==1);
    turn_switch();gAiScript_State.script_base=D_801A9800;assert(ai_run()==2);
    D_800E9FF0[0].life_points.signed_value=2000;D_800E9FF0[0].max_life_points=4000;
    D_800E9FF0[0].deck_draw_cursor=14;DuelSideState projected;
    assert(rank_side(&D_800E9FF0[0],&projected));assert(projected.life_points.signed_value==4000 && projected.deck_draw_cursor==14);
    enabled=0;assert(!query(&view,sizeof(view)) && !rank_side(&D_800E9FF0[0],&projected));enabled=1;
    match_rules.pending=0;init_scene();assert(!match_rules.active);
    puts("PASS: separated solo/tag lifecycle, all opening seats, LP, repeated field cycles, AI guard and solo rank");return 0;
}
'''
parts=[source[start:end]+'\nstatic TagMatchRules match_rules;']
parts += [function(decl) for decl in ['void DuelMatch_Configure(', 'static void init_sides(', 'static void init_scene(', 'static void turn_switch(', 'static s32 ai_run(', 'static int query(', 'static int rank_side(']]
c=OUT/'test.c';c.write_text(prefix+'\n'.join(parts)+tests)
exe=OUT/'test.exe'
subprocess.run([shutil.which('i686-w64-mingw32-clang'),'-std=gnu11','-O2','-w','-I'+str(ROOT/'src'),'-I'+str(ROOT/'examples/mods'),str(c),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
