/* DOS CentaurControl 3b5e8 and AtlanteanControl 3b8e4.
   AI information, target visibility and motion remain explicit services. */
#include "atlantean.h"
#include "pistols.h"
#include "fixed.h"
#include "enemies.h"
#include "hazards.h"
#include <stdlib.h>
static int clamp(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}
static int shot(uint32_t *random,int distance,int damage){
    return distance<=0x3100000 && (int)tomb_control_random(random)<(0x3100000-distance)/0x620-0x2000?damage:0;
}
/* NatlaControl 2e2ec, following its LOT/mood/turn services. 174 requests
   the stationary three-bomb volley; projectile allocation remains a service. */
void tomb_natla_flight(TombObject *o,TombCreature *c,const TombCreatureInfo *i,int targetable,uint32_t *random){
    if(c->health<=200)return;
    int shoot=targetable && i->angle> -5460 && i->angle<5460;
    if(o->actor.current==2 && (c->flags&0x8000)){if(shoot && tomb_control_random(random)<256)c->flags&=32767;}
    else if(!shoot)c->flags=tomb_word(c->flags|0x8000);
}
TombCreatureDecision tomb_natla_control(TombObject *o,TombCreature *c,const TombCreatureInfo *i,int16_t turn,int targetable){
    TombActor *a=&o->actor;TombCreatureDecision d={0,turn,0,0};int timer=c->flags&32767,head=7*c->head/8,aim=0;
    int shoot=targetable && i->angle> -5460 && i->angle<5460;
    if(c->health<=0 && c->health> -16384){a->goal=9;d.turn=0;}
    else if(c->health<=200){
        aim=i->ahead?i->angle:0;a->yaw=tomb_word(a->yaw+c->yaw_offset);c->yaw_offset=0;
        switch(a->current){
        case 1:case 4:case 6:a->goal=5;o->flags=0;timer=0;break;
        case 2:a->goal=7;timer=0;break;
        case 7:if(a->y<c->floor){a->speed=0;a->flags|=8;}else{a->goal=5;a->y=c->floor;a->flags&=(uint8_t)~8u;timer=0;}break;
        case 8:if(!shoot)a->goal=3;if(timer>=20){d.blood=172;timer=0;}break;
        case 3:d.tilt=turn;if(timer>=20){d.blood=172;timer=0;}if(shoot)a->goal=8;break;
        case 5:if(timer==480){a->goal=8;c->flags=0;timer=0;c->health=200;}else c->health=-16384;break;
        default:break;
        }
    }else{
        aim=i->ahead?i->angle:0;
        if(a->current==2){c->yaw_offset=tomb_word(c->yaw_offset+clamp(i->angle,-910,910));a->yaw=tomb_word(a->yaw+c->yaw_offset);}
        else {a->yaw=tomb_word(a->yaw+c->yaw_offset-turn);c->yaw_offset=0;}
        switch(a->current){
        case 1:timer=0;a->goal=c->flags&0x8000?2:4;break;
        case 2:if(!(c->flags&0x8000) && a->y==c->floor)a->goal=1;if(timer>=30){d.blood=173;timer=0;}break;
        case 4:a->goal=c->required?c->required:shoot?6:1;break;
        case 6:if(!c->required){d.blood=174;c->required=1;}break;
        default:break;
        }
    }
    c->roll=tomb_word(c->roll+clamp(tomb_word(4*d.tilt-c->roll),-546,546));c->neck=tomb_word(-aim);if(head)c->head=(int16_t)head;
    c->flags=tomb_word((c->flags&0x8000)+timer+1);return d;
}
/* Giant mutant 2ddfc. Its walk state deliberately modifies goal, as DOS does. */
TombCreatureDecision tomb_torso_control(TombObject *o,TombCreature *c,const TombCreatureInfo *i,int16_t angle,int16_t lara_health,uint32_t *random,const TombAnimContext *ctx,int base){
    TombActor *a=&o->actor;TombCreatureDecision d={0};int head=c->health>0 && i->ahead?i->angle:0;
    if(c->health<=0){if(a->current!=10){a->animation=(int16_t)(base+13);a->frame=ctx->animations[a->animation].first_frame;a->current=10;}}
    else{
        if(c->touch)d.damage=5;
        switch(a->current){
        case 8:a->goal=9;a->flags|=8;break;
        case 1:
            if(lara_health-d.damage<=0)break;c->flags=0;
            if(angle>8190)a->goal=3;else if(angle< -8190)a->goal=2;
            else if(i->distance>=0x672640)a->goal=7;
            else if(lara_health-d.damage<=500)a->goal=i->distance<0x4d3f64?6:7;
            else a->goal=tomb_control_random(random)<16384?4:5;break;
        case 7:
            a->goal=tomb_word(a->goal+clamp(angle,-546,546));
            if(angle>8190 || angle< -8190 || i->distance<0x672640)a->goal=1;break;
        case 3:
            if(!c->flags)c->flags=a->frame;else if(a->frame-c->flags>16 && a->frame-c->flags<23)a->yaw=tomb_word(a->yaw+2548);
            if(angle<8190)a->goal=1;break;
        case 2:
            if(!c->flags)c->flags=a->frame;else if(a->frame-c->flags>13 && a->frame-c->flags<23)a->yaw=tomb_word(a->yaw-1638);
            if(angle> -8190)a->goal=1;break;
        case 4:case 5:
            if(!c->flags && (c->touch&(a->current==4?0x3ff8000u:0x3fffff0u))){d.damage+=500;c->flags=1;}break;
        case 6:if((c->touch&0x3ff8000u) || lara_health-d.damage<=0){a->goal=11;d.damage=10000;}break;
        default:break;
        }
    }
    c->head=tomb_word(c->head+clamp(tomb_word(head-c->head),-910,910));c->head=(int16_t)clamp(c->head,-16384,16384);return d;
}
/* DOS Kid/Cowboy/BaldyControl 334d4 / 337f8 / 33b90.
   blood is a muzzle bit mask: first gun=1, second gun=2. */
TombCreatureDecision tomb_mines_control(TombObject *o,TombCreature *c,const TombCreatureInfo *i,int16_t turn,uint32_t *random,const TombAnimContext *ctx,int base,int targetable){
    TombActor *a=&o->actor;TombCreatureDecision d={0,turn,0,0};
    int head=c->health>0 && i->ahead?i->angle:0,kid=o->object==30,bald=o->object==32;
    if(c->health<=0){d.turn=0;if(a->current!=5){a->animation=(int16_t)(base+(kid?13:bald?14:7));a->frame=ctx->animations[a->animation].first_frame;a->current=5;}}
    else if(kid)switch(a->current){
    case 0:c->flags=0;a->goal=c->required?c->required:targetable?1:2;break;
    case 2:c->flags=0;if(tomb_control_random(random)<512)a->goal=3;else if(targetable)a->goal=i->distance>0x640000 && i->distance<0x1000000 && c->mood!=2?0:4;break;
    case 3:if(tomb_control_random(random)<1024)a->goal=2;break;
    case 1:case 4:
        if(!c->flags && targetable){int damage=a->current==1?50:40;d.damage=(int16_t)shot(random,i->distance,damage);d.damage+=(int16_t)shot(random,i->distance,damage);d.blood=3;c->flags=1;}
        if(c->mood==2 || i->distance<0x100000)c->required=2;break;
    default:break;
    }else switch(a->current){
    case 1:a->goal=c->required?c->required:targetable?4:c->mood?3:2;break;
    case 2:
        c->maximum_turn=546;
        if(c->mood==2 || !i->ahead){c->required=3;a->goal=1;}
        else if(targetable){c->required=4;a->goal=1;}
        else if(i->distance>(bald?0x1000000:0x900000)){c->required=3;a->goal=1;}break;
    case 3:
        c->maximum_turn=1092;d.tilt=turn/2;
        if(c->mood!=2 || i->ahead){if(targetable){c->required=4;a->goal=1;}else if(i->ahead && i->distance<(bald?0x1000000:0x900000)){c->required=2;a->goal=1;}}break;
    case 4:c->flags=0;a->goal=c->required || !targetable?1:6;break;
    case 6:
        if(!c->flags){d.damage=(int16_t)shot(random,bald?i->distance/2:i->distance,bald?150:70);d.blood=1;}
        else if(!bald && c->flags==6){if(targetable)d.damage=(int16_t)shot(random,i->distance,70);d.blood=2;}
        c->flags=bald?(c->flags?c->flags:1):tomb_word(c->flags+1);if(c->mood==2)c->required=3;break;
    default:break;
    }
    if(!kid)c->roll=tomb_word(c->roll+clamp(tomb_word(4*d.tilt-c->roll),-546,546));
    c->head=tomb_word(c->head+clamp(tomb_word(head-c->head),-910,910));c->head=(int16_t)clamp(c->head,-16384,16384);
    return d;
}
int tomb_atlantean_spawners_init(TombObjects *w){
    TombModel cinema;if(tomb_visual_model(w->visual,77,&cinema))return 1;
    for(size_t i=0;i<w->source_count;i++){
        TombObject *o=w->items+i;if(o->object!=161 && o->object!=163 && o->object!=181)continue;
        int type=23;
        if(o->object!=161){int bits=(w->level->items[i].flags&0x3e00)>>9;type=bits==1?21:bits==2?23:bits==4?34:bits==8?22:20;o->flags=0;}
        /* UB includes empty pods whose selected hatchling has no loaded model.
           Keep the shell active, but never manufacture an enemy from other meshes. */
        TombModel hatchling;if(!tomb_visual_model(w->visual,type,&hatchling)){w->enemies->items[i].required=0;continue;}
        int index=tomb_objects_spawn(w,type,&o->actor);if(index<0)return 0;
        TombObject *born=w->items+index;born->actor.flags|=6;
        w->enemies->items[i].required=(int16_t)(index+1);
        if(o->object==161){
            TombModel model;if(!tomb_visual_model(w->visual,23,&model))return 0;
            born->actor.animation=(int16_t)(model.animation+7);const TombAnim *a=w->level->animations+born->actor.animation;
            born->actor.frame=(int16_t)(a->first_frame+36);born->actor.current=born->actor.goal=a->state;
        }else born->actor.yaw=tomb_word(born->actor.yaw+((int)tomb_control_random(&w->enemies->random)-16384)/2);
    }
    return 1;
}
int tomb_atlantean_spawners_tick(TombObjects *w,TombAnimContext *ctx,const TombActor *lara){
    for(size_t i=0;i<w->source_count;i++){
        TombObject *o=w->items+i;if(!o->active || (o->object!=161 && o->object!=163 && o->object!=181))continue;
        TombCreature *c=w->enemies->items+i;int64_t dx=(int64_t)lara->x-o->actor.x,dz=(int64_t)lara->z-o->actor.z;int dy=lara->y-o->actor.y;
        int statue=o->object==161;
        int hatch=statue?abs(dy)<1024 && dx*dx+dz*dz<0xc40000:(o->flags&0x100) || o->object==181 || (llabs(dx)<4096 && llabs(dz)<4096 && abs(dy)<4096);
        if(!c->flags && hatch){
            int index=c->required-1;if(index>=0 && (size_t)index>=w->count)return 0;
            if(!tomb_explode_meshes(w,o,NULL,statue?UINT32_MAX:0xfffe00,0,ctx->sine_quarter))return 0;
            c->flags=1;o->actor.flags&=(uint8_t)~32u;
            if(statue){o->active=0;o->actor.flags|=6;if(ctx->event)ctx->event(ctx->user,5,104,&o->actor);}
            else o->actor.goal=1;
            if(index>=0 && !tomb_enemies_birth(w,(size_t)index))return 0;
        }
        if(!statue && !tomb_object_animate(o,ctx))return 0;
    }
    return 1;
}
void tomb_atlantean_flight(TombObject *o,TombCreature *c,const TombCreatureInfo *i,int targetable){
    if(o->object!=20)return;
    int shoot=targetable && (i->zone!=i->enemy_zone || i->distance>0xe10000);
    int right=shoot && i->angle>0 && i->angle<8192,left=shoot && i->angle<0 && i->angle> -8192;
    if(o->actor.current==13){if((c->flags&4) && c->mood!=2 && i->zone==i->enemy_zone)c->flags-=4;}
    else if((i->zone!=i->enemy_zone && !right && !left && (!i->ahead || !c->mood)) || c->mood==2)c->flags|=4;
}
TombCreatureDecision tomb_atlantean_control(TombObject *o,TombCreature *c,const TombCreatureInfo *i,int16_t turn,uint32_t *random,const TombAnimContext *ctx,int base,int targetable){
    TombActor *a=&o->actor;TombCreatureDecision d={0,turn,0,0};int head=c->health>0 && i->ahead?i->angle:0;
    if(o->object==23){
        if(c->health<=0){d.turn=0;if(a->current!=5){a->animation=(int16_t)(base+8);a->frame=ctx->animations[a->animation].first_frame;a->current=5;}}
        else switch(a->current){
        case 1:c->neck=0;a->goal=c->required?c->required:i->bite && i->distance<0x240000?3:targetable?4:3;break;
        case 2:if(!c->required){c->required=4;d.blood=173;}break;
        case 3:if(i->bite && i->distance<0x240000){c->required=6;a->goal=1;}else if(targetable){c->required=4;a->goal=1;}else if(tomb_control_random(random)<96){c->required=6;a->goal=1;}break;
        case 4:if(c->required)a->goal=c->required;else if(targetable)a->goal=2;else a->goal=1;break;
        case 6:if(!c->required && (c->touch&0x30199)){d.damage=200;d.blood=1;c->required=1;}break;
        default:break;
        }
    }else{
        if(c->health<=0){d.turn=0;return d;} /* Runtime performs mesh explosion/removal. */
        int shoot=targetable && o->object!=22 && (i->zone!=i->enemy_zone || i->distance>0xe10000);
        int right=shoot && i->angle>0 && i->angle<8192,left=shoot && i->angle<0 && i->angle> -8192;
        switch(a->current){
        case 1:
            c->flags&=(int16_t)~11;
            a->goal=(c->flags&4)?13:(c->touch&0x678) || (i->bite && i->distance<0x15f90)?8:i->bite && i->distance<0x57e40?4:right?9:left?10:!c->mood || (c->mood==3 && i->distance<0x1440000)?6:3;break;
        case 2:
            c->maximum_turn=364;
            if(right || left || (c->flags&4) || c->mood==1 || c->mood==2)a->goal=1;
            else if(!c->mood || (c->mood==3 && i->zone!=i->enemy_zone)){if(tomb_control_random(random)<80)a->goal=6;}
            else if(c->mood==3 && i->distance>0x1440000)a->goal=1;
            break;
        case 3:
            c->maximum_turn=1092;
            if((c->flags&4) || (c->touch&0x678) || (i->bite && i->distance<0x57e40))a->goal=1;
            else if(i->ahead && i->distance<0x640000)a->goal=7;
            else if(right || left || !c->mood || (c->mood==3 && i->distance<0x1440000))a->goal=1;
            break;
        case 4:case 7:case 8:
            if(!c->required && (c->touch&0x678)){d.damage=(int16_t)(a->current==4?150:a->current==7?100:200);d.blood=1;c->required=(int16_t)(a->current==7?3:1);}break;
        case 6:
            head=0;
            if(right || left || (c->flags&4))a->goal=1;
            else if(c->mood==3){if(i->distance>=0x1440000)a->goal=1;else if(i->zone==i->enemy_zone || tomb_control_random(random)<256)a->goal=2;}
            else if(!c->mood && tomb_control_random(random)<256)a->goal=2;
            else if(c->mood==1 || c->mood==2)a->goal=1;
            break;
        case 9:c->flags|=9;a->goal=(int16_t)(right?11:1);break;
        case 10:c->flags|=2;a->goal=(int16_t)(left?11:1);break;
        case 11:if(c->flags&1){--c->flags;d.blood=172;}else if(c->flags&2){c->flags-=2;d.blood=173;}break;
        case 12:a->goal=1;break;
        case 13:if(!(c->flags&4) && a->y==c->floor)a->goal=1;break;
        default:break;
        }
        if(!(c->flags&8))c->head=c->neck;
    }
    c->head=tomb_word(c->head+clamp(tomb_word(head-c->head),-910,910));c->head=(int16_t)clamp(c->head,-16384,16384);
    if(o->object!=23){if(!(c->flags&8)){c->neck=c->head;c->head=0;}else c->neck=0;}
    return d;
}
