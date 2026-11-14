#include "tr2_world.h"
#include <stdlib.h>
#include <string.h>

static const uint8_t *anim(const Tr2Level *l,int n){return n>=0 && (uint32_t)n<l->animations.count?l->animations.data+n*32:NULL;}
static void event(Tr2Actor *p,int kind,int value){if(p->event_count<16){int i=p->event_count++;p->event_kind[i]=kind;p->event_value[i]=value;}}
static void translate(Tr2Actor *p,int x,int y,int z){
    int s=tr2_sin(p->yaw),c=tr2_cos(p->yaw);
    p->x=tr2_long((int64_t)p->x+tr2_asr(tr2_long((int64_t)x*c+(int64_t)z*s),14));
    p->z=tr2_long((int64_t)p->z+tr2_asr(tr2_long((int64_t)z*c-(int64_t)x*s),14));p->y=tr2_long((int64_t)p->y+y);
}
static int commands(const Tr2Level *l,const uint8_t *a,Tr2Actor *p,int ending){
    uint32_t at=tr2_u16(a+30);int count=tr2_i16(a+28);static const int length[]={0,3,2,0,0,2,2};
    for(int i=0;i<count;i++){
        if(at>=l->commands.count)return 0;int op=tr2_i16(l->commands.data+at++*2);
        if(op<1 || op>6)return 0;int n=length[op];if(at+(uint32_t)n>l->commands.count)return 0;
        const uint8_t *c=l->commands.data+at*2;at+=(uint32_t)n;
        if(ending){
            if(op==1)translate(p,tr2_i16(c),tr2_i16(c+2),tr2_i16(c+4));
            else if(op==2){p->fall_speed=tr2_i16(c);p->speed=tr2_i16(c+2);p->control.flags|=8;if(p->calculated_fall){p->fall_speed=p->calculated_fall;p->calculated_fall=0;}}
            else if(op==3 && p->gun_status!=5)p->gun_status=0;
        }else if((op==5 || op==6) && tr2_i16(c)==p->control.frame){
            unsigned value=tr2_u16(c+2),condition=value&0xc000;int water=p->water_distance<0;
            int enabled=!condition || (condition==0x4000 && (!water || p->water_distance==TR2_NO_HEIGHT)) ||
                (condition==0x8000 && water && (op==6 || p->water_distance!=TR2_NO_HEIGHT));
            if(enabled){
                /* Effect 12 (0x41d760) frees Lara's hands during pull-ups and
                   vaults, before the animation ends. Keep the event for observers. */
                if(op==6 && (value&0x3fff)==12)p->gun_status=0;
                event(p,op,value&0x3fff);
                Tr2World *w=l->world;
                if(op==6 && w && (value&0x3fff)==1){
                    /* 41cf50 uses the previous simulation camera position. */
                    int x=p->x-w->ai_camera[0],y=p->y-w->ai_camera[1],z=p->z-w->ai_camera[2];
                    if(abs(x)<16384 && abs(y)<16384 && abs(z)<16384)
                        w->camera_bounce=(1048576-(x*x+y*y+z*z)/256)*100/1048576;
                }
                if(op==6 && (value&0x3fff)==3 && w && w->hazards){
                    uint32_t *rng=&w->hazards->random_draw;
                    *rng=*rng*0x41c64e6du+12345;
                    int count=3*(int)((*rng>>10)&32767)/32768;
                    if(count){
                        if((unsigned)l->listener_room<l->room_count && (l->rooms[l->listener_room].flags&1))event(p,5,37);
                        int mouth[3]={0,0,50};
                        if(tr2_joint(l,p,0,14,mouth))
                            for(int j=0;j<count;j++)tr2_bubble(w,mouth,p->room);
                    }
                }
            }
        }
    }return 1;
}
int tr2_animate_required(const Tr2Level *l,Tr2Actor *p,int16_t *required){
    Tr2Control *s=&p->control;const uint8_t *a=anim(l,s->animation);if(!a)return 0;p->event_count=0;
    s->frame=tr2_word(s->frame+1);
    if(s->current!=s->goal){
        uint32_t start=tr2_u16(a+26),count=tr2_u16(a+24);int changed=0;
        if(start+count>l->changes.count)return 0;
        for(uint32_t i=0;i<count && !changed;i++){
            const uint8_t *c=l->changes.data+(start+i)*6;if(tr2_i16(c)!=s->goal)continue;
            uint32_t first=tr2_u16(c+4),nr=tr2_u16(c+2);if(first+nr>l->ranges.count)return 0;
            for(uint32_t j=0;j<nr;j++){
                const uint8_t *r=l->ranges.data+(first+j)*8;
                if(s->frame>=tr2_i16(r) && s->frame<=tr2_i16(r+2)){
                    s->animation=tr2_i16(r+4);s->frame=tr2_i16(r+6);a=anim(l,s->animation);if(!a)return 0;
                    s->current=tr2_i16(a+6);if(required && *required==s->current)*required=0;changed=1;break;
                }
            }
        }
    }
    if(s->frame>tr2_i16(a+18)){
        if(!commands(l,a,p,1))return 0;s->animation=tr2_i16(a+20);s->frame=tr2_i16(a+22);
        a=anim(l,s->animation);if(!a)return 0;s->current=tr2_i16(a+6);
        if(required && *required==s->current)*required=0;
    }
    if(!commands(l,a,p,0))return 0;
    int velocity=tr2_i32(a+8),accel=tr2_i32(a+12),delta=s->frame-tr2_i16(a+16);
    if(!(s->flags&8))p->speed=tr2_word(tr2_asr(tr2_long((int64_t)velocity+(int64_t)accel*delta),16));
    else{
        int before=tr2_long((int64_t)velocity+(int64_t)(delta-1)*accel);
        p->speed=tr2_word(p->speed-tr2_asr(before,16)+tr2_asr(tr2_long((int64_t)before+accel),16));
        p->fall_speed=tr2_word(p->fall_speed+(p->fall_speed>=128?1:6));p->y=tr2_long((int64_t)p->y+p->fall_speed);
    }
    p->x=tr2_long((int64_t)p->x+tr2_asr(p->speed*tr2_sin(p->move_angle),14));
    p->z=tr2_long((int64_t)p->z+tr2_asr(p->speed*tr2_cos(p->move_angle),14));
    return 1;
}
int tr2_animate(const Tr2Level *l,Tr2Actor *p){return tr2_animate_required(l,p,NULL);}
int tr2_actor_init(const Tr2Level *l,Tr2Actor *p){
    if(l->lara<0 || l->animations.count<=103)return 0;memset(p,0,sizeof *p);
    const uint8_t *item=l->items.data+l->lara*24;p->x=tr2_i32(item+4);p->y=tr2_i32(item+8);p->z=tr2_i32(item+12);
    p->yaw=tr2_i16(item+16);p->move_angle=p->yaw;p->room=tr2_i16(item+2);
    p->control.health=1000;p->control.current=p->control.goal=2;p->control.animation=103;
    p->control.frame=tr2_i16(l->animations.data+103*32+16);p->water_distance=TR2_NO_HEIGHT;return 1;
}
int tr2_actor_bounds(const Tr2Level *l,const Tr2Actor *p,int bounds[6]){
    const uint8_t *a=anim(l,p->control.animation);if(!a || !a[4])return 0;
    int tick=p->control.frame-tr2_i16(a+16),index=tick/a[4],fraction=tick%a[4],rate=a[4];if(index<0)return 0;
    uint32_t offset=tr2_u32(a)+(uint32_t)index*a[5]*2;if(offset+12>l->frames.count*2)return 0;
    int next=(index+1)*rate;if(next>tr2_i16(a+18))rate+=tr2_i16(a+18)-next;
    const uint8_t *b=l->frames.data+offset;
    for(int i=0;i<6;i++){bounds[i]=tr2_i16(b+i*2);if(fraction && rate>0 && offset+a[5]*2+12<=l->frames.count*2)bounds[i]+=(tr2_i16(b+a[5]*2+i*2)-bounds[i])*fraction/rate;}
    return 1;
}
