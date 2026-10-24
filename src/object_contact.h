#ifndef TOMB_OBJECT_CONTACT_H
#define TOMB_OBJECT_CONTACT_H
#include "objects.h"
TOMB_EXPORT void tomb_rotate_vector(int16_t yaw,int16_t pitch,int16_t roll,const int32_t in[3],int inverse,const int16_t *,int32_t out[3]);
TOMB_EXPORT int tomb_object_joint(const TombVisual *,int object,const TombActor *,int16_t pitch,int16_t roll,int16_t head,int joint,int32_t point[3],const int16_t *);
TOMB_EXPORT int tomb_object_joint_joints(const TombVisual *,int,const TombActor *,int16_t,int16_t,int16_t,int16_t,int,int32_t[3],const int16_t *);
typedef struct TombSphere { int32_t x,y,z,radius; } TombSphere;
TOMB_EXPORT int tomb_object_spheres_joints(const TombVisual *,int,const TombActor *,int16_t,int16_t,int16_t,int16_t,const int32_t[3],int16_t,int16_t,const int16_t *,TombSphere[64]);
/* DOS nearest-key pose, fixed-point hierarchy and mesh spheres (0x39130). */
TOMB_EXPORT int tomb_object_pose(const TombVisual *,const TombActor *,TombPose *);
TOMB_EXPORT int tomb_object_spheres_view(const TombVisual *,int,const TombActor *,int16_t pitch,int16_t roll,int16_t head,const int32_t origin[3],int16_t view_yaw,int16_t view_pitch,const int16_t *,TombSphere out[64]);
TOMB_EXPORT int tomb_object_spheres(const TombVisual *,int object,const TombActor *,int16_t pitch,int16_t roll,const int16_t *,TombSphere out[64]);
TOMB_EXPORT int tomb_object_push(const TombActor *object,const int16_t bounds[6],TombActor *lara,int radius,const int16_t *sine);
TOMB_EXPORT int16_t tomb_object_angle(int32_t z,int32_t x);
struct TombPlaytest;
int tomb_lara_gun_point(const struct TombPlaytest *,int left,int32_t out[3]);
TOMB_EXPORT int tomb_doors_collide(TombObjects *,TombActor *,TombContact *,TombAnimContext *,int16_t pitch,int16_t roll,TombQuery,void *,int *hit);
TOMB_EXPORT int tomb_scion_altar_collide(TombObjects *,TombActor *,TombContact *,TombAnimContext *,TombQuery,void *);
int tomb_crystals_collide(TombObjects *,TombActor *,TombContact *,TombAnimContext *,int16_t,int16_t,TombQuery,void *);
#endif
