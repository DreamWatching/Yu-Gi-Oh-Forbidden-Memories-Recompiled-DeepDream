"""Exercise actual tag projection, live scorer and final scorer with fixture tables."""
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / 'tmp/pc/tag-rank-test'
out.mkdir(parents=True, exist_ok=True)
runtime = (root / 'examples/mods/tag-duel-menu/tag_duel_runtime.c').read_text()
rank = (root / 'src/pc/cards/rank.c').read_text()
result = (root / 'src/game/duel_result_runtime.c').read_text()

def function(source, declaration):
    start = source.index(declaration)
    brace = source.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

prefix = r'''
#define MEMORIES_PC 1
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pc/cards/rank.h"
#include "game/duel_rank.h"
#include "tag_rank.h"
#define RANK_RULE_COUNT 10
#define MAIN_MODE_DUEL 3
typedef struct {int page_text_ids[3],side_scores[2];} DuelResultDisplayState;
static DuelResultDisplayState result_state,*D_8009B1E8=&result_state;
static struct {int rank_rows[16][2];} D_801D5608[1];
static unsigned char D_8009B26C=3;
unsigned char gDuel_bWinnerSide;
DuelSideState D_800E9FF0[2];
DuelRankScoreChangeEntry gDuel_awRankScoreChange[10][5];
static int enabled=1,provided=1,options_provided=0;
typedef struct Host Host;
struct Host {int (*applied)(const Host *);};
static int applied(const Host *unused) {return enabled;}
static Host host={applied};static const Host *tag_host=&host;
static struct {int active;} match_rules={1};
static struct {int active,active_member[2];struct {int draw_cursor;} member[4];} state={1},*tag=&state;
static int member_index(int side,int member) {return side+2*member;}
'''
prefix += "\nstatic void refresh_match(void) {}\n"
bridge = r'''
static int options_project(const DuelSideState *side,DuelSideState *out) {
 *out=*side;out->life_points.signed_value=1234;return 1;
}
static void *Mods_Find(const char *key) {
 if(!strcmp(key,"duel-options:rank_side_v1"))return options_provided?(void*)options_project:NULL;
 assert(!strcmp(key,"tag-duel-menu:rank_side_v1"));return provided?(void*)rank_side:NULL;
}
'''
tests = r'''
int main(void) {
 DuelSideState before[2],view,raw,alien;
 int i,j,side,first,second,score,active;
 /* Fixture tables deliberately vary by rule; do not alter retail weights. */
 for(i=0;i<10;i++) for(j=0;j<5;j++) {
  static const int limits[]={5,10,20,40,32767};
  gDuel_awRankScoreChange[i][j].threshold=limits[j];
  gDuel_awRankScoreChange[i][j].score_change=(j-2)*(i+1);
 }
 for(side=0;side<2;side++) {
  D_800E9FF0[side].max_life_points=20000;
  D_800E9FF0[side].life_points.unsigned_value=10000;
  D_800E9FF0[side].rank.turns_taken=11;
  D_800E9FF0[side].rank.effective_attacks=9;
  D_800E9FF0[side].rank.fusions_initiated=3;
  D_800E9FF0[side].rank.pure_magic_used=4;
  D_800E9FF0[side].rank.result_adjustment=side?-40:2;
 }
 for(side=0;side<2;side++) for(first=5;first<=40;first++) for(second=5;second<=40;second++) {
  int previous=-99999;
  state.member[side].draw_cursor=first;state.member[side+2].draw_cursor=second;
  for(active=0;active<2;active++) {
   state.active_member[side]=active;
   D_800E9FF0[side].deck_draw_cursor=active?second:first;
   memcpy(before,D_800E9FF0,sizeof(before));
   Rank_ProjectSide(&D_800E9FF0[side],&view);
   assert(view.deck_draw_cursor==(first+second+1)/2);
   assert(view.life_points.signed_value==4000 && view.rank.turns_taken==6);
   assert(view.rank.effective_attacks==5 && view.rank.fusions_initiated==2);
   assert(view.rank.pure_magic_used==2);
   score=Rank_Score(&D_800E9FF0[side],D_800E9FF0[side].rank.result_adjustment);
   if(active) assert(score==previous);previous=score;
   Duel_CalcRankScore();
   assert(result_state.side_scores[side]==score);
   assert(D_801D5608[0].rank_rows[0][side]==D_800E9FF0[side].deck_draw_cursor);
   assert(D_801D5608[0].rank_rows[1][side]==10000);
   assert(D_801D5608[0].rank_rows[15][side]==11);
   assert(!memcmp(before,D_800E9FF0,sizeof(before)));
  }
 }
 puts("PASS: both teams and 2,592 deck pairs/rotations keep live and final rank equal without changing gameplay or raw results");
 /* Unsaved current draws must override a stale stored active cursor. */
 state.active_member[0]=0;state.member[0].draw_cursor=5;state.member[2].draw_cursor=20;
 D_800E9FF0[0].deck_draw_cursor=10;Rank_ProjectSide(&D_800E9FF0[0],&view);assert(view.deck_draw_cursor==15);
 state.member[2].draw_cursor=0;D_800E9FF0[0].deck_draw_cursor=5;
 Rank_ProjectSide(&D_800E9FF0[0],&view);assert(view.deck_draw_cursor==5);
 puts("PASS: active draws are immediate; undealt partner cannot inflate opening rank");
 for(i=4000;i<=20000;i+=4000) {
  D_800E9FF0[0].max_life_points=i;D_800E9FF0[0].life_points.unsigned_value=i/2;
  Rank_ProjectSide(&D_800E9FF0[0],&view);assert(view.life_points.signed_value==4000);
 }
 state.active=0;raw=D_800E9FF0[0];Rank_ProjectSide(&D_800E9FF0[0],&view);
 assert(view.rank.turns_taken==raw.rank.turns_taken && view.deck_draw_cursor==raw.deck_draw_cursor);
 assert(view.life_points.signed_value==4000);
 puts("PASS: all five LP presets use equivalent LP; solo retains original action/deck counters");
 for(i=0;i<2;i++) {
  D_800E9FF0[0].rank.result_adjustment=i?DUEL_RANK_ADJUST_DECK_OUT_WIN:DUEL_RANK_ADJUST_EXODIA_WIN;
  Duel_CalcRankScore();
  assert(result_state.side_scores[0]==Rank_Score(&D_800E9FF0[0],D_800E9FF0[0].rank.result_adjustment));
  assert(result_state.page_text_ids[1]==(i?DUEL_RESULT_TEXT_SELECTOR_DECK_OUT:DUEL_RESULT_TEXT_SELECTOR_EXODIA));
 }
 puts("PASS: Exodia and deck-out adjustments and result messages remain intact");
 raw=D_800E9FF0[0];enabled=0;Rank_ProjectSide(&D_800E9FF0[0],&view);assert(!memcmp(&raw,&view,sizeof(raw)));
 enabled=1;provided=0;Rank_ProjectSide(&D_800E9FF0[0],&view);assert(!memcmp(&raw,&view,sizeof(raw)));
 provided=1;D_8009B26C=6;Rank_ProjectSide(&D_800E9FF0[0],&view);assert(!memcmp(&raw,&view,sizeof(raw)));
 D_8009B26C=3;alien=raw;Rank_ProjectSide(&alien,&view);assert(!memcmp(&alien,&view,sizeof(raw)));
 puts("PASS: disabled/absent mod, outside duel and unrelated records preserve retail values");
 provided=0;options_provided=1;Rank_ProjectSide(&D_800E9FF0[0],&view);
 assert(view.life_points.signed_value==1234 && view.deck_draw_cursor==D_800E9FF0[0].deck_draw_cursor);
 provided=1;Rank_ProjectSide(&D_800E9FF0[0],&view);assert(view.life_points.signed_value!=1234);
 puts("PASS: standalone options rank provider works; active Tag projection takes priority");
 return 0;
}
'''
parts = [function(runtime,'static int rank_side('), bridge,
         function(rank,'void Rank_ProjectSide('),
         function(result,'s32 Duel_CalcRankScoreChange('),
         function(rank,'static int change('),function(rank,'int Rank_Score('),
         function(result,'void Duel_CalcRankScore(')]
c = out / 'test.c'
if 'Tables_Rank(' in result:
    prefix += '''
#include "pc/cards/tables.h"
static int custom_rank;
static const short custom_rank_row[10]={1,7,2,7,3,7,4,7,32767,7};
const short *Tables_Rank(int rule) { (void)rule;return custom_rank?custom_rank_row:NULL; }
'''
    tests = tests.replace(' return 0;', '''
 options_provided=provided=0;custom_rank=1;
 assert(Rank_Score(&D_800E9FF0[0],0)==DUEL_RANK_SCORE_INITIAL+70);
 puts("PASS: preview custom ranking rows retain live/final score agreement");
 return 0;''')
c.write_text(prefix + '\n'.join(parts) + tests)
exe = out / 'test.exe'
subprocess.run([shutil.which('i686-w64-mingw32-clang'),'--target=i686-w64-mingw32',
                '-std=gnu11','-O2','-w','-I'+str(root/'src'),
                '-I'+str(root/'examples/mods/tag-duel-menu'),str(c),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
