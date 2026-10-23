#include "playtest.h"
#include <math.h>
#include "fixed.h"
#include "object_contact.h"
#include "city.h"
#include "peru.h"
#include "folly.h"
#include "atlantean.h"
#include "hazards.h"
#include "enemies.h"
#include "combat.h"
#include "progression.h"
#include "follow_camera.h"
#include <stdlib.h>
#include <string.h>
static int supported(int state) { return state==42 || state==43 || state==52 || state==53 || state==36 || state==37 || state==38 || state==21 || state==22 || state==23 || state==45 || state==39 || state==40 || state==41 || state==24 || state==32 || state==11 || state==55 || state==0 || state==1 || state==2 || state==3 || state==5 || state==6 || state==7 || state==12 || state==16 || state==20 || state==9 || state==29 || state==15 || state==28 || state==10 || state==19 || state==54 || state==30 || state==31 || (state>=25 && state<=27); }
static void sound_request(TombPlaytest *p,int kind,int id,int environment,const TombActor *a) {
    /* The first-person listener may already be submerged on the entry tick. */
    if(p->shooter.mode==TOMB_MODE_FPS && kind==5 && id==33)environment=2;
    if(p->sound_count>=TOMB_SOUND_EVENTS){p->blocked=1;p->status="Sound event capacity exceeded";return;}
    const int32_t *source=NULL;
    if(a==&p->lara.actor)source=&a->x;
    else if(p->objects){
        for(size_t i=0;i<p->objects->count;i++)if(a==&p->objects->items[i].actor){source=&a->x;break;}
        if(!source && p->objects->hazards)for(int i=0;i<TOMB_DART_CAPACITY;i++)
            if(a==&p->objects->hazards->darts[i].item.actor){source=&a->x;break;}
    }
    /* Temporary impact/attachment actors are snapshots, never retained pointers. */
    p->sounds[p->sound_count++]=(TombSoundEvent){kind,id,environment,a->x,a->y,a->z,source};
}
static void event(void *user,int kind,int16_t id,TombActor *actor) {
    TombPlaytest *p=user;
    if(kind==12){p->spasm_ticks=5;p->spasm_effect=id;}
    else if(kind==5) {
        if(p->shooter.mode==TOMB_MODE_FPS && actor==&p->lara.actor &&
           (id==0 || ((id==34 || id==35 || id==36 || id==37 || id==52) && p->lara.water_status)))return;
        ++p->sound_events;sound_request(p,5,id,actor==&p->lara.actor?2:0,actor);}
    else if(kind==11){if(p->objects && p->objects->progress->music.current_track!=id){p->objects->progress->music.current_track=id;++p->objects->progress->music.audio_serial;}}
    else if(kind==9){++p->sound_events;sound_request(p,9,id,0,actor);}
    else if(kind==10)sound_request(p,10,id,0,actor);
    else if(kind==7) {++p->sound_stops;sound_request(p,7,id,2,actor);}
    else if(kind==6 && id==3) {++p->bubble_events;sound_request(p,6,3,1,actor);}
    else if(kind==6 && id==0) actor->yaw=tomb_word((int)actor->yaw-32768); /* DOS 0x1de74 */
    else if(kind==6 && id==2){actor->current=actor->goal=2;actor->animation=11;actor->frame=185;if(p->objects)p->objects->peru->scion_item=-1;}
    else if(kind==6 && id==4 && p->objects)p->objects->progress->complete=1;
    else if(kind==6 && id==12) p->animation.weapon_status=0;
    else if(kind==6 && id==1) sound_request(p,8,1,0,actor); /* Ground impact, consumed by the camera. */
    else { p->blocked=1; p->status="Animation effect not reconstructed"; }
}
static void air_sound(void *user,int kind,int16_t id,TombActor *actor) {
    TombPlaytest *p=user;
    if(kind==5 && id==30 && (p->gameplay_options&TOMB_INVINCIBLE))return;
    if(kind==5)++p->sound_events;else if(kind==7)++p->sound_stops;
    sound_request(p,kind,id,0,actor); /* DOS fast-fall scream: above-water environment. */
}
static void query(void *user,TombActor *a,TombContact *c,int32_t height) {
    TombPlaytest *p=user; TombStaticContact sc;
    TombActor probe=*a; int radius=100;
    if(height==400) { probe.y=tomb_long((int64_t)probe.y+200); radius=300; }
    else if(height==700) probe.y=tomb_long((int64_t)probe.y+700);
    if(!(p->objects?tomb_active_contact(p->level,&probe,c,height,radius,a->y,p->animation.sine_quarter,0,p->level->items,p->level->item_count,&p->terrain,&sc):tomb_world_contact(p->level,&probe,c,height,radius,a->y,p->animation.sine_quarter,0,&p->terrain,&sc))) {
        p->blocked=1; p->status="World query failed"; return;
    }
    p->static_hit=sc.hit;
    p->ledge=(TombLedgeSamples){p->terrain.samples[0].ceiling,p->terrain.samples[1].floor,p->terrain.samples[1].ceiling,p->terrain.samples[2].floor,p->terrain.samples[3].floor};
    p->collision.front_floor=p->terrain.samples[1].floor;
    p->collision.front_type=p->terrain.samples[1].type;
    if(p->terrain.object_references && !p->movement_only) { p->blocked=1; p->status="Dynamic object behaviour pending"; }
    p->slide.tilt_x=(int8_t)p->terrain.tilt_x; p->slide.tilt_z=(int8_t)p->terrain.tilt_z;
}
static int32_t prepare_query(void *user,TombActor *a,TombContact *c,int32_t height) {
    TombPlaytest *p=user; query(user,a,c,height); return p->terrain.samples[0].ceiling;
}
static int32_t probe_sine(const int16_t *table,int angle) {
    unsigned v=(unsigned)angle&65535; int sign=v>=32768?-1:1;
    v&=32767; if(v>16384) v=32768-v; return sign*table[v>>4];
}
static int16_t jump_floor(void *user,TombActor *a,int16_t angle,int32_t distance) {
    TombPlaytest *p=user; TombSectorRef ref; TombHeights h;
    int32_t x=tomb_long((int64_t)a->x+tomb_asr(distance*probe_sine(p->animation.sine_quarter,angle),14));
    int32_t z=tomb_long((int64_t)a->z+tomb_asr(distance*probe_sine(p->animation.sine_quarter,(int)angle+16384),14));
    if(!tomb_find_sector(p->level,x,tomb_long((int64_t)a->y-762),z,a->room,&ref) ||
       !tomb_item_heights(p->level,ref,x,a->y,z,0,p->level->items,p->level->item_count,&h) || (h.object_references && !p->movement_only)) {
        p->blocked=1; p->status="Jump floor service pending"; return -32512;
    }
    return h.floor==-32512?-32512:tomb_word(tomb_long((int64_t)h.floor-a->y));
}
static int16_t bounds_min_y(void *user,TombActor *a) {
    TombPlaytest *p=user; int16_t bounds[6];
    if(!tomb_visual_bounds(p->visual,a,bounds)) { p->blocked=1; p->status="Animation bounds unavailable"; return 0; }
    return bounds[2];
}
static int upward_grab(void *user,TombActor *a,TombContact *c) {
    TombPlaytest *p=user;
    TombGrabContext ctx={p->movement.input,p->animation.weapon_status,bounds_min_y,NULL,p};
    int caught=tomb_ledge_grab(a,c,&p->ledge,&ctx,0);
    p->animation.weapon_status=ctx.weapon_status;
    return caught;
}
static void hanging(TombPlaytest *p,TombActor *a,TombContact *c) {
    TombHangContext ctx={query,bounds_min_y,&p->ledge,p,p->movement.input,p->lara.health,p->animation.weapon_status,p->air.move_angle};
    int state=a->current;
    if(state==30 || state==31) tomb_shimmy_collision(a,c,&ctx,state==31);
    else {
        tomb_hang_maintain(a,c,&ctx);
        tomb_hang_pullup_goal(a,&p->ledge,p->movement.input,p->terrain.samples[2].ceiling,p->terrain.samples[3].ceiling,p->static_hit);
    }
    p->animation.weapon_status=ctx.weapon_status; p->air.move_angle=ctx.move_angle;
    if(!p->blocked) p->status=(a->current==10 || a->current==30 || a->current==31)?"Hanging: Ctrl hold, A/D move, W pull up, release Ctrl to drop":"Movement test";
}
typedef struct VaultAdvance { TombPlaytest *p; TombVaultContext *v; } VaultAdvance;
static int vault_advance(void *user,TombActor *a) {
    VaultAdvance *b=user; b->p->animation.fall_override=b->v->fall_override;
    return tomb_animate(a,&b->p->animation);
}
static int vault(void *user,TombActor *a,TombContact *c) {
    TombPlaytest *p=user;
    TombVaultContext v={p->movement.input,p->animation.weapon_status,p->animation.fall_override,
        p->terrain.samples[2].ceiling,p->terrain.samples[3].ceiling,vault_advance,NULL};
    VaultAdvance b={p,&v}; v.user=&b;
    int result=tomb_vault(a,c,&p->ledge,&v);
    p->animation.weapon_status=v.weapon_status;
    return result;
}
static int swing_space(void *user,TombActor *a,int16_t yaw) {
    TombPlaytest *p=user; TombSectorRef ref; TombHeights h;
    int32_t x=a->x,z=a->z;
    if(yaw==0) z+=256; else if(yaw==16384) x+=256; else if(yaw==-32768) z-=256; else x-=256;
    if(!tomb_find_sector(p->level,x,a->y,z,a->room,&ref) || !tomb_item_heights(p->level,ref,x,a->y,z,0,p->level->items,p->level->item_count,&h) || (h.object_references && !p->movement_only)) {
        p->blocked=1; p->status="Swing space unavailable"; return 0;
    }
    return h.floor!=-32512 && h.floor-a->y>0 && h.ceiling-a->y<-400;
}
static int16_t fps_reach_bounds(void *user,TombActor *a){return bounds_min_y(user,a)-(a->animation==95?128:0);}
static int forward_grab(void *user,TombActor *a,TombContact *c) {
    TombPlaytest *p=user; TombGrabContext ctx={p->movement.input,p->animation.weapon_status,p->shooter.mode==TOMB_MODE_FPS?fps_reach_bounds:bounds_min_y,swing_space,p};
    int caught=tomb_ledge_grab(a,c,&p->ledge,&ctx,1); p->animation.weapon_status=ctx.weapon_status; return caught;
}
static int slide(void *user,TombActor *a,TombContact *c) {
    TombPlaytest *p=user;
    p->slide.move_angle=p->collision.walk.move_angle;
    if(p->shooter.mode==TOMB_MODE_SHOOTER && a->current!=24 && a->current!=32 &&
       (abs(p->slide.tilt_x)>2 || abs(p->slide.tilt_z)>2))a->yaw=p->shooter.yaw;
    int result=tomb_slide_start(a,c,&p->slide);
    p->collision.walk.move_angle=p->slide.move_angle;
    return result;
}
/* Validate the whole ordinary presentation-only list before publishing it.
   DOS 0x17d2e consumes an extra flags/timer word for a camera action; its
   end bit, not the camera index word, terminates the list. Object actions remain guarded. This landing pass records requests; the
   successful gameplay tick dispatches tutorial/camera/completion behaviour. */
static int deferred_presentation(const TombLevel *l,size_t index,unsigned *last_track,
                                 unsigned *camera,uint16_t *camera_flags) {
    if(index>=l->floor_count || l->floor_data[index]!=0x8004 ||
       l->floor_count-index<3) return 0;
    size_t cursor=index+2;
    unsigned track=0,shot=0;uint16_t flags=0;
    while(cursor<l->floor_count) {
        uint16_t action=l->floor_data[cursor++];
        unsigned value=action&0x3ff,type=(action&0x3fff)>>10;
        if(type==8) {
            if(value<2 || value>=64)return 0;
            track=value;
        } else if(type==1) {
            if(value>=l->camera_count || cursor>=l->floor_count)return 0;
            action=l->floor_data[cursor++];shot=value+1;flags=action;
        } else if(type!=7)return 0;
        if(action&0x8000) { *last_track=track;*camera=shot;*camera_flags=flags;return 1; }
    }
    return 0;
}
static int landing(void *user,TombActor *a,TombContact *c) {
    TombPlaytest *p=user; TombSectorRef ref; TombHeights h; (void)c;
    /* Original landing service queries the current sector before dispatching
       its trigger. Structural landings and presentation-only triggers are supported;
       other trigger behaviour remains guarded. */
    if(!tomb_find_sector(p->level,a->x,a->y,a->z,a->room,&ref) ||
       !tomb_item_heights(p->level,ref,a->x,a->y,a->z,0,p->level->items,p->level->item_count,&h) || h.floor==-32512) {
        p->blocked=1; p->status="Landing geometry unavailable"; return 0;
    }
    unsigned track=0,camera=0;uint16_t camera_flags=0;
    if(!p->movement_only && (h.object_references || (h.trigger_index && !deferred_presentation(p->level,h.trigger_index,&track,&camera,&camera_flags)))) {
        p->blocked=1; p->status="Landing trigger service pending"; return 0;
    }
    if(track) p->deferred_cd_track=track;
    if(camera) {p->deferred_camera=camera;p->deferred_camera_flags=camera_flags;}
    p->lara.sector_floor=h.floor;
    return p->gameplay_options&TOMB_INVINCIBLE?0:tomb_landing_damage(&p->lara.health,a->fall_speed);
}
static int landing_advance(void *user,TombActor *a) {
    TombPlaytest *p=user;
    p->animation.move_angle=p->air.move_angle;
    return tomb_animate(a,&p->animation);
}
/* FPS contact keeps collision geometry, but has no wall-hit animation or rebound. */
static void fps_shift(TombPlaytest *p,TombActor *a,TombContact *c){
    int type=c->type,static_hit=p->static_hit;
    int32_t wanted_x=a->x,wanted_z=a->z;
    int quadrant=((unsigned)(uint16_t)c->facing+8192u)>>14 & 3;
    tomb_contact_shift(a,c);
    if(type!=1 || static_hit)return;
    /* DOS front contact also cancels tangent movement. Retry that component
       with a separate collision query so corners and pillars stay solid. */
    TombActor trial=*a;TombContact side=*c;
    side.old_x=a->x;side.old_y=a->y;side.old_z=a->z;
    if(quadrant&1){trial.z=wanted_z;side.facing=trial.z>=a->z?0:-32768;}
    else {trial.x=wanted_x;side.facing=trial.x>=a->x?16384:-16384;}
    query(p,&trial,&side,762);
    if(side.type!=1 && side.type!=16 && side.type!=32){tomb_contact_shift(&trial,&side);a->x=trial.x;a->z=trial.z;}
}
static int fps_collision(TombPlaytest *p,TombActor *a,TombContact *c){
    int airborne=(a->flags&8)!=0;
    c->positive_limit=32512;c->negative_limit=-384;c->ceiling_limit=airborne?192:0;
    c->facing=a->yaw;c->flags|=1;
    p->air.move_angle=p->collision.walk.move_angle=a->yaw;
    query(p,a,c,762);if(p->blocked)return 0;
    if(airborne && (p->movement.input&64)){
        TombActor grab=*a;
        /* Probe the original reaching bounds without entering its animation. */
        grab.animation=95;grab.frame=1492;
        int16_t normal=tomb_word(((uint16_t)a->yaw+8192u)&0xc000u);
        int facing=abs(tomb_word(p->shooter.yaw-normal))<=3640 && abs(tomb_word(a->yaw-normal))<=3640;
        /* Walking collision treats steep slopes as walls by replacing their
           heights with a sentinel. A hand catch needs the actual edge height,
           while retaining the original reach, clearance and hand-level tests. */
        TombContact reach=*c;reach.flags&=(uint8_t)~1u;
        if(facing)query(p,a,&reach,762);
        if(facing && !p->blocked && forward_grab(p,&grab,&reach)){
            *a=grab;a->y-=128;a->animation=96;a->frame=1514;a->current=a->goal=10;
            sound_request(p,5,54,2,a);return 1;
        }
        if(facing){query(p,a,c,762);if(p->blocked)return 0;}
    }
    if(!airborne && (p->movement.input&64) && vault(p,a,c))return 1;
    int type=c->type,ceiling_hit=c->shift_y>0;int32_t floor=c->floor;
    fps_shift(p,a,c);
    if(type==32){a->x=c->old_x;a->y=c->old_y;a->z=c->old_z;a->speed=0;if(a->fall_speed<0)a->fall_speed=0;return 1;}
    if(airborne){
        if((type==8 || type==16 || ceiling_hit) && a->fall_speed<0)a->fall_speed=0;
        if(floor<=0 && a->fall_speed>0){
            int health=p->lara.health;int fatal=landing(p,a,c);
            if(!fatal && p->lara.health<health)sound_request(p,5,27,2,a);
            a->y+=floor;a->fall_speed=a->speed=0;a->flags&=(uint8_t)~8u;
            sound_request(p,7,30,2,a);sound_request(p,5,4,2,a);
            a->animation=fatal?25:11;a->frame=p->level->animations[a->animation].first_frame;
            a->current=a->goal=fatal?8:2;
        }
    }else {
        if(type==8){a->x=c->old_x;a->y=c->old_y;a->z=c->old_z;a->speed=0;return 1;}
        if(floor>384){a->animation=34;a->frame=492;a->current=a->goal=3;a->fall_speed=0;a->flags|=8;}
        else if(!slide(p,a,c))a->y+=floor>=50?50:floor;
    }
    return !p->blocked;
}
static void water_room(void *user,TombActor *a,int32_t offset) {
    TombPlaytest *p=user; TombSectorRef ref;
    if(!tomb_find_sector(p->level,a->x,tomb_long((int64_t)a->y+offset),a->z,a->room,&ref)) {
        p->blocked=1; p->status="Water room update failed";
    } else a->room=(int16_t)ref.room;
}
static int16_t water_height(void *user,TombActor *a) {
    TombPlaytest *p=user; return tomb_water_height(p->level,a->x,a->z,a->room);
}
static void water_save(TombPlaytest *p) {
    p->lara.pitch=p->water.pitch; p->lara.lean=p->water.lean; p->movement.lean=p->water.lean;
    p->lara.health=p->water.health; p->lara.air=p->water.air; p->lara.water_status=p->water.status;
    p->animation.weapon_status=p->water.weapon_status; p->animation.move_angle=p->water.move_angle;
}
/* FPS mantles resolve a standing destination above the ledge before moving.
   In particular, querying at the hanging feet would select a thin ledge's underside. */
static int fps_begin_mantle(TombPlaytest *p,int water,const int32_t from[3]){
    TombActor *a=&p->lara.actor;TombSectorRef ref;TombHeights h;
    int32_t x=a->x+tomb_asr(256*probe_sine(p->animation.sine_quarter,a->yaw),14);
    int32_t z=a->z+tomb_asr(256*probe_sine(p->animation.sine_quarter,a->yaw+16384),14);
    int32_t y=a->y-(water?100:850);
    /* Rise into the ledge's room before stepping across its edge. Resolving
       the destination from the hanging room can follow a horizontal portal
       into the slope underneath an overhanging platform instead. */
    if(!tomb_find_sector(p->level,a->x,y,a->z,a->room,&ref) ||
       !tomb_find_sector(p->level,x,y,z,ref.room,&ref) ||
       !tomb_item_heights(p->level,ref,x,y,z,0,p->level->items,p->level->item_count,&h) ||
       h.floor==-32512 || h.ceiling==-32512 || h.floor-h.ceiling<762 ||
       abs(h.floor-(a->y-(water?0:735)))>256)return 0;
    p->fps.mantle_ticks=8;p->fps.mantle_water=water;p->fps.mantle_room=ref.room;
    for(int j=0;j<3;j++)p->fps.mantle_from[j]=from[j];
    /* Anchor is expressed as standing feet; retain the surface eye at entry. */
    if(water)p->fps.mantle_from[1]+=480;
    p->fps.mantle_to[0]=x;p->fps.mantle_to[1]=h.floor;p->fps.mantle_to[2]=z;
    a->animation=water?111:97;a->frame=p->level->animations[a->animation].first_frame;
    a->current=water?55:19;a->goal=2;a->speed=a->fall_speed=0;a->flags&=(uint8_t)~8u;
    p->lara.water_status=p->water.status=0;p->lara.pitch=p->water.pitch=0;
    return 1;
}
static int fps_mantle_tick(TombPlaytest *p){
    TombActor *a=&p->lara.actor;
    int step=9-p->fps.mantle_ticks;
    /* Vertical clearance first, then travel onto the platform. */
    for(int j=0;j<3;j++){
        int32_t from=p->fps.mantle_from[j],to=p->fps.mantle_to[j];
        int n=j==1?step:(step>4?(step-4)*2:0);
        int32_t value=from+(int32_t)((int64_t)(to-from)*n/8);
        if(j==0)a->x=value;else if(j==1)a->y=value;else a->z=value;
    }
    if(!--p->fps.mantle_ticks){
        a->room=(int16_t)p->fps.mantle_room;a->animation=11;a->frame=185;a->current=a->goal=2;
        p->animation.weapon_status=0;p->fps.mantle_water=0;
    }
    return 1;
}
static int water_tick(TombPlaytest *p,uint32_t input) {
    p->camera_request=tomb_camera_control(p->lara.actor.current,p->water.status,p->water.pitch);
    TombActor *a=&p->lara.actor; TombWater *w=&p->water;
    int surface=w->status==2;
    int fps_swim=p->shooter.mode==TOMB_MODE_FPS && w->health>0 && a->current!=55;
    int previous_speed=a->fall_speed;
    w->input=input&(1|2|4|8|16|64|128|1024|2048);
    if(w->health>=0) {
        if(surface) { w->air=tomb_word(w->air+10); if(w->air>1800) w->air=1800; }
        else if(p->swim_override==1 && !(p->level->rooms[a->room].flags&1))w->air=1800;
        else if(!(p->gameplay_options&TOMB_INVINCIBLE)){ w->air=tomb_word(w->air-1); if(w->air<0) { w->air=-1; w->health=tomb_word(w->health-5); } }
    }
    TombContact c={0}; c.old_x=a->x; c.old_y=a->y; c.old_z=a->z;
    c.positive_limit=32512; c.negative_limit=surface?-100:-400; c.ceiling_limit=surface?100:400;
    if(surface && a->current==33 && (input&512) && w->health>0) {
        TombMovement *m=&p->movement;m->input=input;m->head_yaw=p->lara.head_yaw;m->head_pitch=p->lara.head_pitch;
        tomb_surface_look(m);a->fall_speed=tomb_word(a->fall_speed-4);if(a->fall_speed<0)a->fall_speed=0;
        p->lara.head_yaw=m->head_yaw;p->lara.head_pitch=m->head_pitch;p->lara.torso_yaw=m->torso_yaw;p->lara.torso_pitch=m->torso_pitch;
        p->camera_request=(TombCameraRequest){1536,TOMB_CAMERA_LOOK,4,tomb_word(m->head_yaw+m->torso_yaw),m->head_pitch,p->lara.pitch,-1,0};
    } else if(!tomb_water_control(a,w,surface)) { p->blocked=1; p->status="Water control pending"; return 0; }
    if(fps_swim){
        int moving=surface?(input&1)!=0:(input&16)!=0;
        int target=moving?(surface?60:200):0;
        a->fall_speed=(int16_t)(previous_speed+(previous_speed<target?(target-previous_speed>8?8:target-previous_speed):-(previous_speed-target>8?8:previous_speed-target)));
        a->animation=surface?114:108;a->frame=p->level->animations[a->animation].first_frame;
        a->current=a->goal=surface?33:13;
    }
    tomb_water_damping(w,surface);
    tomb_water_current(p->objects,a,&c,w);
    if(!tomb_animate(a,&p->animation)) { p->blocked=1; p->status="Water animation unavailable"; return 0; }
    w->weapon_status=p->animation.weapon_status; /* Preserve animation end commands. */
    int32_t focus_x=a->x,focus_y=a->y,focus_z=a->z;
    tomb_water_motion(a,w,surface);
    if(p->shooter.mode==TOMB_MODE_FPS && p->shooter.mode_held){
        a->x=focus_x+(a->x-focus_x)/2;a->y=focus_y+(a->y-focus_y)/2;a->z=focus_z+(a->z-focus_z)/2;
    }
    if(a->current==13 || a->current==17 || a->current==18 || a->current==35) tomb_underwater_collision(a,&c,w);
    else if(a->current==33 || a->current==34 || (a->current>=47 && a->current<=49)) {
        w->move_angle=tomb_word(a->yaw+(a->current==47?-32768:a->current==48?-16384:a->current==49?16384:0));
        tomb_surface_collision(a,&c,&p->ledge,w);
    } else if(a->current==39 || a->current==40) {c.positive_limit=384;c.negative_limit=-384;c.ceiling_limit=0;c.flags|=3;w->move_angle=c.facing=a->yaw;query(p,a,&c,762);}
    else if(a->current==44) {w->health=-1;}
        else { p->blocked=1; p->status="Water state pending"; }
    water_room(p,a,surface?100:0); water_save(p);
    if(p->shooter.mode==TOMB_MODE_FPS && a->current==55){
        int32_t from[3]={c.old_x,c.old_y,c.old_z};
        if(fps_begin_mantle(p,1,from))sound_request(p,5,34,2,a);
    }
    if(!p->blocked) p->status=w->status==0?"Climbing out of water":w->status==1?"Underwater: J/Alt swim, W/S pitch, A/D turn":"Surface: W/S swim, J/Alt dive, Ctrl at edge to climb out";
    return !p->blocked;
}
static int gym_tick(TombPlaytest *p) {
    if(p->movement_only)return 1;
    TombSectorRef ref;TombHeights h;TombActor *a=&p->lara.actor;
    tomb_gym_begin_tick(&p->gym);
    if(tomb_find_sector(p->level,a->x,a->y,a->z,a->room,&ref) &&
       tomb_item_heights(p->level,ref,a->x,a->y,a->z,0,p->level->items,p->level->item_count,&h)) {
        if(!tomb_gym_trigger(&p->gym,p->level,h.trigger_index,a->current)) {
            p->status="Unsupported gym trigger";return 0;
        }
    }
    if(p->gym.complete)p->status="Gym complete! Press R to restart";
    return 1;
}
static void dead_contact(TombPlaytest *p){
    TombActor *a=&p->lara.actor;
        /* DOS dead collision uses a wider 400-unit radius and 384 step limits. */
        TombContact c={0};TombTerrain terrain;TombStaticContact statics;
        c.old_x=a->x;c.old_y=a->y;c.old_z=a->z;c.facing=a->yaw;
        c.positive_limit=384;c.negative_limit=-384;c.ceiling_limit=0;
        if(tomb_world_contact(p->level,a,&c,762,400,a->y,p->animation.sine_quarter,0,&terrain,&statics)) {
            a->x+=c.shift_x;a->y+=c.shift_y;a->z+=c.shift_z;
            if(c.floor!=-32512)a->y+=c.floor;
        }
        a->flags&=(uint8_t)~8u;a->speed=a->fall_speed=0;
}
static int death_tick(TombPlaytest *p) {
    TombActor *a=&p->lara.actor;
    p->blocked=0;p->lara.health=-1;p->lara.air=-1;p->dead_ticks++;
    p->gym.camera=0;
    if(p->gym.current_track){p->gym.current_track=0;p->gym.audio_serial++;}
    if(a->current==46){
        a->goal=46;p->movement.lean=0;p->door_hit_ticks=0;
        p->lara.head_yaw=p->lara.head_pitch=p->lara.torso_yaw=p->lara.torso_pitch=0;
        p->camera_request=tomb_camera_control(46,0,0);
        tomb_animate(a,&p->animation);p->status="Lara died";
    } else if(p->lara.water_status) {
        a->goal=44;p->water.health=-1;p->water.air=-1;p->water.input=0;
        if(a->current==44) {
            a->fall_speed=tomb_word(a->fall_speed-8);if(a->fall_speed<0)a->fall_speed=0;
            if(p->lara.pitch>=-364 && p->lara.pitch<=364)p->lara.pitch=0;
            else p->lara.pitch=tomb_word(p->lara.pitch+(p->lara.pitch<0?364:-364));
        }
        p->water.pitch=p->lara.pitch;
        TombContact c={0};c.old_x=a->x;c.old_y=a->y;c.old_z=a->z;
        c.positive_limit=32512;c.negative_limit=-400;c.ceiling_limit=400;
        tomb_water_current(p->objects,a,&c,&p->water);
        tomb_animate(a,&p->animation);tomb_water_motion(a,&p->water,0);
        int16_t height=water_height(p,a);if(height!=-32512 && height<a->y-100)a->y-=5;
        tomb_underwater_collision(a,&c,&p->water);water_room(p,a,0);water_save(p);
        p->status="Drowned - press R to restart";
    } else {
        a->goal=8;tomb_animate(a,&p->animation);
        dead_contact(p);
        p->status="Lara died";
    }
    return !p->blocked;
}
int tomb_playtest_init(TombPlaytest *p,const TombLevel *l,const int16_t *sine,const TombVisual *visual) {
    if(!p || !l || !sine || !visual || visual->level!=l) return 0;
    memset(p,0,sizeof *p); p->level=l; p->visual=visual;
    tomb_pistols_init(&p->pistols);tomb_inventory_init(&p->inventory);p->weapon_type=p->requested_weapon=1;
    for(size_t i=0;i<l->item_count;i++)if(l->items[i].object==30){tomb_inventory_confiscate(&p->inventory);break;}
    if(!tomb_lara_start(l,&p->lara)) return 0;
    tomb_hud_init(&p->hud,p->lara.health);
    p->movement.health=p->lara.health;
    p->animation=(TombAnimContext){l->animations,l->animation_count,l->changes,l->change_count,l->ranges,l->range_count,
        l->commands,l->command_count,sine,p->lara.move_angle,0,0,event,p};
    p->slide=(TombSlideContext){query,p,p->lara.move_angle,0,0,0};
    p->collision.walk=(TombWalkContext){query,vault,slide,p,p->lara.move_angle};
    p->air=(TombAirContext){query,landing,p,sine,p->lara.move_angle,air_sound,landing_advance};
    p->water=(TombWater){0,p->lara.health,0,0,p->lara.water_status,0,p->lara.move_angle,0,p->lara.air,query,water_room,water_height,p,sine};
    p->camera_request=tomb_camera_control(p->lara.actor.current,p->lara.water_status,p->lara.pitch);
    p->status="Movement test"; return 1;
}
static int playtest_tick(TombPlaytest *p,uint32_t input) {
    if(p->gym.complete){p->status="Gym complete! Press R to restart";return 1;}
    /* DOS above-water control (24f90) dispatches collision by animation state,
       even after lethal damage. An airborne hit must not invoke the grounded
       death collision and snap Lara to a floor far below her. */
    int state=p->lara.actor.current;
    /* Jump entry and landing frames can have gravity cleared while still
       needing their own state transition/collision. Do not replace their
       landing goal with death before they can reach the standing state. */
    int airborne_death=p->lara.health<=0 && !p->lara.water_status && state!=8 &&
        ((p->lara.actor.flags&8) || state==3 || state==9 || state==11 ||
         state==15 || (state>=25 && state<=29));
    if((p->lara.health<=0 && !airborne_death) || p->lara.actor.current==8)return death_tick(p);
    if(airborne_death){++p->dead_ticks;p->lara.air=-1;}
    if(p->shooter.mode==TOMB_MODE_FPS && p->fps.mantle_ticks)return fps_mantle_tick(p);
    TombLaraStart saved=p->lara;
    TombActor before=p->lara.actor, *a=&p->lara.actor;
    TombMovement *m=&p->movement;
    int16_t old_move=p->animation.move_angle;
    TombSlideContext saved_slide=p->slide;
    int16_t old_weapon=p->animation.weapon_status;
    unsigned old_cd_track=p->deferred_cd_track,old_camera=p->deferred_camera;
    uint16_t old_camera_flags=p->deferred_camera_flags;
    p->blocked=0; p->status="Movement test"; m->health=p->lara.health;
    TombWater saved_water=p->water;
    p->water.health=p->lara.health; p->water.pitch=p->lara.pitch; p->water.lean=p->lara.water_status?p->lara.lean:m->lean;
    p->water.status=p->lara.water_status; p->water.air=p->lara.air;
    p->water.weapon_status=p->animation.weapon_status; p->water.move_angle=p->animation.move_angle;
    int dive_entry=!p->water.status && (a->current==52 || a->current==53) && (p->level->rooms[a->room].flags&1);
    TombActor dive_before=*a;
    if(p->swim_override<0 && !(p->level->rooms[a->room].flags&1))p->swim_override=0;
    if(!p->swim_override)tomb_water_transition(a,&p->water,p->level->rooms[a->room].flags&1,water_height(p,a));
    if(dive_entry){
        a->fall_speed=dive_before.fall_speed;a->animation=dive_before.animation;a->frame=dive_before.frame;a->current=dive_before.current;a->goal=35;
        p->water.pitch=dive_before.current==52?-8190:-15470;
        if(!tomb_animate(a,&p->animation))return 0;a->fall_speed=tomb_word(a->fall_speed*2);
    }
    if(!saved.water_status && p->water.status==1) {
        sound_request(p,7,30,2,a); /* DOS 0x28435: stop fall scream. */
        sound_request(p,5,33,0,a); /* DOS splash 0x1dc50. */
    } else if(saved.water_status==1 && p->water.status==2) {
        sound_request(p,5,36,2,a); /* DOS 0x285c3: surfacing breath. */
    }
    if(saved.water_status || p->water.status) water_save(p);
    if(p->water.status) {
        if(water_tick(p,input)) {gym_tick(p);return 1;}
        p->sound_count=0;p->lara=saved; p->water=saved_water; p->animation.move_angle=old_move; p->animation.weapon_status=old_weapon; return 0;
    }
    p->lara.air=1800;
    p->camera_request=tomb_camera_control(a->current,0,p->lara.pitch);
    m->input=input & (1|2|4|8|16|64|128|512|1024|2048|4096);
    m->head_yaw=p->lara.head_yaw;m->head_pitch=p->lara.head_pitch;
    m->torso_yaw=p->lara.torso_yaw;m->torso_pitch=p->lara.torso_pitch;m->camera_mode=0;
    m->weapon_status=p->animation.weapon_status;
    /* Jump entry is implemented from standing and running. */
    if(a->current!=1 && a->current!=2 && a->current!=24 && a->current!=32) m->input&=~16u;
    m->current_state=a->current; m->goal_state=a->goal; m->animation=a->animation; m->frame=a->frame; m->item_flags=a->flags;
    TombContact c={0}; c.old_x=before.x; c.old_y=before.y; c.old_z=before.z; c.flags=0x18;
    if(a->current==52 || a->current==53){tomb_swan_control(a,&c);m->goal_state=a->goal;} else if(a->current==23 || a->current==45) { /* DOS null roll control 0x25cd8 */
    } else if(a->current==24 || a->current==32) {
        tomb_slide_control(a,m->input,&p->requested_camera_mode,&p->requested_camera_elevation);m->goal_state=a->goal;
    } else if(a->current==10 || a->current==30 || a->current==31) {
        TombHangControl control={m->input,0,0}; tomb_hang_control(a,&c,&control);
        p->requested_camera_angle=control.camera_angle; m->goal_state=a->goal;
    } else if(a->current>=36 && a->current<=38) {
        c.flags&=(uint8_t)~0x18u;if(a->current==38 && !(input&64))m->goal_state=2;
    } else if(a->current==19 || a->current==54 || a->current==55 || a->current==39 || a->current==40 || a->current==41 || a->current==42 || a->current==43) {
        /* Original shared null control 0x256d8 clears these collision flags. */
        c.flags&=(uint8_t)~0x18u;
    } else if(a->current==15) {
        tomb_jump_prepare_control(a,m->input,jump_floor,p,&p->animation.move_angle); m->goal_state=a->goal;
    } else if(a->current>=25 && a->current<=27) {
        tomb_directional_jump_control(a,&p->requested_camera_angle); m->goal_state=a->goal;
    } else if(a->current==28) {
        tomb_up_jump_control(a); m->goal_state=a->goal;
    } else if(a->current==11) {
        tomb_reach_control(a,&p->requested_camera_angle); m->goal_state=a->goal;
    } else if(a->current==3) {
        tomb_forward_air_control(a,p->shooter.mode==TOMB_MODE_FPS?m->input&~64u:m->input,p->animation.weapon_status,&m->turn_rate); m->goal_state=a->goal;
    } else if(a->current==9) {
        if(!tomb_fast_fall_control(a,air_sound,p)) { p->blocked=1; p->status="Fast fall sound service unavailable"; }
    } else if(a->current==29) {
        tomb_back_fall_control(a,p->shooter.mode==TOMB_MODE_FPS?m->input&~64u:m->input,p->animation.weapon_status); m->goal_state=a->goal;
    } else if(!supported(a->current) || !tomb_movement_control((enum TombControl)a->current,m)) {
        p->blocked=1; p->status="Control state pending"; return 0;
    }
    /* Shooter retreat retains backward collision without the DOS hop stop. */
    if(p->shooter.mode && a->current==5 && (m->input&2))m->goal_state=5;
    a->current=m->current_state; a->goal=m->goal_state; a->animation=m->animation; a->frame=m->frame; a->flags=m->item_flags;
    p->lara.head_yaw=m->head_yaw;p->lara.head_pitch=m->head_pitch;
    p->lara.torso_yaw=m->torso_yaw;p->lara.torso_pitch=m->torso_pitch;
    if(m->camera_mode==2)p->camera_request=(TombCameraRequest){1536,TOMB_CAMERA_LOOK,4,tomb_word(m->head_yaw+m->torso_yaw),tomb_word(m->head_pitch+m->torso_pitch),p->lara.pitch,-1,0};
    tomb_ground_damping(m,a);
    if(p->shooter.mode && !(a->flags&8) &&
       ((a->current==5 && a->animation==0) || (a->current==16 && a->animation==1))){
        const TombAnim *gait=p->level->animations+a->animation;
        int frame=a->frame-1;
        if(frame<gait->first_frame)frame=gait->last_frame;
        /* tomb_animate increments once and dispatches events at the resulting frame. */
        a->frame=(int16_t)(frame-1);
    }
    int32_t fps_x=a->x,fps_z=a->z;
    if(!tomb_animate(a,&p->animation)) { p->blocked=1; p->status="Animation service unavailable"; }
    if(p->shooter.mode==TOMB_MODE_FPS && a->current==19 && (a->animation==97 || a->animation==102)){
        for(int n=1;n<10 && a->current==19;n++)if(!tomb_animate(a,&p->animation)){p->blocked=1;break;}
    }
    if(p->shooter.mode==TOMB_MODE_FPS && !(a->flags&8) && a->current==1 && !(p->shooter.input&128)){
        a->x+=a->x-fps_x;a->z+=a->z-fps_z;a->speed=(int16_t)(a->speed*2);
    }
    if(p->shooter.mode==TOMB_MODE_FPS && p->shooter.mode_held && !(a->flags&8) && a->current==1){
        a->x=fps_x+(a->x-fps_x)/2;a->z=fps_z+(a->z-fps_z)/2;a->speed/=2;
    }
    if(p->objects && !p->blocked) {
        int hit=0;
        if(!tomb_hazards_contact(p->objects,a,&c,&p->animation,p->lara.pitch,m->lean,query,p) || !tomb_doors_collide(p->objects,a,&c,&p->animation,p->lara.pitch,m->lean,query,p,&hit)) {p->blocked=1;p->status="Door contact unavailable";}
        if(hit && !p->door_hit_ticks)sound_request(p,5,27,0,a);
        p->door_hit_ticks=hit?(p->door_hit_ticks<34?p->door_hit_ticks+1:34):0;
        p->door_hit_direction=hit-1;
    }
    p->collision.lean=m->lean;
    int16_t contact_animation=a->animation,contact_frame=a->frame;
    int health_before_contact=p->lara.health;
    int air_collision=a->current==52 || a->current==53 || a->current==42 || a->current==43 || (a->current>=36 && a->current<=38) || a->current==40 || a->current==41 || a->current==11 || a->current==3 || a->current==29 || a->current==9 || a->current==28 || a->current==10 || a->current==30 || a->current==31 || a->current==19 || a->current==54 || a->current==55 || a->current==15 || (a->current>=25 && a->current<=27);
    p->air.move_angle=p->animation.move_angle;
    if(!p->blocked) {
        int ok;
        if(a->current==8){dead_contact(p);ok=1;}
        else if(a->current>=36 && a->current<=43) {
            c.positive_limit=384;c.negative_limit=-384;c.ceiling_limit=0;c.flags|=3;
            p->air.move_angle=c.facing=a->yaw;query(p,a,&c,762);ok=1;
        }
        else if(p->shooter.mode==TOMB_MODE_FPS && (a->current==0 || a->current==1 || a->current==2 || a->current==3 || a->current==9 || a->current==11 || a->current==28 || a->current==29)){ok=fps_collision(p,a,&c);}
        else if(a->current==52 || a->current==53) {ok=tomb_swan_collision(a,&c,&p->air);if((p->gameplay_options&TOMB_INVINCIBLE) && a->goal==8)a->goal=2;}
        else if(a->current==24 || a->current==32) { tomb_slide_collision(a,&c,&p->slide);p->collision.walk.move_angle=p->slide.move_angle;ok=1; }
        else if(a->current==10 || a->current==30 || a->current==31) { hanging(p,a,&c); ok=1; }
        else if(a->current==19 || a->current==54 || a->current==55) { tomb_pullup_collision(a,&c,&p->air); ok=1; }
        else if(a->current==15) { tomb_jump_prepare_collision(a,&c,prepare_query,p,p->air.move_angle); ok=1; }
        else if(a->current==11) { ok=tomb_reach_collision(a,&c,&p->air,forward_grab); }
        else ok=(a->current>=25 && a->current<=27)?tomb_directional_jump_collision(a,&c,&p->air):a->current==28?tomb_up_jump_collision(a,&c,&p->air,upward_grab):a->current==3?tomb_forward_air_collision(a,&c,&p->air,m->input):
            a->current==9?tomb_fast_fall_collision(a,&c,&p->air):
            a->current==29?tomb_back_fall_collision(a,&c,&p->air):tomb_ground_collision(a->current,a,&c,&p->collision);
        if(!ok) { p->blocked=1; p->status="Collision state pending"; }
    }
    if(p->shooter.mode==TOMB_MODE_SHOOTER && !p->blocked){
        if((before.current==24 || before.current==32) && (input&16))
            p->tps_slope_jump=1;
        if(!(before.flags&8) && (before.current==0 || before.current==1) &&
           (a->flags&8) && a->current==3 && (input&64) && !p->animation.weapon_status && !p->pistols.drawn){
            /* Turn back to the edge just crossed, then fall into the existing
               reach/grab collision. No ledge-height teleport or forced catch. */
            /* Collision faces the ledge immediately; presentation completes the
               half-turn over eight ticks, with normal frame interpolation. */
            p->tps_drop_yaw_offset=32768;
            a->yaw=tomb_word(a->yaw+32768);a->animation=95;a->frame=1492;
            a->current=a->goal=11;a->speed=2;
            p->air.move_angle=p->collision.walk.move_angle=a->yaw;
        }
        if(!(a->flags&8) && (a->animation==53 || a->animation==54)){
            a->animation=11;a->frame=185;a->current=a->goal=2;a->speed=0;
        }
        if(a->animation==55 || a->animation==56){
            a->animation=0;a->frame=contact_animation==0?contact_frame:0;
        }
        if(!(a->flags&8) && p->lara.health>0 && p->lara.health<health_before_contact){
            a->animation=24;a->frame=358;a->current=a->goal=2;a->speed=0;
        }
    }
    if(p->shooter.mode==TOMB_MODE_FPS && a->current==10 && a->goal==19){
        int32_t from[3]={a->x,a->y,a->z};
        if(!fps_begin_mantle(p,0,from))a->goal=10;
    }
    if((!supported(a->current) && a->current!=8) || ((a->flags&8) && a->current!=11 && a->current!=3 && a->current!=29 && a->current!=9 && a->current!=28 && a->current!=52 && a->current!=53 && !(a->current>=25 && a->current<=27)) ) { p->blocked=1; p->status="Traversal pending"; }
    TombSectorRef ref;
    if(!p->blocked) {
        if(!tomb_find_sector(p->level,a->x,a->y-381,a->z,a->room,&ref)) { p->blocked=1; p->status="Room update failed"; }
        else a->room=(int16_t)ref.room;
    }
    if(p->blocked) {
        p->sound_count=0;
        int16_t yaw=a->yaw; p->lara=saved; a->yaw=yaw;
        if(!(before.flags&8) && before.current!=10 && before.current!=30 && before.current!=31 && before.current!=19 && before.current!=54 && before.current!=55) { a->current=a->goal=2; a->animation=11; a->frame=185; a->speed=0; }
        m->lean=0; p->animation.move_angle=old_move; p->slide=saved_slide;
        p->deferred_cd_track=old_cd_track;p->deferred_camera=old_camera;p->deferred_camera_flags=old_camera_flags; p->animation.weapon_status=old_weapon;
        return 0;
    }
    if(p->lara.health<=0){p->status="Lara died";return 1;}
    gym_tick(p);
    p->animation.move_angle=air_collision?p->air.move_angle:p->collision.walk.move_angle; m->lean=p->collision.lean;
    return 1;
}

void tomb_gameplay_start(TombPlaytest *p,uint32_t options){
    p->gameplay_options=options&(2047|TOMB_GUN_POWER_MASK|TOMB_3D_PICKUPS);
    for(int j=0;j<3;j++)if(options&(4u<<j))tomb_inventory_add(&p->inventory,85+j);
    if(p->objects)p->objects->invincible=(options&TOMB_INVINCIBLE)!=0;
    if(p->objects && !tomb_enemies_configure(p->objects,(options&TOMB_NO_ENEMIES)!=0)){p->blocked=1;p->status="Enemy removal service unavailable";}
}
/* Count physical jump edges before shooter controls map forward to swim. */
static uint32_t swim_toggle(TombPlaytest *p,uint32_t input){
    int held=(input&16)!=0,edge=held && !p->swim_jump_held;
    p->swim_jump_held=held;
    if(!(p->gameplay_options&TOMB_DOUBLE_JUMP_SWIM)){p->swim_tap_ticks=0;return input;}
    if(p->swim_tap_ticks>0)--p->swim_tap_ticks;
    if(!edge)return input;
    if(!p->swim_tap_ticks){p->swim_tap_ticks=10;return input;}
    p->swim_tap_ticks=0;
    TombActor *a=&p->lara.actor;
    if(p->lara.health<=0 || a->current==50 || a->current==51 || (p->objects && p->objects->peru->scion_item>=0))return input;
    int swimming=p->lara.water_status!=0;
    p->swim_override=swimming?-1:1;
    p->lara.water_status=p->water.status=swimming?0:1;
    p->lara.pitch=p->lara.lean=p->water.pitch=p->water.lean=p->movement.lean=0;
    p->lara.head_yaw=p->lara.head_pitch=p->lara.torso_yaw=p->lara.torso_pitch=0;
    p->animation.weapon_status=p->water.weapon_status=0;p->pistols.target=-1;
    a->speed=a->fall_speed=0;
    a->animation=swimming?34:108;a->frame=p->level->animations[a->animation].first_frame;
    a->current=a->goal=swimming?3:13;
    if(swimming)a->flags|=8;else a->flags&=(uint8_t)~8u;
    p->water.air=p->lara.air=1800;p->water.health=p->lara.health;
    return input&~16u;
}
/* Remote switch references can be floor-height callbacks only. Select the
   nearest active-room activation tile, not every list referencing the item. */
static size_t fps_trigger_index(const TombLevel *l,size_t at){
    if(!at)return 0;
    while(at<l->floor_count){
        unsigned word=l->floor_data[at],op=word&255;
        if(op==4)return at;
        if(op>=1 && op<=3)at+=2;else if(op==5)++at;else return 0;
        if(word&0x8000)return 0;
    }
    return 0;
}
int tomb_fps_activate(TombPlaytest *p,size_t id){
    TombObjects *w=p->objects;const TombLevel *l=p->level;
    if(id>=w->count)return 0;
    TombObject *o=w->items+id;size_t best=0;int64_t distance=INT64_MAX;
    for(size_t r=0;r<l->room_count;r++){
        int alternate=0;for(size_t k=0;k<l->room_count;k++)if(l->rooms[k].alternate==(int)r)alternate=1;
        if(alternate)continue;
        const TombRoom *room=l->rooms+r;
        for(int j=0;j<room->nx*room->nz;j++){
            size_t at=fps_trigger_index(l,room->sectors[j].floor_index);
            if(!at || at+2>=l->floor_count)continue;
            unsigned type=(l->floor_data[at]>>8)&63;
            if((type!=2 && type!=3) || (l->floor_data[at+2]&1023)!=id)continue;
            int64_t dx=room->x+j/room->nz*1024+512-o->actor.x;
            int64_t dz=room->z+j%room->nz*1024+512-o->actor.z;
            int64_t dy=room->sectors[j].floor*256-o->actor.y;
            int64_t d=dx*dx+dz*dz+dy*dy;
            if(d<distance){distance=d;best=at;}
        }
    }
    if(!best)return 1;
    int timer=l->floor_data[best+1]&255;
    if(!tomb_objects_trigger_activated(w,best))return 0;
    if(o->object==55 || o->object==56)o->timer=o->actor.goal==0?(int16_t)(timer==1?1:timer*30):0;
    else o->actor.flags=(o->actor.flags&~6u)|4;
    return 1;
}
static int fps_switches(TombPlaytest *p,int pressed){
    TombObjects *w=p->objects;const TombLevel *l=p->level;TombActor *a=&p->lara.actor;
    for(size_t i=0;i<w->count;i++){
        TombObject *o=w->items+i;
        /* FPS dispatches at the start of switch travel. Timed return travel
           needs the same matching trigger: switch actions XOR their masks.
           Consume expiry here before object control clears the timer, so a
           manually raised switch cannot dispatch its action twice. */
        if((o->object==55 || o->object==56) && o->active &&
           o->timer==-1 && o->actor.goal==0){
            o->actor.goal=1;o->timer=0;
            if(!tomb_fps_activate(p,i))return 0;
        }
        if((o->object==55 || o->object==56) && (o->actor.flags&6)==4){
            if(o->timer>0)o->actor.flags=(o->actor.flags&~6u)|2;
            else {o->actor.flags&=(uint8_t)~7u;o->active=0;}
        }
        if(((o->object>=118 && o->object<=125) || (o->object>=137 && o->object<=140)) &&
           (o->actor.flags&6)==2 && !tomb_fps_activate(p,i))return 0;
    }
    if(!pressed || p->lara.health<=0)return 1;
    int best=-1;int64_t distance=769*769;
    for(size_t i=0;i<w->count;i++){
        TombObject *o=w->items+i;
        if((o->object!=55 && o->object!=56) || (o->actor.flags&6) ||
           (o->actor.current!=0 && o->actor.current!=1))continue;
        if((o->object==56)!=(p->lara.water_status==1))continue;
        if(abs(a->y-o->actor.y)>(o->object==56?768:256))continue;
        if(abs(tomb_word(p->shooter.yaw-o->actor.yaw))>8192)continue;
        int64_t dx=(int64_t)o->actor.x-a->x,dz=(int64_t)o->actor.z-a->z,d=dx*dx+dz*dz;
        if(d>=distance)continue;
        TombCameraPoint eye={a->x,a->y-(p->lara.water_status?220:700),a->z,a->room};
        TombCameraPoint target={o->actor.x,o->actor.y-(o->object==56?0:512),o->actor.z,o->actor.room};
        if(d>0){double length=sqrt((double)d);target.x-=(int32_t)(dx*64/length);target.z-=(int32_t)(dz*64/length);}
        if(!tomb_dos_camera_los(l,&eye,&target,0))continue;
        best=(int)i;distance=d;
    }
    if(best>=0){
        TombObject *o=w->items+best;o->actor.goal=o->actor.current==1?0:1;
        /* Lara animations 63/64/129 own these sounds in Original mode.
           FPS skips those poses, so successful interaction supplies the event. */
        int activation_sound=o->object==56?61:o->actor.current==1?38:57;
        o->active=1;o->actor.flags=(o->actor.flags&~6u)|3;
        TombPose first,pose;
        if(!tomb_object_pose(p->visual,&o->actor,&first))return 0;
        for(int n=0;n<120;n++){
            if(!tomb_object_animate(o,&p->animation))return 0;
            if(!tomb_object_pose(p->visual,&o->actor,&pose))return 0;
            if(pose.count!=first.count || memcmp(pose.root,first.root,sizeof pose.root) ||
               memcmp(pose.rotation,first.rotation,pose.count*sizeof pose.rotation[0]))break;
            if((o->actor.flags&6)==4)break;
        }
        if(!tomb_fps_activate(p,(size_t)best))return 0;
        sound_request(p,5,activation_sound,2,&o->actor);
    }
    return 1;
}
int tomb_playtest_tick(TombPlaytest *p,uint32_t input) {
    int fps_action=p->shooter.mode==TOMB_MODE_FPS && (input&64) && !(p->shooter.input&64);
    input=swim_toggle(p,input);
    input=tomb_shooter_input(p,input);
    TombActor fps_before=p->lara.actor;int fps_wet=p->lara.water_status;
    int previous_hit=p->door_hit_ticks;
    if(!p->spasm_ticks && p->spasm_effect>0){p->door_hit_ticks=0;p->spasm_effect=0;}
    int protected_health=p->lara.health;
    int invincible=(p->gameplay_options&TOMB_INVINCIBLE) && protected_health>0;
    if(p->objects)p->objects->invincible=invincible;
    if(p->objects && p->objects->no_enemies!=!!(p->gameplay_options&TOMB_NO_ENEMIES) && !tomb_enemies_configure(p->objects,(p->gameplay_options&TOMB_NO_ENEMIES)!=0))return 0;
    int previous_camera_flags=p->camera_request.flags&3;
    p->last_input=input;
    if(p->objects && p->objects->progress->complete){p->sound_count=0;return 1;}
    tomb_hud_tick(&p->hud,p->lara.health);
    ++p->inventory.ticks;if(p->inventory.pickup_ticks>0)--p->inventory.pickup_ticks;
    p->sound_count=0;
    p->camera_request=tomb_camera_control(p->lara.actor.current,p->lara.water_status,p->lara.pitch);
    if(p->objects)tomb_objects_camera_begin(p->objects);
    if(p->objects && (!tomb_late_world_tick(p) || !tomb_midas_tick(p) || !tomb_folly_tick(p) || !tomb_atlantean_spawners_tick(p->objects,&p->animation,&p->lara.actor))){p->blocked=1;p->status="Folly object service unavailable";return 0;}
    TombCameraRequest object_camera=p->camera_request;
    int lava_camera=p->lara.health<=0 && object_camera.distance==3072 && object_camera.item_target>=0;
    if(p->objects && !tomb_peru_hazards(p->objects,&p->animation,&p->lara.actor,&p->lara.health)){p->blocked=1;p->status="Peru object service unavailable";return 0;}
    if(p->objects && !tomb_hazards_tick(p->objects,&p->animation,&p->lara.actor,&p->lara.health)) {p->blocked=1;p->status="Hazard service unavailable";return 0;}
    if(p->objects && !tomb_objects_tick(p->objects,&p->animation)) {
        p->blocked=1;p->status="Object animation unavailable";return 0;
    }
    if(p->objects && !tomb_enemies_tick(p->objects,&p->animation,&p->lara)) {
        p->blocked=1;p->status="Enemy service unavailable";return 0;
    }
    int ok;
    if(invincible)p->lara.health=(int16_t)protected_health;
    if(p->objects && (p->objects->peru->scion_item>=0 || p->lara.actor.current==50 || p->lara.actor.current==51)){
        if(p->lara.health<=0)++p->dead_ticks;
        ok=tomb_midas_sparkles(p) && tomb_animate(&p->lara.actor,&p->animation);p->status="Cinematic animation";
    }else if(p->shooter.mode==TOMB_MODE_VR){ok=gym_tick(p);if(p->lara.health<=0)++p->dead_ticks;}
    else ok=playtest_tick(p,input);
    if(p->objects && p->spasm_ticks>0 && p->lara.health>0){
        int index=p->spasm_effect-1;
        if(index>=0 && index<256){
            const TombHazardEffect *e=p->objects->hazards->effects+index;TombActor *a=&p->lara.actor;
            int angle=tomb_object_angle(e->z-a->z,e->x-a->x);
            p->door_hit_direction=((unsigned)(a->yaw-angle+32768+8192)&65535)>>14;
            if(!p->door_hit_ticks)p->door_hit_ticks=previous_hit;
            if(!p->door_hit_ticks)sound_request(p,5,27,0,a);
            if(p->door_hit_ticks<34)++p->door_hit_ticks;
        }
        --p->spasm_ticks;
    }
    uint32_t interaction=p->shooter.mode==TOMB_MODE_FPS?(input&~64u)|(p->shooter.input&64):input;
    unsigned pickups_before=p->inventory.pickups;
    int scion_before=p->objects?p->objects->peru->scion_item:-1;
    TombActor before_scion=p->lara.actor;
    if(ok && p->objects)ok=tomb_peru_lara(p,interaction) && tomb_folly_contact(p) && tomb_midas_contact(p,interaction);
    if(ok && p->objects && scion_before<0 && p->objects->peru->scion_item>=0 &&
       p->objects->items[p->objects->peru->scion_item].object==146){
        /* DOS BaddieCollision processes the Scion before its altar in the
           room list. Finish that contact after alignment, while the entry
           frame still allows pushing; subsequent cinematic frames do not.
           Keep scion_origin at the pre-push position for the authored camera. */
        TombContact c={0};c.old_x=before_scion.x;c.old_y=before_scion.y;c.old_z=before_scion.z;c.flags=8;
        ok=tomb_scion_altar_collide(p->objects,&p->lara.actor,&c,&p->animation,query,p);
    }
    if(ok && p->objects && p->lara.health>0) {
        ok=tomb_pickups_tick(p,input);
        if(ok)ok=tomb_city_interact(p,interaction);
        p->inventory.chosen=-1; /* DOS BaddieCollision tail 0x16277: one contact pass only. */
        if(ok)ok=p->shooter.mode==TOMB_MODE_FPS?fps_switches(p,fps_action):tomb_objects_interact(p->objects,&p->lara.actor,&p->animation,input,p->lara.pitch,p->movement.lean);
    }
    if(ok && p->shooter.mode==TOMB_MODE_FPS){
        TombActor *a=&p->lara.actor;
        /* Include both automatic pickups and scripted Scion collection. The
           pickup counter wraps at 256; feedback is audible underwater too. */
        unsigned collected=(p->inventory.pickups-pickups_before)&255u;
        for(unsigned i=0;i<collected;i++)sound_request(p,5,6,2,a);
        int block=p->lara.health>0 && a->current>=36 && a->current<=38;
        if(block && p->fps.block_view<8)++p->fps.block_view;
        else if(!block && p->fps.block_view>0)--p->fps.block_view;
        int wet=p->lara.water_status;
        if(!fps_wet && wet){
            int present=0;for(unsigned j=0;j<p->sound_count;j++)if(p->sounds[j].kind==5 && p->sounds[j].id==33)present=1;
            if(!present)sound_request(p,5,33,2,a);
        }
        /* FPS ground travel is silent; landing sounds have their own contact event. */
        int moving=p->lara.health>0 && !p->fps.mantle_ticks && wet;
        int dx=a->x-fps_before.x,dy=wet?a->y-fps_before.y:0,dz=a->z-fps_before.z;
        int distance=(int)sqrt((double)dx*dx+(double)dy*dy+(double)dz*dz);
        if(moving && distance>0 && distance<512){
            int first_surface_stroke=wet==2 && p->fps.sound_distance==0;
            p->fps.sound_distance+=distance;
            /* Surface animation 116 repeats over 40 ticks at 15 units/tick. */
            int stride=wet==2?600:900;
            if(first_surface_stroke || p->fps.sound_distance>=stride){
                p->fps.sound_distance%=stride;
                if(!p->fps.sound_distance)p->fps.sound_distance=1;
                sound_request(p,5,wet==2?52:35,2,a);
            }
        }else p->fps.sound_distance=0;
    }
    if(ok) {
        TombMovement *m=&p->movement;m->head_yaw=p->lara.head_yaw;m->head_pitch=p->lara.head_pitch;
        m->torso_yaw=p->lara.torso_yaw;m->torso_pitch=p->lara.torso_pitch;
        m->camera_mode=p->camera_request.flags==TOMB_CAMERA_LOOK?2:0;tomb_look_relax(m);
        p->lara.head_yaw=m->head_yaw;p->lara.head_pitch=m->head_pitch;p->lara.torso_yaw=m->torso_yaw;p->lara.torso_pitch=m->torso_pitch;
        ok=tomb_combat_tick(p,input);
        tomb_shooter_pose(p);
    }
    /* DOS land/surface/underwater control calls GunControl before TestTriggers.
       Combat and look replace even a heavy camera before RefreshCamera runs. */
    if(ok && p->objects && p->lara.health>0) {
        int camera_mode=p->camera_request.flags==TOMB_CAMERA_LOOK?2:p->camera_request.flags==TOMB_CAMERA_COMBAT?3:0;
        if(camera_mode)p->objects->camera.active=0;
        unsigned secret_before=p->objects->progress->secret_serial;
        p->objects->progress->lara_state=p->lara.actor.current;
        TombActor *a=&p->lara.actor;TombSectorRef ref;TombHeights h;
        if(ok && tomb_find_sector(p->level,a->x,a->y,a->z,a->room,&ref) && tomb_item_heights(p->level,ref,a->x,a->y,a->z,0,p->level->items,p->level->item_count,&h))
            {
            if(h.trigger_index && (p->level->floor_data[h.trigger_index]&255)==5 && h.floor==a->y)ok=tomb_lava_touch(p);
            unsigned trigger_type=h.trigger_index?(p->level->floor_data[h.trigger_index]>>8)&63:0;
            if(ok && !(p->shooter.mode==TOMB_MODE_FPS && (trigger_type==2 || trigger_type==3)))
                ok=tomb_objects_trigger_camera(p->objects,h.trigger_index,h.floor==a->y,p->animation.weapon_status,camera_mode);
        }
        if(p->objects->progress->secret_serial!=secret_before)sound_request(p,5,173,2,&p->lara.actor);
        if(!ok){p->blocked=1;p->status="Object service unavailable";}
    }
    if(ok && !p->shooter.mode && p->lara.health>0 && p->objects && !p->objects->camera.active && p->objects->camera.target>=0 && !(p->camera_request.flags&(TOMB_CAMERA_LOOK|TOMB_CAMERA_COMBAT))){
        TombObject *o=p->objects->items+p->objects->camera.target;TombActor target=o->actor;TombModel model;TombPose lp,tp;
        if(tomb_visual_model(p->visual,o->object,&model) && model.animation<p->level->animation_count){
            if(!tomb_object_supported(o->object)){target.animation=(int16_t)model.animation;target.frame=p->level->animations[model.animation].first_frame;}
            if(tomb_object_pose(p->visual,&p->lara.actor,&lp) && tomb_object_pose(p->visual,&target,&tp) && tomb_camera_interest(&p->lara.actor,lp.bounds,&target,tp.bounds,&p->lara.head_yaw,&p->lara.head_pitch)){
                p->lara.torso_yaw=p->lara.head_yaw;p->lara.torso_pitch=p->lara.head_pitch;o->actor.flags|=64;
                p->camera_request=(TombCameraRequest){1536,TOMB_CAMERA_LOOK,4,tomb_word(2*p->lara.head_yaw),tomb_word(2*p->lara.head_pitch),p->lara.pitch,-1,0};
            }
        }
    }
    /* DOS CalculateCamera 14a59 skips the flags reset while a heavy shot is
       active. Lara controls only write flags for selected states (push/pull
       set centre=1); returning to idle does not clear that framing. */
    if(p->objects && p->objects->camera.active==5 && !(p->camera_request.flags&3))
        p->camera_request.flags=previous_camera_flags;
    if(lava_camera)p->camera_request=object_camera;
    if(p->objects && p->lara.actor.current==46)for(size_t i=0;i<p->objects->count;i++){
        const TombObject *o=p->objects->items+i;
        if(o->object==34 && o->active && o->actor.current==11){p->camera_request.distance=2048;p->camera_request.flags=1;break;}
    }
    if(invincible && p->lara.health<protected_health)p->lara.health=(int16_t)protected_health;
    if(!ok || p->blocked)p->sound_count=0;
    return ok;
}

int tomb_city_fixture(TombPlaytest *p,TombObjects *w,const int16_t *s,int which){
 if(which<0 || which>7)return 0;
 if(which==7){
  tomb_objects_reset(w);if(!tomb_playtest_init(p,w->level,s,w->visual))return 0;p->objects=w;p->movement_only=1;
  p->lara.actor.x=27600;p->lara.actor.y=-1024;p->lara.actor.z=29184;p->lara.actor.room=61;p->lara.actor.yaw=-16384;p->animation.move_angle=-16384;return 1;
 }
 if(which>=3){
  tomb_objects_reset(w);if(!tomb_playtest_init(p,w->level,s,w->visual))return 0;p->objects=w;p->movement_only=1;
  int ids[]={129,110,137,118};
  for(size_t i=0;i<w->count;i++)if(w->items[i].object==ids[which-3]){
   TombActor *a=&p->lara.actor,*o=&w->items[i].actor;*a=(TombActor){o->x,o->y,o->z,2,2,11,185,0,0,0,o->room,32};
   if(which<5)a->z-=100;
   else {int32_t local[3]={0,0,400},pos[3];tomb_rotate_vector(o->yaw,0,0,local,0,s,pos);a->x+=pos[0];a->z+=pos[2];a->yaw=o->yaw;tomb_inventory_add(&p->inventory,which==5?129:110);}
   p->animation.move_angle=a->yaw;return 1;
  }
  return 0;
 }
 tomb_objects_reset(w);if(!tomb_playtest_init(p,w->level,s,w->visual))return 0;p->objects=w;p->movement_only=1;
 int remaining=which?which-1:0;
 for(size_t i=0;i<w->count;i++)if(w->items[i].object==(which?56:48)){
  if(remaining-- >0)continue;
  TombActor *a=&p->lara.actor,*o=&w->items[i].actor;a->x=o->x;a->y=o->y;a->z=o->z;a->room=o->room;
  if(!which){a->z-=612;a->yaw=0;}
  else {int32_t offset[3]={0,0,300},pos[3];tomb_rotate_vector(o->yaw,0,0,offset,0,s,pos);a->x+=pos[0];a->z+=pos[2];a->yaw=o->yaw;
   a->current=a->goal=13;a->animation=108;a->frame=1736;a->flags=32;p->lara.water_status=p->water.status=1;p->lara.pitch=p->water.pitch=0;
  }
  p->animation.move_angle=a->yaw;TombSectorRef ref;if(tomb_find_sector(w->level,a->x,a->y,a->z,a->room,&ref))a->room=(int16_t)ref.room;return 1;
 }
 return 0;
}
