#include "builtin_options.h"
#include "../shared/duel_options.h"
#include <stdio.h>
#include <string.h>
static const DuelOptionsService *attempted;
void DuelOptions_RegisterBuiltins(const MemoriesModHost *host)
{
    const DuelOptionsService *api=host->find(host,"duel-options:options_v1");
    DuelOption option;
    static const char *fields[]={"Normal","Forest","Wasteland","Mountain","Sogen","Umi","Yami","Random / Static","Random / Dynamic"};
    if(!api || api==attempted)return;
    attempted=api;
    if(api->abi!=1 || api->size!=sizeof(*api) || !api->add || !api->remove)return;
    memset(&option,0,sizeof(option));option.abi=1;option.size=sizeof(option);
    option.modes=DUEL_OPTION_SOLO|DUEL_OPTION_TAG;
    strcpy(option.tab,"Battle Rules");strcpy(option.key,"starting_life");
    strcpy(option.label,"Starting Life");
    strcpy(option.description,"Both sides begin with the selected Life Points.");
    option.default_value=8000;option.choice_count=5;
    for(int i=0;i<5;i++) {
        option.choices[i].value=4000*(i+1);
        snprintf(option.choices[i].label,sizeof(option.choices[i].label),"%d LP",option.choices[i].value);
    }
    if(!api->add(host,&option))goto failed;
    strcpy(option.key,"starting_field");strcpy(option.label,"Starting Field");
    strcpy(option.description,"Choose the initial terrain or a static/dynamic random field.");
    option.default_value=0;option.choice_count=9;
    for(int i=0;i<9;i++) {
        option.choices[i].value=i;strcpy(option.choices[i].label,fields[i]);
    }
    if(!api->add(host,&option))goto failed;
    strcpy(option.key,"opening_clash");strcpy(option.label,"Roll For Who Plays First");
    strcpy(option.description,"Guardian Star Face-off decides who chooses the starting team and duelist.");
    option.default_value=1;option.choice_count=2;
    option.choices[0].value=0;strcpy(option.choices[0].label,"Off");
    option.choices[1].value=1;strcpy(option.choices[1].label,"On");
    if(!api->add(host,&option))goto failed;
    strcpy(option.key,"hard_mode");strcpy(option.label,"Opponent Draw Hand");
    strcpy(option.description,"CPU searches five or twenty cards; visible hand size remains five.");
    option.default_value=1;option.choice_count=2;
    option.choices[0].value=0;strcpy(option.choices[0].label,"5 Cards");
    option.choices[1].value=1;strcpy(option.choices[1].label,"20 Cards");
    if(api->add(host,&option))return;
failed:
    api->remove(host);host->log(host,"Duel Options: could not register the four built-in rules; existing menu remains available");
}
void DuelOptions_RemoveBuiltins(const MemoriesModHost *host)
{
    if(attempted && attempted->abi==1 && attempted->size==sizeof(*attempted) && attempted->remove)
        attempted->remove(host);
    attempted=NULL;
}
