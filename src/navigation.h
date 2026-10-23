#ifndef TOMB_NAVIGATION_H
#define TOMB_NAVIGATION_H
#include "creature.h"
typedef struct TombNavNode { int16_t exit;uint16_t search;int16_t next,zone_box; } TombNavNode;
typedef struct TombNavigation {
    TombNavNode *nodes;
    int16_t head,tail;
    uint16_t search,block;
    int16_t step,drop,fly,zone_count,target_box,required_box;
    int32_t x,y,z;
} TombNavigation;
TOMB_EXPORT int tomb_navigation_init(TombNavigation *,const TombLevel *,const TombActor *,int object);
TOMB_EXPORT void tomb_navigation_free(TombNavigation *);
TOMB_EXPORT int tomb_navigation_search(TombNavigation *,const TombLevel *,int count);
TOMB_EXPORT int tomb_navigation_update(TombNavigation *,const TombLevel *,int count);
TOMB_EXPORT void tomb_navigation_target_box(TombNavigation *,const TombLevel *,int box,uint32_t *);
TOMB_EXPORT int tomb_navigation_target(TombNavigation *,const TombLevel *,const TombActor *,int box,uint32_t *,int32_t out[3]);
TOMB_EXPORT int tomb_navigation_box(const TombLevel *,const TombActor *);
TOMB_EXPORT void tomb_creature_info(TombCreatureInfo *,const TombCreature *,const TombNavigation *,const TombLevel *,const TombActor *,int object,const TombActor *lara,const int16_t *);
TOMB_EXPORT void tomb_creature_mood(TombCreature *,TombNavigation *,const TombLevel *,const TombActor *,int object,const TombCreatureInfo *,const TombActor *lara,int16_t lara_health,int water,int16_t lara_top,uint32_t *);
TOMB_EXPORT int tomb_creature_move(TombObject *,TombCreature *,TombNavigation *,const TombObjects *,TombAnimContext *,int16_t turn,int16_t tilt);
#endif
