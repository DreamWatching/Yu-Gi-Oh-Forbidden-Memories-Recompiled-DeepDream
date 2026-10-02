#include "option_registry.h"
#include <string.h>
typedef struct { const MemoriesModHost *owner; DuelOption option; } Entry;
static Entry entries[DUEL_OPTION_MAX];
static int count;
static DuelOptionSnapshot confirmed;
static int terminated(const char *s,size_t size) { return memchr(s,0,size)!=NULL; }
static int key_valid(const char *s,size_t size)
{
    if(!s[0])return 0;
    for(size_t i=0;i<size;i++) {
        char c=s[i];if(!c)return 1;
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
             (c>='0'&&c<='9')||c=='_'||c=='-'))return 0;
    }
    return 0;
}
static int choice(const DuelOption *o,int value)
{
    for(int i=0;i<o->choice_count;i++)if(o->choices[i].value==value)return 1;
    return 0;
}
static int mode_valid(unsigned mode)
{ return mode==DUEL_OPTION_SOLO || mode==DUEL_OPTION_TAG || mode==DUEL_OPTION_TWO_PLAYER; }
int DuelOptions_Add(const MemoriesModHost *owner,const DuelOption *o)
{
    if(!owner || !owner->id || !key_valid(owner->id,64) || !owner->applied ||
       !owner->setting || !owner->set_setting || !o || o->abi!=1 || o->size!=sizeof(*o) ||
       !o->modes || (o->modes&~7u) || !key_valid(o->key,sizeof(o->key)) ||
       !terminated(o->tab,sizeof(o->tab)) || !o->tab[0] ||
       !terminated(o->label,sizeof(o->label)) || !o->label[0] ||
       !terminated(o->description,sizeof(o->description)) ||
       o->choice_count<1 || o->choice_count>DUEL_CHOICE_MAX ||
       !choice(o,o->default_value) || count==DUEL_OPTION_MAX)return 0;
    /* Reserved host settings must never become editable duel options. */
    if(!strcmp(o->key,"order"))return 0;
    for(int i=0;i<o->choice_count;i++) {
        if(!terminated(o->choices[i].label,sizeof(o->choices[i].label)) || !o->choices[i].label[0])return 0;
        for(int j=0;j<i;j++)if(o->choices[i].value==o->choices[j].value)return 0;
    }
    for(int i=0;i<count;i++)if(!strcmp(entries[i].owner->id,owner->id) &&
                             !strcmp(entries[i].option.key,o->key))return 0;
    entries[count].owner=owner;entries[count++].option=*o;return 1;
}
static int current(const Entry *e)
{
    int value=e->owner->setting(e->owner,e->option.key,e->option.default_value);
    return choice(&e->option,value) ? value : e->option.default_value;
}
int DuelOptions_Rows(unsigned mode,DuelOptionRow *out,int capacity)
{
    int n=0;
    if(!mode_valid(mode) || capacity<0 || (capacity && !out))return -1;
    for(int i=0;i<count;i++) {
        Entry *e=&entries[i];
        if(!(e->option.modes&mode) || !e->owner->applied(e->owner))continue;
        if(n<capacity) {
            memset(&out[n],0,sizeof(out[n]));
            memcpy(out[n].owner,e->owner->id,strlen(e->owner->id)+1);
            out[n].option=e->option;out[n].value=current(e);
        }
        n++;
    }
    return n; /* Total count; caller can detect insufficient capacity. */
}
int DuelOptions_Set(const char *owner,const char *key,int value)
{
    if(!owner || !key)return 0;
    for(int i=0;i<count;i++) {
        Entry *e=&entries[i];
        if(strcmp(e->owner->id,owner) || strcmp(e->option.key,key))continue;
        if(!e->owner->applied(e->owner) || !choice(&e->option,value))return 0;
        e->owner->set_setting(e->owner,e->option.key,value);return 1;
    }
    return 0;
}
int DuelOptions_Snapshot(unsigned mode,DuelOptionSnapshot *out,size_t size)
{
    if(!out || size<sizeof(*out) || !mode_valid(mode))return 0;
    memset(out,0,sizeof(*out));out->abi=1;out->size=sizeof(*out);out->mode=mode;
    out->count=DuelOptions_Rows(mode,out->rows,DUEL_OPTION_MAX);return out->count>=0;
}
void DuelOptions_Remove(const MemoriesModHost *owner)
{
    for(int i=0;i<count;) {
        if(entries[i].owner!=owner) { i++;continue; }
        memmove(entries+i,entries+i+1,(count-i-1)*sizeof(*entries));count--;
    }
}
int DuelOptions_Freeze(unsigned mode)
{ return DuelOptions_Snapshot(mode,&confirmed,sizeof(confirmed)); }
int DuelOptions_MatchValue(const char *owner,const char *key,int *out)
{
    if(!owner || !key || !out)return 0;
    for(int i=0;i<confirmed.count;i++) {
        DuelOptionRow *row=&confirmed.rows[i];
        if(!strcmp(owner,row->owner) && !strcmp(key,row->option.key)) { *out=row->value;return 1; }
    }
    return 0;
}
void DuelOptions_ClearMatch(void) { memset(&confirmed,0,sizeof(confirmed)); }
void DuelOptions_Clear(void) { memset(entries,0,sizeof(entries));count=0;DuelOptions_ClearMatch(); }
