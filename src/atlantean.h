#ifndef TOMB_ATLANTEAN_H
#define TOMB_ATLANTEAN_H
#include "creature.h"
TOMB_EXPORT void tomb_natla_flight(TombObject *,TombCreature *,const TombCreatureInfo *,int,uint32_t *);
TOMB_EXPORT TombCreatureDecision tomb_natla_control(TombObject *,TombCreature *,const TombCreatureInfo *,int16_t,int);
TOMB_EXPORT TombCreatureDecision tomb_torso_control(TombObject *,TombCreature *,const TombCreatureInfo *,int16_t,int16_t,uint32_t *,const TombAnimContext *,int);
TOMB_EXPORT TombCreatureDecision tomb_mines_control(TombObject *,TombCreature *,const TombCreatureInfo *,int16_t,uint32_t *,const TombAnimContext *,int,int);
TOMB_EXPORT void tomb_atlantean_flight(TombObject *,TombCreature *,const TombCreatureInfo *,int);
int tomb_atlantean_spawners_init(TombObjects *);
int tomb_atlantean_spawners_tick(TombObjects *,TombAnimContext *,const TombActor *);
/* blood=172/173 requests the original shard/bomb service. */
TOMB_EXPORT TombCreatureDecision tomb_atlantean_control(TombObject *,TombCreature *,const TombCreatureInfo *,int16_t,uint32_t *,const TombAnimContext *,int,int);
#endif
