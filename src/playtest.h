#ifndef TOMB_PLAYTEST_H
#define TOMB_PLAYTEST_H
#include "lara_start.h"
#include "ground.h"
#include "air.h"
#include "ledge.h"
#include "visual.h"
#include "water.h"
#include "gym.h"
#include "sound.h"
#include "slide.h"
#include "objects.h"
#include "dos_camera.h"
#include "pistols.h"
#include "inventory.h"
#include "interface.h"
#include "shooter.h"
typedef struct TombPlaytest {
    const TombLevel *level;
    const TombVisual *visual;
    TombLedgeSamples ledge;
    int static_hit;
    TombLaraStart lara;
    TombMovement movement;
    TombAnimContext animation;
    TombTerrain terrain;
    TombGroundContext collision;
    TombAirContext air;
    TombWater water;
    TombGym gym;
    TombObjects *objects; /* Optional independent door/switch runtime. */
    TombSlideContext slide;
    int movement_only; /* Explicit sandbox: object/level triggers are inactive. */
    int32_t requested_camera_mode;
    int16_t requested_camera_elevation;
    int dead_ticks;
    int door_hit_ticks;
    int door_hit_direction;
    const char *status;
    int blocked;
    unsigned sound_events,sound_stops,bubble_events;
    TombSoundEvent sounds[TOMB_SOUND_EVENTS];
    unsigned sound_count; /* Transactional requests from the most recent tick. */
    unsigned deferred_cd_track; /* Latest deferred tutorial audio request. */
    unsigned deferred_camera; /* Camera index + 1; zero means no request. */
    uint16_t deferred_camera_flags;
    int16_t requested_camera_angle; /* DOS camera request; manual preview camera ignores it. */
    TombCameraRequest camera_request;
    TombPistols pistols;
    uint32_t last_input;
    TombInventory inventory;
    TombHud hud;
    int quest_request,quest_latch;
    int weapon_type,requested_weapon; /* DOS types: 1 pistols, 4 shotgun. */
    int sidearm_type; /* Last sidearm meshes installed by DrawPistols. */
    uint32_t gameplay_options;
    int spasm_ticks,spasm_effect; /* DOS explosion hit reaction; effect-pool index. */
    TombShooter shooter;
    int swim_override; /* 0 normal water rules, 1 swim anywhere, -1 force land. */
    double vr_body_bottom,vr_body_height;
    int swim_jump_held,swim_tap_ticks;
    int tps_slope_jump,tps_drop_yaw_offset; /* Air momentum and visual ledge-turn recovery. */
    int fps_shot_held; /* First actual shot consumed during this Shoot hold. */
    struct TombFpsPresentation {
        int mantle_ticks;
        int32_t mantle_from[3],mantle_to[3];
        int mantle_water,mantle_room;
        int sound_distance,block_view;
    } fps;
} TombPlaytest;
enum { TOMB_INVINCIBLE=1, TOMB_INFINITE_AMMO=2, TOMB_START_SHOTGUN=4, TOMB_START_MAGNUMS=8, TOMB_START_UZIS=16, TOMB_NO_ENEMIES=32, TOMB_DOUBLE_JUMP_SWIM=64, TOMB_MAX_DRAW_DISTANCE=128, TOMB_CONSISTENT_WEAPONS=256, TOMB_VR_CLIMB_ANYTHING=512, TOMB_AUTOSAVE=1024, TOMB_GUN_POWER_SHIFT=11, TOMB_GUN_POWER_MASK=6144, TOMB_3D_PICKUPS=8192, TOMB_SAVE_CRYSTALS=16384, TOMB_SHOW_LEVEL_NAME=32768, TOMB_SKIP_STATS=65536 };
static inline int tomb_gun_damage(uint32_t options,int weapon,int health){
    int mode=(options&TOMB_GUN_POWER_MASK)>>TOMB_GUN_POWER_SHIFT;
    int base=weapon==4?4:weapon==2?2:1;
    return mode==3 && health>0?health:base*(mode==1?2:mode==2?5:1);
}
int tomb_fps_activate(TombPlaytest *,size_t);
void tomb_gameplay_start(TombPlaytest *,uint32_t options);
int tomb_playtest_init(TombPlaytest *,const TombLevel *,const int16_t *sine,const TombVisual *);
/* Ground/air/climbing/water development harness. Unsupported services roll back movement. */
int tomb_playtest_tick(TombPlaytest *,uint32_t input);
#endif
