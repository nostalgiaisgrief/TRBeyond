/* Pistol targeting and FireWeapon 0x2a8c0..0x2b130. */
#include "combat.h"
#include "playtest.h"
#include "enemies.h"
#include "object_contact.h"
#include "hazards.h"
#include "fixed.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
void tomb_gun_angles(int32_t x,int32_t y,int32_t z,int16_t out[2]) {
    out[0]=tomb_object_angle(z,x);
    while(x!=(int16_t)x || y!=(int16_t)y || z!=(int16_t)z){x=tomb_asr(x,2);y=tomb_asr(y,2);z=tomb_asr(z,2);}
    int32_t horizontal=(int32_t)sqrt((double)((int64_t)x*x+(int64_t)z*z));
    int16_t pitch=tomb_object_angle(horizontal,y);if((y>0 && pitch>0) || (y<0 && pitch<0))pitch=tomb_word(-(int)pitch);out[1]=pitch;
}
int tomb_gun_target_point(const TombVisual *v,const TombActor *a,const int16_t *s,TombCameraPoint *p) {
    TombPose pose;if(!tomb_object_pose(v,a,&pose))return 0;
    int x=(pose.bounds[0]+pose.bounds[1])/2,z=(pose.bounds[4]+pose.bounds[5])/2;
    int si=tomb_hazard_sine(s,a->yaw),co=tomb_hazard_sine(s,a->yaw+16384);
    *p=(TombCameraPoint){a->x+tomb_asr(x*co+z*si,14),a->y+pose.bounds[2]+(pose.bounds[3]-pose.bounds[2])/3,a->z+tomb_asr(z*co-x*si,14),a->room};return 1;
}
void tomb_sidearm_spread(uint32_t *random,int focus,int16_t yaw,int16_t pitch,int16_t out[2]){
    int scatter=focus?728:1456;
    out[1]=tomb_word(pitch+((int)tomb_control_random(random)-16384)*scatter/65536);
    out[0]=tomb_word(yaw+((int)tomb_control_random(random)-16384)*scatter/65536);
}
static int gun_ray(int scatter,const TombVisual *v,const TombObject *o,const TombCreature *c,TombCameraPoint origin,int16_t yaw,int16_t pitch,uint32_t *random,const int16_t *s,TombCameraPoint *hit) {
    pitch=tomb_word((int)pitch+((int)tomb_control_random(random)-16384)*scatter/65536);
    yaw=tomb_word((int)yaw+((int)tomb_control_random(random)-16384)*scatter/65536);
    int32_t p[3]={origin.x,origin.y,origin.z};TombSphere spheres[64];
    int count=o?tomb_object_spheres_joints(v,o->object,&o->actor,c->pitch,c->roll,c->head,c->neck,p,yaw,pitch,s,spheres):0;
    int distance=INT_MAX;
    for(int i=0;i<count;i++) {
        TombSphere b=spheres[i];if(abs(b.x)>=b.radius || abs(b.y)>=b.radius || b.z<=b.radius)continue;
        if((int64_t)b.x*b.x+(int64_t)b.y*b.y<=(int64_t)b.radius*b.radius && b.z-b.radius<distance)distance=b.z-b.radius;
    }
    int found=distance!=INT_MAX;if(!found)distance=16384;
    int sy=tomb_hazard_sine(s,yaw),cy=tomb_hazard_sine(s,yaw+16384),sp=tomb_hazard_sine(s,pitch),cp=tomb_hazard_sine(s,pitch+16384);
    int vx=tomb_asr(cp*sy,14),vy=-sp,vz=tomb_asr(cp*cy,14);
    *hit=(TombCameraPoint){origin.x+tomb_asr(tomb_long((int64_t)distance*vx),14),origin.y+tomb_asr(tomb_long((int64_t)distance*vy),14),origin.z+tomb_asr(tomb_long((int64_t)distance*vz),14),origin.room};return found;
}
int tomb_pistol_ray(const TombVisual *v,const TombObject *o,const TombCreature *c,TombCameraPoint origin,int16_t yaw,int16_t pitch,uint32_t *random,const int16_t *s,TombCameraPoint *hit) {
    return gun_ray(1456,v,o,c,origin,yaw,pitch,random,s,hit);
}
int tomb_shotgun_ray(const TombVisual *v,const TombObject *o,const TombCreature *c,TombCameraPoint origin,int16_t yaw,int16_t pitch,uint32_t *random,const int16_t *s,TombCameraPoint *hit) {
    return gun_ray(0,v,o,c,origin,yaw,pitch,random,s,hit);
}
void tomb_shotgun_spread(uint32_t *random,int16_t yaw,int16_t pitch,int16_t out[2]) {
    out[0]=tomb_word(yaw+((int)tomb_control_random(random)-16384)*3640/65536);
    out[1]=tomb_word(pitch+((int)tomb_control_random(random)-16384)*3640/65536);
}
static void target_info(TombPlaytest *p) {
    TombPistols *g=&p->pistols;TombObjects *w=p->objects;TombActor *a=&p->lara.actor;
    /* DOS KillItem (247f5) clears Lara.target when an item is removed.
       Reject stale removed targets here too, including restored older saves.
       Dead bodies still in the level retain the original targeting behavior. */
    if(g->target<0 || (size_t)g->target>=w->count || (!w->items[g->target].active && (w->items[g->target].actor.flags&6)==6)){g->target=-1;g->left.lock=g->right.lock=0;g->target_yaw=g->target_pitch=0;return;}
    TombCameraPoint target,origin={a->x,a->y-650,a->z,a->room};
    if(!tomb_gun_target_point(p->visual,&w->items[g->target].actor,p->animation.sine_quarter,&target)){g->target=-1;return;}
    int16_t angles[2];tomb_gun_angles(target.x-origin.x,target.y-origin.y,target.z-origin.z,angles);
    int yaw=tomb_word((int)angles[0]-a->yaw),pitch=tomb_word((int)angles[1]-p->lara.pitch);
    if(tomb_dos_camera_los(p->level,&origin,&target,0)) {
        if(yaw>=-10920 && yaw<=10920 && pitch>=-(p->weapon_type==4?10010:10920) && pitch<=(p->weapon_type==4?10010:10920))g->left.lock=g->right.lock=1;
        else if(p->weapon_type==4){if(yaw< -14560 || yaw>14560 || pitch< -11830 || pitch>11830)g->left.lock=g->right.lock=0;}
        else {if(yaw< -30940 || yaw>10920 || pitch< -14560 || pitch>14560)g->left.lock=0;if(yaw< -10920 || yaw>30940 || pitch< -14560 || pitch>14560)g->right.lock=0;}
    } else g->left.lock=g->right.lock=0;
    g->target_yaw=(int16_t)yaw;g->target_pitch=(int16_t)pitch;
}
static void acquire(TombPlaytest *p) {
    TombPistols *g=&p->pistols;TombEnemies *e=p->objects->enemies;TombActor *a=&p->lara.actor;TombCameraPoint origin={a->x,a->y-650,a->z,a->room};
    int best=32767;g->target=-1;
    for(int id=e->head;id>=0;id=e->next[id]) {
        const TombObject *o=p->objects->items+id;if(e->items[id].health<=0)continue;
        int64_t dx=(int64_t)o->actor.x-origin.x,dy=(int64_t)o->actor.y-origin.y,dz=(int64_t)o->actor.z-origin.z;
        if(llabs(dx)>8192 || llabs(dy)>8192 || llabs(dz)>8192 || dx*dx+dy*dy+dz*dz>=8192*8192)continue;
        TombCameraPoint t;if(!tomb_gun_target_point(p->visual,&o->actor,p->animation.sine_quarter,&t) || !tomb_dos_camera_los(p->level,&origin,&t,0))continue;
        int16_t angles[2];tomb_gun_angles(t.x-origin.x,t.y-origin.y,t.z-origin.z,angles);
        int yaw=tomb_word((int)angles[0]-a->yaw-g->torso_yaw),pitch=tomb_word((int)angles[1]-p->lara.pitch-g->torso_pitch);
        if(yaw< -10920 || yaw>10920 || pitch< -(p->weapon_type==4?10010:10920) || pitch>(p->weapon_type==4?10010:10920) || abs(yaw)>=best)continue;
        best=abs(yaw);g->target=id;
    }
    target_info(p);
}
static void sound(void *user,int id) {
    TombPlaytest *p=user;if(p->sound_count>=TOMB_SOUND_EVENTS){p->blocked=1;return;}
    TombActor *a=&p->lara.actor;p->sounds[p->sound_count++]=(TombSoundEvent){5,id,0,a->x,a->y,a->z,&a->x};++p->sound_events;
}
void tomb_combat_target(TombPlaytest *p,int action) {
    if(p->shooter.mode){tomb_shooter_target(p);return;}
    if(action)target_info(p);else p->pistols.target=-1;
    if(p->pistols.target<0)acquire(p);
}
static int fire(void *user,int16_t yaw,int16_t pitch) {
    TombPlaytest *p=user;TombPistols *g=&p->pistols;TombObjects *w=p->objects;TombEnemies *e=w->enemies;TombActor *a=&p->lara.actor;
    if((p->weapon_type==2 || p->weapon_type==3) && !(p->gameplay_options&TOMB_INFINITE_AMMO)){
        if(p->inventory.ammo[p->weapon_type]<=0){p->inventory.ammo[p->weapon_type]=0;p->requested_weapon=1;sound(p,48);return 0;}
        --p->inventory.ammo[p->weapon_type];
    }
    TombObject *o=g->target>=0?w->items+g->target:NULL;TombCreature *c=g->target>=0?e->items+g->target:NULL;
    TombCameraPoint origin={a->x,a->y-(p->weapon_type==4?500:650),a->z,a->room},hit;
    uint32_t shot_seed=e->random;
    int got=gun_ray(p->weapon_type==4?0:1456,p->visual,o,c,origin,tomb_word((int)yaw+a->yaw),pitch,&e->random,p->animation.sine_quarter,&hit);
    if(p->shooter.mode){
        /* Each gun traces to the crosshair from its side of Lara, so cover
           between her and the camera's target still blocks the shot. */
        TombCameraPoint shoulder=origin;
        int32_t muzzle[3];int left=p->weapon_type!=4 && (!g->right.lock || g->right.frame!=4);
        if(tomb_lara_gun_point(p,left,muzzle)){origin.x=muzzle[0];origin.y=muzzle[1];origin.z=muzzle[2];}
        tomb_shooter_world_ray(p,shoulder,&origin);
        if(p->shooter.mode==TOMB_MODE_FPS){
            origin=p->shooter.eye;
        }
        int16_t angles[2];tomb_gun_angles(p->shooter.target.x-origin.x,p->shooter.target.y-origin.y,p->shooter.target.z-origin.z,angles);
        int focus=p->shooter.mode==TOMB_MODE_FPS && p->shooter.mode_held;
        if(p->weapon_type==4){
            int divisor=focus?2:1;
            angles[0]=tomb_word(angles[0]+tomb_word(yaw-g->left.yaw)/divisor);
            angles[1]=tomb_word(angles[1]+tomb_word(pitch-g->left.pitch)/divisor);
        }else {
            if(!(focus && !p->fps_shot_held))tomb_sidearm_spread(&shot_seed,focus,angles[0],angles[1],angles);
        }
        int id=tomb_shooter_ray(p,origin,angles[0],angles[1],&hit);got=id>=0;o=got?w->items+id:NULL;c=got?e->items+id:NULL;
    }
    if(got){int damage=tomb_gun_damage(p->gameplay_options,p->weapon_type,c->health);++g->hits;if(c->health>0 && c->health<=damage)++g->kills;c->health=tomb_word(c->health-damage);o->actor.flags|=16;}
    else if(!p->shooter.mode)tomb_dos_camera_los(p->level,&origin,&hit,0);
    TombActor point={0};point.x=hit.x;point.y=hit.y;point.z=hit.z;point.room=got?o->actor.room:hit.room;
    int effect=tomb_effect_spawn(w,got?158:164,&point,got?o->actor.speed:0,got?o->actor.yaw:0);
    if(!got && effect>=0) {
        w->hazards->effects[effect].counter=4;
        if(p->sound_count<TOMB_SOUND_EVENTS){p->sounds[p->sound_count++]=(TombSoundEvent){5,10,0,hit.x,hit.y,hit.z,NULL};++p->sound_events;}
        else p->blocked=1;
    }
    return got?1:-1;
}
static int fire_shotgun(void *user,int16_t yaw,int16_t pitch) {
    TombPlaytest *p=user;uint32_t *random=&p->objects->enemies->random;int fired=0;
    for(int pellet=0;pellet<6;pellet++) {
        int16_t angles[2];tomb_shotgun_spread(random,yaw,pitch,angles);
        if(!(p->gameplay_options&TOMB_INFINITE_AMMO) && p->inventory.ammo[1]<=0){p->inventory.ammo[1]=0;p->requested_weapon=1;sound(p,48);continue;}
        if(!(p->gameplay_options&TOMB_INFINITE_AMMO))--p->inventory.ammo[1];++p->pistols.shots;fire(p,angles[0],angles[1]);fired=1;
    }
    return fired;
}
/* Tracked hands fire independently; this shares damage/effects, never the
   desktop arm controller or its animation cadence. */
int tomb_vr_fire(TombPlaytest *p,int weapon,TombCameraPoint origin,int16_t yaw,int16_t pitch){
    if(!p->objects || !p->objects->enemies || p->lara.health<=0 || weapon<1 || weapon>4)return 0;
    int slot=weapon==4?1:weapon==2?2:weapon==3?3:0;
    if(!p->inventory.counts[slot])return 0;
    int pellets=weapon==4?6:1;
    if(slot && !(p->gameplay_options&TOMB_INFINITE_AMMO)){
        if(p->inventory.ammo[slot]<pellets){sound(p,48);return 0;}
        p->inventory.ammo[slot]-=pellets;
    }
    for(int i=0;i<pellets;i++){
        int16_t angles[2]={yaw,pitch};if(weapon==4)tomb_shotgun_spread(&p->objects->enemies->random,yaw,pitch,angles);
        TombCameraPoint hit;int id=tomb_shooter_ray(p,origin,angles[0],angles[1],&hit);++p->pistols.shots;
        if(id>=0){TombCreature *c=p->objects->enemies->items+id;int damage=tomb_gun_damage(p->gameplay_options,weapon,c->health);
            if(c->health>0 && c->health<=damage)++p->pistols.kills;c->health=tomb_word(c->health-damage);p->objects->items[id].actor.flags|=16;++p->pistols.hits;}
        TombActor point={0};point.x=hit.x;point.y=hit.y;point.z=hit.z;point.room=hit.room;
        int effect=tomb_effect_spawn(p->objects,id>=0?158:164,&point,0,0);
        if(id<0 && effect>=0)p->objects->hazards->effects[effect].counter=4;
    }
    sound(p,weapon==4?45:weapon==2?44:weapon==3?43:8);return 1;
}
static int fps_combat(TombPlaytest *p){
    TombPistols *g=&p->pistols;TombShooter *s=&p->shooter;
    if(!(s->input&TOMB_INPUT_SHOOT))p->fps_shot_held=0;
    if(!tomb_fps_ready(p)){g->drawn=0;g->status=0;return 1;}
    int slot=p->requested_weapon==4?1:p->requested_weapon==2?2:p->requested_weapon==3?3:0;
    if(!p->inventory.counts[slot])p->requested_weapon=1;
    if(p->weapon_type!=p->requested_weapon){p->weapon_type=p->requested_weapon;s->fire_delay=0;s->next_hand=0;}
    p->sidearm_type=p->weapon_type==4?1:p->weapon_type;
    g->status=4;g->drawn=(s->dual || p->weapon_type==4)?3:2;
    tomb_shooter_target(p);g->left.lock=g->right.lock=1;
    g->left.yaw=g->right.yaw=tomb_word(s->yaw-p->lara.actor.yaw);
    g->left.pitch=g->right.pitch=s->pitch;
    if(g->left.flash)--g->left.flash;if(g->right.flash)--g->right.flash;
    if(s->fire_delay>0)--s->fire_delay;
    if(p->weapon_type==4){int f=g->left.frame;if(f<47 || f>=79)f=47;else if(f>47)++f;g->left.frame=g->right.frame=(int16_t)f;if(f==57)sound(p,9);}
    else {TombGunArm *arms[2]={&g->right,&g->left};for(int j=0;j<2;j++){int f=arms[j]->frame;arms[j]->frame=(int16_t)(f>=24 && f+1<24+(p->weapon_type==3?3:9)?f+1:4);}}
    if(!(s->input&TOMB_INPUT_SHOOT)){s->next_hand=0;return 1;}
    if(!s->fire_delay){
        if(p->weapon_type==4){if(fire_shotgun(p,g->left.yaw,g->left.pitch)){sound(p,45);g->right.flash=3;g->left.frame=g->right.frame=48;}s->fire_delay=33;}
        else {
            TombGunArm *arm=s->next_hand?&g->left:&g->right;
            if(arm->frame!=4)return 1;
            if(fire(p,arm->yaw,arm->pitch)){p->fps_shot_held=1;++g->shots;arm->frame=24;arm->flash=p->weapon_type==3?2:3;sound(p,p->weapon_type==2?44:p->weapon_type==3?43:8);}
            s->fire_delay=p->weapon_type==3?4:10;if(s->dual)s->fire_delay/=2;
            s->next_hand=s->dual?!s->next_hand:0;
        }
    }
    return !p->blocked;
}
int tomb_combat_tick(TombPlaytest *p,uint32_t input) {
    if(p->shooter.mode==TOMB_MODE_VR){p->pistols.target=-1;p->pistols.status=p->pistols.drawn=p->animation.weapon_status=0;return 1;}
    if(!p->objects || !p->objects->enemies)return 1;
    if(p->shooter.mode==TOMB_MODE_FPS)return fps_combat(p);
    TombPistols *g=&p->pistols;g->status=p->animation.weapon_status;
    int toggle=(input&32)!=0;
    if(p->shooter.mode){
        toggle=(p->shooter.aim_held && !g->status) || (!p->shooter.aim_held && g->status==4);
        input=(input&~64u)|((p->shooter.aim_held && (p->shooter.input&64))?64u:0);
    }
    int wanted=p->requested_weapon==4?1:p->requested_weapon==2?2:p->requested_weapon==3?3:0;
    if(!p->inventory.counts[wanted] && !g->status){g->target=-1;g->left.lock=g->right.lock=0;return 1;}
    if(p->lara.health>0 && !p->lara.water_status && p->weapon_type!=p->requested_weapon) {
        if(!g->status){
            p->weapon_type=p->requested_weapon;
            memset(&g->left,0,sizeof g->left);memset(&g->right,0,sizeof g->right);g->target=-1;g->drawn=0;toggle=!p->shooter.mode || p->shooter.aim_held;
        } else if(g->status==4)toggle=1;
    }
    int looking=p->camera_request.flags==TOMB_CAMERA_LOOK;
    g->head_yaw=p->lara.head_yaw;g->head_pitch=p->lara.head_pitch;
    g->torso_yaw=p->lara.torso_yaw;g->torso_pitch=p->lara.torso_pitch;
    if(g->status==4 && p->lara.health>0 && !p->lara.water_status && !toggle) {
        tomb_combat_target(p,(input&64)!=0);
    }
    int combat=g->status==2 || g->status==4 || (!g->status && toggle && !p->lara.water_status);
    if(p->weapon_type==4)tomb_shotgun_tick(g,toggle,(input&64)!=0 && (!p->shooter.mode || g->left.lock),p->lara.health>0,p->lara.water_status,fire_shotgun,sound,p);
    else if(p->weapon_type==2)tomb_magnums_tick(g,toggle,(input&64)!=0,p->lara.health>0,p->lara.water_status,fire,sound,p);
    else if(p->weapon_type==3)tomb_uzis_tick(g,toggle,(input&64)!=0,p->lara.health>0,p->lara.water_status,fire,sound,p);
    else tomb_pistols_tick(g,toggle,(input&64)!=0,p->lara.health>0,p->lara.water_status,fire,sound,p);
    /* DOS DrawPistols swaps meshes at frame 13, not when gun_type changes. */
    if(p->weapon_type!=4 && g->status==2 && g->left.frame==13)p->sidearm_type=p->weapon_type;
    p->animation.weapon_status=g->status;
    p->lara.head_yaw=g->head_yaw;p->lara.head_pitch=g->head_pitch;p->lara.torso_yaw=g->torso_yaw;p->lara.torso_pitch=g->torso_pitch;
    if(looking) {
        p->camera_request.angle=tomb_word(g->head_yaw+g->torso_yaw);
        p->camera_request.elevation=tomb_word(g->head_pitch+g->torso_pitch);
    }
    if(!looking && combat && g->status!=3 && p->lara.health>0) {
        p->camera_request=(TombCameraRequest){2560,TOMB_CAMERA_COMBAT,8,g->target>=0?g->target_yaw:tomb_word(g->head_yaw+g->torso_yaw),g->target>=0?g->target_pitch:tomb_word(g->head_pitch+g->torso_pitch),p->lara.pitch,-1,0};
    }
    return !p->blocked;
}
