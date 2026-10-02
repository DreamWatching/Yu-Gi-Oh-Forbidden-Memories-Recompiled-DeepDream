#ifndef MEMORIES_DUEL_PARTICIPANTS_V1_H
#define MEMORIES_DUEL_PARTICIPANTS_V1_H
#include <stdint.h>
#include <stddef.h>
/* Caller-owned, pointer-free presentation snapshot. No private deck records.
 * Providers return zero when disabled or outside their own active match.
 * Query every time: find() pointers remain valid after a mod is disabled. */
#define DUEL_PARTICIPANTS_ABI 1u
#define DUEL_PORTRAIT_YUGI (-1)
typedef struct {
    int32_t portrait; /* -1 Yugi's two campaign poses, 0..39 retail opponents */
    int32_t identity; /* Stable within this match; distinguishes teammates. */
    int32_t hand, deck;
    char name[64], control[24];
} DuelParticipant;
typedef struct {
    uint32_t abi, size;
    int32_t active_side;
    DuelParticipant side[2];
    DuelParticipant previous[2];
    int32_t transition_side;
    uint32_t transition_progress; /* 0..255, fully visible at 255 */
} DuelParticipants;
typedef int (*DuelParticipantsQuery)(DuelParticipants *, size_t);
#endif
