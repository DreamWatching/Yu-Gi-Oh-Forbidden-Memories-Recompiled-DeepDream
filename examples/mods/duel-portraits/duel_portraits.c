/* Independent presentation only: never hooks AI, decks, turns or rewards. */
#include "pc/mods/modapi.h"
#include "pc/saves/save_slots.h"
#include "pc/cards/tables.h"
#include "game/duel_side_state.h"
#include "game/ai_opponent_data.h"
#include "game/save_data.h"
#include "game/main_modes.h"
#include "game/duel_init_scene.h"
#include "game/duel_draw_status_numbers.h"
#include "../shared/duel_participants.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const MemoriesModHost *host;
typedef void (*DrawPixels)(const MemoriesModHost *,int,int,int,int,const uint32_t *,int,int,unsigned,unsigned);
static DrawPixels draw_pixels;
extern u8 D_8009B26C;
extern u8 D_8009B26E;
static DuelParticipants view;
static int duel_started;
static void *original_init_scene,*original_draw_lp;
static void init_scene(void)
{
    duel_started=0;
    ((void (*)(void))original_init_scene)();
}
static void draw_lp(DisplayObject *object)
{
    /* The first native LP draw, including its filling animation, opens the HUD.
     * Loading or editing decks cannot expose portraits from the previous duel. */
    if((D_8009B26C&31)==MAIN_MODE_DUEL && D_8009B26E==0x81)duel_started=1;
    ((void (*)(DisplayObject *))original_draw_lp)(object);
}
static uint32_t portraits[2][2][48*48];
static int portrait_keys[2] = {-999,-999};
static unsigned char *campaign_bank, *opponent_bank;
static uint64_t retry_at;

static void clear_cache(void)
{
    free(campaign_bank); free(opponent_bank);
    campaign_bank = opponent_bank = NULL;
    portrait_keys[0] = portrait_keys[1] = -999;
    retry_at = 0;
}

static int load_banks(void)
{
    int lba;
    if (campaign_bank && opponent_bank) return 1;
    if (host->now_us(host) < retry_at) return 0;
    lba = host->disc_file_start(host,"\\DATA\\WA_MRG.MRG;1");
    if (lba < 0) { retry_at=host->now_us(host)+1000000u;return 0; }
    campaign_bank = malloc(30 * 2048u);
    opponent_bank = malloc(48 * 2048u);
    if (!campaign_bank || !opponent_bank ||
        host->disc_read(host,lba+0x1e6a,30,campaign_bank)!=30 ||
        host->disc_read(host,lba+0x1eaa,48,opponent_bank)!=48) {
        clear_cache();
        retry_at=host->now_us(host)+1000000u; return 0;
    }
    return 1;
}
static uint32_t colour(unsigned word)
{
    unsigned r=word&31,g=(word>>5)&31,b=(word>>10)&31;
    return word ? 0xff000000u|((r<<3|r>>2)<<16)|((g<<3|g>>2)<<8)|(b<<3|b>>2) : 0;
}
static void decode(uint32_t *out,const unsigned char *record)
{
    int i; for(i=0;i<48*48;i++) {
        unsigned index=record[i]&63;
        unsigned word=record[0x900+index*2]|record[0x901+index*2]<<8;
        out[i]=colour(word);
    }
}
static int image(int slot,int key)
{
    if (!load_banks() || key < -1 || key >= 40) return 0;
    if (portrait_keys[slot]!=key) {
        decode(portraits[slot][0],key==-1 ? campaign_bank+8*0x980 : opponent_bank+key*0x980);
        if(key==-1) decode(portraits[slot][1],campaign_bank+9*0x980);
        else memcpy(portraits[slot][1],portraits[slot][0],sizeof(portraits[slot][0]));
        portrait_keys[slot]=key;
    }
    return 1;
}
static int retail_view(DuelParticipants *out)
{
    int side,two_player=gDuel_bOpponentID<0;
    memset(out,0,sizeof(*out));out->abi=1;out->size=sizeof(*out);
    out->active_side=D_8009B1D5;out->transition_side=-1;out->transition_progress=255;
    for(side=0;side<2;side++) {
        DuelParticipant *p=&out->side[side]; int i;
        p->identity=side;p->portrait=side && !two_player ? gDuel_bOpponentID : -1;
        if(side && !two_player) {
            const char *name=Tables_DuelistShortName(gDuel_bOpponentID);
            snprintf(p->name,sizeof(p->name),"%s",name ? name : "OPPONENT");
            snprintf(p->control,sizeof(p->control),"CPU");
        } else {
            const unsigned char *state=two_player ? D_801D1200+side*0x1000 :
                (const unsigned char *)&((SaveDataWorkspace *)D_801D0000)->state;
            SaveSlots_StateName(state,p->name,sizeof(p->name));
            if(!p->name[0]) snprintf(p->name,sizeof(p->name),"PLAYER %d",side+1);
            snprintf(p->control,sizeof(p->control),"PLAYER %d",side+1);
        }
        for(i=0;i<5;i++) p->hand+=D_800E9FF0[side].hand[i]>=0;
        p->deck=40-D_800E9FF0[side].deck_draw_cursor;
        if(p->deck<0)p->deck=0;
        if(p->deck>40)p->deck=40;
        out->previous[side]=*p;
    }
    return 1;
}
static int query(void)
{
    DuelParticipantsQuery provider;
    /* Main_RunDuel state 0 is the preparation chest, not a live duel.
     * State 1 becomes initialized before the native LP/draw startup begins. */
    if(!host->applied(host) || (D_8009B26C&31)!=MAIN_MODE_DUEL ||
       !(D_8009B26C&0x40) || D_8009B26E!=0x81 || !duel_started) return 0;
    provider=(DuelParticipantsQuery)host->find(host,"tag-duel-menu:participants_v1");
    memset(&view,0,sizeof(view));
    if(!provider || !provider(&view,sizeof(view))) retail_view(&view);
    if(view.abi!=1 || view.size!=sizeof(view) || view.active_side<0 || view.active_side>1) return 0;
    if(view.transition_progress>255)view.transition_progress=255;
    for(int side=0;side<2;side++) {
        view.side[side].name[63]=view.previous[side].name[63]=0;
        view.side[side].control[23]=view.previous[side].control[23]=0;
    }
    return 1;
}
static void centred_text(int x, int width, int middle, const char *text,
                         uint32_t colour)
{
    int text_width = host->text_width(host, text, 1);
    host->draw_text(host, x + (width - text_width) / 2, middle,
                        text, colour, 1);
}
static void portrait_frame(int x, int y, int size, uint32_t colour, int strong)
{
    int border = strong ? 4 : 2;
    if (strong) host->fill(host, x - 8, y - 8, size + 16, size + 16,
                               colour, 48);
    host->fill(host, x - border, y - border,
                   size + border * 2, size + border * 2, colour, 255);
    host->fill(host, x, y, size, size, 0x07101c, 255);
}
static void draw_portrait(const DuelParticipant *p,int slot,int active,int x,int y,int size,unsigned brightness,unsigned alpha)
{
    if(image(slot,p->portrait)) draw_pixels(host,x,y,size,size,portraits[slot][active!=0],48,48,brightness,alpha);
}
static void draw_duelist_panel(int side, int x, int y, int width, int height)
{
    int active = side == view.active_side;
    const DuelParticipant *member = &view.side[side];
    int portrait_size = width - 34;
    int portrait_x, portrait_y;
    int hand, deck;
    char stats[48];
    uint32_t team_colour = side ? 0xc94b55 : 0x3bb6e8;
    uint32_t frame_colour = active ? 0xffd35a : team_colour;
    uint32_t name_colour = active ? 0xfff0ad : 0xe3e8ef;
    if (portrait_size > 112) portrait_size = 112;
    if (portrait_size < 52) portrait_size = 52;
    portrait_x = x + (width - portrait_size) / 2;
    portrait_y = y + 28;
    host->fill(host, x, y, width, height, 0x050b14, 218);
    host->fill(host, x, y, 3, height, team_colour, active ? 255 : 150);
    host->fill(host, x + width - 3, y, 3, height,
                   team_colour, active ? 255 : 150);
    centred_text(x, width, y + 14, active ? "ACTIVE TURN" : "NEXT DUELIST",
                   active ? 0xffd35a : 0x8d9aae);
    portrait_frame(portrait_x, portrait_y, portrait_size, frame_colour, active);

    if (view.transition_side == side && view.transition_progress < 255 &&
        view.previous[side].identity != member->identity) {
        unsigned progress = view.transition_progress;
        int old_shift = (int)progress * (side ? 18 : -18) / 255;
        int new_shift = (int)(255u - progress) * (side ? -18 : 18) / 255;
        draw_portrait(&view.previous[side], side, active,
                      portrait_x + old_shift,
                      portrait_y, portrait_size, active ? 255 : 150,
                      255u - progress);
        draw_portrait(member, side, active, portrait_x + new_shift, portrait_y,
                      portrait_size, active ? 255 : 150, progress);
        host->fill(host, portrait_x - 5, portrait_y - 5,
                       portrait_size + 10, portrait_size + 10,
                       frame_colour, progress < 128 ? progress : 255 - progress);
    } else {
        draw_portrait(member, side, active, portrait_x, portrait_y, portrait_size,
                      active ? 255 : 145, 255);
    }

    centred_text(x, width, portrait_y + portrait_size + 13,
                  member->name, name_colour);
    centred_text(x, width, portrait_y + portrait_size + 31,
                  member->control, active ? 0xffffff : 0x8d9aae);
    hand=member->hand;deck=member->deck;
    snprintf(stats, sizeof(stats), "HAND %d   DECK %d", hand, deck);
    centred_text(x, width, y + height - 12, stats,
                  active ? 0xdff6ff : 0x7d899b);
}
static unsigned signature(void)
{
    unsigned value=2166136261u;const unsigned char *bytes;
    if(!query())return 0;
    bytes=(const unsigned char *)&view;
    for(size_t i=0;i<sizeof(view);i++)value=(value^bytes[i])*16777619u;
    /* Failed reads must retry even when the duel state itself is unchanged. */
    if(!campaign_bank || !opponent_bank)value^=(unsigned)(host->now_us(host)/1000000u);
    return value ? value : 1;
}
static void overlay(void)
{
    int width,height,scale,game_width,margin,panel_width,panel_height;
    if(!query())return;
    host->overlay_size(host,&width,&height,&scale);
    if(width<=0 || height<=0)return;
    game_width=height*4/3;if(game_width>width)game_width=width;
    margin=(width-game_width)/2;if(margin<64)return;
    panel_width=margin-8;if(panel_width>170)panel_width=170;
    panel_height=208;if(panel_height>height/3)panel_height=height/3;
    draw_duelist_panel(0,(margin-panel_width)/2,height-panel_height,panel_width,panel_height);
    draw_duelist_panel(1,width-margin+(margin-panel_width)/2,height-panel_height,panel_width,panel_height);
}
static int available(void) { return host && host->applied(host); }
static void reset(void) { duel_started=0;clear_cache(); }
static void applied(int on)
{
    clear_cache();
    duel_started=on && (D_8009B26C&31)==MAIN_MODE_DUEL && D_8009B26E==0x81 &&
        (D_800E9FF0[0].displayed_life_points>0 || D_800E9FF0[1].displayed_life_points>0);
}
static void frame(void)
{
    /* A new scene/disc will load fresh art, with no file lookup every frame. */
    int mode=D_8009B26C&31;
    if(mode!=MAIN_MODE_DUEL && mode!=MAIN_MODE_ANIMATED_BATTLE &&
       (campaign_bank || opponent_bank))clear_cache();
}
int MemoriesModInit(const MemoriesModHost *from,MemoriesMod *mod)
{
    if(from->api<4)return 0;
    host=from;
    draw_pixels=(DrawPixels)host->symbol(host,"ModMenu_DrawPixelsV1");
    if(!draw_pixels) { host->log(host,"Duel Portraits needs the companion pixel-drawing service");return 0; }
    mod->api=4;mod->overlay=overlay;mod->overlay_signature=signature;
    mod->frame=frame;
    mod->applied=applied;mod->reset=reset;mod->shutdown=clear_cache;
    return host->register_state(host,&duel_started,sizeof(duel_started),1) &&
           host->hook(host,(void *)Duel_InitScene,(void *)init_scene,&original_init_scene) &&
           host->hook(host,(void *)Duel_DrawLifePointsAndDeckCounts,(void *)draw_lp,&original_draw_lp) &&
           host->provide(host,"available_v1",(void *)available);
}
