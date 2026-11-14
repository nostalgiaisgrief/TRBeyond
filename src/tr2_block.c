#include "tr2_world.h"
#include <stdlib.h>

static uint8_t *floor_at(const Tr2Level *l,int x,int y,int z,int room,Tr2Sector *ref){if(!tr2_find_sector(l,x,y,z,room,ref))return NULL;return (uint8_t *)l->rooms[ref->room].sectors.data+ref->index*8;}
/* AlterFloorHeight 0x434160, including the NO_HEIGHT/ceiling sentinel. */
void tr2_block_floor(const Tr2Level *l,const Tr2Object *o,int delta){
    const Tr2Actor *a=&o->pose;Tr2Sector r,t;uint8_t *s=floor_at(l,a->x,a->y,a->z,a->room,&r),*top=floor_at(l,a->x,a->y+delta-1024,a->z,a->room,&t);if(!s || !top)return;
    if(s[5]==129)s[5]=(uint8_t)(top[7]+delta/256);
    else{s[5]=(uint8_t)(s[5]+delta/256);if(s[5]==top[7])s[5]=129;}
    unsigned box=tr2_u16(s+2);if(box<l->boxes.count){uint8_t *b=(uint8_t *)l->boxes.data+box*8+7;if(*b&128){if(delta<0)*b|=64;else *b&=(uint8_t)~64u;}}
}
static int obstruction(const Tr2Level *l,int x,int y,int z,int room,int radius,int height,int quadrant){
    Tr2Actor a={0};a.x=x;a.y=y;a.z=z;a.room=(int16_t)room;Tr2Contact c={0};c.radius=radius;c.quadrant=(int16_t)quadrant;return tr2_static_contact(l,&a,&c,height,room);
}
int tr2_block_can_move(const Tr2Level *l,const Tr2Object *o,const Tr2Actor *lara,int pull,int quadrant){
    const Tr2Actor *a=&o->pose;Tr2Sector r;uint8_t *s=floor_at(l,a->x,a->y,a->z,a->room,&r);if(!s || (s[5]!=129 && (int8_t)s[5]*256!=a->y-1024))return 0;
    int dx=quadrant==1?1024:quadrant==3?-1024:0,dz=quadrant==0?1024:quadrant==2?-1024:0;if(pull){dx=-dx;dz=-dz;}
    int x=a->x+dx,z=a->z+dz;s=floor_at(l,x,a->y,z,a->room,&r);if(!s || obstruction(l,x,a->y,z,r.room,500,1000,quadrant) || (int8_t)s[5]*256!=a->y)return 0;
    if(!pull){Tr2Height h;if(!tr2_query(l,x,a->y,z,r.room,&h,NULL) || h.type)return 0;
        if(!tr2_query(l,x,a->y-924,z,r.room,&h,NULL) || h.ceiling>a->y-924)return 0;return 1;}
    s=floor_at(l,x,a->y-1024,z,r.room,&r);if(!s || (int8_t)s[7]*256>a->y-1024)return 0;
    x+=dx;z+=dz;s=floor_at(l,x,a->y,z,a->room,&r);if(!s || (int8_t)s[5]*256!=a->y)return 0;
    s=floor_at(l,x,a->y-762,z,r.room,&r);if(!s || (int8_t)s[7]*256>a->y-762)return 0;
    s=floor_at(l,lara->x+dx,lara->y,lara->z+dz,lara->room,&r);return s && !obstruction(l,lara->x+dx,lara->y,lara->z+dz,r.room,100,762,(quadrant+2)&3);
}
static int bounds(const Tr2Actor *o,const Tr2Actor *a){
    int dx=a->x-o->x,dz=a->z-o->z,s=tr2_sin(o->yaw),c=tr2_cos(o->yaw);
    int x=tr2_asr((int64_t)dx*c-(int64_t)dz*s,14),z=tr2_asr((int64_t)dx*s+(int64_t)dz*c,14);
    return abs(x)<=300 && a->y==o->y && z>=-692 && z<=-512 && abs(tr2_word(a->pitch-o->pitch))<=1820 && abs(tr2_word(a->yaw-o->yaw))<=5460 && abs(tr2_word(a->control.roll-o->control.roll))<=1820;
}
int tr2_block_use(const Tr2Level *l,Tr2Object *o,Tr2Actor *a,uint32_t input){
    if((o->id<67 || o->id>70) || !(input&TR2_ACTION) || o->status==1 || (a->control.flags&8) || a->y!=o->pose.y)return 0;
    int q=((unsigned)(a->yaw+8192)&65535)>>14;
    if(a->control.current==2){
        if(a->gun_status)return 0;o->pose.yaw=o->pose.move_angle=tr2_word(q*16384);if(!bounds(&o->pose,a))return 0;
        Tr2Sector r;if(!tr2_find_sector(l,o->pose.x,o->pose.y,o->pose.z,a->room,&r) || r.room!=o->pose.room)return 0;
        if(q&1)a->x=(a->x&~1023)+(q==1?924:100);else a->z=(a->z&~1023)+(q==0?924:100);
        a->yaw=a->move_angle=o->pose.yaw;a->control.goal=38;tr2_animate(l,a);if(a->control.current==38)a->gun_status=1;return 1;
    }
    if(a->control.current!=38 || a->control.frame!=tr2_i16(l->animations.data+120*32+16)+19 || !bounds(&o->pose,a))return 0;
    int pull=!(input&TR2_FORWARD);if(pull && !(input&TR2_BACK))return 0;if(!tr2_block_can_move(l,o,a,pull,q))return 0;
    o->pose.control.goal=(int16_t)(pull?3:2);a->control.goal=(int16_t)(pull?37:36);o->active=o->status=1;tr2_block_floor(l,o,1024);
    tr2_object_animate(l,o);tr2_animate(l,a);return 2;
}
void tr2_block_tick(const Tr2Level *l,Tr2World *w,Tr2Object *o,Tr2Actor *lara){
    if(o->flags&256){tr2_block_floor(l,o,1024);o->active=0;o->status=3;return;}
    Tr2Actor *a=&o->pose;a->move_angle=a->yaw;tr2_object_animate(l,o);Tr2Height h;Tr2Sector r;if(!tr2_query(l,a->x,a->y,a->z,a->room,&h,&r))return;
    if(a->y<h.floor)a->control.flags|=8;
    else if(a->control.flags&8){a->control.flags&=(uint16_t)~8u;a->y=h.floor;o->status=2;if(a->event_count<16){int n=a->event_count++;a->event_kind[n]=5;a->event_value[n]=70;}}
    a->room=(int16_t)r.room;
    if(o->status==2){o->status=o->active=0;tr2_block_floor(l,o,-1024);if(tr2_query(l,a->x,a->y,a->z,a->room,&h,NULL))tr2_world_trigger(l,w,lara,h.trigger,1);}
}
