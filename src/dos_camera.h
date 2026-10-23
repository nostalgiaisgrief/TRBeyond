#ifndef TOMB_DOS_CAMERA_H
#define TOMB_DOS_CAMERA_H
#include "level.h"
typedef struct TombCameraPoint {int32_t x,y,z;int16_t room;} TombCameraPoint;
typedef struct TombDosCamera {
    TombCameraPoint eye,target;
    int32_t shift,fixed,ready;
    int32_t distance_squared; /* DOS retains the last chase radius in combat. */
} TombDosCamera;
#define TOMB_CAMERA_LOOK 512
#define TOMB_CAMERA_COMBAT 256 /* Request tag; not a DOS camera.flags value. */
typedef struct TombCameraRequest {
    int32_t distance,flags,speed;
    int16_t angle,elevation,pitch;
    int16_t fixed_index,item_target;
} TombCameraRequest;
TOMB_EXPORT int tomb_dos_camera_los(const TombLevel *,const TombCameraPoint *,TombCameraPoint *,int heavy);
TOMB_EXPORT int tomb_dos_camera_adjust(const TombLevel *,const TombCameraPoint *,TombCameraPoint *,int32_t radius2,int heavy);
TOMB_EXPORT int tomb_dos_camera_move(TombDosCamera *,const TombLevel *,TombCameraPoint,int speed);
/* One original 30 Hz camera update. Bounds are the DOS interpolated bounds of
   the focus item. Callers interpolate only the resulting presentation states. */
TOMB_EXPORT int tomb_dos_camera_tick(TombDosCamera *,const TombLevel *,const TombActor *,const int16_t bounds[6],const int16_t *sine,const TombCameraRequest *);
TOMB_EXPORT int tomb_dos_camera_stomp(const TombCameraPoint *,const TombActor *,int previous);
TOMB_EXPORT int tomb_dos_camera_move_effects(TombDosCamera *,const TombLevel *,TombCameraPoint,int speed,int *bounce,uint32_t *random);
int tomb_dos_camera_tick_effects(TombDosCamera *,const TombLevel *,const TombActor *,const int16_t[6],const int16_t *,const TombCameraRequest *,int *bounce,uint32_t *random);
#endif
