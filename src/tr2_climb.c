#include "tr2_runtime.h"
#include <stdlib.h>
#include <math.h>

static int align(int yaw,int tolerance,int *angle){int quadrant=((yaw+8192)&65535)>>14,cardinal=tr2_word(quadrant*16384);if(abs(tr2_word(yaw-cardinal))>tolerance)return 0;*angle=cardinal;return 1;}
int tr2_vault(const Tr2Level *l,Tr2Actor *a,Tr2Contact *c,uint32_t input){
    int yaw;if(c->type!=1 || !(input&TR2_ACTION) || a->gun_status || !align(a->yaw,5460,&yaw))return 0;
    int front=c->sample[1][0],uneven=abs(c->sample[2][0]-c->sample[3][0])>=60;
    if(!uneven && front>=-896 && front<=-384){
        for(int i=1;i<4;i++)if(c->sample[i][0]<c->sample[i][1])return 0;
        int high=front< -640;tr2_set_animation(l,a,high?42:50,0);a->control.current=19;a->control.goal=2;a->y+=front+(high?768:512);a->gun_status=1;
    }else if(!uneven && front>=-1920 && front<=-896){
        tr2_set_animation(l,a,11,0);a->control.current=2;a->control.goal=28;a->calculated_fall=tr2_word(-3-(int)sqrt((double)(-9600-12*front)));tr2_animate(l,a);
    }else if(tr2_ladder_marked(l,a)){
        if(front<=-1920 && a->control.water_status!=4 && c->sample[2][0]<=-1920 && c->sample[3][0]<=-2048 && c->sample[0][1]<=-1158){
            tr2_set_animation(l,a,11,0);a->control.current=2;a->control.goal=28;a->calculated_fall=-116;tr2_animate(l,a);
        }else if((front< -1024 || c->sample[1][1]>=506) && c->sample[0][1]<=-518){
            a->x+=c->shift[0];a->y+=c->shift[1];a->z+=c->shift[2];
            if(!tr2_ladder_stance(l,a,c->radius))return 0;tr2_set_animation(l,a,11,0);a->control.current=2;a->control.goal=56;tr2_animate(l,a);a->yaw=(int16_t)yaw;a->gun_status=1;return 1;
        }else return 0;
    }else return 0;
    a->yaw=(int16_t)yaw;a->x+=c->shift[0];a->y+=c->shift[1];a->z+=c->shift[2];return 1;
}
int tr2_grab(const Tr2Level *l,Tr2Actor *a,Tr2Contact *c,uint32_t input,int up){
    int b[6],yaw;if(c->type!=1 || !(input&TR2_ACTION) || a->gun_status || c->hit_static || c->sample[0][1]>-384 || (!up && c->sample[0][0]<200))return 0;
    if(!tr2_actor_bounds(l,a,b))return 0;
    int edge=c->sample[1][0]-b[2],grid=0,ledge=0;
    if((edge<0 && edge+a->fall_speed<0) || (edge>0 && edge+a->fall_speed>0)){
        int head=a->y+b[2],next=head+a->fall_speed;if(((head^next)&~255)==0 || !tr2_ladder_hang(l,a,c))return 0;grid=1;ledge=(a->fall_speed>0?next:head)&~255;
    }else if(abs(c->sample[2][0]-c->sample[3][0])>=60)return 0;
    if(!align(a->yaw,6370,&yaw))return 0;
    int animation=96;
    if(!up){Tr2Height h;int x=a->x+tr2_asr(256*tr2_sin(yaw),14),z=a->z+tr2_asr(256*tr2_cos(yaw),14);
        if(tr2_query(l,x,a->y,z,a->room,&h,NULL) && h.floor!=TR2_NO_HEIGHT && h.floor>a->y && h.ceiling-a->y< -400)animation=150;}
    tr2_set_animation(l,a,animation,up?12:0);a->control.current=a->control.goal=10;
    if(!tr2_actor_bounds(l,a,b))return 0;
    if(grid)a->y=ledge-b[2];else a->y+=c->sample[1][0]-b[2];
    if(!grid || up){a->x+=c->shift[0];a->z+=c->shift[2];}a->yaw=(int16_t)yaw;a->gun_status=1;
    if(up){a->control.flags&=(uint16_t)~8u;a->speed=a->fall_speed=0;}
    else{a->control.flags|=8;a->speed=2;a->fall_speed=1;}return 1;
}
int tr2_hang_control(Tr2Actor *a,uint32_t input){
    int state=a->control.current;
    if(state==10){if(input&(TR2_LEFT|1024))a->control.goal=30;else if(input&(TR2_RIGHT|2048))a->control.goal=31;return 1;}
    if(state==30 || state==31){if(!(input&(state==30?(TR2_LEFT|1024):(TR2_RIGHT|2048))))a->control.goal=10;return 1;}
    return state==19 || state==54 || state==55 || state==40 || state==41;
}
int tr2_hang_collision(const Tr2Level *l,Tr2Actor *a,Tr2Contact *c,uint32_t input){
    int state=a->control.current;
    if(state==19 || state==54 || state==55 || state==40 || state==41){a->move_angle=a->yaw;c->facing=a->yaw;c->positive=384;c->negative=-384;c->ceiling_limit=0;c->flags|=3;tr2_contact(l,a,c,762,1);return 1;}
    if(state!=10 && state!=30 && state!=31)return 0;
    int marked=tr2_ladder_marked(l,a);
    if(state==30 || state==31)a->move_angle=tr2_word(a->yaw+(state==30?-16384:16384));
    c->positive=32512;c->negative=-32512;c->ceiling_limit=0;c->flags=0;c->facing=a->move_angle;
    if(!tr2_contact(l,a,c,762,1))return 1;int too_close=c->sample[1][0]<200;
    a->control.flags&=(uint16_t)~8u;a->fall_speed=0;a->move_angle=a->yaw;
    int quadrant=((a->yaw+8192)&65535)>>14;
    if(quadrant&1)a->x+=quadrant==1?2:-2;else a->z+=quadrant==0?2:-2;
    c->negative=-384;c->facing=a->move_angle;if(!tr2_contact(l,a,c,762,1))return 1;
    if(marked && (!(input&TR2_ACTION) || a->control.health<=0)){
        int root[3]={0,0,0};
        if(tr2_joint(l,a,0,0,root)){if(quadrant&1)a->z=root[2];else a->x=root[0];}
        tr2_set_animation(l,a,34,0);a->control.current=a->control.goal=3;
        a->y+=256;a->control.flags|=8;a->speed=2;a->fall_speed=1;a->gun_status=0;
        goto hang_done;
    }
    if(marked){
        if(!tr2_ladder_hang(l,a,c)){a->x=c->old[0];a->y=c->old[1];a->z=c->old[2];a->control.current=a->control.goal=10;tr2_set_animation(l,a,96,21);}
        else if(a->control.animation==96 && a->control.frame==tr2_i16(l->animations.data+96*32+16)+21 && tr2_ladder_stance(l,a,c->radius))a->control.goal=56;
        goto hang_done;
    }
    int b[6];tr2_actor_bounds(l,a,b);
    if(!(input&TR2_ACTION) || a->control.health<=0 || c->sample[1][0]>0){
        a->control.current=a->control.goal=28;tr2_set_animation(l,a,28,9);tr2_actor_bounds(l,a,b);a->y+=b[3];a->x+=c->shift[0];a->z+=c->shift[2];a->control.flags|=8;a->speed=2;a->fall_speed=1;a->gun_status=0;
    }else{
        int delta=c->sample[1][0]-b[2];
        if(abs(c->sample[2][0]-c->sample[3][0])<60 && c->sample[0][1]<0 && c->type==1 && !too_close && !c->hit_static && abs(delta)<=60){
            if(quadrant&1)a->x+=c->shift[0];else a->z+=c->shift[2];a->y+=delta;
        }else{a->x=c->old[0];a->y=c->old[1];a->z=c->old[2];if(state==30 || state==31){a->control.current=a->control.goal=10;tr2_set_animation(l,a,96,21);}}
    }
hang_done:
    if(state==10 && a->control.goal==10){
        int clear=c->sample[1][0]>-850 && c->sample[1][0]<-650 && !c->hit_static;
        for(int i=1;i<4;i++)if(c->sample[i][0]<c->sample[i][1])clear=0;
        if((input&TR2_FORWARD) && clear)a->control.goal=(input&TR2_WALK)?54:19;
        else if(tr2_ladder_marked(l,a) && a->control.animation==96 && a->control.frame==tr2_i16(l->animations.data+96*32+16)+21){
            if((input&TR2_FORWARD) && c->sample[0][1]<=-256)tr2_set_animation(l,a,187,0);
            else if(!(input&TR2_FORWARD) && (input&TR2_BACK))tr2_set_animation(l,a,188,0);
        }
    }
    if(state==30 || state==31)a->move_angle=tr2_word(a->yaw+(state==30?-16384:16384));return 1;
}
