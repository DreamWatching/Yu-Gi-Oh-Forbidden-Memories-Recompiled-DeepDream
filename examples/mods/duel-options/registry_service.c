#include "option_registry.h"
static const MemoriesModHost *host;
static int rows(unsigned mode,DuelOptionRow *out,int capacity)
{ return host->applied(host) ? DuelOptions_Rows(mode,out,capacity) : 0; }
static int set(const char *owner,const char *key,int value)
{ return host->applied(host) && DuelOptions_Set(owner,key,value); }
static int snapshot(unsigned mode,DuelOptionSnapshot *out,size_t size)
{ return host->applied(host) && DuelOptions_Snapshot(mode,out,size); }
static const DuelOptionsService service = {
    DUEL_OPTIONS_ABI,sizeof(DuelOptionsService),DuelOptions_Add,rows,set,snapshot,DuelOptions_Remove
};
static int match_value(const char *owner,const char *key,int *out)
{ return host->applied(host) && DuelOptions_MatchValue(owner,key,out); }
int DuelOptions_RegistryInit(const MemoriesModHost *from)
{
    if(from->api<4)return 0;
    host=from;
    return host->provide(host,"options_v1",(void *)&service) &&
           host->provide(host,"match_value_v1",(void *)match_value);
}
