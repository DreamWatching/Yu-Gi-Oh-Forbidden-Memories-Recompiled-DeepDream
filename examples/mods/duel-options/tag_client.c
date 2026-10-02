#include "tag_client.h"
#include "../shared/tag_duel_service.h"
static const MemoriesModHost *host;
void TagClient_Init(const MemoriesModHost *from) { host=from; }
static const TagDuelService *service(void)
{
    const TagDuelService *s=host->find(host,"tag-duel-menu:tag_v1");
    return s && s->abi==1 && s->size==sizeof(*s) && s->available && s->available() ? s : NULL;
}
int TagDuel_Available(void) { return service()!=NULL; }
int TagDuel_Start(int p,int a,int b,int c,int d,int slot,int hard)
{ const TagDuelService *s=service();return s && s->start ? s->start(p,a,b,c,d,slot,hard) : 0; }
int TagDuel_SignatureDeck(int id,unsigned short out[40])
{ const TagDuelService *s=service();return s && s->signature_deck ? s->signature_deck(id,out) : 0; }
void TagDuel_SetPartnerDeck(const unsigned short cards[40])
{ const TagDuelService *s=service();if(s && s->partner_deck)s->partner_deck(cards); }
