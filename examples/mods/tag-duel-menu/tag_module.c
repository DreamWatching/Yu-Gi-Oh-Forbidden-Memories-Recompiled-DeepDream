#include "tag_duel_runtime.h"
#include "../shared/tag_duel_service.h"
#include "pc/mods/modapi.h"
static const MemoriesModHost *host;
extern int TagDuel_RuntimeInit(const MemoriesModHost *);
extern void TagDuel_RuntimeShutdown(void);
extern int TagRewards_Init(const MemoriesModHost *);
extern unsigned TagRewards_Signature(void);
extern void TagRewards_Draw(void);
extern void TagRewards_Reset(void);
static void reset(void) { TagDuel_ResetHud();TagRewards_Reset(); }
static void overlay(void) { TagDuel_DrawHud();TagRewards_Draw(); }
static unsigned signature(void) { return TagDuel_HudSignature()*31u+TagRewards_Signature(); }
static int available(void) { return host->applied(host); }
static const TagDuelService service={1,sizeof(TagDuelService),available,TagDuel_Start,TagDuel_SignatureDeck,TagDuel_SetPartnerDeck,TagDuel_ResetHud};
int MemoriesModInit(const MemoriesModHost *from,MemoriesMod *mod)
{
    if(from->api<4)return 0;host=from;
    mod->api=4;mod->reset=reset;mod->shutdown=TagDuel_RuntimeShutdown;
    mod->overlay=overlay;mod->overlay_signature=signature;
    return TagDuel_RuntimeInit(host) && TagRewards_Init(host) && host->provide(host,"tag_v1",(void *)&service);
}
