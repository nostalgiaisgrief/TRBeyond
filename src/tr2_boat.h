#ifndef TR2_BOAT_H
#define TR2_BOAT_H
#include "tr2_runtime.h"
/* Root boat data allocation 0x40cb10, 24 bytes. */
typedef struct {int turn,left_fall,right_fall;int16_t tilt,extra_rotation;int water,pitch;} Tr2Boat;
int tr2_boat_sound_pitch(const Tr2Boat *state);
typedef struct {int room,floor,ceiling,water,driving,look;uint32_t trigger;} Tr2BoatSurface;
int tr2_boat_float(const Tr2Level *l,Tr2Actor *boat,Tr2Boat *state,const Tr2Actor *rider,uint32_t input,Tr2BoatSurface *surface);
int tr2_vehicle_shift(const Tr2Level *l,Tr2Actor *vehicle,const int point[3],const int old[3]);
int tr2_vehicle_hit(const Tr2Actor *vehicle,int displacement[3]);
int tr2_boat_obstacle(Tr2Actor *boat,Tr2Actor *other,int id);
int tr2_boat_dynamics(const Tr2Level *l,Tr2Actor *boat,Tr2Boat *state,Tr2Actor *rider,void (*obstacles)(void *),void *context);
int tr2_boat_mount_kind(const Tr2Actor *boat,const Tr2Actor *lara,uint32_t input);
int tr2_boat_sample(const Tr2Level *l,const Tr2Actor *boat,int forward,int side,int point[3]);
int tr2_boat_vertical(int height,int fall,int *y);
int tr2_boat_input(Tr2Actor *boat,Tr2Boat *state,uint32_t input,int *look);
void tr2_boat_rider(const Tr2Level *l,const Tr2Actor *boat,const Tr2Boat *state,Tr2Actor *lara,int collision,uint32_t input,int animation_base);
#endif
