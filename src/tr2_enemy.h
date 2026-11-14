#ifndef TR2_ENEMY_H
#define TR2_ENEMY_H
#include "tr2_world.h"
/* Decision inputs are the original CreatureAIInfo and mood outputs. Keeping
   this boundary explicit permits testing without substituting chase logic. */
typedef struct {int mood,distance,ahead,random;uint32_t touch;int16_t required,attack,maximum_turn;} Tr2EnemyDecision;
typedef struct {int16_t zone,enemy_zone;int distance,ahead,bite;int16_t angle,enemy_facing;} Tr2EnemyInfo;
int tr2_enemy_info(const Tr2Level *l,const Tr2Lot *lot,const Tr2Actor *a,const Tr2Actor *enemy,int pivot,Tr2EnemyInfo *info,int *box,int *enemy_box);
int tr2_enemy_targetable(const Tr2Level *l,const Tr2Actor *a,const Tr2Actor *lara,const Tr2EnemyInfo *info);
int tr2_enemy_shot_chance(int distance,int speed,int facing);
int tr2_stick_decide(Tr2Object *o,Tr2EnemyDecision *c,int bite);
int tr2_creature_vault_adjust(Tr2Actor *a,int old_x,int old_y,int old_z,int old_room,int floor,int shift);
int tr2_cultist_decide(Tr2Object *o,Tr2EnemyDecision *c,int targetable,int same_zone);
int tr2_worker1_decide(Tr2Object *o,Tr2EnemyDecision *c,int targetable,int same_zone);
int tr2_worker2_decide(Tr2Object *o,Tr2EnemyDecision *c,int targetable,int same_zone);
int tr2_diver_surface(const Tr2Level *l,int x,int z,int room);
int tr2_diver_decide(Tr2Object *o,Tr2EnemyDecision *c,int targetable,int target_y,int surface,int fly);
void tr2_cultist3_pose(Tr2Creature *c,int state,int angle,int ahead);
void tr2_cultist3_init(const Tr2Level *l,Tr2World *w,Tr2Object *o);
int tr2_cultist3_decide(Tr2Object *o,Tr2EnemyDecision *c,int targetable,int same_zone,int angle,int lara_health);
int tr2_rat_decide(Tr2Object *o,Tr2EnemyDecision *c);
int tr2_dog_decide(Tr2Object *o,Tr2EnemyDecision *c);
int tr2_spider_decide(Tr2Object *o,Tr2EnemyDecision *c);
int tr2_tiger_decide(Tr2Object *o,Tr2EnemyDecision *c,int lara_speed);
int tr2_crow_decide(Tr2Object *o,Tr2EnemyDecision *c,int floor);
int tr2_trex_decide(Tr2Object *o,Tr2EnemyDecision *c,int bite,int facing);
int tr2_enemy_enable(const Tr2Level *l,Tr2World *w,int index);
void tr2_eel_tick(const Tr2Level *l,Tr2World *w,int index,Tr2Actor *lara,uint32_t touch);
void tr2_enemy_tick(const Tr2Level *l,Tr2World *w,int index,Tr2Actor *lara);
#endif

