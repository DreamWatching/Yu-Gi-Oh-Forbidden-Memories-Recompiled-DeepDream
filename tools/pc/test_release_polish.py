"""Production Face-off state/input, fade bounds, and portrait preparation gate."""
from pathlib import Path
import subprocess,shutil,re
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'tmp/pc/release-polish-test';OUT.mkdir(parents=True,exist_ok=True)
menu=(ROOT/'examples/mods/duel-options/tag_duel_menu.c').read_text()
runtime=(ROOT/'examples/mods/duel-options/match_runtime.c').read_text()
portraits=(ROOT/'examples/mods/duel-portraits/duel_portraits.c').read_text()
def function(s,name):
 m=re.search(r'(?:static )?(?:void|int|s32) '+name+r'\([^;{}]*\)\s*\{',s);assert m,name
 start=m.start();end=m.end();depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game/input.h"
#include "pc/mods/modapi.h"
#include "shared/duel_participants.h"
#include "tag-duel-menu/duel_rules.h"
#define tag_host host
#define DUEL_OPTION_TAG 2
#define DUEL_OPTION_SOLO 1
static struct {int seats,opponent_draw_pool,pending,active,starting_lp,field_choice,start_side,ally_starter,enemy_starter,completed_turns,terrain_effect_stage,terrain_effect_slot,terrain_queued;} match_rules;
static int draw_choice=1,frozen,original_calls;
static int DuelOptions_Freeze(int mode){frozen=mode;return 1;}
static int DuelOptions_MatchValue(const char*a,const char*b,int*out){*out=draw_choice;return 1;}
static int native_hand(void){original_calls++;return 13;}
static void *original_hand_size=(void*)native_hand;
static int D_8009B1D5,gDuel_bOpponentID;
enum {PAGE_GRID,PAGE_RULES,PAGE_CLASH};
enum {SE_MOVE,SE_PORTRAIT,SE_CONFIRM,SE_BACK};
#define MAIN_MODE_DUEL 3
#define C_INK 0x080812
static int mini(int a,int b){return a<b?a:b;}
static int maxi(int a,int b){return a>b?a:b;}
enum {RULE_CONTROL,RULE_DECK,RULE_RECIPE,RULE_DRAW_POOL,RULE_LIFE,RULE_FIELD,RULE_OPENING,RULE_CONTINUE,RULE_COUNT};
static int page=PAGE_CLASH,clash_phase,clash_cursor,clash_enemy_card,clash_player_card;
static int clash_enemy_cursor,clash_scan_step,clash_winner,clash_first_side;
static int clash_cpu_step,clash_ally_starter,clash_enemy_starter,clash_flip_sounded;
static int tag_mode=1,revision,started,shuffled,sounds;
static unsigned long long now,clash_phase_at;
static int clash_order[10]={0,1,2,3,4,5,6,7,8,9};
static const char *clash_star_names[10]={"0","1","2","3","4","5","6","7","8","9"};
static unsigned long long clocknow(const MemoriesModHost*h){return now;}
static void logmsg(const MemoriesModHost*h,const char*s,...){ }
static void SD_SEPlay(int a,int b,int c){sounds++;}
static int clash_pick_enemy(int p){return (p+1)%10;}
static int clash_beats(int a,int b){return (a+1)%10==b;}
static void clash_shuffle(void){clash_phase=0;shuffled++;}
static int begin_tag_battle(void){started++;page=PAGE_GRID;return 1;}
static int begin_solo_battle(void){started++;page=PAGE_GRID;return 1;}
static int fx,fy,fw,fh,fa;
static void fill(const MemoriesModHost*h,int x,int y,int w,int ht,unsigned c,unsigned a){fx=x;fy=y;fw=w;fh=ht;fa=a;}
static const MemoriesModHost *host;
static unsigned char D_8009B26C,D_8009B26E;
static DuelParticipants view;
static int duel_started=1;
static void *find(const MemoriesModHost*h,const char*s){return NULL;}
static int on(const MemoriesModHost*h){return 1;}
static int retail_calls;
static int retail_view(DuelParticipants*out){retail_calls++;memset(out,0,sizeof(*out));out->abi=1;out->size=sizeof(*out);out->active_side=0;return 1;}
'''
tests=r'''
int main(void){
 MemoriesModHost h={0};h.now_us=clocknow;h.log=logmsg;h.fill=fill;h.find=find;h.applied=on;host=&h;
 for(int tag=0;tag<=1;tag++)for(int rows=1;rows<=10;rows++)for(int offset=0;offset<8;offset++){
  int first,visible,total,base=tag?0:RULE_DRAW_POOL;
  rule_scroll_layout(tag,rows,offset,&first,&visible,&total);
  assert(total==RULE_COUNT-base && visible>=1 && visible<=total);
  assert(first>=base && first+visible<=RULE_COUNT);
  int track=170,thumb=mini(track,maxi(24,track*visible/total));
  int y=(track-thumb)*(first-base)/maxi(1,total-visible);
  assert(y>=0 && y+thumb<=track);
  if(rows>=total)assert(first==base && thumb==track && y==0);
 }
 puts("PASS: solo/tag scrollbar uses eligible rows, stays in bounds and fills the rail when scrolling is unnecessary");
 /* Circle is ignored in every live phase, for solo and tag alike. */
 for(tag_mode=0;tag_mode<=1;tag_mode++)for(int phase=0;phase<=11;phase++){
  clash_phase=phase;page=PAGE_CLASH;clash_phase_at=now;started=shuffled=sounds=0;
  update_clash(PAD_BUTTON_CIRCLE);assert(page==PAGE_CLASH && clash_phase==phase && !started && !shuffled && !sounds);
 }
 puts("PASS: Circle cannot cancel any Face-off phase in solo or tag");
 clash_phase=0;page=PAGE_CLASH;update_clash(PAD_BUTTON_CROSS);assert(clash_phase==1);
 now=650000;update_clash(0);assert(clash_phase==2);
 now+=2000000;update_clash(0);assert(clash_phase==3);
 now+=650000;update_clash(0);assert(clash_phase==10);
 now+=300000;update_clash(0);assert(clash_phase==4);
 now+=3800000;update_clash(0);assert(clash_phase==11);
 now+=300000;update_clash(0);assert(clash_phase==6);
 update_clash(PAD_BUTTON_CROSS);assert(clash_phase==7);
 update_clash(PAD_BUTTON_CROSS);assert(started==1 && page==PAGE_GRID);
 puts("PASS: committed Face-off still completes card choice, reveal and starter confirmation");
 for(int scale=1;scale<=3;scale++)for(int t=0;t<=300;t+=150){
  draw_clash_transition(10,20,600*scale,430*scale,scale,t);
  assert(fx==10+5*scale && fy==20+5*scale && fw==590*scale && fh==420*scale);
  assert(fa==t*255/300);
 }
 puts("PASS: fade covers complete panel interior at all tested scales and becomes fully opaque");
 D_8009B26C=MAIN_MODE_DUEL|0x40;
 for(int sub=0;sub<256;sub++){
  D_8009B26E=sub;retail_calls=0;assert(query()==(sub==0x81));assert(retail_calls==(sub==0x81));
 }
 D_8009B26C=0x43;D_8009B26E=0x81;duel_started=0;assert(!query());duel_started=1;assert(query());
 D_8009B26C=MAIN_MODE_DUEL;D_8009B26E=0x81;assert(!query());
 D_8009B26C=5|0x40;assert(!query());
 puts("PASS: portrait provider is not queried during preparation, loading, return, or other menus");
 for(int seats=2;seats<=4;seats+=2)for(draw_choice=0;draw_choice<=1;draw_choice++){
  DuelMatch_Configure(8000,0,0,0,0,seats);assert(frozen==(seats==4?2:1));
  match_rules.active=1;D_8009B26C=0x43;D_8009B1D5=1;gDuel_bOpponentID=9;
  assert(opponent_hand_size()==(draw_choice?20:5));
  /* Live setting changes cannot change a confirmed match. */
  draw_choice^=1;assert(opponent_hand_size()==(draw_choice?5:20));draw_choice^=1;
  D_8009B1D5=0;assert(opponent_hand_size()==13);
  D_8009B1D5=1;match_rules.active=0;assert(opponent_hand_size()==13);
 }
 puts("PASS: solo and tag share frozen 5/20 search policy; player/partner/campaign stay native");
 return 0;
}
'''
c=OUT/'test.c';c.write_text(prefix+function(menu,'rule_scroll_layout')+function(menu,'update_clash')+function(menu,'draw_clash_transition')+function(portraits,'query')+function(runtime,'DuelMatch_Configure')+function(runtime,'opponent_hand_size')+tests)
exe=OUT/'test.exe';subprocess.run([shutil.which('i686-w64-mingw32-clang'),'-std=gnu11','-O2','-I'+str(ROOT/'src'),'-I'+str(ROOT/'examples/mods'),str(c),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
