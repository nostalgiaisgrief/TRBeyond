#ifndef TOMB_ENEMIES_H
#define TOMB_ENEMIES_H
#include "navigation.h"
#include "lara_start.h"
typedef struct TombEnemyTarget {
    unsigned char player, cooldown; /* 0 unassigned, 1 host, 2 guest */
    int clone_pair; /* Partner item index plus one. */
    uint32_t contact[2];
} TombEnemyTarget;
typedef struct TombEnemies {
    TombLaraStart *players[2];
    TombAnimContext *player_animation[2];
    TombEnemyTarget *targets;
    uint8_t *reachable;
    int *route_queue;
    TombCreature *items;
    TombNavigation navigation[8];
    int slots[8],head,*next;
    uint32_t random;
    int32_t camera[3];
    int *room_head,*room_next,*room_id;
} TombEnemies;
struct TombPlaytest;
int tomb_enemies_coop_init(TombObjects *,struct TombPlaytest *,struct TombPlaytest *);
void tomb_enemy_hit(TombObjects *,size_t,const TombLaraStart *);
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
