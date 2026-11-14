#include "tr2_runtime.h"
#include <stdlib.h>
#include <string.h>

void tr2_set_animation(const Tr2Level *l,Tr2Actor *a,int id,int frame){
    if(id<0 || (uint32_t)id>=l->animations.count)return;
    a->control.animation=(int16_t)id;a->control.frame=tr2_word(tr2_i16(l->animations.data+id*32+16)+frame);
}
int tr2_landing_damage(Tr2Actor *a){
    int impact=a->fall_speed-140;if(impact<=0)return 0;
    a->control.health=impact>14?-1:tr2_word(a->control.health-impact*impact*1000/196);
    return a->control.health<=0;
}
static int floor_ahead(const Tr2Level *l,Tr2Actor *a,int angle){
    Tr2Height h;int x=a->x+tr2_asr(256*tr2_sin(angle),14),z=a->z+tr2_asr(256*tr2_cos(angle),14);
    if(!tr2_query(l,x,a->y-762,z,a->room,&h,NULL) || h.floor==TR2_NO_HEIGHT)return TR2_NO_HEIGHT;
    return tr2_word(h.floor-a->y);
}
int tr2_air_control(const Tr2Level *l,Tr2Actor *a,uint32_t input){
    Tr2Control *s=&a->control;
    switch(s->current){
    case 15:
        if(s->water_status!=4){
            const unsigned bits[]={TR2_FORWARD,TR2_LEFT,TR2_RIGHT,TR2_BACK};const int angle[]={0,-16384,16384,32768},goal[]={3,27,26,25};
            for(int i=0;i<4;i++)if((input&bits[i]) && floor_ahead(l,a,tr2_word(a->yaw+angle[i]))>=-384){s->goal=(int16_t)goal[i];a->move_angle=tr2_word(a->yaw+angle[i]);break;}
        }
        if(a->fall_speed>131)s->goal=9;return 1;
    case 9:a->speed=tr2_word(a->speed*95/100);return 1;
    case 52:if(a->fall_speed>131 && s->goal!=35)s->goal=53;return 1;
    case 53:if((input&TR2_ROLL) && s->goal==53)s->goal=68;a->speed=tr2_word(a->speed*95/100);return 1;
    case 25:
        if(a->fall_speed>131)s->goal=9;
        else if(s->goal==1)s->goal=2;
        else if((input&(TR2_FORWARD|TR2_ROLL)) && s->goal!=2)s->goal=68;return 1;
    case 26:case 27:
        if(a->fall_speed>131)s->goal=9;
        else if((input&(s->current==26?TR2_LEFT:TR2_RIGHT)) && s->goal!=2)s->goal=68;return 1;
    case 28:if(a->fall_speed>131)s->goal=9;return 1;
    case 29:
        if(a->fall_speed>131)s->goal=9;
        if((input&TR2_ACTION) && !a->gun_status)s->goal=11;return 1;
    case 11:if(a->fall_speed>131)s->goal=9;return 1;
    default:return 0;
    }
}
static void shift(Tr2Actor *a,Tr2Contact *c){a->x+=c->shift[0];a->y+=c->shift[1];a->z+=c->shift[2];memset(c->shift,0,sizeof c->shift);}
static void deflect(const Tr2Level *l,Tr2Actor *a,Tr2Contact *c,int falling){
    shift(a,c);int type=c->type;
    if(type==2 || type==4)a->yaw=tr2_word(a->yaw+(type==2?910:-910));
    else if(type==8 || (falling && type==16)){if(a->fall_speed<=0)a->fall_speed=1;}
    else if(type==32){a->x-=tr2_asr(100*tr2_sin(c->facing),14);a->z-=tr2_asr(100*tr2_cos(c->facing),14);a->speed=0;c->sample[0][0]=0;if(a->fall_speed<=0)a->fall_speed=16;}
    else if(!falling && (type==1 || type==16)){
        /* 0x42a381: releasing a climbable wall uses speed 2. Keep that
           fall available for a new grab instead of entering wall rebound. */
        if(a->speed==2 && tr2_ladder_marked(l,a))return;
        if(c->sample[0][0]>512){a->control.current=a->control.goal=9;tr2_set_animation(l,a,32,1);}
        else if(c->sample[0][0]<=128){a->control.current=a->control.goal=14;tr2_set_animation(l,a,31,0);}
        a->speed=(int16_t)(a->speed/4);a->move_angle=tr2_word(a->move_angle+32768);if(a->fall_speed<=0)a->fall_speed=1;
    }
}
int tr2_air_collision(const Tr2Level *l,Tr2Actor *a,Tr2Contact *c,uint32_t input){
    int state=a->control.current;Tr2Control *s=&a->control;
    if(state==15){
        s->flags&=(uint16_t)~8u;a->fall_speed=0;c->positive=32512;c->negative=-32512;c->ceiling_limit=0;c->flags=0;c->facing=a->move_angle;
        if(!tr2_contact(l,a,c,762,1))return 1;
        if(c->sample[0][1]>-100){s->current=s->goal=2;tr2_set_animation(l,a,11,0);a->speed=0;a->x=c->old[0];a->y=c->old[1];a->z=c->old[2];}
        if(c->sample[0][0]>-256 && c->sample[0][0]<256)a->y+=c->sample[0][0];return 1;
    }
    if(state==68)return 1;
    if(state!=3 && state!=9 && state!=11 && state!=52 && state!=53 && (state<25 || state>29))return 0;
    if(state==3)a->move_angle=tr2_word(a->yaw+(a->speed<0?32768:0));
    else if(state==25 || state==29)a->move_angle=tr2_word(a->yaw+32768);
    else if(state==26)a->move_angle=tr2_word(a->yaw+16384);
    else if(state==27)a->move_angle=tr2_word(a->yaw-16384);
    else if(state==28 || state==11 || state==52 || state==53)a->move_angle=a->yaw;
    if(state==9 || state==11)s->flags|=8;
    c->positive=32512;c->negative=state==11?0:-384;c->ceiling_limit=192;c->flags=0;c->facing=a->move_angle;
    if(state==28 && a->speed<0)c->facing=tr2_word(c->facing+32768);
    if(!tr2_contact(l,a,c,state==28?870:762,1))return 1;
    if((state==28 || state==11) && tr2_grab(l,a,c,input,state==28))return 1;
    if(state==28){
        shift(a,c);
        if(c->type==32 || c->type==8 || c->type==16)s->goal=2;
        if(c->type)a->speed=a->speed>0?2:-2;
        else if(a->fall_speed< -70){if((input&TR2_FORWARD) && a->speed<5)a->speed++;else if((input&TR2_BACK) && a->speed> -5)a->speed-=2;}
    }else deflect(l,a,c,state==9 || state==11);
    if(state==3 && a->speed<0)a->move_angle=a->yaw;
    if(c->sample[0][0]<=0 && (a->fall_speed>0 || state==9)){
        if(state==52 || state==53){s->goal=state==53 && a->fall_speed>133?8:2;a->fall_speed=0;a->y+=c->sample[0][0];s->flags&=(uint16_t)~8u;return 1;}
        int dead=tr2_landing_damage(a);s->goal=dead?8:state==3 && (input&TR2_FORWARD) && !(input&TR2_WALK) && s->water_status!=4?1:2;
        if(state==9 && !dead){s->current=s->goal=2;tr2_set_animation(l,a,24,0);}
        a->y+=c->sample[0][0];a->fall_speed=0;s->flags&=(uint16_t)~8u;
        if(state==3){a->speed=0;tr2_animate(l,a);}
    }
    if(state>=25 && state<=27 && abs(c->sample[0][1]-c->sample[0][0])<762){
        s->current=s->goal=9;tr2_set_animation(l,a,32,1);a->speed=(int16_t)(a->speed/4);a->move_angle=tr2_word(a->move_angle+32768);if(a->fall_speed<=0)a->fall_speed=1;
    }return 1;
}
