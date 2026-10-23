/* Independently recovered from DOS 0x283b0, 0x29430..0x2a440. */
#include "water.h"
#include "fixed.h"
#include "navigation.h"
#include "enemies.h"
#include "object_contact.h"
static int32_t add(int32_t a,int32_t b) { return tomb_long((int64_t)a+b); }
static int32_t sub(int32_t a,int32_t b) { return tomb_long((int64_t)a-b); }
static int32_t mul(int32_t a,int32_t b) { return tomb_long((int64_t)a*b); }
static int32_t sine(const int16_t *s,int angle) {
    unsigned v=(unsigned)angle&65535; int sign=v>=32768?-1:1;
    v&=32767; if(v>16384) v=32768-v; return sign*s[v>>4];
}
static void dive(TombActor *a,TombWater *w) {
    a->goal=17; a->current=35; a->animation=119; a->frame=2041;
    w->pitch=-8190; a->fall_speed=80; w->status=1;
}
int tomb_water_control(TombActor *a,TombWater *w,int surface) {
    uint32_t in=w->input; int state=a->current;
    if(!surface) {
        if(state==39 || state==40)return 1;
        if(state==35) { if(in&1) w->pitch=tomb_word(w->pitch-182); return 1; }
        if(state!=13 && state!=17 && state!=18) return 0;
        if(w->health<=0) { a->goal=44; return 1; }
        if(in&1) w->pitch=tomb_word(w->pitch-364); else if(in&2) w->pitch=tomb_word(w->pitch+364);
        if(in&4) { a->yaw=tomb_word(a->yaw-1092); w->lean=tomb_word(w->lean-546); }
        else if(in&8) { a->yaw=tomb_word(a->yaw+1092); w->lean=tomb_word(w->lean+546); }
        if(state==17) {
            a->fall_speed=tomb_word(a->fall_speed+8); if(a->fall_speed>200) a->fall_speed=200;
            if(!(in&16)) a->goal=18;
        } else {
            if(in&16) a->goal=17;
            a->fall_speed=tomb_word(a->fall_speed-6); if(a->fall_speed<0) a->fall_speed=0;
            if(state==18 && a->fall_speed<=133) a->goal=13;
        }
        return 1;
    }
    if(state!=33 && state!=34 && state!=47 && state!=48 && state!=49) return 0;
    if(state==33) { a->fall_speed=tomb_word(a->fall_speed-4); if(a->fall_speed<0) a->fall_speed=0; }
    if(w->health<=0) { a->goal=44; return 1; }
    int rate=(state==33 || state==34)?728:364;
    if(in&4) a->yaw=tomb_word(a->yaw-rate); else if(in&8) a->yaw=tomb_word(a->yaw+rate);
    if(state==33) {
        if(in&1) a->goal=34; else if(in&2) a->goal=47;
        if(in&1024) a->goal=48; else if(in&2048) a->goal=49;
        if(in&16) { w->dive_count=tomb_word(w->dive_count+1); if(w->dive_count==10) dive(a,w); }
        else w->dive_count=0;
    } else {
        w->dive_count=0;
        uint32_t needed=state==34?1:state==47?2:state==48?1024:2048;
        if(!(in&needed) || (state==34 && (in&16))) a->goal=33;
        a->fall_speed=tomb_word(a->fall_speed+8); if(a->fall_speed>60) a->fall_speed=60;
    }
    return 1;
}
void tomb_water_damping(TombWater *w,int surface) {
    if(w->lean>=-364 && w->lean<=364) w->lean=0;
    else w->lean=tomb_word(w->lean+(w->lean<0?364:-364));
    if(!surface) {
        if(w->pitch>18200) w->pitch=18200; else if(w->pitch<-18200) w->pitch=-18200;
        if(w->lean>4004) w->lean=4004; else if(w->lean<-4004) w->lean=-4004;
    }
}
void tomb_water_motion(TombActor *a,const TombWater *w,int surface) {
    int32_t speed=a->fall_speed;
    int angle=surface?w->move_angle:a->yaw;
    int32_t dx=tomb_asr(mul(sine(w->sine,angle),speed),16),dz=tomb_asr(mul(sine(w->sine,angle+16384),speed),16);
    if(!surface) {
        a->y=sub(a->y,tomb_asr(mul(sine(w->sine,w->pitch),speed),16));
        int32_t cosine=sine(w->sine,w->pitch+16384);
        dx=tomb_asr(mul(cosine,dx),14); dz=tomb_asr(mul(cosine,dz),14);
    }
    a->x=add(a->x,dx); a->z=add(a->z,dz);
}
static void shift(TombActor *a,TombContact *c) { a->x=add(a->x,c->shift_x); a->y=add(a->y,c->shift_y); a->z=add(a->z,c->shift_z); c->shift_x=c->shift_y=c->shift_z=0; }
void tomb_underwater_collision(TombActor *a,TombContact *c,TombWater *w) {
    w->move_angle=(w->pitch<-16384 || w->pitch>16384)?tomb_word(a->yaw-32768):a->yaw; c->facing=w->move_angle;
    w->query(w->user,a,c,400); shift(a,c);
    switch(c->type) {
    case 1: if(w->pitch>6370) w->pitch=tomb_word(w->pitch+364); else if(w->pitch<-6370) w->pitch=tomb_word(w->pitch-364); else a->fall_speed=0; break;
    case 8: if(w->pitch>=-8190) w->pitch=tomb_word(w->pitch-364); break;
    case 16:a->fall_speed=0;break;
    case 2:a->yaw=tomb_word(a->yaw+910);break;
    case 4:a->yaw=tomb_word(a->yaw-910);break;
    case 32:a->fall_speed=0;return;
    }
    if(c->floor<0) { a->y=add(a->y,c->floor); w->pitch=tomb_word(w->pitch+364); }
}
int tomb_water_exit(TombActor *a,const TombContact *c,const TombLedgeSamples *s,TombWater *w) {
    int32_t diff=sub(s->left_floor,s->right_floor); if(diff<0) diff=tomb_long(-(int64_t)diff);
    if(w->move_angle!=a->yaw || c->type!=1 || !(w->input&64) || diff>=60 || s->front_ceiling>0 || s->ceiling>-384) return 0;
    int32_t delta=add(s->front_floor,700); if(delta<=-512 || delta>100) return 0;
    int yaw=a->yaw;
    if(yaw>=-6370 && yaw<=6370) yaw=0;
    else if(yaw>=10014 && yaw<=22754) yaw=16384;
    else if(yaw>=26397 || yaw<=-26397) yaw=-32768;
    else if(yaw>=-22754 && yaw<=-10014) yaw=-16384;
    if((uint16_t)yaw&0x3fff) return 0;
    a->y=add(a->y,delta-5); w->room(w->user,a,-381);
    if(yaw==0) a->z=add(a->z&~1023,1124);
    else if(yaw==16384) a->x=add(a->x&~1023,1124);
    else if(yaw==-32768) a->z=add(a->z&~1023,-100);
    else a->x=add(a->x&~1023,-100);
    a->animation=111; a->frame=1849; a->current=55; a->goal=2;
    w->pitch=w->lean=0; a->speed=a->fall_speed=0; a->yaw=(int16_t)yaw; a->flags&=(uint8_t)~8u;
    w->weapon_status=1; w->status=0; return 1;
}
void tomb_surface_collision(TombActor *a,TombContact *c,TombLedgeSamples *s,TombWater *w) {
    c->facing=w->move_angle; w->query(w->user,a,c,700); shift(a,c);
    if((c->type&0x39) || c->floor<0) { a->fall_speed=0; a->x=c->old_x; a->y=c->old_y; a->z=c->old_z; }
    else if(c->type==2) a->yaw=tomb_word(a->yaw+910);
    else if(c->type==4) a->yaw=tomb_word(a->yaw-910);
    if(sub(w->height(w->user,a),a->y)<=-100) dive(a,w);
    else tomb_water_exit(a,c,s,w);
}
int16_t tomb_water_height(const TombLevel *l,int32_t x,int32_t z,int room) {
    if(!l || room<0 || (size_t)room>=l->room_count) return -32512;
    int wet=l->rooms[room].flags&1;
    for(size_t n=0;n<=l->room_count;n++) {
        const TombRoom *r=&l->rooms[room];
        int ix=tomb_asr(sub(x,r->x),10),iz=tomb_asr(sub(z,r->z),10);
        if(ix<0 || ix>=r->nx || iz<0 || iz>=r->nz) return -32512;
        const TombSector *s=&r->sectors[ix*r->nz+iz];
        int next=wet?s->above:s->below;
        if(next==255) return wet?(int16_t)(s->ceiling*256):-32512;
        if((size_t)next>=l->room_count) return -32512;
        if(!!(l->rooms[next].flags&1)!=!!wet) return (int16_t)((wet?s->ceiling:s->floor)*256);
        room=next;
    }
    return -32512;
}
void tomb_water_transition(TombActor *a,TombWater *w,int wet_room,int16_t height) {
    if(!w->status && wet_room) {
        w->air=1800; w->status=1; a->flags&=(uint8_t)~8u; a->y=add(a->y,100); w->room(w->user,a,0);
        w->pitch=-8190; a->animation=112; a->frame=1895; a->current=35; a->goal=17;
        a->fall_speed=tomb_word(a->fall_speed*3/2);
    } else if(w->status==1 && !wet_room && height!=-32512 && sub(height,a->y)>-256 && sub(height,a->y)<256) {
        a->frame=1937; a->animation=114; a->current=a->goal=33; a->fall_speed=0; w->status=2;
        a->y=add(height,1); w->pitch=w->lean=0; w->dive_count=11; w->room(w->user,a,-381);
    } else if((w->status==1 || w->status==2) && !wet_room) {
        a->speed=(int16_t)(a->fall_speed/4); a->fall_speed=0; a->frame=492; a->animation=34; a->current=a->goal=3;
        w->status=0; w->weapon_status=0; w->pitch=w->lean=0; a->flags|=8;
    }
}

/* Original current displacement/collision 0x299e4. A sink is reached through
   the same LOT corridor target calculation used by the DOS executable. */
void tomb_current_pull(TombActor *a,TombContact *c,TombWater *w,const int32_t target[3],int strength){
    int32_t delta[3]={sub(target[0],a->x),sub(target[1],a->y),sub(target[2],a->z)};
    for(int i=0;i<3;i++)delta[i]=delta[i]>strength?strength:delta[i]<-strength?-strength:delta[i];
    a->x=add(a->x,delta[0]);a->y=add(a->y,delta[1]);a->z=add(a->z,delta[2]);
    c->facing=tomb_object_angle(sub(a->z,c->old_z),sub(a->x,c->old_x));
    w->query(w->user,a,c,400);
    if(c->type==1){if(w->pitch>6370)w->pitch=tomb_word(w->pitch+364);else if(w->pitch<-6370)w->pitch=tomb_word(w->pitch-364);else a->fall_speed=0;}
    else if(c->type==8)w->pitch=tomb_word(w->pitch-364);
    else if(c->type==16)a->fall_speed=0;
    else if(c->type==2)a->yaw=tomb_word(a->yaw+910);
    else if(c->type==4)a->yaw=tomb_word(a->yaw-910);
    if(c->floor<0){a->y=add(a->y,c->floor);w->pitch=tomb_word(w->pitch+364);}
    a->x=add(a->x,c->shift_x);a->y=add(a->y,c->shift_y);a->z=add(a->z,c->shift_z);
    c->shift_x=c->shift_y=c->shift_z=0;
    c->old_x=a->x;c->old_y=a->y;c->old_z=a->z;
}
int tomb_water_current(TombObjects *objects,TombActor *a,TombContact *c,TombWater *w){
    if(!objects || !objects->current || !objects->current_strength)return 0;
    int32_t target[3];int box=tomb_navigation_box(objects->level,a);
    if(!tomb_navigation_target(objects->current,objects->level,a,box,&objects->enemies->random,target))return 0;
    int strength=objects->current_strength;objects->current_strength=0;
    tomb_current_pull(a,c,w,target,strength);return 1;
}
