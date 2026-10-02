"""Compile the actual recipe codec; inject rename failure at the OS boundary."""
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'tmp/pc/partner-recipes-test'
OUT.mkdir(parents=True, exist_ok=True)
source = (ROOT / 'src/pc/saves/partner_recipes.c').read_text()
# Use the native CRT in the fixture; production uses the UTF-8 OS adapter.
source = source.replace('#include "pc/compat/posix.h"', '''#include <io.h>
#define fsync _commit
static int fail_replace, fail_sync;
static int sync_file(int fd) {return fail_sync ? -1 : _commit(fd);}
#undef fsync
#define fsync sync_file
static int replace_file(const char *a, const char *b) {
 if (fail_replace && !strstr(a,".bak.partial")) return -1;
 /* Fixture emulates the port's replace-existing rename adapter. */
 remove(b); return rename(a,b);
}
#define rename replace_file''')
prefix = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
static int custom_id=800;
int Cards_Valid(int id) {return id>0 && (id<=722 || id==custom_id);}
const char *Cards_Identity(int id) {return id==custom_id ? "fixture:dragon" : "";}
int Cards_FindIdentity(const char *s) {return !strcmp(s,"fixture:dragon") ? custom_id : 0;}
'''
tests = r'''
int main(void) {
 PartnerRecipes store,read;DeckSlot deck;char name[64];unsigned short cards[40];
 unsigned char legacy[LEGACY_BYTES]={0},*saved;FILE *f;int i;long bytes;
 const char *path="recipes.bin";
 remove(path);remove("recipes.bin.bak");remove("recipes.bin.partial");
 assert(PartnerRecipes_Read(path,123,&store)==0);
 for(i=0;i<40;i++) cards[i]=i+1;
 cards[0]=800;
 assert(PartnerRecipes_Save(path,123,&store,0,"Heishin's Deck",cards));
 assert(PartnerRecipes_Read(path,123,&read)==1);
 assert(PartnerRecipes_Get(&read,0,&deck,name) && deck.cards[0]==800);
 assert(!strcmp(name,"Heishin's Deck"));
 custom_id=900;
 assert(PartnerRecipes_Get(&read,0,&deck,name) && deck.cards[0]==900);
 custom_id=0;
 assert(!PartnerRecipes_Get(&read,0,&deck,name));
 for(i=0;i<40;i++) cards[i]=i+1;
 assert(PartnerRecipes_Save(path,123,&read,1,"Joey",cards));
 custom_id=950;assert(PartnerRecipes_Read(path,123,&read)==1);
 assert(PartnerRecipes_Get(&read,0,&deck,name) && deck.cards[0]==950);
 assert(PartnerRecipes_Get(&read,1,&deck,name));
 assert(PartnerRecipes_Read(path,124,&store)==-1);
 f=fopen(path,"rb");fseek(f,0,SEEK_END);bytes=ftell(f);rewind(f);
 saved=malloc(bytes);assert(fread(saved,1,bytes,f)==bytes);fclose(f);
 store=read; fail_replace=1;
 assert(!PartnerRecipes_Save(path,123,&read,1,"Overwrite fails",cards));
 assert(!memcmp(&store,&read,sizeof(store)));
 f=fopen(path,"rb");for(i=0;i<bytes;i++) assert(fgetc(f)==saved[i]);fclose(f);
 fail_replace=0;
 /* Real filesystem failures before replace must preserve disk and cache. */
 for(int failure=0;failure<3;failure++) {
  const char *blocked=failure==0?"recipes.bin.partial":"recipes.bin.bak.partial";
  if(failure<2) assert(!_mkdir(blocked));else fail_sync=1;
  store=read;
  assert(!PartnerRecipes_Save(path,123,&read,1,"Write fails",cards));
  assert(!memcmp(&store,&read,sizeof(store)));
  f=fopen(path,"rb");for(i=0;i<bytes;i++)assert(fgetc(f)==saved[i]);assert(fgetc(f)==EOF);fclose(f);
  if(failure<2)assert(!_rmdir(blocked));else fail_sync=0;
 }
 puts("PASS: failed partial open, backup open, sync and replace retain previous file and memory");
 /* Valid overwrite persists only the chosen slot and backs up the prior file. */
 assert(PartnerRecipes_Save(path,123,&read,1,"Joey Updated",cards));
 assert(PartnerRecipes_Read(path,123,&store)==1);
 assert(PartnerRecipes_Get(&store,1,&deck,name) && !strcmp(name,"Joey Updated"));
 assert(PartnerRecipes_Get(&store,0,&deck,name) && deck.cards[0]==950);
 f=fopen("recipes.bin.bak","rb");for(i=0;i<bytes;i++)assert(fgetc(f)==saved[i]);fclose(f);
 puts("PASS: valid overwrite replaces only its slot and preserves byte-exact previous backup");
 assert(PartnerRecipes_Read("recipes.bin.bak",123,&store)==1);
 cards[0]=cards[1]=cards[2]=cards[3]=1;
 assert(!PartnerRecipes_Save(path,123,&read,1,"Four copies",cards));
 cards[0]=0;assert(!PartnerRecipes_Save(path,123,&read,1,"Invalid",cards));
 /* Corruption and extra bytes cannot be overwritten by Save. */
 saved[HEADER+90]^=1;f=fopen(path,"wb");fwrite(saved,1,bytes,f);fclose(f);
 assert(PartnerRecipes_Read(path,123,&store)==-1);
 for(i=0;i<40;i++)cards[i]=i+1;
 assert(!PartnerRecipes_Save(path,123,&read,1,"Corrupt",cards));
 saved[HEADER+90]^=1;f=fopen(path,"wb");fwrite(saved,1,bytes,f);fputc(0,f);fclose(f);
 assert(PartnerRecipes_Read(path,123,&store)==-1);
 /* Legacy retail recipes migrate; ambiguous numeric mod IDs do not. */
 put32(legacy,0x54414731u);put32(legacy+4,123);put32(legacy+8,1);
 for(i=0;i<40;i++){legacy[12+2*i]=i+1;}
 strcpy((char *)legacy+8+10*84,"Old deck");
 f=fopen(path,"wb");fwrite(legacy,1,sizeof(legacy),f);fclose(f);
 assert(PartnerRecipes_Read(path,123,&read)==1);
 assert(PartnerRecipes_Get(&read,0,&deck,name));
 assert(PartnerRecipes_Save(path,123,&read,1,"Migrated",cards));
 f=fopen("recipes.bin.bak","rb");assert(fread(legacy,1,sizeof(legacy),f)==sizeof(legacy));assert(fgetc(f)==EOF);fclose(f);
 legacy[12]=0x20;legacy[13]=3;
 f=fopen(path,"wb");fwrite(legacy,1,sizeof(legacy),f);fclose(f);
 assert(PartnerRecipes_Read(path,123,&read)==1 && !PartnerRecipes_Get(&read,0,&deck,name));
 free(saved);
 puts("PASS: schema/checksum/owner, stable IDs, removed mods, migration, backup, failed replace, invalid cards and copy limits");
 return 0;
}
'''
c = OUT / 'test.c'
c.write_text(prefix + source + tests)
exe = OUT / 'test.exe'
subprocess.run([shutil.which('i686-w64-mingw32-clang'), '-std=gnu11', '-O2',
               '-I'+str(ROOT/'src'), '-I'+str(ROOT/'src/pc/saves'), str(c), '-o', str(exe)], check=True)
subprocess.run([str(exe)], cwd=OUT, check=True)
