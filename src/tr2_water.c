#include "tr2_runtime.h"
#include "tr2_world.h"
#include <stdlib.h>

static int damp(int n,int rate){return n< -rate?n+rate:n>rate?n-rate:0;}
static int clamp(int n,int limit){return n< -limit?-limit:n>limit?limit:n;}
static void reset_angles(Tr2Actor *a){a->pitch=0;a->control.roll=0;}
static void room_at(const Tr2Level *l,Tr2Actor *a,int offset){Tr2Sector ref;if(tr2_find_sector(l,a->x,a->y+offset,a->z,a->room,&ref))a->room=(int16_t)ref.room;}
static void dive(const Tr2Level *l,Tr2Actor *a){a->control.current=35;a->control.goal=17;tr2_set_animation(l,a,119,0);a->pitch=-8190;a->fall_speed=80;a->control.water_status=1;}
/* WaterCurrent 0x432920. The LOT routes the pull around room geometry. */
void tr2_current_tick(const Tr2Level *l,Tr2World *w,Tr2Actor *a,Tr2Contact *c){
    if(!w || !w->current_strength || a->control.water_status==3)return;
    if((uint32_t)l->lara>=w->count || !w->creatures[l->lara].lot.nodes){w->current_strength=0;return;}
    if((uint32_t)a->room>=l->room_count)return;const Tr2Room *r=l->rooms+a->room;
    int x=tr2_asr(a->x-r->x,10),z=tr2_asr(a->z-r->z,10);
    if(x<0 || z<0 || x>=r->nx || z>=r->nz)return;
    int box=tr2_u16(r->sectors.data+(x*r->nz+z)*8+2),target[3];Tr2Lot *lot=&w->creatures[l->lara].lot;
    if((uint32_t)lot->required_box>=l->boxes.count && lot->required_box!=-1)return;
    if(!tr2_creature_target(l,lot,a,box,target,&w->random_state))return;
    int strength=w->current_strength;a->x+=clamp(target[0]-a->x,strength);a->z+=clamp(target[2]-a->z,strength);a->y+=clamp(target[1]-a->y,strength);w->current_strength=0;
    c->facing=tr2_word(tr2_atan(a->z-c->old[2],a->x-c->old[0]));Tr2Actor query=*a;query.y+=200;
    if(tr2_contact_body(l,&query,c,400,1,a->y)){
        if(c->type==1){if(a->pitch>6370)a->pitch=tr2_word(a->pitch+364);else if(a->pitch< -6370)a->pitch=tr2_word(a->pitch-364);else a->fall_speed=0;}
        else if(c->type==8)a->pitch=tr2_word(a->pitch-364);
        else if(c->type==16)a->fall_speed=0;
        else if(c->type==2)a->yaw=tr2_word(a->yaw+910);
        else if(c->type==4)a->yaw=tr2_word(a->yaw-910);
        if(c->sample[0][0]<0){a->y+=c->sample[0][0];a->pitch=tr2_word(a->pitch+364);}
        a->x+=c->shift[0];a->y+=c->shift[1];a->z+=c->shift[2];
        c->shift[0]=c->shift[1]=c->shift[2]=0;
    }
    c->old[0]=a->x;c->old[1]=a->y;c->old[2]=a->z;
}
void tr2_water_control(const Tr2Level *l,Tr2Actor *a,Tr2Water *w,uint32_t input){
    Tr2Control *s=&a->control;int state=s->current;
    /* Pickup / flare pickup controls 428450 / 428480 do not steer. */
    if(state==39 || state==67)return;
    if(state==44){a->fall_speed=tr2_word(a->fall_speed-8);if(a->fall_speed<0)a->fall_speed=0;a->pitch=(int16_t)damp(a->pitch,364);return;}
    if(state==35){if(input&TR2_FORWARD)a->pitch=tr2_word(a->pitch-182);return;}
    if(state==66){a->fall_speed=0;return;}
    if(s->water_status==1){
        if(s->health<=0){s->goal=44;return;}
        if(input&TR2_ROLL){s->current=66;tr2_set_animation(l,a,203,0);return;}
        if(input&TR2_FORWARD)a->pitch=tr2_word(a->pitch-364);else if(input&TR2_BACK)a->pitch=tr2_word(a->pitch+364);
        if(input&TR2_LEFT){s->turn_rate=(int16_t)clamp(tr2_word(s->turn_rate-409),1092);s->roll=tr2_word(s->roll-546);}
        else if(input&TR2_RIGHT){s->turn_rate=(int16_t)clamp(tr2_word(s->turn_rate+409),1092);s->roll=tr2_word(s->roll+546);}
        if(state==17){a->fall_speed=tr2_word(a->fall_speed+8);if(a->fall_speed>200)a->fall_speed=200;if(!(input&TR2_JUMP))s->goal=18;}
        else if(state==18 || state==13){if(input&TR2_JUMP)s->goal=17;a->fall_speed=tr2_word(a->fall_speed-6);if(a->fall_speed<0)a->fall_speed=0;if(state==18 && a->fall_speed<=133)s->goal=13;if(state==13 && a->gun_status==1)a->gun_status=0;}
        return;
    }
    if(state==33){a->fall_speed=tr2_word(a->fall_speed-4);if(a->fall_speed<0)a->fall_speed=0;}
    if(s->health<=0){s->goal=44;return;}
    int turn=state==33 || state==34?728:364;
    if(input&TR2_LEFT)a->yaw=tr2_word(a->yaw-turn);else if(input&TR2_RIGHT)a->yaw=tr2_word(a->yaw+turn);
    if(state==33){
        if(input&TR2_FORWARD)s->goal=34;else if(input&TR2_BACK)s->goal=47;
        if(input&1024)s->goal=48;else if(input&2048)s->goal=49;
        if(input&TR2_JUMP){w->dive_count=tr2_word(w->dive_count+1);if(w->dive_count==10)s->goal=17;}else w->dive_count=0;
    }else{
        w->dive_count=0;unsigned bit=state==34?TR2_FORWARD:state==47?TR2_BACK:state==48?1024:2048;
        if(!(input&bit) || (state==34 && (input&TR2_JUMP)))s->goal=33;
        a->fall_speed=tr2_word(a->fall_speed+8);if(a->fall_speed>60)a->fall_speed=60;
    }
}
int tr2_surface_exit(const Tr2Level *l,Tr2Actor *a,Tr2Contact *c,uint32_t input){
    int step=c->sample[1][0]+700,quadrant=((a->yaw+8192)&65535)>>14,yaw=tr2_word(quadrant*16384);
    if(c->type!=1 || !(input&TR2_ACTION) || c->sample[1][2]==2 || abs(c->sample[2][0]-c->sample[3][0])>=60 || a->gun_status || c->sample[1][1]>0 || c->sample[0][1]>-384 || step<=-512 || step>316 || abs(tr2_word(a->yaw-yaw))>6370)return 0;
    a->y+=step-5;room_at(l,a,-381);
    if(quadrant&1)a->x=(a->x&~1023)+(quadrant==1?1124:-100);else a->z=(a->z&~1023)+(quadrant==0?1124:-100);
    tr2_set_animation(l,a,step< -128?111:step<128?191:193,0);a->control.current=55;a->control.goal=2;a->yaw=(int16_t)yaw;a->gun_status=1;a->control.flags&=(uint16_t)~8u;reset_angles(a);a->speed=a->fall_speed=0;a->control.water_status=0;return 1;
}
static void collision(const Tr2Level *l,Tr2Actor *a,Tr2Contact *c,uint32_t input,int surface){
    Tr2Control *s=&a->control;Tr2Actor query=*a;int height;
    if(surface){int state=s->current;
        a->move_angle=tr2_word(a->yaw+(state==47?32768:state==48?-16384:state==49?16384:0));
        if(state==33 && s->goal==17)dive(l,a);
        height=800;query.y+=700;c->negative=state==34?-384:-128;
    }else{a->move_angle=tr2_word(a->yaw+((a->pitch< -16384 || a->pitch>16384)?32768:0));height=abs(tr2_asr(762*tr2_sin(a->pitch),14));if(height<200)height=200;query.y+=height/2;c->negative=-height;}
    c->facing=a->move_angle;if(!tr2_contact(l,&query,c,height,1))return;
    a->x+=c->shift[0];a->y+=c->shift[1];a->z+=c->shift[2];
    if(surface){
        if((c->type&57) || (c->sample[0][0]<0 && c->sample[0][2]==2)){a->fall_speed=0;a->x=c->old[0];a->y=c->old[1];a->z=c->old[2];}
        else if(c->type==2 || c->type==4)a->yaw=tr2_word(a->yaw+(c->type==2?910:-910));
        int wh=tr2_water_height(l,a->x,a->z,a->room,NULL);
        if(wh-a->y<=-100)dive(l,a);
        else if(c->type!=1 && c->sample[0][2]!=2 && c->sample[0][0]<0){
            if(c->sample[0][0]<-128){tr2_set_animation(l,a,190,0);s->current=55;s->goal=2;}
            else{tr2_set_animation(l,a,177,0);s->current=s->goal=65;}
            a->y+=c->sample[1][0]+695;room_at(l,a,-381);s->flags&=(uint16_t)~8u;reset_angles(a);a->speed=a->fall_speed=0;s->water_status=4;
        }
        if(s->current==34)tr2_surface_exit(l,a,c,input);
    }else{
        if(c->type==32){a->x=c->old[0];a->y=c->old[1];a->z=c->old[2];a->fall_speed=0;return;}
        if(c->type==1){if(a->pitch>6370)a->pitch=tr2_word(a->pitch+364);else if(a->pitch< -6370)a->pitch=tr2_word(a->pitch-364);else a->fall_speed=0;}
        else if(c->type==8){if(a->pitch>=-8190)a->pitch=tr2_word(a->pitch-364);}
        else if(c->type==16)a->fall_speed=0;
        else if(c->type==2 || c->type==4)a->yaw=tr2_word(a->yaw+(c->type==2?910:-910));
        if(c->sample[0][0]<0){a->y+=c->sample[0][0];a->pitch=tr2_word(a->pitch+364);}
        int depth;tr2_water_height(l,a->x,a->z,a->room,&depth);
        if(depth==TR2_NO_HEIGHT){a->x=c->old[0];a->y=c->old[1];a->z=c->old[2];a->fall_speed=0;}
        else if(depth<=512){tr2_set_animation(l,a,192,0);s->current=55;s->goal=2;s->flags&=(uint16_t)~8u;reset_angles(a);a->speed=a->fall_speed=0;s->water_status=4;Tr2Height h;if(tr2_query(l,a->x,a->y,a->z,a->room,&h,NULL))a->y=h.floor;}
    }
}
void tr2_water_tick(const Tr2Level *l,Tr2Actor *a,Tr2Water *w,uint32_t input){
    int surface=a->control.water_status==2;Tr2Contact c={0};c.old[0]=a->x;c.old[1]=a->y;c.old[2]=a->z;c.radius=surface?100:300;c.positive=32512;c.negative=surface?-128:-400;c.ceiling_limit=surface?100:400;c.flags=surface?0:8;
    tr2_water_control(l,a,w,input);a->control.roll=(int16_t)damp(a->control.roll,364);
    if(!surface){a->pitch=(int16_t)clamp(a->pitch,15470);a->control.roll=(int16_t)clamp(a->control.roll,4004);a->control.turn_rate=(int16_t)damp(a->control.turn_rate,364);a->yaw=tr2_word(a->yaw+a->control.turn_rate);}
    tr2_current_tick(l,l->world,a,&c);tr2_animate(l,a);
    /* Animation effect 0, root 0x41cf30: turn around and reverse pitch.
       The underwater roll's last frame emits this before swim movement. */
    for(int i=0;i<a->event_count;i++)if(a->event_kind[i]==6 && a->event_value[i]==0){a->yaw=tr2_word(a->yaw+32768);a->pitch=tr2_word(-a->pitch);}
    if(surface){a->x+=tr2_asr(a->fall_speed*tr2_sin(a->move_angle),16);a->z+=tr2_asr(a->fall_speed*tr2_cos(a->move_angle),16);}
    else{a->y-=tr2_asr(a->fall_speed*tr2_sin(a->pitch),16);a->x+=tr2_asr(tr2_asr(a->fall_speed*tr2_sin(a->yaw),16)*tr2_cos(a->pitch),14);a->z+=tr2_asr(tr2_asr(a->fall_speed*tr2_cos(a->yaw),16)*tr2_cos(a->pitch),14);}
    if(a->control.current==44){a->control.health=-1;w->air=-1;a->gun_status=1;int wh=tr2_water_height(l,a->x,a->z,a->room,NULL);if(wh!=TR2_NO_HEIGHT && wh<a->y-100)a->y-=5;}
    if(!tr2_player_object_contacts(l,a,input,&c))collision(l,a,&c,input,surface);room_at(l,a,surface?100:0);
}
void tr2_water_transition(const Tr2Level *l,Tr2Actor *a,Tr2Water *w){
    Tr2Control *s=&a->control;int depth,wh=tr2_water_height(l,a->x,a->z,a->room,&depth),distance=wh==TR2_NO_HEIGHT?TR2_NO_HEIGHT:a->y-wh,wet=l->rooms[a->room].flags&1;
    a->water_distance=-distance;
    if(s->water_status==0 && distance>=384){
        if(depth>474 && wet){w->air=1800;s->water_status=1;s->flags&=(uint16_t)~8u;a->y+=100;room_at(l,a,0);
            if(s->current==52 || s->current==53){a->pitch=s->current==52?-8190:-15470;s->goal=35;tr2_animate(l,a);a->fall_speed=tr2_word(a->fall_speed*2);}
            else{a->pitch=-8190;tr2_set_animation(l,a,112,0);s->current=35;s->goal=17;a->fall_speed=tr2_word(a->fall_speed*3/2);}
            /* LaraControl 430555: all above-water to submerged entries,
               including swan dives, emit the ten Splash sprites here. */
            Tr2World *world=l->world;if(world && world->hazards)tr2_splash(l,world,a);
        }
        else if(depth<=474 && distance>384){s->water_status=4;if(!(s->flags&8))s->goal=2;}
    }else if(s->water_status==4){
        if(distance<384){s->water_status=0;if(s->current==65)s->goal=1;}
        else if(distance>730){s->water_status=2;a->y=wh+1;int state=s->current;tr2_set_animation(l,a,state==16?140:state==21?144:state==22?143:116,0);s->current=s->goal=state==16?47:state==21?49:state==22?48:34;s->flags&=(uint16_t)~8u;a->fall_speed=0;w->dive_count=0;reset_angles(a);room_at(l,a,-381);}
    }else if((s->water_status==1 || s->water_status==2) && !wet){
        if(s->water_status==1 && depth!=TR2_NO_HEIGHT && abs(distance)<256){s->water_status=2;a->y=wh+1;tr2_set_animation(l,a,114,0);s->current=s->goal=33;a->fall_speed=0;w->dive_count=11;reset_angles(a);room_at(l,a,-381);}
        else if(s->water_status==2 && distance>384){s->water_status=4;tr2_set_animation(l,a,103,0);s->current=2;s->goal=65;tr2_animate(l,a);a->fall_speed=0;reset_angles(a);}
        else{s->water_status=0;tr2_set_animation(l,a,34,0);s->current=s->goal=3;s->flags|=8;a->speed=(int16_t)(a->fall_speed/4);a->fall_speed=0;reset_angles(a);}
    }
}
/* InitialiseLara 0x431134, supported Rig extra8 entry. */
int tr2_arrival_start(const Tr2Level *l,Tr2World *w,Tr2Actor *a,int extra){
    if(!extra && (uint32_t)a->room<l->room_count){
        int submerged=(l->rooms[a->room].flags&1)!=0;
        a->control.water_status=(int16_t)submerged;
        if(submerged)a->fall_speed=0;
        tr2_set_animation(l,a,submerged?108:11,0);
        a->control.current=a->control.goal=submerged?13:2;
        return 1;
    }
    const uint8_t *model=tr2_model(l,12);if((extra!=8 && extra!=9) || !model || !l->cinematic.count)return 0;
    a->gun_status=1;a->control.water_status=0;tr2_set_animation(l,a,tr2_u16(model+16),0);a->control.current=0;a->control.goal=(int16_t)extra;tr2_animate(l,a);a->extra_animation=1;
    w->cinematic_active=1;w->cinematic_frame=0;w->cinematic_origin=(Tr2Position){a->x,a->y,a->z,a->room,0};w->cinematic_yaw=a->yaw;return 1;
}
void tr2_player_tick(const Tr2Level *l,Tr2Actor *a,Tr2Water *w,uint32_t input){
    Tr2World *world=l->world;if(world)world->lara=a;
    if(world && (uint32_t)world->vehicle<world->count){
        Tr2Object *vehicle=world->objects+world->vehicle;
        if(vehicle->id!=13 || !(vehicle->flags&256)){a->event_count=0;return;}
    }
    if(a->extra_animation){
        /* ExtraLaraControl state0 / Breath 0x428790 restores normal standing. */
        if(a->control.current==0){tr2_set_animation(l,a,103,0);a->control.current=a->control.goal=2;a->gun_status=0;a->extra_animation=0;if(world){world->cinematic_active=0;world->cinematic_fov=14560;world->cinematic_roll=0;}}
        else if(world && (a->control.current==7 || a->control.current==9 || a->control.current==10)){
            const uint8_t *anim=l->animations.data+a->control.animation*32;
            int frame=a->control.frame-tr2_i16(anim+16),state=a->control.current;
            if(frame==1)world->cd_pending=-(state==7?28:state==9?63:27);
            if(state==7){
                if(frame==180)tr2_inventory_add(world,175);
                if(a->control.frame==tr2_i16(anim+18)){
                    a->yaw=tr2_word(a->yaw+16384);Tr2Height h;
                    if(tr2_query(l,a->x,a->y,a->z,a->room,&h,NULL))tr2_world_trigger(l,world,a,h.trigger,1);
                }
            }else if(state==9 && frame==401)tr2_inventory_add(world,174);
            else if(state==10){
                a->control.health=1000;
                /* Shower control 428b68 clears the back attachment on frame 1. */
                if(frame==1)world->combat->back_weapon=0;
                if(a->control.frame==tr2_i16(anim+18)-1)world->finished=1;
            }
        }
        else if(a->control.current==8 && world){Tr2Height h;if(tr2_query(l,a->x,a->y,a->z,a->room,&h,NULL))tr2_world_trigger(l,world,a,h.trigger,0);}
        else if(a->control.current==3){
            if(world)world->hit_direction=-1;
            const uint8_t *animation=l->animations.data+a->control.animation*32;
            if(a->control.frame==tr2_i16(animation+18)){
                int surface=tr2_water_height(l,a->x,a->z,a->room,NULL);
                if(surface!=TR2_NO_HEIGHT && surface<a->y-100)a->y-=5;
            }
        }
        tr2_animate(l,a);return;
    }
    int old_x=a->x,old_y=a->y,old_z=a->z;
    tr2_water_transition(l,a,w);Tr2Control *s=&a->control;
    if(s->water_status==1){if(s->health>=0 && --w->air<0){w->air=-1;s->health=tr2_word(s->health-5);}tr2_water_tick(l,a,w,input);}
    else if(s->water_status==2){if(s->health>=0){w->air=tr2_word(w->air+10);if(w->air>1800)w->air=1800;}tr2_water_tick(l,a,w,input);}
    else{w->air=1800;tr2_ground_tick(l,a,input);}
    if(world && world->inventory){
        uint32_t dx=(uint32_t)(a->x-old_x),dy=(uint32_t)(a->y-old_y),dz=(uint32_t)(a->z-old_z);
        uint32_t square=dx*dx+dy*dy+dz*dz,root=0,bit=1u<<30;
        while(bit>square)bit>>=2;
        while(bit){if(square>=root+bit){square-=root+bit;root=(root>>1)+bit;}else root>>=1;bit>>=2;}
        world->inventory->distance=(int)((uint32_t)world->inventory->distance+root);
    }
}
