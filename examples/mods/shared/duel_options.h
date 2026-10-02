#ifndef MEMORIES_DUEL_OPTIONS_V1_H
#define MEMORIES_DUEL_OPTIONS_V1_H
#include "pc/mods/modapi.h"
/* Experimental registration contract. Not yet a supported public SDK.
 * Settings belong to the contributing mod. The framework only presents them.
 * Numeric choices use actual values, never positions in an array. */
#define DUEL_OPTIONS_ABI 1u
#define DUEL_OPTION_MAX 64
#define DUEL_CHOICE_MAX 16
enum { DUEL_OPTION_SOLO=1, DUEL_OPTION_TAG=2, DUEL_OPTION_TWO_PLAYER=4 };
typedef struct { int value; char label[40]; } DuelOptionChoice;
typedef struct {
    unsigned abi, size, modes;
    char key[32], tab[32], label[64], description[192];
    int default_value, choice_count;
    DuelOptionChoice choices[DUEL_CHOICE_MAX];
} DuelOption;
typedef struct {
    char owner[64];
    DuelOption option;
    int value;
} DuelOptionRow;
typedef struct {
    unsigned abi, size, mode;
    int count;
    DuelOptionRow rows[DUEL_OPTION_MAX];
} DuelOptionSnapshot;
typedef struct {
    unsigned abi, size;
    int (*add)(const MemoriesModHost *, const DuelOption *);
    int (*rows)(unsigned mode, DuelOptionRow *, int capacity);
    int (*set)(const char *owner, const char *key, int value);
    int (*snapshot)(unsigned mode, DuelOptionSnapshot *, size_t);
    void (*remove)(const MemoriesModHost *);
} DuelOptionsService;
/* Resolve duel-options:options_v1. Service refuses all operations while off.
 * Register during Init; registrations persist across disable/re-enable,
 * and rows hide unapplied contributors.
 * A snapshot copies settings at confirmation, so later edits cannot change
 * a running match. Consumers retain/serialize only their own match values. */
#endif
