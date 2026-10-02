"""Exercise actual Duel Options confirmation handlers with failure-injected services."""
from pathlib import Path
import subprocess,shutil,re
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'tmp/pc/deck-menu-flow-test';OUT.mkdir(parents=True,exist_ok=True)
s=(ROOT/'examples/mods/duel-options/tag_duel_menu.c').read_text()
def function(name):
 match=re.search(r'static void '+name+r'\([^;{}]*\)\s*\{',s);start=match.start();brace=match.end()-1;end=brace+1;depth=1
 while depth:
  depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "game/input.h"
enum {PAGE_RULES,PAGE_RECIPES,PAGE_CONFIRM};
enum {SE_REFUSED=9,SE_CONFIRM=1,SE_MOVE=2,SE_BACK=3,RULE_RECIPE=2};
static int page,recipe_tab,recipe_cursor,recipe_save,recipe_for_player,editing_player_recipe;
static int ask_kind,revision,partner_edited,partner_deck_slot,partner_recipe_tab,rule_row;
static unsigned short edit_draft[40],selected_recipe[40],partner_draft[40];
static char draft_name[64]="Partner Deck";
static int available=1,copy_ok=1,missing=7,apply_result=7,save_ok=1;
static int checks,applies,saves,openings,edits,last_sound;
static int DeckMenu_SavePartnerRecipe(int a,const char *b,const unsigned short*c){saves++;return save_ok;}
static int DeckMenu_SavePlayerRecipeCards(int a,const unsigned short*c){saves++;return save_ok;}
static int DeckMenu_PartnerRecipeInfo(int a,char*b,int c){if(b&&c)snprintf(b,c,"Partner Deck");return available;}
static int slot_info(int a,char*b,int c){return DeckMenu_PartnerRecipeInfo(a,b,c);}
static int DeckMenu_CopyPartnerRecipe(int a,unsigned short*b){return copy_ok;}
static int DeckMenu_CopySlot(int a,unsigned short*b){return copy_ok;}
static int DeckMenu_EquipOwnedRecipe(const unsigned short*c,int apply){if(apply){applies++;return apply_result;}checks++;return missing;}
static void begin_opening(void){openings++;}
static void begin_draft_edit(const unsigned short*c,const char*n,int p){edits++;}
static void SD_SEPlay(int s,int v,int p){last_sound=s;}
static void reset(void){page=PAGE_RECIPES;recipe_tab=1;recipe_cursor=1;recipe_save=0;recipe_for_player=1;editing_player_recipe=0;ask_kind=0;checks=applies=saves=openings=edits=0;available=copy_ok=save_ok=1;missing=apply_result=7;}
'''
tests=r'''
int main(void){
 reset();update_recipes(PAD_BUTTON_CROSS);assert(page==PAGE_CONFIRM&&ask_kind==3&&checks==1&&!applies&&!openings);
 update_confirm(PAD_BUTTON_CIRCLE);assert(page==PAGE_RECIPES&&!applies&&!openings);
 puts("PASS: missing-card warning and cancellation never equip or start duel");
 update_recipes(PAD_BUTTON_CROSS);update_confirm(PAD_BUTTON_CROSS);assert(applies==1&&openings==1);
 reset();apply_result=-1;update_recipes(PAD_BUTTON_CROSS);update_confirm(PAD_BUTTON_CROSS);assert(page==PAGE_CONFIRM&&!openings&&last_sound==SE_REFUSED);
 puts("PASS: shortage approval applies once; rejected application cannot start");
 reset();missing=0;apply_result=0;update_recipes(PAD_BUTTON_CROSS);assert(checks==1&&applies==1&&openings==1);
 reset();available=0;update_recipes(PAD_BUTTON_CROSS);assert(!checks&&!applies&&!openings);
 reset();copy_ok=0;update_recipes(PAD_BUTTON_CROSS);assert(!checks&&!applies&&!openings);
 puts("PASS: unavailable recipe or failed copy is refused before mutation");
 reset();recipe_save=1;recipe_for_player=0;update_recipes(PAD_BUTTON_CROSS);assert(page==PAGE_CONFIRM&&ask_kind==4&&!saves);
 update_confirm(PAD_BUTTON_CIRCLE);assert(page==PAGE_RECIPES&&!saves);
 update_recipes(PAD_BUTTON_CROSS);save_ok=0;update_confirm(PAD_BUTTON_CROSS);assert(page==PAGE_CONFIRM&&recipe_save&&saves==1&&last_sound==SE_REFUSED);
 save_ok=1;update_confirm(PAD_BUTTON_CROSS);assert(page==PAGE_RULES&&!recipe_save&&saves==2);
 puts("PASS: overwrite requires confirmation; failure keeps draft and permits retry");
 reset();recipe_save=1;available=0;save_ok=0;update_recipes(PAD_BUTTON_CROSS);assert(page==PAGE_RECIPES&&recipe_save&&saves==1);
 update_recipes(PAD_BUTTON_CIRCLE);assert(page==PAGE_CONFIRM&&ask_kind==1);
 update_confirm(PAD_BUTTON_CIRCLE);assert(ask_kind==2);update_confirm(PAD_BUTTON_CIRCLE);assert(ask_kind==1);
 update_confirm(PAD_BUTTON_CIRCLE);update_confirm(PAD_BUTTON_CROSS);assert(page==PAGE_RULES&&!recipe_save&&saves==1);
 puts("PASS: failed new save retains draft; back/discard confirmation does not write");
 reset();recipe_for_player=0;update_recipes(PAD_BUTTON_TRIANGLE);assert(edits==1&&!saves&&!applies);
 reset();recipe_for_player=0;update_recipes(PAD_BUTTON_R1);assert(!recipe_tab&&recipe_cursor==0);
 puts("PASS: recipe edit and tab navigation do not equip or save");return 0;
}
'''
c=OUT/'test.c';c.write_text(prefix+'\n'.join(function(n) for n in ['finish_draft_save','update_confirm','update_recipes'])+tests)
exe=OUT/'test.exe';subprocess.run([shutil.which('i686-w64-mingw32-clang'),'-std=gnu11','-O2','-I'+str(ROOT/'src'),str(c),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
