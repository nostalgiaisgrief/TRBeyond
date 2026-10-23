#ifndef TOMB_CREATURE_H
#define TOMB_CREATURE_H
#include "objects.h"
typedef struct TombCreatureInfo {
    int16_t zone,enemy_zone;
    int32_t distance,ahead,bite;
    int16_t angle,enemy_facing;
} TombCreatureInfo;
typedef struct TombCreature {
    int16_t health,required,head,maximum_turn,flags,pitch,roll;
    uint8_t mood;
    uint32_t touch;
    int32_t floor,target_x,target_y,target_z;
    int16_t neck; /* DOS second joint rotation; separate from body pitch/roll. */
    int16_t yaw_offset; /* Natla's persistent flight-facing offset (DOS c3b68). */
} TombCreature;
typedef struct TombCreatureDecision { int16_t damage,turn,tilt,blood; } TombCreatureDecision;
/* Species controllers with AIInfo/Mood/Turn service results supplied explicitly.
   Their state changes, random calls, damage and head/tilt rules are original. */
TOMB_EXPORT TombCreatureDecision tomb_creature_control(TombObject *,TombCreature *,
    const TombCreatureInfo *,int16_t turn,int16_t lara_health,uint32_t *,const TombAnimContext *,int animation_base);
TOMB_EXPORT int16_t tomb_creature_turn(TombActor *,const TombCreature *,int16_t maximum);
#endif
