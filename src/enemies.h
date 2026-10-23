#ifndef TOMB_ENEMIES_H
#define TOMB_ENEMIES_H
#include "navigation.h"
#include "lara_start.h"
typedef struct TombEnemies {
    TombCreature *items;
    TombNavigation navigation[8];
    int slots[8],head,*next;
    uint32_t random;
    int32_t camera[3];
    int *room_head,*room_next,*room_id;
} TombEnemies;
int tomb_enemy_object(int id);
int tomb_enemies_init(TombObjects *);
void tomb_enemies_reset(TombObjects *);
void tomb_enemies_free(TombObjects *);
int tomb_enemies_activate(TombObjects *,size_t index,int always);
int tomb_enemies_tick(TombObjects *,TombAnimContext *,TombLaraStart *);
void tomb_enemies_trigger(TombObjects *,size_t index,int type,uint16_t flags);
void tomb_enemies_room_sync(TombObjects *);
int tomb_enemies_birth(TombObjects *,size_t);
int tomb_enemies_configure(TombObjects *,int no_enemies);
#endif
