"""Actual field-request blocking and portrait-consistent HUD names."""
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'tmp/pc/duel-visual-lifecycle-test'
OUT.mkdir(parents=True,exist_ok=True)
def function(text,decl):
 start=text.index(decl);end=text.index('{',start)+1;depth=1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
match=(ROOT/'examples/mods/duel-options/match_runtime.c').read_text()
tag=(ROOT/'examples/mods/tag-duel-menu/tag_duel_runtime.c').read_text()
pool=(ROOT/'src/game/duel_effect_object_pool.c').read_text()
fixture=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc/mods/modapi.h"
#include "game/duel_effect_request.h"
#include "shared/duel_participants.h"
#include "tag-duel-menu/duel_rules.h"
DuelEffectRequest D_800EAD88[DUEL_EFFECT_REQUEST_COUNT];
u8 gDuel_bEffectRequestStatus;
static int full,allocations,sounds;
static u8 gDuel_bTerrain=3;
static struct {int terrain_effect_stage,terrain_queued,terrain_next,terrain_effect_slot,completed_turns;} match_rules;
static void logger(const MemoriesModHost *h,const char *s,...) {(void)h;(void)s;}
static MemoriesModHost test_host;
static const MemoriesModHost *tag_host=&test_host;
u8 *DuelEffect_AllocateRequest(s32 id) {
 allocations++;if(full)return NULL;
 memset(D_800EAD88,0,sizeof(D_800EAD88));
 D_800EAD88[0].id=id;D_800EAD88[0].flags=DUEL_EFFECT_REQUEST_FLAG_ACTIVE;
 return (u8*)D_800EAD88;
}
static void SD_SEPlayFull(int id) {assert(id==0x13);sounds++;}
static DuelParticipants displayed;
static int available=1;
static int participants(DuelParticipants *out,size_t size) {
 assert(size==sizeof(*out));if(!available)return 0;*out=displayed;return 1;
}
'''
fixture+=function(pool,'DuelEffectRequest *DuelEffect_CreateRequest(')
fixture+=function(match,'static void reroll_terrain(')
fixture+=function(tag,'static int hud_name(')
fixture+=r'''
int main(void) {
 test_host.log=logger;
 for(int terrain=1;terrain<=6;terrain++) {
  gDuel_bTerrain=terrain;memset(&match_rules,0,sizeof(match_rules));
  gDuel_bEffectRequestStatus=0;reroll_terrain();
  assert(gDuel_bEffectRequestStatus&DUEL_EFFECT_REQUEST_STATUS_ACTIVE);
  assert(match_rules.terrain_effect_stage==1 && match_rules.terrain_effect_slot==0);
  assert(match_rules.terrain_next!=terrain && match_rules.terrain_next>=1 && match_rules.terrain_next<=6);
  assert(D_800EAD88[0].field_1A==match_rules.terrain_next-1);
  int before=allocations;reroll_terrain();assert(allocations==before && match_rules.terrain_queued);
 }
 memset(&match_rules,0,sizeof(match_rules));full=1;gDuel_bEffectRequestStatus=0;
 reroll_terrain();assert(match_rules.terrain_queued && !match_rules.terrain_effect_stage && !gDuel_bEffectRequestStatus);
 char text[64],tiny[3];strcpy(displayed.side[0].name,"Yugi");strcpy(displayed.side[1].name,"Teana");
 assert(hud_name(1,text,sizeof(text)) && !strcmp(text,"Teana"));
 strcpy(displayed.side[1].name,"Jono");assert(hud_name(1,text,sizeof(text)) && !strcmp(text,"Jono"));
 strcpy(displayed.side[0].name,"Simon Muran");assert(hud_name(0,text,sizeof(text)) && !strcmp(text,"Simon Muran"));
 assert(hud_name(0,tiny,sizeof(tiny)) && !strcmp(tiny,"Si"));
 assert(!hud_name(-1,text,sizeof(text)) && !hud_name(2,text,sizeof(text)) && !hud_name(0,NULL,64) && !hud_name(0,text,0));
 available=0;assert(!hud_name(0,text,sizeof(text)));
 puts("PASS: field effects pause normal scene dispatch, rerolls do not repeat, full pool queues; HUD names follow displayed teammates");return 0;
}
'''
c=OUT/'test.c';c.write_text(fixture);exe=OUT/'test.exe'
subprocess.run([shutil.which('i686-w64-mingw32-clang'),'-std=gnu11','-O2','-Wno-gnu-folding-constant','-I'+str(ROOT/'src'),'-I'+str(ROOT/'examples/mods'),str(c),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
