#ifndef TOMB_WATER_H
#define TOMB_WATER_H
#include "ledge.h"
#include "level.h"
typedef void (*TombWaterRoom)(void *,TombActor *,int32_t offset);
typedef int16_t (*TombWaterHeight)(void *,TombActor *);
typedef struct TombWater {
    uint32_t input;
    int16_t health,pitch,lean,status,dive_count,move_angle,weapon_status,air;
    TombQuery query;
    TombWaterRoom room;
    TombWaterHeight height;
    void *user;
    const int16_t *sine;
} TombWater;
TOMB_EXPORT int tomb_water_control(TombActor *,TombWater *,int surface);
TOMB_EXPORT void tomb_water_damping(TombWater *,int surface);
TOMB_EXPORT void tomb_water_motion(TombActor *,const TombWater *,int surface);
TOMB_EXPORT void tomb_underwater_collision(TombActor *,TombContact *,TombWater *);
TOMB_EXPORT int tomb_water_exit(TombActor *,const TombContact *,const TombLedgeSamples *,TombWater *);
TOMB_EXPORT void tomb_surface_collision(TombActor *,TombContact *,TombLedgeSamples *,TombWater *);
TOMB_EXPORT int16_t tomb_water_height(const TombLevel *,int32_t x,int32_t z,int room);
/* Normal land entry, surfacing and leaving water. Dive states 52/53 are separate. */
TOMB_EXPORT void tomb_water_transition(TombActor *,TombWater *,int wet_room,int16_t height);
struct TombObjects;
TOMB_EXPORT void tomb_current_pull(TombActor *,TombContact *,TombWater *,const int32_t target[3],int strength);
int tomb_water_current(struct TombObjects *,TombActor *,TombContact *,TombWater *);
#endif
