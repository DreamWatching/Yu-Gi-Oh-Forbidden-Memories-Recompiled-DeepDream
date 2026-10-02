#ifndef TAG_DUEL_RANK_H
#define TAG_DUEL_RANK_H
#include "game/duel_side_state.h"

/* Rank-only projection: never change decks, LP or counters in gameplay.
 * Round half actions upward so a genuine action is not lost to truncation.
 * Undealt teammates are credited with their opening hand for the scoring
 * baseline only; no cards are drawn, granted or removed by this helper. */
static void TagRank_Project(const DuelSideState *live, DuelSideState *out,
                            int team, int first_drawn, int second_drawn)
{
    int maximum = live->max_life_points;
    *out = *live;
    if (maximum > 0 && maximum != 8000) {
        int lp = live->life_points.unsigned_value;
        if (lp > maximum) lp = maximum;
        out->life_points.signed_value = (s16)((lp * 8000 + maximum / 2) / maximum);
    }
    if (!team) return;
    if (first_drawn < HAND_SIZE) first_drawn = HAND_SIZE;
    if (second_drawn < HAND_SIZE) second_drawn = HAND_SIZE;
    if (first_drawn > DECK_SIZE) first_drawn = DECK_SIZE;
    if (second_drawn > DECK_SIZE) second_drawn = DECK_SIZE;
    out->deck_draw_cursor = (s8)((first_drawn + second_drawn + 1) / 2);
#define TEAM_COUNTER(name) out->rank.name = (u8)((live->rank.name + 1) / 2)
    TEAM_COUNTER(turns_taken);
    TEAM_COUNTER(effective_attacks);
    TEAM_COUNTER(defensive_wins);
    TEAM_COUNTER(face_down_plays);
    TEAM_COUNTER(fusions_initiated);
    TEAM_COUNTER(equips_used);
    TEAM_COUNTER(pure_magic_used);
    TEAM_COUNTER(traps_triggered);
#undef TEAM_COUNTER
}
#endif
