#ifndef TAG_DUEL_RUNTIME_H
#define TAG_DUEL_RUNTIME_H

enum TagPartnerControl {
    TAG_CONTROL_AI,
    TAG_CONTROL_PLAYER_ONE,
    TAG_CONTROL_PLAYER_TWO
};

enum TagPartnerDeckSource {
    TAG_DECK_PARTNER_SIGNATURE,
    TAG_DECK_CAMPAIGN,
    TAG_DECK_CUSTOM_SLOT
};

int TagDuel_Start(int partner, int opponent_1, int opponent_2,
                  int partner_control, int partner_deck_source,
                  int partner_deck_slot, int hard_mode);
int TagDuel_SignatureDeck(int duelist, unsigned short out[40]);
void TagDuel_SetPartnerDeck(const unsigned short cards[40]);
void TagDuel_DrawHud(void);
unsigned TagDuel_HudSignature(void);
void TagDuel_ResetHud(void);

#endif
