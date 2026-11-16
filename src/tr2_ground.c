#include "tr2_world.h"
#include <stdlib.h>
#include <string.h>

static void set_anim(const Tr2Level *l,Tr2Actor *a,int animation){if(animation<0 || (uint32_t)animation>=l->animations.count)return;a->control.animation=(int16_t)animation;a->control.frame=tr2_i16(l->animations.data+animation*32+16);}
static void wade_control(Tr2Control *s,uint32_t input){
    if(s->health<=0){s->goal=2;return;}
    if(input&TR2_LEFT){s->turn_rate=tr2_word(s->turn_rate-409);if(s->turn_rate< -1456)s->turn_rate=-1456;s->roll=tr2_word(s->roll-273);if(s->roll< -2002)s->roll=-2002;}
    else if(input&TR2_RIGHT){s->turn_rate=tr2_word(s->turn_rate+409);if(s->turn_rate>1456)s->turn_rate=1456;s->roll=tr2_word(s->roll+273);if(s->roll>2002)s->roll=2002;}
    s->goal=(input&TR2_FORWARD)?(s->water_status==0?1:65):2;
}
void tr2_ground_control(const Tr2Level *l,Tr2Actor *a,uint32_t input){
    if(tr2_ladder_control(a,input) || tr2_hang_control(a,input) || tr2_air_control(l,a,input))return;
    Tr2Control *s=&a->control;int roll_base=tr2_i16(l->animations.data+146*32+16);
    switch(s->current){
    case 38:if(!(input&TR2_ACTION))s->goal=2;break;
    case 70:if(!(input&TR2_ACTION)){s->goal=3;tr2_animate(l,a);s->flags|=8;a->speed=100;a->fall_speed=40;a->move_angle=a->yaw;}break;
    case 0:tr2_control_walk(s,input);break;
    case 1:tr2_control_run(s,input,(int16_t)roll_base);break;
    case 65:wade_control(s,input);break;
    case 21:case 22:
        if(s->health<=0){s->goal=2;break;}
        if(!(input&(s->current==21?TR2_STEP_RIGHT:TR2_STEP_LEFT)))s->goal=2;
        if(input&TR2_LEFT){s->turn_rate=tr2_word(s->turn_rate-409);if(s->turn_rate< -728)s->turn_rate=-728;}
        else if(input&TR2_RIGHT){s->turn_rate=tr2_word(s->turn_rate+409);if(s->turn_rate>728)s->turn_rate=728;}break;
    case 24:if((input&TR2_JUMP) && !(input&TR2_BACK))s->goal=3;break;
    case 32:if((input&TR2_JUMP) && !(input&TR2_FORWARD))s->goal=25;break;
    case 2:
        if(s->health<=0){s->goal=8;break;}
        if((input&TR2_ROLL) && s->water_status!=4){s->animation=146;s->frame=tr2_word(roll_base+2);s->current=45;s->goal=2;break;}
        s->goal=2;
        if(input&1024)s->goal=22;else if(input&2048)s->goal=21;else if(input&TR2_LEFT)s->goal=7;else if(input&TR2_RIGHT)s->goal=6;
        if(s->water_status==4){
            if(input&TR2_JUMP)s->goal=15;
            if(input&TR2_FORWARD){if(input&TR2_WALK)wade_control(s,input);else tr2_control_walk(s,input);break;}
            if(input&TR2_BACK){s->goal=16;break;}break;
        }
        if(input&TR2_JUMP){s->goal=15;break;}
        if(input&TR2_FORWARD){if(input&TR2_WALK)tr2_control_walk(s,input);else tr2_control_run(s,input,(int16_t)roll_base);}
        else if(input&TR2_BACK){if(input&TR2_WALK){s->goal=16;
            if(input&TR2_LEFT){s->turn_rate=tr2_word(s->turn_rate-409);if(s->turn_rate< -728)s->turn_rate=-728;}
            else if(input&TR2_RIGHT){s->turn_rate=tr2_word(s->turn_rate+409);if(s->turn_rate>728)s->turn_rate=728;}
        }else s->goal=5;}
        break;
    case 6:case 7:{
        if(s->health<=0){s->goal=2;break;}int sign=s->current==6?1:-1;s->turn_rate=tr2_word(s->turn_rate+sign*409);
        if(a->gun_status==4)s->goal=20;
        else if((sign>0 && s->turn_rate>728) || (sign<0 && s->turn_rate< -728)){if(input&TR2_WALK)s->turn_rate=(int16_t)(sign*728);else s->goal=20;}
        if(input&TR2_FORWARD)s->goal=s->water_status==4?65:(input&TR2_WALK)?0:1;
        else if(!(input&(sign>0?TR2_RIGHT:TR2_LEFT)))s->goal=2;
        break;
    }
    case 20:
        if(s->health<=0){s->goal=2;break;}
        if(s->turn_rate<0){s->turn_rate=-1456;if(!(input&TR2_LEFT))s->goal=2;}
        else{s->turn_rate=1456;if(!(input&TR2_RIGHT))s->goal=2;}break;
    case 16:
        if(s->health<=0){s->goal=2;break;}
        s->goal=(input&TR2_BACK) && ((input&TR2_WALK) || s->water_status==4)?16:2;
        if(input&TR2_LEFT){s->turn_rate=tr2_word(s->turn_rate-409);if(s->turn_rate< -728)s->turn_rate=-728;}
        else if(input&TR2_RIGHT){s->turn_rate=tr2_word(s->turn_rate+409);if(s->turn_rate>728)s->turn_rate=728;}break;
    case 3:
        if(s->goal==52 || s->goal==11)s->goal=3;
        if(s->goal!=8 && s->goal!=2 && s->goal!=1){
            if((input&TR2_ACTION) && !a->gun_status)s->goal=11;
            if(input&(TR2_ROLL|TR2_BACK))s->goal=68;
            if((input&TR2_WALK) && !a->gun_status)s->goal=52;
            if(a->fall_speed>131)s->goal=9;
        }
        if(input&TR2_LEFT){s->turn_rate=tr2_word(s->turn_rate-409);if(s->turn_rate< -546)s->turn_rate=-546;}
        else if(input&TR2_RIGHT){s->turn_rate=tr2_word(s->turn_rate+409);if(s->turn_rate>546)s->turn_rate=546;}break;
    case 5:
        s->goal=2;
        if(input&TR2_LEFT){s->turn_rate=tr2_word(s->turn_rate-409);if(s->turn_rate< -1092)s->turn_rate=-1092;}
        else if(input&TR2_RIGHT){s->turn_rate=tr2_word(s->turn_rate+409);if(s->turn_rate>1092)s->turn_rate=1092;}break;
    default:break;
    }
}
static void shift(Tr2Actor *a,Tr2Contact *c){a->x+=c->shift[0];a->y+=c->shift[1];a->z+=c->shift[2];memset(c->shift,0,sizeof c->shift);}
static int ceiling(const Tr2Level *l,Tr2Actor *a,Tr2Contact *c){
    if(c->type!=8 && c->type!=32)return 0;
    a->x=c->old[0];a->y=c->old[1];a->z=c->old[2];a->control.current=a->control.goal=2;set_anim(l,a,11);
    a->speed=a->fall_speed=0;a->control.flags&=(uint16_t)~8u;return 1;
}
static int wall(Tr2Actor *a,Tr2Contact *c){
    if(c->type==1 || c->type==16){shift(a,c);a->control.current=a->control.goal=2;a->speed=0;a->control.flags&=(uint16_t)~8u;return 1;}
    if(c->type==2 || c->type==4){shift(a,c);a->yaw=tr2_word(a->yaw+(c->type==2?910:-910));}return 0;
}
static int slide(const Tr2Level *l,Tr2Actor *a,Tr2Contact *c){
    int x=c->tilt_x,z=c->tilt_z;if(abs(x)<=2 && abs(z)<=2)return 0;
    int angle=x>2?-16384:x< -2?16384:0;if(z>2 && z>abs(x))angle=-32768;else if(z< -2 && -z>abs(x))angle=0;
    int delta=tr2_word(angle-a->yaw);shift(a,c);
    int state=(delta>=-16384 && delta<=16384)?24:32;
    if(a->control.current!=state || a->move_angle!=angle){set_anim(l,a,state==24?70:104);a->control.current=a->control.goal=(int16_t)state;a->yaw=tr2_word(angle+(state==32?32768:0));}
    a->move_angle=(int16_t)angle;return 1;
}
static int fallen(const Tr2Level *l,Tr2Actor *a,const Tr2Contact *c){
    if(c->sample[0][0]<=384 || a->control.water_status==4)return 0;set_anim(l,a,34);a->control.current=a->control.goal=3;a->control.flags|=8;a->fall_speed=0;return 1;
}
static void collide_stop(const Tr2Level *l,Tr2Actor *a,Tr2Contact *c,uint32_t input){
    if(c->old_state==2 || c->old_state==6 || c->old_state==7 || c->old_state==20){
        a->control.current=c->old_state;a->control.animation=c->old_animation;a->control.frame=c->old_frame;
        a->control.goal=(input&TR2_LEFT)?7:(input&TR2_RIGHT)?6:2;tr2_animate(l,a);
    }else set_anim(l,a,11);
}
static void ground_collision(const Tr2Level *l,Tr2Actor *a,Tr2Contact *c,uint32_t input){
    if(tr2_ladder_collision(l,a,c,input) || tr2_hang_collision(l,a,c,input) || tr2_air_collision(l,a,c,input))return;
    Tr2Control *s=&a->control;int state=s->current;
    if(state==70)return;
    if(state>=36 && state<=38){a->move_angle=c->facing=a->yaw;c->positive=384;c->negative=-384;c->ceiling_limit=0;c->flags|=3;tr2_contact(l,a,c,762,1);return;}
    if(state==21 || state==22){
        a->move_angle=c->facing=tr2_word(a->yaw+(state==21?16384:-16384));
        s->flags&=(uint16_t)~8u;a->fall_speed=0;c->positive=s->water_status==4?32512:128;c->negative=-128;c->ceiling_limit=0;c->flags|=3;
        if(!tr2_contact(l,a,c,762,1) || ceiling(l,a,c))return;
        if(wall(a,c))collide_stop(l,a,c,input);
        if(fallen(l,a,c) || slide(l,a,c))return;a->y+=c->sample[0][0];return;
    }
    if(state==8){a->move_angle=c->facing=a->yaw;c->positive=384;c->negative=-384;c->ceiling_limit=0;c->radius=400;if(tr2_contact(l,a,c,762,1)){shift(a,c);a->y+=c->sample[0][0];}s->health=-1;return;}
    if(state==45 || state==23){
        s->flags&=(uint16_t)~8u;a->fall_speed=0;a->move_angle=c->facing=tr2_word(a->yaw+(state==23?32768:0));c->flags|=1;c->positive=32512;c->negative=-384;c->ceiling_limit=0;
        if(!tr2_contact(l,a,c,762,1) || ceiling(l,a,c) || slide(l,a,c))return;
        if(state==45){if(fallen(l,a,c))return;}
        else if(c->sample[0][0]>200){set_anim(l,a,93);s->current=s->goal=29;s->flags|=8;a->fall_speed=0;return;}
        if(c->sample[0][0]<c->negative){a->x=c->old[0];a->y=c->old[1];a->z=c->old[2];return;}
        shift(a,c);a->y+=c->sample[0][0];return;
    }
    if(state==24 || state==32){
        a->move_angle=c->facing=tr2_word(a->yaw+(state==32?32768:0));c->positive=32512;c->negative=-512;c->ceiling_limit=0;
        if(!tr2_contact(l,a,c,762,1) || ceiling(l,a,c))return;wall(a,c);
        if(c->sample[0][0]>200){set_anim(l,a,state==24?34:93);s->current=s->goal=state==24?3:29;s->flags|=8;a->fall_speed=0;return;}
        slide(l,a,c);a->y+=c->sample[0][0];if(abs(c->tilt_x)<=2 && abs(c->tilt_z)<=2)s->goal=2;return;
    }
    int wade=state==65,run=state==1 || wade,walk=state==0,back=state==16,fastback=state==5,turn=state==6 || state==7,sliding=state==24 || state==32;
    c->positive=run || sliding || fastback?32512:384;c->negative=-384;c->ceiling_limit=0;c->flags=(uint16_t)(run?1:walk?7:3);
    if(!run){a->fall_speed=0;s->flags&=(uint16_t)~8u;}
    if(!sliding)a->move_angle=tr2_word(a->yaw+(back || fastback?32768:0));
    c->facing=a->move_angle;
    if(!tr2_contact(l,a,c,762,1)){a->x=c->old[0];a->y=c->old[1];a->z=c->old[2];return;}
    if(turn){
        if(c->sample[0][0]>100){set_anim(l,a,34);s->current=s->goal=3;s->flags|=8;a->fall_speed=0;return;}
        if(!slide(l,a,c))a->y+=c->sample[0][0];return;
    }
    if(ceiling(l,a,c))return;
    if((run || walk) && tr2_vault(l,a,c,input))return;
    if(fastback && c->sample[0][0]>200){set_anim(l,a,93);s->current=s->goal=29;s->flags|=8;a->fall_speed=0;return;}
    if(run || walk || back || fastback || sliding){
        if(wall(a,c)){
            if(walk && s->frame>=29 && s->frame<=47)set_anim(l,a,3);
            else if(walk && ((s->frame>=22 && s->frame<=28) || (s->frame>=48 && s->frame<=57)))set_anim(l,a,2);
            else if(run && s->animation!=6){
                Tr2Height h;int x=a->x+tr2_asr(tr2_sin(a->yaw)*256,14),z=a->z+tr2_asr(tr2_cos(a->yaw)*256,14);
                int bump=wade?(c->sample[1][2]==0 && c->sample[1][0]<-640):(!tr2_query(l,x,a->y-640,z,a->room,&h,NULL) || h.floor==TR2_NO_HEIGHT || h.floor<a->y-640 || h.ceiling>a->y-640);
                s->roll=0;
                if(bump){s->current=12;if(s->frame>=0 && s->frame<=21){int frame=s->frame;set_anim(l,a,frame<10?53:54);return;}}
                collide_stop(l,a,c,input);
            }else collide_stop(l,a,c,input);
            if(run)s->roll=0;
        }
    }
    if(fastback){a->y+=c->sample[0][0];return;}
    if(fallen(l,a,c))return;
    int floor=c->sample[0][0];
    if(walk && floor>128)set_anim(l,a,s->frame>=28 && s->frame<=45?59:60);
    if(back && floor>128 && floor<384)set_anim(l,a,s->frame>=964 && s->frame<=993?62:61);
    if(floor>=-384 && floor< -128){
        if(walk)set_anim(l,a,s->frame>=27 && s->frame<=44?58:57);
        else if(run){if(!wade && c->sample[1][0]<=-384)return;set_anim(l,a,s->frame>=3 && s->frame<=14?56:55);}
    }
    if(slide(l,a,c)){if(sliding)a->y+=floor;return;}
    if(sliding)s->goal=2;
    if(!run && !walk && !back && !sliding)shift(a,c);
    a->y+=run && floor>=50?50:floor;
}
void tr2_ground_tick(const Tr2Level *l,Tr2Actor *a,uint32_t input){
    Tr2Contact c={0};c.radius=100;c.old[0]=a->x;c.old[1]=a->y;c.old[2]=a->z;c.old_state=a->control.current;c.old_animation=a->control.animation;c.old_frame=a->control.frame;
    tr2_ground_control(l,a,input);
    int roll=a->control.roll;a->control.roll=(int16_t)(roll< -182?roll+182:roll>182?roll-182:0);
    int rate=a->control.turn_rate;a->control.turn_rate=(int16_t)(rate< -364?rate+364:rate>364?rate-364:0);a->yaw=tr2_word(a->yaw+a->control.turn_rate);
    if(!tr2_animate(l,a))return;
    for(int i=0;i<a->event_count;i++)if(a->event_kind[i]==6 && a->event_value[i]==0){a->yaw=tr2_word(a->yaw+32768);a->move_angle=tr2_word(a->move_angle+32768);}
    /* The six TR2 ladder controls (42d8f0..42da34) clear object push and
       hit reaction before AnimateLara and the room's object contacts. */
    c.flags=(c.old_state>=56 && c.old_state<=61)?0:24;
    if(!tr2_player_object_contacts(l,a,input,&c))ground_collision(l,a,&c,input);
    Tr2Sector ref;if(tr2_find_sector(l,a->x,a->y-381,a->z,a->room,&ref))a->room=(int16_t)ref.room;
}
