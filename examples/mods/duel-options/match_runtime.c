#include "match_runtime.h"
#include "option_registry.h"
#include "../shared/duel_match.h"
#include "../shared/tag_duel_service.h"

#include "../shared/duel_participants.h"

#define AI_GET_HAND_SIZE_RETURNS_S32
#include "game/ai.h"
#include "game/ai_opponent_data.h"
#include "game/card_constants.h"
#include "game/duel_card.h"
#include "game/duel_card_checks.h"
#include "game/duel_check_ritual.h"
#include "game/duel_check_quit_input.h"
#define DUEL_CARD_STAGING_DECK_VIEW
#include "game/duel_card_staging.h"
#include "game/duel_deck_card.h"
#include "game/duel_deck_card_data.h"
#include "game/display_object.h"
#include "game/display_object_work_slots.h"
#include "game/duel_grid.h"
#define DUEL_SAVE_WINDOWS_AS_PAIR
#include "game/duel_init_scene.h"
#include "game/duel_rewards.h"
#define DUEL_PACKAGE_STAGE_RAW_ARENAS
#include "game/duel_load_package_stage.h"
#include "game/duel_package.h"
#include "game/duel_result_display.h"
#include "game/duel_init_scene.h"
#include "game/duel_card_record_lifecycle.h"
#include "game/duel_effect_request.h"
#include "game/display_object_config.h"
#include "game/file_transfer.h"
#include "game/view_state.h"
#include "game/trig_constants.h"
#include "game/duel_scene_state.h"
#include "game/duel_scene_card_placement.h"
#include "game/duel_scene_turn_switch.h"
#include "game/duel_scene_update.h"
#include "game/func_8001B938.h"
#include "game/duel_shuffle_deck.h"
#define D_8009B360_AS_SIDE_ARRAY
#include "game/duel_side_state.h"
#define DUEL_TERRAIN_SCALAR_IN_DATA
#include "game/duel_terrain_boost.h"
#include "../tag-duel-menu/duel_rules.h"
#include "../tag-duel-menu/tag_rank.h"
#include "game/func_80024DC8.h"
#include "game/fade.h"
#include "game/input.h"
#include "game/main_menu_selection.h"
#include "game/main_modes.h"
#include "game/main_services.h"
#include "overlays/free_duel/free_duel.h"
#include "game/save_data.h"
#include "game/sound.h"
#include "pc/cards/cards.h"
#include "pc/cards/drops.h"
#include "pc/cards/tables.h"
#include "pc/platform/settings.h"
#include "pc/mods/modapi.h"
#include "pc/saves/save_slots.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct {
    int seats;
    int opponent_draw_pool;
    int pending;
    int active;
    int starting_lp;
    int field_choice; /* 0 normal, 1..6 retail fields, 7 static random, 8 dynamic. */
    int start_side;
    int ally_starter;
    int enemy_starter;
    int completed_turns;
    int terrain_effect_stage;
    int terrain_effect_slot;
    int terrain_next;
    int terrain_queued;
} TagMatchRules;
static const MemoriesModHost *tag_host;
static TagMatchRules match_rules;
static void *original_init_scene,*original_init_sides,*original_turn_switch,*original_side_input,*original_ai_run,*original_hand_size;
extern u8 D_8009B26C;
void DuelMatch_Configure(int starting_lp, int field_choice,
                            int start_side, int ally_starter,
                            int enemy_starter, int seats)
{
    DuelOptions_Freeze(seats==4 ? DUEL_OPTION_TAG : DUEL_OPTION_SOLO);
    int draw_pool=1;
    DuelOptions_MatchValue("duel-options","hard_mode",&draw_pool);
    match_rules.opponent_draw_pool=draw_pool ? 20 : 5;
    match_rules.seats = seats == 4 ? 4 : 2;
    match_rules.pending = 1;
    match_rules.active = 0;
    match_rules.starting_lp = starting_lp >= 4000 && starting_lp <= 20000
        ? starting_lp : 8000;
    match_rules.field_choice = field_choice >= 0 && field_choice <= 8
        ? field_choice : 0;
    match_rules.start_side = start_side != 0;
    match_rules.ally_starter = ally_starter != 0;
    match_rules.enemy_starter = enemy_starter != 0;
    if (match_rules.start_side)
        match_rules.ally_starter = TagRules_WaitingStarter(1, match_rules.enemy_starter);
    else
        match_rules.enemy_starter = TagRules_WaitingStarter(0, match_rules.ally_starter);
    match_rules.completed_turns = 0;
    match_rules.terrain_effect_stage = 0;
    match_rules.terrain_effect_slot = -1;
    match_rules.terrain_queued = 0;
}
static void init_sides(void)
{
    int side;
    ((void (*)(void))original_init_sides)();
    if (!match_rules.pending) return;
    for (side = 0; side < DUEL_SIDE_COUNT; side++) {
        D_800E9FF0[side].life_points.signed_value =
            (s16)match_rules.starting_lp;
        D_800E9FF0[side].max_life_points =
            (s16)match_rules.starting_lp;
    }
}
static void init_scene(void)
{
    int field = match_rules.field_choice;
    if (match_rules.pending)
        gDuel_bTerrain = (u8)(field >= 7 ? 1 + rand() % 6 : field);
    ((void (*)(void))original_init_scene)();
    if (!match_rules.pending) {
        match_rules.active = 0;
        DuelOptions_ClearMatch();
        return;
    }
    match_rules.active = 1;
    match_rules.pending = 0;
    match_rules.terrain_effect_stage = 0;
    match_rules.terrain_effect_slot = -1;
    match_rules.terrain_queued = 0;
    D_8009B16C |= 0x1000; /* Retain native human opening-turn attack guard. */
    if (match_rules.start_side) {
        D_8009B1D5 = 1;
        D_8009B1C8 = &D_800E9FF0[1];
        D_8009B22C = &D_800907D8[DUEL_FIELD_SIDE_GRID_SLOT_COUNT];
        D_800F2848.angle = TRIG_ANGLE_HALF_TURN + TRIG_ANGLE_QUARTER_TURN;
        ViewState_ApplyOrbit();
    }
    const TagDuelService *teams = tag_host->find(tag_host,"tag-duel-menu:tag_v1");
    if(teams && teams->abi==1 && teams->size==sizeof(*teams) && teams->available && teams->available() && teams->reset_presentation)
        teams->reset_presentation();
    tag_host->log(tag_host,
        "free duel rules: LP %d terrain %d mode %d first side %d allies %d rivals %d",
        match_rules.starting_lp, gDuel_bTerrain, field,
        match_rules.start_side, match_rules.ally_starter,
        match_rules.enemy_starter);
}
static void reroll_terrain(void)
{
    DuelEffectRequest *request;
    if (match_rules.terrain_effect_stage) {
        match_rules.terrain_queued = 1;
        return;
    }
    /* Outside a spell handler, pause the scene while this request runs.
     * AllocateRequest alone does not raise the pool ACTIVE status, allowing
     * draw resolution to overlap the field effect's shared workspace. */
    request = DuelEffect_CreateRequest(0xA);
    if (!request) {
        match_rules.terrain_queued = 1;
        return;
    }
    match_rules.terrain_next = TagRules_NextTerrain(gDuel_bTerrain, (unsigned)rand());
    request->field_1A = (s16)(match_rules.terrain_next - 1);
    match_rules.terrain_effect_slot = (int)(request - D_800EAD88);
    match_rules.terrain_effect_stage = 1;
    SD_SEPlayFull(0x13);
    tag_host->log(tag_host,
        "dynamic field: turn %d animating change %d -> %d",
        match_rules.completed_turns, gDuel_bTerrain, match_rules.terrain_next);
}
static void terrain_animation_tick(void)
{
    DuelEffectRequest *request;
    int card;
    if (!match_rules.active) return;
    if (!match_rules.terrain_effect_stage) {
        if (match_rules.terrain_queued) {
            match_rules.terrain_queued = 0;
            reroll_terrain();
        }
        return;
    }
    if (match_rules.terrain_effect_slot < 0 ||
        match_rules.terrain_effect_slot >= DUEL_EFFECT_REQUEST_COUNT) {
        match_rules.terrain_effect_stage = 0;
        return;
    }
    request = &D_800EAD88[match_rules.terrain_effect_slot];
    if (match_rules.terrain_effect_stage == 1) {
        if (!request->field_1D) return;
        /* The retail effect flashes the field first. Begin its texture
         * transfer only when that effect reaches the same cue as a spell. */
        gDuel_bTerrain = (u8)match_rules.terrain_next;
        File_RequestAsyncTransfer(0, (u8 *)0,
            gDuel_bTerrain * DUEL_TERRAIN_PACKAGE_SECTOR_COUNT +
                DUEL_TERRAIN_EFFECT_DATA_FIRST_SECTOR,
            DUEL_TERRAIN_EFFECT_DATA_SECTOR_COUNT, 0, 0, 0x1000280);
        match_rules.terrain_effect_stage = 2;
    } else if (match_rules.terrain_effect_stage == 2) {
        if ((D_8009B0F4_abs & FILE_TRANSFER_REQUEST_BLOCKED_MASK) ||
            D_8009B134_abs) return;
        if (D_8009B214)
            DisplayObject_SetResourceVariant((DisplayObjectConfig *)D_8009B214,
                                             match_rules.terrain_next);
        request->field_1A = -2;
        match_rules.terrain_effect_stage = 3;
    } else if (!(request->flags & DUEL_EFFECT_REQUEST_FLAG_ACTIVE)) {
        for (card = 0; card < DUEL_CARD_RECORD_COUNT; card++) {
            DuelCardRecord *record = &D_801A7AD8[card];
            if ((record->flags & DUEL_CARD_FLAG_OCCUPIED) && record->object)
                record->terrain_modifier = Duel_GetTerrainBoost(
                    ((DisplayObject *)record->object)->field_68);
        }
        tag_host->log(tag_host, "dynamic field: changed to %d after turn %d",
                      gDuel_bTerrain, match_rules.completed_turns);
        match_rules.terrain_effect_stage = 0;
        match_rules.terrain_effect_slot = -1;
    }
}
static void turn_switch(void)
{
    int before=D_8009B1D5;
    ((void (*)(void))original_turn_switch)();
    if(match_rules.active && before!=D_8009B1D5) {
        match_rules.completed_turns++;
        if(match_rules.field_choice==8 && TagRules_FieldChangeDue(match_rules.completed_turns,match_rules.seats))reroll_terrain();
    }
}
static void side_input(void)
{
    terrain_animation_tick();((void (*)(void))original_side_input)();
}
static s32 opponent_hand_size(void)
{
    /* Match-scoped policy: both tag rivals, or the solo CPU. Never changes
     * campaign tables, player/partner hands, or the number of visible slots. */
    if(tag_host->applied(tag_host) && match_rules.active &&
       (D_8009B26C&31)==MAIN_MODE_DUEL && D_8009B1D5==1 && gDuel_bOpponentID>=0)
        return match_rules.opponent_draw_pool;
    return ((s32 (*)(void))original_hand_size)();
}
static s32 ai_run(void)
{
    if(TagRules_OpeningAttacksBlocked(match_rules.active,match_rules.completed_turns) &&
       gAiScript_State.script_base==D_801A9800)return 3;
    return ((s32 (*)(void))original_ai_run)();
}
static int query(DuelMatchView *out,size_t size)
{
    if(!out || size<sizeof(*out) || !tag_host->applied(tag_host))return 0;
    memset(out,0,sizeof(*out));out->abi=1;out->size=sizeof(*out);
    out->pending=match_rules.pending;out->active=match_rules.active;
    out->starting_lp=match_rules.starting_lp;out->field_choice=match_rules.field_choice;
    out->start_side=match_rules.start_side;out->ally_starter=match_rules.ally_starter;
    out->enemy_starter=match_rules.enemy_starter;out->completed_turns=match_rules.completed_turns;out->seats=match_rules.seats;
    return 1;
}
static int rank_side(const DuelSideState *live,DuelSideState *out)
{
    if(!tag_host->applied(tag_host) || !match_rules.active || (D_8009B26C&31)!=MAIN_MODE_DUEL ||
       (live!=&D_800E9FF0[0] && live!=&D_800E9FF0[1]))return 0;
    TagRank_Project(live,out,0,live->deck_draw_cursor,live->deck_draw_cursor);return 1;
}
static const DuelMatchService service={1,sizeof(DuelMatchService),query};
void DuelMatch_Clear(void) { memset(&match_rules,0,sizeof(match_rules));DuelOptions_ClearMatch(); }
int DuelMatch_Init(const MemoriesModHost *host)
{
    tag_host=host;
    if(!host->register_state(host,&match_rules,sizeof(match_rules),11) ||
       !host->provide(host,"match_v1",(void *)&service) ||
       !host->provide(host,"rank_side_v1",(void *)rank_side))return 0;
    return host->hook(host,(void *)Duel_InitScene,(void *)init_scene,&original_init_scene) &&
           host->hook(host,(void *)Duel_InitSideStates,(void *)init_sides,&original_init_sides) &&
           host->hook(host,(void *)DuelScene_UpdateTurnSwitch,(void *)turn_switch,&original_turn_switch) &&
           host->hook(host,(void *)DuelScene_UpdateWithSideInput,(void *)side_input,&original_side_input) &&
           host->hook(host,(void *)AiScript_Run,(void *)ai_run,&original_ai_run) &&
           host->hook(host,(void *)Ai_GetHandSize,(void *)opponent_hand_size,&original_hand_size);
}
