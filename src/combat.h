#ifndef TOMB_COMBAT_H
#define TOMB_COMBAT_H
#include "pistols.h"
#include "creature.h"
#include "dos_camera.h"
struct TombPlaytest;
TOMB_EXPORT int tomb_gun_target_point(const TombVisual *,const TombActor *,const int16_t *,TombCameraPoint *);
TOMB_EXPORT void tomb_gun_angles(int32_t x,int32_t y,int32_t z,int16_t out[2]);
TOMB_EXPORT int tomb_pistol_ray(const TombVisual *,const TombObject *,const TombCreature *,TombCameraPoint origin,int16_t yaw,int16_t pitch,uint32_t *,const int16_t *,TombCameraPoint *hit);
TOMB_EXPORT int tomb_shotgun_ray(const TombVisual *,const TombObject *,const TombCreature *,TombCameraPoint origin,int16_t yaw,int16_t pitch,uint32_t *,const int16_t *,TombCameraPoint *hit);
TOMB_EXPORT void tomb_shotgun_spread(uint32_t *,int16_t yaw,int16_t pitch,int16_t out[2]);
void tomb_sidearm_spread(uint32_t *,int focus,int16_t yaw,int16_t pitch,int16_t out[2]);
int tomb_combat_tick(struct TombPlaytest *,uint32_t input);
void tomb_combat_target(struct TombPlaytest *,int action);
/* Developer fixture: resets the level and places Lara near an original enemy. */
int tomb_combat_fixture(struct TombPlaytest *,TombObjects *,const int16_t *,int species);
#endif
