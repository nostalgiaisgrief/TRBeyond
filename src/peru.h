#ifndef TOMB_PERU_H
#define TOMB_PERU_H
#include "creature.h"
#include "dos_camera.h"
struct TombPlaytest;
typedef struct TombPeru {
    uint16_t flip_flags[1024];
    int flipped,effect,effect_ticks,scion_item,scion_frame,camera_target_y;
    TombActor scion_origin;
} TombPeru;
TOMB_EXPORT int tomb_flip_map(TombObjects *,int floors);
TOMB_EXPORT int tomb_flip_action(TombObjects *,int action,int id,int type,uint16_t flags);
/* Returns a room-flip request; sounds are -1 when silent. */
TOMB_EXPORT int tomb_flip_effect(int *effect,int *ticks,int sounds[2],int *bounce);
int tomb_peru_object(int);
int tomb_peru_tick(TombObjects *,size_t,TombAnimContext *);
int tomb_peru_lara(struct TombPlaytest *,uint32_t);
int tomb_scion_camera(const TombObjects *,const int16_t *,struct TombDosCamera *,int *,int *);
int tomb_peru_hazards(TombObjects *,TombAnimContext *,TombActor *,int16_t *);
TOMB_EXPORT void tomb_mummy_control(TombObject *,struct TombCreature *,const TombActor *);
TOMB_EXPORT struct TombCreatureDecision tomb_larson_control(TombObject *,struct TombCreature *,const struct TombCreatureInfo *,int16_t,uint32_t *,const TombAnimContext *,int,int);
#endif
