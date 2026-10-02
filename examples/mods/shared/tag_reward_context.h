#ifndef TAG_REWARD_CONTEXT_V1_H
#define TAG_REWARD_CONTEXT_V1_H
#include <stddef.h>
#include "game/card_constants.h"
/* Owned retail tables, not pointers into Tag's private state. Overrides are
 * resolved by the reward consumer using the existing Tables service. */
typedef struct {
    unsigned abi,size;
    int duelist[2];
    unsigned short weights[2][3][CARD_COUNT];
} TagRewardContext;
/* Resolve tag-duel-menu:reward_context_v1 as this type. NULL,0 is a cheap
 * availability query; otherwise a complete owned copy is required. */
typedef int (*TagRewardQuery)(TagRewardContext *,size_t);
#endif
