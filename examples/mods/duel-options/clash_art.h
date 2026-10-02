#ifndef TAG_CLASH_ART_H
#define TAG_CLASH_ART_H
#include "pc/mods/modapi.h"
extern const unsigned int *clash_backs[10], *clash_fronts[10];
extern unsigned int clash_crossed_swords[6400];
int ClashArt_Load(const MemoriesModHost *host);
#endif
