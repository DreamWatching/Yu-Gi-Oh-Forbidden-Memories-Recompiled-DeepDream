#ifndef DUEL_MATCH_SERVICE_V1_H
#define DUEL_MATCH_SERVICE_V1_H
#include <stddef.h>
typedef struct {
    unsigned abi, size;
    int pending, active, starting_lp, field_choice;
    int start_side, ally_starter, enemy_starter, completed_turns, seats;
} DuelMatchView;
typedef struct {
    unsigned abi, size;
    int (*query)(DuelMatchView *,size_t);
} DuelMatchService;
#endif
