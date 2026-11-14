#include "tr2_world.h"
#include <math.h>
#include <stdlib.h>
#include "tr2_enemy.h"

static void sound(Tr2Actor *a,int id){if(a->event_count<16){int n=a->event_count++;a->event_kind[n]=5;a->event_value[n]=id;}}
static void damage(Tr2Actor *a,int amount){a->control.health=tr2_word(a->control.health-amount);a->control.flags|=16;}
static int control_random(Tr2World *w){w->random_state=w->random_state*0x41c64e6du+12345;return (int)((w->random_state>>10)&32767);}
static void contact_blood(Tr2World *w,const Tr2Actor *lara){
        int x=lara->x+(control_random(w)-16384)/256;
        int z=lara->z+(control_random(w)-16384)/256;
        int y=lara->y-control_random(w)/44;
        int yaw=tr2_word(lara->yaw+(control_random(w)-16384)/8);
        Tr2Position p={x,y,z,lara->room,0};tr2_blood(w,&p,lara->speed,yaw);
}
/* PendulumControl 0x4419f0. Only mesh bit7 damages Lara while swinging. */
void tr2_pendulum_tick(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Actor *lara,uint32_t touch){
    Tr2Actor *a=&o->pose;
    a->control.goal=tr2_trigger_active(o->flags,&o->timer) && a->control.current==1?2:1;
    if((touch&128) && a->control.current==2){
        damage(lara,20);
        contact_blood(w,lara);
    }
    tr2_object_animate(l,o);
}
void tr2_wall_trap_tick(const Tr2Level *l,Tr2Object *o,Tr2Actor *lara,uint32_t touch){
    Tr2Actor *a=&o->pose;Tr2Height h;Tr2Sector sector;a->move_angle=a->yaw;
    if(o->id==85){
        if(tr2_trigger_active(o->flags,&o->timer) && o->status!=2){
            int x=a->x+tr2_asr(tr2_sin(a->yaw),10),z=a->z+tr2_asr(tr2_cos(a->yaw),10);
            if(!tr2_query(l,x,a->y,z,a->room,&h,&sector) || h.floor!=a->y)o->status=2;
            else{a->x=x;a->z=z;a->room=(int16_t)sector.room;}sound(a,204);
        }
        if(touch){damage(lara,20);sound(a,205);}return;
    }
    if(o->id==81){
        a->control.goal=tr2_trigger_active(o->flags,&o->timer) && a->control.current==1?2:1;
        if((touch&2) && a->control.current==2)damage(lara,100);
        tr2_object_animate(l,o);return;
    }
    if(o->id==80){
        int moving=a->control.current==2;
        if(moving){
            if(a->control.goal!=1){int x=a->x+tr2_asr(1536*tr2_sin(a->yaw),14),z=a->z+tr2_asr(1536*tr2_cos(a->yaw),14);
                if(!tr2_query(l,x,a->y,z,a->room,&h,NULL) || h.floor==TR2_NO_HEIGHT)a->control.goal=1;}
            if(touch)damage(lara,100);
        }else if(tr2_trigger_active(o->flags,&o->timer))a->control.goal=2;
        tr2_object_animate(l,o);
        if(moving)sound(a,231);
        if(tr2_query(l,a->x,a->y,a->z,a->room,&h,&sector)){a->y=h.floor;a->room=(int16_t)sector.room;}
        if(moving && a->control.current==1)a->yaw=tr2_word(a->yaw+32768);
    }
}
void tr2_spikes_contact(const Tr2Level *l,const Tr2Object *o,Tr2Actor *a){
    if(a->control.health<0)return;
    if(a->control.flags&8){if(a->fall_speed>6)a->control.health=-1;}
    else if(a->speed<30)return;
    a->control.health=tr2_word(a->control.health-15);
    if(a->control.health<=0){tr2_set_animation(l,a,149,0);a->control.current=a->control.goal=8;a->control.flags&=(uint16_t)~8u;a->y=o->pose.y;}
}
void tr2_boulder_tick(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Actor *lara){
    Tr2Actor *a=&o->pose;Tr2Height h;Tr2Sector sector;
    if(o->status==1){
        a->move_angle=a->yaw;
        if(a->control.goal==2){tr2_object_animate(l,o);return;}
        if(a->y<o->floor){if(!(a->control.flags&8)){a->control.flags|=8;a->fall_speed=-10;}}
        else if(a->control.current==0)a->control.goal=1;
        int oldx=a->x,oldz=a->z;tr2_object_animate(l,o);
        if(!tr2_query(l,a->x,a->y,a->z,a->room,&h,&sector))return;a->room=(int16_t)sector.room;o->floor=h.floor;
        for(uint32_t i=0;i<w->count;i++)if(o==w->objects+i){tr2_world_room(w,(int)i,sector.room);break;}
        tr2_world_trigger(l,w,lara,h.trigger,1);
        if(a->y>=h.floor-256){
            a->control.flags&=(uint16_t)~8u;a->y=h.floor;a->fall_speed=0;sound(a,o->id==83?222:o->id==101?227:147);
            int64_t dx=(int64_t)w->ai_camera[0]-a->x,dz=(int64_t)w->ai_camera[2]-a->z;
            int distance=(int)sqrt((double)(uint32_t)(dx*dx+dz*dz));
            if(distance<10240)w->camera_bounce=((distance-10240)*40)/10240;
        }
        int radius=o->id==60?384:1024;
        int x=a->x+tr2_asr(radius*tr2_sin(a->yaw),14),z=a->z+tr2_asr(radius*tr2_cos(a->yaw),14);Tr2Height front;
        if(tr2_query(l,x,a->y,z,a->room,&front,NULL) && front.floor<a->y){
            if(o->id==83 || o->id==101){sound(a,o->id==101?228:223);a->control.goal=2;}else o->status=2;
            a->y=h.floor;a->x=oldx;a->z=oldz;a->fall_speed=a->speed=0;
        }
    }else if(o->status==2 && !tr2_trigger_active(o->flags,&o->timer)){
        size_t index=(size_t)(o-w->objects);if(index>=l->items.count)return;const uint8_t *p=l->items.data+index*24;
        a->x=tr2_i32(p+4);a->y=tr2_i32(p+8);a->z=tr2_i32(p+12);a->room=tr2_i16(p+2);
        const uint8_t *m=tr2_model(l,(uint32_t)o->id);if(m){tr2_set_animation(l,a,tr2_u16(m+16),0);a->control.current=a->control.goal=tr2_i16(l->animations.data+a->control.animation*32+6);}o->status=o->active=0;
    }
}
/* RollingBallCollision 0x441f70, including inactive solid-object contact. */
void tr2_boulder_contact(const Tr2Level *l,Tr2World *w,const Tr2Object *o,Tr2Actor *a,Tr2Contact *contact){
    if(o->status==3 || !tr2_object_overlap(l,&o->pose,a,contact->radius) || !tr2_touch(l,&o->pose,o->id,a,0))return;
    if(o->status!=1){if(contact->flags&8)tr2_object_push(l,&o->pose,a,contact);return;}
    if(a->control.flags&8){
        if(contact->flags&8)tr2_object_push_reaction(l,&o->pose,a,contact,contact->flags&16);
        a->control.health=tr2_word(a->control.health-100);
        int dx=a->x-o->pose.x,dy=a->y-o->pose.y+162,dz=a->z-o->pose.z;
        int distance=tr2_word((int)sqrt((double)(uint32_t)((int64_t)dx*dx+(int64_t)dy*dy+(int64_t)dz*dz)));
        if(distance<512)distance=512;
        Tr2Position p={o->pose.x+(tr2_long((int64_t)dx*1024)/2)/distance,
            o->pose.y-512+(tr2_long((int64_t)dy*1024)/2)/distance,
            o->pose.z+(tr2_long((int64_t)dz*1024)/2)/distance,o->pose.room,0};
        tr2_blood(w,&p,o->pose.speed,o->pose.yaw);
    }else{
        a->control.flags|=16;if(a->control.health<=0)return;
        a->control.health=-1;a->yaw=o->pose.yaw;a->pitch=a->control.roll=0;
        tr2_set_animation(l,a,139,0);a->control.current=a->control.goal=46;
        for(int i=0;i<15;i++){
            int x=a->x+(control_random(w)-16384)/256;
            int z=a->z+(control_random(w)-16384)/256;
            int y=a->y-control_random(w)/64;
            int yaw=tr2_word(o->pose.yaw+(control_random(w)-16384)/8);
            Tr2Position p={x,y,z,o->pose.room,0};tr2_blood(w,&p,tr2_word(o->pose.speed*2),yaw);
        }
    }
}

/* DetonatorControl 0x435e20: frame offsets are measured after AnimateItem. */
void tr2_detonator_tick(const Tr2Level *l,Tr2World *w,Tr2Object *o){
    tr2_object_animate(l,o);Tr2Actor *a=&o->pose;
    int frame=a->control.frame-tr2_i16(l->animations.data+a->control.animation*32+16);
    if(frame>75 && frame<100)tr2_dynamic_light(w,a->x,a->y,a->z,13,11);
    if(frame==80){w->camera_bounce=-150;if(a->event_count<16){int n=a->event_count++;a->event_kind[n]=7;a->event_value[n]=105;}}
    if(o->status==2)o->active=0;
}

/* FallingCeilingControl 0x442810, shared by the hanging falling trap79. */
void tr2_falling_trap_tick(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Actor *lara,uint32_t touch){
    Tr2Actor *a=&o->pose;Tr2Height h;Tr2Sector sector;
    if(a->control.current==0){a->control.flags|=8;a->control.goal=1;}
    else if(a->control.current==1 && touch)damage(lara,300);
    tr2_object_animate(l,o);
    if(o->status==2){o->active=0;return;}
    if(!tr2_query(l,a->x,a->y,a->z,a->room,&h,&sector))return;
    a->room=(int16_t)sector.room;for(uint32_t i=0;i<w->count;i++)if(o==w->objects+i){tr2_world_room(w,(int)i,sector.room);break;}
    if(a->control.current==1 && a->y>=h.floor){a->control.flags&=(uint16_t)~8u;a->control.goal=2;a->y=h.floor;a->fall_speed=0;}
}
/* IcicleControl 0x4417c0. Damage is checked before the fall advances. */
void tr2_icicle_tick(const Tr2Level *l,Tr2World *w,int index,Tr2Actor *lara,uint32_t touch){
    Tr2Object *o=w->objects+index;
    Tr2Actor *a=&o->pose;
    switch(a->control.current){
    case 1:
        a->control.goal=2;
        break;
    case 2:
        if(!(a->control.flags&8)){
            a->control.flags|=8;
            a->fall_speed=50;
        }
        if(touch)damage(lara,200);
        break;
    case 3:
        a->control.flags&=(uint16_t)~8u;
        break;
    }
    tr2_object_animate(l,o);
    if(o->status==2){o->active=0;return;}
    Tr2Height h;Tr2Sector sector;
    if(!tr2_query(l,a->x,a->y,a->z,a->room,&h,&sector))return;
    tr2_world_room(w,index,sector.room);
    a->room=(int16_t)sector.room;
    o->floor=h.floor;
    if(a->control.current==2 && a->y>=h.floor){
        a->control.flags&=(uint16_t)~8u;
        a->control.goal=3;
        a->y=h.floor;
        a->fall_speed=0;
        o->mesh_mask=0x2b;
    }
}
/* SwingingBladeControl 0x4424a0, used by objects58 and96. */
void tr2_swinging_trap_tick(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Actor *lara,uint32_t touch){
    if(touch){damage(lara,50);contact_blood(w,lara);}
    Tr2Height h;Tr2Sector sector;
    if(tr2_query(l,o->pose.x,o->pose.y,o->pose.z,o->pose.room,&h,&sector))o->pose.room=(int16_t)sector.room;
    tr2_object_animate(l,o);
}

/* Object63 control 0x435d40, including its animated joint-attached light. */
void tr2_object63_tick(const Tr2Level *l,Tr2World *w,Tr2Object *o){
    Tr2Actor *a=&o->pose;a->control.goal=tr2_trigger_active(o->flags,&o->timer)?1:0;
    tr2_object_animate(l,o);
    Tr2Sector sector;
    if(tr2_find_sector(l,a->x,a->y,a->z,a->room,&sector)){
        a->room=(int16_t)sector.room;
        for(uint32_t i=0;i<w->count;i++)if(o==w->objects+i){tr2_world_room(w,(int)i,sector.room);break;}
    }
    int point[3]={3000,720,0};
    if(tr2_joint(l,a,o->id,0,point))tr2_dynamic_light(w,point[0],point[1],point[2],14,11);
    if(o->status==2){o->active=0;o->flags|=256;}
}

/* LiftControl 0x4357f0: 90 ticks at either stop, 16 units per moving tick. */
void tr2_lift_tick(const Tr2Level *l,Tr2World *w,Tr2Object *o){
    Tr2Actor *a=&o->pose;int down=tr2_trigger_active(o->flags,&o->timer);
    if(down?a->y<o->lift_origin+5616:a->y>o->lift_origin+16){
        if(o->lift_wait<90){a->control.goal=1;o->lift_wait++;}
        else{a->y+=down?16:-16;a->control.goal=0;}
    }else{a->control.goal=1;o->lift_wait=0;}
    tr2_object_animate(l,o);
    Tr2Sector sector;
    if(tr2_find_sector(l,a->x,a->y,a->z,a->room,&sector)){
        a->room=(int16_t)sector.room;
        for(uint32_t i=0;i<w->count;i++)if(o==w->objects+i){tr2_world_room(w,(int)i,sector.room);break;}
    }
}

static int draw_random(Tr2World *w){uint32_t *r=&w->hazards->random_draw;*r=*r*0x41c64e6du+12345;return (int)((*r>>10)&32767);}
/* Spinning blade controller 0x441420; shared suppression flag 0x4d7e70. */
void tr2_blade78_tick(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Actor *a,uint32_t touch){
    if(touch && !w->blade_suppressed){
        damage(a,50);
        for(int i=0;i<3;i++){
            int z=a->z+256-(draw_random(w)*512)/32768;
            int y=a->y-512+256-(draw_random(w)*512)/32768;
            int x=a->x+256-(draw_random(w)*512)/32768;
            Tr2Position p={x,y,z,a->room,0};tr2_blood(w,&p,a->speed,a->yaw);
        }
    }else w->blade_suppressed=0;
    tr2_object_animate(l,o);
}
/* CreatureCollision 0x413840: blade78 uses unexpanded bounds and hit reaction. */
void tr2_blade_contact(const Tr2Level *l,Tr2World *w,const Tr2Object *o,Tr2Actor *a,Tr2Contact *contact){
    (void)w;if(!(contact->flags&8) || a->control.water_status==1)return;
    if(tr2_object_overlap(l,&o->pose,a,contact->radius) && tr2_touch(l,&o->pose,o->id,a,0))
        tr2_object_push_bounds(l,&o->pose,a,contact,contact->flags&16,0);
}
/* TrapCollision 0x4139a0: active fan supplies touch bits, stopped fan is solid. */
void tr2_propeller_contact(const Tr2Level *l,Tr2World *w,const Tr2Object *o,Tr2Actor *a,Tr2Contact *contact){
    (void)w;if(((o->id==87 || o->id==64 || o->id==94) && (o->status==1 || o->status==3)) || !(contact->flags&8))return;
    if(tr2_object_overlap(l,&o->pose,a,contact->radius) && tr2_touch(l,&o->pose,o->id,a,0))tr2_object_push(l,&o->pose,a,contact);
}
/* PropellerControl 0x4414b0, Opera objects76/95; DoBloodSplat3 0x41c610. */
void tr2_propeller_tick(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Actor *lara,uint32_t touch){
    Tr2Actor *a=&o->pose;int cue=-1;
    if(tr2_trigger_active(o->flags,&o->timer) && !(o->flags&256)){
        a->control.goal=0;
        if(touch&6){
            damage(lara,200);int speed=draw_random(w)>>10,yaw=tr2_word(a->yaw+16384);
            for(int i=0;i<3;i++){
                int z=lara->z+256-(draw_random(w)*512)/32768;
                int y=lara->y-512+256-(draw_random(w)*512)/32768;
                int x=lara->x+256-(draw_random(w)*512)/32768;
                Tr2Position p={x,y,z,lara->room,0};tr2_blood(w,&p,speed,yaw);
            }
            if(o->id==77)cue=207;
        }else cue=o->id==77?206:o->id==76?213:o->id==94?215:217;
    }else if(a->control.goal!=1){if(o->id==76)cue=213;else if(o->id==94)cue=216;a->control.goal=1;}
    tr2_object_animate(l,o);
    if(cue>=0){int count=a->event_count<16?a->event_count:15;for(int i=count;i>0;i--){a->event_kind[i]=a->event_kind[i-1];a->event_value[i]=a->event_value[i-1];}a->event_kind[0]=5;a->event_value[0]=cue;a->event_count=count+1;}
    if(o->status==2){o->active=0;if(o->id!=77)a->control.flags&=(uint16_t)~32u;}
}

#include "tr2_final_objects.h"
