"""Exercise the real one-time legacy preference migration without user files."""
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'tmp/pc/duel-preferences-test'
OUT.mkdir(parents=True,exist_ok=True)
source=(ROOT/'examples/mods/duel-options/tag_duel_menu.c').read_text()
start=source.index('static void migrate_choices(void)')
brace=source.index('{',start);end=brace+1;depth=1
while depth:
    depth+=(source[end]=='{')-(source[end]=='}');end+=1
fixture=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pc/mods/modapi.h"
static int migrated,lp=-2147483647,partner=6,writes,missing_symbol;
static int legacy(const char *key,int fallback) {
 if(!strcmp(key,"mod.tag-duel-menu.starting_life"))return 16000;
 if(!strcmp(key,"mod.tag-duel-menu.partner"))return 3;
 return fallback;
}
static void *symbol(const MemoriesModHost *h,const char *key) {
 (void)h;assert(!strcmp(key,"Settings_GetNamed"));return missing_symbol?NULL:(void*)legacy;
}
static int setting(const MemoriesModHost *h,const char *key,int fallback) {
 (void)h;
 if(!strcmp(key,"choices_migrated"))return migrated;
 if(!strcmp(key,"starting_life"))return lp;
 if(!strcmp(key,"partner"))return partner;
 return fallback;
}
static void set(const MemoriesModHost *h,const char *key,int value) {
 (void)h;writes++;
 if(!strcmp(key,"choices_migrated"))migrated=value;
 else {assert(!strcmp(key,"starting_life"));lp=value;}
}
static MemoriesModHost test_host;
static const MemoriesModHost *host=&test_host;
'''
tests=r'''
int main(void) {
 test_host.symbol=symbol;test_host.setting=setting;test_host.set_setting=set;
 missing_symbol=1;migrate_choices();assert(writes==0 && !migrated);
 missing_symbol=0;migrate_choices();assert(migrated && lp==16000 && partner==6 && writes==2);
 lp=12000;migrate_choices();assert(lp==12000 && writes==2);
 assert(legacy("mod.tag-duel-menu.starting_life",0)==16000);
 puts("PASS: migration copies missing settings once, preserves existing/new preferences and legacy values");
 return 0;
}
'''
c=OUT/'test.c';c.write_text(fixture+source[start:end]+tests)
exe=OUT/'test.exe'
subprocess.run([shutil.which('i686-w64-mingw32-clang'),'-std=gnu11','-O2',
               '-I'+str(ROOT/'src'),str(c),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
