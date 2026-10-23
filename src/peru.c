#include <math.h>
/* DOS room swaps 0x18b3c, moving pillars 0x2f288 and gears 0x30020. */
#include "peru.h"
#include "city.h"
#include "enemies.h"
#include "playtest.h"
#include "fixed.h"
#include "hazards.h"
#include "object_contact.h"
#include "pistols.h"
#include "progression.h"
#include "dos_camera.h"
#include <stdlib.h>
#include <string.h>
int tomb_peru_object(int id){return id==24 || id==37 || id==38 || id==52 || id==53 || (id>=74 && id<=76) || id==143;}
int tomb_flip_map(TombObjects *w,int floors){
    TombLevel *l=w->level;const TombVisual *v=w->visual;
    for(size_t i=0;i<l->room_count;i++)if(l->rooms[i].alternate>=0 && (size_t)l->rooms[i].alternate>=l->room_count)return 0;
    unsigned char *occupied=floors?calloc(w->count?w->count:1,1):NULL;
    if(floors && !occupied)return 0;
    if(floors)for(size_t i=0;i<w->count;i++){
        TombObject *o=w->items+i;
        if(((o->object>=48 && o->object<=51) || o->object==52) && w->doors[i].count && l->rooms[o->actor.room].alternate>=0){
            occupied[i]=1;if(!tomb_block_floor(w,i,o->object==52?2048:1024)){free(occupied);return 0;}
        }
    }
    for(size_t i=0;i<l->room_count;i++){
        int alt=l->rooms[i].alternate;if(alt<0)continue;
        TombRoom r=l->rooms[i];l->rooms[i]=l->rooms[alt];l->rooms[alt]=r;
        l->rooms[i].alternate=(int16_t)alt;l->rooms[alt].alternate=-1;
        TombMeshView mesh=v->rooms[i];v->rooms[i]=v->rooms[alt];v->rooms[alt]=mesh;
        TombRoomPortals portals=v->portals[i];v->portals[i]=v->portals[alt];v->portals[alt]=portals;
        if(v->lighting){TombRoomLights light=v->lighting[i];v->lighting[i]=v->lighting[alt];v->lighting[alt]=light;}
        /* Door backups follow their physical sector allocation across swaps. */
        for(size_t j=0;j<w->count;j++)for(unsigned k=0;k<w->doors[j].count;k++){
            int32_t *room=&w->doors[j].parts[k].ref.room;
            if(*room==(int)i)*room=alt;else if(*room==alt)*room=(int)i;
        }
    }
    for(size_t i=0;i<3*l->box_count;i++){int16_t z=l->zones[i];l->zones[i]=l->zones[i+3*l->box_count];l->zones[i+3*l->box_count]=z;}
    w->peru->flipped=!w->peru->flipped;
    if(floors)for(size_t i=0;i<w->count;i++){
        TombObject *o=w->items+i;
        if(occupied[i] && !tomb_block_floor(w,i,o->object==52?-2048:-1024)){free(occupied);return 0;}
    }
    free(occupied);
    return 1;
}
/* Mummy 0x3c384: stationary, turns its head, then falls when touched/shot. */
void tomb_mummy_control(TombObject *o,TombCreature *c,const TombActor *lara){
    int head=0;if(o->actor.current==1){
        head=tomb_word(tomb_object_angle(lara->z-o->actor.z,lara->x-o->actor.x)-o->actor.yaw);
        if(head< -16384)head=-16384;if(head>16384)head=16384;
        if(c->health<=0 || c->touch)o->actor.goal=2;
    }
    int delta=tomb_word(head-c->head);if(delta< -910)delta=-910;if(delta>910)delta=910;
    c->head=tomb_word(c->head+delta);if(c->head< -16384)c->head=-16384;if(c->head>16384)c->head=16384;
}
/* Larson 0x327fc; Targetable 0x324e0 supplied by the runtime LOS service. */
TombCreatureDecision tomb_larson_control(TombObject *o,TombCreature *c,const TombCreatureInfo *i,int16_t turn,uint32_t *random,const TombAnimContext *ctx,int base,int targetable){
    TombActor *a=&o->actor;TombCreatureDecision d={0,turn,0,0};int head=c->health>0 && i->ahead?i->angle:0;
    if(c->health<=0){d.turn=0;if(a->current!=5){a->animation=(int16_t)(base+15);a->frame=ctx->animations[a->animation].first_frame;a->current=5;}}
    else switch(a->current){
    case 1:if(c->required)a->goal=c->required;else if(!c->mood)a->goal=tomb_control_random(random)<96?6:2;else a->goal=c->mood==2?3:2;break;
    case 6:if(c->mood)a->goal=1;else if(tomb_control_random(random)<96){c->required=2;a->goal=1;}break;
    case 2:c->maximum_turn=546;
        if(!c->mood && tomb_control_random(random)<96){c->required=6;a->goal=1;}
        else if(c->mood==2){c->required=3;a->goal=1;}
        else if(targetable){c->required=4;a->goal=1;}
        else if(!i->ahead || i->distance>0x900000){c->required=3;a->goal=1;}break;
    case 3:c->maximum_turn=1092;d.tilt=turn/2;
        if(!c->mood && tomb_control_random(random)<96){c->required=6;a->goal=1;}
        else if(targetable){c->required=4;a->goal=1;}
        else if(i->ahead && i->distance<0x900000){c->required=2;a->goal=1;}break;
    case 4:if(c->required)a->goal=c->required;else if(targetable)a->goal=7;else a->goal=1;break;
    case 7:if(!c->required){
        if(i->distance<=0x3100000 && (int)tomb_control_random(random)<(0x3100000-i->distance)/0x620-0x2000)d.damage=50;
        d.blood=1;c->required=4;
        }if(c->mood==2)c->required=1;break;
    default:break;
    }
    int delta=tomb_word(4*d.tilt-c->roll);if(delta< -546)delta=-546;if(delta>546)delta=546;c->roll=tomb_word(c->roll+delta);
    delta=tomb_word(head-c->head);if(delta< -910)delta=-910;if(delta>910)delta=910;c->head=tomb_word(c->head+delta);if(c->head< -16384)c->head=-16384;if(c->head>16384)c->head=16384;
    return d;
}
static int floor_at(TombObjects *w,TombActor *a,TombHeights *h){
    TombSectorRef r;if(!tomb_find_sector(w->level,a->x,a->y,a->z,a->room,&r) || !tomb_item_heights(w->level,r,a->x,a->y,a->z,0,w->level->items,w->count,h))return 0;
    a->room=(int16_t)r.room;return 1;
}
int tomb_peru_hazards(TombObjects *w,TombAnimContext *ctx,TombActor *lara,int16_t *health){
    for(size_t i=0;i<w->count;i++){
        TombObject *o=w->items+i;TombActor *a=&o->actor;TombCreature *c=w->enemies->items+i;if(!o->active)continue;
        if(o->object==53){
            if(a->current==0){a->goal=1;a->flags|=8;}
            else if(a->current==1 && c->touch){*health=tomb_word(*health-300);lara->flags|=16;}
            c->touch=0;if(!tomb_object_animate(o,ctx))return 0;
            if((a->flags&6)==4){o->active=0;a->flags&=(uint8_t)~1u;}
            else if(a->current==1 && a->y>=c->floor){a->goal=2;a->y=c->floor;a->fall_speed=0;a->flags&=(uint8_t)~8u;}
        }else if(o->object==38){
            if((a->flags&6)==2){
                if(a->y<c->floor){if(!(a->flags&8)){a->fall_speed=-10;a->flags|=8;}}
                else if(a->current==0)a->goal=1;
                int x=a->x,z=a->z;if(!tomb_object_animate(o,ctx))return 0;TombHeights h;
                if(!floor_at(w,a,&h))return 0;c->floor=h.floor;
                if(!tomb_objects_trigger_heavy(w,h.trigger_index))return 0;
                if(a->y>=c->floor-256){a->y=c->floor;a->fall_speed=0;a->flags&=(uint8_t)~8u;}
                TombActor probe=*a;probe.x+=tomb_asr(512*tomb_hazard_sine(ctx->sine_quarter,a->yaw),14);probe.z+=tomb_asr(512*tomb_hazard_sine(ctx->sine_quarter,a->yaw+16384),14);
                if(!floor_at(w,&probe,&h))return 0;
                if(h.floor<a->y){a->x=x;a->z=z;a->y=c->floor;a->speed=a->fall_speed=0;a->flags=(a->flags&~6u)|4;c->touch=0;}
            }else if((a->flags&6)==4 && !tomb_trigger_active(o)){
                const TombItem *start=w->level->items+i;TombModel m;if(!tomb_visual_model(w->visual,38,&m))return 0;
                a->x=start->x;a->y=start->y;a->z=start->z;a->room=start->room;a->flags&=(uint8_t)~7u;o->active=0;
                a->animation=(int16_t)m.animation;a->frame=ctx->animations[m.animation].first_frame;a->current=a->goal=ctx->animations[m.animation].state;
            }
        }
    }
    return 1;
}
/* DOS effect table c1858: 1dfd4..1e234. Geometry is swapped by the
   trigger; most effects add only sound, while 16 performs timed flips. */
int tomb_flip_effect(int *effect,int *ticks,int sounds[2],int *bounce){
    int flip=0;sounds[0]=sounds[1]=-1;*bounce=0;
    switch(*effect){
      case 5:
          if(!*ticks){sounds[0]=99;*bounce=-250;}
          else if(*ticks==3)sounds[0]=147;
          else if(*ticks==35)sounds[0]=99;
          else if(*ticks==20 || *ticks==50 || *ticks==70)sounds[0]=70;
          if(++*ticks==105)*effect=-1;break;
    case 6:case 9:case 10:
        if(*ticks>120)*effect=-1;
        else {sounds[0]=*effect==6?81:*effect==9?118:155;if(*effect==9 && !*ticks)sounds[1]=161;}
        ++*ticks;break;
    case 7:sounds[0]=117;*effect=-1;break;
    case 8:if(*ticks==5){sounds[0]=119;*effect=-1;}++*ticks;break;
    case 11:sounds[0]=170;*bounce=-75;*effect=-1;break;
    case 15:if(!*ticks)sounds[0]=173;if(++*ticks==55){sounds[1]=33;*effect=-1;}break;
    case 16:flip=*ticks==90 || *ticks==92 || *ticks==105 || *ticks==107 || *ticks>125;if(*ticks>125)*effect=-1;++*ticks;break;
    default:break;
    }
    return flip;
}
int tomb_peru_lara(TombPlaytest *p,uint32_t input){
    TombObjects *w=p->objects;TombActor *a=&p->lara.actor;TombPeru *state=w->peru;
    if(state->scion_item>=0)++state->scion_frame;
    if(state->effect>=5){
        int effect=state->effect,ticks=state->effect_ticks,sounds[2],bounce;
        if(tomb_flip_effect(&state->effect,&state->effect_ticks,sounds,&bounce) && !tomb_flip_map(w,1))return 0;
        if(p->animation.event){
            /* Global sounds use event 9; moving flood/sand sounds retain positions. */
            for(int j=0;j<2;j++)if(sounds[j]>=0){
                TombActor sound=*a;int positioned=j==0 && (effect==6 || effect==9 || effect==10);
                if(positioned){sound.x=effect==6?a->x:w->enemies->camera[0];sound.z=effect==6?a->z:w->enemies->camera[2];sound.y=state->camera_target_y+100*(effect==6?abs(ticks-30):ticks);}
                p->animation.event(p,positioned?5:9,(int16_t)sounds[j],&sound);
            }
            if(bounce)p->animation.event(p,10,(int16_t)bounce,a);
        }
    }
    if(p->lara.health<=0)return 1;
    for(size_t i=0;i<w->count;i++){
        TombObject *o=w->items+i;TombCreature *c=w->enemies->items+i;
        int vr_spike=0;
        if(p->shooter.mode==TOMB_MODE_VR && o->object==38 && p->vr_body_height>0){
            int16_t b[6];if(tomb_visual_item_bounds(p->visual,38,&o->actor,b)){
                double cx=o->actor.x+(b[0]+b[1])*.5,cy=o->actor.y+(b[2]+b[3])*.5,cz=o->actor.z+(b[4]+b[5])*.5;
                double r=fmax(b[1]-b[0],fmax(b[3]-b[2],b[5]-b[4]))*.5+110;
                double dy=cy-fmax(p->vr_body_bottom-p->vr_body_height,fmin(p->vr_body_bottom,cy)),dx=a->x-cx,dz=a->z-cz;
                c->touch=dx*dx+dy*dy+dz*dz<r*r;
            }
        }
        if(p->shooter.mode==TOMB_MODE_VR && o->object==37){
            int16_t bounds[6];if(tomb_visual_item_bounds(p->visual,o->object,&o->actor,bounds)){
                double angle=o->actor.yaw*(6.283185307179586/65536),dx=a->x-o->actor.x,dz=a->z-o->actor.z;
                double x=cos(angle)*dx-sin(angle)*dz,z=sin(angle)*dx+cos(angle)*dz;
                vr_spike=x>=bounds[0]-90 && x<=bounds[1]+90 && z>=bounds[4]-90 && z<=bounds[5]+90 && a->y>=o->actor.y+bounds[2] && a->y<=o->actor.y+bounds[3]+256;
            }
        }
        if((!c->touch && !vr_spike) || (o->actor.flags&6)==6 || (o->object!=37 && o->object!=38))continue;
        c->touch=0;
        if(w->invincible)continue;
        if(o->object==38 && (o->actor.flags&6)==2){
            if(a->flags&8){p->lara.health=tomb_word(p->lara.health-100);tomb_effect_spawn(w,158,a,a->speed,a->yaw);}
            else {
                p->lara.health=-1;a->animation=139;a->frame=3561;a->current=a->goal=46;a->yaw=o->actor.yaw;a->room=o->actor.room;
                p->lara.pitch=p->movement.lean=0;a->flags|=16;p->animation.weapon_status=1;
                for(int j=0;j<15;j++){
                    TombActor b=*a;b.x+=((int)tomb_control_random(&w->enemies->random)-16384)/256;b.z+=((int)tomb_control_random(&w->enemies->random)-16384)/256;b.y-=tomb_control_random(&w->enemies->random)/64;
                    tomb_effect_spawn(w,158,&b,20,tomb_control_random(&w->enemies->random));
                }
            }
        }else if(o->object==37){
            int blood=tomb_control_random(&w->enemies->random)/24576;
            if(a->flags&8){if(a->fall_speed>0){p->lara.health=-1;blood=20;}}
            else if((p->shooter.mode!=TOMB_MODE_VR && a->speed<30) || (p->shooter.mode==TOMB_MODE_FPS && (p->shooter.input&128)))continue;
            p->lara.health=tomb_word(p->lara.health-15);
            for(int j=0;j<blood;j++){
                TombActor b=*a;b.x+=((int)tomb_control_random(&w->enemies->random)-16384)/256;b.z+=((int)tomb_control_random(&w->enemies->random)-16384)/256;b.y-=tomb_control_random(&w->enemies->random)/64;
                tomb_effect_spawn(w,158,&b,20,tomb_control_random(&w->enemies->random));
            }
            if(p->lara.health<=0){a->animation=149;a->frame=3887;a->current=a->goal=8;a->y=o->actor.y;a->flags&=(uint8_t)~8u;}
        }
    }
    if(p->lara.health<=0)return 1;
    for(size_t i=0;i<w->count;i++){
        TombObject *o=w->items+i;if((o->object!=143 && o->object!=146) || (o->actor.flags&6)==6)continue;
        if(state->scion_item==(int)i){
            if(o->object==143 && a->frame==p->level->animations[a->animation].first_frame+44){o->actor.flags|=6;o->active=0;tomb_inventory_add(&p->inventory,143);++p->inventory.pickups;}
            continue;
        }
        if(!(input&64) || p->animation.weapon_status || p->lara.water_status || a->current!=2 || (a->flags&8) || abs(p->lara.pitch)>1820 || p->movement.lean)continue;
        int32_t d[3]={a->x-o->actor.x,a->y-o->actor.y,a->z-o->actor.z},v[3];o->actor.yaw=a->yaw;
        tomb_rotate_vector(a->yaw,0,0,d,1,p->animation.sine_quarter,v);
        if(abs(v[0])>256 || v[1]<(o->object==146?206:540) || v[1]>(o->object==146?306:740) || v[2]<(o->object==146?-862:-350) || v[2]> -200)continue;
        TombModel m;if(!tomb_visual_model(p->visual,5,&m))return 0;
        int32_t offset[3]={0,o->object==146?280:640,o->object==146?-407:-310};tomb_rotate_vector(a->yaw,0,0,offset,0,p->animation.sine_quarter,v);
        a->x=o->actor.x+v[0];a->y=o->actor.y+v[1];a->z=o->actor.z+v[2];
        /* DOS AlignLaraPosition (16a04) copies the pickup's zero pitch/roll. */
        p->lara.pitch=p->movement.lean=0;
        a->animation=(int16_t)m.animation;a->frame=p->level->animations[m.animation].first_frame;a->current=a->goal=39;p->animation.weapon_status=1;state->scion_item=(int)i;state->scion_origin=*a;if(o->object==146)state->scion_origin.yaw=tomb_word(a->yaw-16384);state->scion_frame=1;
    }
    return 1;
}
/* In-game cinematic camera 0x14fe4; frame records follow the light table/palette. */
int tomb_scion_camera(const TombObjects *w,const int16_t *sine,TombDosCamera *camera,int *fov,int *roll){
    const TombLevel *l=w->level;size_t at=l->cinematic_offset;
    if(at>l->file_size || l->file_size-at<2)return 0;
    const unsigned char *p=l->file_data+at;unsigned count=p[0]|p[1]<<8;at+=2;
    if(!count || count>(l->file_size-at)/16)return 0;
    int frame=w->peru->scion_frame;if(frame<0)return 0;if(frame>=(int)count)frame=(int)count-1;
    p=l->file_data+at+16*(size_t)frame;int16_t v[8];for(int j=0;j<8;j++)v[j]=tomb_word(p[2*j]|p[2*j+1]<<8);
    const TombActor *origin=&w->peru->scion_origin;int si=tomb_hazard_sine(sine,origin->yaw),co=tomb_hazard_sine(sine,origin->yaw+16384);
    camera->target=(TombCameraPoint){origin->x+tomb_asr(v[0]*co+v[2]*si,14),origin->y+v[1],origin->z+tomb_asr(v[2]*co-v[0]*si,14),origin->room};
    camera->eye=(TombCameraPoint){origin->x+tomb_asr(v[3]*co+v[5]*si,14),origin->y+v[4],origin->z+tomb_asr(v[5]*co-v[3]*si,14),origin->room};
    TombSectorRef r;if(tomb_find_sector(l,camera->eye.x,camera->eye.y,camera->eye.z,camera->eye.room,&r))camera->eye.room=(int16_t)r.room;
    camera->ready=1;*fov=v[6];*roll=v[7];return 1;
}
int tomb_flip_action(TombObjects *w,int action,int id,int type,uint16_t flags){
    if(id<0 || id>=1024)return 0;uint16_t *f=w->peru->flip_flags+id;
    if(action==3){
        if(*f&0x100)return 0;
        if(type==2)*f^=flags&0x3e00;else *f|=flags&0x3e00;
        if((*f&0x3e00)==0x3e00){if(flags&0x100)*f|=0x100;return !w->peru->flipped;}
        return w->peru->flipped;
    }
    return (*f&0x3e00)==0x3e00 && (action==4?!w->peru->flipped:w->peru->flipped);
}
int tomb_peru_tick(TombObjects *w,size_t id,TombAnimContext *ctx){
    TombObject *o=w->items+id;TombActor *a=&o->actor;
    if(o->object==47 || (o->object>=74 && o->object<=76))a->goal=(int16_t)!!tomb_trigger_active(o);
    else if(o->object==52){
        int active=tomb_trigger_active(o);
        if((active && a->current==0) || (!active && a->current==1)){
            a->goal=(int16_t)active;if(!tomb_block_floor(w,id,2048))return 0;
        }
    }else return 1;
    if(!tomb_object_animate(o,ctx))return 0;
    TombSectorRef ref;if(!tomb_find_sector(w->level,a->x,a->y,a->z,a->room,&ref))return 0;a->room=(int16_t)ref.room;
    if(o->object==52 && (a->flags&6)==4){
        a->flags=(a->flags&~6u)|2;if(!tomb_block_floor(w,id,-2048))return 0;
        a->x=(a->x&~1023)+512;a->z=(a->z&~1023)+512;
    }
    return 1;
}
