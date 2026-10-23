/* DOS 0x25ad8, 0x2793c, 0x26dc8 and damage tail at 0x28346. */
#include "air.h"
#include "fixed.h"
static int32_t sine(const int16_t *table,int32_t angle) {
    uint32_t a=(uint32_t)angle&65535; int negative=a>=32768;
    if(negative) a-=32768;
    if(a>16384) a=32768-a;
    return negative?-table[a>>4]:table[a>>4];
}
void tomb_back_fall_control(TombActor *a,uint32_t input,int16_t weapon_status) {
    if(a->fall_speed>131) a->goal=9;
    if((input&64) && weapon_status==0) a->goal=11;
}
void tomb_air_deflect(TombActor *a,TombContact *c,TombAirContext *ctx) {
    tomb_contact_shift(a,c);
    switch(c->type) {
    case 1: case 16:
        /* The DOS sequence adds 3 for negative inputs before shifting,
           reproducing signed division by four toward zero. */
        a->speed=(int16_t)tomb_asr(tomb_word((int32_t)a->speed+(a->speed<0?3:0)),2);
        a->goal=a->current=9; a->animation=32; a->frame=481;
        ctx->move_angle=tomb_word((int32_t)ctx->move_angle-32768);
        if(a->fall_speed<=0) a->fall_speed=1;
        break;
    case 2: a->yaw=tomb_word((int32_t)a->yaw+910); break;
    case 4: a->yaw=tomb_word((int32_t)a->yaw-910); break;
    case 8: if(a->fall_speed<=0) a->fall_speed=1; break;
    case 32:
        a->z=tomb_long((int64_t)a->z-tomb_asr(100*sine(ctx->sine_quarter,(int32_t)c->facing+16384),14));
        a->x=tomb_long((int64_t)a->x-tomb_asr(100*sine(ctx->sine_quarter,c->facing),14));
        a->speed=0; c->floor=0;
        if(a->fall_speed<=0) a->fall_speed=16;
        break;
    default: break;
    }
}
int tomb_back_fall_collision(TombActor *a,TombContact *c,TombAirContext *ctx) {
    if(!a || !c || !ctx || !ctx->query || !ctx->land || !ctx->sine_quarter) return 0;
    c->positive_limit=32512; c->negative_limit=-384; c->ceiling_limit=192;
    c->facing=tomb_word((int32_t)a->yaw-32768); ctx->move_angle=c->facing;
    ctx->query(ctx->user,a,c,762);
    tomb_air_deflect(a,c,ctx);
    if(c->floor<=0 && a->fall_speed>0) {
        a->goal=ctx->land(ctx->user,a,c)?8:2;
        a->fall_speed=0; a->y=tomb_long((int64_t)a->y+c->floor); a->flags&=(uint8_t)~8u;
    }
    return 1;
}
int tomb_landing_damage(int16_t *health,int16_t speed) {
    int excess=(int)speed-140;
    if(excess<=0) return 0;
    if(excess>14) *health=-1;
    else *health=tomb_word((int32_t)*health-excess*excess*1000/196);
    return *health<=0;
}

/* State 9: DOS 0x256e0, 0x27a74 and 0x2674c. */
int tomb_fast_fall_control(TombActor *a,TombAnimEvent sound,void *user) {
    if(!a) return 0;
    a->speed=(int16_t)((int32_t)a->speed*95/100);
    if(a->fall_speed>=154) {
        if(!sound) return 0;
        sound(user,5,30,a);
    }
    return 1;
}
void tomb_fast_fall_deflect(TombActor *a,TombContact *c,TombAirContext *ctx) {
    /* Fast falling differs from jump deflection at front and top-front only. */
    if(c->type==1 || c->type==16) {
        tomb_contact_shift(a,c);
        if(c->type==16 && a->fall_speed<=0) a->fall_speed=1;
    } else tomb_air_deflect(a,c,ctx);
}
int tomb_fast_fall_collision(TombActor *a,TombContact *c,TombAirContext *ctx) {
    if(!a || !c || !ctx || !ctx->query || !ctx->land || !ctx->sine_quarter || !ctx->sound) return 0;
    a->flags|=8;
    c->positive_limit=32512; c->negative_limit=-384; c->ceiling_limit=192;
    c->facing=ctx->move_angle;
    ctx->query(ctx->user,a,c,762);
    tomb_fast_fall_deflect(a,c,ctx);
    if(c->floor<=0) {
        if(ctx->land(ctx->user,a,c)) a->goal=8;
        else { a->goal=a->current=2; a->animation=24; a->frame=358; }
        ctx->sound(ctx->user,7,30,a);
        a->fall_speed=0; a->y=tomb_long((int64_t)a->y+c->floor); a->flags&=(uint8_t)~8u;
    }
    return 1;
}

/* State 3: DOS 0x25494 and 0x26484 (same state for forward jump/fall). */
void tomb_forward_air_control(TombActor *a,uint32_t input,int16_t weapon,int16_t *turn_rate) {
    if(a->goal==52 || a->goal==11) a->goal=3;
    if(a->goal!=8 && a->goal!=2) {
        if((input&64) && weapon==0) a->goal=11;
        if((input&128) && weapon==0) a->goal=52;
        if(a->fall_speed>131) a->goal=9;
    }
    if(input&4) {
        *turn_rate=tomb_word((int32_t)*turn_rate-409);
        if(*turn_rate< -546) *turn_rate=-546;
    } else if(input&8) {
        *turn_rate=tomb_word((int32_t)*turn_rate+409);
        if(*turn_rate>546) *turn_rate=546;
    }
}
int tomb_forward_air_collision(TombActor *a,TombContact *c,TombAirContext *ctx,uint32_t input) {
    if(!a || !c || !ctx || !ctx->query || !ctx->land || !ctx->advance || !ctx->sine_quarter) return 0;
    c->positive_limit=32512; c->negative_limit=-384; c->ceiling_limit=192;
    c->facing=a->yaw; ctx->move_angle=a->yaw;
    ctx->query(ctx->user,a,c,762);
    tomb_air_deflect(a,c,ctx);
    if(c->floor<=0 && a->fall_speed>0) {
        a->goal=ctx->land(ctx->user,a,c)?8:((input&1) && !(input&128)?1:2);
        a->fall_speed=0; a->speed=0; a->y=tomb_long((int64_t)a->y+c->floor); a->flags&=(uint8_t)~8u;
        if(!ctx->advance(ctx->user,a)) return 0;
    }
    return 1;
}

/* Standing compression: DOS 0x2579c / 0x26978. The probe returns a signed
   word, including the original no-floor sentinel. Direction priority matters. */
void tomb_jump_prepare_control(TombActor *a,uint32_t input,TombJumpFloor probe,void *user,int16_t *move) {
    const unsigned bits[]={1,4,8,2};
    const int offsets[]={0,-16384,16384,-32768},goals[]={3,27,26,25};
    for(int i=0;i<4;i++) if(input&bits[i]) {
        int16_t angle=tomb_word((int32_t)a->yaw+offsets[i]);
        if(probe(user,a,angle,256)>=-384) { a->goal=(int16_t)goals[i]; *move=angle; break; }
    }
    if(a->fall_speed>131) a->goal=9;
}
void tomb_jump_prepare_collision(TombActor *a,TombContact *c,TombJumpQuery query,void *user,int16_t move) {
    a->fall_speed=0; a->flags&=(uint8_t)~8u;
    c->positive_limit=32512; c->negative_limit=-32512; c->ceiling_limit=0; c->facing=move;
    if(query(user,a,c,762)>-100) {
        a->current=a->goal=2; a->animation=11; a->frame=185;
        a->speed=a->fall_speed=0; a->flags&=(uint8_t)~8u;
        a->x=c->old_x; a->y=c->old_y; a->z=c->old_z;
    }
}
/* Upward jump: DOS 0x25ac8 / 0x26d28. Original grab service is explicit. */
void tomb_up_jump_control(TombActor *a) { if(a->fall_speed>131) a->goal=9; }
int tomb_up_jump_collision(TombActor *a,TombContact *c,TombAirContext *ctx,TombContactAction grab) {
    if(!a || !c || !ctx || !ctx->query || !ctx->land || !ctx->sine_quarter || !grab) return 0;
    c->positive_limit=32512; c->negative_limit=-384; c->ceiling_limit=192;
    c->facing=a->yaw; ctx->move_angle=a->yaw;
    ctx->query(ctx->user,a,c,870);
    if(grab(ctx->user,a,c)) return 1;
    tomb_fast_fall_deflect(a,c,ctx);
    if(a->fall_speed>0 && c->floor<=0) {
        a->goal=ctx->land(ctx->user,a,c)?8:2;
        a->fall_speed=0; a->y=tomb_long((int64_t)a->y+c->floor); a->flags&=(uint8_t)~8u;
    }
    return 1;
}

/* States 25/26/27: DOS 0x25a88/0x25aa8/0x25ab8 and
   0x26cdc/0x26cf4/0x26d0c, sharing collision body 0x2745c. */
int tomb_directional_jump_control(TombActor *a,int16_t *camera_angle) {
    if(!a || !camera_angle || a->current<25 || a->current>27) return 0;
    if(a->current==25) *camera_angle=24570;
    if(a->fall_speed>131) a->goal=9;
    return 1;
}
int tomb_directional_jump_collision(TombActor *a,TombContact *c,TombAirContext *ctx) {
    if(!a || !c || !ctx || !ctx->query || !ctx->land || !ctx->sine_quarter || a->current<25 || a->current>27) return 0;
    int offset=a->current==25?-32768:a->current==26?16384:-16384;
    ctx->move_angle=tomb_word((int32_t)a->yaw+offset);
    c->positive_limit=32512; c->negative_limit=-384; c->ceiling_limit=192;
    c->facing=ctx->move_angle;
    ctx->query(ctx->user,a,c,762);
    tomb_air_deflect(a,c,ctx);
    if(a->fall_speed>0 && c->floor<=0) {
        a->goal=ctx->land(ctx->user,a,c)?8:2;
        a->fall_speed=0; a->y=tomb_long((int64_t)a->y+c->floor); a->flags&=(uint8_t)~8u;
    }
    return 1;
}

/* DOS 0x26014/0x26038 and 0x27280/0x27304. */
void tomb_swan_control(TombActor *a,TombContact *c){
    c->flags=(c->flags&~24u)|8;
    if(a->current==52){if(a->fall_speed>131)a->goal=53;}
    else a->speed=(int16_t)(a->speed*95/100);
}
int tomb_swan_collision(TombActor *a,TombContact *c,TombAirContext *ctx){
    int fast=a->current==53;
    c->positive_limit=32512;c->negative_limit=-384;c->ceiling_limit=192;
    ctx->move_angle=c->facing=a->yaw;ctx->query(ctx->user,a,c,762);tomb_air_deflect(a,c,ctx);
    if(c->floor<=0 && a->fall_speed>0){a->goal=fast && a->fall_speed>133?8:2;
        a->fall_speed=0;a->flags&=(uint8_t)~8u;a->y=tomb_long((int64_t)a->y+c->floor);
    }
    return 1;
}
