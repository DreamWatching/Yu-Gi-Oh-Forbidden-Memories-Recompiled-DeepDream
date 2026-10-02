"""Actual optional reward policy and mandatory Tag inventory repair wrappers."""
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'tmp/pc/tag-rewards-test';OUT.mkdir(parents=True,exist_ok=True)
def function(source,decl):
    start=source.index(decl);brace=source.index('{',start);end=brace+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
policy=(ROOT/'examples/mods/tag-duel-menu/tag_rewards.c').read_text()
core=(ROOT/'examples/mods/tag-duel-menu/tag_duel_runtime.c').read_text()
prefix=r'''
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc/mods/modapi.h"
#include "pc/cards/cards.h"
#include "pc/cards/tables.h"
#include "pc/cards/drops.h"
#include "pc/platform/settings.h"
#include "game/duel_rewards.h"
#include "shared/tag_reward_context.h"
#define TAG_RARE_MAX_WEIGHT 16
static const MemoriesModHost *tag_host;
static TagRewardQuery context_query;
static struct { TagRewardContext context;unsigned source_count[2],spoils_source; } state;
static void *original_select_drop,*original_result_rewards;
static int reward_rolling,reward_roll_index,reward_roll_count;
static int reward_setting=1;
static int enabled=1,tag_enabled=1,override_on,retail_calls,requested_drops=3,variant_on,context_abi=1;
static int reward_enabled(void) { return enabled && reward_setting; }
static unsigned short override_weights[CARD_COUNT+2];
int gCard_nCount=CARD_COUNT+1;
static int on(const MemoriesModHost *h) { (void)h;return enabled; }
static void logmsg(const MemoriesModHost *h,const char *s,...) { (void)h;(void)s; }
const unsigned short *Tables_PoolFor(int duelist,int pool,const unsigned short *retail)
{ (void)pool;(void)retail;return override_on && duelist==20?override_weights:NULL; }
int Cards_PickVariant(int id,int use) { (void)use;return variant_on?id+1:id; }
int Settings_Get(SettingId key) { assert(key==SET_CARD_DROPS);return requested_drops; }
static int retail(int pool) { (void)pool;retail_calls++;return 55; }
static unsigned gDuel_wSceneStateFlags;
static unsigned char gDuel_bWinnerSide;
static int query(TagRewardContext *out,size_t size) {
 if(!tag_enabled)return 0;
 if(!out)return size==0;
 assert(size==sizeof(*out));out->abi=context_abi;out->size=sizeof(*out);return 1;
}
static int native_awards[99],native_count;
static void native_result(void) {
 for(int i=0;i<requested_drops;i++)native_awards[native_count++]=select_drop(0);
}
'''
# Native result fixture needs a declaration before its definition.
prefix=prefix.replace('static int native_awards[99]', 'static s32 select_drop(s32);\nstatic int native_awards[99]')
tests=r'''
int main(void) {
 MemoriesModHost host={0};host.applied=on;host.log=logmsg;tag_host=&host;
 original_select_drop=(void*)retail;original_result_rewards=(void*)native_result;context_query=query;
 state.context.duelist[0]=10;state.context.duelist[1]=20;
 /* Both pools have common and genuinely rare cards, never each other's ID. */
 state.context.weights[0][0][0]=2032;state.context.weights[0][0][1]=16;
 state.context.weights[1][0][2]=2032;state.context.weights[1][0][3]=16;
 reward_rolling=1;reward_roll_count=2;
 for(int i=0;i<10000;i++) {
  reward_roll_index=0;int id=select_drop(0);assert(id>=1 && id<=4);
  id=select_drop(0);assert(id==2 || id==4);
 }
 assert(state.source_count[0]>8000 && state.source_count[1]>8000 && retail_calls==0);
 assert(state.spoils_source<2);
 /* No card <=16: guarantee the globally least-likely positive entry. */
 memset(state.context.weights,0,sizeof(state.context.weights));
 state.context.weights[0][0][0]=2000;state.context.weights[0][0][1]=48;
 state.context.weights[1][0][2]=2016;state.context.weights[1][0][3]=32;
 for(int i=0;i<100;i++){reward_roll_index=1;assert(select_drop(0)==4);}
 /* Edited tables include new card IDs; retail tables use zero-based weights. */
 override_on=1;override_weights[CARD_COUNT+1]=1;
 for(int i=0;i<100;i++){reward_roll_index=1;assert(select_drop(0)==CARD_COUNT+1);}
 override_on=0;variant_on=1;reward_roll_index=1;assert(select_drop(0)==5);variant_on=0;
 memset(state.context.weights,0,sizeof(state.context.weights));state.spoils_source=255;
 reward_roll_index=1;assert(select_drop(0)==55 && state.spoils_source==255);
 reward_setting=0;reward_roll_index=0;assert(select_drop(0)==55 && reward_roll_index==0);reward_setting=1;
 enabled=0;reward_roll_index=0;assert(select_drop(0)==55 && reward_roll_index==0);enabled=1;
 reward_rolling=0;assert(select_drop(0)==55);
 reward_rolling=1;assert(select_drop(-1)==55 && select_drop(3)==55);
 /* Wrapper owns a bounded result-only rolling window; no leaking into solo. */
 native_count=0;tag_enabled=0;gDuel_bWinnerSide=0;result_rewards();
 assert(native_count==3 && native_awards[0]==55 && !reward_rolling);
 native_count=0;tag_enabled=1;gDuel_bWinnerSide=1;result_rewards();
 assert(native_count==3 && native_awards[0]==55 && !reward_rolling);
 gDuel_bWinnerSide=0;state.context.weights[0][0][0]=2032;state.context.weights[0][0][1]=16;
 saved_context=state.context;requested_drops=1;native_count=0;result_rewards();
 assert(native_count==1 && native_awards[0]==2 && state.spoils_source==0 && !reward_rolling);
 requested_drops=99;native_count=0;result_rewards();
 assert(native_count==99 && native_awards[98]==2 && state.source_count[0]+state.source_count[1]==99);
 context_abi=99;requested_drops=1;native_count=0;result_rewards();
 assert(native_awards[0]==55 && state.source_count[0]+state.source_count[1]==0);
 puts("PASS: both pools, guaranteed rarity/fallback, custom cards, variants, disabled/solo/loss passthrough and scoped rolling");
 return 0;
}
'''
# Query returns actual owned weights like Tag, instead of leaving reset context empty.
prefix=prefix.replace('static int query(', 'static TagRewardContext saved_context;\nstatic int query(')
prefix=prefix.replace('assert(size==sizeof(*out));out->abi=context_abi;', 'assert(size==sizeof(*out));*out=saved_context;out->abi=context_abi;')
parts=[function(policy,decl) for decl in ['static const u16 *drop_weights(',
       'static int drop_card(', 'static int roll_opponent_pool(',
       'static int rare_weight_floor(', 'static int roll_rare_drop(',
       'static s32 select_drop(', 'static void result_rewards(']]
core_prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pc/mods/modapi.h"
#include "pc/cards/drops.h"
#include "game/duel_result_display.h"
#include "game/save_data.h"
#include "game/fade.h"
static const MemoriesModHost *tag_host;
static struct {int active;short reward_ids[99];unsigned char reward_before[99],reward_expected[99],reward_count,reward_checked;} state,*tag=&state;
static unsigned char chest[724];static int awards,limit=250,lose_awards;
static unsigned short gDuel_wSceneStateFlags;
static unsigned char gDuel_bWinnerSide;
static short D_8009B360[2];
unsigned short gDuel_awPlayerDeck[2048];
static SaveDataState *D_8009B1D8[2];
u8 D_800E9EC8_arr[FADE_TRANSITION_STATE_SIZE];
static DuelResultDisplayState result;DuelResultDisplayState *D_8009B1E8=&result;
CardDropsState gCardDrops;
static void *original_result_rewards;
static void logmsg(const MemoriesModHost *h,const char *s,...) {(void)h;(void)s;}
static int Cards_Valid(int id) {return id>0 && id<724;}
static unsigned char *Cards_ChestSlot(void *p,int id) {(void)p;return chest+id;}
static int Tables_ChestLimit(void) {return limit;}
static int Tables_ChestFull(unsigned quantity) {return quantity>=(unsigned)limit;}
static void Duel_AwardCard(int id) {if(!lose_awards)chest[id]++;awards++;}
static void retail_result(void) {
 assert(D_8009B360[0]==-1 && D_8009B1D8[0]==(SaveDataState*)gDuel_awPlayerDeck);
 result.dropped_card_id=2;gCardDrops.count=2;gCardDrops.cards[0]=2;gCardDrops.cards[1]=3;
}
'''
core_tests=r'''
int main(void) {
 MemoriesModHost host={0};host.log=logmsg;tag_host=&host;state.active=1;
 original_result_rewards=(void*)retail_result;D_8009B360[0]=5;
 chest[2]=7;result_rewards();assert(state.reward_count==2 && state.reward_expected[0]==2 && chest[2]==7);
 gDuel_wSceneStateFlags=0xe000;result_rewards();assert(chest[2]==9 && chest[3]==1 && awards==3);
 result_rewards();assert(awards==3);
 /* Already awarded copies are never added twice. */
 state.reward_checked=0;verify_rewards();assert(awards==3);
 state.reward_checked=0;limit=9;chest[2]=9;verify_rewards();assert(awards==3);
 state.reward_checked=0;chest[3]=0;lose_awards=1;verify_rewards();assert(awards==4 && chest[3]==0);
 puts("PASS: mandatory campaign award repair, duplicate drops, no double award, chest limits and no-progress guard");
 return 0;
}
'''
core_parts=[function(core,decl) for decl in ['static void remember_reward(',
            'static void snapshot_rewards(', 'static void verify_rewards(', 'static void result_rewards(']]
for name,contents in [('policy',prefix+'\n'.join(parts)+tests),
                      ('inventory',core_prefix+'\n'.join(core_parts)+core_tests)]:
    c=OUT/(name+'.c');c.write_text(contents);exe=OUT/(name+'.exe')
    subprocess.run([shutil.which('i686-w64-mingw32-clang'),'-std=gnu11','-O2',
                   '-Wno-gnu-folding-constant','-I'+str(ROOT/'src'),
                   '-I'+str(ROOT/'examples/mods'),str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
