"""Exercise the actual NEW badge hooks without changing a user's inventory."""
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'tmp/pc/new-card-markers-test'
OUT.mkdir(parents=True, exist_ok=True)
fixture = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "new-card-markers/new_card_markers.c"
int gCard_nCount = CARD_TABLE_ID_END - 1;
u16 gDuel_awPlayerDeck[DECK_SIZE];
BuildDeckTransitionState *gBuildDeck_pState;
static u8 chest[CARD_TABLE_ID_END];
static int sorted, calls;
int Cards_Valid(int id) { return id > 0 && id <= gCard_nCount; }
unsigned char *Cards_ChestSlot(void *deck, int id) {
    assert(deck == gDuel_awPlayerDeck && Cards_Valid(id)); return &chest[id];
}
void Duel_AwardCard(s32 id) {
    calls++; if (Cards_Valid(id) && chest[id] < 255) chest[id]++;
}
void func_800323F8(u8 *base, void *deck, s32 other, s32 flags) {
    (void)deck; (void)other; (void)flags;
    BuildDeckTransitionState *ws = (void *)base;
    for (int p = 0; p < 2; p++) memset(ws[p].card_sort_rank, 7, sizeof(ws[p].card_sort_rank));
}
void func_80032C48(CardList *list) {
    assert(gBuildDeck_pState && list == &gBuildDeck_pState->lists[0]); sorted++;
}
int main(void) {
    static BuildDeckTransitionState ws[2], prior;
    state = calloc(1, sizeof(*state)); assert(state);
    original_award = (void *)Duel_AwardCard;
    original_build_deck = (void *)func_800323F8;
    for (int id = 1; id <= 40; id++) award_card(id);
    for (int id = 1; id <= 40; id++) assert(marked(state->newly_owned, id));
    award_card(1); assert(chest[1] == 2 && marked(state->newly_owned, 1));
    chest[41] = 2; award_card(41); assert(!marked(state->newly_owned, 41));
    gDuel_awPlayerDeck[0] = 42; award_card(42); assert(!marked(state->newly_owned, 42));
    chest[43] = 255; award_card(43); assert(!marked(state->newly_owned, 43));
    award_card(gCard_nCount); assert(marked(state->newly_owned, gCard_nCount));
    award_card(0); award_card(CARD_TABLE_ID_END); award_card(-1);
    assert(!marked(state->awarded, 0) && !marked(state->awarded, CARD_TABLE_ID_END));
    ws[0].deck_cards = ws[1].deck_cards = gDuel_awPlayerDeck;
    ws[0].lists[0].sort_mode = 8; ws[1].lists[0].sort_mode = 0;
    gBuildDeck_pState = &prior;
    build_deck((u8 *)ws, NULL, 0, 0);
    assert(sorted == 1 && gBuildDeck_pState == &prior);
    for (int p = 0; p < 2; p++) {
        for (int id = 1; id <= 40; id++) assert(ws[p].card_sort_rank[id] == 1);
        assert(ws[p].card_sort_rank[41] == 0 && ws[p].card_sort_rank[42] == 0);
        assert(ws[p].card_sort_rank[44] == 7);
    }
    shutdown(); assert(!state);
    build_deck((u8 *)ws, NULL, 0, 0); assert(sorted == 1);
    puts("PASS: 40 NEW rewards, duplicates, equipped ownership, cap, expanded IDs and both editor panes");
    return 0;
}
'''
c = OUT / 'test.c'
c.write_text(fixture)
exe = OUT / 'test.exe'
subprocess.run([shutil.which('i686-w64-mingw32-clang'), '-std=gnu11', '-O2',
                '-Wno-gnu-folding-constant', '-I'+str(ROOT/'src'),
                '-I'+str(ROOT/'examples/mods'), str(c), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
