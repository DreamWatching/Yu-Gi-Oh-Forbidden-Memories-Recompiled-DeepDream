"""Run the actual mod AI wrapper against instrumented native/AI planners."""
from pathlib import Path
import shutil, subprocess
root=Path(__file__).resolve().parents[2]
s=(root/'examples/mods/tag-duel-menu/tag_duel_runtime.c').read_text()
a=s.index('static s32 ai_run(void)'); b=s.index('{',a); d=1; e=b+1
while d:
 d+=(s[e]=='{')-(s[e]=='}'); e+=1
out=root/'tmp/pc/tag-opening-ai-test';out.mkdir(parents=True,exist_ok=True)
prefix=r'''#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "duel_rules.h"
typedef int s32; typedef signed char s8;
typedef struct {int unused;} HmBoard;
typedef struct {int result; unsigned char selection[12];} HmDecision;
typedef struct {int a,b,c,d,e,f,g,h,i,j,k,l;} HmOptions;
static int tag_rules;
static struct {int active,completed_turns;} match_rules;
static void refresh_match(void) {}
static struct {int active,active_member[2],hard_mode,partner_control;struct{int duelist;}member[4];} storage,*tag=&storage;
static int D_8009B1D5;
static char D_801A8000[4],D_801A9800[4];
static struct {char *script_base;} gAiScript_State;
static struct {signed char values[1];} gDuel_aOpponentData[40];
static s8 gDuel_bOpponentID;
static unsigned char D_800EAE88[12];
#define AI_OPPONENT_COUNT 40
#define TAG_CONTROL_AI 0
static int calls,plans;
static s32 original(void) {calls++;return 2;}
static void *original_ai_run=(void*)original;
static int member_index(int side,int member){return side*2+member;}
static HmBoard partner_board(void){HmBoard b={0};return b;}
static HmDecision Hm_PlanHand(HmBoard*b,HmOptions*o,int*r){HmDecision d={0};plans++;return d;}
static HmDecision Hm_PlanField(HmBoard*b,HmOptions*o,int*r,int first){HmDecision d={0};plans++;return d;}
'''
tests=r'''
int main(void){int side,member;
 for(side=0;side<2;side++)for(member=0;member<2;member++){
  int seat=member*2+side;
  int waiting=TagRules_WaitingStarter(side,member);
  assert(waiting*2+(side^1)==(seat+1)%4);
  assert(TagRules_OpeningAttacksBlocked(1,0));
  assert(!TagRules_OpeningAttacksBlocked(1,1));
 }
 puts("PASS: all four starting seats preserve the canonical four-duelist ring");
 match_rules.active=1;tag->active=1;tag->partner_control=TAG_CONTROL_AI;
 tag->member[2].duelist=9;tag->member[3].duelist=10;
 for(side=0;side<2;side++)for(member=0;member<2;member++){
  D_8009B1D5=side;tag->active_member[side]=member;
  match_rules.completed_turns=0;gAiScript_State.script_base=D_801A9800;
  calls=plans=0;assert(ai_run()==3 && calls==0 && plans==0);
  gAiScript_State.script_base=D_801A8000;
  assert(ai_run()==2 && calls==1); /* can draw and place */
  match_rules.completed_turns=1;gAiScript_State.script_base=D_801A9800;
  calls=plans=0;assert(ai_run()==2 && calls==1); /* attacks permitted later */
 }
 tag->active=0;match_rules.completed_turns=0;calls=0;
 assert(ai_run()==3 && calls==0); /* solo CPU opening */
 match_rules.active=0;assert(ai_run()==2 && calls==1);
 puts("PASS: all four opening seats and solo end field phase without calling attack planners; hand phase and later turns remain enabled");
 return 0;
}
'''
c=out/'test.c';c.write_text(prefix+s[a:e]+tests);exe=out/'test.exe'
subprocess.run([shutil.which('i686-w64-mingw32-clang'),'-O2','-w','-I'+str(root/'examples/mods/tag-duel-menu'),str(c),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
