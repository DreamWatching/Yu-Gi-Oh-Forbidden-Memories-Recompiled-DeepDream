#define _POSIX_C_SOURCE 200809L
#include "partner_recipes.h"
#include "pc/cards/cards.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "pc/compat/posix.h"

#define HEADER 24
#define LEGACY_BYTES (8 + 10 * 84 + 10 * 64)
static const unsigned char magic[8] = {'Y','F','M','T','A','G','2',0};
static unsigned get16(const unsigned char *p) { return p[0] | p[1] << 8; }
static uint32_t get32(const unsigned char *p) { return get16(p) | (uint32_t)get16(p + 2) << 16; }
static void put32(unsigned char *p, uint32_t n) { int i; for (i=0;i<4;i++) p[i]=(unsigned char)(n >> (i*8)); }
static uint32_t checksum(const unsigned char *p, unsigned n)
{ uint32_t h=2166136261u; while(n--) h=(h ^ *p++)*16777619u; return h; }

static int layout_valid(const PartnerRecipes *s)
{
    int slot, i;
    for(slot=0;slot<10;slot++) {
        const unsigned char *p=s->bytes+slot*PARTNER_RECIPE_SLOT_BYTES;
        if(p[0]>1 || !memchr(p+1,0,64)) return 0;
        if(!p[0]) continue;
        for(i=0;i<40;i++) {
            const unsigned char *card=p+65+i*130;
            unsigned id=get16(card);
            if(!memchr(card+2,0,128) || (id && (id>CARD_COUNT || card[2])) || (!id && !card[2])) return 0;
        }
    }
    return 1;
}

int PartnerRecipes_Get(const PartnerRecipes *s, int slot, DeckSlot *deck, char name[64])
{
    const unsigned char *p;
    int i,j;
    memset(deck,0,sizeof(*deck));
    if(name) name[0]=0;
    if(slot<0 || slot>=10) return 0;
    p=s->bytes+slot*PARTNER_RECIPE_SLOT_BYTES;
    if(!p[0]) return 0;
    if(name) { memcpy(name,p+1,64); name[63]=0; }
    for(i=0;i<40;i++) {
        const unsigned char *card=p+65+i*130;
        int id=get16(card), copies=1;
        if(!id) id=Cards_FindIdentity((const char *)card+2);
        if(!Cards_Valid(id)) return 0;
        for(j=0;j<i;j++) if(deck->cards[j]==id) copies++;
        if(copies>DECK_SLOT_COPIES_MAX) return 0;
        deck->cards[i]=(unsigned short)id;
    }
    deck->used=1;
    return 1;
}

int PartnerRecipes_Read(const char *path,uint32_t owner,PartnerRecipes *out)
{
    unsigned char *raw;
    FILE *file=fopen(path,"rb");
    size_t n;
    int slot,i,ok=-1;
    memset(out,0,sizeof(*out));
    if(!file) return errno==ENOENT ? 0 : -1;
    raw=malloc(HEADER+PARTNER_RECIPE_PAYLOAD+1);
    if(!raw) { fclose(file); return -1; }
    n=fread(raw,1,HEADER+PARTNER_RECIPE_PAYLOAD+1,file);
    if(ferror(file)) n=0;
    fclose(file);
    if(n==HEADER+PARTNER_RECIPE_PAYLOAD && !memcmp(raw,magic,8) && get32(raw+8)==2 &&
       get32(raw+12)==owner && get32(raw+16)==PARTNER_RECIPE_PAYLOAD &&
       get32(raw+20)==checksum(raw+HEADER,PARTNER_RECIPE_PAYLOAD)) {
        memcpy(out->bytes,raw+HEADER,PARTNER_RECIPE_PAYLOAD);
        if(layout_valid(out)) ok=1;
    } else if(n==LEGACY_BYTES && get32(raw)==0x54414731u && get32(raw+4)==owner) {
        for(slot=0;slot<10;slot++) {
            unsigned char *p=out->bytes+slot*PARTNER_RECIPE_SLOT_BYTES;
            const unsigned char *old=raw+8+slot*84;
            DeckSlot deck;
            if(get32(old)!=1) continue;
            p[0]=1; memcpy(p+1,raw+8+10*84+slot*64,63);
            for(i=0;i<40;i++) {
                unsigned id=get16(old+4+i*2);
                if(!id || id>CARD_COUNT) {p[0]=0; break;}
                p[65+i*130]=(unsigned char)id; p[66+i*130]=(unsigned char)(id>>8);
            }
            if(p[0] && !PartnerRecipes_Get(out,slot,&deck,NULL)) p[0]=0;
        }
        ok=1; /* Migration is in memory only until an explicit Save. */
    }
    free(raw);
    if(ok<0) memset(out,0,sizeof(*out));
    return ok;
}

static int write_synced(const char *path,const unsigned char *bytes,size_t size)
{
    FILE *f=fopen(path,"wb"); int failed;
    if(!f) return 0;
    failed=fwrite(bytes,1,size,f)!=size;
    if(fflush(f) || fsync(fileno(f))) failed=1;
    if(fclose(f)) failed=1;
    return !failed;
}

int PartnerRecipes_Save(const char *path,uint32_t owner,PartnerRecipes *store,
                        int slot,const char *name,const unsigned short cards[40])
{
    PartnerRecipes *next;
    unsigned char *raw,*p;
    char partial[1100],backup[1100],backup_partial[1100];
    int i,j,ok=0,read_result;
    if(slot<0 || slot>=10 || !name || strlen(name)>=64) return 0;
    if(snprintf(partial,sizeof(partial),"%s.partial",path)>=(int)sizeof(partial) ||
       snprintf(backup,sizeof(backup),"%s.bak",path)>=(int)sizeof(backup) ||
       snprintf(backup_partial,sizeof(backup_partial),"%s.bak.partial",path)>=(int)sizeof(backup_partial)) return 0;
    next=malloc(sizeof(*next)); raw=malloc(HEADER+PARTNER_RECIPE_PAYLOAD+1);
    if(!next || !raw) {free(next);free(raw);return 0;}
    /* Re-read to preserve recipes whose card mods are currently absent. Never
     * silently replace a corrupt or future-format file with empty recipes. */
    read_result=PartnerRecipes_Read(path,owner,next);
    if(read_result<0) goto done;
    p=next->bytes+slot*PARTNER_RECIPE_SLOT_BYTES;
    memset(p,0,PARTNER_RECIPE_SLOT_BYTES);p[0]=1;memcpy(p+1,name,strlen(name));
    for(i=0;i<40;i++) {
        const char *identity;
        int copies=1;
        if(!Cards_Valid(cards[i])) goto done;
        for(j=0;j<i;j++) if(cards[j]==cards[i]) copies++;
        if(copies>DECK_SLOT_COPIES_MAX) goto done;
        if(cards[i]<=CARD_COUNT) {
            p[65+i*130]=(unsigned char)cards[i];p[66+i*130]=(unsigned char)(cards[i]>>8);
        } else {
            identity=Cards_Identity(cards[i]);
            if(!identity || !*identity || strlen(identity)>=128) goto done;
            memcpy(p+67+i*130,identity,strlen(identity));
        }
    }
    /* Retain the previous valid file as a recoverable backup before replace. */
    if(read_result>0) {
        FILE *old=fopen(path,"rb"); size_t n;
        if(!old) goto done;
        n=fread(raw,1,HEADER+PARTNER_RECIPE_PAYLOAD+1,old);
        if(ferror(old)) n=0;
        fclose(old);
        if(!n || n>HEADER+PARTNER_RECIPE_PAYLOAD || !write_synced(backup_partial,raw,n)) goto done;
        if(rename(backup_partial,backup)) goto done;
    }
    memcpy(raw,magic,8);put32(raw+8,2);put32(raw+12,owner);put32(raw+16,PARTNER_RECIPE_PAYLOAD);
    memcpy(raw+HEADER,next->bytes,PARTNER_RECIPE_PAYLOAD);
    put32(raw+20,checksum(next->bytes,PARTNER_RECIPE_PAYLOAD));
    if(!write_synced(partial,raw,HEADER+PARTNER_RECIPE_PAYLOAD) || rename(partial,path)) goto done;
    *store=*next;ok=1;
done:
    remove(partial);remove(backup_partial);free(next);free(raw);return ok;
}
