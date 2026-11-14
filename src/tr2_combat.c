#include "tr2_world.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* The original has one head/torso state shared by Look and weapon control. */
void tr2_combat_look_pose(const Tr2Actor *a,const Tr2Combat *g,const Tr2Look *look,Tr2Look *out){
    *out=look?*look:(Tr2Look){0};
    if(g && (a->gun_status==3 || a->gun_status==4)){
        out->torso_yaw=g->torso_yaw;out->torso_pitch=g->torso_pitch;
        if(g->weapon>=1 && g->weapon<=3){out->yaw=g->torso_yaw;out->pitch=g->torso_pitch;}
    }
}
uint32_t tr2_combat_look_input(Tr2Combat *g,const Tr2Actor *a,Tr2Look *look,uint32_t input){
    Tr2Look shared;tr2_combat_look_pose(a,g,look,&shared);*look=shared;
    uint32_t result=tr2_look_input(look,a,input);
    if(g && look->active && (a->gun_status==3 || a->gun_status==4)){
        g->torso_yaw=look->torso_yaw;g->torso_pitch=look->torso_pitch;
    }
    return result;
}

/* GrenadeControl 0x42c5c0. Speed decays before movement, while vertical
   acceleration starts only when the previous speed is below 190. */
void tr2_grenade_step(Tr2Actor *a){
    int old_speed=a->speed;a->speed=tr2_word(a->speed-1);if(old_speed<190)a->fall_speed=tr2_word(a->fall_speed+1);
    a->y=tr2_long((int64_t)a->y+a->fall_speed-tr2_asr(a->speed*tr2_sin(a->pitch),14));
    int forward=tr2_asr(a->speed*tr2_cos(a->pitch),14);
    a->z=tr2_long((int64_t)a->z+tr2_asr(forward*tr2_cos(a->yaw),14));a->x=tr2_long((int64_t)a->x+tr2_asr(forward*tr2_sin(a->yaw),14));
}
/* The original sweeps both horizontal endpoints through the target's local
   bounds, but uses only the new vertical position. */
int tr2_grenade_overlap(const int b[6],const Tr2Actor *target,const Tr2Actor *a,int old_x,int old_z,int radius){
    if(a->y+radius<target->y+b[2] || a->y-radius>target->y+b[3])return 0;
    int s=tr2_sin(target->yaw),c=tr2_cos(target->yaw),dx=a->x-target->x,dz=a->z-target->z,ox=old_x-target->x,oz=old_z-target->z;
    int x=tr2_asr(tr2_long((int64_t)dx*c-(int64_t)dz*s),14),old=tr2_asr(tr2_long((int64_t)ox*c-(int64_t)oz*s),14);
    if((x+radius<b[0] && old+radius<b[0]) || (x-radius>b[1] && old-radius>b[1]))return 0;
    int z=tr2_asr(tr2_long((int64_t)dz*c+(int64_t)dx*s),14);old=tr2_asr(tr2_long((int64_t)oz*c+(int64_t)ox*s),14);
    return !((z+radius<b[4] && old+radius<b[4]) || (z-radius>b[5] && old-radius>b[5]));
}

int tr2_pistol_draw(Tr2Combat *g){
    int f=g->arms[0].frame+1;if(f<5 || f>23)f=5;
    if(f==13)g->meshes=3;
    if(f==23){for(int i=0;i<2;i++){memset(&g->arms[i].aim,0,sizeof g->arms[i].aim);g->arms[i].frame=0;}g->target=-1;return 1;}
    g->arms[0].frame=g->arms[1].frame=(int16_t)f;return 0;
}
int tr2_pistol_holster(Tr2Combat *g){
    for(int i=0;i<2;i++){
        Tr2GunArm *a=g->arms+i;int f=a->frame;
        if(f>=24)f=4;
        else if(f>0 && f<5){a->aim.pitch=tr2_word(a->aim.pitch-a->aim.pitch/f);a->aim.yaw=tr2_word(a->aim.yaw-a->aim.yaw/f);f--;}
        else if(!f){a->aim.yaw=a->aim.pitch=a->aim.roll=0;f=23;}
        else if(f>5){if(--f==12)g->meshes&=(uint16_t)~(1u<<i);}
        a->frame=(int16_t)f;
    }
    if(g->arms[0].frame==5 && g->arms[1].frame==5){g->arms[0].frame=g->arms[1].frame=0;g->arms[0].aim.locked=g->arms[1].aim.locked=0;g->target=-1;return 1;}
    return 0;
}
/* AnimatePistols frame decisions, 0x42d5c0; caller emits shots/sounds. */
int tr2_pistol_frame(Tr2GunArm *a,int action,int target,int recoil){
    int f=a->frame,fire=0;
    if(!a->aim.locked && !(action && !target)){if(f>=24)f=4;else if(f>0 && f<=4)f--;}
    else if(f>=0 && f<4)f++;
    else if(f==4){if(action){fire=1;f=24;}}
    else if(f>=24 && ++f==24+recoil)f=4;
    a->frame=tr2_word(f);return fire;
}
int tr2_pistol_sound(Tr2Combat *g,int arm,int frame,int action,int target,int fired,int previous_shot){
    int base=g->weapon==1?8:g->weapon==2?21:43;
    if((!g->arms[arm].aim.locked && !(action && !target)) || (frame==4 && !action)){
        if(g->uzi_sound[arm]){g->uzi_sound[arm]=0;return base+1;}return -1;
    }
    if(frame==4 && action && fired){if(g->weapon==3)g->uzi_sound[arm]=1;return previous_shot?-1:base;}
    if(frame>=24 && g->weapon==3){g->uzi_sound[arm]=1;return base;}return -1;
}

/* AimWeapon 0x42f2a0. Angles approach linearly, without wrap-shortening. */
static int16_t approach(int current,int target,int speed){
    if(current>=target-speed && current<=target+speed)return tr2_word(target);
    return tr2_word(current+(current<target?speed:-speed));
}
void tr2_weapon_aim(Tr2ArmAim *arm,int speed,int yaw,int pitch){
    arm->yaw=approach(arm->yaw,arm->locked?tr2_word(yaw):0,speed);
    arm->pitch=approach(arm->pitch,arm->locked?tr2_word(pitch):0,speed);arm->roll=0;
}
/* FireWeapon 0x42f4df: spheres are in shot-view coordinates. The original
   selects the smallest z-radius, not the exact ray/sphere intersection. */
int tr2_weapon_sphere(const Tr2Sphere *spheres,int count,int *distance){
    int best=-1,depth=INT_MAX;
    for(int i=0;i<count;i++){
        const Tr2Sphere *s=spheres+i;int r=s->radius;
        if(r<=0 || s->x<=-r || s->x>=r || s->y<=-r || s->y>=r || s->z<=r)continue;
        if((int64_t)s->x*s->x+(int64_t)s->y*s->y>(int64_t)r*r)continue;
        if(s->z-r<depth){depth=s->z-r;best=i;}
    }
    *distance=depth;return best;
}
/* find_target_point 0x42f1f0; target the upper third of animated bounds. */
void tr2_weapon_target(const int b[6],const Tr2Actor *a,Tr2Position *out){
    int x=(b[0]+b[1])/2,z=(b[4]+b[5])/2,s=tr2_sin(a->yaw),c=tr2_cos(a->yaw);
    *out=(Tr2Position){a->x+tr2_asr((int64_t)c*x+(int64_t)s*z,14),a->y+b[2]+(b[3]-b[2])/3,a->z+tr2_asr((int64_t)c*z-(int64_t)s*x,14),a->room,0};
}
/* HitTarget 0x42f6e0, health/hit flag and kill-credit branch. Blood and
   monk-specific hostility are deliberately outside this shared primitive. */
int tr2_weapon_damage(Tr2Actor *a,int damage){
    int killed=a->control.health>0 && a->control.health<=damage;
    a->control.health=tr2_word(a->control.health-damage);a->control.flags|=16;return killed;
}
static unsigned root(unsigned n){unsigned r=0,b=1u<<30;while(b>n)b>>=2;while(b){if(n>=r+b){n-=r+b;r=(r>>1)+b;}else r>>=1;b>>=2;}return r;}
void tr2_weapon_angles(int x,int y,int z,int16_t angles[2]){
    angles[0]=tr2_word(tr2_atan(z,x));while(x!=tr2_word(x) || y!=tr2_word(y) || z!=tr2_word(z)){x=tr2_asr(x,2);y=tr2_asr(y,2);z=tr2_asr(z,2);}
    int p=tr2_word(tr2_atan((int)root((unsigned)(x*x)+(unsigned)(z*z)),y));if((y>0 && p>0) || (y<0 && p<0))p=-p;angles[1]=tr2_word(p);
}
static void sound(Tr2Actor *a,int id){if(a->event_count<16){int n=a->event_count++;a->event_kind[n]=5;a->event_value[n]=id;}}
static int aim_point(const Tr2Level *l,const Tr2Actor *a,Tr2Position *p){int b[6];if(!tr2_best_bounds(l,a,b))return 0;tr2_weapon_target(b,a,p);return 1;}
/* HitTarget (42f733). Store allegiance with the monks in the saved item array. */
int tr2_player_damage(Tr2World *w,Tr2Object *o,int amount){
    if((o->id==53 || o->id==54) && !o->linked[1]){
        Tr2Creature *c=w->creatures+(o-w->objects);
        c->attack+=amount;
        if((c->attack&4095)>10 || !c->mood)
            for(uint32_t i=0;i<w->count;i++)if(w->objects[i].id==53 || w->objects[i].id==54)w->objects[i].linked[1]=1;
    }
    return tr2_weapon_damage(&o->pose,amount);
}
static int enemy(const Tr2Object *o){return o->active && o->status==1 && o->pose.control.health>0 && ((o->id>=15 && o->id<=18) || o->id==20 || o->id==21 || o->id==25 || o->id==28 || o->id==29 || o->id==30 || o->id==31 || o->id==34 || (o->id==32 || o->id==33) || o->id==36 || o->id==38 || tr2_xian_enemy(o->id) || tr2_tibetan_enemy(o->id) || o->id==47 || o->id==49 || o->id==50 || o->id==51 || o->id==39 || o->id==214);}
/* Root LOS leaves this transient list for ObjectOnLOS. Targeting refreshes
   it before each firing tick; missed pellets can replace it within a burst. */
static int shot_rooms[1024],shot_room_count;
void tr2_target_info(const Tr2Level *l,Tr2World *w,const Tr2Actor *a){
    Tr2Combat *g=w->combat;Tr2Position p,origin={a->x,a->y-650,a->z,a->room,0};
    if((uint32_t)g->target>=w->count || !aim_point(l,&w->objects[g->target].pose,&p)){g->arms[0].aim.locked=g->arms[1].aim.locked=0;g->yaw=g->pitch=0;return;}
    int mounted=(uint32_t)w->vehicle<w->count && w->objects[w->vehicle].id==13 && w->objects[w->vehicle].snow.drive.armed;
    int16_t angles[2];tr2_weapon_angles(p.x-origin.x,p.y-origin.y,p.z-origin.z,angles);int yaw=tr2_word(angles[0]-a->yaw),pitch=tr2_word(angles[1]-a->pitch);
    if(!tr2_shot_los(l,&origin,&p,0,shot_rooms,&shot_room_count))g->arms[0].aim.locked=g->arms[1].aim.locked=0;
    else if(yaw>=-(mounted?5460:10920) && yaw<=(mounted?5460:10920) && pitch>=-(g->weapon==7?11830:(mounted || g->weapon>=4)?10010:10920) && pitch<=(g->weapon==7?11830:(mounted || g->weapon>=4)?10010:10920))g->arms[0].aim.locked=g->arms[1].aim.locked=1;
    else{
        if(yaw< -(g->weapon>=4?14560:30940) || yaw>(g->weapon>=4?14560:10920) || pitch< -(g->weapon==7?13650:g->weapon>=4?11830:14560) || pitch>(g->weapon==7?13650:g->weapon>=4?11830:14560))g->arms[0].aim.locked=0;
        if(yaw< -(g->weapon>=4?14560:10920) || yaw>(g->weapon>=4?14560:30940) || pitch< -(g->weapon==7?13650:g->weapon>=4?11830:14560) || pitch>(g->weapon==7?13650:g->weapon>=4?11830:14560))g->arms[1].aim.locked=0;
    }g->yaw=(int16_t)yaw;g->pitch=(int16_t)pitch;
}
void tr2_new_target(const Tr2Level *l,Tr2World *w,const Tr2Actor *a){
    Tr2Combat *g=w->combat;Tr2Position origin={a->x,a->y-650,a->z,a->room,0};int distance=INT_MAX,angle=32767,range=g->weapon==5?12288:8192;g->target=-1;
    for(int slot=0;slot<5;slot++){
        int i=w->creature_slots[slot];if((uint32_t)i>=w->count)continue;
        Tr2Object *o=w->objects+i;if(!enemy(o))continue;int dx=o->pose.x-origin.x,dy=o->pose.y-origin.y,dz=o->pose.z-origin.z;
        if(abs(dx)>range || abs(dy)>range || abs(dz)>range)continue;int d=dx*dx+dy*dy+dz*dz;if(d>=range*range)continue;
        Tr2Position p;if(!aim_point(l,&o->pose,&p) || !tr2_camera_los(l,&origin,&p,0))continue;
        int mounted=(uint32_t)w->vehicle<w->count && w->objects[w->vehicle].id==13 && w->objects[w->vehicle].snow.drive.armed;
    int16_t angles[2];tr2_weapon_angles(p.x-origin.x,p.y-origin.y,p.z-origin.z,angles);int yaw=tr2_word(angles[0]-a->yaw-g->torso_yaw),pitch=tr2_word(angles[1]-a->pitch-g->torso_pitch);
        if(yaw< -(mounted?5460:10920) || yaw>(mounted?5460:10920) || pitch< -(g->weapon==7?11830:(mounted || g->weapon>=4)?10010:10920) || pitch>(g->weapon==7?11830:(mounted || g->weapon>=4)?10010:10920))continue;
        if(abs(yaw)<angle+2730 && d<distance){angle=abs(yaw);distance=d;g->target=(int)i;}
    }tr2_target_info(l,w,a);
}
static int random15(Tr2World *w){w->random_state=w->random_state*0x41c64e6du+12345;return (w->random_state>>10)&32767;}
/* ObjectOnLOS 0x416310: the original tests forward planes in room-list
   order, including planes beyond the endpoint, rather than nearest distance. */
int tr2_object_on_los(const Tr2Level *l,const Tr2World *w,const Tr2Position *s,const Tr2Position *t,const int *rooms,int count){
    int dx=t->x-s->x,dy=t->y-s->y,dz=t->z-s->z,axis=abs(dz)>abs(dx),direction=axis?dz:dx;
    if(!direction)return -1;
    for(int r=0;r<count;r++){if(rooms[r]<0 || rooms[r]>=1024)continue;
        for(int i=w->room_head[rooms[r]],steps=0;i>=0 && (uint32_t)i<w->count && steps++<(int)w->count;i=w->objects[i].next_room){
            const Tr2Object *o=w->objects+i;if(o->status==2 || (o->id!=72 && o->id!=88))continue;
            int b[6];if(!tr2_best_bounds(l,&o->pose,b))continue;
            int swap=(tr2_word(o->pose.yaw+8192)&16384)!=0,xbase=swap?4:0,zbase=swap?0:4,mask=0;
            for(int side=0;side<2;side++){
                int delta=(axis?o->pose.z-s->z:o->pose.x-s->x)+b[(axis?zbase:xbase)+side];
                if((delta^direction)<0)continue;
                int y=tr2_long((int64_t)delta*dy)/direction;if(y<=o->pose.y+b[2]-s->y || y>=o->pose.y+b[3]-s->y)continue;
                int cross=tr2_long((int64_t)delta*(axis?dx:dz))/direction,base=(axis?o->pose.x-s->x:o->pose.z-s->z),bi=axis?xbase:zbase;
                if(cross<base+b[bi])mask|=1;else if(cross>base+b[bi+1])mask|=2;else return i;
            }if(mask==3)return i;
        }
    }return -1;
}
static int fire_weapon(const Tr2Level *l,Tr2World *w,Tr2Actor *a,const Tr2GunArm *arm,int weapon){
    /* Root 42bf70 writes its moving/stationary adjustments into weapon 6's
       descriptor. Weapon 5 therefore retains spread 728 and damage 3. */
    Tr2Combat *g=w->combat;int ammo=weapon==9?1:weapon;if(ammo==1)g->ammo[1]=1000;if(g->ammo[ammo]<=0){g->ammo[ammo]=0;return 0;}g->ammo[ammo]--;
    int pitch=tr2_word(arm->aim.pitch+(random15(w)-16384)*(weapon==4?0:weapon==5?728:1456)/65536),yaw=tr2_word(a->yaw+arm->aim.yaw+(random15(w)-16384)*(weapon==4?0:weapon==5?728:1456)/65536);
    Tr2Position origin={a->x,a->y-(weapon==9?400:weapon>=4?500:650),a->z,a->room,0};Tr2Sphere spheres[64];int distance=8192,hit=-1;
    if((uint32_t)g->target<w->count){Tr2Object *o=w->objects+g->target;int count=tr2_spheres_shot(l,&o->pose,o->id,&origin,yaw,pitch,spheres);hit=tr2_weapon_sphere(spheres,count,&distance);}
    if(hit<0)distance=8192;g->shots++;
    int cp=tr2_cos(pitch),forward[3]={tr2_asr(tr2_sin(yaw)*cp,14),-tr2_sin(pitch),tr2_asr(tr2_cos(yaw)*cp,14)};
    Tr2Position p={origin.x+tr2_asr(distance*forward[0],14),origin.y+tr2_asr(distance*forward[1],14),origin.z+tr2_asr(distance*forward[2],14),origin.room,0};
    int clear=1;if(hit<0)clear=tr2_shot_los(l,&origin,&p,0,shot_rooms,&shot_room_count);
    int object=tr2_object_on_los(l,w,&origin,&p,shot_rooms,shot_room_count);
    if(object>=0 && w->objects[object].id==72)tr2_smash_window(l,w,w->objects+object);
    if(object>=0 && w->objects[object].id==88)w->objects[object].active=w->objects[object].status=1;
    if(hit>=0){Tr2Object *o=w->objects+g->target;g->hits++;w->inventory->kills+=tr2_player_damage(w,o,(weapon==4 || weapon==5 || weapon==9)?3:weapon==2?2:1);p.room=o->pose.room;tr2_blood(w,&p,o->pose.speed,o->pose.yaw);}
    else if(object<0 && !clear)tr2_ricochet(w,&p);
    return 1;
}
static int fire(const Tr2Level *l,Tr2World *w,Tr2Actor *a,const Tr2GunArm *arm){
    return fire_weapon(l,w,a,arm,w->combat->weapon);
}
void tr2_snow_guns(const Tr2Level *l,Tr2World *w,Tr2Actor *a,Tr2Object *sled,uint32_t input){
    Tr2Combat *g=w->combat;
    tr2_new_target(l,w,a);
    Tr2GunArm *arm=&g->arms[1];
    tr2_weapon_aim(&arm->aim,1820,g->yaw,g->pitch);
    if(!(input&TR2_ACTION) || !fire_weapon(l,w,a,arm,9))return;
    arm->flash=2;sound(a,43);
    tr2_dynamic_light(w,a->x+tr2_asr(tr2_sin(a->yaw),4),a->y-512,a->z+tr2_asr(tr2_cos(a->yaw),4),12,11);
    for(int j=0;j<2;j++){
        int p[]={j?-235:219,-71,550};
        if(!tr2_joint(l,&sled->pose,13,0,p))continue;
        for(int k=0;k<TR2_MAX_EFFECTS;k++)if(!w->hazards->effects[k].active){
            Tr2Fragment *f=w->hazards->effects+k;*f=(Tr2Fragment){0};f->active=1;f->object=240;f->room=a->room;
            f->x=p[0];f->y=p[1];f->z=p[2];f->yaw=a->yaw;f->counter=3;f->shade=4096;w->hazards->owner[k]=-2;break;
        }
    }
}
int tr2_weapon_model(int weapon){static const int models[8]={0,1,4,5,3,6,7,8};return weapon>=1 && weapon<=7?models[weapon]:0;}
/* AnimateShotgun 42cc50, including M16 movement and underwater harpoon states.
   Return the firing weapon; actual firing/animation stays with the caller. */
int tr2_rifle_state(Tr2Combat *g,Tr2Actor *a,uint32_t input,int frame){
    Tr2Control *r=&g->rifle.control;int action=(input&TR2_ACTION)!=0,locked=g->arms[0].aim.locked;
    int alternate=a->control.water_status==1 || (g->weapon==5 && a->speed!=0),fire=0;
    if(r->current==0 || r->current==6){
        g->rifle_sound=0;
        if(g->harpoon_reload){r->goal=5;g->harpoon_reload=0;}
        else if(r->current==0)r->goal=alternate?6:((action && g->target<0) || locked)?2:4;
        else r->goal=!alternate?0:((action && g->target<0) || locked)?8:7;
    }else if(r->current==2){
        if(!frame){
            r->goal=4;
            if(!alternate && !g->harpoon_reload){
                if(action && (g->target<0 || locked)){fire=g->weapon;r->goal=2;if(g->weapon==5){sound(a,78);g->rifle_sound=1;}}
                else if(!action && locked)r->goal=0;
            }
            if(r->goal!=2 && g->rifle_sound){sound(a,104);g->rifle_sound=0;}
        }else if(g->rifle_sound)sound(a,78);
        else if(g->weapon==4 && !action && !locked)r->goal=4;
    }else if(r->current==8){
        if(!frame){r->goal=7;if(alternate && !g->harpoon_reload){
            if(action && (g->target<0 || locked)){fire=g->weapon==7?7:5;r->goal=8;}
            else if(!action && locked)r->goal=6;
        }}
        if(g->weapon==5 && r->goal==8)sound(a,78);
    }
    return fire;
}
static void rifle_tick(const Tr2Level *l,Tr2World *w,Tr2Actor *a,uint32_t input){
    Tr2Combat *g=w->combat;const uint8_t *m=tr2_model(l,tr2_weapon_model(g->weapon));if(!m)return;
    if(!g->rifle_active){memset(&g->rifle,0,sizeof g->rifle);tr2_set_animation(l,&g->rifle,tr2_u16(m+16)+(g->weapon==6?0:1),0);g->rifle.control.current=g->rifle.control.goal=1;g->rifle_active=1;}
    Tr2Actor *r=&g->rifle;int mode=a->gun_status;
    if(mode==3)r->control.goal=a->control.water_status==2?9:3;
    if(mode==4){
        if(input&TR2_ACTION)tr2_target_info(l,w,a);else g->target=-1;if(g->target<0)tr2_new_target(l,w,a);
        tr2_weapon_aim(&g->arms[0].aim,1820,g->yaw,g->pitch);
        if(g->arms[0].aim.locked){g->torso_yaw=g->arms[0].aim.yaw;g->torso_pitch=g->arms[0].aim.pitch;}
        int frame=r->control.frame-tr2_i16(l->animations.data+r->control.animation*32+16);
        int shot=tr2_rifle_state(g,a,input,frame);
        if(shot==7){tr2_player_harpoon_fire(l,w,a);if(!(g->ammo[7]&3))g->harpoon_reload=1;}
        else if(shot==6)tr2_grenade_fire(l,w,a);
        else if(shot==5){if(fire(l,w,a,&g->arms[0]))g->arms[1].flash=3;}
        else if(shot==4){int fired=0;for(int i=0;i<6;i++){Tr2GunArm pellet=g->arms[0];pellet.aim.yaw=tr2_word(pellet.aim.yaw+(random15(w)-16384)*3640/65536);pellet.aim.pitch=tr2_word(pellet.aim.pitch+(random15(w)-16384)*3640/65536);fired|=fire(l,w,a,&pellet);}
            if(fired){sound(a,45);g->arms[1].flash=3;}}
    }

    Tr2Object item={0};item.pose=*r;item.status=1;tr2_object_animate(l,&item);*r=item.pose;
    for(int i=0;i<r->event_count;i++)if(a->event_count<16){int n=a->event_count++;a->event_kind[n]=r->event_kind[i];a->event_value[n]=r->event_value[i];}
    int frame=r->control.frame-tr2_i16(l->animations.data+r->control.animation*32+16);
    if(mode==2){if(r->control.current==0 || r->control.current==6){a->gun_status=4;memset(g->arms,0,sizeof g->arms);g->target=-1;}else if(frame==10){g->meshes=2;g->back_weapon=0;}else if(a->control.water_status==1)r->control.goal=6;}
    if(mode==3){if(item.status==2){a->gun_status=0;g->rifle_active=0;g->target=-1;memset(g->arms,0,sizeof g->arms);}else if(r->control.current==3 && frame==21){g->meshes=0;g->back_weapon=g->weapon;}}
    g->arms[0].frame=g->arms[1].frame=(int16_t)frame;
}
void tr2_combat_view_tick(const Tr2Level *l,Tr2World *w,Tr2Actor *a,uint32_t input,const Tr2Look *look){
    Tr2Combat *g=w->combat;if(!g || !w->inventory)return;
    /* ResetLook (0x4277f0) runs before WeaponControl in the original.
       PistolHandler writes the same rotation to head and torso; RifleHandler
       only writes the torso. Keep the armed pose in that reset path too. */
    if(look && !a->extra_animation){g->torso_yaw=look->torso_yaw;g->torso_pitch=look->torso_pitch;}
    else if(!(input&TR2_LOOK) && !a->extra_animation){
        Tr2Look reset={0};
        if(g->weapon>=1 && g->weapon<=3){reset.yaw=g->torso_yaw;reset.pitch=g->torso_pitch;}
        tr2_look_input(&reset,a,0);
        g->torso_yaw=reset.torso_yaw;g->torso_pitch=reset.torso_pitch;
    }
    for(int i=0;i<2;i++)if(g->arms[i].flash>0)g->arms[i].flash--;
    if(a->extra_animation)return;
    if(a->control.health<=0){a->gun_status=0;if(g->weapon==8)tr2_flare_tick(l,w,a);return;}
    if(!a->gun_status && (input&32) && g->last_weapon>0 && g->requested==g->weapon)g->requested=g->last_weapon;
    if((input&0x80000) && (!a->gun_status || a->gun_status==4)){
        if(g->weapon==8 && !a->gun_status)a->gun_status=3;
        else if(w->inventory->count[151]>0)g->requested=8;
    }
    /* 42ead9/42eb19: mounted Lara may use flares, but cannot draw guns.
       A gun request while holding a flare only updates the remembered gun. */
    if(!a->gun_status && w->vehicle>=0 && g->requested>=1 && g->requested<=7){
        g->last_weapon=g->requested;
        if(g->weapon==8)g->requested=8;else g->weapon=g->requested;
        input&=~32u;
    }
    if(g->weapon==8){
        if(!a->gun_status && (input&32))g->requested=g->last_weapon;
        if(!a->gun_status && g->requested!=8){tr2_flare_create(l,w,a,0);g->meshes=0;g->flare_control=0;g->weapon=g->requested;g->rifle_active=0;input|=32;}
        else{tr2_flare_tick(l,w,a);return;}
    }
    if(!a->gun_status && g->requested==8 && w->inventory->count[151]>0){
        if(g->weapon>0 && g->weapon<8)g->last_weapon=g->weapon;g->weapon=8;a->gun_status=2;g->rifle_active=0;g->arms[0].frame=0;g->flare_timer=0;g->torso_yaw=g->torso_pitch=0;tr2_flare_tick(l,w,a);return;
    }
    int allowed=g->requested==7 || a->control.water_status==0 || (a->control.water_status==4 && a->water_distance> -(g->weapon>=4?500:650));
    if(!a->gun_status && g->requested>=1 && g->requested<=7 && ((input&32) || g->weapon!=g->requested) && allowed && w->inventory->count[g->requested==7?139:g->requested==5?140:g->requested==6?141:g->requested==4?136:g->requested==2?137:g->requested==3?138:135]){a->gun_status=2;g->weapon=g->last_weapon=g->requested;
        if(l->custom.options&TR2_CONSISTENT_WEAPONS){
            w->level->custom.last_drawn=g->weapon;
            if(g->weapon<=3)w->level->custom.sidearm=g->weapon;
        }
        g->arms[0].frame=g->arms[1].frame=0;g->target=-1;g->rifle_active=0;}
    if(g->weapon<1 || g->weapon>7)return;
    allowed=g->weapon==7 || a->control.water_status==0 || (a->control.water_status==4 && a->water_distance> -(g->weapon>=4?500:650));
    if(a->gun_status==4 && ((input&32) || !allowed || g->requested!=g->weapon))a->gun_status=3;
    if(a->gun_status==4 && (input&TR2_ACTION) && g->weapon>1 && g->ammo[g->weapon]<=0){g->ammo[g->weapon]=0;sound(a,48);g->requested=w->inventory->count[135]?1:0;return;}
    if(g->weapon>=4 && g->weapon<=7){int ready=a->gun_status==4;if(a->gun_status>=2 && a->gun_status<=4)rifle_tick(l,w,a,input);if(ready && (g->weapon==4 || g->weapon==5) && g->arms[1].flash)tr2_dynamic_light(w,a->x+tr2_asr(tr2_sin(a->yaw),4),a->y-512,a->z+tr2_asr(tr2_cos(a->yaw),4),12,11);return;}
    if(a->gun_status==2){int before=g->arms[0].frame;if(tr2_pistol_draw(g))a->gun_status=4;else if(before!=13 && g->arms[0].frame==13)sound(a,6);return;}
    if(a->gun_status==3){int frames[2]={g->arms[0].frame,g->arms[1].frame};if(tr2_pistol_holster(g))a->gun_status=0;for(int i=0;i<2;i++)if(frames[i]==13)sound(a,7);g->torso_yaw=(int16_t)((g->arms[0].aim.yaw+g->arms[1].aim.yaw)/4);g->torso_pitch=(int16_t)((g->arms[0].aim.pitch+g->arms[1].aim.pitch)/4);return;}
    if(a->gun_status!=4)return;
    if(input&TR2_ACTION)tr2_target_info(l,w,a);else g->target=-1;
    if(g->target<0)tr2_new_target(l,w,a);
    for(int i=0;i<2;i++)tr2_weapon_aim(&g->arms[i].aim,1820,g->yaw,g->pitch);
    int locked=g->arms[0].aim.locked+g->arms[1].aim.locked;
    if(locked){g->torso_yaw=(int16_t)(((g->arms[0].aim.locked?g->arms[0].aim.yaw:0)+(g->arms[1].aim.locked?g->arms[1].aim.yaw:0))/(locked*2));g->torso_pitch=(int16_t)(((g->arms[0].aim.locked?g->arms[0].aim.pitch:0)+(g->arms[1].aim.locked?g->arms[1].aim.pitch:0))/(locked*2));}
    int fired=0;for(int i=1;i>=0;i--){int frame=g->arms[i].frame,action=(input&TR2_ACTION)!=0;
        int shot=tr2_pistol_frame(g->arms+i,action,g->target>=0,g->weapon==3?3:9) && fire(l,w,a,g->arms+i);
        if(shot)g->arms[i].flash=3;int id=tr2_pistol_sound(g,i,frame,action,g->target>=0,shot,fired);if(id>=0)sound(a,id);fired|=shot;
    }
    if(g->arms[0].flash || g->arms[1].flash)tr2_dynamic_light(w,a->x+tr2_asr(tr2_sin(a->yaw),4),a->y-512,a->z+tr2_asr(tr2_cos(a->yaw),4),12,11);
}

void tr2_combat_tick(const Tr2Level *l,Tr2World *w,Tr2Actor *a,uint32_t input){tr2_combat_view_tick(l,w,a,input,NULL);}




