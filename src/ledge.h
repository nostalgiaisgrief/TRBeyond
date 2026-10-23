#ifndef TOMB_LEDGE_H
#define TOMB_LEDGE_H
#include "air.h"
/* Additional original collision samples required by ledge checks. */
typedef struct TombLedgeSamples {
    int32_t ceiling,front_floor,front_ceiling,left_floor,right_floor;
} TombLedgeSamples;
typedef int16_t (*TombBoundsMinY)(void *,TombActor *);
typedef int (*TombSwingSpace)(void *,TombActor *,int16_t yaw);
typedef struct TombGrabContext {
    uint32_t input;
    int16_t weapon_status;
    TombBoundsMinY bounds_min_y;
    TombSwingSpace swing_space;
    void *user;
} TombGrabContext;
/* Original upward (0x27fa0) or forward reach (0x27d4c) catch. The bounds
   service must return the original current-animation minimum Y. */
TOMB_EXPORT int tomb_ledge_grab(TombActor *,const TombContact *,const TombLedgeSamples *,TombGrabContext *,int forward);
TOMB_EXPORT void tomb_reach_control(TombActor *,int16_t *camera_angle);
TOMB_EXPORT int tomb_reach_collision(TombActor *,TombContact *,TombAirContext *,TombContactAction grab);
typedef struct TombHangContext {
    TombQuery query;
    TombBoundsMinY bounds_min_y;
    TombLedgeSamples *samples; /* Query refreshes these samples on each call. */
    void *user;
    uint32_t input;
    int16_t health,weapon_status,move_angle;
} TombHangContext;
TOMB_EXPORT void tomb_hang_maintain(TombActor *,TombContact *,TombHangContext *);
typedef struct TombHangControl {
    uint32_t input;
    int16_t camera_angle,camera_elevation;
} TombHangControl;
TOMB_EXPORT void tomb_hang_control(TombActor *,TombContact *,TombHangControl *);
/* Pull-up clearance after the maintenance query has refreshed samples. */
TOMB_EXPORT void tomb_hang_pullup_goal(TombActor *,const TombLedgeSamples *,uint32_t input,
    int32_t left_ceiling,int32_t right_ceiling,int static_hit);
TOMB_EXPORT void tomb_shimmy_collision(TombActor *,TombContact *,TombHangContext *,int right);
TOMB_EXPORT void tomb_pullup_collision(TombActor *,TombContact *,TombAirContext *);
typedef struct TombVaultContext {
    uint32_t input;
    int16_t weapon_status,fall_override;
    int32_t left_ceiling,right_ceiling;
    TombAirAdvance advance;
    void *user;
} TombVaultContext;
TOMB_EXPORT int tomb_vault(TombActor *,TombContact *,const TombLedgeSamples *,TombVaultContext *);
#endif
