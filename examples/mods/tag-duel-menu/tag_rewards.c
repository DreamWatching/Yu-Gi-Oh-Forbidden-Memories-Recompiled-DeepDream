/* Embedded optional reward policy. Core Tag Duels always retains inventory repairs. */
#include "../shared/tag_reward_context.h"
#include "pc/mods/modapi.h"
#include "pc/cards/cards.h"
#include "pc/cards/tables.h"
#include "pc/cards/drops.h"
#include "pc/platform/settings.h"
#include "game/duel_rewards.h"
#include "game/duel_side_state.h"
#include "game/duel_result_display.h"
#include "game/duel_scene_state.h"
#include "game/main_modes.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tag_rewards.h"
#define TAG_RARE_MAX_WEIGHT 16
static const MemoriesModHost *tag_host;
static TagRewardQuery context_query;
static TagRewardsState *reward_state;
#define state (*reward_state)
void TagRewards_SetState(TagRewardsState *value) { reward_state=value; }
static void *original_select_drop,*original_result_rewards;
static int reward_rolling,reward_roll_index,reward_roll_count;
static int reward_enabled(void)
{
    return tag_host->applied(tag_host) && tag_host->setting(tag_host,"tag_rewards",1);
}
extern u8 D_8009B26C;

static const u16 *drop_weights(int opponent, int pool, int *edited)
{
    const u16 *retail = state.context.weights[opponent][pool];
    int duelist = state.context.duelist[opponent];
    const u16 *override = Tables_PoolFor(duelist, TABLES_POOL_POW + pool,
                                         retail);
    *edited = override != 0;
    return override ? override : retail;
}

static int drop_card(int id)
{
    return id <= CARD_COUNT ? Cards_PickVariant(id, CARDS_USE_DROP) : id;
}

static int roll_opponent_pool(int opponent, int pool, int *chosen_weight)
{
    int edited, id, sum = 0;
    int threshold = (rand() & (DUEL_DROP_WEIGHT_TOTAL - 1)) + 1;
    const u16 *weights = drop_weights(opponent, pool, &edited);
    int last = edited ? gCard_nCount : CARD_COUNT;
    for (id = 1; id <= last; id++) {
        sum += weights[edited ? id : id - 1];
        if (sum >= threshold) {
            *chosen_weight = weights[edited ? id : id - 1];
            return drop_card(id);
        }
    }
    return 0;
}

static int rare_weight_floor(int pool)
{
    int opponent, floor = INT_MAX;
    for (opponent = 0; opponent < 2; opponent++) {
        int edited, id;
        const u16 *weights = drop_weights(opponent, pool, &edited);
        int last = edited ? gCard_nCount : CARD_COUNT;
        for (id = 1; id <= last; id++) {
            int weight = weights[edited ? id : id - 1];
            if (weight > 0 && weight < floor) floor = weight;
        }
    }
    return floor;
}

static int roll_rare_drop(int pool, int *source, int *chosen_weight)
{
    int floor = rare_weight_floor(pool);
    int limit = floor <= TAG_RARE_MAX_WEIGHT ? TAG_RARE_MAX_WEIGHT : floor;
    int opponent, candidates = 0, pick;
    if (floor == INT_MAX) return 0;
    for (opponent = 0; opponent < 2; opponent++) {
        int edited, id;
        const u16 *weights = drop_weights(opponent, pool, &edited);
        int last = edited ? gCard_nCount : CARD_COUNT;
        for (id = 1; id <= last; id++) {
            int weight = weights[edited ? id : id - 1];
            if (weight > 0 && weight <= limit) candidates++;
        }
    }
    if (!candidates) return 0;
    pick = rand() % candidates;
    for (opponent = 0; opponent < 2; opponent++) {
        int edited, id;
        const u16 *weights = drop_weights(opponent, pool, &edited);
        int last = edited ? gCard_nCount : CARD_COUNT;
        for (id = 1; id <= last; id++) {
            int weight = weights[edited ? id : id - 1];
            if (weight > 0 && weight <= limit && pick-- == 0) {
                *source = opponent;
                *chosen_weight = weight;
                return drop_card(id);
            }
        }
    }
    return 0;
}

static s32 select_drop(s32 pool)
{
    int result, roll, rare, opponent, weight = 0;
    if (!reward_enabled() || !reward_rolling || pool < 0 || pool > 2)
        return ((s32 (*)(s32))original_select_drop)(pool);
    roll = reward_roll_index++;
    rare = roll == reward_roll_count - 1;
    opponent = rare ? -1 : rand() & 1;
    result = rare ? roll_rare_drop(pool, &opponent, &weight)
                  : roll_opponent_pool(opponent, pool, &weight);
    if (!result) {
        result = ((s32 (*)(s32))original_select_drop)(pool);
        opponent = 0;
        weight = -1;
        rare = 0; /* An empty pool cannot promise or label rare spoils. */
    }
    state.source_count[opponent]++;
    if (rare) state.spoils_source = (u8)opponent;
    tag_host->log(tag_host,
        "tag drop: roll %d/%d source %d (duelist %d) card %d weight %d/2048 pool %d%s",
        roll + 1, reward_roll_count, opponent + 1,
        state.context.duelist[opponent],
        result, weight, pool, rare ? " RARE SPOILS" : "");
    return result;
}
static void centred_text(int x, int width, int middle, const char *text,
                         uint32_t colour)
{
    int text_width = tag_host->text_width(tag_host, text, 1);
    tag_host->draw_text(tag_host, x + (width - text_width) / 2, middle,
                        text, colour, 1);
}

static void draw_reward_source(int opponent, int x, int width)
{
    char count[48];
    uint32_t accent = state.spoils_source == opponent
        ? 0xffd35a : 0x92bbd8;
    tag_host->fill(tag_host, x, 18, width, 84, 0x050b14, 225);
    tag_host->fill(tag_host, x, 18, width, 3, accent, 255);
    centred_text(x, width, 35, Tables_DuelistShortName(state.context.duelist[opponent]), 0xe3e8ef);
    snprintf(count, sizeof(count), "%u DROP ROLLS",
             state.source_count[opponent]);
    centred_text(x, width, 58, count, 0xb0c9da);
    if (state.spoils_source == opponent)
        centred_text(x, width, 82, "RARE SPOILS", accent);
}

static void result_rewards(void)
{
    int opening=!(gDuel_wSceneStateFlags&0x8000);
    reward_rolling=0;
    if(opening) {
        memset(&state,0,sizeof(state));state.spoils_source=255;
        if(reward_enabled() && gDuel_bWinnerSide==0 &&
           context_query(&state.context,sizeof(state.context)) &&
           state.context.abi==1 && state.context.size==sizeof(state.context)) {
            reward_roll_count=Settings_Get(SET_CARD_DROPS);
            if(reward_roll_count<1)reward_roll_count=1;
            if(reward_roll_count>CARD_DROPS_MAX)reward_roll_count=CARD_DROPS_MAX;
            reward_roll_index=0;reward_rolling=1;
        }
    }
    ((void (*)(void))original_result_rewards)();
    reward_rolling=0;
}
unsigned TagRewards_Signature(void)
{
    if(!reward_enabled() || !context_query(NULL,0) ||
       (D_8009B26C&31)!=MAIN_MODE_DUEL ||
       (gDuel_wSceneStateFlags&DUEL_SCENE_PHASE_MASK)!=13 ||
       (state.source_count[0]+state.source_count[1])==0)return 0;
    return 1+state.source_count[0]*31+state.source_count[1]*131+state.spoils_source*17;
}
void TagRewards_Draw(void)
{
    int width,height,scale,game_width,margin,panel_width;
    if(!TagRewards_Signature())return;
    tag_host->overlay_size(tag_host,&width,&height,&scale);
    if(width<=0 || height<=0)return;
    game_width=height*4/3;if(game_width>width)game_width=width;
    margin=(width-game_width)/2;if(margin<64)return;
    panel_width=margin-8;if(panel_width>170)panel_width=170;
    draw_reward_source(0,(margin-panel_width)/2,panel_width);
    draw_reward_source(1,width-margin+(margin-panel_width)/2,panel_width);
}
int TagRewards_Init(const MemoriesModHost *host)
{
    if(host->api<4)return 0;tag_host=host;
    context_query=host->find(host,"tag-duel-menu:reward_context_v1");
    if(!context_query || !reward_state)return 0;
    return host->hook(host,(void *)Duel_SelectCardDrop,(void *)select_drop,&original_select_drop) &&
           host->hook(host,(void *)DuelScene_UpdateResultRewards,(void *)result_rewards,&original_result_rewards);
}

void TagRewards_Reset(void)
{
    memset(&state,0,sizeof(state));state.spoils_source=255;
    reward_rolling=reward_roll_index=reward_roll_count=0;
}
