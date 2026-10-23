#ifndef TOMB_PISTOLS_H
#define TOMB_PISTOLS_H
#include "step.h"
typedef struct TombGunArm { int16_t frame,lock,yaw,pitch,roll,flash; } TombGunArm;
typedef struct TombPistols {
    TombGunArm left,right;
    int16_t status,head_yaw,head_pitch,torso_yaw,torso_pitch;
    int16_t target_yaw,target_pitch;
    int target,drawn;
    uint32_t random,shots,hits,kills;
} TombPistols;
/* Service boundaries isolate DOS state/animation decisions from world queries. */
typedef int (*TombGunFire)(void *,int16_t yaw,int16_t pitch);
typedef void (*TombGunSound)(void *,int id);
TOMB_EXPORT void tomb_pistols_init(TombPistols *);
TOMB_EXPORT void tomb_pistols_aim(TombGunArm *,int16_t yaw,int16_t pitch);
TOMB_EXPORT void tomb_pistols_tick(TombPistols *,int toggle,int action,int alive,int water,
    TombGunFire,TombGunSound,void *);
TOMB_EXPORT void tomb_magnums_tick(TombPistols *,int toggle,int action,int alive,int water,
    TombGunFire,TombGunSound,void *);
TOMB_EXPORT void tomb_uzis_tick(TombPistols *,int,int,int,int,TombGunFire,TombGunSound,void *);
TOMB_EXPORT uint32_t tomb_control_random(uint32_t *);
TOMB_EXPORT void tomb_shotgun_tick(TombPistols *,int toggle,int action,int alive,int water,
    TombGunFire,TombGunSound,void *);
#endif
