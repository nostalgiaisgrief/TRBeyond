#include <math.h>
#include "tr2_enemy.h"
#include <stdlib.h>
#include <string.h>

static int box_at(const Tr2Level *l,const Tr2Actor *a){
    if(a->room<0 || (uint32_t)a->room>=l->room_count)return -1;
    const Tr2Room *r=l->rooms+a->room;int x=tr2_asr(a->x-r->x,10),z=tr2_asr(a->z-r->z,10);
    if(x<0 || z<0 || x>=r->nx || z>=r->nz)return -1;
    unsigned b=tr2_u16(r->sectors.data+(x*r->nz+z)*8+2);return b<l->boxes.count?(int)b:-1;
}
/* CreatureAIInfo 0x40e210, Lara-targeting creatures. */
int tr2_enemy_info(const Tr2Level *l,const Tr2Lot *lot,const Tr2Actor *a,const Tr2Actor *e,int pivot,Tr2EnemyInfo *i,int *box,int *enemy_box){
    int b=box_at(l,a),eb=box_at(l,e);*box=b;*enemy_box=eb;if(b<0 || eb<0 || !lot->nodes)return 0;
    int zi=lot->fly?4:(lot->step>>8)-1;if(zi<0 || zi>4)return 0;
    const Tr2World *world=l->world;
    const uint8_t *zones=l->zones.data+l->boxes.count*2*((unsigned)zi+(world && world->flip_status?5u:0u));
    i->zone=tr2_i16(zones+b*2);i->enemy_zone=tr2_i16(zones+eb*2);
    if((tr2_u16(l->boxes.data+eb*8+6)&lot->block) || lot->nodes[b].search==(lot->search|0x8000))i->enemy_zone|=0x4000;
    int dx=e->x-a->x-tr2_asr(pivot*tr2_sin(a->yaw),14),dz=e->z-a->z-tr2_asr(pivot*tr2_cos(a->yaw),14);
    int angle=tr2_atan(dz,dx);i->distance=tr2_long((int64_t)dx*dx+(int64_t)dz*dz);
    i->angle=tr2_word(angle-a->yaw);i->enemy_facing=tr2_word(angle-e->yaw-32768);
    i->ahead=i->angle> -16384 && i->angle<16384;i->bite=i->ahead && e->control.health>0 && abs(e->y-a->y)<=384;return 1;
}

/* Root controller helpers; individual decision routines cite their entry points. */
static int enemy_random(uint32_t *state){*state=*state*0x41c64e6du+12345;return (int)((*state>>10)&32767);}
int tr2_enemy_targetable(const Tr2Level *l,const Tr2Actor *a,const Tr2Actor *lara,const Tr2EnemyInfo *info){
    if(lara->control.health<=0 || !info->ahead || info->distance>=67108864)return 0;
    Tr2Position from={a->x,a->y-768,a->z,a->room,0},to={lara->x,lara->y-768,lara->z,lara->room,0};int rooms[1024],count=0;
    return tr2_shot_los(l,&from,&to,0,rooms,&count);
}
static void enemy_shoot(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Actor *lara,const Tr2EnemyInfo *info,int targetable,int head,int muzzle){
    Tr2Actor *a=&o->pose;
    if(targetable){
        int chance=tr2_enemy_shot_chance(info->distance,lara->speed,info->enemy_facing);
        int hit=chance>=0 && enemy_random(&w->random_state)<chance;
        int npc=w->lara && lara!=w->lara && (o->id==48 || o->id==49 || o->id==50);
        /* ShootAtLara divides damage to other creatures by ten (4362ce). */
        if(npc && hit){lara->control.health-=(o->id==48?8:50)/10;lara->control.flags|=16;}
        else if(hit){int point[3]={0,0,0};int joint=enemy_random(&w->random_state)*25/32767;
            if(tr2_joint(l,lara,0,joint,point)){Tr2Position p={point[0],point[1],point[2],lara->room,0};tr2_blood(w,&p,lara->speed,lara->yaw);}
            lara->control.health=tr2_word(lara->control.health-(o->id==48?8:o->id==30?150:o->id==31?30:o->id==51 && w->vehicle>=0?10:50));lara->control.flags|=16;
            if(lara->event_count<16){int n=lara->event_count++;lara->event_kind[n]=5;lara->event_value[n]=50;}
        }else if(!npc){Tr2Height height;int floor=lara->y;if(tr2_query(l,lara->x,lara->y,lara->z,lara->room,&height,NULL))floor=height.floor;
            Tr2Position p={lara->x+(enemy_random(&w->hazards->random_draw)-16384)*512/32767,floor,0,lara->room,0};p.z=lara->z+(enemy_random(&w->hazards->random_draw)-16384)*512/32767;tr2_ricochet(w,&p);}
        int point[3]={(o->id==30 || o->id==31)?0:o->id==20?(muzzle==1?-2:2):3,o->id==31?308:o->id==30?281:o->id==20?275:331,o->id==31?32:o->id==30?40:o->id==20?23:56};int joint=(o->id==30 || o->id==31)?9:o->id==20 && muzzle==1?6:10;
          if(o->id==49 || o->id==50){point[0]=-1;point[1]=230;point[2]=9;joint=17;}
          if(o->id==48){point[0]=-2;point[1]=150;point[2]=19;joint=17;}
          if(o->id==51){point[0]=muzzle?-235:219;point[1]=-71;point[2]=550;joint=0;}
          if(tr2_joint(l,a,o->id,joint,point))for(int i=0;i<TR2_MAX_EFFECTS;i++)if(!w->hazards->effects[i].active){
            Tr2Fragment *f=w->hazards->effects+i;memset(f,0,sizeof *f);f->active=1;f->object=240;f->room=a->room;f->x=point[0];f->y=point[1];f->z=point[2];f->yaw=tr2_word(a->yaw+head);f->counter=3;f->shade=4096;w->hazards->owner[i]=-2;break;}
    }
    Tr2Position from={a->x,a->y-768,a->z,a->room,0},to={lara->x,lara->y-768,lara->z,lara->room,0};int rooms[1024],count=0;
    tr2_shot_los(l,&from,&to,0,rooms,&count);int object=tr2_object_on_los(l,w,&from,&to,rooms,count);
    if(object>=0 && w->objects[object].id==72)tr2_smash_window(l,w,w->objects+object);
}
/* ShootAtLara 0x4361b0 arithmetic; preserve x86 signed 32-bit overflow. */
int tr2_enemy_shot_chance(int distance,int speed,int facing){
    int lateral=tr2_asr(speed*tr2_sin(facing),14);
    int correction=tr2_long((int64_t)lateral*67108864)/300;
    if(tr2_long((int64_t)correction*correction+distance)>67108864)return -1;
    return 8192+(67108864-distance)/3276;
}
/* Stick2Control 0x41e4b0. */
int tr2_stick_decide(Tr2Object *o,Tr2EnemyDecision *c,int bite){
    Tr2Control *a=&o->pose.control;int damage=0;
    switch(a->current){
    case 1:
        c->attack=0;c->maximum_turn=0;
        if(c->mood==2)a->goal=10;
        else if(!c->mood)a->goal=c->required?c->required:c->random<16384?2:5;
        else a->goal=bite && c->distance<262144?7:bite && c->distance<1048576?6:bite && c->distance<4194304?2:10;
        break;
    case 5:if(c->mood)a->goal=1;else if(c->random<256){c->required=2;a->goal=1;}break;
    case 2:
        c->maximum_turn=910;
        if(c->mood==2)a->goal=10;
        else if(!c->mood){if(c->random<256){c->required=5;a->goal=1;}}
        else a->goal=bite && c->distance<262144?1:bite && c->distance<2359296?4:10;
        break;
    case 10:c->maximum_turn=1092;if(c->mood!=2 && (!c->mood || (c->ahead && c->distance<4194304)))a->goal=2;break;
    case 7:c->attack=0;a->goal=bite && c->distance<262144?9:1;break;
    case 6:c->attack=0;a->goal=c->ahead && c->distance<1048576?8:1;break;
    case 4:c->attack=0;a->goal=bite && c->distance<2359296?3:2;break;
    case 9:case 8:
        if(!c->attack && (c->touch&0x600)){c->attack=1;damage=80;}
        if(a->current==8 && c->ahead && c->distance>1048576 && c->distance<2359296)a->goal=3;
        break;
    case 3:if(c->attack!=2 && (c->touch&0x600)){c->attack=2;damage=100;}break;
    }return damage;
}
/* Post-animation half of CreatureVault 0x4100f0. */
int tr2_creature_vault_adjust(Tr2Actor *a,int old_x,int old_y,int old_z,int old_room,int floor,int shift){
    int vault=floor>old_y+896?-4:a->y>old_y-384?0:a->y>old_y-640?2:a->y>old_y-896?3:4;
    if(!vault)return 0;
    int ox=tr2_asr(old_x,10),oz=tr2_asr(old_z,10),x=tr2_asr(a->x,10),z=tr2_asr(a->z,10);
    if(oz==z){if(ox==x)return 0;if(ox<x){a->yaw=16384;a->x=x*1024-shift;}else{a->yaw=-16384;a->x=ox*1024+shift;}}
    else if(ox==x){if(oz<z){a->yaw=0;a->z=z*1024-shift;}else{a->yaw=-32768;a->z=oz*1024+shift;}}
    a->y=old_y;a->room=(int16_t)old_room;return vault;
}
/* Cult1Control 0x4363d0 decision layer. Shot LOS/effects stay separate. */
int tr2_cultist_decide(Tr2Object *o,Tr2EnemyDecision *c,int targetable,int same_zone){
    Tr2Control *a=&o->pose.control;
    switch(a->current){
    case 3:c->maximum_turn=0;if(c->required)a->goal=c->required;break;
    case 4:
        if(c->mood==2){c->required=2;a->goal=3;}
        else if(targetable){c->required=c->random<16384?6:10;a->goal=3;}
        else if(!c->mood && c->ahead){if(c->random<1280){c->required=5;a->goal=3;}else if(c->random<2560){c->required=1;a->goal=3;}}
        else{c->required=c->distance>=4194304 && c->mood?2:1;a->goal=3;}
        break;
    case 5:
        if(targetable){a->goal=3;c->required=6;}
        else if(c->mood || c->random<256 || !c->ahead)a->goal=3;
        break;
    case 1:
        c->maximum_turn=910;
        if(c->mood==2)a->goal=2;
        else if(targetable){a->goal=3;c->required=c->random<16384?6:10;}
        else if(c->distance>4194304 || !c->ahead)a->goal=2;
        else if(!c->mood && c->random<768)a->goal=3;
        break;
    case 2:
        c->maximum_turn=910;c->attack=0;
        if(c->mood==2){if(targetable)a->goal=9;}
        else if(targetable)a->goal=c->distance<4194304 || !same_zone?3:9;
        else if(!c->mood)a->goal=3;
        break;
    case 6:case 10:c->attack=0;a->goal=c->mood==2 || !targetable?3:a->current==6?7:11;break;
    case 7:case 11:if(!c->attack){c->attack=1;return 1;}break;
    case 9:if(!c->required){if(!targetable)a->goal=2;c->required=9;return 1;}break;
    }return 0;
}
/* Diver's surface query 0x416c70 follows vertical links only, unlike Lara's
   water-depth query. A fully submerged stack with no dry room returns -32512. */
int tr2_diver_surface(const Tr2Level *l,int x,int z,int room){
    if(room<0 || (uint32_t)room>=l->room_count)return TR2_NO_HEIGHT;
    int water=l->rooms[room].flags&1;
    for(uint32_t n=0;n<l->room_count;n++){
        const Tr2Room *r=l->rooms+room;int ix=tr2_asr(x-r->x,10),iz=tr2_asr(z-r->z,10);
        if(ix<0 || iz<0 || ix>=r->nx || iz>=r->nz)return TR2_NO_HEIGHT;
        const uint8_t *s=r->sectors.data+(ix*r->nz+iz)*8;int next=s[water?6:4];
        if(next==255 || (uint32_t)next>=l->room_count)return TR2_NO_HEIGHT;
        if((l->rooms[next].flags&1)!=water){unsigned h=s[water?7:5];return (h<128?(int)h:(int)h-256)*256;}
        room=next;
    }return TR2_NO_HEIGHT;
}
static void creature_float(const Tr2Level *l,Tr2World *w,int index){
    Tr2Object *o=w->objects+index;Tr2Actor *a=&o->pose;
        a->control.health=-16384;a->pitch=0;int surface=tr2_water_height(l,a->x,a->z,a->room,NULL);
        if(a->y>surface)a->y-=32;if(a->y<surface)a->y=surface;
        tr2_object_animate(l,o);Tr2Height h;Tr2Sector s;
        if(tr2_query(l,a->x,a->y,a->z,a->room,&h,&s)){o->floor=h.floor;tr2_world_room(w,index,s.room);}
}
/* DiverControl 0x416d80 decision boundary; surface is GetWaterSurface+512. */
int tr2_diver_decide(Tr2Object *o,Tr2EnemyDecision *c,int targetable,int target_y,int surface,int fly){
    Tr2Control *a=&o->pose.control;int rise=target_y<surface && o->pose.y<tr2_long((int64_t)surface+fly);
    switch(a->current){
    case 1:c->maximum_turn=546;if(rise)a->goal=2;else if(c->mood!=2 && targetable)a->goal=4;break;
    case 4:c->attack=0;a->goal=targetable && c->mood!=2 && !rise?3:1;break;
    case 2:c->maximum_turn=546;if(target_y>surface)a->goal=1;else if(c->mood!=2 && targetable)a->goal=6;break;
    case 6:c->attack=0;a->goal=targetable && c->mood!=2 && target_y<=surface?7:2;break;
    case 3:case 7:if(!c->attack){c->attack=1;return 1;}break;
    }return 0;
}
/* Worker1Control 0x436dc0. */
int tr2_worker1_decide(Tr2Object *o,Tr2EnemyDecision *c,int targetable,int same_zone){
    Tr2Control *a=&o->pose.control;int shot=0;
    switch(a->current){
    case 2:
        c->attack=0;c->maximum_turn=0;
        if(c->mood==2)a->goal=5;
        else if(targetable)a->goal=c->distance>=0x900000 && same_zone?1:c->random<16384?8:9;
        else if(!c->mood && c->ahead)a->goal=3;
        else a->goal=c->distance>0x400000?5:1;
        break;
    case 3:if(targetable)a->goal=4;else if(c->mood || !c->ahead)a->goal=2;break;
    case 1:
        c->attack=0;c->maximum_turn=546;
        if(c->mood==2)a->goal=5;
        else if(targetable)a->goal=c->distance<0x900000 || !same_zone?2:6;
        else if(!c->mood && c->ahead)a->goal=2;
        else if(c->distance>0x400000)a->goal=5;
        break;
    case 5:c->maximum_turn=910;if(c->mood!=2 && (targetable || !c->mood || c->mood==3))a->goal=1;break;
    case 9:if(targetable)a->goal=10;/* fall through */
    case 8:c->attack=0;break;
    case 4:case 10:
        if(!c->attack){shot=1;c->attack=1;}
        if(a->goal!=2 && (c->mood==2 || c->distance>0x900000 || !targetable))a->goal=2;
        break;
    case 6:if(!c->attack){shot=1;c->attack=1;}break;
    }return shot;
}
/* Worker2Control 0x4371c0: gunner31 and flamethrower34. */
int tr2_worker2_decide(Tr2Object *o,Tr2EnemyDecision *c,int targetable,int same_zone){
    Tr2Control *a=&o->pose.control;
    switch(a->current){
    case 1:
        c->attack=0;c->maximum_turn=0;
        if(c->mood==2)a->goal=3;
        else if(targetable)a->goal=c->distance>=0x900000 && same_zone?2:o->id==34 || c->random<16384?8:10;
        else if(!c->mood && c->ahead)a->goal=4;
        else a->goal=c->distance>0x400000?3:2;
        break;
    case 4:if(targetable)a->goal=5;else if(c->mood || !c->ahead)a->goal=1;break;
    case 2:
        c->attack=0;c->maximum_turn=546;
        if(c->mood==2)a->goal=3;
        else if(targetable)a->goal=c->distance<0x900000 || !same_zone?1:9;
        else if(!c->mood && c->ahead)a->goal=1;
        else if(c->distance>0x400000)a->goal=3;
        break;
    case 3:c->maximum_turn=910;if(c->mood!=2 && (targetable || !c->mood || c->mood==3))a->goal=2;break;
    case 8:case 10:
        c->attack=0;if(c->ahead)a->goal=targetable?(a->current==8?5:11):1;break;
    case 9:c->attack=0;if(c->ahead)a->goal=targetable?6:2;break;
    case 11:
        if(a->goal!=1 && (c->mood==2 || c->distance>0x900000 || !targetable))a->goal=1;
        /* fall through */
    case 5:case 6:
        if(o->id==34)return 1;
        if(!c->attack){c->attack=5;return 1;}c->attack=tr2_word(c->attack-1);break;
    }return 0;
}
/* Cult3Control 0x436850. Return bit0/bit1 for the left/right shot calls. */
void tr2_cultist3_init(const Tr2Level *l,Tr2World *w,Tr2Object *o){
    const uint8_t *model=tr2_model(l,20);if(!model)return;
    o->pose.yaw=tr2_word(o->pose.yaw+tr2_asr(enemy_random(&w->random_state)-16384,1));
    o->pose.control.flags|=32;tr2_set_animation(l,&o->pose,tr2_u16(model+16)+3,0);o->pose.control.current=o->pose.control.goal=2;
}
int tr2_cultist3_decide(Tr2Object *o,Tr2EnemyDecision *c,int targetable,int same_zone,int angle,int lara_health){
    Tr2Control *a=&o->pose.control;
    switch(a->current){
    case 1:case 2:
        if(!c->mood && lara_health<=0)a->goal=2;
        else if(targetable)a->goal=c->distance>0x900000?3:9;
        else if(c->mood==2)a->goal=4;
        else if(c->mood==1)a->goal=c->distance>0x1900000 || !c->ahead?4:3;
        else if(c->mood==3 || !c->ahead)a->goal=3;
        break;
    case 3:
        c->maximum_turn=546;
        if(targetable)a->goal=c->distance<0x900000 || !same_zone?1:angle<0?5:6;
        else if(c->mood==2)a->goal=4;
        else if(c->mood==1 || c->mood==3){if(c->distance>0x1900000 || !c->ahead)a->goal=4;}
        else if(lara_health<=0)a->goal=2;
        else if(c->ahead)a->goal=1;
        break;
    case 4:
        c->maximum_turn=546;
        if(targetable)a->goal=!same_zone?1:angle<0?5:6;
        else if(!c->mood)a->goal=lara_health<=0?2:1;
        else if(c->ahead && c->distance<0x1900000)a->goal=3;
        break;
    case 5:case 6:c->attack=0;a->goal=targetable?(a->current==5?7:8):3;break;
    case 9:c->attack=0;a->goal=targetable?10:1;break;
    case 7:case 8:case 10:if(!c->attack){c->attack=1;return a->current==7?1:a->current==8?2:3;}break;
    }return 0;
}
/* RatControl 0x438fa0, required-state attack latch is independent of flags. */
int tr2_rat_decide(Tr2Object *o,Tr2EnemyDecision *c){
    Tr2Control *a=&o->pose.control;
    switch(a->current){
    case 4:
        if(c->mood && c->mood!=3)c->required=c->distance<116281?5:1;
        else if(c->random<1280)c->required=3;else if(c->random>2560)c->required=1;
        if(c->required)a->goal=2;break;
    case 2:c->maximum_turn=0;if(c->required)a->goal=c->required;break;
    case 1:
        c->maximum_turn=1092;
        if(c->mood && c->mood!=3){if(c->ahead && c->distance<116281)a->goal=2;}
        else if(c->random<1280){c->required=3;a->goal=2;}else if(c->random<2560)a->goal=2;
        break;
    case 5:if(!c->required && (c->touch&127)){c->required=2;return 20;}break;
    case 3:if(c->random<1280)a->goal=2;break;
    }return 0;
}
/* DogControl 0x417130; state table at 0x4174b8. */
int tr2_dog_decide(Tr2Object *o,Tr2EnemyDecision *c){
    Tr2Control *a=&o->pose.control;int damage=0;
    switch(a->current){
    case 3:
        c->maximum_turn=0;c->attack=0;
        if(!c->mood){if(c->required)a->goal=c->required;else if(c->random<768)a->goal=4;else if(c->random<1536)a->goal=5;else if(c->random<9728)a->goal=1;}
        else if(c->mood==2)a->goal=2;
        else a->goal=c->distance<116281 && c->ahead?7:2;
        break;
    case 1:
        c->maximum_turn=546;
        if(c->mood)a->goal=2;
        else if(c->random<768){c->required=4;a->goal=3;}
        else if(c->random<1536){c->required=5;a->goal=3;}
        else if(c->random<2816)a->goal=3;
        break;
    case 2:c->maximum_turn=1092;if(!c->mood)a->goal=3;else if(c->distance<589824)a->goal=8;break;
    case 7:
        c->maximum_turn=0;
        if(c->attack!=1 && c->ahead && (c->touch&0x13f7000)){c->attack=1;damage=100;}
        a->goal=c->distance>116281 && c->distance<465124?9:3;break;
    case 8:
        if(c->attack!=2 && (c->touch&0x13f7000)){c->attack=2;damage=200;}
        if(c->distance<116281)a->goal=7;else if(c->distance<465124)a->goal=9;break;
    case 9:
        c->maximum_turn=1092;
        if(c->attack!=3 && (c->touch&0x13f7000)){c->attack=3;damage=100;}
        if(c->distance<116281)a->goal=7;break;
    case 4:if(c->mood || c->random<1280)a->goal=3;break;
    case 5:if(c->mood || c->random<256)a->goal=3;break;
    case 6:if(c->mood || c->random<512)a->goal=3;break;
    }return damage;
}
int tr2_spider_decide(Tr2Object *o,Tr2EnemyDecision *c){
    Tr2Control *a=&o->pose.control;
    switch(a->current){
    case 1:
        c->attack=0;
        if(!c->mood){if(c->random<256)a->goal=2;}
        else if(c->ahead && c->touch)a->goal=4;
        else if(c->mood==3)a->goal=2;
        else if(c->mood==1 || c->mood==2)a->goal=3;
        break;
    case 2:
        if(!c->mood){if(c->random<256)a->goal=1;}
        else if(c->mood==1 || c->mood==2)a->goal=3;
        break;
    case 3:
        c->attack=0;
        if(!c->mood || c->mood==3)a->goal=2;
        else if(c->ahead && c->touch)a->goal=1;
        else if(c->ahead && c->distance<41616)a->goal=6;
        else if(c->ahead && c->distance<262144)a->goal=5;
        break;
    case 4:case 5:case 6:
        if(!c->attack && c->touch){c->attack=1;return 25;}
        break;
    }return 0;
}
int tr2_tiger_decide(Tr2Object *o,Tr2EnemyDecision *c,int lara_speed){
    Tr2Control *a=&o->pose.control;
    switch(a->current){
    case 1:
        c->maximum_turn=0;c->attack=0;
        if(c->mood==2)a->goal=3;
        else if(!c->mood)a->goal=2; /* Original overwrites the random roar. */
        else if(c->ahead && c->distance<116281)a->goal=6;
        else if(c->ahead && c->distance<1048576){c->maximum_turn=546;a->goal=8;}
        else if(c->required)a->goal=c->required;
        else if(c->mood!=1 && c->random<96)a->goal=5;
        else a->goal=3;
        break;
    case 2:
        c->maximum_turn=546;if(c->random<96){a->goal=1;c->required=5;}break;
    case 3:
        c->maximum_turn=1092;
        if(!c->mood)a->goal=1;
        else if(c->attack && c->ahead){a->goal=1;c->attack=0;return 0;}
        else if(!c->attack && c->ahead && c->distance<2359296){
            if(!lara_speed)a->goal=1;else a->goal=7;
            c->attack=0;return 0;
        }else if(c->mood!=1 && c->random<96){c->required=5;a->goal=1;}
        c->attack=0;break;
    case 6:case 7:case 8:
        if(!c->attack && (c->touch&0x07fdc000)){c->attack=1;return 100;}break;
    }return 0;
}
int tr2_crow_decide(Tr2Object *o,Tr2EnemyDecision *c,int floor){
    Tr2Control *a=&o->pose.control;
    switch(a->current){
    case 2:case 7:o->pose.y=floor;if(c->mood)a->goal=1;break;
    case 1:
        c->attack=0;if(c->required)a->goal=c->required;
        if(!c->mood)a->goal=2;
        else if(c->ahead && c->distance<262144)a->goal=6;
        else a->goal=3;
        break;
    case 3:
        if(!c->mood){c->required=2;a->goal=1;}
        else if(c->ahead && c->distance<262144)a->goal=6;
        break;
    case 6:if(!c->attack && c->touch){c->attack=1;return 20;}break;
    }return 0;
}
/* TRexControl 0x418670; return its jaw-kill request separately from trampling. */
int tr2_trex_decide(Tr2Object *o,Tr2EnemyDecision *c,int bite,int facing){
    Tr2Control *a=&o->pose.control;
    if(a->health<=0){a->goal=a->current==1?5:1;return 0;}
    c->attack=(c->mood!=2 && !c->ahead && facing> -16384 && facing<16384) || (c->distance>0x225510 && c->distance<0x1000000 && bite);
    switch(a->current){
    case 1: a->goal=c->required?c->required:c->distance<0x225510 && bite?7:c->mood && !c->attack?3:2;break;
    case 2:c->maximum_turn=364;if(c->mood && !c->attack)a->goal=1;else if(c->ahead && c->random<512){c->required=6;a->goal=1;}break;
    case 3:c->maximum_turn=728;if((c->distance<0x1900000 && bite) || c->attack)a->goal=1;else if(c->mood!=2 && c->ahead && c->random<512){c->required=6;a->goal=1;}else if(!c->mood)a->goal=1;break;
    case 7:if(c->touch&0x3000)return 1;c->required=2;break;
    }return 0;
}
static int camera_distance(const Tr2World *w,int index){
    const Tr2Actor *a=w->level && index==w->level->lara && w->lara?w->lara:&w->objects[index].pose;int dx=tr2_asr(a->x-w->ai_camera[0],8),dy=tr2_asr(a->y-w->ai_camera[1],8),dz=tr2_asr(a->z-w->ai_camera[2],8);
    return tr2_long((int64_t)dx*dx+(int64_t)dy*dy+(int64_t)dz*dz);
}
int tr2_creature_head(int current,int target){
    int delta=tr2_word(target-current);if(delta>910)delta=910;else if(delta< -910)delta=-910;int value=tr2_word(current+delta);return value>12288?12288:value< -12288?-12288:value;
}
int tr2_creature_joint_rotation(int id,int joint,const Tr2Creature *c){
    if(c && id==19)return joint==1?c->head:joint==9?c->neck:0;
    if(c && id==41)return joint==7?c->head:joint==13?c->neck:0;
    if(c && id==43)return joint==7?c->head:joint==17?c->neck:0;
    if(c && id==22 && joint==11)return c->head;
    if(c && (id==53 || id==54))return joint==7?c->head:0;
    if(c && id==46)return joint==15?c->head:0;
    if(c && id==45)return joint==7?c->head:joint==15?c->neck:0;
    if(c && (id==48 || id==49 || id==50))return joint==7?c->head:joint==9?c->neck:0;
    if(c && id==25)return joint==10?c->head:0;
    if(c && id==28)return joint==7?c->head:0;
    if(c && id==29)return joint==11?c->head:joint==15?c->neck:0;
    if(c && (id==30 || id==31 || id==34)){if(joint==5)return c->head;if(joint==14)return c->neck;return 0;}
    if(c && id==20 && joint>=0 && joint<16){unsigned bit=1u<<joint;if(!(c->joint_mask&bit))return 0;return (c->joint_mask&(bit-1))?c->neck:c->head;}
    if(!c)return 0;if(id==32 || id==33){if(joint==1)return c->head;if(joint==5)return c->neck;}if(id>=16 && id<=18 && joint==1)return c->head;if(id==21 && joint==4)return c->head;if(id==15 && joint==20)return c->head;if(id==39 && joint==22)return c->head;if(id==214){if(joint==11)return c->head;if(joint==12)return c->neck;}return 0;
}
/* Cult3Control's dynamic bone flags and ordered head/neck rotation slots. */
void tr2_cultist3_pose(Tr2Creature *c,int state,int angle,int ahead){
    int torso=0,left=0,right=0,head=0;
    if(ahead){
        if(state==9 || state==10)torso=angle;
        else if(state==5 || state==7)left=head=angle;
        else if(state==6 || state==8)right=head=angle;
        else if(state>=1 && state<=4)head=angle;
    }
    c->joint_mask=(uint16_t)((torso?2:0)|(left?8:0)|(right?128:0)|(head?2048:0));
    if(torso)c->head=(int16_t)tr2_creature_head(c->head,torso);
    else if(left || right){c->head=(int16_t)tr2_creature_head(c->head,left?left:right);c->neck=(int16_t)tr2_creature_head(c->neck,head);}
    else if(head)c->head=(int16_t)tr2_creature_head(c->head,head);
}
/* EnableBaddieAI 0x432bc0: five slots, evict only a strictly more distant
   creature, measuring distance from the camera in 256-unit steps. */
int tr2_creature_slot(const Tr2World *w,int index,int force){
    if((uint32_t)index>=w->count)return -1;
    for(int i=0;i<5;i++)if(w->creature_slots[i]==index)return i;
    for(int i=0;i<5;i++)if(w->creature_slots[i]<0)return i;
    int distance=force?0:camera_distance(w,index),slot=-1;
    for(int i=0;i<5;i++){int n=w->creature_slots[i];if((uint32_t)n>=w->count)continue;int d=camera_distance(w,n);if(d>distance){distance=d;slot=i;}}
    return slot;
}
void tr2_enemy_disable(Tr2World *w,int index){
    if((uint32_t)index>=w->count)return;
    for(int i=0;i<5;i++)if(w->creature_slots[i]==index)w->creature_slots[i]=-1;
    free(w->creatures[index].lot.nodes);memset(w->creatures+index,0,sizeof *w->creatures);
}
/* CreatureDie 0x40f440: a finished corpse must never be triggered again. */
void tr2_enemy_finish(Tr2World *w,int index){
    if((uint32_t)index>=w->count)return;Tr2Object *o=w->objects+index;
    o->active=0;o->flags|=256;o->pose.control.health=-16384;o->pose.control.flags&=(uint16_t)~32u;
    tr2_enemy_disable(w,index);
    tr2_carried_drop(w,index);
}
int tr2_enemy_enable(const Tr2Level *l,Tr2World *w,int index){
    if((uint32_t)index>=w->count)return 0;Tr2Object *o=w->objects+index;Tr2Creature *c=w->creatures+index;Tr2Lot *lot=&c->lot;
    if(lot->nodes)return 1;
    if((o->id<15 || o->id>18) && o->id!=20 && o->id!=21 && o->id!=25 && o->id!=28 && o->id!=29 && o->id!=30 && o->id!=31 && o->id!=34 && o->id!=32 && o->id!=33 && o->id!=36 && o->id!=38 && !tr2_xian_enemy(o->id) && !tr2_tibetan_enemy(o->id) && o->id!=47 && o->id!=49 && o->id!=50 && o->id!=51 && o->id!=39 && o->id!=214)return 0;
    int box=box_at(l,&o->pose);if(box<0)return 0;
    int slot=tr2_creature_slot(w,index,o->status==0 || o->id==51);if(slot<0)return 0;
    lot->nodes=calloc(l->boxes.count,sizeof *lot->nodes);if(!lot->nodes)return 0;
    int victim=w->creature_slots[slot];if(victim>=0 && victim!=index){w->objects[victim].status=3;tr2_enemy_disable(w,victim);}w->creature_slots[slot]=index;
    lot->head=lot->tail=lot->target_box=lot->required_box=-1;lot->block=(o->id==25 || o->id==214)?0x8000:0x4000;
    lot->step=(o->id==45 || o->id==32 || o->id==33)?1024:(o->id==36 || o->id==51)?512:(o->id==25 || o->id==28 || o->id==29 || o->id==38 || o->id==47)?20480:256;lot->drop=(o->id==45 || o->id==32 || o->id==33) || o->id==36 || o->id==51?-1024:(o->id==25 || o->id==28 || o->id==29 || o->id==38 || o->id==47)?-20480:-512;lot->fly=(o->id==25 || o->id==28 || o->id==29 || o->id==38 || o->id==47)?16:0;c->maximum_turn=182;
    int zone_index=lot->fly?4:(lot->step>>8)-1;const uint8_t *normal=l->zones.data+zone_index*l->boxes.count*2,*flip=normal+l->boxes.count*10;
    for(uint32_t j=0;j<l->boxes.count;j++){
        lot->nodes[j].exit=lot->nodes[j].next=-1;
        if(tr2_i16(normal+j*2)==tr2_i16(normal+box*2) || tr2_i16(flip+j*2)==tr2_i16(flip+box*2))lot->nodes[lot->zone_count++].box=(int16_t)j;
    }return 1;
}
/* TestTriggers current action 0x41564b and Lara's separate AI data pointer.
   Lara shares the five LOT slots, but does not build a creature zone list. */
void tr2_current_trigger(const Tr2Level *l,Tr2World *w,const Tr2Actor *a,int sink){
    if((uint32_t)sink>=l->cameras.count || (uint32_t)l->lara>=w->count)return;
    int index=l->lara;w->lara=a;Tr2Creature *c=w->creatures+index;Tr2Lot *lot=&c->lot;
    if(!lot->nodes){
        int slot=tr2_creature_slot(w,index,1);if(slot<0)return;
        Tr2LotNode *nodes=calloc(l->boxes.count,sizeof *nodes);if(!nodes)return;
        int victim=w->creature_slots[slot];if(victim>=0 && victim!=index){w->objects[victim].status=3;tr2_enemy_disable(w,victim);}
        memset(c,0,sizeof *c);lot->nodes=nodes;w->creature_slots[slot]=index;c->maximum_turn=182;
        lot->head=lot->tail=lot->target_box=lot->required_box=-1;lot->step=20480;lot->drop=-20480;lot->fly=256;lot->block=0x4000;
        for(uint32_t i=0;i<l->boxes.count;i++)nodes[i].exit=nodes[i].next=-1;
    }
    const uint8_t *p=l->cameras.data+sink*16;
    lot->target[0]=tr2_i32(p);lot->target[1]=tr2_i32(p+4);lot->target[2]=tr2_i32(p+8);lot->required_box=tr2_i16(p+14);
    w->current_strength=tr2_word(tr2_i16(p+12)*6);
}
static int decision_random(const Tr2Object *o,const Tr2EnemyDecision *c){
    int state=o->pose.control.current;
    if((o->id==32 || o->id==33))return !c->mood && ((state==1 && !c->required) || state==2 || state==5);
    if(o->id==21)return state==3 || ((state==1 || state==4) && (!c->mood || c->mood==3));
    if(o->id==15)return !c->mood && (state==1 || (state==3 && !c->required) || (state>=4 && state<=6));
    if(o->id==36)return (state==1 || state==2) && !c->mood;
    if(o->id!=39)return 0;
    if(state==2)return 1;
    if(state==1)return !c->mood || (c->mood!=1 && c->mood!=2 && !(c->ahead && c->distance<1048576) && !c->required);
    return state==3 && c->mood && c->mood!=1 && !(c->attack && c->ahead) && !(!c->attack && c->ahead && c->distance<2359296);
}
/* DiverControl 0x416d80; Float 0x40ff90 deliberately retains its AI slot. */
static void diver_tick(const Tr2Level *l,Tr2World *w,int index,Tr2Actor *lara){
    Tr2Object *o=w->objects+index;Tr2Actor *a=&o->pose;Tr2Creature *c=w->creatures+index;
    if(a->control.health<=0){
        if(a->control.current!=9){const uint8_t *m=tr2_model(l,29);tr2_set_animation(l,a,tr2_u16(m+16)+16,0);a->control.current=9;}
        creature_float(l,w,index);return;
    }
    Tr2EnemyInfo info;int box,enemy_box;
    if(!tr2_enemy_info(l,&c->lot,a,lara,0,&info,&box,&enemy_box))return;
    tr2_enemy_target(l,c,a,lara,box,enemy_box,0,info.distance,&w->random_state);
    int visible=0,rooms[1024],count=0;
    Tr2Position from={a->x,a->y,a->z,a->room,0},to={lara->x,lara->y,lara->z,lara->room,0};
    if(!lara->control.water_status){
        from.y-=256;to.y-=612;visible=tr2_shot_los(l,&from,&to,0,rooms,&count);
        if(visible){c->target[0]=lara->x;c->target[1]=lara->y;c->target[2]=lara->z;}
        if(info.angle< -8192 || info.angle>8192)visible=0;
    }else if(info.angle> -8192 && info.angle<8192)visible=tr2_shot_los(l,&from,&to,0,rooms,&count);
    int turn=tr2_creature_turn(a,c->target[0],c->target[2],c->maximum_turn);
    int surface=tr2_diver_surface(l,a->x,a->z,a->room)+512,state=a->control.current,head=0,neck=0;
    if(visible){if(state==1 || state==3 || state==4)neck=-info.angle;else if(state==2 || state==6 || state==7)head=info.angle;}
    Tr2EnemyDecision decision={c->mood,info.distance,info.ahead,0,0,c->required,c->attack,c->maximum_turn};
    if(tr2_diver_decide(o,&decision,visible,c->target[1],surface,c->lot.fly)){
        int point[3]={17,164,44};if(tr2_joint(l,a,29,18,point)){Tr2Position p={point[0],point[1],point[2],a->room,0};tr2_diver_harpoon(l,w,&p,lara);}
    }
    c->attack=decision.attack;c->maximum_turn=decision.maximum_turn;
    c->head=(int16_t)tr2_creature_head(c->head,head);c->neck=(int16_t)tr2_creature_head(c->neck,neck);
    a->move_angle=a->yaw;tr2_creature_move(l,o,&c->lot,box,turn,341,c->target[1]);
    state=a->control.current;
    if(state==1 || state==3 || state==4){int water=tr2_water_height(l,a->x,a->z,a->room,NULL)+512;if(a->y<water){a->y=water;if(a->pitch>364)a->pitch-=364;else if(a->pitch>0)a->pitch=0;}}
    else a->y=surface-512;
}
/* BarracudaControl 0x43ca20. Only jaw contact during an attack can bite. */
int tr2_barracuda_decide(Tr2Object *o,Tr2EnemyDecision *c){
    Tr2Control *a=&o->pose.control;
    switch(a->current){
    case 1:
        c->attack=0;
        if(!c->mood)a->goal=2;
        else if(c->ahead && c->distance<0x718e4)a->goal=4;
        else a->goal=c->mood==3?2:3;
        break;
    case 2:
        c->maximum_turn=364;
        if(c->mood){
            if(c->ahead && (c->touch&0xe0))a->goal=1;
            else if(c->mood!=3)a->goal=3;
        }
        break;
    case 3:
        c->maximum_turn=728;c->attack=0;
        if(!c->mood)a->goal=2;
        else if(c->ahead && c->distance<0x1c639)a->goal=5;
        else if(c->ahead && c->distance<0x718e4)a->goal=1;
        else if(c->mood==3)a->goal=2;
        break;
    case 4:case 5:
        if(!c->attack && (c->touch&0xe0)){c->attack=1;return 100;}
        break;
    }
    return 0;
}
/* SharkControl 0x43cc50, including the control RNG gate at 0x43cdea. */
static int shark_decide(Tr2Control *a,Tr2EnemyDecision *c,int same_zone,uint32_t *random){
    switch(a->current){
    case 0:
        c->attack=0;c->maximum_turn=0;
        a->goal=c->ahead && c->distance<0x90000 && same_zone?3:1;
        break;
    case 1:
        c->maximum_turn=91;
        if(c->mood){
            if(c->ahead && c->distance<0x90000)a->goal=0;
            else if(c->mood==2 || c->distance>0x900000 || !c->ahead)a->goal=2;
        }
        break;
    case 2:
        c->attack=0;c->maximum_turn=364;
        if(!c->mood)a->goal=1;
        else if(c->mood!=2 && c->ahead && c->distance<0x1c6e39 && same_zone){
            if(enemy_random(random)<2048)a->goal=0;
            else if(c->distance<0x90000)a->goal=4;
        }
        break;
    case 3:case 4:
        if(!c->attack && (c->touch&0x3400)){c->attack=1;return 400;}
        break;
    }
    return 0;
}
/* CreatureKill 0x410230 with the shark's animation and extra state. */
static void creature_kill(const Tr2Level *l,Tr2World *w,Tr2Actor *shark,Tr2Actor *lara,int id,int offset,int state,int extra_state){
    const uint8_t *model=tr2_model(l,id),*extra=tr2_model(l,12);
    if(!model || !extra)return;
    tr2_set_animation(l,shark,tr2_u16(model+16)+offset,0);shark->control.current=state;
    tr2_set_animation(l,lara,tr2_u16(extra+16),0);
    lara->control.current=0;lara->control.goal=extra_state;
    lara->x=shark->x;lara->y=shark->y;lara->z=shark->z;lara->room=shark->room;
    lara->yaw=shark->yaw;lara->pitch=shark->pitch;lara->control.roll=shark->control.roll;
    lara->speed=lara->fall_speed=0;lara->control.flags&=(uint16_t)~8u;
    Tr2Object pose={0};pose.pose=*lara;tr2_object_animate(l,&pose);*lara=pose.pose;
    lara->control.goal=lara->control.current=extra_state;lara->extra_animation=1;lara->gun_status=1;
    w->combat->weapon=0;w->hit_direction=-1;
}
static void fish_tick(const Tr2Level *l,Tr2World *w,int index,Tr2Actor *lara){
    Tr2Object *o=w->objects+index;Tr2Actor *a=&o->pose;Tr2Creature *c=w->creatures+index;
    int shark=o->id==25,radius=shark?341:204;
    if(a->control.health<=0){
        int death=shark?5:6;if(a->control.current!=death){const uint8_t *m=tr2_model(l,o->id);tr2_set_animation(l,a,tr2_u16(m+16)+(shark?4:6),0);a->control.current=(int16_t)death;}
        creature_float(l,w,index);return;
    }
    Tr2EnemyInfo info;int box,enemy_box;
    if(!tr2_enemy_info(l,&c->lot,a,lara,200,&info,&box,&enemy_box))return;
    tr2_enemy_target(l,c,a,lara,box,enemy_box,shark,info.distance,&w->random_state);
    int turn=tr2_creature_turn(a,c->target[0],c->target[2],c->maximum_turn);
    uint32_t touch=tr2_object_overlap(l,a,lara,lara->control.water_status==1?300:100)?tr2_touch(l,a,o->id,lara,0):0;
    Tr2EnemyDecision decision={c->mood,info.distance,info.ahead,0,touch,c->required,c->attack,c->maximum_turn};
    int attack_state=shark?3:4;int head=info.ahead && (a->control.current==attack_state || a->control.current==attack_state+1)?info.angle:0;
    int damage=shark?shark_decide(&a->control,&decision,info.zone==info.enemy_zone,&w->random_state):tr2_barracuda_decide(o,&decision);
    if(damage){
        int alive=lara->control.health>0;
        lara->control.health=tr2_word(lara->control.health-damage);lara->control.flags|=16;
        int point[3]={shark?17:2,shark?-22:-60,shark?344:121};
        if(tr2_joint(l,a,o->id,shark?12:7,point)){Tr2Position p={point[0],point[1],point[2],a->room,0};tr2_blood(w,&p,a->speed,a->yaw);}
        if(shark && alive && lara->control.health<=0){c->attack=decision.attack;creature_kill(l,w,a,lara,25,19,6,3);return;}
    }
    c->attack=decision.attack;c->maximum_turn=decision.maximum_turn;
    if(shark && a->control.current==6){tr2_object_animate(l,o);return;}
    c->head=(int16_t)tr2_creature_head(c->head,head);
    a->move_angle=a->yaw;tr2_creature_move(l,o,&c->lot,box,turn,radius,c->target[1]);
    int depth=tr2_water_height(l,a->x,a->z,a->room,NULL)+(shark?341:256);
    if(a->y<depth){a->y=depth;if(a->pitch>364)a->pitch-=364;else if(a->pitch>0)a->pitch=0;}
}
/* EelControl 0x41c2c0. Item data stores distance from the hole. */
void tr2_eel_tick(const Tr2Level *l,Tr2World *w,int index,Tr2Actor *lara,uint32_t touch){
    Tr2Object *o=&w->objects[index];
    Tr2Actor *eel=&o->pose;
    int extension=o->effect_counter;
    int large=o->id==27;
    int16_t *required=&w->creatures[index].required;
    eel->x-=tr2_asr(extension*tr2_sin(eel->yaw),14);
    eel->z-=tr2_asr(extension*tr2_cos(eel->yaw),14);
    if(eel->control.health<=0){
        if(extension<(large?3584:1536))extension+=102;
        if(eel->control.current!=3){
            const uint8_t *model=tr2_model(l,o->id);
            tr2_set_animation(l,eel,tr2_u16(model+16)+(large?2:3),0);
            eel->control.current=3;
        }
    }else{
        int dx=lara->x-eel->x,dz=lara->z-eel->z;
        int distance=(int)sqrt((double)((uint32_t)dx*(uint32_t)dx+(uint32_t)dz*(uint32_t)dz));
        int angle=tr2_word(tr2_atan(dz,dx));
        if(eel->control.current==2){
            if(extension>0)extension-=102;
            int facing=tr2_word((eel->yaw+8192)&0xc000);
            int difference=large?tr2_word(angle-eel->yaw):angle-facing;
            if(distance<=(large?6144:2048) && abs(difference)<1820)eel->control.goal=1;
        }else if(eel->control.current==1){
            if(extension<distance-(large?2560:512))extension+=102;
            if(!large && angle<eel->yaw-91)eel->yaw=tr2_word(eel->yaw-91);
            else if(!large && angle>eel->yaw+91)eel->yaw=tr2_word(eel->yaw+91);
            if(!*required && (touch&0x180)){
                lara->control.health=tr2_word(lara->control.health-(large?500:50));lara->control.flags|=16;
                int point[3]={7,157,333};
                if(tr2_joint(l,eel,o->id,7,point)){
                    Tr2Position p={point[0],point[1],point[2],eel->room,0};
                    tr2_blood(w,&p,eel->speed,eel->yaw);
                }
                *required=2;
            }
        }
    }
    eel->x+=tr2_asr(extension*tr2_sin(eel->yaw),14);
    eel->z+=tr2_asr(extension*tr2_cos(eel->yaw),14);
    o->effect_counter=extension;
    int previous=eel->control.current;
    tr2_object_animate(l,o);
    if(previous!=eel->control.current && *required==eel->control.current)*required=0;
}
static int target_distance(const Tr2Actor *a,const Tr2Actor *b){
    int dx=tr2_asr(a->x-b->x,6),dy=tr2_asr(a->y-b->y,6),dz=tr2_asr(a->z-b->z,6);
    return dx*dx+dy*dy+dz*dz;
}
/* Same five active AI slots used by GetBaddieTarget (4103a0). */
static Tr2Actor *monastery_target(Tr2World *w,Tr2Object *o,Tr2Actor *lara){
    int monk=o->id==53 || o->id==54;
    Tr2Actor *best=NULL;int distance=INT32_MAX;
    for(int i=0;i<5;i++){
        int index=w->creature_slots[i];if((uint32_t)index>=w->count)continue;
        Tr2Object *t=w->objects+index;
        if(monk?(t->id!=48 && t->id!=49):(t->id!=53 && t->id!=54))continue;
        int d=target_distance(&t->pose,&o->pose);
        if(d<distance){distance=d;best=&t->pose;}
    }
    if((!monk || o->linked[1]) && target_distance(lara,&o->pose)<distance)best=lara;
    return best;
}
static void bite_blood(const Tr2Level *l,Tr2World *w,Tr2Object *o,int joint,int x,int y,int z){
    int p[3]={x,y,z};
    if(tr2_joint(l,&o->pose,o->id,joint,p)){
        Tr2Position pos={p[0],p[1],p[2],o->pose.room,0};tr2_blood(w,&pos,o->pose.speed,o->pose.yaw);
    }
}
static void tibet_damage(Tr2Actor *target,int amount){
    target->control.health=tr2_word(target->control.health-amount);target->control.flags|=16;
}
/* MonkControl (41dfe0). Low twelve attack bits count damage from Lara. */
static void monk_control(Tr2World *w,Tr2Object *o,Tr2Creature *c,const Tr2EnemyInfo *i,Tr2Actor *target,Tr2Actor *lara,uint32_t touch){
    Tr2Control *a=&o->pose.control;int state=a->current,near=i->ahead && i->distance<0x40000;
    int watched=!o->linked[1] && i->ahead && w->combat->target==o-w->objects;
    switch(state){
    case 1:case 11:
        c->attack&=4095;if(watched)break;
        if(!c->mood)a->goal=2;
        else if(c->mood==2)a->goal=3;
        else if(near){
            int r=enemy_random(&w->random_state);
            a->goal=state==1?(r<0x7000?4:11):(r<0x3000?5:r<0x6000?8:1);
        }else if(state==1 && i->ahead && i->distance<0x100000)a->goal=7;
        else a->goal=i->ahead && i->distance<0x400000?2:3;
        break;
    case 2:
        c->maximum_turn=546;
        if(!c->mood){if(watched)a->goal=enemy_random(&w->random_state)<16384?1:11;}
        else if(c->mood==2)a->goal=3;
        else if(near)a->goal=enemy_random(&w->random_state)<16384?1:11;
        else if(!i->ahead || i->distance>0x400000)a->goal=3;
        break;
    case 3:
        c->attack&=4095;c->maximum_turn=o->linked[1]?910:728;
        if(!c->mood)a->goal=1;
        else if(c->mood!=2 && near)a->goal=enemy_random(&w->random_state)<16384?1:11;
        else if(c->mood!=2 && i->ahead && i->distance<0x900000)a->goal=10;
        break;
    case 8:a->goal=near?6:11;break;
    case 4:case 5:case 6:case 7:case 10:
        if(!target || (c->attack&0xf000))break;
        if(target==lara ? (touch&0x4000)!=0 :
           abs(target->x-o->pose.x)<512 && abs(target->y-o->pose.y)<512 && abs(target->z-o->pose.z)<512){
            tibet_damage(target,target==lara?150:5);c->attack|=4096;
            if(o->pose.event_count<16){int e=o->pose.event_count++;o->pose.event_kind[e]=5;o->pose.event_value[e]=245;}
        }
        break;
    }
}
static void yeti_control(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Creature *c,const Tr2EnemyInfo *i,Tr2Actor *lara,uint32_t touch){
    Tr2Control *a=&o->pose.control;
    int near=i->ahead && i->distance<0x718e4,alive=lara->control.health>0,r;
    switch(a->current){
    case 2:
        c->attack=0;c->maximum_turn=0;
        if(c->mood==2)a->goal=1;
        else if(c->required)a->goal=c->required;
        else if(!c->mood){
            r=enemy_random(&w->random_state);
            if(r<256 || !alive)a->goal=7;else if(r<512)a->goal=9;else if(r<768)a->goal=3;
        }else if(i->ahead && i->distance<0x40000 && enemy_random(&w->random_state)<16384)a->goal=4;
        else a->goal=near?5:c->mood==3?3:1;
        break;
    case 7:case 9:
        if(c->mood==2 || (a->flags&16))a->goal=2;
        else if(c->mood){if(enemy_random(&w->random_state)<512)a->goal=2;}
        else if(alive || a->current==9){
            r=enemy_random(&w->random_state);
            if(r<256 || !alive)a->goal=a->current==7?2:7;
            else if(r<512)a->goal=a->current==7?9:2;
            else if(r<768){a->goal=2;c->required=3;}
        }
        break;
    case 3:
        c->maximum_turn=728;
        if(c->mood==2)a->goal=1;
        else if(!c->mood){
            r=enemy_random(&w->random_state);
            if(r<768 || !alive){a->goal=2;c->required=r<256 || !alive?7:r<512?9:0;}
        }else if(c->mood==1){if(near)a->goal=2;else if(i->distance>0x400000)a->goal=1;}
        break;
    case 1:
        c->attack=0;c->maximum_turn=1092;
        if(!c->mood)a->goal=3;
        else if(c->mood!=2){if(near)a->goal=2;else if(i->ahead && i->distance<0x400000)a->goal=6;else if(c->mood==3)a->goal=3;}
        break;
    case 4:case 5:case 6:
        if(a->current==5)c->maximum_turn=728;
        if(!c->attack && (touch&(a->current==4?0x700:0x3f00))){
            if(touch&0x700)bite_blood(l,w,o,10,12,101,19);
            if(a->current!=4 && (touch&0x3800))bite_blood(l,w,o,13,12,101,19);
            tibet_damage(lara,50*a->current-100);c->attack=1;
        }
        break;
    }
}
static void guardian_control(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Creature *c,const Tr2EnemyInfo *i,Tr2Actor *lara,uint32_t touch){
    Tr2Control *a=&o->pose.control;
    int close=i->ahead && i->distance<0x100000,reach=i->ahead && i->distance<0x400000;
    switch(a->current){
    case 1:c->maximum_turn=0;a->goal=close?(enemy_random(&w->random_state)<16384?3:10):((!c->mood || c->mood==3) && i->ahead?8:2);break;
    case 2:c->maximum_turn=728;if(reach)a->goal=5;else if((!c->mood || c->mood==3) && i->ahead)a->goal=1;break;
    case 8:if(c->mood || !i->ahead)a->goal=1;break;
    case 3:c->attack=0;a->goal=close?4:1;break;
    case 5:c->attack=0;a->goal=reach?6:2;break;
    case 10:c->attack=0;a->goal=close?11:1;break;
    case 4:case 6:case 7:case 11:
        if(!(c->attack&1) && (touch&0x600000)){bite_blood(l,w,o,22,0,224,0);tibet_damage(lara,200);c->attack|=1;}
        if(!(c->attack&2) && (touch&0xc0000)){bite_blood(l,w,o,19,0,224,0);tibet_damage(lara,200);c->attack|=2;}
        break;
    }
}
static void tibetan_enemy_tick(const Tr2Level *l,Tr2World *w,int index,Tr2Actor *lara){
    Tr2Object *o=w->objects+index;Tr2Actor *a=&o->pose;Tr2Creature *c=w->creatures+index;
    const uint8_t *m=tr2_model(l,o->id);if(!m)return;
    int base=tr2_u16(m+16),state=a->control.current,box=box_at(l,a),turn=0;
    int monk=o->id==53 || o->id==54;
    if(a->control.health<=0){
        int dead=o->id==48?13:o->id==45?8:9;
        if(state!=dead){
            int offset=o->id==48?14:o->id==45?31:20;
            if(monk)offset+=enemy_random(&w->random_state)>>14;
            tr2_set_animation(l,a,base+offset,0);a->control.current=dead;
        }
        if(o->id==46 && a->control.frame==tr2_i16(l->animations.data+a->control.animation*32+18))w->finished=1;
    }else{
        Tr2Actor *target=(monk || o->id==48)?monastery_target(w,o,lara):lara;
        Tr2Actor *view=target?target:lara;
        int enemy_box;Tr2EnemyInfo info;
        if(!tr2_enemy_info(l,&c->lot,a,view,o->id==45?100:0,&info,&box,&enemy_box))return;
        if(target)tr2_enemy_target(l,c,a,view,box,enemy_box,o->id!=48,info.distance,&w->random_state);
        else{
            Tr2Actor idle=*view;idle.control.health=0;
            tr2_enemy_target(l,c,a,&idle,box,enemy_box,1,info.distance,&w->random_state);
        }
        turn=tr2_creature_turn(a,c->target[0],c->target[2],c->maximum_turn);
        uint32_t touch=tr2_object_overlap(l,a,lara,100)?tr2_touch(l,a,o->id,lara,0):0;
        int alive=lara->control.health>0,angle=info.ahead?info.angle:0;
        int aiming=o->id==45?(state>=4 && state<=6):o->id==48?(state!=1 && state!=2 && state!=3):1;
        c->head=tr2_creature_head(c->head,aiming?angle:0);
        c->neck=tr2_creature_head(c->neck,aiming?0:angle);
        if(monk)monk_control(w,o,c,&info,target,lara,touch);
        else if(o->id==45)yeti_control(l,w,o,c,&info,lara,touch);
        else if(o->id==46)guardian_control(l,w,o,c,&info,lara,touch);
        else{
            int can=tr2_enemy_targetable(l,a,target,&info);
            switch(state){
            case 1:
                c->maximum_turn=0;
                if(c->mood==2)a->control.goal=3;
                else if(can && info.distance<=0x400000){int r=enemy_random(&w->random_state);a->control.goal=r<8192?5:r<16384?7:11;}
                else a->control.goal=can?2:c->mood?3:info.ahead?1:2;
                break;
            case 2:
                c->maximum_turn=728;
                if(c->mood==2)a->control.goal=3;
                else if(can)a->control.goal=info.distance>0x400000 && info.zone==info.enemy_zone?12:1;
                else a->control.goal=c->mood?3:info.ahead?2:1;
                break;
            case 3:c->maximum_turn=1092;if(c->mood!=2){if(can)a->control.goal=1;else if(!c->mood)a->control.goal=2;}break;
            case 5:case 7:case 8:case 9:case 10:case 14:
                enemy_shoot(l,w,o,target,&info,can,angle,0);
                if(state==10 || state==14){if(!can || info.distance<0x400000)a->control.goal=2;}
                else if(!can)a->control.goal=1;
                break;
            }
        }
        if(o->id==45 && alive && lara->control.health<=0){creature_kill(l,w,a,lara,45,36,14,2);return;}
    }
    int x=a->x,y=a->y,z=a->z,room=a->room;
    a->move_angle=a->yaw;tr2_creature_move(l,o,&c->lot,box,turn,o->id==46?341:o->id==45?128:102,c->target[1]);
    if(o->id==45 && state<10){
        Tr2Height h;
        if(tr2_query(l,a->x,a->y,a->z,a->room,&h,NULL)){
            int v=tr2_creature_vault_adjust(a,x,y,z,room,h.floor,300);
            if(v){tr2_set_animation(l,a,base+(v==-4?35:36-v),0);a->control.current=v==-4?13:8+v;tr2_world_room(w,index,a->room);}
        }
    }
    if(c->required==a->control.current)c->required=0;
    if(o->status==2)tr2_enemy_finish(w,index);
    a->control.flags&=(uint16_t)~16u;
}
/* Shared Bandit2Control (437960), including the two alternating shot latches. */
static void bandit_tick(const Tr2Level *l,Tr2World *w,int index,Tr2Actor *lara){
    Tr2Object *o=w->objects+index;
    Tr2Actor *a=&o->pose;
    Tr2Creature *c=w->creatures+index;
    Tr2EnemyInfo info;
    int box=box_at(l,a),enemy_box,turn=0,head=0,neck=0,tilt=0;
    int state=a->control.current;
    if(a->control.health<=0){
        const uint8_t *m=tr2_model(l,49);
        if(state!=11 && m){tr2_set_animation(l,a,tr2_u16(m+16)+9,0);a->control.current=11;}
    }else{
        if(!tr2_enemy_info(l,&c->lot,a,lara,0,&info,&box,&enemy_box))return;
        tr2_enemy_target(l,c,a,lara,box,enemy_box,1,info.distance,&w->random_state);
        turn=tr2_creature_turn(a,c->target[0],c->target[2],c->maximum_turn);
        int can=tr2_enemy_targetable(l,a,lara,&info),angle=info.ahead?info.angle:0;
        switch(state){
        case 2:
            neck=angle;c->maximum_turn=0;
            if(c->mood==2)a->control.goal=4;
            else if(can){
                if(info.distance>0x400000 && enemy_random(&w->random_state)<0x4000)a->control.goal=3;
                else{int r=enemy_random(&w->random_state);a->control.goal=r<0x2000?7:r<0x5000?8:12;}
            }else if(c->mood)a->control.goal=4;
            else if(!info.ahead || enemy_random(&w->random_state)<256)a->control.goal=3;
            break;
        case 3:
            neck=angle;c->maximum_turn=728;
            if(c->mood==2)a->control.goal=4;
            else if(can)a->control.goal=(info.distance<0x400000 || info.zone==info.enemy_zone || enemy_random(&w->random_state)<1024)?2:1;
            else if(c->mood)a->control.goal=4;
            else if(info.ahead && enemy_random(&w->random_state)<1024)a->control.goal=2;
            break;
        case 4:
            neck=angle;tilt=turn/2;c->maximum_turn=1092;
            if(c->mood!=2 && (!c->mood || can))a->control.goal=2;
            break;
        case 1:case 5:case 6:case 12:
            head=angle;c->attack=0;if(state==12)a->control.goal=can?13:2;
            break;
        case 7:case 8:case 13:
            head=angle;
            if(!c->attack){
                enemy_shoot(l,w,o,lara,&info,can,head,0);
                if(!can || enemy_random(&w->random_state)<8192)a->control.goal=2;
                c->attack=1;
            }
            break;
        case 9:case 10:{
            int latch=state==9?1:2;head=angle;
            if(c->attack!=latch){enemy_shoot(l,w,o,lara,&info,can,head,0);if(!can)a->control.goal=3;c->attack=latch;}
            if(info.distance<0x400000)a->control.goal=3;
            break;
        }}
    }
    c->neck=tr2_creature_head(c->neck,neck);c->head=tr2_creature_head(c->head,head);
    int delta=tr2_word(tilt*4-a->control.roll);
    a->control.roll+=delta>546?546:delta< -546?-546:delta;
    a->move_angle=a->yaw;
    tr2_creature_move(l,o,&c->lot,box,turn,102,c->target[1]);
    if(o->status==2)tr2_enemy_finish(w,index);
}

static void rider_tick(const Tr2Level *l,Tr2World *w,int index,Tr2Actor *lara){
    Tr2Object *rider=w->objects+index;int child=rider->linked[0];
    if((uint32_t)child>=w->count)return;
    Tr2Object *sled=w->objects+child;
    if(sled->id!=51 || !tr2_enemy_enable(l,w,child))return;
    sled->active=sled->status=1;
    Tr2Actor *s=&sled->pose,*r=&rider->pose;
    Tr2Creature *c=w->creatures+child;
    const uint8_t *rm=tr2_model(l,52),*sm=tr2_model(l,51);
    int turn=0,box=box_at(l,s),state=s->control.current;
    if(s->control.health<=0){
        if(r->control.current!=7){
            r->x=s->x;r->y=s->y;r->z=s->z;r->yaw=s->yaw;r->room=s->room;
            tr2_world_room(w,index,r->room);tr2_set_animation(l,r,tr2_u16(rm+16)+10,0);
            r->control.current=7;if(w->combat->target==child)w->combat->target=-1;
        }else tr2_object_animate(l,rider);
        s->control.goal=state==1 || state==2?1:2;
    }else{
        int enemy_box;Tr2EnemyInfo info;
        if(!tr2_enemy_info(l,&c->lot,s,lara,0,&info,&box,&enemy_box))return;
        tr2_enemy_target(l,c,s,lara,box,enemy_box,1,info.distance,&w->random_state);
        turn=tr2_creature_turn(s,c->target[0],c->target[2],546);
        int near=abs(info.angle)<2730 && info.distance<0x1000000;
        if(state==1){if(c->mood && !near)s->control.goal=2;}
        else if(state==2){if(!c->mood || near)s->control.goal=1;else if(turn< -364)s->control.goal=3;else if(turn>364)s->control.goal=4;}
        else if(state>=3 && state<=6)s->control.goal=turn< -364?5:2;
        if(!c->attack && abs(info.angle)<2730 && lara->control.health>0){
            int can=tr2_enemy_targetable(l,s,lara,&info);
            for(int j=0;j<2;j++)enemy_shoot(l,w,sled,lara,&info,can,0,j);
            if(can)c->attack=5;
        }
        if(c->attack){if(s->event_count<16){int n=s->event_count++;s->event_kind[n]=5;s->event_value[n]=43;}c->attack--;}
    }
    if(s->event_count<16){int n=s->event_count++;s->event_kind[n]=5;s->event_value[n]=state==1?153:155;}
    sled->effect_counter=state==1?0:sled->effect_counter==1?2:1;
    if(state!=1)tr2_snow_spray(l,w,s);
    s->move_angle=s->yaw;tr2_creature_move(l,sled,&c->lot,box,turn,341,c->target[1]);
    if(r->control.current!=7){
        r->x=s->x;r->y=s->y;r->z=s->z;r->yaw=s->yaw;r->room=s->room;tr2_world_room(w,index,r->room);
        int anim=tr2_u16(rm+16)+s->control.animation-tr2_u16(sm+16);
        if((uint32_t)anim<l->animations.count)tr2_set_animation(l,r,anim,s->control.frame-tr2_i16(l->animations.data+s->control.animation*32+16));
    }else if(rider->status==2 && !s->speed && !s->fall_speed){
        tr2_enemy_finish(w,index);tr2_enemy_disable(w,child);
        sled->id=13;sled->active=0;sled->status=2;
        sled->snow=(Tr2Snowmobile){0};sled->snow.momentum=s->yaw;sled->snow.drive.armed=1;
    }
}

#include "tr2_xian_enemies.h"

void tr2_enemy_tick(const Tr2Level *l,Tr2World *w,int index,Tr2Actor *lara){
    if((uint32_t)index>=w->count)return;Tr2Object *o=w->objects+index;Tr2Actor *a=&o->pose;
    if(o->id==51)return;
    if(o->id==52){if(o->active)rider_tick(l,w,index,lara);return;}
    if(!o->active || (o->status!=1 && o->status!=3) || ((o->id<15 || o->id>18) && o->id!=20 && o->id!=21 && o->id!=25 && o->id!=28 && o->id!=29 && o->id!=30 && o->id!=31 && o->id!=34 && o->id!=32 && o->id!=33 && o->id!=36 && o->id!=38 && !tr2_xian_enemy(o->id) && !tr2_tibetan_enemy(o->id) && o->id!=47 && o->id!=49 && o->id!=50 && o->id!=51 && o->id!=39 && o->id!=214))return;
    if(!tr2_enemy_enable(l,w,index)){o->status=3;return;}o->status=1;
    if(o->id==25 || o->id==28){fish_tick(l,w,index,lara);return;}
    if(tr2_xian_enemy(o->id)){xian_enemy_tick(l,w,index,lara);return;}
    if(tr2_tibetan_enemy(o->id)){tibetan_enemy_tick(l,w,index,lara);return;}
    if(o->id==49 || o->id==50){bandit_tick(l,w,index,o->id==49?monastery_target(w,o,lara):lara);return;}
    if(o->id==29){diver_tick(l,w,index,lara);return;}
    Tr2Creature *c=w->creatures+index;Tr2EnemyInfo info;int box,enemy_box;
    if(!tr2_enemy_info(l,&c->lot,a,lara,(o->id>=16 && o->id<=18) || o->id==21?50:o->id==15?300:o->id==39?200:o->id==214?1800:0,&info,&box,&enemy_box))return;
    int turn=0,head=0,neck=0,tilt_target=0;int ranged=o->id>=16 && o->id<=18;
    if(a->control.health>0){
        if(info.ahead)head=info.angle;
        tr2_enemy_target(l,c,a,lara,box,enemy_box,o->id==20 || (o->id==32 || o->id==33) || o->id==39 || o->id==214,info.distance,&w->random_state);
        turn=tr2_creature_turn(a,c->target[0],c->target[2],o->id==21?1092:o->id==36?1456:(o->id==38 || o->id==47)?546:c->maximum_turn);
        uint32_t touch=tr2_object_overlap(l,a,lara,100)?tr2_touch(l,a,o->id,lara,0):0;
        Tr2EnemyDecision decision={c->mood,info.distance,info.ahead,0,touch,c->required,c->attack,c->maximum_turn};
        if(o->id==214){decision.attack=(c->mood!=2 && !info.ahead && info.enemy_facing> -16384 && info.enemy_facing<16384) || (info.distance>0x225510 && info.distance<0x1000000 && info.bite);}
        int state=a->control.current;
        int targetable=(ranged || o->id==20 || o->id==30 || o->id==31 || o->id==34)?tr2_enemy_targetable(l,a,lara,&info):0;
        int needs_random=o->id==214?((state==2 && (!c->mood || decision.attack) && info.ahead) || (state==3 && !(info.distance<0x1900000 && info.bite) && !decision.attack && c->mood!=2 && info.ahead)):decision_random(o,&decision);
        if(ranged)needs_random=(state==4 && c->mood!=2 && (targetable || (!c->mood && info.ahead))) || (state==1 && c->mood!=2 && (targetable || (info.distance<=4194304 && info.ahead && !c->mood))) || (state==5 && !targetable && !c->mood);
        if(o->id==30)needs_random=state==2 && c->mood!=2 && targetable && (info.distance<0x900000 || info.zone!=info.enemy_zone);
        if(o->id==31)needs_random=state==1 && c->mood!=2 && targetable && (info.distance<0x900000 || info.zone!=info.enemy_zone);
        if(needs_random){w->random_state=w->random_state*0x41c64e6du+12345;decision.random=(w->random_state>>10)&32767;}
        Tr2Height h;int floor=a->y;if(tr2_query(l,a->x,a->y,a->z,a->room,&h,NULL))floor=h.floor;
        int damage=0;
        if(ranged){
            head=(state==6 || state==7 || state==9 || state==10 || state==11) && info.ahead?info.angle:0;
            tilt_target=state==2?turn/4:0;
            if(tr2_cultist_decide(o,&decision,targetable,info.zone==info.enemy_zone))enemy_shoot(l,w,o,lara,&info,targetable,head,0);
        }else if(o->id==20){
            int shots=tr2_cultist3_decide(o,&decision,targetable,info.zone==info.enemy_zone,info.angle,lara->control.health);
            if(shots&1)enemy_shoot(l,w,o,lara,&info,targetable,state==10?0:(info.ahead?info.angle:0),1);
            if(shots&2)enemy_shoot(l,w,o,lara,&info,targetable,state==10?0:(info.ahead?info.angle:0),2);
            tilt_target=state==4?turn/4:0;
            tr2_cultist3_pose(c,state,info.angle,info.ahead);
        }else if(o->id==30){
            head=info.ahead && (state==4 || state==6 || state==8 || state==9 || state==10)?info.angle:0;
            neck=info.ahead && (state==1 || state==2 || state==3 || state==5)?info.angle:0;
            tilt_target=state==5?turn/2:0;
            if(tr2_worker1_decide(o,&decision,targetable,info.zone==info.enemy_zone)){
                enemy_shoot(l,w,o,lara,&info,targetable,head,0);
                /* The root rechecks Targetable after the shot's damage. */
                if((state==4 || state==10) && !tr2_enemy_targetable(l,a,lara,&info))a->control.goal=2;
            }
        }else if(o->id==31 || o->id==34){
            head=info.ahead && (state==5 || state==6 || state==8 || state==9 || state==10 || state==11)?info.angle:0;
            neck=info.ahead && state>=1 && state<=4?info.angle:0;
            tilt_target=state==3?turn/2:0;
            if(tr2_worker2_decide(o,&decision,targetable,info.zone==info.enemy_zone)){
                if(o->id==34){int point[3]={0,308,32};if(tr2_joint(l,a,34,9,point)){Tr2Position p={point[0],point[1],point[2],a->room,0};tr2_flame_missile(l,w,&p,lara);}}
                else enemy_shoot(l,w,o,lara,&info,targetable,head,0);
            }
        }else if((o->id==32 || o->id==33)){
            neck=(state==1 || state==2 || state==5 || state==10) && info.ahead?info.angle:0;
            head=(state==3 || state==4 || state==6 || state==7 || state==8 || state==9) && info.ahead?info.angle:0;
            tilt_target=state==10?turn/2:0;damage=tr2_stick_decide(o,&decision,info.bite);
        }else if(o->id==214){
            if(touch)lara->control.health=tr2_word(lara->control.health-(a->control.current==3?10:1));
            if(tr2_trex_decide(o,&decision,info.bite,info.enemy_facing)){
                const uint8_t *m=tr2_model(l,214),*kill=tr2_model(l,12);
                if(m && kill){tr2_set_animation(l,a,tr2_u16(m+16)+11,0);a->control.current=8;tr2_set_animation(l,lara,tr2_u16(kill+16),0);lara->control.current=0;lara->control.goal=6;lara->x=a->x;lara->y=a->y;lara->z=a->z;lara->room=a->room;lara->yaw=a->yaw;lara->pitch=a->pitch;lara->control.roll=a->control.roll;lara->speed=lara->fall_speed=0;lara->control.flags=(uint16_t)((lara->control.flags&~8u)|16);tr2_animate(l,lara);lara->control.current=lara->control.goal=6;lara->control.health=tr2_word(lara->control.health-10000);lara->gun_status=1;lara->extra_animation=1;w->combat->weapon=w->combat->requested=0;w->combat->meshes=0;}return;
            }
        }else damage=o->id==21?tr2_rat_decide(o,&decision):o->id==15?tr2_dog_decide(o,&decision):o->id==36?tr2_spider_decide(o,&decision):o->id==39?tr2_tiger_decide(o,&decision,lara->speed):tr2_crow_decide(o,&decision,floor);
        c->required=decision.required;c->attack=decision.attack;c->maximum_turn=decision.maximum_turn;
        if(damage){if((o->id==32 || o->id==33) && a->event_count<16){int n=a->event_count++;a->event_kind[n]=5;a->event_value[n]=state==9?72:71;}lara->control.health=tr2_word(lara->control.health-damage);lara->control.flags|=16;
            int point[3]={(o->id==32 || o->id==33)?247:o->id==39?19:o->id==47?15:o->id==38?2:0,(o->id==32 || o->id==33)?10:o->id==15?30:o->id==39?-13:o->id==47?46:o->id==38?10:0,(o->id==32 || o->id==33)?11:o->id==21?57:o->id==15?141:o->id==39?3:o->id==47?21:o->id==38?60:41};
            if(tr2_joint(l,a,o->id,(o->id==32 || o->id==33)?10:o->id==21?2:o->id==15?20:o->id==39?26:o->id==47?6:o->id==38?14:1,point)){Tr2Position p={point[0],point[1],point[2],a->room,0};tr2_blood(w,&p,a->speed,a->yaw);}}
    }else{
        const uint8_t *m=tr2_model(l,(uint32_t)o->id);if(!m)return;
        if(o->id==36){tr2_object_fragments(l,w,o,UINT64_MAX);o->status=3;tr2_enemy_finish(w,index);tr2_world_kill(w,index);if(a->event_count<16){int n=a->event_count++;a->event_kind[n]=5;a->event_value[n]=349;}return;}
        if(o->id==20){if(a->control.current!=11){tr2_set_animation(l,a,tr2_u16(m+16)+32,0);a->control.current=11;}tr2_cultist3_pose(c,-1,0,0);}
        if(o->id==30 && a->control.current!=7){tr2_set_animation(l,a,tr2_u16(m+16)+18,0);a->control.current=7;}
        if((o->id==31 || o->id==34) && a->control.current!=7){tr2_set_animation(l,a,tr2_u16(m+16)+19,0);a->control.current=7;}
        if((o->id==32 || o->id==33) && a->control.current!=11){tr2_set_animation(l,a,tr2_u16(tr2_model(l,32)+16)+26,0);a->control.current=11;}
        if(ranged && a->control.current!=12){const uint8_t *base=tr2_model(l,16);if(base){tr2_set_animation(l,a,tr2_u16(base+16)+20+enemy_random(&w->random_state)/16384,0);a->control.current=12;}}
        if(o->id==21 && a->control.current!=6){tr2_set_animation(l,a,tr2_u16(m+16)+9,0);a->control.current=6;}
        if(o->id==15 && a->control.current!=10){tr2_set_animation(l,a,tr2_u16(m+16)+13,0);a->control.current=10;}
        if(o->id==39 && a->control.current!=9){tr2_set_animation(l,a,tr2_u16(m+16)+11,0);a->control.current=9;}
        if(o->id==214)a->control.goal=a->control.current==1?5:1;
        if(o->id==38 || o->id==47){
            turn=index; /* 40c9b9: a dead bird passes its item number as the turn. */
            Tr2Height h;int floor=a->y;if(tr2_query(l,a->x,a->y,a->z,a->room,&h,NULL))floor=h.floor;
            if(a->control.current==5){a->y=floor;a->pitch=0;}
            else if(a->control.current==4){if(a->y>floor){a->control.flags&=(uint16_t)~8u;a->y=floor;a->fall_speed=0;a->control.goal=5;}a->pitch=0;}
            else{tr2_set_animation(l,a,tr2_u16(m+16)+(o->id==47?8:1),0);a->control.current=4;a->control.flags|=8;a->speed=0;a->pitch=0;}
        }
    }
    if(o->id==30 || o->id==31 || o->id==34 || (o->id==32 || o->id==33))c->neck=(int16_t)tr2_creature_head(c->neck,neck);
    if(ranged || o->id==30 || o->id==31 || o->id==34 || (o->id==32 || o->id==33)){c->head=(int16_t)tr2_creature_head(c->head,head);int tilt=tr2_word(tilt_target*4-a->control.roll);if(tilt>546)tilt=546;else if(tilt< -546)tilt=-546;a->control.roll=tr2_word(a->control.roll+tilt);}
    if(o->id==20){int tilt=tr2_word(tilt_target*4-a->control.roll);if(tilt>546)tilt=546;else if(tilt< -546)tilt=-546;a->control.roll=tr2_word(a->control.roll+tilt);}
    if(o->id==21)c->head=(int16_t)tr2_creature_head(c->head,head);
    if(o->id==15 || o->id==39 || o->id==214){c->head=(int16_t)tr2_creature_head(c->head,o->id==214?tr2_asr(head,1):head);if(o->id==214)c->neck=c->head;else{int tilt=tr2_word((o->id==15 && a->control.health>0 && a->control.current==2?turn*4:0)-a->control.roll);if(tilt>546)tilt=546;else if(tilt< -546)tilt=-546;a->control.roll=tr2_word(a->control.roll+tilt);}}
    int x=a->x,y=a->y,z=a->z,room=a->room;
    int check_vault=(o->id==32 || o->id==33) && a->control.current<12;
    a->move_angle=a->yaw;
    tr2_creature_move(l,o,&c->lot,box,turn,o->id==214?341:o->id==15 || o->id==39?341:(o->id==38 || o->id==47)?204:102,c->target[1]);
    if(check_vault){Tr2Height height;int floor=a->y;if(tr2_query(l,a->x,a->y,a->z,a->room,&height,NULL))floor=height.floor;
        int vault=tr2_creature_vault_adjust(a,x,y,z,room,floor,260);
        if(vault){const uint8_t *m=tr2_model(l,32);int offset=vault==-4?30:vault==2?28:vault==3?29:27;int state=vault==-4?15:vault==2?13:vault==3?14:12;
            tr2_set_animation(l,a,tr2_u16(m+16)+offset,0);a->control.current=(int16_t)state;tr2_world_room(w,index,a->room);}}
    if(o->id==36 && a->y<=y-384){const uint8_t *m=tr2_model(l,36);a->x=x;a->y=y;a->z=z;a->room=(int16_t)room;tr2_set_animation(l,a,tr2_u16(m+16)+2,0);a->control.current=5;tr2_creature_move(l,o,&c->lot,box,turn,102,c->target[1]);}
    if(o->status==2)tr2_enemy_finish(w,index);
    a->control.flags&=(uint16_t)~16u;
}







