/* Four-duelist tag runtime for the PC port.
 *
 * The retail arena remains two sides: teammates share LP and field zones.
 * This layer gives each of the four duelists a private shuffled deck, hand,
 * and draw cursor, then swaps the active teammate into the retail side at a
 * turn boundary. Human/AI ownership is selected independently of team side.
 */
#include "tag_duel_runtime.h"
#include "../shared/duel_match.h"
#include "../shared/duel_participants.h"
#include "../shared/tag_reward_context.h"
#include "tag_rewards.h"
#include "partner_planner.h"
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
#include "duel_rules.h"
#include "tag_rank.h"
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

#define TAG_MEMBER_COUNT 4
#define TAG_THUMB_SIZE DUEL_CARD_DATA_BLOCK_SIZE
#define TAG_WA_PATH "\\DATA\\WA_MRG.MRG;1"
#define TAG_CAMPAIGN_PORTRAIT_FIRST_SECTOR 0x1E6A
#define TAG_CAMPAIGN_PORTRAIT_SECTORS 30
#define TAG_FREE_DUEL_PORTRAIT_FIRST_SECTOR 0x1EAA
#define TAG_FREE_DUEL_PORTRAIT_SECTORS 48
#define TAG_PORTRAIT_WIDTH 48
#define TAG_PORTRAIT_HEIGHT 48
#define TAG_PORTRAIT_IMAGE_SIZE 0x900
#define TAG_PORTRAIT_RECORD_SIZE 0x980
#define TAG_YUGI_WAITING_PORTRAIT 8
#define TAG_YUGI_ACTIVE_PORTRAIT 9
#define TAG_TURN_EFFECT_US 420000

enum { TAG_PLAYER, TAG_OPPONENT_1, TAG_PARTNER, TAG_OPPONENT_2 };
enum { TAG_PORTRAIT_WAITING, TAG_PORTRAIT_ACTIVE, TAG_PORTRAIT_STATES };
enum { TAG_STATUS_OFF, TAG_STATUS_PENDING, TAG_STATUS_ACTIVE, TAG_STATUS_ERROR };
enum { TAG_ERROR_NONE, TAG_ERROR_PARTNER_DECK, TAG_ERROR_PARTNER_DATA,
       TAG_ERROR_OPPONENT_DECK, TAG_ERROR_OPPONENT_DATA,
       TAG_ERROR_OPPONENT_1_DROPS, TAG_ERROR_OPPONENT_2_DROPS };

typedef struct {
    s16 deck[DECK_SIZE];
    DuelDeckCardRecord records[DECK_SIZE];
    u8 thumbnails[DECK_SIZE][TAG_THUMB_SIZE];
    s16 thumbnail_ids[DECK_SIZE];
    s8 hand[HAND_SIZE];
    s8 draw_cursor;
    s8 duelist;
    s8 ready;
} TagMember;

typedef struct {
    int requested;
    int active;
    int status;
    int error;
    int partner_control;
    int partner_deck_source;
    int partner_deck_slot;
    int hard_mode;
    int active_member[DUEL_SIDE_COUNT];
    int entries[DUEL_SIDE_COUNT];
    int wa_lba;
    /* A field/hand record keeps the exact private card which created it.
     * Deck positions are only 0..39 per side and therefore collide between
     * teammates; they are not an identity once tag rotation begins. */
    s8 record_member[DUEL_CARD_RECORD_COUNT];
    s8 record_local[DUEL_CARD_RECORD_COUNT];
    u16 opponent_drops[2][3][CARD_COUNT];
    s16 reward_ids[CARD_DROPS_MAX];
    u8 reward_before[CARD_DROPS_MAX];
    u8 reward_expected[CARD_DROPS_MAX];
    u8 reward_count;
    u8 reward_checked;
    TagMember member[TAG_MEMBER_COUNT];
    TagRewardsState rewards;
} TagState;



static const MemoriesModHost *tag_host;
static TagState *tag;
static unsigned short explicit_partner_deck[40];
static int has_explicit_partner_deck;
void TagDuel_SetPartnerDeck(const unsigned short cards[40])
{
    has_explicit_partner_deck = cards != NULL;
    if (cards) memcpy(explicit_partner_deck, cards, sizeof(explicit_partner_deck));
}
static DuelMatchView match_rules;
static const DuelMatchService *match_service;
static void refresh_match(void)
{
    memset(&match_rules,0,sizeof(match_rules));
    if(match_service)match_service->query(&match_rules,sizeof(match_rules));
}
static void *original_populate;
static void *original_turn_switch;
static void *original_side_input;
static void *original_ai_run;
static void *original_setup_card;
static void *original_ai_swap;
static void *original_result_rewards;
static int (*copy_deck_slot)(int, unsigned short *);
static int displayed_member[DUEL_SIDE_COUNT];
static int previous_displayed_member[DUEL_SIDE_COUNT];
static int transition_side = -1;
static uint64_t transition_started;
static unsigned turn_revision;
static unsigned last_card_anomaly[DUEL_CARD_RECORD_COUNT];
static int ai_swapping;
static int displayed_for_side(int side, int current_side);
extern unsigned char D_800EAE88[];
extern u8 D_8009B26C;

static int member_index(int side, int teammate)
{
    return side ? (teammate ? TAG_OPPONENT_2 : TAG_OPPONENT_1)
                : (teammate ? TAG_PARTNER : TAG_PLAYER);
}






static void clear_private(TagMember *m)
{
    int i;
    m->draw_cursor = 0;
    for (i = 0; i < HAND_SIZE; i++) m->hand[i] = -1;
}

static int read_thumbnail(int id, u8 *out)
{
    u8 sector[2048];
    int base = Cards_BaseId(id);
    if (base < 1 || tag->wa_lba < 0 ||
        !tag_host->disc_read(tag_host, tag->wa_lba + base - 1, 1, sector))
        return 0;
    memcpy(out, sector, TAG_THUMB_SIZE);
    Cards_PatchThumbnail(id, out);
    return 1;
}

static int prepare_member(TagMember *m, const s16 *deck, int side)
{
    int i;
    clear_private(m);
    for (i = 0; i < DECK_SIZE; i++) {
        int slot = side * DECK_SIZE + i;
        m->deck[i] = deck[i];
        m->records[i].id = deck[i];
        m->records[i].deck_index = (s8)slot;
        m->records[i].data_block_index = (u8)slot;
        m->records[i].flags_04 = 0;
        m->records[i].unk_05 = 0;
        if (!read_thumbnail(deck[i], m->thumbnails[i])) return 0;
        m->thumbnail_ids[i] = deck[i];
    }
    m->ready = 1;
    return 1;
}

static int deal_signature(int duelist, s16 deck[DECK_SIZE])
{
    u8 pool[DUELIST_DATA_SECTOR_COUNT * 2048];
    u8 held[CARD_COUNT];
    s16 dealt[DECK_SIZE];
    u8 order[DECK_SIZE];
    int count = 0;
    int attempts = 0;
    int lba = tag->wa_lba + DUELIST_DATA_FIRST_SECTOR +
              duelist * DUELIST_DATA_SECTOR_COUNT;
    if (tag->wa_lba < 0 ||
        tag_host->disc_read(tag_host, lba, DUELIST_DATA_SECTOR_COUNT, pool) !=
            DUELIST_DATA_SECTOR_COUNT)
        return 0;

    /* These three sectors are a 722-entry table of draw weights, not a
     * literal card list. The retail shuffler reads the same table through
     * gDuel_awOpponentDeckPool when its source argument is zero. Deal it
     * here so a second opponent or partner can have an independent deck. */
    memset(held, 0, sizeof(held));
    while (count < DECK_SIZE && attempts++ < 100000) {
        int limit = (rand() & (DUEL_DROP_WEIGHT_TOTAL - 1)) + 1;
        int accumulated = 0;
        int card;
        for (card = 0; card < STARTER_DECK_WEIGHT_SCAN_COUNT; card++) {
            accumulated += ((u16 *)pool)[card];
            if (accumulated >= limit) break;
        }
        if (card >= CARD_COUNT || held[card] >= DECK_CARD_COPY_LIMIT) continue;
        held[card]++;
        dealt[count++] = Cards_PickVariant(card + 1, CARDS_USE_OPPONENT);
    }
    if (count != DECK_SIZE) return 0;
    Duel_ShuffleDeck((s32)dealt, (u8 *)deck, order);
    return 1;
}

int TagDuel_SignatureDeck(int duelist, unsigned short out[40])
{
    if (!tag || duelist < 1 || duelist >= 40) return 0;
    return deal_signature(duelist, (s16 *)out);
}

static int read_opponent_drops(int opponent)
{
    u8 block[DUELIST_DATA_SECTOR_COUNT * 2048];
    int lba = tag->wa_lba + DUELIST_DATA_FIRST_SECTOR +
              tag->member[opponent ? TAG_OPPONENT_2 : TAG_OPPONENT_1].duelist *
                  DUELIST_DATA_SECTOR_COUNT;
    int pool;
    if (tag->wa_lba < 0 ||
        tag_host->disc_read(tag_host, lba, DUELIST_DATA_SECTOR_COUNT, block) !=
            DUELIST_DATA_SECTOR_COUNT)
        return 0;
    for (pool = 0; pool < 3; pool++)
        memcpy(tag->opponent_drops[opponent][pool],
               block + (pool + 1) * sizeof(DuelDropTable),
               sizeof(tag->opponent_drops[opponent][pool]));
    return 1;
}
















static void shuffle_explicit(const s16 source[DECK_SIZE],
                             s16 deck[DECK_SIZE])
{
    u8 order[DECK_SIZE];
    Duel_ShuffleDeck((s32)source, (u8 *)deck, order);
}

static void capture_retail_member(int which, int side)
{
    TagMember *m = &tag->member[which];
    int i;
    clear_private(m);
    for (i = 0; i < DECK_SIZE; i++) {
        int slot = side * DECK_SIZE + i;
        m->deck[i] = gDuel_aDeckCardRecords[slot].id;
        m->records[i] = gDuel_aDeckCardRecords[slot];
        memcpy(m->thumbnails[i], D_8018C2D8 + slot * TAG_THUMB_SIZE,
               TAG_THUMB_SIZE);
        m->thumbnail_ids[i] = m->records[i].id;
    }
    m->ready = 1;
}

static int private_source(const DuelDeckCardRecord *source,
                          int *which_out, int *local_out)
{
    uintptr_t at = (uintptr_t)source;
    int which;
    if (!tag || !source) return 0;
    for (which = 0; which < TAG_MEMBER_COUNT; which++) {
        uintptr_t first = (uintptr_t)&tag->member[which].records[0];
        uintptr_t last = (uintptr_t)&tag->member[which].records[DECK_SIZE];
        if (at >= first && at < last &&
            (at - first) % sizeof(DuelDeckCardRecord) == 0) {
            if (which_out) *which_out = which;
            if (local_out)
                *local_out = (int)((at - first) / sizeof(DuelDeckCardRecord));
            return 1;
        }
    }
    return 0;
}

static int member_side(int which)
{
    return which == TAG_OPPONENT_1 || which == TAG_OPPONENT_2;
}

static int normalized_card_record(int record)
{
    return (record & 0x80)
        ? (record & 0x7f) + DUEL_CARD_SIDE_RECORD_COUNT : record;
}

static int normalized_deck_slot(int card_record, int deck_index)
{
    int side = card_record >= DUEL_CARD_SIDE_RECORD_COUNT;
    if (deck_index & 0x80) return (deck_index & 0x7f) + DECK_SIZE;
    if (side && deck_index < DECK_SIZE) return deck_index + DECK_SIZE;
    return deck_index;
}

static int source_thumbnail(int which, DuelDeckCardRecord *source)
{
    TagMember *m = &tag->member[which];
    int side = member_side(which);
    int block = source->data_block_index - side * DECK_SIZE;
    if (block < 0 || block >= DECK_SIZE) return -1;
    if (m->thumbnail_ids[block] != source->id) {
        if (!read_thumbnail(source->id, m->thumbnails[block])) return -1;
        m->thumbnail_ids[block] = source->id;
    }
    return block;
}

static DuelDeckCardRecord *mapped_source(int card_record)
{
    int which, local;
    if (card_record < 0 || card_record >= DUEL_CARD_RECORD_COUNT) return 0;
    which = tag->record_member[card_record];
    local = tag->record_local[card_record];
    if (which < 0 || which >= TAG_MEMBER_COUNT ||
        local < 0 || local >= DECK_SIZE) return 0;
    return &tag->member[which].records[local];
}

static DuelDeckCardRecord *remember_source(int card_record,
                                           DuelDeckCardRecord *source)
{
    int which, local;
    if (!private_source(source, &which, &local)) return 0;
    tag->record_member[card_record] = (s8)which;
    tag->record_local[card_record] = (s8)local;
    return source;
}

/* Placement publishes the fusion result in D_8009B150 before rebuilding its
 * selected card record. The result survives a transfer into the chosen field
 * slot only if both setups use that selected object's private source. The
 * shared deck mirror can still contain the material at the same deck index. */
static DuelDeckCardRecord *fusion_source_for_setup(int card_record,
                                                    int deck_slot)
{
    DisplayObject *object;
    DuelDeckCardRecord *source;
    int result_id, source_record;
    if (((gDuel_wSceneStateFlags & DUEL_SCENE_PHASE_MASK) != 7 &&
         (gDuel_wSceneStateFlags & DUEL_SCENE_PHASE_MASK) != 8) ||
        !(D_8009B150 & 0x8000)) return 0;
    object = D_800E9EF0[0];
    if (!object) return 0;
    source_record = object->field_6A;
    if (source_record < 0 || source_record >= DUEL_CARD_RECORD_COUNT ||
        (card_record != source_record && card_record != D_8009B19C) ||
        normalized_deck_slot(card_record, object->field_6B) != deck_slot)
        return 0;
    source = (DuelDeckCardRecord *)D_801A7AD8[source_record].data;
    if (!private_source(source, 0, 0)) source = mapped_source(source_record);
    if (!private_source(source, 0, 0)) return 0;
    result_id = D_8009B150 & CARD_ID_FIELD_MASK;
    if (!Cards_Valid(result_id)) return 0;
    source->id = (s16)result_id;
    return remember_source(card_record, source);
}

static DuelDeckCardRecord *occupied_source_for_setup(int card_record,
                                                     int deck_slot, int side,
                                                     int require_slot)
{
    DuelDeckCardRecord *source;
    int which;
    if (!(D_801A7AD8[card_record].flags & DUEL_CARD_FLAG_OCCUPIED)) return 0;
    source = (DuelDeckCardRecord *)D_801A7AD8[card_record].data;
    if (private_source(source, &which, 0) && member_side(which) == side &&
        (!require_slot ||
         normalized_deck_slot(card_record, source->deck_index) == deck_slot))
        return remember_source(card_record, source);
    source = mapped_source(card_record);
    if (private_source(source, &which, 0) && member_side(which) == side &&
        (!require_slot ||
         normalized_deck_slot(card_record, source->deck_index) == deck_slot))
        return source;
    return 0;
}

static DuelDeckCardRecord *source_for_setup(int card_record, int deck_slot)
{
    DuelDeckCardRecord *source;
    TagMember *m;
    int side = card_record >= DUEL_CARD_SIDE_RECORD_COUNT;
    int base_record = side * DUEL_CARD_SIDE_RECORD_COUNT;
    int record_in_side = card_record - base_record;
    int local = deck_slot - side * DECK_SIZE;
    int active, which, work;
    int phase = gDuel_wSceneStateFlags & DUEL_SCENE_PHASE_MASK;
    int placing = phase == 7 || phase == 8;

    source = fusion_source_for_setup(card_record, deck_slot);
    if (source) return source;

    if (local < 0 || local >= DECK_SIZE) return 0;
    active = member_index(side, tag->active_member[side]);

    /* Turn entry clears the five retail hand records and reconstructs them
     * from the incoming teammate's hand indexes. Never search the shared
     * field here: the outgoing teammate may have a live field card with the
     * same 0..39 deck position. */
    if (record_in_side >= 0 && record_in_side < HAND_SIZE) {
        m = &tag->member[active];
        /* The retail AI can swap deck records while choosing its hand. That
         * is the one legitimate reason for a hand slot to differ from our
         * private copy. Never import a stale mirror during a fusion rebuild
         * or when the card is already live elsewhere on the field. */
        if (ai_swapping &&
            gDuel_aDeckCardRecords[deck_slot].id != m->records[local].id) {
            int i, live = 0;
            for (i = side * DUEL_CARD_SIDE_RECORD_COUNT + HAND_SIZE;
                 i < (side + 1) * DUEL_CARD_SIDE_RECORD_COUNT; i++) {
                if ((D_801A7AD8[i].flags & DUEL_CARD_FLAG_OCCUPIED) &&
                    D_801A7AD8[i].data == (u8 *)&m->records[local]) {
                    live = 1;
                    break;
                }
            }
            if (!live) m->records[local] = gDuel_aDeckCardRecords[deck_slot];
        }
        return remember_source(card_record, &m->records[local]);
    }

    /* Battle (phase 9) calls func_80024D34 to rebuild both combatants. Its
     * work slots can include another teammate's card with the same deck
     * position. The already occupied record is authoritative there, and on
     * every other non-placement rebuild, for both sides of the field. */
    if (!placing) {
        source = occupied_source_for_setup(card_record, deck_slot, side, 0);
        if (source) return source;
    }

    /* Moving a hand/field object to another record leaves the exact source
     * in the duel work slots. Resolve the source record through the ownership
     * map as well, since fusion temporarily clears its occupied flag. */
    for (work = 0; work < 6; work++) {
        DisplayObject *object = D_800E9EF0[work];
        int source_record;
        if (!object) continue;
        source_record = object->field_6A;
        if (source_record < 0 || source_record >= DUEL_CARD_RECORD_COUNT)
            continue;
        source = (DuelDeckCardRecord *)D_801A7AD8[source_record].data;
        if (!private_source(source, &which, 0)) {
            source = mapped_source(source_record);
            if (!private_source(source, &which, 0)) continue;
        }
        /* Rebuilding the same shared-field record (battle/turn animation)
         * must retain its original teammate even while the other teammate is
         * taking the turn. A different source record is a new move and must
         * come from the active teammate. */
        if (member_side(which) == side &&
            (source_record == card_record || which == active) &&
            normalized_deck_slot(source_record, source->deck_index) == deck_slot)
            return remember_source(card_record, source);
    }

    /* Only placement lets a selected moving card replace an occupied slot. */
    if (placing) {
        source = occupied_source_for_setup(card_record, deck_slot, side, 1);
        if (source) return source;
    }

    /* Fusion and effect paths can clear a field record before rebuilding the
     * same card. Its explicit map survives that short gap. */
    source = mapped_source(card_record);
    if (source && private_source(source, &which, 0) &&
        member_side(which) == side &&
        normalized_deck_slot(card_record, source->deck_index) == deck_slot)
        return source;

    /* A missing transfer source must never replace a private card with the
     * shared mirror: that mirror changes owner at every teammate rotation.
     * Keep the active duelist's original card and record the unexpected
     * route for a reproducible gameplay trace. */
    m = &tag->member[active];
    tag_host->log(tag_host,
        "tag card source fallback: record %d deck %d active %d private %d mirror %d",
        card_record, deck_slot, active, m->records[local].id,
        gDuel_aDeckCardRecords[deck_slot].id);
    return remember_source(card_record, &m->records[local]);
}

static void save_member(int side)
{
    TagMember *m = &tag->member[member_index(side, tag->active_member[side])];
    /* Live card records point into m->records. Fusion and card effects write
     * there directly. Copying the retail deck mirror back here destroys
     * those writes whenever the mirror still contains a fusion material or
     * has since been loaded for another teammate. */
    memcpy(m->hand, D_800E9FF0[side].hand, HAND_SIZE);
    m->draw_cursor = D_800E9FF0[side].deck_draw_cursor;
}

static void audit_field_cards(void)
{
    int i;
    for (i = 0; i < DUEL_CARD_RECORD_COUNT; i++) {
        DuelCardRecord *card = &D_801A7AD8[i];
        DuelDeckCardRecord *source = (DuelDeckCardRecord *)card->data;
        unsigned anomaly = 0;
        int which = -1;
        if (i % DUEL_CARD_SIDE_RECORD_COUNT < HAND_SIZE ||
            !(card->flags & DUEL_CARD_FLAG_OCCUPIED)) {
            last_card_anomaly[i] = 0;
            continue;
        }
        if (!private_source(source, &which, 0)) anomaly = 1;
        else if (source->id != card->card_id) anomaly = 2;
        else if (member_side(which) != (i >= DUEL_CARD_SIDE_RECORD_COUNT))
            anomaly = 3;
        if (anomaly && anomaly != last_card_anomaly[i])
            tag_host->log(tag_host,
                "tag card anomaly: record %d kind %u owner %d shown %d source %d phase %d",
                i, anomaly, which, card->card_id,
                private_source(source, 0, 0) ? source->id : -1,
                gDuel_wSceneStateFlags & DUEL_SCENE_PHASE_MASK);
        last_card_anomaly[i] = anomaly;
    }
}

static void load_member(int side, int teammate)
{
    TagMember *m = &tag->member[member_index(side, teammate)];
    int base = side * DECK_SIZE;
    memcpy(&gDuel_aDeckCardRecords[base], m->records, sizeof(m->records));
    memcpy(&D_8015C424_cards.cards[base], m->records, sizeof(m->records));
    memcpy(D_8018C2D8 + base * TAG_THUMB_SIZE, m->thumbnails,
           sizeof(m->thumbnails));
    memcpy(D_800E9FF0[side].hand, m->hand, HAND_SIZE);
    D_800E9FF0[side].deck_draw_cursor = m->draw_cursor;
    tag->active_member[side] = teammate;

    if (side == 0) {
        D_8009B360[0] = teammate && tag->partner_control == TAG_CONTROL_AI
                            ? m->duelist : -1;
    } else {
        D_8009B360[1] = m->duelist;
    }
}

static void populate(void)
{
    s16 deck[DECK_SIZE];
    int ok = 1;
    ((void (*)(void))original_populate)();
    refresh_match();
    if (!tag) return;
    if (!tag->requested) {
        tag->active = 0;
        tag->status = TAG_STATUS_OFF;
        return;
    }

    capture_retail_member(TAG_PLAYER, 0);
    capture_retail_member(TAG_OPPONENT_1, 1);

    tag->error = TAG_ERROR_NONE;
    if (has_explicit_partner_deck) {
        shuffle_explicit((const s16 *)explicit_partner_deck, deck);
    } else if (tag->partner_deck_source == TAG_DECK_CUSTOM_SLOT) {
        s16 source[DECK_SIZE];
        ok = copy_deck_slot &&
             copy_deck_slot(tag->partner_deck_slot - 1,
                            (unsigned short *)source);
        if (ok) shuffle_explicit(source, deck);
    } else if (tag->partner_deck_source == TAG_DECK_CAMPAIGN) {
        shuffle_explicit((const s16 *)gDuel_awPlayerDeck, deck);
    } else {
        ok = deal_signature(tag->member[TAG_PARTNER].duelist, deck);
    }
    if (!ok) tag->error = TAG_ERROR_PARTNER_DECK;
    if (ok && !prepare_member(&tag->member[TAG_PARTNER], deck, 0)) {
        ok = 0;
        tag->error = TAG_ERROR_PARTNER_DATA;
    }
    if (ok && !deal_signature(tag->member[TAG_OPPONENT_2].duelist, deck)) {
        ok = 0;
        tag->error = TAG_ERROR_OPPONENT_DECK;
    }
    if (ok && !prepare_member(&tag->member[TAG_OPPONENT_2], deck, 1)) {
        ok = 0;
        tag->error = TAG_ERROR_OPPONENT_DATA;
    }
    if (ok && !read_opponent_drops(0)) {
        ok = 0;
        tag->error = TAG_ERROR_OPPONENT_1_DROPS;
    }
    if (ok && !read_opponent_drops(1)) {
        ok = 0;
        tag->error = TAG_ERROR_OPPONENT_2_DROPS;
    }

    if (!ok) {
        tag->status = TAG_STATUS_ERROR;
        tag->active = 0;
        tag_host->log(tag_host, "tag duel: setup failed at stage %d", tag->error);
        tag->requested = 0;
        return;
    }
    tag->active = 1;
    tag->status = TAG_STATUS_ACTIVE;
    tag->requested = 0;
    tag->active_member[0] = tag->active_member[1] = 0;
    memset(last_card_anomaly, 0, sizeof(last_card_anomaly));
    memset(tag->record_member, -1, sizeof(tag->record_member));
    memset(tag->record_local, -1, sizeof(tag->record_local));
    /* The starter's private deck must be live before the first draw. For
     * the waiting side, entries=1 with member=0 means its first entry will
     * switch to teammate 1. Subsequent turns keep the same four-seat ring. */
    tag->entries[0] = match_rules.start_side == 0 ||
                      match_rules.ally_starter;
    tag->entries[1] = match_rules.start_side == 1 ||
                      match_rules.enemy_starter;
    if (match_rules.start_side == 0 && match_rules.ally_starter)
        load_member(0, 1);
    if (match_rules.start_side == 1 && match_rules.enemy_starter)
        load_member(1, 1);
    displayed_member[0] = displayed_for_side(0, D_8009B1D5);
    displayed_member[1] = displayed_for_side(1, D_8009B1D5);
    previous_displayed_member[0] = displayed_member[0];
    previous_displayed_member[1] = displayed_member[1];
    transition_side = -1;
    turn_revision++;
    tag_host->log(tag_host,
        "tag duel started: partner %d, opponent 1 %d, opponent 2 %d, hard mode %d",
        tag->member[TAG_PARTNER].duelist,
        tag->member[TAG_OPPONENT_1].duelist,
        tag->member[TAG_OPPONENT_2].duelist, tag->hard_mode);
}

static int displayed_for_side(int side, int current_side)
{
    int teammate;
    if (side == current_side) teammate = tag->active_member[side];
    else teammate = tag->entries[side] ? tag->active_member[side] ^ 1 : 0;
    return member_index(side, teammate);
}

static void turn_switch(void)
{
    int before = D_8009B1D5;
    ((void (*)(void))original_turn_switch)();
    if (tag && tag->active && before != D_8009B1D5) {
        int side = D_8009B1D5;
        int i;
        int next;
        audit_field_cards();
        save_member(before);
        next = tag->entries[side] ? tag->active_member[side] ^ 1 : 0;
        tag->entries[side]++;
        load_member(side, next);
        transition_side = -1;
        for (i = 0; i < DUEL_SIDE_COUNT; i++) {
            int shown = displayed_for_side(i, side);
            previous_displayed_member[i] = displayed_member[i];
            if (shown != displayed_member[i]) transition_side = i;
            displayed_member[i] = shown;
        }
        transition_started = tag_host->now_us(tag_host);
        turn_revision++;
        /* Free Duel uses 47 when its portrait cursor arrives. Reusing it here
         * makes the teammate portrait swap feel native to that screen. */
        SD_SEPlayFull(47);
    }
}

static u8 *setup_card(s32 record, s32 deck_index)
{
    DuelDeckCardRecord saved_record, saved_staging, patched;
    u8 saved_thumbnail[TAG_THUMB_SIZE];
    DuelDeckCardRecord *source;
    u8 *result;
    int card_record, deck_slot, which, block;
    int battle_rebuild, old_card_id;
    DuelDeckCardRecord *old_source;
    if (!tag || !tag->active)
        return ((u8 *(*)(s32, s32))original_setup_card)(record, deck_index);

    card_record = normalized_card_record(record);
    deck_slot = normalized_deck_slot(card_record, deck_index);
    if (card_record < 0 || card_record >= DUEL_CARD_RECORD_COUNT ||
        deck_slot < 0 || deck_slot >= COMBINED_DECK_SIZE)
        return ((u8 *(*)(s32, s32))original_setup_card)(record, deck_index);
    battle_rebuild =
        (gDuel_wSceneStateFlags & DUEL_SCENE_PHASE_MASK) == 9 &&
        card_record % DUEL_CARD_SIDE_RECORD_COUNT >= HAND_SIZE &&
        (D_801A7AD8[card_record].flags & DUEL_CARD_FLAG_OCCUPIED);
    old_card_id = D_801A7AD8[card_record].card_id;
    old_source = (DuelDeckCardRecord *)D_801A7AD8[card_record].data;
    source = source_for_setup(card_record, deck_slot);
    /* A battle cutscene only redraws cards already on the field. Neither
     * combatant may acquire another teammate's same-numbered deck card. */
    if (battle_rebuild && private_source(old_source, &which, 0) &&
        member_side(which) == (card_record >= DUEL_CARD_SIDE_RECORD_COUNT) &&
        source != old_source) {
        tag_host->log(tag_host,
            "tag battle source corrected: record %d old %d proposed %d",
            card_record, old_card_id, source ? source->id : -1);
        source = remember_source(card_record, old_source);
    }
    if (!source || !private_source(source, &which, 0))
        return ((u8 *(*)(s32, s32))original_setup_card)(record, deck_index);
    block = source_thumbnail(which, source);
    if (block < 0)
        return ((u8 *(*)(s32, s32))original_setup_card)(record, deck_index);
    if (((gDuel_wSceneStateFlags & DUEL_SCENE_PHASE_MASK) == 7 ||
         (gDuel_wSceneStateFlags & DUEL_SCENE_PHASE_MASK) == 8) &&
        (D_8009B150 & 0x8000) &&
        (card_record == D_8009B19C ||
         (D_800E9EF0[0] && card_record == D_800E9EF0[0]->field_6A)))
        tag_host->log(tag_host,
            "tag fusion setup: record %d deck %d source %d card %d result %d",
            card_record, deck_slot, which, source->id,
            D_8009B150 & CARD_ID_FIELD_MASK);

    saved_record = gDuel_aDeckCardRecords[deck_slot];
    saved_staging = D_8015C424_cards.cards[deck_slot];
    memcpy(saved_thumbnail, D_8018C2D8 + deck_slot * TAG_THUMB_SIZE,
           TAG_THUMB_SIZE);
    patched = *source;
    patched.deck_index = (s8)deck_slot;
    patched.data_block_index = (u8)deck_slot;
    gDuel_aDeckCardRecords[deck_slot] = patched;
    D_8015C424_cards.cards[deck_slot] = patched;
    memcpy(D_8018C2D8 + deck_slot * TAG_THUMB_SIZE,
           tag->member[which].thumbnails[block], TAG_THUMB_SIZE);

    result = ((u8 *(*)(s32, s32))original_setup_card)(record, deck_index);

    gDuel_aDeckCardRecords[deck_slot] = saved_record;
    D_8015C424_cards.cards[deck_slot] = saved_staging;
    memcpy(D_8018C2D8 + deck_slot * TAG_THUMB_SIZE,
           saved_thumbnail, TAG_THUMB_SIZE);
    D_801A7AD8[card_record].data = (u8 *)source;
    /* Placement, fusion, trap and battle steps continue through the replay
     * snapshot after Duel_SetupCardRecord returns. Keep that snapshot bound
     * to the private source and the resulting card for the whole action. */
    D_8015C424_cards.field_cards[card_record] = D_801A7AD8[card_record];
    if (battle_rebuild && old_card_id != D_801A7AD8[card_record].card_id)
        tag_host->log(tag_host,
            "tag battle card changed: record %d side %d old %d new %d owner %d",
            card_record, card_record >= DUEL_CARD_SIDE_RECORD_COUNT,
            old_card_id, D_801A7AD8[card_record].card_id, which);
    return result;
}

static void ai_swap(void)
{
    int previous = ai_swapping;
    ai_swapping = 1;
    ((void (*)(void))original_ai_swap)();
    ai_swapping = previous;
}

static void side_input(void)
{
    int controller_two = tag && tag->active && D_8009B1D5 == 0 &&
        tag->active_member[0] == 1 &&
        tag->partner_control == TAG_CONTROL_PLAYER_TWO;
    if (controller_two) {
        Input_BackupPad1AndUsePad2();
        DuelScene_Update();
        Input_RestorePad1FromBackup();
    } else {
        ((void (*)(void))original_side_input)();
    }
}

static HmCard card_info(int id)
{
    HmCard c = {0};
    unsigned stats;
    if (!Cards_Valid(id)) return c;
    stats = (unsigned)gDuel_adwCardStats[id - 1];
    c.id = id;
    c.effect = Cards_EffectId(id);
    c.type = (stats >> CARD_STAT_TYPE_SHIFT) & CARD_STAT_TYPE_MASK;
    c.attack = (stats & CARD_STAT_VALUE_MASK) * CARD_STAT_SCALE;
    c.defense = ((stats >> CARD_STAT_DEFENSE_SHIFT) & CARD_STAT_VALUE_MASK) * CARD_STAT_SCALE;
    c.star = (stats >> CARD_STAT_GUARDIAN_STAR_1_SHIFT) & 15;
    c.star2 = (stats >> CARD_STAT_GUARDIAN_STAR_2_SHIFT) & 15;
    return c;
}

static int terrain_bonus(int type, int field)
{
    return type >= 0 && type < 20 && field >= 1 && field <= 6
        ? gDuel_aTerrainBoost[type][field - 1] * CARD_STAT_SCALE : 0;
}
static int ritual_card(int id) { return Duel_CheckRitual(0, id); }
static const HmRules tag_rules = {
    card_info, Duel_CheckFusion, Duel_CheckEquip, terrain_bonus, ritual_card
};

static HmCard active_card(int index, int hidden)
{
    AiActiveCard a = gDuel_aActiveCards[index];
    HmCard c = {0};
    if (!a.card_id) return c;
    if (hidden && (a.flags & HM_DOWN)) {
        c.id = -1;
        c.flags = a.flags;
        c.type = index >= 61 ? 21 : 0;
        return c;
    }
    c = card_info(a.card_id);
    c.flags = a.flags;
    c.attack = a.attack;
    c.defense = a.defense;
    c.star = a.guardian_star;
    return c;
}

static HmBoard partner_board(void)
{
    HmBoard b = {0};
    int i, hand = 0;
    b.terrain = gDuel_bTerrain;
    b.lp = D_800E9FF0[0].life_points.unsigned_value;
    b.max_lp = D_800E9FF0[0].max_life_points;
    b.enemy_lp = D_800E9FF0[1].life_points.unsigned_value;
    b.pinned = D_800E9FF0[0].swords_turns_remaining != 0;
    b.enemy_pinned = D_800E9FF0[1].swords_turns_remaining != 0;
    for (i = 0; i < HAND_SIZE; i++) hand += D_800E9FF0[0].hand[i] >= 0;
    b.hand_count = hand;
    for (i = 0; i < 5; i++) {
        b.own[i] = active_card(1 + i, 0);
        b.back[i] = active_card(6 + i, 0);
        b.enemy[i] = active_card(56 + i, 1);
        b.enemy_back[i] = active_card(61 + i, 1);
    }
    for (i = 0; i < hand; i++) b.hand[i] = card_info(gDuel_aActiveCards[11 + i].card_id);
    return b;
}

static s32 ai_run(void)
{
    refresh_match();
    HmBoard board;
    HmDecision decision;
    HmOptions options = {1, 1, 1, 1, 1, 1, 1, 4, 6000, 50, 150, 1};
    int hand_phase;
    /* Field-script result 3 ends the action phase. Gate before calling
     * any AI/planner hook, including other mods that skip retail's flag. */
    if (TagRules_OpeningAttacksBlocked(match_rules.active, match_rules.completed_turns) &&
        gAiScript_State.script_base == D_801A9800)
        return 3;
    if (tag && tag->active && D_8009B1D5 == 1) {
        int id = tag->member[member_index(1, tag->active_member[1])].duelist;
        s8 previous_id = gDuel_bOpponentID;
        s32 result;
        if (id < 1 || id >= AI_OPPONENT_COUNT)
            return ((s32 (*)(void))original_ai_run)();
        /* The retail AI reads one global opponent ID. Route the active rival
         * while its VM runs, then restore the result/drop context. Duel Options
         * owns the searchable draw-pool limit for both solo and tag battles. */
        gDuel_bOpponentID = (s8)id;
        result = ((s32 (*)(void))original_ai_run)();
        gDuel_bOpponentID = previous_id;
        return result;
    }
    if (!tag || !tag->active || D_8009B1D5 != 0 ||
        tag->active_member[0] != 1 ||
        tag->partner_control != TAG_CONTROL_AI)
        return ((s32 (*)(void))original_ai_run)();

    /* All indexes in this snapshot are relative to active side 0. Enemy
     * targets are therefore side 1; the partner cannot select its own team. */
    board = partner_board();
    hand_phase = gAiScript_State.script_base == D_801A8000;
    decision = hand_phase
        ? Hm_PlanHand(&board, &options, &tag_rules)
        : Hm_PlanField(&board, &options, &tag_rules, 0);
    if (!decision.result) return ((s32 (*)(void))original_ai_run)();
    memcpy(D_800EAE88, decision.selection, 12);
    return decision.result;
}

static void remember_reward(int id)
{
    int i;
    if (!Cards_Valid(id)) return;
    for (i = 0; i < tag->reward_count; i++) {
        if (tag->reward_ids[i] == id) {
            tag->reward_expected[i]++;
            return;
        }
    }
    if (tag->reward_count >= CARD_DROPS_MAX) return;
    i = tag->reward_count++;
    tag->reward_ids[i] = (s16)id;
    tag->reward_before[i] = *Cards_ChestSlot(gDuel_awPlayerDeck, id);
    tag->reward_expected[i] = 1;
}

static void snapshot_rewards(void)
{
    int i;
    tag->reward_count = 0;
    tag->reward_checked = 0;
    for (i = 0; i < gCardDrops.count; i++)
        remember_reward(gCardDrops.cards[i]);
    remember_reward(D_8009B1E8->dropped_card_id);
    tag_host->log(tag_host, "tag rewards dealt: %d total, %d distinct",
                  gCardDrops.count + (Cards_Valid(D_8009B1E8->dropped_card_id) != 0),
                  tag->reward_count);
}

static void verify_rewards(void)
{
    int i, repairs = 0;
    if (tag->reward_checked) return;
    tag->reward_checked = 1;
    for (i = 0; i < tag->reward_count; i++) {
        int id = tag->reward_ids[i];
        int target = tag->reward_before[i] + tag->reward_expected[i];
        u8 *quantity = Cards_ChestSlot(gDuel_awPlayerDeck, id);
        if (target > Tables_ChestLimit()) target = Tables_ChestLimit();
        while (*quantity < target && !Tables_ChestFull(*quantity)) {
            int before = *quantity;
            Duel_AwardCard(id);
            if (*quantity == before) break;
            repairs++;
        }
        if (*quantity < target)
            tag_host->log(tag_host,
                "tag reward shortfall: card %d expected chest %d, found %d",
                id, target, *quantity);
    }
    if (repairs)
        tag_host->log(tag_host, "tag reward: repaired %d missing chest copies",
                      repairs);
}

static void result_rewards(void)
{
    int opening = !(gDuel_wSceneStateFlags & 0x8000);
    int leaving = (gDuel_wSceneStateFlags & 0x6000) == 0x6000 &&
                  !(((FadeTransitionState *)D_800E9EC8_arr)->flags &
                    FADE_FLAG_ACTIVE);
    /* The retail reward path uses side 0's AI marker to distinguish a
     * player duel from CPU-versus-CPU. A partner AI may legitimately leave
     * that marker set at the end of a tag duel, but the team still belongs
     * to the loaded player and must receive its card and starchips. */
    if (tag && tag->active) {
        D_8009B360[0] = -1;
        /* The normal winner path must always use the loaded campaign state.
         * An absent window otherwise shows SPOILS but silently awards none. */
        if (!D_8009B1D8[0])
            D_8009B1D8[0] = (SaveDataState *)gDuel_awPlayerDeck;

    }
    ((void (*)(void))original_result_rewards)();
    if (tag && tag->active && gDuel_bWinnerSide == 0) {
        if (opening) snapshot_rewards();
        if (leaving) verify_rewards();
    }
}

int TagDuel_Start(int partner, int opponent_1, int opponent_2,
                  int partner_control, int partner_deck_source,
                  int partner_deck_slot, int hard_mode)
{
    int wa_lba;
    if (!tag) return 0;
    tag_host->log(tag_host, "tag start request: partner %d rivals %d/%d",
                  partner, opponent_1, opponent_2);
    wa_lba = tag_host->disc_file_start(tag_host, TAG_WA_PATH);
    if (wa_lba < 0) return 0;
    memset(tag, 0, sizeof(*tag));
    tag->wa_lba = wa_lba;
    tag->requested = 1;
    tag->status = TAG_STATUS_PENDING;
    tag->partner_control = partner_control;
    tag->partner_deck_source = partner_deck_source;
    tag->partner_deck_slot = partner_deck_slot;
    tag->hard_mode = hard_mode != 0;
    tag->member[TAG_PLAYER].duelist = -1;
    tag->member[TAG_PARTNER].duelist = (s8)partner;
    tag->member[TAG_OPPONENT_1].duelist = (s8)opponent_1;
    tag->member[TAG_OPPONENT_2].duelist = (s8)opponent_2;
    ai_swapping = 0;
    func_80024DC8(-1, opponent_1, 0, 0);
    /* Return through retail Free Duel's own post-duel path. Its grid keeps
     * the loaded workspace and lets the player immediately duel again. */
    gFreeDuel_bReturnFlags = 0x80;
    D_8009B368 = MAIN_MODE_FREE_DUEL;
    return 1;
}

unsigned TagDuel_HudSignature(void)
{
    unsigned signature;
    uint64_t elapsed;
    int side, i;
    if (!tag || tag->status == TAG_STATUS_OFF ||
        (D_8009B26C & 0x0f) != 3) return 0;
    signature = 17u + (unsigned)tag->status * 13u +
                (unsigned)tag->error * 31u + turn_revision * 131u;
    if (tag->active) {
        audit_field_cards();
        signature = signature * 31u + D_8009B1D5;
        signature = signature * 31u +
                    (gDuel_wSceneStateFlags & DUEL_SCENE_PHASE_MASK);
        for (side = 0; side < DUEL_SIDE_COUNT; side++) {
            signature = signature * 31u + displayed_member[side] + 1u;
            signature = signature * 31u + (u8)D_800E9FF0[side].deck_draw_cursor;
            for (i = 0; i < HAND_SIZE; i++)
                signature = signature * 31u + (u8)D_800E9FF0[side].hand[i];
        }
        elapsed = tag_host->now_us(tag_host) - transition_started;
        if (transition_side >= 0 && elapsed < TAG_TURN_EFFECT_US)
            signature = signature * 31u + (unsigned)(elapsed / 16000u) + 1u;
    }
    return signature ? signature : 1;
}

static const char *member_name(int member)
{
    static char player_name[16];
    const char *name;
    if (member == TAG_PLAYER) {
        SaveSlots_StateName((const unsigned char *)&
            ((SaveDataWorkspace *)D_801D0000)->state,
            player_name, sizeof(player_name));
        return player_name[0] ? player_name : "PLAYER";
    }
    name = Tables_DuelistShortName(tag->member[member].duelist);
    return name ? name : "DUELIST";
}

static const char *member_control(int member)
{
    if (member == TAG_PLAYER) return "PLAYER 1";
    if (member == TAG_OPPONENT_1 || member == TAG_OPPONENT_2) return "CPU";
    if (tag->partner_control == TAG_CONTROL_AI) return "PARTNER AI";
    if (tag->partner_control == TAG_CONTROL_PLAYER_TWO) return "PLAYER 2";
    return "PLAYER 1";
}

static void member_counts(int side, int member, int active, int *hand, int *deck)
{
    const TagMember *m = &tag->member[member];
    int i;
    *hand = 0;
    if (active) {
        for (i = 0; i < HAND_SIZE; i++) *hand += D_800E9FF0[side].hand[i] >= 0;
        *deck = DECK_SIZE - D_800E9FF0[side].deck_draw_cursor;
    } else {
        for (i = 0; i < HAND_SIZE; i++) *hand += m->hand[i] >= 0;
        *deck = DECK_SIZE - m->draw_cursor;
    }
    if (*deck < 0) *deck = 0;
}

/* Presentation adapter. Disabled Tag Duels never provides a stale match. */
static int participants(DuelParticipants *out, size_t size)
{
    int side; uint64_t elapsed;
    if (!out || size < sizeof(*out) || !tag || !tag->active ||
        !tag_host->applied(tag_host) || (D_8009B26C & 31) != MAIN_MODE_DUEL) return 0;
    memset(out, 0, sizeof(*out));
    out->abi = DUEL_PARTICIPANTS_ABI; out->size = sizeof(*out);
    out->active_side = D_8009B1D5; out->transition_side = transition_side;
    elapsed = tag_host->now_us(tag_host) - transition_started;
    out->transition_progress = elapsed >= TAG_TURN_EFFECT_US ? 255 :
        (unsigned)(elapsed * 255u / TAG_TURN_EFFECT_US);
    for (side = 0; side < 2; side++) {
        int member = displayed_member[side];
        DuelParticipant *p = &out->side[side];
        p->identity = member; p->portrait = tag->member[member].duelist;
        snprintf(p->name, sizeof(p->name), "%s", member_name(member));
        snprintf(p->control, sizeof(p->control), "%s", member_control(member));
        member_counts(side, member, side == D_8009B1D5, &p->hand, &p->deck);
        out->previous[side] = *p;
        member = previous_displayed_member[side];
        out->previous[side].identity = member;
        out->previous[side].portrait = tag->member[member].duelist;
    }
    return 1;
}






/* Same displayed identity as the portrait; never changes AI globals. */
static int hud_name(int side, char *out, size_t size)
{
    DuelParticipants view;
    if (!out || !size || side < 0 || side > 1 ||
        !participants(&view, sizeof(view))) return 0;
    snprintf(out, size, "%s", view.side[side].name);
    return out[0] != 0;
}

static const char *tag_error_text(void)
{
    switch (tag->error) {
    case TAG_ERROR_PARTNER_DECK: return "PARTNER DECK COULD NOT BE CREATED";
    case TAG_ERROR_PARTNER_DATA: return "PARTNER CARD DATA COULD NOT BE LOADED";
    case TAG_ERROR_OPPONENT_DECK: return "SECOND OPPONENT DECK COULD NOT BE CREATED";
    case TAG_ERROR_OPPONENT_DATA: return "SECOND OPPONENT CARD DATA COULD NOT BE LOADED";
    case TAG_ERROR_OPPONENT_1_DROPS: return "FIRST OPPONENT DROPS COULD NOT BE LOADED";
    case TAG_ERROR_OPPONENT_2_DROPS: return "SECOND OPPONENT DROPS COULD NOT BE LOADED";
    default: return "TAG DUEL COULD NOT START";
    }
}



void TagDuel_DrawHud(void)
{
    int width, height, scale;
    const char *error;
    if (!TagDuel_HudSignature()) return;
    tag_host->overlay_size(tag_host, &width, &height, &scale);
    if (width <= 0 || height <= 0) return;
    if (tag->status == TAG_STATUS_ERROR) {
        int text_width;
        error = tag_error_text();
        text_width = tag_host->text_width(tag_host, error, 1);
        tag_host->fill(tag_host, (width - text_width) / 2 - 14, 14,
                       text_width + 28, 34, 0x7b1820, 235);
        tag_host->draw_text(tag_host, (width - text_width) / 2, 31,
                            error, 0xffffff, 1);
        return;
    }
}


void TagDuel_ResetHud(void)
{
    int side;
    if (!tag || !tag->active) return;
    for (side = 0; side < DUEL_SIDE_COUNT; side++) {
        displayed_member[side] = displayed_for_side(side, D_8009B1D5);
        previous_displayed_member[side] = displayed_member[side];
    }
    transition_side = -1;
    transition_started = 0;
    turn_revision++;
}

static int rank_side(const DuelSideState *live, DuelSideState *out)
{
    refresh_match();
    int side, team, first, second;
    if (!tag_host || !tag_host->applied(tag_host) || !match_rules.active ||
        (D_8009B26C & 31) != MAIN_MODE_DUEL) return 0;
    if (live == &D_800E9FF0[0]) side = 0;
    else if (live == &D_800E9FF0[1]) side = 1;
    else return 0;
    team = tag && tag->active;
    first = second = live->deck_draw_cursor;
    if (team) {
        first = tag->member[member_index(side, 0)].draw_cursor;
        second = tag->member[member_index(side, 1)].draw_cursor;
        /* Stored state is updated on departure. Read the active hand's
         * current cursor so newly drawn cards enter the score immediately. */
        if (tag->active_member[side]) second = live->deck_draw_cursor;
        else first = live->deck_draw_cursor;
    }
    TagRank_Project(live, out, team, first, second);
    return 1;
}

/* Copy only public reward inputs; inventory and private hands stay internal. */
static int reward_context(TagRewardContext *out,size_t size)
{
    if(!tag || !tag->active || !tag_host->applied(tag_host) ||
       (D_8009B26C&31)!=MAIN_MODE_DUEL)return 0;
    if(!out)return size==0;
    if(size<sizeof(*out))return 0;
    memset(out,0,sizeof(*out));out->abi=1;out->size=sizeof(*out);
    out->duelist[0]=tag->member[TAG_OPPONENT_1].duelist;
    out->duelist[1]=tag->member[TAG_OPPONENT_2].duelist;
    memcpy(out->weights,tag->opponent_drops,sizeof(out->weights));return 1;
}

int TagDuel_RuntimeInit(const MemoriesModHost *host)
{
    tag_host = host;
    if (!host->symbol(host, "Rank_ProjectSide")) {
        host->log(host, "Tag Duels requires the matching engine with shared live/final rank projection support");
        return 0;
    }
    tag = calloc(1, sizeof(*tag));
    if (!tag) return 0;
    TagRewards_SetState(&tag->rewards);
    if (!host->register_state(host, tag, sizeof(*tag), 10)) return 0;
    match_service=host->find(host,"duel-options:match_v1");
    if(!match_service || match_service->abi!=1 || match_service->size!=sizeof(*match_service) || !match_service->query)return 0;
    tag->wa_lba = host->disc_file_start(host, TAG_WA_PATH);
    copy_deck_slot = (int (*)(int, unsigned short *))
        host->symbol(host, "DeckMenu_CopySlot");
    if (!host->provide(host, "rank_side_v1", (void *)rank_side) ||
        !host->provide(host, "participants_v1", (void *)participants) ||
        !host->provide(host, "hud_name_v1", (void *)hud_name) ||
        !host->provide(host,"reward_context_v1",(void *)reward_context)) return 0;
    return host->hook(host, (void *)Duel_PopulateCombinedDeckData,
                      (void *)populate, &original_populate) &&
           host->hook(host, (void *)DuelScene_UpdateTurnSwitch,
                      (void *)turn_switch, &original_turn_switch) &&
           host->hook(host, (void *)DuelScene_UpdateWithSideInput,
                      (void *)side_input, &original_side_input) &&
           host->hook(host, (void *)AiScript_Run,
                      (void *)ai_run, &original_ai_run) &&
           host->hook(host, (void *)Duel_SetupCardRecord,
                      (void *)setup_card, &original_setup_card) &&
           host->hook(host, (void *)func_8001BAF0,
                      (void *)ai_swap, &original_ai_swap) &&
           host->hook(host, (void *)DuelScene_UpdateResultRewards,
                      (void *)result_rewards, &original_result_rewards);
}

void TagDuel_RuntimeShutdown(void)
{
    free(tag);
    tag = 0;
}

