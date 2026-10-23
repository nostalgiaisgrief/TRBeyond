#ifndef TOMB_FOLLY_H
#define TOMB_FOLLY_H
#include "creature.h"
struct TombPlaytest;
int tomb_folly_object(int);
void tomb_folly_init(TombObjects *,size_t);
int tomb_folly_tick(struct TombPlaytest *);
int tomb_folly_contact(struct TombPlaytest *);
int tomb_midas_tick(struct TombPlaytest *);
int tomb_late_world_tick(struct TombPlaytest *);
int tomb_lava_touch(struct TombPlaytest *);
int tomb_midas_sparkles(struct TombPlaytest *);
int tomb_midas_contact(struct TombPlaytest *,uint32_t);
TOMB_EXPORT uint32_t tomb_midas_gold(int frame);
TOMB_EXPORT int tomb_midas_teeth(TombObject *,uint32_t);
int tomb_folly_head(const TombObjects *,const TombObject *,TombObject *);
TOMB_EXPORT void tomb_folly_sword(TombObject *,TombCreature *,const TombActor *,TombAnimContext *);
TOMB_EXPORT TombCreatureDecision tomb_folly_control(TombObject *,TombCreature *,const TombCreatureInfo *,int16_t,uint32_t *,const TombAnimContext *,int,int);
#endif
