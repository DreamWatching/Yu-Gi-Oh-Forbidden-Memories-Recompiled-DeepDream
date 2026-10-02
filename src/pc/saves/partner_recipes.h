#ifndef MEMORIES_PARTNER_RECIPES_H
#define MEMORIES_PARTNER_RECIPES_H
#include <stdint.h>
#include "deck_slots.h"
/* Canonical little-endian schema 2. Preserve unresolved mod identities when
 * a different slot is saved. The UI sees unresolved recipes as unavailable. */
#define PARTNER_RECIPE_IDENTITY 128
#define PARTNER_RECIPE_SLOT_BYTES (65 + 40 * (2 + PARTNER_RECIPE_IDENTITY))
#define PARTNER_RECIPE_PAYLOAD (10 * PARTNER_RECIPE_SLOT_BYTES)
typedef struct { unsigned char bytes[PARTNER_RECIPE_PAYLOAD]; } PartnerRecipes;
/* 1 valid/migrated, 0 absent, -1 corrupt/wrong owner/unsupported schema.
 * Legacy numeric mod IDs cannot be migrated safely and are left unavailable. */
int PartnerRecipes_Read(const char *path, uint32_t owner, PartnerRecipes *out);
int PartnerRecipes_Get(const PartnerRecipes *store, int slot, DeckSlot *deck, char name[64]);
/* On failure both the prior file and in-memory store remain unchanged.
 * Save succeeds only with 40 valid cards and at most three of each card. */
int PartnerRecipes_Save(const char *path, uint32_t owner, PartnerRecipes *store,
                        int slot, const char *name, const unsigned short cards[40]);
#endif
