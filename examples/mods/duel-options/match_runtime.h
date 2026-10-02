#ifndef DUEL_MATCH_RUNTIME_H
#define DUEL_MATCH_RUNTIME_H
#include "pc/mods/modapi.h"
int DuelMatch_Init(const MemoriesModHost *);
void DuelMatch_Configure(int,int,int,int,int,int);
void DuelMatch_Clear(void);
#endif
