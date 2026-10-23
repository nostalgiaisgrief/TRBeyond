/* DOS 0x2a460, 0x2ace8, 0x2b8a0..0x2bcd8, 0x2be38.
   No ammo limit for pistols: the original replenishes it before each shot. */
#include "pistols.h"
#include "fixed.h"
#include <string.h>
uint32_t tomb_control_random(uint32_t *seed){*seed=*seed*0x41c64e6du+0x3039u;return (*seed>>10)&32767;}
void tomb_pistols_init(TombPistols *g){memset(g,0,sizeof *g);g->target=-1;g->random=(uint32_t)-747505337;}
static int16_t approach(int16_t a,int16_t b,int step){return a<b-step?tomb_word(a+step):a>b+step?tomb_word(a-step):b;}
void tomb_pistols_aim(TombGunArm *a,int16_t yaw,int16_t pitch){a->yaw=approach(a->yaw,a->lock?yaw:0,1820);a->pitch=approach(a->pitch,a->lock?pitch:0,1820);a->roll=0;}
static void undraw(TombGunArm *a,TombPistols *g,TombGunSound sound,void *user) {
    if(a->frame>=24)a->frame=4;
    else if(a->frame>0 && a->frame<5){a->yaw-=a->yaw/a->frame;a->pitch-=a->pitch/a->frame;--a->frame;}
    else if(!a->frame){a->yaw=a->pitch=0;a->frame=23;}
    else if(a->frame>5 && a->frame<24){--a->frame;if(a->frame==12){g->drawn&=a==&g->left?~1:~2;if(sound)sound(user,7);}}
}
static void fire_arm(TombGunArm *a,TombPistols *g,int action,TombGunFire fire,TombGunSound sound,void *user,int shot_sound,int recoil,int flash) {
    if(a->lock || (action && g->target<0 && g->target!=-2)) {
        if(a->frame>=0 && a->frame<4)++a->frame;
        else if(action && a->frame==4){if(!fire || fire(user,a->yaw,a->pitch)){++g->shots;a->flash=(int16_t)flash;if(sound)sound(user,shot_sound);}a->frame=24;}
        else if(a->frame>=24 && ++a->frame==24+recoil)a->frame=4;
    } else if(a->frame>=24)a->frame=4;
    else if(a->frame>0 && a->frame<=4)--a->frame;
}
static void dual_tick(TombPistols *g,int toggle,int action,int alive,int water,TombGunFire fire,TombGunSound sound,void *user,int shot_sound,int recoil,int flash) {
    if(g->left.flash>0)--g->left.flash;if(g->right.flash>0)--g->right.flash;
    if(!alive)g->status=0;
    else if((water && g->status==4) || (!water && toggle)) {
        if(!g->status){g->status=2;g->left.frame=g->right.frame=0;}
        else if(g->status==4)g->status=3;
    }
    if(g->status==2) {
        int frame=g->left.frame+1;
        if(frame<5 || frame>23)frame=5;
        else if(frame==13){g->drawn=3;if(sound)sound(user,6);}
        else if(frame==23){g->status=4;int16_t lf=g->left.flash,rf=g->right.flash;memset(&g->left,0,sizeof g->left);memset(&g->right,0,sizeof g->right);g->left.flash=lf;g->right.flash=rf;g->target=-1;g->head_yaw=g->head_pitch=g->torso_yaw=g->torso_pitch=0;frame=0;}
        g->left.frame=g->right.frame=(int16_t)frame;
    } else if(g->status==3) {
        undraw(&g->left,g,sound,user);undraw(&g->right,g,sound,user);
        if(g->left.frame==5 && g->right.frame==5){g->status=0;g->left.frame=g->right.frame=0;g->left.lock=g->right.lock=0;g->target=-1;}
        g->head_yaw=g->torso_yaw=(int16_t)((g->left.yaw+g->right.yaw)/4);
        g->head_pitch=g->torso_pitch=(int16_t)((g->left.pitch+g->right.pitch)/4);
    } else if(g->status==4) {
        tomb_pistols_aim(&g->left,g->target_yaw,g->target_pitch);tomb_pistols_aim(&g->right,g->target_yaw,g->target_pitch);
        if(g->left.lock || g->right.lock) {
            int n=g->left.lock && g->right.lock?4:2;
            g->head_yaw=g->torso_yaw=(int16_t)(((g->left.lock?g->left.yaw:0)+(g->right.lock?g->right.yaw:0))/n);
            g->head_pitch=g->torso_pitch=(int16_t)(((g->left.lock?g->left.pitch:0)+(g->right.lock?g->right.pitch:0))/n);
        }
        fire_arm(&g->right,g,action,fire,sound,user,shot_sound,recoil,flash);fire_arm(&g->left,g,action,fire,sound,user,shot_sound,recoil,flash);
    }
}
void tomb_pistols_tick(TombPistols *g,int toggle,int action,int alive,int water,TombGunFire fire,TombGunSound sound,void *user){dual_tick(g,toggle,action,alive,water,fire,sound,user,8,9,3);}
/* DOS weapon table c3a44: Magnums share pistol aim, recoil and flash timing. */
void tomb_magnums_tick(TombPistols *g,int toggle,int action,int alive,int water,TombGunFire fire,TombGunSound sound,void *user){dual_tick(g,toggle,action,alive,water,fire,sound,user,44,9,3);}
/* Original Uzi table: recoil 3, flash 2, sound 43. */
void tomb_uzis_tick(TombPistols *g,int toggle,int action,int alive,int water,TombGunFire fire,TombGunSound sound,void *user){dual_tick(g,toggle,action,alive,water,fire,sound,user,43,3,2);}
/* DOS DrawShotgun 2b200, UndrawShotgun 2b2a4 and AnimateShotgun 2b5c0.
   The fire callback emits the six pellets and returns whether any fired. */
void tomb_shotgun_tick(TombPistols *g,int toggle,int action,int alive,int water,TombGunFire fire,TombGunSound sound,void *user) {
    if(g->left.flash>0)--g->left.flash;if(g->right.flash>0)--g->right.flash;
    if(!alive)g->status=0;
    else if((water && g->status==4) || (!water && toggle)) {
        if(!g->status){g->status=2;g->left.frame=g->right.frame=0;}
        else if(g->status==4)g->status=3;
    }
    int frame=g->left.frame,active=g->status;
    if(g->status==2) {
        ++frame;
        if(frame<13 || frame>47)frame=13;
        else if(frame==23){g->drawn=3;if(sound)sound(user,6);}
        else if(frame==47){
            g->status=4;int16_t lf=g->left.flash,rf=g->right.flash;
            memset(&g->left,0,sizeof g->left);memset(&g->right,0,sizeof g->right);
            g->left.flash=lf;g->right.flash=rf;g->target=-1;
            g->head_yaw=g->head_pitch=g->torso_yaw=g->torso_pitch=0;frame=0;
        }
    } else if(g->status==3) {
        if(!frame)frame=80;
        else if(frame>0 && frame<13){if(++frame==13)frame=114;}
        else if(frame==47)frame=114;
        else if(frame>47 && frame<80){++frame;if(frame==60)frame=0;else if(frame==80)frame=114;}
        else if(frame>=114 && frame<127){if(++frame==127)frame=80;}
        else if(frame>=80 && frame<114){
            ++frame;
            if(frame==101){g->drawn=0;if(sound)sound(user,6);}
            else if(frame==114){frame=0;g->status=0;g->target=-1;g->left.lock=g->right.lock=0;}
        }
        g->torso_yaw-=g->torso_yaw/2;g->torso_pitch-=g->torso_pitch/2;
        g->head_yaw=g->head_pitch=0;
    } else if(g->status==4) {
        tomb_pistols_aim(&g->left,g->target_yaw,g->target_pitch);
        if(g->left.lock){g->torso_yaw=g->left.yaw/2;g->torso_pitch=g->left.pitch/2;g->head_yaw=g->head_pitch=0;}
        if(g->left.lock) {
            if(frame>=0 && frame<13){if(++frame==13)frame=47;}
            else if(frame==47){if(action){if(!fire || fire(user,g->left.yaw,g->left.pitch)){if(sound)sound(user,45);}++frame;}}
            else if(frame>47 && frame<80){++frame;if(frame==80)frame=47;else if(frame==57 && sound)sound(user,9);}
            else if(frame>=114 && frame<127){if(++frame==127)frame=0;}
        } else {
            if(!frame && action)frame=1;
            else if(frame>0 && frame<13){if(++frame==13)frame=action?47:114;}
            else if(frame==47){if(action){if(!fire || fire(user,g->left.yaw,g->left.pitch)){if(sound)sound(user,45);}++frame;}else frame=114;}
            else if(frame>47 && frame<80){++frame;if(frame==60)frame=0;else if(frame==80)frame=114;else if(frame==57 && sound)sound(user,9);}
            else if(frame>=114 && frame<127){if(++frame==127)frame=0;}
        }
    }
    if(active>=2 && active<=4)g->left.frame=g->right.frame=(int16_t)frame;
}
