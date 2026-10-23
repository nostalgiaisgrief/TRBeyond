#ifndef TOMB_AIR_H
#define TOMB_AIR_H
#include "step.h"
typedef int (*TombAirAdvance)(void *,TombActor *);
typedef struct TombAirContext {
    TombQuery query;
    TombContactAction land;
    void *user;
    const int16_t *sine_quarter;
    int16_t move_angle;
    TombAnimEvent sound; /* kind 5 play, kind 7 stop; actor reflects call-time state */
    TombAirAdvance advance;
} TombAirContext;
TOMB_EXPORT void tomb_swan_control(TombActor *,TombContact *);
TOMB_EXPORT int tomb_swan_collision(TombActor *,TombContact *,TombAirContext *);
TOMB_EXPORT void tomb_forward_air_control(TombActor *,uint32_t input,int16_t weapon_status,int16_t *turn_rate);
TOMB_EXPORT int tomb_forward_air_collision(TombActor *,TombContact *,TombAirContext *,uint32_t input);
TOMB_EXPORT int tomb_fast_fall_control(TombActor *,TombAnimEvent sound,void *user);
TOMB_EXPORT void tomb_fast_fall_deflect(TombActor *,TombContact *,TombAirContext *);
TOMB_EXPORT int tomb_fast_fall_collision(TombActor *,TombContact *,TombAirContext *);
TOMB_EXPORT void tomb_back_fall_control(TombActor *,uint32_t input,int16_t weapon_status);
TOMB_EXPORT void tomb_air_deflect(TombActor *,TombContact *,TombAirContext *);
TOMB_EXPORT int tomb_back_fall_collision(TombActor *,TombContact *,TombAirContext *);
/* Damage arithmetic only: caller supplies original floor/trigger services first. */
TOMB_EXPORT int tomb_landing_damage(int16_t *health,int16_t fall_speed);
typedef int16_t (*TombJumpFloor)(void *,TombActor *,int16_t angle,int32_t distance);
typedef int32_t (*TombJumpQuery)(void *,TombActor *,TombContact *,int32_t height);
TOMB_EXPORT void tomb_jump_prepare_control(TombActor *,uint32_t,TombJumpFloor,void *,int16_t *move_angle);
TOMB_EXPORT void tomb_jump_prepare_collision(TombActor *,TombContact *,TombJumpQuery,void *,int16_t move_angle);
TOMB_EXPORT void tomb_up_jump_control(TombActor *);
TOMB_EXPORT int tomb_up_jump_collision(TombActor *,TombContact *,TombAirContext *,TombContactAction grab);
TOMB_EXPORT int tomb_directional_jump_control(TombActor *,int16_t *camera_angle);
TOMB_EXPORT int tomb_directional_jump_collision(TombActor *,TombContact *,TombAirContext *);
#endif
