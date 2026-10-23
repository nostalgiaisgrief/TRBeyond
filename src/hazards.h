#ifndef TOMB_HAZARDS_H
#define TOMB_HAZARDS_H
#include "objects.h"
struct TombCreature;
#define TOMB_DART_CAPACITY 256
typedef struct TombDart { TombObject item; int touching; } TombDart;
typedef struct TombHazardEffect { int32_t x,y,z;int16_t room,id,frame,counter,speed,yaw;int active; } TombHazardEffect;
int tomb_hazard_sprite(const TombVisual *,int id,int frame,int *count);
TOMB_EXPORT void tomb_hazard_effect_tick(TombHazardEffect *,int frames,const int16_t *);
typedef struct TombHazards { TombHazardEffect effects[256];uint32_t random; TombDart darts[TOMB_DART_CAPACITY]; unsigned spawned,hits,impacts; } TombHazards;
int tomb_effect_spawn(TombObjects *,int id,const TombActor *,int speed,int yaw);
int tomb_missile_spawn(TombObjects *,int,const TombActor *,const TombActor *,const int16_t *);
TOMB_EXPORT void tomb_missile_move(TombHazardEffect *,const int16_t *);
TOMB_EXPORT void tomb_missile_aim(TombHazardEffect *,const TombActor *,const int16_t bounds[6],uint32_t *);
int tomb_explode_meshes(TombObjects *,const TombObject *,const struct TombCreature *,uint32_t,int,const int16_t *);
int tomb_hazard_sine(const int16_t *,int);
TOMB_EXPORT int tomb_blade_control(TombObject *,uint32_t touch);
TOMB_EXPORT int tomb_floor_control(TombObject *,int32_t lara_y);
TOMB_EXPORT int tomb_dart_emit(TombObject *,const TombAnimContext *,TombActor *);
TOMB_EXPORT int tomb_hazards_tick(TombObjects *,TombAnimContext *,TombActor *,int16_t *health);
int tomb_hazards_contact(TombObjects *,TombActor *,TombContact *,TombAnimContext *,int16_t,int16_t,TombQuery,void *);
#endif
