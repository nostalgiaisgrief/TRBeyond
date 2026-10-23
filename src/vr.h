#ifndef TOMB_VR_H
#define TOMB_VR_H
#include "playtest.h"
/* Tracking coordinates: metres, right/up/back. Game coordinates: right/down/forward. */
#define TOMB_VR_SCALE 400.0
typedef struct TombVrPose { double p[3],q[4]; int valid; } TombVrPose;
typedef struct TombVrInput {
    TombVrPose head,hand[2],aim[2];
    float stick[2][2],grip[2],trigger[2];
    int primary[2],secondary[2];
} TombVrInput;
typedef struct TombVrHand {
    int weapon,pickup,gripped,triggered,anchored,hover;
    double anchor[3],previous[3],automatic,flash,ledge_normal[3];
    int support,pump,pump_ready,ledge_room;
    double pump_origin;
    int lever,lever_count,lever_end,lever_sound,key;double lever_start_y,lever_progress,lever_travel,lever_y[128];TombActor lever_frames[128];
} TombVrHand;
typedef struct TombVr {
    TombVrHand hands[2];
    TombVrInput input;
    double yaw,last_head[3],feet[3],vertical,swim_speed,climb_offset;
    int ready,snap_latch,jump_latch,menu_latch,menu_request;
    int level_room,swimming,menu_open;
    double wrist_alpha[2];
    struct { TombCameraPoint origin; int weapon,hand; int16_t yaw,pitch; } shots[64];
    unsigned shot_count;
    double swim_velocity[3],hand_velocity[2][3],sound_distance;
    int death_sound,was_swimming;
    int jumped,sliding,align_heading,pool_exit_ticks,selected_key;unsigned fired[2];
    double deadzone,view_offset,move_speed,impact;
    int sounds[32];unsigned sound_count;
    struct { int item,hand,axis,sign,start_x,start_z,moving,push_latch; double start[3],rope_origin[3],elapsed; } block;
} TombVr;
void tomb_vr_reset(TombVr *,const TombPlaytest *);
void tomb_vr_point(const TombVr *,const double local[3],double world[3]);
void tomb_vr_direction(const TombVr *,const TombVrPose *,double direction[3]);
void tomb_vr_matrix(const TombVr *,const TombVrPose *,double matrix[16]);
void tomb_vr_hand_matrix(const TombVr *,int hand,double matrix[16]);
int tomb_vr_barrel(const TombMeshView *,int weapon,double rear[3],double muzzle[3]);
void tomb_vr_weapon_matrix(const TombVr *,int hand,const TombMeshView *,double matrix[16]);
void tomb_vr_frame(TombVr *,TombPlaytest *,const TombVrInput *,double dt,int paused);
int tomb_vr_tick(TombVr *,TombPlaytest *);
void tomb_vr_wrist_model_matrix(const double hand_matrix[16],int hand,double matrix[16]);
void tomb_vr_wrist_matrix(const TombVr *,int hand,double matrix[16]);
double tomb_vr_wrist(const TombVr *,int hand,double position[3]);
int tomb_vr_zone(const TombVrInput *,int hand);
int tomb_vr_fire(TombPlaytest *,int weapon,TombCameraPoint,int16_t yaw,int16_t pitch);
#endif
