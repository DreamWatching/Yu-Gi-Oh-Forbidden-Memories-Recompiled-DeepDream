"""Exercise the actual deck sandbox/ownership functions with mocked UI services."""
from pathlib import Path
import subprocess
import shutil

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'tmp/pc/tag-deck-sandbox-test'
OUT.mkdir(parents=True, exist_ok=True)
source = (ROOT / 'src/pc/saves/deck_menu.c').read_text()

def function(name):
    start = source.index(name + '(')
    start = source.rfind('\n', 0, start) + 1
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

prefix = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game/save_data.h"
#include "duel_rules.h"
#define DECK_SLOT_TRUNK_MAX 250
#define MODE_FREE_DUEL 6
#define MODE_BUILD_DECK 7
#define MAIN_MENU_SELECTION_BUILD_DECK 2
#define PICK_NONE 0
static unsigned char ram[0x3000];
static unsigned char gCard_abExtraChest[2048];
static int gCard_nCount = 1000;
static struct {int active, ready; unsigned char saved_state[SAVE_DATA_STATE_SIZE];
 unsigned short result[40]; unsigned char *extra; int extra_count;} temporary;
static struct {int active;} draft;
static int repair_pending, requested, picking;
static unsigned char D_8009B269, D_8009B26C;
static SaveDataWorkspace *workspace(void) {return (SaveDataWorkspace *)ram;}
static int game_loaded(void) {return workspace()->state.player_deck[0] || repair_pending;}
static int Cards_Valid(int id) {return id > 0 && id <= gCard_nCount;}
static unsigned char *trunk(void *unused, int id) {
 return id <= CARD_COUNT ? workspace()->state.card_quantities + id - 1 : gCard_abExtraChest + id;
}
static void reconcile(void) {}
static void DeckMenu_Close(void) {}
static void Main_ApplyMenuSelection(int unused) {D_8009B26C = 7;}
static int chest_left(void) {assert(!"Sandbox reached player recipe reconciliation"); return 0;}
'''
names = ['DeckMenu_CancelTemporary', 'DeckMenu_BeginTemporary', 'DeckMenu_TakeTemporary',
         'DeckMenu_BuildDeckLeft', 'DeckMenu_EquipOwnedRecipe']
tests = r'''
static unsigned ownership(void) {
 unsigned sum = 0; int i;
 for(i=0;i<CARD_COUNT;i++) sum += workspace()->state.card_quantities[i];
 for(i=0;i<40;i++) sum += workspace()->state.player_deck[i] != 0;
 for(i=CARD_COUNT+1;i<=gCard_nCount;i++) sum += gCard_abExtraChest[i];
 return sum;
}
int main(void) {
 unsigned short wanted[40], result[40]; unsigned char before[SAVE_DATA_STATE_SIZE], extra[2048];
 int i, n=0, nonzero=0; unsigned total;
 /* Every active terrain maps onto precisely the five alternatives. */
 for(i=1;i<=6;i++) {
  unsigned seen=0, roll;
  for(roll=0;roll<500;roll++) {
   int next=TagRules_NextTerrain(i,roll);
   assert(next>=1 && next<=6 && next!=i);
   seen |= 1u << next;
  }
  assert(seen == (126u & ~(1u << i)));
 }
 for(i=0;i<600;i++) assert(TagRules_NextTerrain(0,i)==1+i%6);
 puts("PASS: terrain rerolls cover every alternative and never repeat the active terrain");
 {
  int opener, turn, at;
  const int expected[] = {4,9,12,17,20,25,28,33,36,41,44,49,52,57,60};
  for(opener=0;opener<4;opener++) {
   at=0;
   for(turn=1;turn<=60;turn++) if(TagRules_FieldChangeDue(turn,4)) {
    assert(at<15 && turn==expected[at++]);
    assert((opener+turn-1)%4 == (opener+(turn%8==1 ? 0 : 3))%4);
   }
   assert(at==15);
  }
  assert(!TagRules_FieldChangeDue(1,2));
  assert(TagRules_FieldChangeDue(2,2) && TagRules_FieldChangeDue(5,2));
  assert(!TagRules_FieldChangeDue(3,2) && !TagRules_FieldChangeDue(4,2));
  puts("PASS: persistent field cadence through 60 turns, all four opening seats, and solo cadence");
 }
 memset(ram,0,sizeof(ram));
 for(i=0;i<40;i++) workspace()->state.player_deck[i]=i+1;
 workspace()->state.card_quantities[0]=1;
 workspace()->state.card_quantities[1]=2;
 workspace()->state.card_quantities[40]=1;
 for(i=0;i<3;i++) wanted[n++]=1;
 for(i=0;i<3;i++) wanted[n++]=2;
 for(i=0;i<3;i++) wanted[n++]=41;
 for(i=42;i<=72;i++) wanted[n++]=i;
 memcpy(before,&workspace()->state,SAVE_DATA_STATE_SIZE);
 total=ownership();
 assert(DeckMenu_EquipOwnedRecipe(wanted,0)==34);
 assert(!memcmp(before,&workspace()->state,SAVE_DATA_STATE_SIZE));
 assert(DeckMenu_EquipOwnedRecipe(wanted,1)==34);
 assert(repair_pending && draft.active == -1);
 for(i=0;i<40;i++) nonzero += workspace()->state.player_deck[i] != 0;
 assert(nonzero==6 && ownership()==total);
 for(i=2;i<40;i++) assert(workspace()->state.card_quantities[i]==1);
 puts("PASS: unavailable copies omitted; owned inventory conserved; no slot overwrite");
 memcpy(&workspace()->state,before,SAVE_DATA_STATE_SIZE); repair_pending=0;
 workspace()->state.card_quantities[2]=250;
 memcpy(before,&workspace()->state,SAVE_DATA_STATE_SIZE);
 assert(DeckMenu_EquipOwnedRecipe(wanted,1)==-1);
 assert(!memcmp(before,&workspace()->state,SAVE_DATA_STATE_SIZE));
 puts("PASS: chest overflow rejects atomically");
 for(i=0;i<40;i++) wanted[i]=100+i;
 wanted[39]=723; gCard_abExtraChest[723]=4;
 memcpy(extra,gCard_abExtraChest,sizeof(extra));
 /* A held borrowed card must not also exist as an extra chest copy. */
 workspace()->state.player_deck[0] = 100;
 workspace()->state.card_quantities[99] = 0;
 memcpy(before,&workspace()->state,SAVE_DATA_STATE_SIZE);
 assert(DeckMenu_BeginTemporary(wanted));
 assert(*trunk(NULL,100)==0);
 DeckMenu_CancelTemporary();
 assert(!memcmp(before,&workspace()->state,SAVE_DATA_STATE_SIZE));
 /* Remove every borrowed card, exit, and reopen repeatedly. No growth. */
 for(n=0;n<5;n++) {
  assert(DeckMenu_BeginTemporary(wanted));
  assert(*trunk(NULL,100)==0);
  assert(*trunk(NULL,723)==3); /* four owned copies, one held */
  for(i=0;i<40;i++) {
   (*trunk(NULL,workspace()->state.player_deck[i]))++;
   workspace()->state.player_deck[i]=0;
  }
  DeckMenu_BuildDeckLeft();
  assert(DeckMenu_TakeTemporary(result));
  assert(!memcmp(before,&workspace()->state,SAVE_DATA_STATE_SIZE));
  assert(!memcmp(extra,gCard_abExtraChest,sizeof(extra)));
 }
 puts("PASS: five remove-all/reopen cycles preserve stock without duplicated held cards");
 assert(DeckMenu_BeginTemporary(wanted));
 assert(!memcmp(workspace()->state.player_deck,wanted,80));
 workspace()->state.card_quantities[400]=99;
 gCard_abExtraChest[723]=99;
 workspace()->state.player_deck[0]=400;
 DeckMenu_BuildDeckLeft();
 assert(!memcmp(before,&workspace()->state,SAVE_DATA_STATE_SIZE));
 assert(!memcmp(extra,gCard_abExtraChest,sizeof(extra)));
 assert(DeckMenu_TakeTemporary(result) && result[0]==400);
 assert(!DeckMenu_TakeTemporary(result));
 puts("PASS: native editor output captured; all real inventory restored, including mod cards");
 assert(DeckMenu_BeginTemporary(wanted));
 workspace()->state.card_quantities[100]=200;
 DeckMenu_CancelTemporary();
 assert(!memcmp(before,&workspace()->state,SAVE_DATA_STATE_SIZE));
 assert(!memcmp(extra,gCard_abExtraChest,sizeof(extra)));
 puts("PASS: cancelling sandbox restores inventory");
 return 0;
}
'''
c = OUT / 'test.c'
c.write_text(prefix + '\n'.join(function(n) for n in names) + tests)
compiler = shutil.which('i686-w64-mingw32-clang')
exe = OUT / 'test.exe'
subprocess.run([str(compiler), '--target=i686-w64-mingw32', '-std=gnu11', '-O2', '-w',
                '-I'+str(ROOT/'src'), '-I'+str(ROOT/'examples/mods/tag-duel-menu'),
                str(c), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
