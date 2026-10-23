/* DOS damping and collision: 0x25089, 0x26234, 0x263b8, 0x26618,
   0x26918 (wall hit), 0x26a28 (backward walk). */
#include "ground.h"
#include "fixed.h"
static int16_t relax(int16_t v,int amount) {
    return (int16_t)(v < -amount ? v+amount : v > amount ? v-amount : 0);
}
void tomb_ground_damping(TombMovement *m,TombActor *a) {
    m->lean=relax(m->lean,182); m->turn_rate=relax(m->turn_rate,364);
    a->yaw=tomb_word((int32_t)a->yaw+m->turn_rate);
}
static void fall(TombActor *a) {
    a->animation=34; a->frame=492; a->current=a->goal=3;
    a->fall_speed=0; a->flags|=8;
}
int tomb_ground_collision(int mode,TombActor *a,TombContact *c,TombGroundContext *ctx) {
    if(!a || !c || !ctx || !ctx->walk.query || !ctx->walk.slide || !ctx->walk.vault) return 0;
    TombWalkContext *w=&ctx->walk;
    /* DOS roll (45) and continuing roll (23): ceiling, slide, 200-unit drop. */
    if(mode==45 || mode==23) {
        a->fall_speed=0;a->flags&=(uint8_t)~8u;
        c->positive_limit=32512;c->negative_limit=-384;c->ceiling_limit=0;c->flags|=1;
        w->move_angle=c->facing=tomb_word(a->yaw+(mode==23?-32768:0));
        w->query(w->user,a,c,762);
        if(tomb_ceiling_response(a,c) || w->slide(w->user,a,c))return 1;
        if(c->floor>200) {
            if(mode==45)fall(a);
            else {a->animation=93;a->frame=1473;a->current=a->goal=29;a->fall_speed=0;a->flags|=8;}
        } else {tomb_contact_shift(a,c);a->y=tomb_long((int64_t)a->y+c->floor);}
        return 1;
    }
    if(mode==21 || mode==22) {
        a->fall_speed=0;a->flags&=(uint8_t)~8u;
        c->positive_limit=128;c->negative_limit=-128;c->ceiling_limit=0;c->flags|=3;
        w->move_angle=c->facing=tomb_word(a->yaw+(mode==21?16384:-16384));
        w->query(w->user,a,c,762);
        if(tomb_ceiling_response(a,c))return 1;
        if(tomb_wall_response(a,c)){a->animation=11;a->frame=185;a->current=a->goal=2;}
        if(!w->slide(w->user,a,c))a->y=tomb_long((int64_t)a->y+c->floor);
        return 1;
    }
    if(mode==0) return tomb_walk_collision(a,c,w);
    if(mode!=1 && mode!=2 && mode!=5 && mode!=6 && mode!=7 && mode!=12 && mode!=16 && mode!=20) return 0;
    if(mode!=1 && mode!=12) { a->fall_speed=0; a->flags&=(uint8_t)~8u; }
    c->positive_limit=(mode==1 || mode==5)?32512:384; c->negative_limit=-384; c->ceiling_limit=0;
    c->facing=(mode==5 || mode==16)?tomb_word((int32_t)a->yaw-32768):a->yaw;
    c->flags|=mode==1?1:3; w->move_angle=c->facing;
    w->query(w->user,a,c,762);
    if(mode==12) { tomb_contact_shift(a,c); return 1; }
    if(mode!=6 && mode!=7 && tomb_ceiling_response(a,c)) return 1;
    if(mode==5) {
        if(c->floor>200) {
            a->animation=93; a->frame=1473; a->current=a->goal=29;
            a->fall_speed=0; a->flags|=8; return 1;
        }
        if(tomb_wall_response(a,c)) { a->animation=11; a->frame=185; }
        a->y=tomb_long((int64_t)a->y+c->floor);
        return 1;
    }
    if(mode==16) {
        if(tomb_wall_response(a,c)) { a->animation=11; a->frame=185; }
        if(c->floor>128 && c->floor<384) {
            if(a->frame>=964 && a->frame<=993) { a->animation=62; a->frame=930; }
            else { a->animation=61; a->frame=899; }
        }
        if(!w->slide(w->user,a,c)) a->y=tomb_long((int64_t)a->y+c->floor);
        return 1;
    }
    if(mode==1) {
        if(w->vault(w->user,a,c)) return 1;
        if(tomb_wall_response(a,c)) {
            ctx->lean=0;
            if(ctx->front_type==0 && ctx->front_floor < -640) {
                a->current=12;
                if(a->frame>=0 && a->frame<=9) { a->animation=53; a->frame=800; return 1; }
                if(a->frame>=10 && a->frame<=21) { a->animation=54; a->frame=815; return 1; }
            }
            a->animation=11; a->frame=185;
        }
        if(c->floor>384) { fall(a); return 1; }
        if(c->floor>=-384 && c->floor< -128) {
            if(a->frame>=3 && a->frame<=14) { a->animation=56; a->frame=837; }
            else { a->animation=55; a->frame=830; }
        }
        if(!w->slide(w->user,a,c)) a->y=tomb_long((int64_t)a->y+(c->floor>=50?50:c->floor));
    } else {
        if(c->floor>100) { fall(a); return 1; }
        if(!w->slide(w->user,a,c)) {
            if(mode==2 || mode==20) tomb_contact_shift(a,c);
            a->y=tomb_long((int64_t)a->y+c->floor);
        }
    }
    return 1;
}

/* DOS surface-tread look block 0x29f90..0x2a057. */
void tomb_surface_look(TombMovement *m) {
    m->camera_mode=2;
    if((m->input&4) && m->head_yaw> -9100)m->head_yaw=tomb_word(m->head_yaw-546);
    else if((m->input&8) && m->head_yaw<9100)m->head_yaw=tomb_word(m->head_yaw+546);
    m->torso_yaw=m->head_yaw/2;
    if((m->input&1) && m->head_pitch> -7280)m->head_pitch=tomb_word(m->head_pitch-546);
    else if((m->input&2) && m->head_pitch<7280)m->head_pitch=tomb_word(m->head_pitch+546);
    m->torso_pitch=0;
}
/* DOS above-water release damping 0x24fe0..0x25089. */
void tomb_look_relax(TombMovement *m) {
    if(m->camera_mode==2)return;
    m->head_yaw=m->torso_yaw=(int16_t)(m->head_yaw> -364 && m->head_yaw<364?0:m->head_yaw-m->head_yaw/8);
    m->head_pitch=m->torso_pitch=(int16_t)(m->head_pitch> -364 && m->head_pitch<364?0:m->head_pitch-m->head_pitch/8);
}
