/* Recovered from the original DOS executable; see ledge-services.asm. */
#include "ledge.h"
#include "fixed.h"
static int32_t sub(int32_t a,int32_t b) { return tomb_long((int64_t)a-b); }
static int32_t add(int32_t a,int32_t b) { return tomb_long((int64_t)a+b); }
static int32_t magnitude(int32_t v) { return v<0?tomb_long(-(int64_t)v):v; }
static int snap(int16_t yaw,int16_t *out) {
    int angle=yaw;
    if(angle>=-6370 && angle<=6370) angle=0;
    else if(angle>=10014 && angle<=22754) angle=16384;
    else if(angle>=26397 || angle<=-26397) angle=-32768;
    else if(angle>=-22754 && angle<=-10014) angle=-16384;
    if((uint16_t)angle&0x3fff) return 0;
    *out=(int16_t)angle; return 1;
}
int tomb_ledge_grab(TombActor *a,const TombContact *c,const TombLedgeSamples *s,TombGrabContext *ctx,int forward) {
    if(!a || !c || !s || !ctx || !ctx->bounds_min_y || (forward && !ctx->swing_space)) return 0;
    if(c->type!=1 || !(ctx->input&64) || ctx->weapon_status!=0 ||
       magnitude(sub(s->left_floor,s->right_floor))>=60) return 0;
    if(s->front_ceiling>0 || s->ceiling>-384 || (forward && c->floor<200)) return 0;
    int32_t delta=sub(s->front_floor,ctx->bounds_min_y(ctx->user,a));
    int32_t next=add(delta,a->fall_speed);
    if((delta<0 && next<0) || (delta>0 && next>0)) return 0;
    int16_t yaw;
    if(!snap(a->yaw,&yaw)) return 0;
    if(forward) {
        int swing=ctx->swing_space(ctx->user,a,yaw);
        a->animation=swing?150:96; a->frame=swing?3974:1493;
    } else { a->animation=96; a->frame=1505; }
    a->current=a->goal=10;
    int16_t min_y=ctx->bounds_min_y(ctx->user,a);
    a->y=add(a->y,forward?delta:sub(s->front_floor,min_y));
    a->x=add(a->x,c->shift_x); a->z=add(a->z,c->shift_z);
    a->speed=a->fall_speed=0; a->yaw=yaw; a->flags&=(uint8_t)~8u;
    ctx->weapon_status=1; return 1;
}
/* Reach state 11, original control 0x25778 and collision 0x2686c. */
void tomb_reach_control(TombActor *a,int16_t *camera_angle) {
    *camera_angle=15470; if(a->fall_speed>131) a->goal=9;
}
int tomb_reach_collision(TombActor *a,TombContact *c,TombAirContext *ctx,TombContactAction grab) {
    if(!a || !c || !ctx || !ctx->query || !ctx->land || !ctx->sine_quarter || !grab) return 0;
    a->flags|=8;
    c->positive_limit=32512; c->negative_limit=0; c->ceiling_limit=192;
    c->facing=a->yaw; ctx->move_angle=a->yaw;
    ctx->query(ctx->user,a,c,762);
    if(grab(ctx->user,a,c)) return 1;
    tomb_fast_fall_deflect(a,c,ctx);
    if(a->fall_speed>0 && c->floor<=0) {
        a->goal=ctx->land(ctx->user,a,c)?8:2;
        a->fall_speed=0; a->y=add(a->y,c->floor); a->flags&=(uint8_t)~8u;
    }
    return 1;
}

/* Original hanging maintenance 0x2769c: two queries, hold or release. */
void tomb_hang_maintain(TombActor *a,TombContact *c,TombHangContext *ctx) {
    TombLedgeSamples *s=ctx->samples;
    c->positive_limit=32512; c->negative_limit=-32512; c->ceiling_limit=0; c->facing=ctx->move_angle;
    ctx->query(ctx->user,a,c,762);
    int edge_bad=s->front_floor<200;
    a->fall_speed=0; a->flags&=(uint8_t)~8u; ctx->move_angle=a->yaw;
    unsigned quadrant=((unsigned)(uint16_t)a->yaw+8192u)&65535u; quadrant>>=14;
    switch(quadrant) {
    case 0:a->z=add(a->z,2);break;
    case 1:a->x=add(a->x,2);break;
    case 2:a->z=sub(a->z,2);break;
    case 3:a->x=sub(a->x,2);break;
    }
    c->positive_limit=32512; c->negative_limit=-384; c->ceiling_limit=0; c->facing=ctx->move_angle;
    ctx->query(ctx->user,a,c,762);
    if(!(ctx->input&64) || ctx->health<=0) {
        a->current=a->goal=28; a->animation=28; a->frame=448;
        a->y=add(a->y,add(sub(s->front_floor,ctx->bounds_min_y(ctx->user,a)),2));
        a->x=add(a->x,c->shift_x); a->z=add(a->z,c->shift_z);
        a->speed=2; a->fall_speed=1; a->flags|=8; ctx->weapon_status=0; return;
    }
    if(magnitude(sub(s->left_floor,s->right_floor))>=60 || s->ceiling>=0 || c->type!=1 || edge_bad) {
        a->x=c->old_x; a->y=c->old_y; a->z=c->old_z;
        if(a->current==30 || a->current==31) { a->current=a->goal=10; a->animation=96; a->frame=1514; }
        return;
    }
    if(quadrant==0 || quadrant==2) a->z=add(a->z,c->shift_z);
    else a->x=add(a->x,c->shift_x);
    int32_t delta=sub(s->front_floor,ctx->bounds_min_y(ctx->user,a));
    if(delta>=-256 && delta<=256) a->y=add(a->y,delta);
}

/* Hanging control 0x25728, lateral control 0x25b00/0x25b38. */
void tomb_hang_control(TombActor *a,TombContact *c,TombHangControl *ctx) {
    c->flags&=(uint8_t)~0x18u; ctx->camera_angle=0; ctx->camera_elevation=-10920;
    if(a->current==10) {
        if(ctx->input&(4|1024)) a->goal=30;
        else if(ctx->input&(8|2048)) a->goal=31;
    } else if(a->current==30) { if(!(ctx->input&(4|1024))) a->goal=10; }
    else if(a->current==31) { if(!(ctx->input&(8|2048))) a->goal=10; }
}
/* Tail of 0x26800, following the original hanging maintenance call. */
void tomb_hang_pullup_goal(TombActor *a,const TombLedgeSamples *s,uint32_t input,int32_t left_ceiling,int32_t right_ceiling,int static_hit) {
    if(a->goal!=10 || !(input&1) || s->front_floor<=-850 || s->front_floor>=-650 ||
       sub(s->front_floor,s->front_ceiling)<0 || sub(s->left_floor,left_ceiling)<0 ||
       sub(s->right_floor,right_ceiling)<0 || static_hit) return;
    a->goal=(input&128)?54:19;
}
/* Wrappers 0x26e60/0x26e90 restore the lateral move angle after maintenance. */
void tomb_shimmy_collision(TombActor *a,TombContact *c,TombHangContext *ctx,int right) {
    int offset=right?16384:-16384;
    ctx->move_angle=tomb_word((int32_t)a->yaw+offset);
    tomb_hang_maintain(a,c,ctx);
    ctx->move_angle=tomb_word((int32_t)a->yaw+offset);
}
/* Pull-up collision 0x26b18 / 0x27398: sample only, no floor snapping. */
void tomb_pullup_collision(TombActor *a,TombContact *c,TombAirContext *ctx) {
    c->positive_limit=384; c->negative_limit=-384; c->ceiling_limit=0;
    ctx->move_angle=a->yaw; c->facing=a->yaw; c->flags|=3;
    ctx->query(ctx->user,a,c,762);
}

/* Standing vault decision, DOS 0x27b44. Animation advance is an ordered service. */
int tomb_vault(TombActor *a,TombContact *c,const TombLedgeSamples *s,TombVaultContext *v) {
    if(c->type!=1 || !(v->input&64) || v->weapon_status || magnitude(sub(s->left_floor,s->right_floor))>=60) return 0;
    int yaw=a->yaw;
    if(yaw>=-5460 && yaw<=5460) yaw=0;
    else if(yaw>=10924 && yaw<=21844) yaw=16384;
    else if(yaw>=27307 || yaw<=-27307) yaw=-32768;
    else if(yaw>=-21844 && yaw<=-10924) yaw=-16384;
    if((uint16_t)yaw&0x3fff) return 0;
    int32_t f=s->front_floor;
    if(f>=-896 && f<=-384) {
        if(sub(f,s->front_ceiling)<0 || sub(s->left_floor,v->left_ceiling)<0 || sub(s->right_floor,v->right_ceiling)<0) return 0;
        int low=f>=-640;
        a->animation=low?50:42; a->frame=low?759:614; a->current=19; a->goal=2;
        a->y=add(a->y,add(f,low?512:768)); v->weapon_status=1;
    } else if(f>=-1920 && f<=-896) {
        if(!v->advance) return 0;
        uint32_t n=(uint32_t)(-(f+800)*12),root=0;
        for(uint32_t bit=0x40000000;bit;bit>>=2) {
            uint32_t trial=root+bit; root>>=1;
            if(trial<=n) { n-=trial; root|=bit; }
        }
        a->animation=11; a->frame=185; a->current=2; a->goal=28;
        v->fall_override=tomb_word(-(int32_t)(root+3));
        if(!v->advance(v->user,a)) return 0;
    } else return 0;
    a->yaw=(int16_t)yaw; a->x=add(a->x,c->shift_x); a->y=add(a->y,c->shift_y); a->z=add(a->z,c->shift_z);
    c->shift_x=c->shift_y=c->shift_z=0;
    return 1;
}
