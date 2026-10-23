#ifndef TOMB_SHOOTER_H
#define TOMB_SHOOTER_H
#include "dos_camera.h"
enum { TOMB_MODE_ORIGINAL=0, TOMB_MODE_SHOOTER=1, TOMB_MODE_FPS=2, TOMB_MODE_VR=3, TOMB_INPUT_AIM=8192, TOMB_INPUT_SHOOT=16384 };
typedef struct TombShooter {
    int mode;
    int16_t yaw,pitch;
    uint32_t input;
    int jump_held,aim_held;
    TombCameraPoint eye,target;
    int dual,mode_held,next_hand,fire_delay,drop_latched;
} TombShooter;
struct TombPlaytest;
void tomb_shooter_start(struct TombPlaytest *,int mode);
void tomb_shooter_mouse(struct TombPlaytest *,int dx,int dy);
uint32_t tomb_shooter_input(struct TombPlaytest *,uint32_t);
int tomb_fps_ready(const struct TombPlaytest *);
int16_t tomb_shooter_render_yaw(const struct TombPlaytest *);
void tomb_shooter_pose(struct TombPlaytest *);
int tomb_shooter_listener(const struct TombPlaytest *,double position[3]);
void tomb_shooter_anchor(const struct TombPlaytest *,double position[3]);
void tomb_shooter_view(const struct TombPlaytest *,const double position[3],double eye[3],double target[3],int *room);
void tomb_shooter_view_blend(const struct TombPlaytest *,const double position[3],double block_view,double eye[3],double target[3],int *room);
void tomb_shooter_target(struct TombPlaytest *);
/* Nearest live hit along a ray, clipped against level geometry first. */
void tomb_shooter_world_ray(const struct TombPlaytest *,TombCameraPoint,TombCameraPoint *);
int tomb_shooter_ray(struct TombPlaytest *,TombCameraPoint,int16_t,int16_t,TombCameraPoint *);
#endif
