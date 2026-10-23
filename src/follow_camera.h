#ifndef TOMB_FOLLOW_CAMERA_H
#define TOMB_FOLLOW_CAMERA_H
#include "visual.h"
#include "objects.h"
#include "dos_camera.h"
typedef struct TombFollowCamera {
    double eye[3],target[3],anchor[3],yaw,boom;
    int room,ready,obstructed;
    int fixed_target;
    TombDosCamera dos;
    double previous_eye[3],previous_target[3],pending;
} TombFollowCamera;
/* DOS simulation state and separate render interpolation endpoints. */
TOMB_EXPORT void tomb_camera_box_shift(double *,double *,double,double,double,double,double,double,double);
int tomb_camera_clear(const TombLevel *,const double point[3],int room,double radius,int *resolved_room);
int tomb_follow_camera(TombFollowCamera *,const TombLevel *,const TombActor *,const TombVisual *,int16_t pitch,int water_status,double orbit,double distance,double dt);
int tomb_scene_camera(TombFollowCamera *,const TombObjects *,const TombActor *,int16_t pitch,int water_status,double orbit,double distance,double dt);
int tomb_camera_tick(TombFollowCamera *,const TombLevel *,const TombVisual *,const TombActor *,int object,const int16_t *,const TombCameraRequest *);
TOMB_EXPORT int tomb_camera_interest(const TombActor *,const int16_t[6],const TombActor *,const int16_t[6],int16_t *,int16_t *);
TombCameraRequest tomb_camera_control(int state,int water,int16_t pitch);
int tomb_camera_tick_effects(TombFollowCamera *,const TombLevel *,const TombVisual *,const TombActor *,int,const int16_t *,const TombCameraRequest *,int *,uint32_t *);
#endif
