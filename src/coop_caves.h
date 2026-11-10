/* One world, two fixed player slots. Run host then guest on both machines so
   a simultaneous switch interaction always has the same winner. */
#ifndef TOMB_COOP_CAVES_H
#define TOMB_COOP_CAVES_H
#include "playtest.h"
#include "enemies.h"
#include "combat.h"
#include "coop_lan.h"
#include <stdlib.h>

typedef struct CavesCoop {
    TombPlaytest *players[2];
    TombObjectCamera cameras[2];
    int last_targets[2];
} CavesCoop;

static uint32_t caves_hash(uint32_t h,const void *bytes,size_t n){
    const unsigned char *p=bytes;
    for(size_t i=0;i<n;i++)h=(h^p[i])*16777619u;
    return h;
}
static void caves_coop_init(CavesCoop *game,TombPlaytest *host,TombPlaytest *guest){
    game->players[0]=host;game->players[1]=guest;
    for(int i=0;i<2;i++){
        TombPlaytest *p=game->players[i];
        tomb_gameplay_start(p,TOMB_NO_ENEMIES|TOMB_INVINCIBLE);
        game->cameras[i]=p->objects->camera;
        game->last_targets[i]=p->objects->last_target;
    }
}
static int caves_coop_wolf_start(CavesCoop *game,const int16_t *sine){
    TombPlaytest *host=game->players[0],*guest=game->players[1];TombObjects *w=host->objects;
    if(tomb_combat_fixture(host,w,sine,7)<0)return 0;
    guest->lara=host->lara;guest->animation.move_angle=host->animation.move_angle;
    for(int i=0;i<2;i++){
        tomb_gameplay_start(game->players[i],0);
        game->cameras[i]=w->camera;game->last_targets[i]=w->last_target;
    }
    return tomb_enemies_coop_init(w,host,guest);
}
static int caves_coop_enable_pickups(CavesCoop *game){
    TombObjects *world=game->players[0]->objects;
    world->pickup_claims=calloc(world->count,1);
    if(!world->pickup_claims)return 0;
    for(int i=0;i<2;i++){
        game->players[i]->coop_partner=game->players[1-i];
        game->players[i]->pickup_bit=1u<<i;
    }
    return 1;
}
static int caves_coop_pickup_start(CavesCoop *game){
    if(!caves_coop_enable_pickups(game))return 0;
    TombObjects *world=game->players[0]->objects;
    /* A shared small pack at the start puts both claims within Action range. */
    for(size_t i=0;i<world->source_count;i++){
        TombObject *pack=world->items+i;
        if(pack->object!=93)continue;
        const TombActor *start=&game->players[0]->lara.actor;
        pack->actor.x=start->x;pack->actor.y=start->y;pack->actor.z=start->z;pack->actor.room=start->room;
        tomb_enemies_room_sync(world);
        return 1;
    }
    return 0;
}
static int caves_coop_medipack(CavesCoop *game,int slot,int id){
    TombPlaytest *user=game->players[slot],*other=game->players[1-slot];
    if(user->lara.health<=0 || !user->inventory.counts[id-84])return 0;
    if(other->lara.health>0){
        if(!tomb_inventory_use(&user->inventory,id,&user->lara.health))return 0;
    }else{
        TombInventory inventory=other->inventory;
        int mode=other->shooter.mode;
        TombPistols old_guns=other->pistols;
        uint32_t options=other->gameplay_options;
        if(!tomb_playtest_init(other,user->level,user->animation.sine_quarter,user->visual))return 0;
        other->objects=user->objects;other->movement_only=user->movement_only;
        other->inventory=inventory;other->gameplay_options=options;
        tomb_shooter_start(other,mode);
        other->pistols.shots=old_guns.shots;other->pistols.hits=old_guns.hits;other->pistols.kills=old_guns.kills;
        other->pistols.random=old_guns.random;
        other->pickup_bit=1u<<(1-slot);other->coop_partner=user;
        TombActor *a=&other->lara.actor;const TombActor *origin=&user->lara.actor;
        a->x=origin->x;a->y=origin->y;a->z=origin->z;a->room=origin->room;a->yaw=origin->yaw;
        other->animation.move_angle=a->yaw;
        if(user->lara.water_status){
            other->lara.water_status=other->water.status=user->lara.water_status;
            a->animation=user->lara.water_status==1?108:114;
            a->current=a->goal=other->level->animations[a->animation].state;
            a->frame=other->level->animations[a->animation].first_frame;
        }
        int amount=id==93?250:500;
        other->lara.health=other->movement.health=other->water.health=amount;
        user->lara.health=user->lara.health+amount>1000?1000:user->lara.health+amount;
        --user->inventory.counts[id-84];
        game->cameras[1-slot]=(TombObjectCamera){.index=-1,.last=-1,.target=-1};game->last_targets[1-slot]=-1;
    }
    user->movement.health=user->water.health=user->lara.health;
    return 1;
}
static int caves_coop_tick(CavesCoop *game,const uint32_t input[2],int local_slot){
    TombObjects *world=game->players[0]->objects;
    for(int i=0;i<2;i++){
        world->camera=game->cameras[i];
        world->last_target=game->last_targets[i];
        if(!tomb_playtest_player_tick(game->players[i],input[i]&COOP_MOVEMENT_MASK,i==0))return 0;
        game->cameras[i]=world->camera;
        game->last_targets[i]=world->last_target;
    }
    if(world->pickup_claims)for(int i=0;i<2;i++){
        int id=input[i]&COOP_SMALL_PACK?93:input[i]&COOP_LARGE_PACK?94:0;
        TombPlaytest *user=game->players[i];
        if(id && caves_coop_medipack(game,i,id) && user->sound_count<TOMB_SOUND_EVENTS){
            const TombActor *a=&user->lara.actor;
            user->sounds[user->sound_count++]=(TombSoundEvent){5,116,2,a->x,a->y,a->z,NULL};
        }
    }
    if(world->pickup_claims && game->players[local_slot]->lara.health<=0 && game->players[1-local_slot]->lara.health>0)local_slot=1-local_slot;
    world->camera=game->cameras[local_slot];
    world->last_target=game->last_targets[local_slot];
    return 1;
}
static uint32_t caves_coop_digest(const CavesCoop *game){
    uint32_t h=2166136261u;
    TombObjects *world=game->players[0]->objects;
    if(world->pickup_claims){
        h=caves_hash(h,world->pickup_claims,world->count);
        for(int i=0;i<2;i++){
            TombInventory *inv=&game->players[i]->inventory;
            h=caves_hash(h,inv->counts,sizeof inv->counts);h=caves_hash(h,inv->quest,sizeof inv->quest);
            h=caves_hash(h,inv->ammo,sizeof inv->ammo);h=caves_hash(h,&inv->scion,sizeof inv->scion);
        }
    }
    for(size_t i=0;i<2+world->count;i++){
        const TombActor *a=i<2?&game->players[i]->lara.actor:&world->items[i-2].actor;
        int32_t values[]={a->x,a->y,a->z,a->room,a->yaw,a->animation,a->frame,a->flags};
        h=caves_hash(h,values,sizeof values);
        if(world->enemies->targets){
            if(i<2){
                TombPlaytest *p=game->players[i];
                uint32_t stats[]={(uint32_t)p->lara.health,p->pistols.hits,p->pistols.kills,p->pistols.shots};
                h=caves_hash(h,stats,sizeof stats);
            }else if(tomb_enemy_object(world->items[i-2].object)){
                TombEnemyTarget *t=world->enemies->targets+i-2;
                TombCreature *creature=world->enemies->items+i-2;
                uint32_t enemy[]={(uint32_t)creature->health,(uint32_t)creature->flags,(uint32_t)creature->mood,(uint32_t)t->clone_pair,t->player,t->cooldown,t->contact[0],t->contact[1]};
                h=caves_hash(h,enemy,sizeof enemy);
            }
        }
        if(i>=2){
            const TombObject *o=world->items+i-2;
            int32_t object[]={o->flags,o->timer,o->active,world->doors[i-2].open};
            h=caves_hash(h,object,sizeof object);
        }
    }
    return h;
}
#endif
