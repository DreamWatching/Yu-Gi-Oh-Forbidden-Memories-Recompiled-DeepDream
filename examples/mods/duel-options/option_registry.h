#ifndef DUEL_OPTION_REGISTRY_H
#define DUEL_OPTION_REGISTRY_H
#include "../shared/duel_options.h"
int DuelOptions_RegistryInit(const MemoriesModHost *);
int DuelOptions_Add(const MemoriesModHost *, const DuelOption *);
int DuelOptions_Rows(unsigned, DuelOptionRow *, int);
int DuelOptions_Set(const char *, const char *, int);
int DuelOptions_Snapshot(unsigned, DuelOptionSnapshot *, size_t);
void DuelOptions_Remove(const MemoriesModHost *);
void DuelOptions_Clear(void);
int DuelOptions_Freeze(unsigned);
int DuelOptions_MatchValue(const char *,const char *,int *);
void DuelOptions_ClearMatch(void);
#endif
