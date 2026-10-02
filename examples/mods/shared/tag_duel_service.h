#ifndef TAG_DUEL_SERVICE_V1_H
#define TAG_DUEL_SERVICE_V1_H
typedef struct {
    unsigned abi, size;
    int (*available)(void);
    int (*start)(int,int,int,int,int,int,int);
    int (*signature_deck)(int,unsigned short[40]);
    void (*partner_deck)(const unsigned short[40]);
    void (*reset_presentation)(void);
} TagDuelService;
#endif
