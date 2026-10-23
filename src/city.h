#ifndef TOMB_CITY_H
#define TOMB_CITY_H
#include "playtest.h"
TOMB_EXPORT int tomb_city_bounds(const TombActor *,const TombActor *,int16_t,int16_t,int,const int16_t *);
TOMB_EXPORT int tomb_block_floor(TombObjects *,size_t,int);
TOMB_EXPORT int tomb_block_can_move(TombObjects *,const TombActor *,size_t,int,int);
TOMB_EXPORT int tomb_water_switch_contact(TombObject *,TombActor *,TombAnimContext *,uint32_t,int,int16_t *,int16_t *);
int tomb_block_tick(TombObjects *,size_t,TombAnimContext *);
int tomb_city_fixture(TombPlaytest *,TombObjects *,const int16_t *,int);
TOMB_EXPORT int tomb_quest_contact(TombObject *,TombActor *,TombAnimContext *,TombInventory *,int16_t *,int16_t *,int);
int tomb_city_interact(TombPlaytest *,uint32_t);
#endif
