/* Free Duel-integrated Tag Duel setup. The retail portrait grid stays live:
 * Triangle opens the duel-type folio; Tag then collects three choices on the
 * original grid before presenting its rules. Duel Portraits draws the battle
 * portraits through a presentation snapshot supplied by the tag runtime. */
#include "tag_client.h"
#include "match_runtime.h"
#include "tag_overlay.h"
#include "option_registry.h"
#include "builtin_options.h"
#define MAIN_MODE_STATE_NEXT_AS_SCALAR
#define MAIN_MODE_STATE_ACTIVE_AS_SCALAR
#include "types.h"
#include "game/main_menu_selection.h"
#include "game/main_modes.h"
#include "game/main_mode_state.h"
#include "game/main_services.h"
#include "game/func_80024DC8.h"
#include "game/save_data.h"
#include "game/input.h"
#include "game/sound.h"
#include "game/duel_effect.h"
#include "game/duel_side_state.h"
#include "game/text_box_lifecycle.h"
#include "game/display_object_render_sprite_sheet.h"
#include "psyq/libgte.h"
#include "psyq/libgpu.h"
#include "overlays/main_menu/frontend.h"
#include "overlays/free_duel/free_duel.h"
#include "pc/cards/tables.h"
#include "pc/mods/modapi.h"
#include "pc/saves/deck_menu.h"
#include "pc/saves/save_slots.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "clash_art.h"

#define GRID_SIZE 40
#define PORTRAIT_SIZE 48
#define PORTRAIT_RECORD 0x980
#define PORTRAIT_IMAGE 0x900
#define PORTRAIT_SECTORS 48
#define PORTRAIT_SECTOR_OFFSET 0x1EAA
#define CAMPAIGN_PORTRAIT_SECTOR_OFFSET 0x1E6A
#define CAMPAIGN_PORTRAIT_SECTORS 30
#define YUGI_WAITING_PORTRAIT 8
#define DEMO_KAIBA 17
#define DEMO_TEANA_SECOND 20
#define DEMO_JOEY_SECOND 19
#define SE_MOVE 6
#define SE_CONFIRM 0x30
#define SE_BACK 8
#define SE_REFUSED 9
#define SE_PORTRAIT 47

/* These are sampled from the Free Duel portraits and PS1 menu palette:
 * warm gold trim, parchment text, and the very dark indigo/bronze ground. */
#define C_GOLD 0xD9AE54u
#define C_GOLD_LIGHT 0xFFE4A1u
#define C_PARCHMENT 0xF5E6C4u
#define C_MUTED 0xB9A884u
#define C_INK 0x100D20u
#define C_PANEL 0x242039u
#define C_SELECTED 0x59442Fu
#define C_TEAM 0xB9D7E8u
#define C_ENEMY 0xF0AF92u
#define C_CHOICE_DIM 0xA0A0A0u

enum MenuPage { PAGE_GRID, PAGE_MODE, PAGE_RULES, PAGE_RECIPES, PAGE_PLAYER_DECK, PAGE_CLASH, PAGE_CONFIRM, PAGE_EXTENSIONS };
enum PickStep { PICK_PARTNER, PICK_OPPONENT_ONE, PICK_OPPONENT_TWO, PICK_DONE };
enum RuleRow {
    RULE_CONTROL, RULE_DECK, RULE_RECIPE, RULE_DRAW_POOL,
    RULE_LIFE, RULE_FIELD, RULE_OPENING, RULE_CONTINUE,
    RULE_COUNT
};
enum PartnerControl { CONTROL_AI, CONTROL_PLAYER_ONE, CONTROL_PLAYER_TWO, CONTROL_COUNT };
enum DeckSource { DECK_SIGNATURE, DECK_EDIT, DECK_RECIPE, DECK_COUNT };
enum EditFlow { EDIT_IDLE, EDIT_WAIT_LIST, EDIT_PICKING, EDIT_BUILDING, EDIT_RETURN };

static const char *const control_names[] = {
    "PARTNER AI", "PLAYER 1", "PLAYER 2"
};
static const char *const controller_labels[] = {
    "CPU", "PLAYER 1", "PLAYER 2"
};
static const char *const deck_names[] = {
    "SIGNATURE", "EDIT DECK", "DECK RECIPE"
};

TagDrawPixels TagOverlay_DrawPixels;
static const MemoriesModHost *host;
static void *original_free_duel;
static enum MenuPage page;
static int tag_mode;
static int pick_step;
static int opponent_1;
static int partner;
static int opponent_2;
static int rule_row;
static int partner_control;
static int partner_deck_source;
static int partner_deck_slot;
static int hard_mode;
static int starting_life;
static int starting_field;
static int opening_clash;
static int rule_scroll;
static int solo_opponent;
static int clash_order[10];
static int clash_cursor;
static int clash_player_card;
static int clash_enemy_card;
static int clash_enemy_cursor;
static int clash_phase;
static int clash_winner;
static int clash_first_side;
static int clash_ally_starter;
static int clash_enemy_starter;
static int clash_cpu_step;
static int clash_flip_sounded;
static int clash_scan_step;
static uint64_t clash_phase_at;
static int clash_card_size;
static const int clash_x[10] = {0, 68, 110, 110, 68, 0, -68, -110, -110, -68};
static const int clash_y[10] = {-116, -94, -36, 36, 94, 116, 94, 36, -36, -94};
static const char *const clash_star_names[10] = {
    "MARS", "JUPITER", "SATURN", "URANUS", "PLUTO", "NEPTUNE",
    "MERCURY", "SUN", "MOON", "VENUS"
};
/* Each line describes the directed advantage in the game's Guardian cycle. */
static const char *const clash_advantage_lines[10] = {
    "MARS TOPPLES JUPITER!",
    "JUPITER'S STORM BATTERS SATURN!",
    "SATURN'S RINGS BIND URANUS!",
    "URANUS' SKY BURIES PLUTO!",
    "PLUTO'S DEPTHS SWALLOW NEPTUNE!",
    "NEPTUNE'S WATERS DOUSE MARS!",
    "MERCURY OUTRUNS THE SUN!",
    "SUN OUTSHINES THE MOON!",
    "MOON OCCULTS VENUS!",
    "VENUS ENSNARES MERCURY!"
};
static enum EditFlow edit_flow;
static int edit_menu_stable_frames;
static int edit_original_slot;
static int edit_deck_seen;
static int resume_rules;
static int recipe_cursor;
static int recipe_for_player;
static int partner_edited;
static int partner_recipe_tab;
static int recipe_tab, recipe_save, editing_player_recipe;
static int ask_kind; /* 1 save draft, 2 discard, 3 shortage, 4 overwrite */
static unsigned short partner_draft[40], edit_draft[40], selected_recipe[40];
static int rematch_pending;
static char draft_name[64];
static void update_confirm(unsigned pressed);
static void begin_opening(void);
static void (*request_edit_slot)(int);
static int (*slot_info)(int, char *, size_t);
static int (*active_slot)(void);
static int (*equip_slot)(int);
static unsigned revision;
static u8 *portrait_bank;
static uint32_t portrait_pixels[3][PORTRAIT_SIZE * PORTRAIT_SIZE];
static int portrait_ids[3] = {-1, -1, -1};
static uint32_t player_portrait[PORTRAIT_SIZE * PORTRAIT_SIZE];
static int player_portrait_ready;
static uint32_t button_icons[3][16 * 16];
static uint32_t clash_glow_pixels[80 * 80];
static int button_icons_ready;
static uint32_t stone_texture[128 * 128];
static int stone_ready;
static uint32_t scroll_pixels[128 * 128];
static int scroll_w, scroll_h;
static uint32_t options_top[64 * 8];
static uint32_t options_bottom[64 * 8];
static uint32_t options_side[16 * 64];
static int options_trim_ready;
/* The same 8x12 glyphs and seven colour ramps used by retail text boxes.
 * Both are decoded from the boot package on the player's own disc. */
static uint32_t game_glyphs[7][95][8 * 12];
static int game_font_ready;

int TagDuel_RuntimeInit(const MemoriesModHost *);
void TagDuel_RuntimeShutdown(void);
void FreeDuel_PlaceCursor(DisplayObject *, s32);

static int mini(int a, int b) { return a < b ? a : b; }
static int maxi(int a, int b) { return a > b ? a : b; }
static uint32_t ps1_colour(unsigned word);

static const char *duelist_name(int id)
{
    const char *name = Tables_DuelistShortName(id);
    return name ? name : "UNKNOWN";
}

static void migrate_choices(void)
{
    int (*get)(const char *,int)=host->symbol(host,"Settings_GetNamed");
    static const char *keys[]={"opponent_1","opponent_2","partner","partner_control","partner_deck_choice","partner_deck_slot","partner_recipe_tab","hard_mode","starting_life","starting_field","opening_clash"};
    if(!get || host->setting(host,"choices_migrated",0))return;
    for(unsigned i=0;i<sizeof(keys)/sizeof(*keys);i++) {
        char previous[96];snprintf(previous,sizeof(previous),"mod.tag-duel-menu.%s",keys[i]);
        int old=get(previous,-2147483647);
        if(old!=-2147483647 && host->setting(host,keys[i],-2147483647)==-2147483647)
            host->set_setting(host,keys[i],old);
    }
    host->set_setting(host,"choices_migrated",1);
}

static void load_choices(void)
{
    opponent_1 = host->setting(host, "opponent_1", 9);
    opponent_2 = host->setting(host, "opponent_2", 10);
    partner = host->setting(host, "partner", 3);
    partner_control = host->setting(host, "partner_control", CONTROL_AI);
    partner_deck_source = host->setting(host, "partner_deck_choice", DECK_SIGNATURE);
    partner_deck_slot = host->setting(host, "partner_deck_slot", 1);
    hard_mode = host->setting(host, "hard_mode", 1) != 0;
    starting_life = host->setting(host, "starting_life", 8000);
    starting_field = host->setting(host, "starting_field", 0);
    opening_clash = host->setting(host, "opening_clash", 1) != 0;
    if (starting_life < 4000 || starting_life > 20000 || starting_life % 4000)
        starting_life = 8000;
    if (starting_field < 0 || starting_field > 8) starting_field = 0;
    if (opponent_1 < 1 || opponent_1 >= GRID_SIZE) opponent_1 = 9;
    if (opponent_2 < 1 || opponent_2 >= GRID_SIZE) opponent_2 = 10;
    if (partner < 1 || partner >= GRID_SIZE) partner = 3;
    if (partner_control < 0 || partner_control >= CONTROL_COUNT) partner_control = CONTROL_AI;
    if (partner_deck_source < 0 || partner_deck_source >= DECK_COUNT) partner_deck_source = DECK_SIGNATURE;
    if (partner_deck_slot < 1 || partner_deck_slot > 10) partner_deck_slot = 1;
    partner_recipe_tab = host->setting(host, "partner_recipe_tab", 0) != 0;
    partner_edited = 0;
    if (partner_deck_source == DECK_RECIPE)
        partner_edited = partner_recipe_tab
            ? DeckMenu_CopyPartnerRecipe(partner_deck_slot - 1, partner_draft)
            : DeckMenu_CopySlot(partner_deck_slot - 1, partner_draft);
}

static void save_choices(void)
{
    host->set_setting(host, "opponent_1", opponent_1);
    host->set_setting(host, "partner", partner);
    host->set_setting(host, "opponent_2", opponent_2);
    host->set_setting(host, "partner_control", partner_control);
    host->set_setting(host, "partner_deck_choice", partner_deck_source);
    host->set_setting(host, "partner_deck_slot", partner_deck_slot);
    host->set_setting(host, "partner_recipe_tab", partner_recipe_tab);
    host->set_setting(host, "hard_mode", hard_mode);
    host->set_setting(host, "starting_life", starting_life);
    host->set_setting(host, "starting_field", starting_field);
    host->set_setting(host, "opening_clash", opening_clash);
}

static int selected_grid_id(void)
{
    return gFreeDuel_bCursorRow * 5 + gFreeDuel_bCursorColumn;
}

static int grid_ready(void)
{
    int id = selected_grid_id();
    return !(gFreeDuel_bScreenFlags & 0x60) &&
           gFreeDuel_bCursorRow == gFreeDuel_bTargetRow &&
           gFreeDuel_bCursorColumn == gFreeDuel_bTargetColumn &&
           id > 0 && id < GRID_SIZE && gFreeDuel_abGridAvailable[id];
}

static void finish_native_intro(void)
{
    if (!(gFreeDuel_bScreenFlags & 0x20)) return;
    /* FreeDuel_UpdateScreen performs this exact cleanup when its retail
     * SELECT OPPONENT textbox completes. Tag now supplies its own changing
     * instruction, so retire the old textbox as soon as Tag is chosen. */
    TextBox_Destroy(&D_800EB15C);
    gFreeDuel_bScreenFlags &= ~0x20;
    gFreeDuel_pCursorWidget->flags |= DISPLAY_OBJECT_FLAG_RENDERABLE;
    FreeDuel_PlaceCursor(gFreeDuel_pCursorWidget, 1);
}

static void load_portrait_bank(void)
{
    int wa;
    if (portrait_bank) return;
    wa = host->disc_file_start(host, "\\DATA\\WA_MRG.MRG;1");
    if (wa < 0) return;
    portrait_bank = malloc(PORTRAIT_SECTORS * 2048u);
    if (!portrait_bank) return;
    if (host->disc_read(host, wa + PORTRAIT_SECTOR_OFFSET,
                        PORTRAIT_SECTORS, portrait_bank) != PORTRAIT_SECTORS) {
        free(portrait_bank);
        portrait_bank = 0;
    }
}

static uint32_t ps1_colour(unsigned word)
{
    unsigned r = word & 31, g = word >> 5 & 31, b = word >> 10 & 31;
    if (!word) return 0;
    return 0xff000000u | ((r << 3 | r >> 2) << 16) |
           ((g << 3 | g >> 2) << 8) | (b << 3 | b >> 2);
}

static int font_cell(int character, int *u, int *v)
{
    static const char punctuation[] = "!\"#$%&'()*+,-./:;<=>?";
    const char *mark;
    unsigned sjis;
    if (character >= '0' && character <= '9') sjis = 0x824F + character - '0';
    else if (character >= 'A' && character <= 'Z') sjis = 0x8260 + character - 'A';
    else if (character >= 'a' && character <= 'z') sjis = 0x8281 + character - 'a';
    else {
        mark = strchr(punctuation, character);
        if (!mark) return 0;
        sjis = 0;
        *u = (int)(mark - punctuation) * 8;
        *v = 0;
        if (*u >= 120) { *u -= 48; *v = 12; }
    }
    if (sjis) {
        *u = (sjis & 15) * 8;
        *v = ((sjis - 0x8240) >> 4) * 12;
    }
    return 1;
}

static void load_game_font(void)
{
    u8 *page, *ramps;
    int wa, colour, letter, x, y, u, v;
    const u16 *palette;
    if (game_font_ready) return;
    wa = host->disc_file_start(host, "\\DATA\\WA_MRG.MRG;1");
    if (wa < 0) return;
    page = malloc(16u * 2048u);
    ramps = malloc(2048u);
    if (!page || !ramps) { free(page); free(ramps); return; }
    if (host->disc_read(host, wa + 0x1690, 16, page) != 16 ||
        host->disc_read(host, wa + 0x1690 + 50, 1, ramps) != 1) {
        free(page); free(ramps); return;
    }
    for (colour = 0; colour < 7; colour++) {
        palette = (const u16 *)(ramps + colour * 32);
        for (letter = 32; letter <= 126; letter++) {
            uint32_t *glyph = game_glyphs[colour][letter - 32];
            if (!font_cell(letter, &u, &v)) continue;
            for (y = 0; y < 12; y++) for (x = 0; x < 8; x++) {
                int pixel = (v + y) * 256 + u + x;
                unsigned index = (page[pixel / 2] >> ((pixel & 1) * 4)) & 15;
                glyph[y * 8 + x] = ps1_colour(palette[index]);
            }
        }
    }
    game_font_ready = 1;
    free(page); free(ramps);
}

static int font_colour(uint32_t rgb)
{
    if (rgb == C_GOLD || rgb == C_GOLD_LIGHT) return 1;
    if (rgb == C_TEAM) return 2;
    if (rgb == C_ENEMY) return 5;
    if (rgb == C_MUTED) return 4;
    return 0;
}

static int ui_text_width(const char *string, int scale)
{
    return game_font_ready ? (int)strlen(string) * 8 * scale :
           host->text_width(host, string, scale);
}

static void ui_text(int x, int middle, const char *string,
                    uint32_t rgb, int scale)
{
    int colour, i;
    uint32_t dim_glyph[8 * 12];
    if (!game_font_ready) {
        host->draw_text(host, x, middle, string, rgb, scale);
        return;
    }
    colour = font_colour(rgb);
    for (i = 0; string[i]; i++) {
        int character = (unsigned char)string[i];
        const uint32_t *glyph;
        if (character < 32 || character > 126) character = '?';
        glyph = game_glyphs[colour][character - 32];
        if (rgb == C_CHOICE_DIM) {
            int pixel;
            for (pixel = 0; pixel < 8 * 12; pixel++) {
                uint32_t c = glyph[pixel];
                dim_glyph[pixel] = (c & 0xFF000000u) |
                    ((((c >> 16) & 255u) * 3u / 5u) << 16) |
                    ((((c >> 8) & 255u) * 3u / 5u) << 8) |
                    ((c & 255u) * 3u / 5u);
            }
            glyph = dim_glyph;
        }
        if (character != ' ')
            TagOverlay_DrawPixels(host, x + i * 8 * scale, middle - 6 * scale,
                              8 * scale, 12 * scale,
                              glyph, 8, 12, 255, 255);
    }
}

static void load_stone_texture(void)
{
    u8 *sheet, *palette_data;
    const u16 *palette;
    int wa, x, y;
    if (stone_ready) return;
    wa = host->disc_file_start(host, "\\DATA\\WA_MRG.MRG;1");
    if (wa < 0) return;
    sheet = malloc(32 * 2048u);
    palette_data = malloc(2048u);
    if (!sheet || !palette_data) {
        free(sheet);
        free(palette_data);
        return;
    }
    if (host->disc_read(host, wa + 0x1E88, 32, sheet) == 32 &&
        host->disc_read(host, wa + 0x1EA8, 1, palette_data) == 1) {
        /* One continuous 64x64 dark stone patch at x=0, y=120 of the
         * Free Duel 8-bit sheet. Other sheet regions contain title letters,
         * scrollbars and portrait ornaments, and must not be tiled. */
        palette = (const u16 *)palette_data;
        for (y = 0; y < 128; y++)
            for (x = 0; x < 128; x++) {
                int source_x = x < 64 ? x : 127 - x;
                int source_y = y < 64 ? y : 127 - y;
                stone_texture[y * 128 + x] =
                    ps1_colour(palette[sheet[(source_y + 120) * 128 + source_x]]);
            }
        stone_ready = 1;
    }
    free(sheet);
    free(palette_data);
}

/* Reconstruct the resolved Free Duel thumb from its sprite-sheet record and
 * live PS1 texture page. This uses the same cell, palette and page offsets
 * as the native renderer, rather than cropping an atlas by appearance. */
static void load_scroll_thumb(void)
{
    DisplayObject **slot = host->symbol(host, "gFreeDuel_pThumbWidget");
    DisplayObject *obj;
    SpriteSheetHeader *hdr;
    SpriteSheetPart *part;
    u16 palette[256], texels[128 * 128];
    RECT rect;
    int wide, page, cx, cy, u, v, x, y, words;
    if (scroll_w || !slot || !(obj = *slot) || !obj->field_4C) return;
    hdr = (SpriteSheetHeader *)obj->field_4C;
    if (hdr->count != 1) return;
    part = (SpriteSheetPart *)(hdr + 1);
    wide = (obj->attribute & DISPLAY_OBJECT_ATTRIBUTE_8BPP) != 0;
    page = obj->field_66 + hdr->tpage * (wide ? 2 : 1);
    cx = obj->field_40.h.field_40; cy = obj->field_40.h.field_42;
    if (obj->flags & DISPLAY_OBJECT_FLAG_TEXTURE_CELL_OFFSET) {
        cx += (hdr->clut & 15) << 4; cy += hdr->clut >> 4;
    }
    if (hdr->flags & 0xe0) {
        page += wide ? ((part->cell >> 9) & 14) : ((part->cell >> 10) & 7);
        if (wide) cy += part->size & 31;
        else { int step = (cx & 255) + ((part->size & 15) << 4);
            cx = (cx & 768) | (step & 255); cy += step >> 8; }
    }
    u = (part->cell & 31) << 3; v = (part->cell & 0x3e0) >> 2;
    if (obj->flags & DISPLAY_OBJECT_FLAG_TEXTURE_CELL_OFFSET) {
        u += ((u8 *)&obj->field_5E)[0]; v += ((u8 *)&obj->field_5E)[1];
    }
    page += u >> 8; u &= 255; v &= 255;
    scroll_w = ((part->size >> 2) & 120) + 8;
    scroll_h = ((part->size >> 6) & 120) + 8;
    words = scroll_w / (wide ? 2 : 4);
    rect.x = cx; rect.y = cy; rect.w = wide ? 256 : 16; rect.h = 1;
    StoreImage(&rect, (u32 *)palette);
    rect.x = (page & 15) * 64 + u / (wide ? 2 : 4);
    rect.y = ((page >> 4) & 1) * 256 + v;
    rect.w = words; rect.h = scroll_h;
    StoreImage(&rect, (u32 *)texels);
    for (y = 0; y < scroll_h; y++) for (x = 0; x < scroll_w; x++) {
        int bits = wide ? 8 : 4;
        int index = (texels[y * words + x / (16 / bits)] >>
                     ((x % (16 / bits)) * bits)) & (wide ? 255 : 15);
        scroll_pixels[y * scroll_w + x] = ps1_colour(palette[index]);
    }
}

static void draw_scroll_thumb(int x, int y, int w, int h)
{
    if (!scroll_w) load_scroll_thumb();
    if (!scroll_w) return;
    /* Caps retain their shape; only the patterned centre changes height. */
    int cap = mini(4, h / 3);
    TagOverlay_DrawPixels(host, x, y, w, cap, scroll_pixels, scroll_w, 4, 255, 255);
    if (h > 2 * cap) TagOverlay_DrawPixels(host, x, y + cap, w, h - 2 * cap,
        scroll_pixels + 4 * scroll_w, scroll_w, scroll_h - 8, 255, 255);
    TagOverlay_DrawPixels(host, x, y + h - cap, w, cap,
        scroll_pixels + (scroll_h - 4) * scroll_w, scroll_w, 4, 255, 255);
}

static void load_player_portrait(void)
{
    u8 *bank;
    const u8 *record;
    const u16 *palette;
    int wa, i;
    if (player_portrait_ready) return;
    wa = host->disc_file_start(host, "\\DATA\\WA_MRG.MRG;1");
    if (wa < 0) return;
    bank = malloc(CAMPAIGN_PORTRAIT_SECTORS * 2048u);
    if (!bank) return;
    if (host->disc_read(host, wa + CAMPAIGN_PORTRAIT_SECTOR_OFFSET,
                        CAMPAIGN_PORTRAIT_SECTORS, bank) ==
        CAMPAIGN_PORTRAIT_SECTORS) {
        record = bank + YUGI_WAITING_PORTRAIT * PORTRAIT_RECORD;
        palette = (const u16 *)(record + PORTRAIT_IMAGE);
        for (i = 0; i < PORTRAIT_SIZE * PORTRAIT_SIZE; i++)
            player_portrait[i] = ps1_colour(palette[record[i] & 63]);
        player_portrait_ready = 1;
    }
    free(bank);
}

static void load_button_icons(void)
{
    static const char *const marks[3][7] = {
        {"X.....X", ".X...X.", "..X.X..", "...X...", "..X.X..", ".X...X.", "X.....X"},
        {"..XXX..", ".X...X.", "X.....X", "X.....X", "X.....X", ".X...X.", "..XXX.."},
        {"...X...", "..X.X..", "..X.X..", ".X...X.", ".X...X.", "X.....X", "XXXXXXX"}
    };
    static const uint32_t inks[3] = {0xFF78A9EFu, 0xFFF17B84u, 0xFF83C99Bu};
    int icon, x, y;
    if (button_icons_ready) return;
    for (icon = 0; icon < 3; icon++)
        for (y = 0; y < 16; y++) for (x = 0; x < 16; x++) {
            int dx = 2 * x - 15, dy = 2 * y - 15;
            int distance = dx * dx + dy * dy;
            uint32_t pixel = 0;
            if (distance <= 225) pixel = distance >= 169
                ? 0xFFB2A8A2u : 0xFF242030u;
            if (x >= 4 && x < 11 && y >= 4 && y < 11 &&
                marks[icon][y - 4][x - 4] == 'X') pixel = inks[icon];
            button_icons[icon][y * 16 + x] = pixel;
        }
    button_icons_ready = 1;
}

static const char *player_save_name(void)
{
    static char name[16];
    SaveSlots_StateName((const unsigned char *)&
        ((SaveDataWorkspace *)D_801D0000)->state, name, sizeof(name));
    return name[0] ? name : "PLAYER";
}

static void load_options_trim(void)
{
    u8 *sheet, *palette_data;
    const u16 *palette;
    int wa, x, y;
    if (options_trim_ready) return;
    wa = host->disc_file_start(host, "\\DATA\\WA_MRG.MRG;1");
    if (wa < 0) return;
    sheet = malloc(16u * 2048u);
    palette_data = malloc(2048u);
    if (!sheet || !palette_data) { free(sheet); free(palette_data); return; }
    if (host->disc_read(host, wa + 8469, 16, sheet) == 16 &&
        host->disc_read(host, wa + 8501, 1, palette_data) == 1) {
        palette = (const u16 *)palette_data;
        for (y = 0; y < 8; y++) for (x = 0; x < 64; x++) {
            int top = y * 256 + x + 40;
            int bottom = (y + 128) * 256 + x + 40;
            options_top[y * 64 + x] =
                ps1_colour(palette[sheet[top / 2] >> ((top & 1) * 4) & 15]);
            options_bottom[y * 64 + x] =
                ps1_colour(palette[sheet[bottom / 2] >> ((bottom & 1) * 4) & 15]);
        }
        for (y = 0; y < 64; y++) for (x = 0; x < 16; x++) {
            int pixel = (y + 40) * 256 + x + 104;
            options_side[y * 16 + x] =
                ps1_colour(palette[sheet[pixel / 2] >> ((pixel & 1) * 4) & 15]);
        }
        options_trim_ready = 1;
    }
    free(sheet); free(palette_data);
}

static void portrait(int slot, int id, int x, int y, int size, unsigned brightness)
{
    const u8 *record;
    const u16 *palette;
    int i;
    if (!portrait_bank || id < 0 || id >= GRID_SIZE) return;
    if (portrait_ids[slot] != id) {
        record = portrait_bank + id * PORTRAIT_RECORD;
        palette = (const u16 *)(record + PORTRAIT_IMAGE);
        for (i = 0; i < PORTRAIT_SIZE * PORTRAIT_SIZE; i++)
            portrait_pixels[slot][i] = ps1_colour(palette[record[i] & 63]);
        portrait_ids[slot] = id;
    }
    TagOverlay_DrawPixels(host, x, y, size, size, portrait_pixels[slot],
                      PORTRAIT_SIZE, PORTRAIT_SIZE, brightness, 255);
}

static void draw_player_portrait(int x, int y, int size)
{
    if (player_portrait_ready)
        TagOverlay_DrawPixels(host, x, y, size, size, player_portrait,
                          PORTRAIT_SIZE, PORTRAIT_SIZE, 255, 255);
}

static void text_center(int x, int w, int y, const char *s, uint32_t colour, int scale)
{
    ui_text(x + (w - ui_text_width(s, scale)) / 2,
                    y, s, colour, scale);
}

static void text_center_fitted(int x, int w, int y, const char *s,
                               uint32_t colour, int scale)
{
    char fitted[64];
    int count = (w - 18 * scale) / (8 * scale);
    int length = (int)strlen(s);
    if (count < 3) return;
    if (count >= (int)sizeof(fitted)) count = sizeof(fitted) - 1;
    if (length > count) {
        memcpy(fitted, s, count - 2);
        fitted[count - 2] = '.';
        fitted[count - 1] = '.';
        fitted[count] = 0;
        s = fitted;
    }
    text_center(x, w, y, s, colour, scale);
}

/* Inline button images keep the action labels short and recognizable on a
 * controller. {X}, {O}, and {T} are Cross, Circle, and Triangle sprites. */
static int prompt_icon(const char *s)
{
    if (s[0] != '{' || !s[1] || s[2] != '}') return -1;
    if (s[1] == 'X') return 0;
    if (s[1] == 'O') return 1;
    if (s[1] == 'T') return 2;
    return -1;
}

static int prompt_width(const char *s, int scale)
{
    int width = 0;
    while (*s) {
        if (prompt_icon(s) >= 0) { width += 16 * scale; s += 3; }
        else { width += 8 * scale; s++; }
    }
    return width;
}

static void prompt_center(int x, int w, int middle, const char *s,
                          uint32_t colour, int scale)
{
    char word[96];
    int position = x + (w - prompt_width(s, scale)) / 2;
    while (*s) {
        int icon = prompt_icon(s);
        int letters = 0;
        if (icon >= 0) {
            TagOverlay_DrawPixels(host, position, middle - 8 * scale,
                              16 * scale, 16 * scale, button_icons[icon],
                              16, 16, 255, 255);
            position += 16 * scale;
            s += 3;
            continue;
        }
        while (s[letters] && prompt_icon(s + letters) < 0 &&
               letters < (int)sizeof(word) - 1) letters++;
        memcpy(word, s, letters);
        word[letters] = 0;
        ui_text(position, middle, word, colour, scale);
        position += letters * 8 * scale;
        s += letters;
    }
}

static void gilded_panel(int x, int y, int w, int h, int scale)
{
    int edge = maxi(7, scale * 7);
    int tx, ty;
    host->fill(host, x - scale, y - scale, w + scale * 2,
               h + scale * 2, C_INK, 240);
    host->fill(host, x, y, w, h, C_PANEL, 255);
    /* Retail dialogue panels place text on black inside their stone rim. */
    host->fill(host, x + edge, y + edge, w - edge * 2,
               h - edge * 2, 0x08070Du, 255);
    if (options_trim_ready) {
        for (tx = x; tx < x + w; tx += 64 * scale) {
            int span = mini(64 * scale, x + w - tx);
            TagOverlay_DrawPixels(host, tx, y, span, edge, options_top,
                              64, 8, 255, 255);
            TagOverlay_DrawPixels(host, tx, y + h - edge, span, edge,
                              options_bottom, 64, 8, 255, 255);
        }
        for (ty = y + edge; ty < y + h - edge; ty += 64 * scale) {
            int span = mini(64 * scale, y + h - edge - ty);
            TagOverlay_DrawPixels(host, x, ty, edge, span, options_side,
                              16, 64, 255, 255);
            TagOverlay_DrawPixels(host, x + w - edge, ty, edge, span,
                              options_side, 16, 64, 255, 255);
        }
    }
    host->fill(host, x + edge, y + edge, w - edge * 2,
               scale, C_MUTED, 160);
    host->fill(host, x + edge, y + h - edge - scale,
               w - edge * 2, scale, C_MUTED, 160);
}

static void selected_bar(int x, int y, int w, int h, int selected, int scale)
{
    if (selected) {
        host->fill(host, x, y, w, h, C_MUTED, 240);
        host->fill(host, x + 2 * scale, y + 2 * scale,
                   w - 4 * scale, h - 4 * scale, 0x25213Du, 255);
    } else {
        host->fill(host, x, y, w, h, C_INK, 120);
    }
}

static void draw_mode_panel(int cw, int ch, int scale)
{
    int w = mini(490 * scale, cw - 24 * scale);
    int h = mini(276 * scale, ch - 24 * scale);
    int x = (cw - w) / 2, y = (ch - h) / 2;
    int inner = w - 44 * scale, row = 64 * scale;
    host->fill(host, 0, 0, cw, ch, C_INK, 68);
    gilded_panel(x, y, w, h, scale);
    if (cw < 620 || ch < 420) {
        int small_row = 54 * scale;
        text_center(x, w, y + 21 * scale, "DUEL TYPE", C_GOLD_LIGHT, scale);
        selected_bar(x + 10 * scale, y + 38 * scale, w - 20 * scale,
                     small_row, !tag_mode, scale);
        ui_text(x + 20 * scale, y + 57 * scale,
                        "SINGLE DUEL", C_PARCHMENT, scale);
        ui_text(x + 20 * scale, y + 76 * scale,
                        "Original Free Duel", C_MUTED, scale);
        portrait(0, DEMO_KAIBA, x + w - 59 * scale, y + 44 * scale,
                 40 * scale, 245);
        selected_bar(x + 10 * scale, y + 97 * scale, w - 20 * scale,
                     small_row, tag_mode, scale);
        ui_text(x + 20 * scale, y + 116 * scale,
                        "TAG DUEL", TagDuel_Available() ? C_PARCHMENT : C_MUTED, scale);
        ui_text(x + 20 * scale, y + 135 * scale,
                        "Two allies vs two rivals", C_MUTED, scale);
        portrait(1, DEMO_TEANA_SECOND, x + w - 105 * scale, y + 103 * scale,
                 40 * scale, 245);
        portrait(2, DEMO_JOEY_SECOND, x + w - 59 * scale, y + 103 * scale,
                 40 * scale, 245);
        host->fill(host, x + 20 * scale, y + 166 * scale,
                   w - 40 * scale, scale, C_GOLD, 150);
        text_center(x, w, y + 179 * scale,
                    tag_mode ? "SHARED FIELD + LP  /  PRIVATE HANDS"
                             : "THE ORIGINAL ONE-ON-ONE BATTLE",
                    C_PARCHMENT, scale);
        prompt_center(x, w, y + h - 25 * scale,
                      "UP/DOWN PICK   {X} ENTER   {O} BACK",
                      C_GOLD, scale);
        return;
    }
    text_center(x, w, y + 35 * scale, "FREE DUEL", C_GOLD_LIGHT, scale);
    text_center(x, w, y + 56 * scale,
                "Choose a battle format", C_PARCHMENT, scale);
    selected_bar(x + 22 * scale, y + row * 1, inner, 68 * scale,
                 !tag_mode, scale);
    ui_text(x + 40 * scale, y + row + 23 * scale,
                    "SINGLE DUEL", C_PARCHMENT, scale);
    ui_text(x + 40 * scale, y + row + 45 * scale,
                    "Original Free Duel", C_MUTED, scale);
    portrait(0, DEMO_KAIBA, x + w - 83 * scale,
             y + row + 10 * scale, 48 * scale, 245);
    selected_bar(x + 22 * scale, y + row * 2 + 6 * scale, inner, 68 * scale,
                 tag_mode, scale);
    ui_text(x + 40 * scale, y + row * 2 + 29 * scale,
                    "TAG DUEL", TagDuel_Available() ? C_PARCHMENT : C_MUTED, scale);
    ui_text(x + 40 * scale, y + row * 2 + 51 * scale,
                    TagDuel_Available() ? "Choose a partner and two rivals" : "Enable Tag Duels in Game > Mods", C_MUTED, scale);
    portrait(1, DEMO_TEANA_SECOND, x + w - 135 * scale,
             y + row * 2 + 16 * scale, 48 * scale, 245);
    portrait(2, DEMO_JOEY_SECOND, x + w - 83 * scale,
             y + row * 2 + 16 * scale, 48 * scale, 245);
    prompt_center(x, w, y + h - 23 * scale,
                  "UP/DOWN SELECT   {X} ENTER   {O} BACK",
                  C_GOLD, scale);
}

static const char *pick_prompt(void)
{
    switch (pick_step) {
    case PICK_PARTNER: return "CHOOSE YOUR PARTNER";
    case PICK_OPPONENT_ONE: return "CHOOSE FIRST OPPONENT";
    case PICK_OPPONENT_TWO: return "CHOOSE SECOND OPPONENT";
    default: return "TAG TEAM READY";
    }
}

static void draw_grid_rails(int cw, int ch, int scale)
{
    int game_w = mini(cw, ch * 4 / 3);
    int margin = (cw - game_w) / 2;
    int rail = mini(margin - 20 * scale, 172 * scale);
    int x, y, i, id, size;
    const int picks[3] = {partner, opponent_1, opponent_2};
    const char *labels[3] = {"PARTNER", "RIVAL 1", "RIVAL 2"};
    char line[96];
    int current = selected_grid_id();
    if (rail >= 104 * scale) {
        int left = (margin - rail) / 2;
        int right = cw - margin + left;
        int group_y = maxi(54 * scale, (ch - 390 * scale) / 2);
        x = right;
        size = mini(56 * scale, rail - 24 * scale);
        for (i = 0; i < 3; i++) {
            id = i < pick_step ? picks[i] : (i == pick_step && grid_ready() ? current : -1);
            y = group_y + i * 132 * scale;
            gilded_panel(x, y, rail, 124 * scale, scale);
            if (id >= 1) {
                portrait(i, id, x + (rail - size) / 2, y + 14 * scale,
                         size, i == pick_step ? 255 : 205);
                text_center_fitted(x, rail, y + 86 * scale,
                                   duelist_name(id), C_PARCHMENT, scale);
            } else {
                text_center(x, rail, y + 47 * scale, "?", C_MUTED, scale);
            }
            text_center(x, rail, y + 106 * scale,
                        labels[i], C_GOLD_LIGHT, scale);
        }
        gilded_panel(left, group_y, rail, 254 * scale, scale);
        text_center(left, rail, group_y + 21 * scale, "TEAM CONTROL", C_GOLD, scale);
        draw_player_portrait(left + (rail - 50 * scale) / 2,
                             group_y + 35 * scale, 50 * scale);
        text_center_fitted(left, rail, group_y + 99 * scale,
                           player_save_name(), C_PARCHMENT, scale);
        text_center(left, rail, group_y + 115 * scale, "PLAYER 1", C_TEAM, scale);
        host->fill(host, left + 20 * scale, group_y + 129 * scale,
                   rail - 40 * scale, scale, C_MUTED, 160);
        text_center(left, rail, group_y + 145 * scale, "+", C_GOLD_LIGHT, scale);
        if (pick_step > PICK_PARTNER)
            portrait(0, partner, left + (rail - 50 * scale) / 2,
                     group_y + 158 * scale, 50 * scale, 255);
        else text_center(left, rail, group_y + 185 * scale,
                         "?", C_MUTED, scale);
        text_center_fitted(left, rail, group_y + 222 * scale,
                           pick_step > PICK_PARTNER ? duelist_name(partner) : "CHOOSE",
                           C_PARCHMENT, scale);
        text_center(left, rail, group_y + 239 * scale,
                    controller_labels[partner_control], C_TEAM, scale);
    }
}

static void draw_grid_guidance(int cw, int ch, int scale)
{
    int game_w = mini(cw, ch * 4 / 3);
    int margin = (cw - game_w) / 2;
    char line[96];
    gilded_panel(margin + 8 * scale, ch - 50 * scale,
                 game_w - 16 * scale, 48 * scale, scale);
    snprintf(line, sizeof(line), "TAG DUEL  /  %s", pick_prompt());
    text_center(0, cw, ch - 35 * scale, line, C_GOLD_LIGHT, scale);
    prompt_center(0, cw, ch - 18 * scale,
                  "{X} CHOOSE   {O} UNDO   {T} DUEL TYPE",
                  C_PARCHMENT, scale);
}

static void draw_solo_guidance(int cw, int ch, int scale)
{
    int w = mini(cw, ch * 4 / 3) - 16 * scale;
    gilded_panel((cw - w) / 2, ch - 50 * scale, w, 48 * scale, scale);
    text_center(0, cw, ch - 35 * scale,
                "SINGLE DUEL  /  CHOOSE AN OPPONENT", C_PARCHMENT, scale);
    prompt_center(0, cw, ch - 18 * scale,
                  TagDuel_Available() ? "{T} DUEL TYPE   {X} DUEL   {O} BACK" :
                                       "{X} DUEL   {O} BACK",
                  C_GOLD, scale);
}

static int next_rule(int row, int direction)
{
    do row = (row + direction + RULE_COUNT) % RULE_COUNT;
    while ((!tag_mode && row < RULE_DRAW_POOL) ||
           (tag_mode && partner_deck_source == DECK_SIGNATURE &&
            row == RULE_RECIPE));
    return row;
}

static const char *rule_help(void)
{
    switch (rule_row) {
    case RULE_CONTROL: return "AI, your controller, or an optional second controller.";
    case RULE_DECK: return "Choose the partner's signature, an edit, or a recipe.";
    case RULE_RECIPE: return partner_deck_source == DECK_EDIT
        ? "Edit the partner's deck before the duel." : "Choose one saved deck by its contents.";
    case RULE_DRAW_POOL: return "CPU searches 5 or 20 cards; its visible hand stays at five.";
    case RULE_LIFE: return "Both teams begin with the selected Life Points.";
    case RULE_FIELD: return "Dynamic rolls after a full cycle, then after the starter's turn.";
    case RULE_OPENING: return "Guardian stars decide who chooses the first team and duelist.";
    default: return tag_mode ? "Choose your own deck next, then check it in the chest."
                             : "Begin this duel with the selected rules.";
    }
}

static void draw_rule_row(int x, int y, int w, int h, int row,
                          const char *label, const char *value, int scale)
{
    int enabled = row != RULE_RECIPE || partner_deck_source != DECK_SIGNATURE;
    int value_w;
    selected_bar(x, y, w, h, row == rule_row && enabled, scale);
    ui_text(x + 10 * scale, y + h / 2, label,
                    enabled ? C_PARCHMENT : C_MUTED, scale);
    value_w = ui_text_width(value, scale);
    if (strcmp(value, "{X}") == 0)
        TagOverlay_DrawPixels(host, x + w - 26 * scale, y + (h - 16 * scale) / 2,
                          16 * scale, 16 * scale, button_icons[0],
                          16, 16, 255, 255);
    else
        ui_text(x + w - 10 * scale - value_w, y + h / 2,
                value, enabled ? C_GOLD_LIGHT : C_MUTED, scale);
}

/* Count only rows exposed by this match mode, including a full thumb when
 * the list fits. Hidden partner rows must not distort the solo scroll range. */
static void rule_scroll_layout(int is_tag, int requested_visible,
                               int requested_first, int *first,
                               int *visible, int *total)
{
    int base = is_tag ? 0 : RULE_DRAW_POOL;
    *total = RULE_COUNT - base;
    *visible = mini(*total, maxi(1, requested_visible));
    *first = mini(RULE_COUNT - *visible, maxi(base, requested_first));
}

static void draw_rules(int cw, int ch, int scale)
{
    static const char *const fields[] = {
        "NORMAL", "FOREST", "WASTELAND", "MOUNTAIN", "SOGEN",
        "UMI", "YAMI", "RANDOM / FIXED", "RANDOM / DYNAMIC"
    };
    int w = mini(592 * scale, cw - 24 * scale);
    int h = mini(396 * scale, ch - 24 * scale);
    int x = (cw - w) / 2, y = (ch - h) / 2;
    int compact = h < 340 * scale;
    int step = (compact ? 23 : 35) * scale;
    int list_y = y + (compact ? 89 : 139) * scale;
    int list_bottom = y + h - (compact ? 33 : 66) * scale;
    int visible = maxi(1, (list_bottom - list_y) / step);
    int first, last, total;
    rule_scroll_layout(tag_mode, visible, rule_scroll, &first, &visible, &total);
    last = first + visible;
    int row, yy, row_w = w - 55 * scale;
    char value[48];
    host->fill(host, 0, 0, cw, ch, C_INK, 64);
    gilded_panel(x, y, w, h, scale);
    text_center(x, w, y + 21 * scale,
                tag_mode ? "TAG DUEL / BATTLE RULES" : "SINGLE DUEL / BATTLE RULES",
                C_GOLD_LIGHT, scale);
    if (tag_mode) {
        int pic = (compact ? 28 : 40) * scale;
        portrait(0, partner, x + w / 4 - pic / 2, y + 44 * scale, pic, 255);
        portrait(1, opponent_1, x + w / 2 - pic / 2, y + 44 * scale, pic, 255);
        portrait(2, opponent_2, x + w * 3 / 4 - pic / 2, y + 44 * scale, pic, 255);
        text_center_fitted(x + w / 4 - 66 * scale, 132 * scale,
                           y + (compact ? 79 : 106) * scale,
                           duelist_name(partner), C_TEAM, scale);
        text_center_fitted(x + w / 2 - 66 * scale, 132 * scale,
                           y + (compact ? 79 : 106) * scale,
                           duelist_name(opponent_1), C_ENEMY, scale);
        text_center_fitted(x + w * 3 / 4 - 66 * scale, 132 * scale,
                           y + (compact ? 79 : 106) * scale,
                           duelist_name(opponent_2), C_ENEMY, scale);
    } else {
        int pic = (compact ? 28 : 40) * scale;
        portrait(0, solo_opponent, x + w / 2 - pic / 2,
                 y + 44 * scale, pic, 255);
        text_center_fitted(x + w / 2 - 66 * scale, 132 * scale,
                           y + (compact ? 79 : 106) * scale,
                           duelist_name(solo_opponent), C_ENEMY, scale);
    }
    for (row = first; row < last; row++) {
        const char *label = "";
        const char *shown = "";
        if (!tag_mode && row < RULE_DRAW_POOL) continue;
        switch (row) {
        case RULE_CONTROL: label = "PARTNER CONTROL"; shown = control_names[partner_control]; break;
        case RULE_DECK: label = "PARTNER DECK"; shown = deck_names[partner_deck_source]; break;
        case RULE_RECIPE:
            label = partner_deck_source == DECK_EDIT ? "EDIT PARTNER DECK" : "DECK RECIPE";
            if (partner_deck_source == DECK_SIGNATURE)
                snprintf(value, sizeof(value), "NOT NEEDED");
            else if (partner_deck_source == DECK_EDIT)
                snprintf(value, sizeof(value), partner_edited ? "READY" : "OPEN");
            else snprintf(value, sizeof(value), "SLOT %d", partner_deck_slot);
            shown = value; break;
        case RULE_DRAW_POOL: label = "OPPONENT DRAW HAND"; shown = hard_mode ? "20 CARDS" : "5 CARDS"; break;
        case RULE_LIFE: label = "STARTING LIFE"; snprintf(value, sizeof(value), "%d LP", starting_life); shown = value; break;
        case RULE_FIELD: label = "STARTING FIELD"; shown = fields[starting_field]; break;
        case RULE_OPENING: label = "ROLL FOR WHO PLAYS FIRST"; shown = opening_clash ? "ON" : "OFF"; break;
        case RULE_CONTINUE: label = tag_mode ? "BEGIN TAG DUEL" : "BEGIN SINGLE DUEL"; shown = ""; break;
        }
        yy = list_y + (row - first) * step;
        draw_rule_row(x + 19 * scale, yy, row_w,
                      (compact ? 21 : 32) * scale,
                      row, label, shown, scale);
    }
    /* Narrow stone rail and arrows echo the chest list's scrollbar. */
    {
        int track_h = list_bottom - list_y;
        int base = tag_mode ? 0 : RULE_DRAW_POOL;
        int thumb_h = mini(track_h, maxi(24 * scale, track_h * visible / total));
        int thumb_y = list_y + (track_h - thumb_h) * (first - base) /
                     maxi(1, total - visible);
        host->fill(host, x + w - 26 * scale, list_y, 4 * scale,
                   track_h, C_MUTED, 110);
        draw_scroll_thumb(x + w - 29 * scale, thumb_y, 10 * scale, thumb_h);
    }
    if (!compact)
        text_center(x, w, y + h - 45 * scale, rule_help(), C_PARCHMENT, scale);
    prompt_center(x, w, y + h - 21 * scale,
                  "UP/DOWN FOCUS   LEFT/RIGHT CHANGE   {X} CHOOSE   {O} BACK",
                  C_GOLD, scale);
}

/* Extension tabs share the same native panel, font and scroll-thumb assets. */
static DuelOptionRow extension_rows[DUEL_OPTION_MAX];
static char extension_tabs[DUEL_OPTION_MAX][32];
static int extension_tab_count, extension_tab, extension_focus, extension_scroll;
static int extension_indices[DUEL_OPTION_MAX], extension_count;
static void extension_collect(void)
{
    int total=DuelOptions_Rows(tag_mode ? DUEL_OPTION_TAG : DUEL_OPTION_SOLO,
                               extension_rows,DUEL_OPTION_MAX);
    extension_tab_count=0;extension_count=0;
    if(total<0)return;
    for(int i=0;i<total;i++) {
        if(!strcmp(extension_rows[i].owner,host->id))continue;
        const char *name=extension_rows[i].option.tab;int j;
        for(j=0;j<extension_tab_count;j++)if(!strcmp(extension_tabs[j],name))break;
        if(j==extension_tab_count)strcpy(extension_tabs[extension_tab_count++],name);
    }
    if(extension_tab_count==0) { extension_tab=0;extension_focus=extension_scroll=0;return; }
    extension_tab=mini(maxi(extension_tab,0),extension_tab_count-1);
    for(int i=0;i<total;i++)if(strcmp(extension_rows[i].owner,host->id) &&
        !strcmp(extension_rows[i].option.tab,extension_tabs[extension_tab]))extension_indices[extension_count++]=i;
    extension_focus=mini(maxi(extension_focus,0),maxi(0,extension_count-1));
    extension_scroll=mini(extension_scroll,extension_focus);
}
static void draw_extensions(int cw,int ch,int scale)
{
    int w=mini(600*scale,cw-24*scale),h=mini(340*scale,ch-24*scale);
    int x=(cw-w)/2,y=(ch-h)/2,visible=5,step=38*scale,list_y=y+62*scale;
    host->fill(host,0,0,cw,ch,C_INK,68);gilded_panel(x,y,w,h,scale);
    text_center(x,w,y+26*scale,extension_tab_count ? extension_tabs[extension_tab] : "MORE DUEL OPTIONS",C_GOLD_LIGHT,scale);
    if(!extension_count)text_center(x,w,y+h/2,"No enabled mod options in this tab.",C_PARCHMENT,scale);
    for(int i=extension_scroll;i<mini(extension_count,extension_scroll+visible);i++) {
        DuelOptionRow *row=&extension_rows[extension_indices[i]];const char *shown="";
        for(int j=0;j<row->option.choice_count;j++)if(row->option.choices[j].value==row->value)shown=row->option.choices[j].label;
        int yy=list_y+(i-extension_scroll)*step;
        selected_bar(x+18*scale,yy,w-48*scale,32*scale,i==extension_focus,scale);
        text_center_fitted(x+28*scale,w/2-40*scale,yy+16*scale,row->option.label,C_PARCHMENT,scale);
        text_center_fitted(x+w/2,w/2-40*scale,yy+16*scale,shown,C_GOLD_LIGHT,scale);
    }
    if(extension_count>visible) {
        int track=visible*step,thumb=maxi(20*scale,track*visible/extension_count);
        int offset=(track-thumb)*extension_scroll/maxi(1,extension_count-visible);
        draw_scroll_thumb(x+w-25*scale,list_y+offset,10*scale,thumb);
    }
    if(extension_count)text_center_fitted(x+20*scale,w-40*scale,y+h-60*scale,
        extension_rows[extension_indices[extension_focus]].option.description,C_PARCHMENT,scale);
    prompt_center(x,w,y+h-24*scale,"UP/DOWN PICK   LEFT/RIGHT CHANGE   {O} BACK",C_GOLD,scale);
}
static void update_extensions(unsigned pressed)
{
    extension_collect();
    if(pressed & PAD_BUTTON_CIRCLE) { page=PAGE_RULES;SD_SEPlay(SE_BACK,255,0);revision++;return; }
    if(pressed & PAD_BUTTON_L1_R1_MASK) {
        int direction=(pressed & PAD_BUTTON_R1) ? 1 : -1;
        int next=extension_tab+direction;
        if(next<0 || next>=extension_tab_count)page=PAGE_RULES;
        else extension_tab=next;
        extension_focus=extension_scroll=0;extension_collect();SD_SEPlay(SE_MOVE,255,0);revision++;return;
    }
    if(!extension_count)return;
    int move=!!(pressed & PAD_DIRECTION_DOWN)-!!(pressed & PAD_DIRECTION_UP);
    if(move) {
        extension_focus=(extension_focus+move+extension_count)%extension_count;
        if(extension_focus<extension_scroll)extension_scroll=extension_focus;
        if(extension_focus>=extension_scroll+5)extension_scroll=extension_focus-4;
        SD_SEPlay(SE_MOVE,255,0);revision++;
    }
    int change=!!(pressed & PAD_DIRECTION_RIGHT)-!!(pressed & PAD_DIRECTION_LEFT);
    if(change) {
        DuelOptionRow *row=&extension_rows[extension_indices[extension_focus]];int at=0;
        for(int i=0;i<row->option.choice_count;i++)if(row->option.choices[i].value==row->value)at=i;
        at=(at+change+row->option.choice_count)%row->option.choice_count;
        if(DuelOptions_Set(row->owner,row->option.key,row->option.choices[at].value)) {
            row->value=row->option.choices[at].value;SD_SEPlay(SE_MOVE,255,0);revision++;
        }
    }
}

static void draw_recipes(int cw, int ch, int scale)
{
    int w = mini(410 * scale, cw - 14 * scale);
    int h = mini(320 * scale, ch - 10 * scale);
    int x = (cw - w) / 2, y = (ch - h) / 2;
    int count = recipe_for_player && !recipe_save ? 11 : 10;
    int row_h = mini(21 * scale, (h - 80 * scale) / count);
    int i;
    char name[96], line[120];
    host->fill(host, 0, 0, cw, ch, C_INK, 75);
    gilded_panel(x, y, w, h, scale);
    text_center(x, w, y + 17 * scale,
                recipe_save ? "SAVE DECK RECIPE" : recipe_for_player ? "CHOOSE YOUR DECK" : "CHOOSE PARTNER RECIPE",
                C_GOLD_LIGHT, scale);
    for (i = 0; i < count; i++) {
        int slot = recipe_for_player && !recipe_save ? i - 1 : i;
        int available = slot < 0 ? 1 : recipe_tab ?
            DeckMenu_PartnerRecipeInfo(slot, name, sizeof(name)) :
            slot_info ? slot_info(slot, name, sizeof(name)) : 0;
        int max_chars = (w - 36 * scale) / (8 * scale);
        if (slot < 0) snprintf(line, sizeof(line), "CURRENT EQUIPPED DECK");
        else if (available == 0) snprintf(line, sizeof(line), "%2d  EMPTY", slot + 1);
        else snprintf(line, sizeof(line), "%2d  %s", slot + 1,
                      name[0] ? name : "Unavailable deck");
        if (max_chars >= 0 && max_chars < (int)strlen(line)) line[max_chars] = 0;
        selected_bar(x + 9 * scale, y + 29 * scale + i * row_h,
                     w - 18 * scale, row_h, i == recipe_cursor, scale);
        ui_text(x + 15 * scale, y + 29 * scale + i * row_h + row_h / 2,
                line, available == 1 ? C_PARCHMENT : C_MUTED, scale);
    }
    prompt_center(x, w, y + h - 23 * scale,
                  recipe_save ? "{X} SAVE   {O} BACK" :
                  "L1/R1 TAB  {T} EDIT  {X} USE  {O} BACK",
                  C_GOLD, scale);
    text_center(x, w, y + h - 43 * scale,
                recipe_tab ? "PLAYER   [PARTNER]" : "[PLAYER]   PARTNER",
                C_TEAM, scale);
}

static void draw_confirm(int cw, int ch, int scale)
{
    int w = mini(600 * scale, cw - 24 * scale), h = 170 * scale;
    int x = (cw - w) / 2, y = (ch - h) / 2;
    char line[120];
    gilded_panel(x, y, w, h, scale);
    if (ask_kind == 1) snprintf(line, sizeof(line), "Save %s recipe?", draft_name);
    else if (ask_kind == 2) snprintf(line, sizeof(line), "Leave without saving?");
    else if (ask_kind == 4) snprintf(line, sizeof(line), "Overwrite recipe slot %d?", recipe_cursor + 1);
    else snprintf(line, sizeof(line), "You lack cards to build this deck. Use it anyway?");
    text_center_fitted(x, w, y + 51 * scale, line, C_PARCHMENT, scale);
    if (ask_kind == 3) {
        text_center(x, w, y + 78 * scale, "Only owned copies will enter your deck.", C_MUTED, scale);
        text_center(x, w, y + 96 * scale, "Fill the empty spaces before the duel.", C_MUTED, scale);
    }
    prompt_center(x, w, y + h - 31 * scale, "{X} YES    {O} NO", C_GOLD, scale);
}

static int clash_beats(int a, int b)
{
    if (a < 6 && b < 6) return (a + 1) % 6 == b;
    if (a >= 6 && b >= 6) return 6 + (a - 6 + 1) % 4 == b;
    return 0;
}

static void clash_shuffle(void)
{
    int i;
    for (i = 0; i < 10; i++) clash_order[i] = i;
    for (i = 9; i > 0; i--) {
        int j = rand() % (i + 1);
        int t = clash_order[i];
        clash_order[i] = clash_order[j];
        clash_order[j] = t;
    }
    clash_cursor = rand() % 10;
    clash_player_card = -1;
    clash_enemy_card = -1;
    /* The rival does not hover or roll a card before the player commits. */
    clash_enemy_cursor = -1;
    clash_phase = 0;
    clash_phase_at = host->now_us(host);
    revision++;
}

static int clash_pick_enemy(int player_position)
{
    int player = clash_order[player_position];
    int desired, pos, choice = -1;
    int roll = rand() % 100;
    if (player < 6)
        desired = roll < 45 ? (player + 5) % 6 : (player + 1) % 6;
    else
        desired = 6 + (roll < 45 ? (player - 6 + 3) % 4
                                  : (player - 6 + 1) % 4);
    if (roll < 90) {
        for (pos = 0; pos < 10; pos++)
            if (pos != player_position && clash_order[pos] == desired)
                choice = pos;
    }
    if (choice < 0) {
        do choice = rand() % 10; while (choice == player_position);
    }
    return choice;
}

static void draw_clash_choice(int cx, int y, int scale, int compact,
                              const char *left, const char *right, int selected)
{
    int option_w = (compact ? 113 : 152) * scale;
    int gap = (compact ? 10 : 20) * scale;
    int option_h = (compact ? 19 : 26) * scale;
    int x = cx - option_w - gap / 2;
    int i;
    for (i = 0; i < 2; i++) {
        int ox = x + i * (option_w + gap);
        host->fill(host, ox, y, option_w, option_h, C_INK, 230);
        if (selected == i)
            host->fill(host, ox, y + option_h - 2 * scale,
                       option_w, 2 * scale, C_GOLD_LIGHT, 255);
        text_center_fitted(ox, option_w, y + (compact ? 3 : 6) * scale,
                           i ? right : left,
                           selected == i ? 0xFFFFFFu : C_CHOICE_DIM, scale);
    }
}

static void draw_faceoff_card(int x, int y, int star, int back_rotation,
                              int face, int size, int width, int alpha)
{
    const unsigned int *pixels = face ? clash_fronts[star] :
                                        clash_backs[back_rotation];
    int drawn_width = maxi(3, size * width / 100);
    TagOverlay_DrawPixels(host, x - drawn_width / 2, y - size / 2,
                      drawn_width, size, pixels, 80, 80, alpha, 255);
}

static void draw_faceoff_glow(int x, int y, int rotation, int size,
                              int player_glow, int enemy_glow)
{
    static const int cosine[10] =
        {1000, 809, 309, -309, -809, -1000, -809, -309, 309, 809};
    static const int sine[10] =
        {0, 588, 951, 951, 588, 0, -588, -951, -951, -588};
    const unsigned int *source = clash_backs[rotation];
    int p;
    for (p = 0; p < 80 * 80; p++) {
        uint32_t base = source[p];
        uint32_t tint = player_glow && enemy_glow ?
            (((p % 80 * 2 - 79) * cosine[rotation] +
              (p / 80 * 2 - 79) * sine[rotation]) < 0 ?
                 C_TEAM : C_ENEMY) :
            player_glow ? C_TEAM : C_ENEMY;
        unsigned r, g, b;
        if (!(base & 0xFF000000u)) {
            clash_glow_pixels[p] = 0;
            continue;
        }
        r = mini(255, (((base >> 16) & 255u) * 3u +
                       ((tint >> 16) & 255u) * 2u) / 5u + 25u);
        g = mini(255, (((base >> 8) & 255u) * 3u +
                       ((tint >> 8) & 255u) * 2u) / 5u + 25u);
        b = mini(255, ((base & 255u) * 3u +
                       (tint & 255u) * 2u) / 5u + 25u);
        clash_glow_pixels[p] = (base & 0xFF000000u) |
                               (r << 16) | (g << 8) | b;
    }
    TagOverlay_DrawPixels(host, x - size / 2, y - size / 2,
                      size, size, clash_glow_pixels, 80, 80, 255, 255);
}

static void draw_clash_transition(int x,int y,int w,int h,int scale,int elapsed)
{
    /* The transition covers content, preserving only the native stone frame. */
    host->fill(host,x+5*scale,y+5*scale,w-10*scale,h-10*scale,C_INK,
               mini(255,elapsed*255/300));
}

static void draw_clash_v2(int cw, int ch, int scale)
{
    int w = mini(620 * scale, cw - 16 * scale);
    int h = mini(434 * scale, ch - 16 * scale);
    int x = (cw - w) / 2, y = (ch - h) / 2;
    int compact = h < 340 * scale;
    int cx = cw / 2;
    int screen = clash_phase <= 3 || clash_phase == 10 ? 1 :
                 clash_phase <= 5 || clash_phase == 11 ? 2 : 3;
    int side_width = (compact ? 104 : 140) * scale;
    int portrait_size = (compact ? 56 : 84) * scale;
    int left_side = x + (compact ? 7 : 20) * scale;
    int right_side = x + w - side_width - (compact ? 7 : 20) * scale;
    /* A shared horizontal axis through the content area's centre aligns
     * both portrait centres, the radial pile and its crossed swords. */
    int content_y = y + (h + 18 * scale) / 2;
    int portrait_y = content_y - portrait_size / 2;
    int names_y = portrait_y + portrait_size + (compact ? 20 : 23) * scale;
    int elapsed = (int)((host->now_us(host) - clash_phase_at) / 1000);
    int i;
    host->fill(host, 0, 0, cw, ch, C_INK, 110);
    gilded_panel(x, y, w, h, scale);
    text_center(x, w, y + 25 * scale,
                "GUARDIAN STAR FACE-OFF", C_GOLD_LIGHT, scale);
    if (screen == 1)
        text_center(x, w, y + 41 * scale, "PICK A CARD!", C_PARCHMENT, scale);
    if (screen != 3) {
        draw_player_portrait(left_side + (side_width - portrait_size) / 2,
                             portrait_y, portrait_size);
        portrait(1, tag_mode ? opponent_1 : solo_opponent,
                 right_side + (side_width - portrait_size) / 2,
                 portrait_y, portrait_size, 255);
        text_center_fitted(left_side, side_width, names_y,
                           player_save_name(), C_TEAM, scale);
        text_center_fitted(right_side, side_width, names_y,
                           tag_mode ? duelist_name(opponent_1) :
                                      duelist_name(solo_opponent), C_ENEMY, scale);
    }

    if (screen == 1) {
        int ring_y = content_y;
        int px, py;
        int card_size = (compact ? 60 : 94) * scale;
        TagOverlay_DrawPixels(host, cx - (compact ? 30 : 62) * scale,
                          ring_y - (compact ? 30 : 62) * scale,
                          (compact ? 60 : 124) * scale,
                          (compact ? 60 : 124) * scale,
                          clash_crossed_swords, 80, 80, 210, 255);
        for (i = 0; i < 10; i++) {
            int player_glow = clash_phase == 0 && i == clash_cursor;
            int enemy_glow = clash_phase == 2 && i == clash_enemy_cursor;
            if ((clash_phase >= 1 && i == clash_player_card) ||
                (clash_phase >= 3 && i == clash_enemy_card)) continue;
            px = cx + clash_x[i] * scale * (compact ? 47 : 100) / 100;
            py = ring_y + clash_y[i] * scale * (compact ? 47 : 100) / 100;
            draw_faceoff_card(px, py, clash_order[i], i, 0,
                              card_size, 100, 255);
            if (player_glow || enemy_glow)
                draw_faceoff_glow(px, py, i, card_size,
                                  player_glow, enemy_glow);
        }
        for (i = 0; i < 2; i++) {
            int chosen = i ? clash_enemy_card : clash_player_card;
            int active = i ? clash_phase >= 3 : clash_phase >= 1;
            int animating = i ? clash_phase == 3 : clash_phase == 1;
            int t = animating ? mini(elapsed, 650) : 650;
            int alpha = t < 325 ? 255 - t * 255 / 325 :
                        (t - 325) * 255 / 325;
            int target_x = (i ? right_side : left_side) + side_width / 2;
            int target_y = portrait_y - (compact ? 27 : 45) * scale;
            if (!active || chosen < 0) continue;
            px = t < 325 ? cx + clash_x[chosen] * scale *
                 (compact ? 47 : 100) / 100 : target_x;
            py = t < 325 ? ring_y + clash_y[chosen] * scale *
                 (compact ? 47 : 100) / 100 : target_y;
            draw_faceoff_card(px, py, clash_order[chosen], 0, 0,
                              (compact ? 49 : 78) * scale, 100, alpha);
        }
        prompt_center(x, w, y + h - 24 * scale,
                      clash_phase == 0 ? "LEFT/RIGHT PICK   {X} DRAW" :
                      clash_phase == 2 ? "RIVAL IS SEARCHING THE PILE" :
                      "CARDS ARE LEAVING THE PILE...", C_GOLD, scale);
    } else if (screen == 2) {
        int t = clash_phase == 4 ? elapsed : 1600;
        int face = t >= 900;
        int width = t < 550 ? 100 : t < 900 ?
                    maxi(5, 100 - (t - 550) * 95 / 350) :
                    t < 1250 ? maxi(5, (t - 900) * 100 / 350) : 100;
        int card_y = y + (compact ? 109 : 211) * scale;
        int player_star = clash_order[clash_player_card];
        int enemy_star = clash_order[clash_enemy_card];
        char matchup[96], winner[96];
        int card_size = (compact ? 138 : 198) * scale;
        draw_faceoff_card(cx - (compact ? 68 : 108) * scale, card_y,
                          player_star, 0, face, card_size, width, 255);
        draw_faceoff_card(cx + (compact ? 68 : 108) * scale, card_y,
                          enemy_star, 0, face, card_size, width, 255);
        if (t >= 1250) {
            const char *explanation;
            if (clash_winner < 0) {
                snprintf(matchup, sizeof(matchup), "%s = %s",
                         clash_star_names[player_star], clash_star_names[enemy_star]);
                explanation = "THESE STARS ARE NEUTRAL";
                snprintf(winner, sizeof(winner), "NO WINNER - DRAW AGAIN");
            } else {
                int strong = clash_winner ? enemy_star : player_star;
                int weak = clash_winner ? player_star : enemy_star;
                snprintf(matchup, sizeof(matchup), "%s > %s",
                         clash_star_names[strong], clash_star_names[weak]);
                explanation = clash_advantage_lines[strong];
                snprintf(winner, sizeof(winner), "TEAM %s WINS!",
                         clash_winner ?
                         (tag_mode ? duelist_name(opponent_1) :
                                     duelist_name(solo_opponent)) :
                         player_save_name());
            }
            text_center_fitted(x + 20 * scale, w - 40 * scale,
                               y + (compact ? 174 : 320) * scale,
                               matchup, 0xFFFFFFu, scale);
            text_center_fitted(x + 20 * scale, w - 40 * scale,
                               y + (compact ? 195 : 349) * scale,
                               explanation, C_GOLD_LIGHT, scale);
            text_center_fitted(x + 20 * scale, w - 40 * scale,
                               y + (compact ? 221 : 389) * scale,
                               winner, C_TEAM, scale);
        }
    } else {
        int cpu = clash_phase == 8 || clash_phase == 9;
        int rival_picking = cpu;
        const char *chooser = rival_picking ?
            (tag_mode ? duelist_name(opponent_1) :
                        duelist_name(solo_opponent)) : player_save_name();
        const char *heading;
        int choice;
        if (rival_picking)
            portrait(2, tag_mode ? opponent_1 : solo_opponent,
                     cx - (compact ? 28 : 42) * scale,
                     y + (compact ? 63 : 98) * scale,
                     (compact ? 56 : 84) * scale, 255);
        else
            draw_player_portrait(cx - (compact ? 28 : 42) * scale,
                                 y + (compact ? 63 : 98) * scale,
                                 (compact ? 56 : 84) * scale);
        text_center_fitted(x + 80 * scale, w - 160 * scale,
                           y + (compact ? 139 : 206) * scale,
                           chooser, rival_picking ? C_ENEMY : C_TEAM, scale);
        if (clash_phase == 6 || clash_phase == 8) {
            choice = clash_phase == 6 ? clash_first_side : 1 - clash_first_side;
            if (cpu) choice ^= clash_cpu_step & 1;
            heading = "WHO PLAYS FIRST?";
            draw_clash_choice(cx, y + h - (compact ? 58 : 78) * scale,
                              scale, compact, "PLAY FIRST", "PLAY SECOND", choice);
        } else {
            int rival_starter = clash_first_side;
            choice = rival_starter ? clash_enemy_starter : clash_ally_starter;
            if (cpu) choice ^= clash_cpu_step & 1;
            heading = "CHOOSE THE OPENING DUELIST";
            draw_clash_choice(cx, y + h - (compact ? 58 : 78) * scale,
                              scale, compact,
                              rival_starter ? duelist_name(opponent_1) :
                                              player_save_name(),
                              rival_starter ? duelist_name(opponent_2) :
                                              duelist_name(partner), choice);
        }
        text_center(x, w, y + h - (compact ? 77 : 105) * scale,
                    heading, C_PARCHMENT, scale);
        prompt_center(x, w, y + h - 24 * scale,
                      cpu ? "RIVAL IS CHOOSING..." :
                            "LEFT/RIGHT PICK   {X} CONFIRM",
                      C_GOLD, scale);
    }
    if (clash_phase == 10 || clash_phase == 11)
        draw_clash_transition(x,y,w,h,scale,elapsed);
}

static void overlay(void)
{
    int cw, ch, scale;
    int (*begin_logical)(int) = host->symbol(host, "ModMenu_BeginLogicalOverlay");
    void (*end_logical)(void) = host->symbol(host, "ModMenu_EndLogicalOverlay");
    int logical;
    if ((D_8009B26C & 0x1f) != MAIN_MODE_FREE_DUEL ||
        !(D_8009B26C & 0x40)) {
        if ((D_8009B26C & 0x1f) == MAIN_MODE_BUILD_DECK &&
            edit_flow == EDIT_BUILDING) {
            char heading[96];
            host->overlay_size(host, &cw, &ch, &scale);
            scale = mini(maxi(scale, 1), maxi(1, ch / 480));
            snprintf(heading, sizeof(heading), "EDITING %s", draft_name);
            host->fill(host, 0, ch - 26 * scale, cw, 26 * scale, C_INK, 235);
            text_center(0, cw, ch - 13 * scale, heading, C_GOLD_LIGHT, scale);
        }
        return;
    }
    if (page == PAGE_GRID && tag_mode) {
        host->overlay_size(host, &cw, &ch, &scale);
        draw_grid_rails(cw, ch, mini(maxi(scale, 1), maxi(1, ch / 480)));
    }
    logical = begin_logical && end_logical && begin_logical(480);
    host->overlay_size(host, &cw, &ch, &scale);
    if (cw <= 0 || ch <= 0) return;
    scale = mini(maxi(scale, 1), maxi(1, ch / 480));
    if (page == PAGE_MODE) draw_mode_panel(cw, ch, scale);
    else if (page == PAGE_RULES)
        draw_rules(cw, ch, scale);
    else if(page==PAGE_EXTENSIONS)draw_extensions(cw,ch,scale);
    else if (page == PAGE_CLASH) draw_clash_v2(cw, ch, scale);
    else if (page == PAGE_RECIPES || page == PAGE_PLAYER_DECK)
        draw_recipes(cw, ch, scale);
    else if (page == PAGE_CONFIRM) draw_confirm(cw, ch, scale);
    else if (tag_mode) draw_grid_guidance(cw, ch, scale);
    else draw_solo_guidance(cw, ch, scale);
    if (logical) end_logical();
}

static unsigned overlay_signature(void)
{
    unsigned state = revision;
    if (page == PAGE_CLASH && (clash_phase == 1 || clash_phase == 2 ||
        clash_phase == 3 || clash_phase == 4 || clash_phase >= 8))
        state ^= (unsigned)(host->now_us(host) / 33333u);
    state = state * 31u + (unsigned)edit_flow;
    if ((D_8009B26C & 0x1f) == MAIN_MODE_FREE_DUEL)
        state = state * 31u + (unsigned)selected_grid_id() +
                ((unsigned)gFreeDuel_bScreenFlags << 9);
    return state;
}

static void begin_draft_edit(const unsigned short cards[40], const char *name, int player)
{
    save_choices();
    snprintf(draft_name, sizeof(draft_name), "%s", name);
    editing_player_recipe = player;
    if (!DeckMenu_BeginTemporary(cards)) {
        SD_SEPlay(SE_REFUSED, 255, 0);
        return;
    }
    edit_flow = EDIT_PICKING;
    edit_menu_stable_frames = 0;
    resume_rules = 1;
    SD_SEPlay(SE_CONFIRM, 255, 0);
    revision++;
}

static void begin_partner_edit(void)
{
    unsigned short cards[40];
    char name[64];
    if (!TagDuel_SignatureDeck(partner, cards)) {
        SD_SEPlay(SE_REFUSED, 255, 0);
        return;
    }
    snprintf(name, sizeof(name), "%s Deck", duelist_name(partner));
    begin_draft_edit(cards, name, 0);
}

static int begin_tag_battle(void)
{
    int deck_source = partner_deck_source == DECK_SIGNATURE
        ? TAG_DECK_PARTNER_SIGNATURE : TAG_DECK_CUSTOM_SLOT;
    save_choices();
    host->log(host, "tag setup launch: partner %d rivals %d/%d LP %d field %d side %d",
              partner, opponent_1, opponent_2, starting_life, starting_field,
              clash_first_side);
    DuelMatch_Configure(starting_life, starting_field,
                           clash_first_side, clash_ally_starter,
                           clash_enemy_starter, 4);
    TagDuel_SetPartnerDeck(partner_deck_source != DECK_SIGNATURE && partner_edited ? partner_draft : NULL);
    if (!TagDuel_Start(partner, opponent_1, opponent_2, partner_control,
                       deck_source, partner_deck_slot, hard_mode)) {
        SD_SEPlay(SE_REFUSED, 255, 0);
        return 0;
    }
    page = PAGE_GRID;
    pick_step = PICK_OPPONENT_TWO;
    rematch_pending = 1;
    SD_SEPlay(SE_CONFIRM, 255, 0);
    revision++;
    return 1;
}

static int begin_solo_battle(void)
{
    extern u8 D_8009B368;
    save_choices();
    DuelMatch_Configure(starting_life, starting_field,
                           clash_first_side, 0, 0, 2);
    page = PAGE_GRID;
    /* Retail's selector rejects short decks before the preparation chest.
     * Borrowed recipes deliberately arrive short, so enter that chest directly. */
    func_80024DC8(-1, solo_opponent, 0x6000, 0x6000);
    gFreeDuel_bReturnFlags = 0x80;
    D_8009B368 = MAIN_MODE_FREE_DUEL;
    revision++;
    return 1;
}

static void begin_opening(void)
{
    clash_first_side = 0;
    clash_ally_starter = 0;
    clash_enemy_starter = 0;
    if (opening_clash) {
        if (!ClashArt_Load(host)) {
            host->log(host, "Tag Duels: cannot reconstruct Guardian Star art from the USA disc");
            SD_SEPlay(SE_REFUSED, 255, 0);
            return;
        }
        clash_shuffle();
        page = PAGE_CLASH;
        SD_SEPlay(SE_CONFIRM, 255, 0);
    } else if (tag_mode) begin_tag_battle();
    else begin_solo_battle();
    revision++;
}

static void update_clash(unsigned pressed)
{
    int elapsed = (int)((host->now_us(host) - clash_phase_at) / 1000);
    int player, enemy;
    /* Begin Duel commits the setup. Circle never cancels a roll/result. */
    if (clash_phase == 0) {
        if (pressed & PAD_DIRECTION_LEFT) clash_cursor = (clash_cursor + 9) % 10;
        if (pressed & PAD_DIRECTION_RIGHT) clash_cursor = (clash_cursor + 1) % 10;
        if (pressed & PAD_DIRECTION_HORIZONTAL_MASK) {
            SD_SEPlay(SE_MOVE, 255, 0);
            revision++;
        }
        if (pressed & PAD_BUTTON_CONFIRM_MASK) {
            clash_player_card = clash_cursor;
            clash_enemy_card = clash_pick_enemy(clash_cursor);
            clash_phase = 1;
            clash_phase_at = host->now_us(host);
            SD_SEPlay(SE_PORTRAIT, 255, 0);
            revision++;
        }
        return;
    }
    if (clash_phase == 1 && elapsed >= 650) {
        clash_phase = 2;
        clash_phase_at = host->now_us(host);
        clash_enemy_cursor = (clash_player_card + 1) % 10;
        clash_scan_step = 0;
        revision++;
    } else if (clash_phase == 2) {
        int first = (clash_player_card + 1) % 10;
        int offset = (clash_enemy_card - first + 10) % 10;
        int steps = 45 + offset;
        int step = mini(steps, elapsed * steps / 2000);
        clash_enemy_cursor = (first + step % 9) % 10;
        if (step != clash_scan_step) {
            clash_scan_step = step;
            revision++;
        }
        if (elapsed >= 2000) {
            clash_enemy_cursor = clash_enemy_card;
            clash_phase = 3;
            clash_phase_at = host->now_us(host);
            SD_SEPlay(SE_PORTRAIT, 255, 0);
            revision++;
        }
    } else if (clash_phase == 3 && elapsed >= 650) {
        player = clash_order[clash_player_card];
        enemy = clash_order[clash_enemy_card];
        clash_winner = clash_beats(player, enemy) ? 0 :
                       clash_beats(enemy, player) ? 1 : -1;
        host->log(host, "Guardian Star Face-off: %s vs %s winner %s",
                  clash_star_names[player], clash_star_names[enemy],
                  clash_winner < 0 ? "neutral" :
                  clash_winner == 0 ? "player" : "rival");
        clash_phase = 10;
        clash_phase_at = host->now_us(host);
        SD_SEPlay(SE_CONFIRM, 255, 0);
        revision++;
    } else if (clash_phase == 10 && elapsed >= 300) {
        clash_phase = 4;
        clash_phase_at = host->now_us(host);
        clash_flip_sounded = 0;
        revision++;
    } else if (clash_phase == 4) {
        if (elapsed >= 900 && !clash_flip_sounded) {
            clash_flip_sounded = 1;
            SD_SEPlay(SE_CONFIRM, 255, 0);
        }
        if (elapsed >= 3800) {
            clash_phase = clash_winner < 0 ? 5 : 11;
            clash_phase_at = host->now_us(host);
            if (clash_winner == 1) {
                clash_first_side = rand() % 10 < 8 ? 1 : 0;
                clash_cpu_step = 0;
            }
            SD_SEPlay(clash_winner < 0 ? SE_BACK : SE_CONFIRM, 255, 0);
            revision++;
        }
    } else if (clash_phase == 5 && elapsed >= 1350) {
        clash_shuffle(); /* Neutral stars require a fresh ten-card layout. */
    } else if (clash_phase == 11 && elapsed >= 300) {
        clash_phase = clash_winner == 0 ? 6 : 8;
        clash_phase_at = host->now_us(host);
        revision++;
    } else if (clash_phase == 6) {
        if (pressed & PAD_DIRECTION_LEFT) clash_first_side = 0;
        if (pressed & PAD_DIRECTION_RIGHT) clash_first_side = 1;
        if (pressed & PAD_DIRECTION_HORIZONTAL_MASK) {
            SD_SEPlay(SE_MOVE, 255, 0);
            revision++;
        }
        if (pressed & PAD_BUTTON_CONFIRM_MASK) {
            if (tag_mode) {
                clash_phase = 7;
                clash_phase_at = host->now_us(host);
            } else begin_solo_battle();
            SD_SEPlay(SE_CONFIRM, 255, 0);
            revision++;
        }
    } else if (clash_phase == 7) {
        int *starter = clash_first_side ? &clash_enemy_starter : &clash_ally_starter;
        if (pressed & PAD_DIRECTION_LEFT) *starter = 0;
        if (pressed & PAD_DIRECTION_RIGHT) *starter = 1;
        if (pressed & PAD_DIRECTION_HORIZONTAL_MASK) {
            SD_SEPlay(SE_MOVE, 255, 0);
            revision++;
        }
        if (pressed & PAD_BUTTON_CONFIRM_MASK) begin_tag_battle();
    } else if (clash_phase == 8) {
        int step = mini(elapsed / 250, 8);
        if (step != clash_cpu_step) {
            clash_cpu_step = step;
            revision++;
        }
        if (elapsed >= 2200) {
            if (tag_mode) {
                if (clash_first_side) clash_enemy_starter = rand() & 1;
                else clash_ally_starter = rand() & 1;
                clash_phase = 9;
                clash_phase_at = host->now_us(host);
                clash_cpu_step = 0;
                revision++;
            } else begin_solo_battle();
        }
    } else if (clash_phase == 9) {
        int step = mini(elapsed / 250, 8);
        if (step != clash_cpu_step) {
            clash_cpu_step = step;
            revision++;
        }
        if (elapsed >= 2200) {
            /* Only the first team's seat is chosen. ConfigureMatch derives
             * the next member of the waiting team from the fixed ring. */
            begin_tag_battle();
        }
    }
}

static void finish_draft_save(void)
{
    int ok = recipe_tab ? DeckMenu_SavePartnerRecipe(recipe_cursor, draft_name, edit_draft)
                        : DeckMenu_SavePlayerRecipeCards(recipe_cursor, edit_draft);
    if (!ok) { SD_SEPlay(SE_REFUSED, 255, 0); return; }
    recipe_save = 0;
    page = PAGE_RULES;
    SD_SEPlay(SE_CONFIRM, 255, 0);
    revision++;
}

static void update_confirm(unsigned pressed)
{
    if (pressed & PAD_BUTTON_CIRCLE) {
        if (ask_kind == 1) { ask_kind = 2; }
        else if (ask_kind == 2) { ask_kind = 1; }
        else { page = PAGE_RECIPES; }
        SD_SEPlay(SE_BACK, 255, 0); revision++;
        return;
    }
    if (!(pressed & PAD_BUTTON_CONFIRM_MASK)) return;
    if (ask_kind == 1) {
        recipe_save = 1;
        recipe_tab = !editing_player_recipe;
        recipe_cursor = 0;
        page = PAGE_RECIPES;
    } else if (ask_kind == 2) {
        recipe_save = 0; page = PAGE_RULES;
    } else if (ask_kind == 4) {
        finish_draft_save();
    } else {
        if (DeckMenu_EquipOwnedRecipe(selected_recipe, 1) < 0) {
            SD_SEPlay(SE_REFUSED, 255, 0); return;
        }
        begin_opening();
    }
    revision++;
}

static void update_recipes(unsigned pressed)
{
    int current_row = recipe_for_player && !recipe_save;
    int count = current_row ? 11 : 10;
    int slot, available;
    char name[96];
    if (!recipe_save && (pressed & PAD_BUTTON_L1_R1_MASK)) {
        recipe_tab ^= 1; recipe_cursor = 0;
        SD_SEPlay(SE_MOVE, 255, 0); revision++;
    }
    if (pressed & PAD_DIRECTION_UP) {
        recipe_cursor = (recipe_cursor + count - 1) % count;
        SD_SEPlay(SE_MOVE, 255, 0); revision++;
    } else if (pressed & PAD_DIRECTION_DOWN) {
        recipe_cursor = (recipe_cursor + 1) % count;
        SD_SEPlay(SE_MOVE, 255, 0); revision++;
    }
    if (pressed & PAD_BUTTON_CIRCLE) {
        if (recipe_save) { ask_kind = 1; page = PAGE_CONFIRM; }
        else page = PAGE_RULES;
        SD_SEPlay(SE_BACK, 255, 0); revision++;
        return;
    }
    slot = current_row ? recipe_cursor - 1 : recipe_cursor;
    if (recipe_save && (pressed & PAD_BUTTON_CONFIRM_MASK)) {
        available = recipe_tab ? DeckMenu_PartnerRecipeInfo(slot, NULL, 0) : slot_info(slot, NULL, 0);
        if (available) { ask_kind = 4; page = PAGE_CONFIRM; revision++; }
        else finish_draft_save();
        return;
    }
    if (!(pressed & (PAD_BUTTON_CONFIRM_MASK | PAD_BUTTON_TRIANGLE | PAD_BUTTON_SQUARE))) return;
    if (slot < 0) {
        if (pressed & PAD_BUTTON_CONFIRM_MASK) begin_opening();
        return;
    }
    available = recipe_tab ? DeckMenu_PartnerRecipeInfo(slot, name, sizeof(name))
                           : slot_info(slot, name, sizeof(name));
    if (!available || !(recipe_tab ? DeckMenu_CopyPartnerRecipe(slot, selected_recipe)
                                  : DeckMenu_CopySlot(slot, selected_recipe))) {
        SD_SEPlay(SE_REFUSED, 255, 0); return;
    }
    if (pressed & (PAD_BUTTON_TRIANGLE | PAD_BUTTON_SQUARE)) {
        begin_draft_edit(selected_recipe, name, !recipe_tab);
        return;
    }
    if (recipe_for_player) {
        int missing = DeckMenu_EquipOwnedRecipe(selected_recipe, 0);
        if (missing < 0) { SD_SEPlay(SE_REFUSED, 255, 0); return; }
        if (missing) { ask_kind = 3; page = PAGE_CONFIRM; revision++; return; }
        if (DeckMenu_EquipOwnedRecipe(selected_recipe, 1) < 0) {
            SD_SEPlay(SE_REFUSED, 255, 0); return;
        }
        begin_opening();
    } else {
        memcpy(partner_draft, selected_recipe, sizeof(partner_draft));
        partner_edited = 1;
        partner_deck_slot = slot + 1;
        partner_recipe_tab = recipe_tab;
        page = PAGE_RULES; rule_row = RULE_RECIPE;
        SD_SEPlay(SE_CONFIRM, 255, 0); revision++;
    }
}

static void update_rules(unsigned pressed)
{
    if(pressed & PAD_BUTTON_L1_R1_MASK) {
        extension_tab=0;extension_collect();
        if(extension_tab_count) {
            if(pressed & PAD_BUTTON_L1)extension_tab=extension_tab_count-1;
            extension_focus=extension_scroll=0;extension_collect();page=PAGE_EXTENSIONS;
            SD_SEPlay(SE_MOVE,255,0);revision++;return;
        }
    }
    int direction = !!(pressed & PAD_DIRECTION_RIGHT) -
                    !!(pressed & PAD_DIRECTION_LEFT);
    int visible = 5;
    if (pressed & PAD_DIRECTION_UP) {
        rule_row = next_rule(rule_row, -1);
        SD_SEPlay(SE_MOVE, 255, 0);
        revision++;
    } else if (pressed & PAD_DIRECTION_DOWN) {
        rule_row = next_rule(rule_row, 1);
        SD_SEPlay(SE_MOVE, 255, 0);
        revision++;
    }
    if (rule_row < rule_scroll) rule_scroll = rule_row;
    if (rule_row >= rule_scroll + visible)
        rule_scroll = rule_row - visible + 1;
    if (direction) {
        if (rule_row == RULE_CONTROL)
            partner_control = (partner_control + direction + CONTROL_COUNT) % CONTROL_COUNT;
        else if (rule_row == RULE_DECK)
            { partner_deck_source = (partner_deck_source + direction + DECK_COUNT) % DECK_COUNT; partner_edited = 0; }
        else if (rule_row == RULE_DRAW_POOL) hard_mode ^= 1;
        else if (rule_row == RULE_LIFE)
            starting_life = 4000 * (1 + (starting_life / 4000 - 1 + direction + 5) % 5);
        else if (rule_row == RULE_FIELD)
            starting_field = (starting_field + direction + 9) % 9;
        else if (rule_row == RULE_OPENING) opening_clash ^= 1;
        else direction = 0;
        if (direction) {
            SD_SEPlay(SE_MOVE, 255, 0);
            revision++;
        }
    }
    if (pressed & PAD_BUTTON_CIRCLE) {
        page = PAGE_GRID;
        if (tag_mode) pick_step = PICK_OPPONENT_TWO;
        SD_SEPlay(SE_BACK, 255, 0);
        revision++;
    } else if (pressed & PAD_BUTTON_CONFIRM_MASK) {
        if (rule_row == RULE_RECIPE && partner_deck_source == DECK_EDIT)
            begin_partner_edit();
        else if (rule_row == RULE_RECIPE && partner_deck_source == DECK_RECIPE) {
            recipe_save = 0; recipe_tab = 1;
            recipe_for_player = 0;
            recipe_cursor = partner_deck_slot - 1;
            page = PAGE_RECIPES;
            SD_SEPlay(SE_CONFIRM, 255, 0);
            revision++;
        } else if (rule_row == RULE_CONTINUE) {
            if (!tag_mode) begin_opening();
            else if (partner_deck_source == DECK_EDIT && !partner_edited)
                begin_partner_edit();
            else {
                recipe_save = 0; recipe_tab = 0;
                recipe_for_player = 1;
                recipe_cursor = active_slot ? active_slot() + 1 : 0;
                if (recipe_cursor < 0 || recipe_cursor > 10) recipe_cursor = 0;
                page = PAGE_PLAYER_DECK;
                SD_SEPlay(SE_CONFIRM, 255, 0);
                revision++;
            }
        }
    }
}

static void free_duel_runner(void)
{
    unsigned pressed, held, suppressed_pressed, suppressed_held;
    int id;
    if (!(D_8009B26C & 0x40)) {
        ((void (*)(void))original_free_duel)();
        return;
    }
    if(tag_mode && !TagDuel_Available()) { tag_mode=0;page=PAGE_GRID;revision++; }
    pressed = gInput_wPad1Pressed;
    held = gInput_wPad1Held;
    if (page == PAGE_MODE || page == PAGE_RULES || page == PAGE_CLASH ||
        page == PAGE_RECIPES || page == PAGE_PLAYER_DECK || page == PAGE_CONFIRM || page==PAGE_EXTENSIONS) {
        gInput_wPad1Pressed = 0;
        gInput_wPad1Held = 0;
        ((void (*)(void))original_free_duel)();
        gInput_wPad1Pressed = pressed;
        gInput_wPad1Held = held;
        if (resume_rules && (gFreeDuel_bScreenFlags & 0x20)) {
            finish_native_intro();
            resume_rules = 0;
        }
        if (page == PAGE_MODE) {
            if ((pressed & PAD_DIRECTION_VERTICAL_MASK) && TagDuel_Available()) {
                tag_mode ^= 1;
                SD_SEPlay(SE_MOVE, 255, 0);
                revision++;
            }
            if (pressed & PAD_BUTTON_CIRCLE) {
                page = PAGE_GRID;
                SD_SEPlay(SE_BACK, 255, 0);
                revision++;
            } else if (pressed & PAD_BUTTON_CONFIRM_MASK) {
                page = PAGE_GRID;
                pick_step = PICK_PARTNER;
                finish_native_intro();
                SD_SEPlay(SE_CONFIRM, 255, 0);
                revision++;
            }
        } else if (page == PAGE_RULES) update_rules(pressed);
        else if(page==PAGE_EXTENSIONS)update_extensions(pressed);
        else if (page == PAGE_CLASH) update_clash(pressed);
        else if (page == PAGE_CONFIRM) update_confirm(pressed);
        else update_recipes(pressed);
        return;
    }
    if ((pressed & PAD_BUTTON_TRIANGLE) && TagDuel_Available()) {
        gInput_wPad1Pressed = 0;
        gInput_wPad1Held = 0;
        ((void (*)(void))original_free_duel)();
        gInput_wPad1Pressed = pressed;
        gInput_wPad1Held = held;
        page = PAGE_MODE;
        SD_SEPlay(SE_CONFIRM, 255, 0);
        revision++;
        return;
    }
    suppressed_pressed = 0;
    suppressed_held = held;
    {
        /* Leave directions with the original selector. Only its confirm and
         * undo actions are redirected while Tag is collecting portraits. */
        suppressed_pressed = PAD_BUTTON_CONFIRM_MASK;
        if (tag_mode && pick_step > PICK_PARTNER)
            suppressed_pressed |= PAD_BUTTON_CIRCLE;
        gInput_wPad1Pressed = pressed & ~suppressed_pressed;
        if (pressed & suppressed_pressed) suppressed_held &= ~PAD_DIRECTION_MASK;
        gInput_wPad1Held = suppressed_held;
    }
    ((void (*)(void))original_free_duel)();
    gInput_wPad1Pressed = pressed;
    gInput_wPad1Held = held;
    if ((D_8009B26C & 0x1f) != MAIN_MODE_FREE_DUEL) return;
    if (!tag_mode) {
        if (pressed & PAD_BUTTON_CONFIRM_MASK) finish_native_intro();
        if ((pressed & PAD_BUTTON_CONFIRM_MASK) && grid_ready()) {
            solo_opponent = selected_grid_id();
            if (solo_opponent > 0) {
                rule_row = RULE_DRAW_POOL;
                rule_scroll = RULE_DRAW_POOL;
                page = PAGE_RULES;
                SD_SEPlay(SE_CONFIRM, 255, 0);
                revision++;
            }
        }
        return;
    }
    if ((pressed & PAD_BUTTON_CIRCLE) && pick_step > PICK_PARTNER) {
        pick_step--;
        SD_SEPlay(SE_BACK, 255, 0);
        revision++;
        return;
    }
    if (!(pressed & PAD_BUTTON_CONFIRM_MASK)) return;
    if (!grid_ready()) {
        SD_SEPlay(SE_REFUSED, 255, 0);
        return;
    }
    id = selected_grid_id();
    if (pick_step == PICK_PARTNER) { if (partner != id) partner_edited = 0; partner = id; }
    else if (pick_step == PICK_OPPONENT_ONE) opponent_1 = id;
    else opponent_2 = id;
    SD_SEPlay(SE_PORTRAIT, 255, 0);
    if (pick_step < PICK_DONE) pick_step++;
    if (pick_step == PICK_DONE) {
        rule_row = RULE_CONTROL;
        rule_scroll = RULE_CONTROL;
        page = PAGE_RULES;
    }
    revision++;
}

static void frame(void)
{
    DuelOptions_RegisterBuiltins(host);
    int mode = D_8009B26C & 0x1f;
    if (rematch_pending && mode == MAIN_MODE_FREE_DUEL &&
        (D_8009B26C & 0x40) && grid_ready()) {
        gFreeDuel_bTargetRow = opponent_2 / 5;
        gFreeDuel_bTargetColumn = opponent_2 % 5;
        pick_step = PICK_OPPONENT_TWO;
        page = PAGE_GRID;
        rematch_pending = 0;
        revision++;
    }
    if (edit_flow == EDIT_IDLE) return;
    if (mode == MAIN_MODE_BUILD_DECK) {
        D_8009B269 = MAIN_MODE_FREE_DUEL;
        edit_flow = EDIT_BUILDING;
        edit_menu_stable_frames = 0;
        return;
    }
    if (mode != MAIN_MODE_FREE_DUEL || !(D_8009B26C & 0x40)) return;
    if (++edit_menu_stable_frames < 20) return;
    if (DeckMenu_TakeTemporary(edit_draft)) {
        int i;
        if (!editing_player_recipe) {
            partner_edited = 1;
            for (i = 0; i < 40; i++) if (!edit_draft[i]) partner_edited = 0;
            memcpy(partner_draft, edit_draft, sizeof(partner_draft));
        }
        ask_kind = 1;
        page = PAGE_CONFIRM;
        edit_flow = EDIT_IDLE;
        resume_rules = 0;
        finish_native_intro();
        revision++;
    }
}

static void scene_event(MemoriesModEvent *event)
{
    if (event->phase != MEMORIES_BEFORE ||
        event->a != MAIN_MENU_SELECTION_FREE_DUEL) return;
    /* The base game now enters its own Free Duel grid unmodified. */
    if (!resume_rules) load_choices();
    load_portrait_bank();
    load_player_portrait();
    load_stone_texture();
    load_options_trim();
    load_game_font();
    load_button_icons();
    tag_mode = resume_rules ? 1 : 0;
    pick_step = resume_rules ? PICK_DONE : PICK_PARTNER;
    page = resume_rules ? PAGE_RULES : PAGE_GRID;
    if (resume_rules) {
        rule_row = RULE_RECIPE;
        rule_scroll = RULE_RECIPE;
    }
    revision++;
}

static void applied(int on)
{
    if (!on) {
        DuelMatch_Clear();
        DeckMenu_CancelTemporary();
        edit_flow = EDIT_IDLE;
        edit_menu_stable_frames = 0;
        page = PAGE_GRID;
        tag_mode = 0;
    }
}

static void shutdown_menu(void)
{
    DuelOptions_RemoveBuiltins(host);
    DuelOptions_Clear();
    DuelMatch_Clear();
    free(portrait_bank);
    portrait_bank = 0;
}

int MemoriesModInit(const MemoriesModHost *from, MemoriesMod *mod)
{
    int hook_token, scene_token;
    if (from->api < 4) return 0;
    host = from;
    TagOverlay_DrawPixels = (TagDrawPixels)host->symbol(host, "ModMenu_DrawPixelsV1");
    if (!TagOverlay_DrawPixels || !host->symbol(host, "ModMenu_BeginLogicalOverlay") ||
        !host->symbol(host, "ModMenu_EndLogicalOverlay") || !host->symbol(host, "Rank_ProjectSide")) {
        host->log(host, "Duel Options requires its companion runtime: missing drawing, logical-canvas or rank support");
        return 0;
    }
    TagClient_Init(host);
    migrate_choices();
    if(!DuelOptions_RegistryInit(host))return 0;
    mod->api = 4;
    mod->frame = frame;
    mod->applied = applied;
    mod->reset = NULL;
    mod->shutdown = shutdown_menu;
    mod->overlay = overlay;
    mod->overlay_signature = overlay_signature;
    request_edit_slot = (void (*)(int))host->symbol(host, "DeckMenu_RequestEditSlot");
    slot_info = (int (*)(int, char *, size_t))host->symbol(host, "DeckMenu_SlotInfo");
    active_slot = (int (*)(void))host->symbol(host, "DeckMenu_ActiveSlot");
    equip_slot = (int (*)(int))host->symbol(host, "DeckMenu_EquipSlot");
    hook_token = host->hook(host, (void *)Main_RunFreeDuelMenu,
                            (void *)free_duel_runner, &original_free_duel);
    scene_token = host->subscribe(host, MEMORIES_EVENT_SCENE, 100, scene_event);
    return hook_token != 0 && scene_token != 0 && DuelMatch_Init(host);
}


