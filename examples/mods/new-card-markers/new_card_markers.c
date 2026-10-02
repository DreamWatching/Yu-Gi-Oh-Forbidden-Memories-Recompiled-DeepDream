/* Independent, session-long NEW badges for cards actually acquired in duels. */
#include "game/build_deck_transition_state.h"
#include "game/card_list_sort.h"
#include "game/duel_rewards.h"
#include "game/func_800323F8.h"
#include "game/save_data.h"
#include "pc/cards/cards.h"
#include "pc/mods/modapi.h"
#include <stdlib.h>

#define MARKER_BYTES ((CARD_TABLE_ID_END + 7) / 8)

typedef struct {
    u8 newly_owned[MARKER_BYTES];
    u8 awarded[MARKER_BYTES];
} MarkerState;

static MarkerState *state;
static void *original_award;
static void *original_build_deck;

static int marked(const u8 *bits, int id)
{
    return id > 0 && id < CARD_TABLE_ID_END &&
           (bits[id >> 3] & (1u << (id & 7))) != 0;
}

static void mark(u8 *bits, int id)
{
    if (id > 0 && id < CARD_TABLE_ID_END)
        bits[id >> 3] |= (u8)(1u << (id & 7));
}

static void award_card(s32 id)
{
    int valid = state && Cards_Valid(id);
    int first = 0;
    int before = 0;
    int i;
    if (valid) {
        before = *Cards_ChestSlot(gDuel_awPlayerDeck, id);
        first = before == 0;
        if (first) {
            for (i = 0; i < DECK_SIZE; i++) {
                if (gDuel_awPlayerDeck[i] == id) {
                    first = 0;
                    break;
                }
            }
        }
    }
    ((void (*)(s32))original_award)(id);
    if (!valid) return;
    /* Suppress the game's recent-card badge even for owned duplicates. */
    mark(state->awarded, id);
    if (first && *Cards_ChestSlot(gDuel_awPlayerDeck, id) > before)
        mark(state->newly_owned, id);
}

static void build_deck(u8 *base, void *deck, s32 other, s32 flags)
{
    BuildDeckTransitionState *ws = (BuildDeckTransitionState *)base;
    BuildDeckTransitionState *prior;
    int pane, id;
    ((void (*)(u8 *, void *, s32, s32))original_build_deck)(
        base, deck, other, flags);
    if (!state) return;
    prior = gBuildDeck_pState;
    for (pane = 0; pane < 2; pane++) {
        if (!ws[pane].deck_cards) continue;
        for (id = 1; id <= gCard_nCount; id++) {
            if (marked(state->awarded, id))
                ws[pane].card_sort_rank[id] =
                    marked(state->newly_owned, id) ? 1 : 0;
        }
        if (ws[pane].lists[0].sort_mode == 8) {
            gBuildDeck_pState = &ws[pane];
            func_80032C48(&ws[pane].lists[0]);
        }
    }
    gBuildDeck_pState = prior;
}

static void shutdown(void)
{
    free(state);
    state = 0;
}

int MemoriesModInit(const MemoriesModHost *host, MemoriesMod *mod)
{
    if (host->api < 4) return 0;
    state = calloc(1, sizeof(*state));
    if (!state) return 0;
    mod->api = 4;
    mod->shutdown = shutdown;
    return host->register_state(host, state, sizeof(*state), 1) &&
           host->hook(host, (void *)Duel_AwardCard,
                      (void *)award_card, &original_award) &&
           host->hook(host, (void *)func_800323F8,
                      (void *)build_deck, &original_build_deck);
}
