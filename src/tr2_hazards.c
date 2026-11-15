#include "tr2_world.h"
#include <string.h>
#include <stdlib.h>
static void sound(Tr2Actor *a,int id){if(a->event_count<16){int n=a->event_count++;a->event_kind[n]=5;a->event_value[n]=id;}}
static void damage(Tr2Actor *a,int n){a->control.health=tr2_word(a->control.health-n);a->control.flags|=16;}
/* Home doorbell 41d1f0 and alarm 41d230: unlike timed triggers these
   controllers test the code bits directly. The bell consumes those bits. */
void tr2_home_sound_tick(Tr2Object *o){
    if((o->flags&0x3e00)!=0x3e00)return;
    sound(&o->pose,o->id==257?334:335);
    if(o->id==257)o->flags&=(uint16_t)~0x3e00u;
}
/* Ambient sound 0x41d2e0 and Venice timed bell 0x41d3a0. */
void tr2_venice_sound_tick(Tr2World *w,Tr2Object *o){
    if(o->id==213 || o->id==215){uint32_t *random=&w->hazards->random_draw;*random=*random*0x41c64e6du+12345;if(((*random>>10)&32767)<(o->id==215?1024:256))sound(&o->pose,o->id==215?316:329);}
    else if(o->id==216 && o->timer){
        if(o->timer%60==59)sound(&o->pose,208);
        o->timer=tr2_word(o->timer-1);
        if(!o->timer){sound(&o->pose,208);o->timer=-1;o->active=o->status=0;o->flags&=(uint16_t)~0x3e00u;}
    }
}
static int frames(const Tr2Level *l,int id){for(uint32_t i=0;i<l->sequences.count;i++){const uint8_t *p=l->sequences.data+i*8;if(tr2_i32(p)==id)return -tr2_i16(p+4);}return 0;}
static Tr2Fragment *effect(Tr2World *w,int id,int owner,int x,int y,int z,int room){
    for(int i=0;i<TR2_MAX_EFFECTS;i++)if(!w->hazards->effects[i].active){Tr2Fragment *f=w->hazards->effects+i;memset(f,0,sizeof *f);f->active=1;f->object=(int16_t)id;f->room=(int16_t)room;f->x=x;f->y=y;f->z=z;w->hazards->owner[i]=owner;return f;}return NULL;
}
/* Barkhang blades (442750), ice bowl (434cc0), springboard (441b00)
   and shot bell (434db0). Heavy triggers belong to the object's sector. */
void tr2_tibet_trap_tick(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Actor *lara){
    Tr2Actor *p=&o->pose;
    if(o->id==64){
        p->control.goal=tr2_trigger_active(o->flags,&o->timer)?1:0;
        if(p->control.goal && p->control.current==1 && tr2_object_overlap(l,p,lara,100) && tr2_touch(l,p,64,lara,0)){
            damage(lara,400);
            for(int joint=0;joint<2;joint++)for(int y=-10;y<=10;y+=10){
                int point[3]={joint?71:-23,y,-1718};
                if(tr2_joint(l,p,64,joint,point)){Tr2Position pos={point[0],point[1],point[2],p->room,0};tr2_blood(w,&pos,p->speed,p->yaw);}
            }
        }
    }else if(o->id==86){
        if(!p->control.current && p->y==lara->y && (p->x>>10)==(lara->x>>10) && (p->z>>10)==(lara->z>>10)){
            if(lara->control.health<=0)return;
            if(lara->control.current==5 || lara->control.current==16)lara->speed=-lara->speed;
            lara->fall_speed=-240;lara->control.flags|=8;tr2_set_animation(l,lara,34,0);
            lara->control.current=lara->control.goal=3;p->control.goal=1;
        }
    }else if(o->id==88){
        Tr2Height h;p->control.goal=1;
        if(tr2_query(l,p->x,p->y,p->z,p->room,&h,NULL))tr2_world_trigger(l,w,lara,h.trigger,1);
    }else if(p->control.current==1){
        Tr2Fragment *f=effect(w,220,-2,p->x+512,p->y+612,p->z+512,p->room);
        if(f){uint32_t *rng=&w->hazards->random_draw;*rng=*rng*0x41c64e6du+12345;
            f->mesh=tr2_asr(-(int)((*rng>>10)&32767)*frames(l,220),15);f->shade=2048;}
        if(++o->timer==150){w->flip_flags[0]=0x3f00;if(!w->flip_status)tr2_world_flip((Tr2Level *)l,w);}
    }
    tr2_object_animate(l,o);
    if(o->id==88 && !p->control.current)o->active=o->status=0;
    if(o->id==71 && o->status==2 && o->timer>=210)o->active=0;
}
void tr2_snow_explode(const Tr2Level *l,Tr2World *w,Tr2Object *o){
    Tr2Actor *s=&o->pose;
    effect(w,229,-2,s->x,s->y,s->z,s->room);
    tr2_object_fragments(l,w,o,UINT32_MAX-3u);
    o->mesh_mask&=3;
    if(s->event_count<16){int i=s->event_count++;s->event_kind[i]=7;s->event_value[i]=105;}
    w->vehicle=-1;
}
void tr2_snow_spray(const Tr2Level *l,Tr2World *w,const Tr2Actor *s){
    Tr2Fragment *f=effect(w,90,-2,s->x,s->y,s->z,s->room);
    if(!f)return;
    uint32_t *rng=&w->hazards->random_draw;
    *rng=*rng*0x41c64e6du+12345;
    int spread=tr2_asr(260*((int)((*rng>>10)&32767)-16384),14);
    int sn=tr2_sin(s->yaw),cs=tr2_cos(s->yaw);
    f->x-=tr2_asr(500*sn+spread*cs,14);
    f->z-=tr2_asr(500*cs-spread*sn,14);
    f->y+=tr2_asr(500*tr2_sin(s->pitch),14);
    if(s->speed<64){
        *rng=*rng*0x41c64e6du+12345;
        f->fall_speed=tr2_asr((abs(s->speed)-64)*(int)((*rng>>10)&32767),15);
    }
    Tr2Light light;
    tr2_room_light(l,s->room,f->x,f->y,f->z,0,0,&light);
    f->shade=light.shade>512?light.shade-512:0;
}
/* Ember emitter442f20 and EmberControl442ff0. */
void tr2_ember_emit(Tr2World *w,Tr2Object *o){
    Tr2Actor *a=&o->pose;Tr2Fragment *f=effect(w,250,-2,a->x,a->y,a->z,a->room);if(!f)return;
    w->random_state=w->random_state*0x41c64e6du+12345;f->yaw=tr2_word(2*(int)((w->random_state>>10)&32767)-32768);
    w->random_state=w->random_state*0x41c64e6du+12345;f->speed=(int16_t)(((w->random_state>>10)&32767)>>10);
    w->random_state=w->random_state*0x41c64e6du+12345;f->fall_speed=(int16_t)(-(int)((w->random_state>>10)&32767)/200);
    w->random_state=w->random_state*0x41c64e6du+12345;f->mesh=(int16_t)(-4*(int)((w->random_state>>10)&32767)/32767);
    sound(a,149);
}
void tr2_ember_tick(const Tr2Level *l,Tr2World *w,Tr2Fragment *f,Tr2Actor *lara){
    (void)w;
    f->z=tr2_long((int64_t)f->z+tr2_asr(f->speed*tr2_cos(f->yaw),14));
    f->x=tr2_long((int64_t)f->x+tr2_asr(f->speed*tr2_sin(f->yaw),14));
    f->fall_speed=tr2_word(f->fall_speed+6);f->y=tr2_long((int64_t)f->y+f->fall_speed);
    Tr2Height h;Tr2Sector sector;
    if(!tr2_query(l,f->x,f->y,f->z,f->room,&h,&sector) || f->y>=h.floor || f->y<h.ceiling){f->active=0;return;}
    int dx=f->x-lara->x,dy=f->y-lara->y,dz=f->z-lara->z,bounds[6];
    if(abs(dx)<=200 && abs(dz)<=200 && abs(dy)<=3072 && dx*dx+dz*dz<=40000 && tr2_actor_bounds(l,lara,bounds) && dy>=bounds[2] && dy<=bounds[3]+100){
        damage(lara,10);f->active=0;return;
    }
    f->room=(int16_t)sector.room;
}
/* AimMissile433360 shared by the diver and flamethrower. */
static void aim_missile(const Tr2Level *l,Tr2World *w,Tr2Fragment *f,const Tr2Position *p,const Tr2Actor *lara){
    int bounds[6]={0};tr2_actor_bounds(l,lara,bounds);
    int dx=tr2_long((int64_t)lara->x-p->x),dz=tr2_long((int64_t)lara->z-p->z);
    int dy=tr2_long((int64_t)lara->y-p->y+bounds[3]+3*(bounds[2]-bounds[3])/4);
    unsigned n=(unsigned)dx*(unsigned)dx+(unsigned)dz*(unsigned)dz,root=0,bit=1u<<30;
    while(bit>n)bit>>=2;while(bit){if(n>=root+bit){n-=root+bit;root=(root>>1)+bit;}else root>>=1;bit>>=2;}
    f->pitch=tr2_word(-tr2_atan((int)root,dy));f->yaw=tr2_word(tr2_atan(dz,dx));
    w->random_state=w->random_state*0x41c64e6du+12345;f->pitch=tr2_word(f->pitch+((int)((w->random_state>>10)&32767)-16384)/64);
    w->random_state=w->random_state*0x41c64e6du+12345;f->yaw=tr2_word(f->yaw+((int)((w->random_state>>10)&32767)-16384)/64);
}
/* Diver harpoon creation 0x416bf0. */
int tr2_diver_harpoon(const Tr2Level *l,Tr2World *w,const Tr2Position *p,const Tr2Actor *lara){
    Tr2Fragment *f=effect(w,245,-2,p->x,p->y,p->z,p->room);if(!f)return -1;
    f->speed=150;f->shade=3584;aim_missile(l,w,f,p,lara);
    return (int)(f-w->hazards->effects);
}
int tr2_knife_missile(const Tr2Level *l,Tr2World *w,const Tr2Position *p,const Tr2Actor *lara){
    Tr2Fragment *f=effect(w,247,-2,p->x,p->y,p->z,p->room);if(!f)return -1;
    f->speed=150;f->shade=3584;aim_missile(l,w,f,p,lara);return (int)(f-w->hazards->effects);
}
/* FlameMissile4179e0; Dragon model22 changes the original lifetime. */
int tr2_flame_missile(const Tr2Level *l,Tr2World *w,const Tr2Position *p,const Tr2Actor *lara){
    Tr2Fragment *f=effect(w,246,-2,p->x,p->y,p->z,p->room);if(!f)return -1;
    f->speed=200;f->shade=3584;
    uint32_t *random=&w->hazards->random_draw;*random=*random*0x41c64e6du+12345;
    f->mesh=tr2_word(tr2_asr((int)((*random>>10)&32767)*(1-frames(l,246)),15));
    aim_missile(l,w,f,p,lara);f->counter=tr2_model(l,22)?16384:20;
    return (int)(f-w->hazards->effects);
}
/* MissileControl433090, flame246 branch. Near hits do not expire the missile
   or decrement its lifetime; a new burn effect is allocated only once. */
int tr2_flame_missile_tick(const Tr2Level *l,Tr2World *w,Tr2Fragment *f,Tr2Actor *lara){
    f->y=tr2_long((int64_t)f->y+tr2_asr(f->speed*tr2_sin(-f->pitch),14));
    int horizontal=tr2_asr(f->speed*tr2_cos(f->pitch),14);
    f->z=tr2_long((int64_t)f->z+tr2_asr(horizontal*tr2_cos(f->yaw),14));
    f->x=tr2_long((int64_t)f->x+tr2_asr(horizontal*tr2_sin(f->yaw),14));
    Tr2Height h;Tr2Sector sector;
    if(!tr2_query(l,f->x,f->y,f->z,f->room,&h,&sector)){f->active=0;return 0;}
    if(f->y>=h.floor || f->y<=h.ceiling){tr2_dynamic_light(w,f->x,f->y,f->z,14,11);f->active=0;return 0;}
    f->room=(int16_t)sector.room;
    int dx=f->x-lara->x,dy=f->y-lara->y,dz=f->z-lara->z,bounds[6];
    if(abs(dx)<=350 && abs(dz)<=350 && abs(dy)<=3072 && dx*dx+dz*dz<=122500 && tr2_actor_bounds(l,lara,bounds) && dy>=bounds[2] && dy<=bounds[3]+100){
        damage(lara,3);
        if(!w->hazards->burning){Tr2Fragment *burn=effect(w,252,-1,0,0,0,lara->room);if(burn){burn->counter=-1;w->hazards->burning=1;}}
        return 0;
    }
    int old=f->counter;f->counter=tr2_word(old-1);
    if(!old){tr2_dynamic_light(w,f->x,f->y,f->z,14,11);f->active=0;return 305;}
    return 0;
}
/* MissileControl 0x433090, harpoon245 branch. Return positioned one-shot sound. */
int tr2_harpoon_tick(const Tr2Level *l,Tr2World *w,Tr2Fragment *f,Tr2Actor *lara){
    int knife=f->object==247;
    if(!knife && !(l->rooms[f->room].flags&1) && f->pitch> -12288)f->pitch=tr2_word(f->pitch-182);
    f->y=tr2_long((int64_t)f->y+tr2_asr(f->speed*tr2_sin(-f->pitch),14));
    int horizontal=tr2_asr(f->speed*tr2_cos(f->pitch),14);
    f->z=tr2_long((int64_t)f->z+tr2_asr(horizontal*tr2_cos(f->yaw),14));
    f->x=tr2_long((int64_t)f->x+tr2_asr(horizontal*tr2_sin(f->yaw),14));
    Tr2Height h;Tr2Sector sector;
    if(!tr2_query(l,f->x,f->y,f->z,f->room,&h,&sector)){f->active=0;return 0;}
    if(f->y>=h.floor || f->y<=h.ceiling){
        f->speed=0;w->random_state=w->random_state*0x41c64e6du+12345;
        f->mesh=(int16_t)(-(int)((w->random_state>>10)&32767)/11000);f->counter=6;f->object=238;return 258;
    }
    f->room=(int16_t)sector.room;
    int dx=f->x-lara->x,dy=f->y-lara->y,dz=f->z-lara->z,bounds[6];
    if(abs(dx)<=200 && abs(dz)<=200 && abs(dy)<=3072 && dx*dx+dz*dz<=40000 && tr2_actor_bounds(l,lara,bounds) && dy>=bounds[2] && dy<=bounds[3]+100){
        damage(lara,50);f->object=233;f->yaw=lara->yaw;f->speed=lara->speed;f->counter=f->mesh=0;return 317;
    }
    if(knife)f->roll=tr2_word(f->roll+0x1554);
    else if(l->rooms[f->room].flags&1){int p[3]={f->x,f->y,f->z};tr2_bubble(w,p,f->room);}
    return 0;
}
/* Boat wake creation 0x40d0f0; all three particles share one random frame. */
void tr2_boat_wake(const Tr2Level *l,Tr2World *w,const Tr2Actor *a,int shade){
    int count=frames(l,89);if(!w->hazards || !count)return;
    uint32_t *random=&w->hazards->random_draw;*random=*random*0x41c64e6du+12345;
    int mesh=tr2_asr(-(int)((*random>>10)&32767)*count,15),s=tr2_sin(a->yaw),c=tr2_cos(a->yaw);
    for(int i=0,side=300;i<3;i++,side-=300){
        int x=a->x+tr2_asr(-700*s-side*c,14),z=a->z+tr2_asr(side*s-700*c,14);
        Tr2Fragment *f=effect(w,89,-2,x,a->y,z,a->room);if(!f)continue;
        f->yaw=tr2_word(a->yaw+i*16384-16384);f->mesh=(int16_t)mesh;f->counter=20;f->speed=(int16_t)tr2_asr(a->speed,2);
        if(a->speed<64){*random=*random*0x41c64e6du+12345;f->fall_speed=(int16_t)tr2_asr((int)((*random>>10)&32767)*(abs(a->speed)-64),15);}
        f->shade=tr2_word(shade-768);if(f->shade<0)f->shade=0;
    }
}
/* WaterSpriteControl 0x41cbc0. Rising droplets disappear once they fall. */
void tr2_wake_tick(Tr2Fragment *f,int count){
    f->counter=tr2_word(f->counter-1);
    if(!(f->counter&3)){f->mesh=tr2_word(f->mesh-1);if(f->mesh<=-count)f->mesh=0;}
    if(!f->counter || f->fall_speed>0){f->active=0;return;}
    f->z+=tr2_asr(f->speed*tr2_cos(f->yaw),14);f->x+=tr2_asr(f->speed*tr2_sin(f->yaw),14);
    if(f->fall_speed){f->y+=f->fall_speed;f->fall_speed=tr2_word(f->fall_speed+6);}
}
/* Splash 0x41ca70 and SplashControl 0x41cb40. */
void tr2_splash(const Tr2Level *l,Tr2World *w,const Tr2Actor *p){
    int water=tr2_water_height(l,p->x,p->z,p->room,NULL);Tr2Sector sector;int room=p->room;
    if(tr2_find_sector(l,p->x,p->y,p->z,room,&sector))room=sector.room;
    for(int i=0;i<10;i++){
        Tr2Fragment *f=effect(w,230,-2,p->x,water,p->z,room);if(!f)continue;
        uint32_t *random=&w->hazards->random_draw;*random=*random*0x41c64e6du+12345;f->yaw=tr2_word(2*(int)((*random>>10)&32767)-32768);
        *random=*random*0x41c64e6du+12345;f->speed=(int16_t)(((*random>>10)&32767)>>8);
    }
}
void tr2_splash_tick(Tr2Fragment *f,int count){
    f->mesh=tr2_word(f->mesh-1);
    if(f->mesh<=-count)f->active=0;
    else{f->z+=tr2_asr(f->speed*tr2_cos(f->yaw),14);f->x+=tr2_asr(f->speed*tr2_sin(f->yaw),14);}
}
/* MineControl 0x440fc0 and Splash 0x41ca70. */
void tr2_mine_tick(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Actor *a){
    if(o->flags&256)return;Tr2Actor *p=&o->pose;
    if(!w->mines_detonated){
        Tr2Sector sector;if(!tr2_find_sector(l,p->x,p->y-2048,p->z,p->room,&sector))return;
        int found=-1;
        for(int i=w->room_head[sector.room],n=0;i>=0 && (uint32_t)i<w->count && (uint32_t)n<w->count;n++){
            Tr2Object *boat=w->objects+i;
            if(boat->id==14){int dx=boat->pose.x-p->x,dz=boat->pose.z-p->z;
                if(tr2_long((int64_t)dx*dx+(int64_t)dz*dz)<262144){found=i;break;}}
            i=boat->next_room;
        }
        if(found<0)return;
        if(w->vehicle==found){Tr2Object *lara=w->objects+l->lara;lara->pose=*a;tr2_object_fragments(l,w,lara,UINT64_MAX);lara->flags|=256;a->control.health=0;}
        Tr2Object *boat=w->objects+found;boat->id=221;tr2_object_fragments(l,w,boat,UINT64_MAX);tr2_world_kill(w,found);boat->id=14;
        Tr2Height height;if(tr2_query(l,p->x,p->y,p->z,p->room,&height,NULL))tr2_world_trigger(l,w,a,height.trigger,1);
        w->mines_detonated=1;
    }else{
        w->random_state=w->random_state*0x41c64e6du+12345;
        if(((w->random_state>>10)&32767)<30720)return;
    }
    effect(w,229,-2,p->x,p->y-1024,p->z,p->room);
    tr2_splash(l,w,p);
    sound(p,105);o->flags|=256;p->control.flags&=(uint16_t)~32u;o->mesh_mask=1;
}
void tr2_blood(Tr2World *w,const Tr2Position *p,int speed,int yaw){
    if(!w->hazards)return;Tr2Fragment *f=effect(w,233,-2,p->x,p->y,p->z,p->room);if(f){f->speed=tr2_word(speed);f->yaw=tr2_word(yaw);}
}
void tr2_ricochet(Tr2World *w,const Tr2Position *p){
    if(!w->hazards)return;Tr2Fragment *f=effect(w,238,-3,p->x,p->y,p->z,p->room);if(!f)return;
    w->hazards->random_draw=w->hazards->random_draw*0x41c64e6du+12345;
    f->counter=4;f->mesh=(int16_t)(-3*(int)((w->hazards->random_draw>>10)&32767)/32768);
}
void tr2_bubble(Tr2World *w,const int p[3],int room){
    Tr2Fragment *f=effect(w,231,-2,p[0],p[1],p[2],room);if(!f)return;
    w->hazards->random_draw=w->hazards->random_draw*0x41c64e6du+12345;f->speed=(int16_t)(10+6*((w->hazards->random_draw>>10)&32767)/32768);
    w->hazards->random_draw=w->hazards->random_draw*0x41c64e6du+12345;f->mesh=(int16_t)(-3*(int)((w->hazards->random_draw>>10)&32767)/32768);
}
/* FireHarpoon 42bff0 and ControlHarpoonBolt 42c180. Player bolts are
   object 249, distinct from the diver's effect 245. */
int tr2_player_harpoon_fire(const Tr2Level *l,Tr2World *w,Tr2Actor *a){
    if(!w->hazards || !w->combat || w->combat->ammo[7]<=0)return 0;
    const Tr2Level *assets=tr2_weapon_assets(l,249);const uint8_t *m=tr2_model(assets,249);if(!m)return 0;
    for(int i=0;i<TR2_MAX_DARTS;i++)if(!w->hazards->darts[i].active){
        int p[3]={-2,373,77};if(!tr2_weapon_joint(l,a,w->combat,10,p))return 0;
        Tr2Object *o=w->hazards->darts+i;memset(o,0,sizeof *o);o->id=249;o->active=o->status=1;
        Tr2Actor *bolt=&o->pose;bolt->x=p[0];bolt->y=p[1];bolt->z=p[2];bolt->room=a->room;
        tr2_set_animation(assets,bolt,tr2_u16(m+16),0);
        bolt->pitch=tr2_word(a->pitch+w->combat->arms[0].aim.pitch);bolt->yaw=tr2_word(a->yaw+w->combat->arms[0].aim.yaw);
        if((uint32_t)w->combat->target<w->count){
            Tr2Actor *target=&w->objects[w->combat->target].pose;int bounds[6];
            if(tr2_best_bounds(l,target,bounds)){Tr2Position aim;int16_t angles[2];tr2_weapon_target(bounds,target,&aim);tr2_weapon_angles(aim.x-bolt->x,aim.y-bolt->y,aim.z-bolt->z,angles);bolt->yaw=angles[0];bolt->pitch=angles[1];}
        }
        bolt->fall_speed=tr2_word(tr2_asr(-150*tr2_sin(bolt->pitch),14));bolt->speed=tr2_word(tr2_asr(150*tr2_cos(bolt->pitch),14));
        w->combat->ammo[7]--;w->combat->shots++;return 1;
    }
    return 0;
}
void tr2_player_harpoon_step(Tr2Actor *a,int underwater){
    if(!underwater)a->fall_speed=tr2_word(a->fall_speed+3);
    a->y=tr2_long((int64_t)a->y+a->fall_speed);
    a->z=tr2_long((int64_t)a->z+tr2_asr(a->speed*tr2_cos(a->yaw),14));
    a->x=tr2_long((int64_t)a->x+tr2_asr(a->speed*tr2_sin(a->yaw),14));
}
void tr2_player_harpoon_tick(const Tr2Level *l,Tr2World *w,Tr2Object *o){
    /* Non-null collision callbacks after root object setup, all loaded slots.
       Non-intelligent colliders stop a bolt without receiving damage. */
    static const int colliders[]={13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,41,43,45,46,47,48,49,50,51,53,54,58,59,60,61,63,64,66,67,68,69,70,72,73,76,77,78,79,80,81,82,83,84,85,87,88,92,93,94,95,96,101,102,103,104,105,106,107,108,109,110,111,112,113,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,174,175,176,177,182,183,184,185,190,191,192,193,194,195,196,201,202,203,204,205,206,209,210,211,214,222,226,227,251,256,260};
    Tr2Actor *a=&o->pose;int x=a->x,z=a->z;
    if((uint32_t)a->room>=l->room_count){o->active=0;return;}
    tr2_player_harpoon_step(a,l->rooms[a->room].flags&1);
    Tr2Height h;Tr2Sector sector;if(!tr2_query(l,a->x,a->y,a->z,a->room,&h,&sector)){o->active=0;return;}
    a->room=(int16_t)sector.room;
    for(int i=w->room_head[a->room],next;i>=0 && (uint32_t)i<w->count;i=next){
        Tr2Object *t=w->objects+i;next=t->next_room;
        if(!(t->pose.control.flags&32) || t->pose.room!=a->room || (t->id!=72 && t->status==3))continue;
        int collision=t->id==72;
        for(unsigned j=0;!collision && j<sizeof colliders/sizeof *colliders;j++)if(t->id==colliders[j])collision=1;
        if(!collision)continue;
        int b[6];if(!tr2_best_bounds(l,&t->pose,b) || !tr2_grenade_overlap(b,&t->pose,a,x,z,0))continue;
        if(t->id==72){tr2_smash_window(l,w,t);continue;}
        if(tr2_object_intelligent(t->id)){
            Tr2Position hit={a->x,a->y,a->z,a->room,0};tr2_blood(w,&hit,0,0);
            w->inventory->kills+=tr2_player_damage(w,t,4);w->combat->hits++;
        }
        o->active=0;o->status=2;return;
    }
    if(a->y>=h.floor || a->y<=h.ceiling){o->active=0;o->status=2;return;}
    if(l->rooms[a->room].flags&1){int p[3]={a->x,a->y,a->z};tr2_bubble(w,p,a->room);}
}
int tr2_grenade_fire(const Tr2Level *l,Tr2World *w,Tr2Actor *a){
    if(!w->hazards || !w->combat || w->combat->ammo[6]<=0)return 0;
    const Tr2Level *assets=tr2_weapon_assets(l,248);const uint8_t *m=tr2_model(assets,248);if(!m)return 0;
    for(int i=0;i<TR2_MAX_DARTS;i++)if(!w->hazards->darts[i].active){
        int p[3]={-2,373,77};if(!tr2_weapon_joint(l,a,w->combat,10,p))return 0;
        Tr2Object *o=w->hazards->darts+i;memset(o,0,sizeof *o);o->id=248;o->active=o->status=1;
        Tr2Actor *g=&o->pose;g->x=p[0];g->y=p[1];g->z=p[2];g->room=a->room;
        tr2_set_animation(assets,g,tr2_u16(m+16),0);g->pitch=tr2_word(a->pitch+w->combat->arms[0].aim.pitch);g->yaw=tr2_word(a->yaw+w->combat->arms[0].aim.yaw);g->speed=200;
        w->combat->ammo[6]--;w->combat->shots++;return 1;
    }
    return 0;
}
void tr2_grenade_tick(const Tr2Level *l,Tr2World *w,Tr2Object *o){
    Tr2Actor *a=&o->pose;int x=a->x,y=a->y,z=a->z;tr2_grenade_step(a);
    Tr2Height h;Tr2Sector sector;if(!tr2_query(l,a->x,a->y,a->z,a->room,&h,&sector)){o->active=0;return;}
    a->room=(int16_t)sector.room;int explode=a->y>=h.floor || a->y<=h.ceiling,radius=explode?512:0;
    for(int i=w->room_head[a->room],next;i>=0 && (uint32_t)i<w->count;i=next){
        Tr2Object *t=w->objects+i;next=t->next_room;if(t->pose.room!=a->room || t->status==3)continue;
        int glass=t->id==72;if(glass?(t->flags&256):((t->id<15 || t->id>18) && t->id!=21 && t->id!=25 && t->id!=28 && t->id!=29 && t->id!=30 && t->id!=31 && t->id!=34 && t->id!=32 && t->id!=33 && t->id!=36 && t->id!=38 && t->id!=39 && t->id!=214 && !tr2_xian_enemy(t->id) && !tr2_tibetan_enemy(t->id)))continue;
        int bounds[6];if(!tr2_actor_bounds(l,&t->pose,bounds) || !tr2_grenade_overlap(bounds,&t->pose,a,x,z,radius))continue;
        if(glass){tr2_smash_window(l,w,t);continue;}explode=1;
        if(t->status==1){w->combat->hits++;tr2_player_damage(w,t,30);
            if(t->pose.control.health<=0 && (t->id==22 || t->id==46))w->inventory->kills++;
            else if(t->pose.control.health<=0){w->inventory->kills++;tr2_object_fragments(l,w,t,UINT64_MAX);t->status=3;tr2_enemy_finish(w,i);tr2_world_kill(w,(int)i);}}
    }
    if(explode){effect(w,229,-2,x,y,z,a->room);sound(a,105);o->active=0;o->status=2;}
}
void tr2_emitter_tick(const Tr2Level *l,Tr2World *w,Tr2Object *o,int index){
    if(!w->hazards)return;Tr2Actor *a=&o->pose;int on=tr2_trigger_active(o->flags,&o->timer);
    if(o->id==253){int found=0;for(int i=0;i<TR2_MAX_EFFECTS;i++){Tr2Fragment *f=w->hazards->effects+i;if(f->active && f->object==252 && w->hazards->owner[i]==index){found=1;if(!on)f->active=0;}}
        if(on && !found)effect(w,252,index,a->x,a->y,a->z,a->room);return;}
    if(on && a->control.current==0)a->control.goal=1;else if(!on && a->control.current==1)a->control.goal=0;
    int fired=0;
    if(a->control.current==1 && a->control.frame==tr2_i16(l->animations.data+a->control.animation*32+16)){
        for(int i=0;i<TR2_MAX_DARTS;i++){Tr2Object *d=w->hazards->darts+i;if(d->active)continue;const uint8_t *m=tr2_model(l,61);if(!m)break;
            /* 0x4429c9 compares 0x3fec, not the usual quarter-turn 0x4000. */
            memset(d,0,sizeof *d);d->id=61;d->active=d->status=1;d->pose.x=a->x+(a->yaw==-16384?412:a->yaw==16364?-412:0);d->pose.z=a->z+(a->yaw==0?-412:a->yaw==-32768?412:0);d->pose.y=a->y-512;d->pose.room=a->room;d->pose.yaw=d->pose.move_angle=a->yaw;
            tr2_set_animation(l,&d->pose,tr2_u16(m+16),0);d->pose.control.current=d->pose.control.goal=tr2_i16(l->animations.data+d->pose.control.animation*32+6);fired=1;break;}}
    tr2_object_animate(l,o);if(fired)sound(a,254);
}
void tr2_flame_tick(const Tr2Level *l,Tr2World *w,Tr2Fragment *f,Tr2Actor *a){
    if(--f->mesh<=-frames(l,252))f->mesh=0;
    if(f->counter<0){int point[3]={0,0,f->counter==-1?-100:0};if(tr2_joint(l,a,0,-1-f->counter,point)){f->x=point[0];f->y=point[1];f->z=point[2];}f->room=a->room;
        int wh=tr2_water_height(l,f->x,f->z,f->room,NULL);if(wh!=TR2_NO_HEIGHT && f->y>wh){f->active=0;f->counter=0;w->hazards->burning=0;return;}sound(a,150);damage(a,7);return;}
    if(f->counter){f->counter--;return;}
    int dx=f->x-a->x,dz=f->z-a->z,dy=f->y-a->y,b[6];
    if(abs(dx)>600 || abs(dz)>600 || abs(dy)>3072 || dx*dx+dz*dz>360000 || !tr2_best_bounds(l,a,b) || dy<b[2] || dy>b[3]+100)return;
    damage(a,5);if(dx*dx+dz*dz<202500){f->counter=100;if(!w->hazards->burning){Tr2Fragment *burn=effect(w,252,-1,a->x,a->y,a->z,a->room);if(burn){burn->counter=-1;w->hazards->burning=1;}}}
}
void tr2_dragon_sparks(Tr2World *w,Tr2Object *o){
    Tr2Actor *p=&o->pose;
    Tr2Fragment *f=effect(w,239,(int)(o-w->objects),p->x,p->y,p->z,p->room);
    if(f){
        w->random_state=w->random_state*0x41c64e6du+12345;
        int angle=(w->random_state>>10&32767)*2;
        w->random_state=w->random_state*0x41c64e6du+12345;
        f->x+=tr2_asr(5120*tr2_sin(angle),14);f->z+=tr2_asr(5120*tr2_cos(angle),14);
        f->y+=(int)(w->random_state>>12&8191)-1024;
    }
    w->random_state=w->random_state*0x41c64e6du+12345;
    int intensity=10+(w->random_state>>23&3);
    w->random_state=w->random_state*0x41c64e6du+12345;
    tr2_dynamic_light(w,p->x,p->y,p->z,12+(w->random_state>>23&3),intensity);
}
static void dragon_spark_tick(const Tr2Level *l,Tr2World *w,Tr2Fragment *f,int owner){
    if((unsigned)owner>=w->count){f->active=0;return;}
    const Tr2Object *o=w->objects+owner;
    int x=o->pose.x,y=o->pose.y,z=o->pose.z;
    if(o->id==22){
        int s=tr2_sin(o->pose.yaw),c=tr2_cos(o->pose.yaw);
        x+=tr2_asr(1100*c+490*s,14);z+=tr2_asr(490*c-1100*s,14);y-=540;
    }
    if(--f->mesh<=-frames(l,239))f->mesh=0;
    f->x+=tr2_asr(x-f->x,4);f->y+=tr2_asr(y-f->y,4);f->z+=tr2_asr(z-f->z,4);
    if(abs(f->x-x)<256 && abs(f->y-y)<256 && abs(f->z-z)<256)f->active=0;
}
/* SphereOfDoomControl and collision, 41d540 and 41d410. */
static void dragon_wave(const Tr2Level *l,Tr2Object *wave,Tr2Actor *lara){
    Tr2Actor *p=&wave->pose;
    wave->timer+=64;
    p->yaw=tr2_word(p->yaw+(wave->id==210?1820:-1820));
    sound(p,341);
    int radius=wave->timer*5/2;
    int dx=lara->x-p->x,dz=lara->z-p->z;
    if(!(l->rooms[lara->room].flags&1) && (int64_t)dx*dx+(int64_t)dz*dz<(int64_t)radius*radius){
        int yaw=tr2_atan(dz,dx),forward=abs(tr2_word(lara->yaw-yaw))<16384;
        lara->yaw=lara->move_angle=tr2_word(yaw+(forward?0:32768));
        lara->speed=forward?150:-150;lara->fall_speed=-50;lara->control.flags|=8;
        lara->x=p->x+tr2_asr((radius+50)*tr2_sin(yaw),14);
        lara->z=p->z+tr2_asr((radius+50)*tr2_cos(yaw),14);
        lara->pitch=lara->control.roll=0;
        tr2_set_animation(l,lara,34,0);lara->control.current=lara->control.goal=3;
    }
    if(wave->timer>3840)wave->active=0;
}
void tr2_hazards_tick(const Tr2Level *l,Tr2World *w,Tr2Actor *a){
    memset(w->effect_sounds,0,sizeof w->effect_sounds);
    if(!w->hazards)return;
    for(int i=0;i<TR2_MAX_DARTS;i++){Tr2Object *d=w->hazards->darts+i;d->pose.event_count=0;if(!d->active)continue;Tr2Actor *p=&d->pose;
        if(d->id>=209 && d->id<=211){dragon_wave(l,d,a);continue;}
        if(d->id==225){
            tr2_object_animate(l,d);
            const uint8_t *anim=l->animations.data+p->control.animation*32;
            if(p->control.frame==tr2_i16(anim+16)+41){w->cd_play=61;w->camera_bounce-=50;}
            if(p->control.frame==tr2_i16(anim+18)){
                Tr2Height h;if(tr2_query(l,p->x,p->y,p->z,p->room,&h,NULL))tr2_world_trigger(l,w,a,h.trigger,1);
                d->active=0;
            }
            continue;
        }
        if(d->id==248){tr2_grenade_tick(l,w,d);continue;}
        if(d->id==249){tr2_player_harpoon_tick(l,w,d);continue;}
        if(d->id==152){tr2_flare_world(l,w,d);continue;}
        int hit=tr2_object_overlap(l,p,a,100) && tr2_touch(l,p,61,a,0);if(hit){damage(a,50);Tr2Fragment *blood=effect(w,233,-2,p->x,p->y,p->z,a->room);if(blood){blood->speed=a->speed;blood->yaw=a->yaw;}}
        tr2_object_animate(l,d);p->pitch=tr2_word(p->pitch+4096);Tr2Height h;Tr2Sector r;
        if(!tr2_query(l,p->x,p->y,p->z,p->room,&h,&r)){d->active=0;continue;}p->room=(int16_t)r.room;
        if(p->y>=h.floor){d->active=0;Tr2Fragment *f=effect(w,238,-2,p->x,p->y,p->z,p->room);if(f){w->random_state=w->random_state*0x41c64e6du+12345;f->mesh=(int16_t)(-3*(int)((w->random_state>>10)&32767)/32768);f->counter=6;}sound(p,258);}
    }
    for(int i=0;i<TR2_MAX_EFFECTS;i++){Tr2Fragment *f=w->hazards->effects+i;if(!f->active)continue;
        if(f->object==220){
            if(--f->mesh<=-frames(l,220))f->mesh=0;
            f->y+=f->counter;f->counter+=6;
            Tr2Height h;Tr2Sector s;
            if(!tr2_query(l,f->x,f->y,f->z,f->room,&h,&s)){f->active=0;continue;}
            f->room=s.room;w->effect_sounds[i]=284;
            if(f->y>=h.floor){
                uint32_t *rng=&w->hazards->random_draw;*rng=*rng*0x41c64e6du+12345;
                f->object=230;f->y=h.floor;f->yaw=tr2_word(((*rng>>10)&32767)*2);
                f->counter=0;f->speed=50;w->effect_sounds[i]=285;
            }
        }else if(f->object==245 || f->object==247)w->effect_sounds[i]=(uint16_t)tr2_harpoon_tick(l,w,f,a);
        else if(f->object==246)w->effect_sounds[i]=(uint16_t)tr2_flame_missile_tick(l,w,f,a);
        else if(f->object==239)dragon_spark_tick(l,w,f,w->hazards->owner[i]);
        else if(f->object==250)tr2_ember_tick(l,w,f,a);
        else if(f->object==252){int owner=w->hazards->owner[i];if(owner>=0 && (uint32_t)owner<w->count)sound(&w->objects[owner].pose,150);tr2_flame_tick(l,w,f,a);}
        else if(f->object==240){if(--f->counter==0)f->active=0;else{f->roll=(int16_t)(((w->random_state=w->random_state*0x41c64e6du+12345)>>10)&32767);tr2_dynamic_light(w,f->x,f->y,f->z,12,11);}}
        else if(f->object==89)tr2_wake_tick(f,frames(l,89));
        else if(f->object==90){
            tr2_splash_tick(f,frames(l,90));
            if(f->active && f->fall_speed){f->y+=f->fall_speed;f->fall_speed+=6;}
        }else if(f->object==230)tr2_splash_tick(f,frames(l,230));
        else if(f->object==238){if(--f->counter==0)f->active=0;}
        else if(f->object==229){if(++f->counter==2){f->counter=0;if(--f->mesh<=-frames(l,229))f->active=0;else tr2_dynamic_light(w,f->x,f->y,f->z,13,11);}else tr2_dynamic_light(w,f->x,f->y,f->z,12,10);}
        else if(f->object==231){
            f->yaw=tr2_word(f->yaw+1638);f->pitch=tr2_word(f->pitch+2366);
            int x=f->x+tr2_asr(11*tr2_sin(f->yaw),14),y=f->y-f->speed,z=f->z+tr2_asr(8*tr2_cos(f->pitch),14);Tr2Height h;Tr2Sector s;
            if(!tr2_query(l,x,y,z,f->room,&h,&s) || !(l->rooms[s.room].flags&1) || h.ceiling==TR2_NO_HEIGHT || y<=h.ceiling)f->active=0;
            else{f->x=x;f->y=y;f->z=z;f->room=(int16_t)s.room;}
        }
        else if(f->object==233){f->x+=tr2_asr(f->speed*tr2_sin(f->yaw),14);f->z+=tr2_asr(f->speed*tr2_cos(f->yaw),14);if(++f->counter==4){f->counter=0;if(--f->mesh<=-frames(l,233))f->active=0;}}
    }
}





