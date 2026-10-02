#ifndef TAG_REWARDS_H
#define TAG_REWARDS_H
#include "../shared/tag_reward_context.h"
typedef struct {
    TagRewardContext context;
    unsigned source_count[2];
    unsigned spoils_source;
} TagRewardsState;
void TagRewards_SetState(TagRewardsState *);
#endif
