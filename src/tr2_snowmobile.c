#include "tr2_world.h"
#include <stdlib.h>

/* Snowmobile input phase at 0x43dd20. The parent controller smooths pitch
   and applies steering and terrain movement separately. */
int tr2_snowmobile_input(Tr2Actor *sled,int floor,uint32_t input,Tr2SnowDrive *drive,
                         uint32_t *random_state,int *pitch,int *look){
    *look=0;
    if(sled->y<floor-256){
        if(!(input&(TR2_FORWARD|TR2_BACK)))return 0;
        *pitch=drive->engine_pitch+50;
        return 1;
    }
    *pitch=floor+sled->speed-sled->y;
    *look=!sled->speed && (input&TR2_LOOK);
    if(*look)input&=~((input&TR2_FORWARD)?TR2_FORWARD:TR2_BACK);
    int back=(input&TR2_BACK)!=0;
    if(input&(back?TR2_RIGHT:TR2_LEFT)){
        drive->turn-=455;
        if(drive->turn< -1092)drive->turn=-1092;
    }
    if(input&(back?TR2_LEFT:TR2_RIGHT)){
        drive->turn+=455;
        if(drive->turn>1092)drive->turn=1092;
    }
    if(back){
        int reversing=sled->speed<=0;
        if(sled->speed> -30)sled->speed-=5;
        return reversing;
    }
    if(input&TR2_FORWARD){
        int maximum=100;
        if(!drive->armed && (input&TR2_ACTION))maximum=150;
        else if(input&TR2_WALK)maximum=50;
        if(sled->speed<maximum)sled->speed+=5+5*sled->speed/maximum;
        else if(sled->speed>maximum+2)sled->speed-=2;
        return 1;
    }
    if(sled->speed>=0 && sled->speed<15 && (input&(TR2_LEFT|TR2_RIGHT))){
        sled->speed=15;
        return 1;
    }
    if(sled->speed<=2){
        sled->speed=0;
        return 0;
    }
    sled->speed-=2;
    *random_state=*random_state*0x41c64e6du+12345u;
    return (int)((*random_state>>10)&127)<sled->speed;
}

void tr2_snow_contact(const Tr2Level *l,Tr2World *w,int index,Tr2Actor *a,uint32_t input,Tr2Contact *c){
    Tr2Object *o=w->objects+index;
    Tr2Actor *s=&o->pose;
    if(w->vehicle>=0 || a->control.health<0 || o->flags&0x8100 ||
       !tr2_object_overlap(l,s,a,c->radius) || !tr2_touch(l,s,13,a,0))return;
    int angle=tr2_word(s->yaw-a->yaw);
    Tr2Height h;
    const uint8_t *model=tr2_model(l,10);
    if(!model || !(input&TR2_ACTION) || a->gun_status || (a->control.flags&8) ||
       abs(angle)<=8192 || abs(angle)>=24576 ||
       !tr2_query(l,s->x,s->y,s->z,s->room,&h,NULL) || h.floor< -32000){
        if(c->flags&8)tr2_object_push(l,s,a,c);
        return;
    }
    if(w->combat->weapon==8){
        tr2_flare_create(l,w,a,0);
        w->combat->flare_control=w->combat->meshes=0;
        w->combat->weapon=w->combat->requested=0;
    }
    w->vehicle=index;
    tr2_set_animation(l,a,tr2_u16(model+16)+(angle>0?1:18),0);
    a->control.current=1;
    a->gun_status=1;
    a->yaw=s->yaw;
    a->x=s->x;a->y=s->y;a->z=s->z;
    s->control.health=1;
}

static int sample(const Tr2Level *l,const Tr2Actor *a,int forward,int side,int p[3]){
    int s=tr2_sin(a->yaw),c=tr2_cos(a->yaw);
    p[0]=a->x+tr2_asr(forward*s+side*c,14);
    p[1]=a->y-tr2_asr(forward*tr2_sin(a->pitch),14)+tr2_asr(side*tr2_sin(a->control.roll),14);
    p[2]=a->z+tr2_asr(forward*c-side*s,14);
    Tr2Height h;
    return tr2_query(l,p[0],p[1],p[2],a->room,&h,NULL)?h.floor:TR2_NO_HEIGHT;
}

static int vertical(int height,int fall,int *y){
    if(height>*y){
        *y+=fall;
        if(*y>height-50){*y=height;return 0;}
        return fall+6;
    }
    int kick=(height-*y)*4;
    if(kick< -80)kick=-80;
    if(*y>height)*y=height;
    return fall+tr2_asr(kick-fall,3);
}

static void sound(Tr2Actor *a,int id){
    if(a->event_count<16){int i=a->event_count++;a->event_kind[i]=5;a->event_value[i]=id;}
}

/* The four runners each resolve a sector crossing. Their wall responses
   rotate the sled on the following tick, including while it is airborne. */
static int blood_offset(uint32_t *rng){
    *rng=*rng*0x41c64e6du+12345;
    return 256-(int)((*rng>>16)&511);
}
static void contacts(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Actor *rider){
    const Tr2Actor *s=&o->pose;
    const Tr2Room *home=l->rooms+s->room;
    for(uint32_t n=0;n<=home->portals.count;n++){
        int room=n?tr2_u16(home->portals.data+(n-1)*32):s->room;
        for(int i=w->room_head[room];(uint32_t)i<w->count;i=w->objects[i].next_room){
            Tr2Object *target=w->objects+i;
            Tr2Actor *p=&target->pose;
            if(!(p->control.flags&32))continue; /* 43d1b3 */
            if(target->status==3 || (target->id!=83 && !tr2_object_intelligent(target->id)))continue;
            if(abs(s->x-p->x)>=2048 || abs(s->y-p->y)>=2048 || abs(s->z-p->z)>=2048 ||
               !tr2_object_overlap(l,p,s,500))continue;
            if(target->id==83){
                if(p->control.current==1){rider->control.health-=100;rider->control.flags|=16;}
            }else if(target->id==51){
                int b[6];if(tr2_best_bounds(l,p,b))tr2_push_horizontal(p,&o->pose,b,500);
            }else{
                for(int k=0;k<3;k++){
                    Tr2Position blood={p->x,s->y-256,p->z,p->room,0};
                    blood.z+=blood_offset(&w->hazards->random_draw);
                    blood.y+=blood_offset(&w->hazards->random_draw);
                    blood.x+=blood_offset(&w->hazards->random_draw);
                    tr2_blood(w,&blood,s->speed,s->yaw);
                }
                p->control.health=0;
            }
        }
    }
}
static int dynamics(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Actor *rider){
    Tr2Snowmobile *b=&o->snow;
    Tr2Actor *a=&o->pose;
    const int forward[]={-550,-550,550,550},side[]={-260,260,-260,260};
    int old[4][3],origin[]={a->x,a->y,a->z};
    for(int i=0;i<4;i++){
        int h=sample(l,a,forward[i],side[i],old[i]);
        if(old[i][1]>h)old[i][1]=h;
    }
    int ground=a->y>o->floor-256;
    if(ground){
        int t=b->drive.turn;
        b->drive.turn=t>364?t-364:t< -364?t+364:0;
    }
    a->yaw=tr2_word(a->yaw+b->rotation+b->drive.turn);
    if(ground){
        int diff=tr2_word(a->yaw-b->momentum);
        if(abs(diff)>27300)b->momentum=tr2_word(a->yaw+(diff<0?27300:-27300));
        else if(abs(diff)>546)b->momentum=tr2_word(b->momentum+(diff<0?-546:546));
        else b->momentum=a->yaw;
    }
    int s=tr2_sin(b->momentum),c=tr2_cos(b->momentum);
    a->x+=tr2_asr(a->speed*s,14);a->z+=tr2_asr(a->speed*c,14);
    int slip=tr2_asr(100*tr2_sin(a->pitch),14);
    if(abs(slip)>50){a->x-=tr2_asr(slip*tr2_sin(a->yaw),14);a->z-=tr2_asr(slip*tr2_cos(a->yaw),14);}
    slip=tr2_asr(50*tr2_sin(a->control.roll),14);
    if(abs(slip)>25){a->x+=tr2_asr(slip*tr2_cos(a->yaw),14);a->z-=tr2_asr(slip*tr2_sin(a->yaw),14);}
    int moved[]={a->x,a->y,a->z},rotation=0;
    if(!(o->flags&256))contacts(l,w,o,rider);
    for(int i=0;i<4;i++){
        int p[3];
        if(sample(l,a,forward[i],side[i],p)<old[i][1]-256)rotation+=tr2_vehicle_shift(l,a,p,old[i]);
    }
    int center[]={a->x,a->y,a->z};
    Tr2Height h;
    if(!tr2_query(l,a->x,a->y,a->z,a->room,&h,NULL) || h.floor<a->y-256)
        tr2_vehicle_shift(l,a,center,origin);
    b->rotation=tr2_word(rotation);
    int hit=tr2_vehicle_hit(a,moved);
    if(hit){
        int speed=tr2_asr((a->x-origin[0])*s+(a->z-origin[2])*c,14);
        if(a->speed>110 && speed<a->speed-10){
            rider->control.health-=tr2_asr(a->speed-speed,1);rider->control.flags|=16;
        }
        if(a->speed>0 && speed<a->speed)a->speed=speed>0?speed:0;
        if(a->speed<0 && speed>a->speed)a->speed=speed<0?speed:0;
        if(a->speed< -30)a->speed=-30;
    }
    return hit;
}

static int dismount_yaw(int state,int yaw){return tr2_word(yaw+(state==7?16384:-16384));}
static int can_dismount(const Tr2Level *l,const Tr2Actor *s,int state){
    int yaw=dismount_yaw(state,s->yaw);
    int x=s->x-tr2_asr(330*tr2_sin(yaw),14),z=s->z-tr2_asr(330*tr2_cos(yaw),14);
    Tr2Height h;
    return tr2_query(l,x,s->y,z,s->room,&h,NULL) && h.type!=2 && h.floor!=TR2_NO_HEIGHT &&
           abs(h.floor-s->y)<=512 && h.ceiling<=s->y-762 && h.floor-h.ceiling>=762;
}

static void rider_state(const Tr2Level *l,Tr2Object *o,Tr2Actor *a,int hit,uint32_t input,int base){
    Tr2Actor *s=&o->pose;
    int state=a->control.current,dead=a->control.health<=0 || state==10;
    if(!dead && s->y!=o->floor && s->fall_speed>0 && state!=4){
        tr2_set_animation(l,a,base+8,0);a->control.current=a->control.goal=4;return;
    }
    if(!dead && hit && state!=4){
        if(state!=5){sound(s,hit==13?202:201);tr2_set_animation(l,a,base+hit,0);a->control.current=a->control.goal=5;}
        return;
    }
    switch(state){
    case 8:
        if(dead){a->control.goal=11;break;}
        a->control.goal=8;
        if(input&TR2_JUMP){
            int exit=0;
            if((input&TR2_RIGHT) && can_dismount(l,s,9))exit=9;
            else if((input&TR2_LEFT) && can_dismount(l,s,7))exit=7;
            if(exit){a->control.goal=exit;s->speed=0;}
        }else if(input&TR2_LEFT)a->control.goal=2;
        else if(input&TR2_RIGHT)a->control.goal=3;
        else if(input&(TR2_FORWARD|TR2_BACK))a->control.goal=0;
        break;
    case 0:
        if(!s->speed)a->control.goal=8;
        if(dead)a->control.goal=12;
        else if(input&TR2_LEFT)a->control.goal=2;
        else if(input&TR2_RIGHT)a->control.goal=3;
        break;
    case 2:if(!(input&TR2_LEFT))a->control.goal=0;break;
    case 3:if(!(input&TR2_RIGHT))a->control.goal=0;break;
    case 4:
        if(s->fall_speed<=0 || o->snow.left_fall<=0 || o->snow.right_fall<=0){sound(s,203);a->control.goal=0;}
        else if(s->fall_speed>154)a->control.goal=10;
        break;
    }
}

void tr2_snow_view_tick(const Tr2Level *l,Tr2World *w,int index,Tr2Actor *a,uint32_t input,Tr2Look *view){
    Tr2Object *o=w->objects+index;
    Tr2Actor *s=&o->pose;
    Tr2Snowmobile *b=&o->snow;
    const uint8_t *rider=tr2_model(l,10),*sled=tr2_model(l,13);
    if(!rider || !sled)return;
    int rb=tr2_u16(rider+16),sb=tr2_u16(sled+16);
    int hit=dynamics(l,w,o,a),left[3],right[3];
    int lh=sample(l,s,550,-260,left),rh=sample(l,s,550,260,right);
    Tr2Height h;Tr2Sector sector;
    if(!tr2_query(l,s->x,s->y,s->z,s->room,&h,&sector))return;
    tr2_world_trigger(l,w,a,h.trigger,0);
    int state=a->control.current,pitch=0,look=0;
    if(a->control.health<=0)input&=~15u;
    int boarding=state==1 || state==7 || state==9 || state==10;
    if(o->flags&256){hit=0;b->drive.engine_pitch=0;o->effect_counter=0;sound(s,153);}
    else if(boarding){b->drive.engine_pitch=0;hit=0;o->effect_counter=0;}
    else if(tr2_snowmobile_input(s,h.floor,input,&b->drive,&w->hazards->random_draw,&pitch,&look)){
        o->effect_counter=o->effect_counter==1?2:1;
        b->drive.engine_pitch+=tr2_asr(pitch-b->drive.engine_pitch,2);sound(s,155);
    }else{b->drive.engine_pitch=0;o->effect_counter=0;sound(s,153);}
    if(look){
        if(view){
            view->active=1;
            if(input&TR2_FORWARD){if(view->pitch>-7644)view->pitch-=364;}
            else if((input&TR2_BACK) && view->pitch<4004)view->pitch+=364;
            if(a->gun_status!=1)view->torso_pitch=view->pitch;
        }
        input&=~((input&TR2_FORWARD)?TR2_FORWARD:TR2_BACK);
    }
    o->floor=h.floor;
    b->left_fall=vertical(lh,b->left_fall,&left[1]);
    b->right_fall=vertical(rh,b->right_fall,&right[1]);
    s->fall_speed=vertical(h.floor,s->fall_speed,&s->y);
    int mean=tr2_asr(left[1]+right[1],1);
    s->pitch+=tr2_asr(tr2_word(tr2_atan(550,s->y-mean))-s->pitch,1);
    s->control.roll+=tr2_asr(tr2_word(tr2_atan(260,mean-left[1]))-s->control.roll,1);
    if(o->flags&256){
        s->room=(int16_t)sector.room;tr2_world_room(w,index,sector.room);
        if(s->y==o->floor)tr2_snow_explode(l,w,o);
        return;
    }
    if(state==8){
        int track=b->drive.armed?53:52;
        if(!(w->cd_flags[track]&256)){w->cd_play=track;w->cd_flags[track]|=256;}
    }
    rider_state(l,o,a,hit,input,rb);
    s->room=a->room=(int16_t)sector.room;tr2_world_room(w,index,sector.room);
    if(a->control.current!=12){
        a->x=s->x;a->y=s->y;a->z=s->z;a->yaw=s->yaw;
        a->pitch=boarding?0:s->pitch;a->control.roll=boarding?0:s->control.roll;
    }
    Tr2Object item={0};item.pose=*a;tr2_object_animate(l,&item);*a=item.pose;
    if(!boarding && a->control.health>0 && b->drive.armed)tr2_snow_guns(l,w,a,o,input);
    int animation=sb+a->control.animation-rb;
    if(a->control.health<=0 || state==10)tr2_set_animation(l,s,sb+15,0);
    else if((uint32_t)animation<l->animations.count){
        s->control.animation=animation;
        s->control.frame=a->control.frame+tr2_i16(l->animations.data+animation*32+16)-tr2_i16(l->animations.data+a->control.animation*32+16);
    }
    state=a->control.current;
    if(s->speed && s->y==o->floor){
        tr2_snow_spray(l,w,s);
        if(s->speed<50)tr2_snow_spray(l,w,s);
    }
    if((state==7 || state==9) && a->control.frame==tr2_i16(l->animations.data+a->control.animation*32+18)){
        a->yaw=dismount_yaw(state,a->yaw);
        a->x-=tr2_asr(330*tr2_sin(a->yaw),14);a->z-=tr2_asr(330*tr2_cos(a->yaw),14);
        tr2_set_animation(l,a,11,0);a->control.current=a->control.goal=2;
        a->pitch=a->control.roll=a->gun_status=0;w->vehicle=-1;
    }else if(state==10 && (s->y==o->floor || a->control.frame==tr2_i16(l->animations.data+a->control.animation*32+18))){
        tr2_set_animation(l,a,23,0);a->control.current=9;
        if(s->y==o->floor){
            a->control.goal=8;a->fall_speed=154;a->speed=0;
            tr2_snow_explode(l,w,o);
        }else{
            a->control.goal=9;a->y-=200;a->fall_speed=s->fall_speed;a->speed=s->speed;
            sound(a,30);
        }
        a->pitch=a->control.roll=a->gun_status=0;a->control.flags|=8;a->move_angle=s->yaw;
        o->flags|=256;
    }
}
void tr2_snow_tick(const Tr2Level *l,Tr2World *w,int index,Tr2Actor *a,uint32_t input){
    tr2_snow_view_tick(l,w,index,a,input,NULL);
}
