/* Optional camera-relative controls. Original controllers are unchanged. */
#include "playtest.h"
#include "combat.h"
#include "enemies.h"
#include "object_contact.h"
#include "fixed.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
static int clamp(int x,int lo,int hi){return x<lo?lo:x>hi?hi:x;}
static int16_t turn(int16_t from,int16_t to,int speed){return tomb_word(from+clamp(tomb_word(to-from),-speed,speed));}
/* Ray/box slabs in object-local space; collision bounds also keep the camera
   outside pillars that are not represented by room floor/ceiling sectors. */
static double box_ray(TombCameraPoint from,const double dir[3],int32_t x,int32_t y,int32_t z,int yaw,const int16_t bounds[6],double limit){
    double angle=yaw*(6.283185307179586/65536),si=sin(angle),co=cos(angle);
    double dx=from.x-x,dz=from.z-z;
    double origin[3]={dx*co-dz*si,from.y-y,dx*si+dz*co};
    double d[3]={dir[0]*co-dir[2]*si,dir[1],dir[0]*si+dir[2]*co};
    double lo=0,hi=limit;
    for(int j=0;j<3;j++){
        if(fabs(d[j])<1e-9){if(origin[j]<bounds[j*2] || origin[j]>bounds[j*2+1])return limit;}
        else {double a=(bounds[j*2]-origin[j])/d[j],b=(bounds[j*2+1]-origin[j])/d[j];if(a>b){double t=a;a=b;b=t;}if(a>lo)lo=a;if(b<hi)hi=b;if(lo>hi)return limit;}
    }
    return lo;
}
static int ray_clear(const TombLevel *l,TombCameraPoint *p){
    TombSectorRef ref;TombHeights h;
    if(!tomb_find_sector(l,p->x,p->y,p->z,p->room,&ref) ||
       !tomb_static_heights(l,ref,p->x,p->z,0,&h))return 0;
    if(l->items && !tomb_item_heights(l,ref,p->x,p->y,p->z,0,l->items,l->item_count,&h))return 0;
    const TombRoom *r=l->rooms+ref.room;
    if(p->x<r->x || p->x>=r->x+r->nx*1024 || p->z<r->z || p->z>=r->z+r->nz*1024 ||
       h.floor==-32512 || h.ceiling==-32512 || p->y>=h.floor || p->y<=h.ceiling)return 0;
    p->room=(int16_t)ref.room;return 1;
}
static TombCameraPoint ray_point(TombCameraPoint from,const double d[3],double t,int room){
    return (TombCameraPoint){(int32_t)floor(from.x+d[0]*t),(int32_t)floor(from.y+d[1]*t),
        (int32_t)floor(from.z+d[2]*t),(int16_t)room};
}
void tomb_shooter_world_ray(const TombPlaytest *p,TombCameraPoint from,TombCameraPoint *to){
    /* Sample actual sloped heights along the unchanged ray, then refine the
       first contact. DOS LOS's horizontal endpoint plane is unsuitable here. */
    double delta[3]={to->x-from.x,to->y-from.y,to->z-from.z};
    double distance=sqrt(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]);
    if(distance<1)return;
    double direction[3]={delta[0]/distance,delta[1]/distance,delta[2]/distance};
    TombCameraPoint clear=from;
    if(!ray_clear(p->level,&clear)){*to=from;return;}
    for(double lo=0;lo<distance;){
        double hi=fmin(lo+8,distance);
        TombCameraPoint next=ray_point(from,direction,hi,clear.room);
        if(!ray_clear(p->level,&next)){
            while(hi-lo>0.125){double mid=(hi+lo)*0.5;next=ray_point(from,direction,mid,clear.room);
                if(ray_clear(p->level,&next)){lo=mid;clear=next;}else hi=mid;}
            *to=ray_point(from,direction,hi,clear.room);break;
        }
        clear=next;to->room=clear.room;lo=hi;
    }
    double dx=to->x-from.x,dy=to->y-from.y,dz=to->z-from.z,len=sqrt(dx*dx+dy*dy+dz*dz);if(len<1)return;
    double d[3]={dx/len,dy/len,dz/len},best=len;
    for(size_t r=0;r<p->level->room_count;r++){
        int inactive=0;for(size_t i=0;i<p->level->room_count;i++)if(p->level->rooms[i].alternate==(int)r){inactive=1;break;}if(inactive)continue;
        const TombRoom *room=p->level->rooms+r;
        for(size_t i=0;i<room->static_count;i++){
            const TombStaticPlacement *s=room->statics+i;
            for(size_t j=0;j<p->level->static_def_count;j++){
                const TombStaticDef *def=p->level->static_defs+j;if(def->id!=s->id)continue;
                if(!(def->flags&1))best=box_ray(from,d,s->x,s->y,s->z,s->rotation,def->bounds,best);break;
            }
        }
    }
    if(p->objects)for(size_t i=0;i<p->objects->count;i++){
        const TombObject *o=p->objects->items+i;if((o->actor.flags&6)==6 || (o->object!=48 && o->object!=50 && o->object!=51))continue;
        TombPose pose;if(tomb_object_pose(p->visual,&o->actor,&pose))best=box_ray(from,d,o->actor.x,o->actor.y,o->actor.z,o->actor.yaw,pose.bounds,best);
    }
    if(best<len){to->x=from.x+(int32_t)(d[0]*best);to->y=from.y+(int32_t)(d[1]*best);to->z=from.z+(int32_t)(d[2]*best);TombSectorRef ref;if(tomb_find_sector(p->level,to->x,to->y,to->z,from.room,&ref))to->room=(int16_t)ref.room;}
}
void tomb_shooter_start(TombPlaytest *p,int mode){
    p->tps_slope_jump=p->tps_drop_yaw_offset=p->fps_shot_held=0;memset(&p->fps,0,sizeof p->fps);
    memset(&p->shooter,0,sizeof p->shooter);p->shooter.mode=mode>=TOMB_MODE_ORIGINAL && mode<=TOMB_MODE_VR?mode:0;
    p->shooter.yaw=p->lara.actor.yaw;p->shooter.pitch=-1820;
    p->shooter.dual=mode==TOMB_MODE_FPS;
}
void tomb_shooter_mouse(TombPlaytest *p,int dx,int dy){
    if(!p->shooter.mode || p->shooter.mode==TOMB_MODE_VR)return;
    p->shooter.yaw=tomb_word(p->shooter.yaw+clamp(dx,-4096,4096)*24);
    p->shooter.pitch=(int16_t)clamp(p->shooter.pitch-clamp(dy,-4096,4096)*24,-14000,14000);
}
static void anim(TombPlaytest *p,int index){
    TombActor *a=&p->lara.actor;const TombAnim *v=p->level->animations+index;
    a->animation=(int16_t)index;a->frame=v->first_frame;a->current=a->goal=v->state;
}
int tomb_fps_ready(const TombPlaytest *p){
    int state=p->lara.actor.current;
    return p->lara.health>0 && !p->lara.water_status &&
        (state==0 || state==1 || state==2 || state==3 || state==5 || state==9 || state==16 || state==24 || state==28 || state==29 || state==32);
}
static uint32_t shooter_input(TombPlaytest *p,uint32_t raw,int fps);
static uint32_t fps_input(TombPlaytest *p,uint32_t raw){
    TombShooter *s=&p->shooter;TombActor *a=&p->lara.actor;
    if(p->fps.mantle_ticks){s->input=raw;s->jump_held=(raw&16)!=0;return 0;}
    int toggle=(raw&TOMB_INPUT_AIM)!=0;
    if(toggle!=s->mode_held){
        s->next_hand=0;
        if(a->flags&8)a->speed=(int16_t)(toggle?a->speed/2:a->speed*2);
    }
    s->dual=!toggle;
    s->mode_held=toggle;if(!(raw&128))s->drop_latched=0;
    int x=((raw&8)!=0)-((raw&4)!=0),z=((raw&1)!=0)-((raw&2)!=0);
    int16_t camera=s->yaw,heading=tomb_word(camera+tomb_object_angle(z,x));
    /* Traversal owns the hands during this part of the tick. */
    p->animation.weapon_status=0;
    if(a->current==10 || a->current==30 || a->current==31){
        int climb=(raw&16) && !s->jump_held;
        int drop=(raw&128) && !(s->input&128);
        s->input=raw;s->aim_held=1;s->jump_held=(raw&16)!=0;
        if(!(raw&(4|8)) && (a->current==30 || a->current==31)){anim(p,96);a->frame=1514;a->speed=0;}
        if(drop)s->drop_latched=1;
        return drop?0:64|(raw&(4|8))|(climb?1:0);
    }
    if(a->current>=36 && a->current<=38){s->input=raw;s->aim_held=1;return raw&(15|64);}
    if(p->lara.water_status){
        int climb=(raw&16) && !s->jump_held && (raw&1) && p->lara.water_status==2;
        s->mode=TOMB_MODE_SHOOTER;
        uint32_t out=shooter_input(p,raw&~(128u|TOMB_INPUT_AIM|TOMB_INPUT_SHOOT),1);
        s->mode=TOMB_MODE_FPS;s->input=raw;s->aim_held=1;
        if(climb)out=(out&~16u)|64u;
        if(p->lara.water_status==1 && (raw&128)){p->lara.pitch=turn(p->lara.pitch,-14560,2730);out|=16;}
        return out;
    }
    if(a->flags&8){
        s->input=raw;s->aim_held=1;s->jump_held=(raw&16)!=0;
        if(!(raw&16) && a->fall_speed<0)a->fall_speed=(int16_t)(a->fall_speed+8);
        if(!x && !z)a->speed=(int16_t)(a->speed>6?a->speed-6:0);
        if(x || z){
            double angle=p->animation.move_angle*(6.283185307179586/65536);
            double vx=sin(angle)*a->speed,vz=cos(angle)*a->speed;
            double target=heading*(6.283185307179586/65536);
            int speed=((raw&128)?47:94)/(s->mode_held?2:1);
            double dx=sin(target)*speed-vx,dz=cos(target)*speed-vz,len=sqrt(dx*dx+dz*dz);
            double t=len>8?8/len:1;vx+=dx*t;vz+=dz*t;
            a->speed=(int16_t)lround(sqrt(vx*vx+vz*vz));
            if(a->speed)p->air.move_angle=p->animation.move_angle=(int16_t)lround(atan2(vx,vz)*65536/6.283185307179586);
        }
        a->yaw=p->animation.move_angle;
        return ((raw&1) && !s->drop_latched && abs(tomb_word(a->yaw-camera))<=10000)?64:0;
    }
    if((raw&16) && !s->jump_held && p->lara.health>0 &&
       (a->current==0 || a->current==1 || a->current==2 || a->current==5 || a->current==16 || a->current==24 || a->current==32)){
        a->yaw=(x||z)?heading:camera;anim(p,77);a->flags|=8;a->fall_speed=-116;
        a->speed=(x||z)?((raw&128)?47:94)/(s->mode_held?2:1):0;
        p->air.move_angle=p->animation.move_angle=a->yaw;
        s->input=raw;s->aim_held=s->jump_held=1;
        return (raw&1)?64:0;
    }
    /* Reuse collision and instant gait entry, with FPS-relative translation. */
    if(a->current==0 || a->current==1 || a->animation==11 || a->animation==103)a->yaw=(x||z)?heading:camera;
    int drawn=p->pistols.drawn;s->mode=TOMB_MODE_SHOOTER;s->yaw=(x||z)?heading:camera;p->pistols.drawn=0;
    uint32_t out=shooter_input(p,(raw&(16|64))|((x||z)?1:0),1);
    p->pistols.drawn=drawn;s->mode=TOMB_MODE_FPS;s->yaw=camera;s->input=raw;s->aim_held=1;
    if((a->flags&8) && a->current==3){a->speed=((raw&128)?47:94)/(s->mode_held?2:1);}
    return out;
}
int16_t tomb_shooter_render_yaw(const TombPlaytest *p){
    return tomb_word(p->lara.actor.yaw+(p->shooter.mode==TOMB_MODE_SHOOTER?p->tps_drop_yaw_offset:0));
}
uint32_t tomb_shooter_input(TombPlaytest *p,uint32_t raw){
    if(p->shooter.mode==TOMB_MODE_SHOOTER){
        if((!(p->lara.actor.flags&8) && p->lara.actor.current!=24 && p->lara.actor.current!=32 &&
            p->lara.actor.current!=3 && p->lara.actor.current!=25) || p->lara.water_status || p->lara.health<=0)p->tps_slope_jump=0;
        if(p->tps_drop_yaw_offset>0)p->tps_drop_yaw_offset-=4096;
        if(p->lara.health<=0 || p->lara.water_status)p->tps_drop_yaw_offset=0;
    }
    return shooter_input(p,raw,0);
}
static uint32_t shooter_input(TombPlaytest *p,uint32_t raw,int fps){
    TombShooter *s=&p->shooter;TombActor *a=&p->lara.actor;
    if(!s->mode)return raw;
    if(s->mode==TOMB_MODE_VR)return 0;
    if(s->mode==TOMB_MODE_FPS)return fps_input(p,raw);
    int jump=(raw&16)!=0,pressed=jump&&!s->jump_held;s->jump_held=jump;s->input=raw;s->aim_held=(raw&TOMB_INPUT_AIM)!=0;
    /* Action belongs to combat while aiming, and to traversal otherwise. */
    uint32_t out=raw&(64|128);if(s->aim_held)out&=~64u;
    int x=((raw&8)!=0)-((raw&4)!=0),z=((raw&1)!=0)-((raw&2)!=0),moving=x||z;
    int16_t heading=tomb_word(s->yaw+tomb_object_angle(z,x));
    if(p->lara.health<=0)return 0;
    p->movement.turn_rate=0;
    /* Heavy landings own movement until their recovery pose completes. */
    if(!fps && a->animation==24 && !(a->flags&8))return 0;
    if(!fps && (a->current==24 || a->current==32))return out|(jump?16:0);
    if(p->lara.water_status){
        if(a->current==39 || a->current==40 || a->current==55)return out;
        if(p->lara.water_status==1){
            if(!fps && (a->current==35 || ((out&64) && !moving && !jump)))return out;
            double pitch=s->pitch*(6.283185307179586/65536),forward=z*cos(pitch);
            if(moving)heading=tomb_word(s->yaw+(int)(atan2(x,forward)*65536/6.283185307179586));
            int tilt=jump?14560:(int)(atan2(z*sin(pitch),sqrt(x*x+forward*forward))*65536/6.283185307179586);
            if(moving)a->yaw=turn(a->yaw,heading,8192);
            if(moving || jump)p->lara.pitch=turn(p->lara.pitch,(int16_t)tilt,2730);
            if(moving || jump)out|=16;
        }else {
            if(moving){a->yaw=turn(a->yaw,heading,8192);out|=1;}
            /* Looking down and swimming forward dives; Space stays ascent. */
            if(z>0 && s->pitch< -5000 && !jump){anim(p,119);a->goal=17;p->lara.water_status=1;p->lara.pitch=-8190;a->fall_speed=80;out|=16;}
        }
        return out;
    }
    /* Release direction to shorten ordinary jumps, without altering gravity. */
    if(!fps && !p->tps_slope_jump && !moving && (a->flags&8) && (a->current==3 || a->current==11 || (a->current>=25 && a->current<=27)))
        a->speed=(int16_t)(a->current==11 && a->speed>0?(a->speed>6?a->speed-4:2):(a->speed>4?a->speed-4:0));
    /* Never interrupt a grab, climb, interaction, slide, hit or death. */
    int locomotion=(a->animation>=0 && a->animation<=10) || a->animation==20 || a->animation==21 ||
        (a->animation>=38 && a->animation<=41) || (a->animation>=55 && a->animation<=62) ||
        (a->animation>=88 && a->animation<=90);
    int ground=!(a->flags&8) && (locomotion || a->animation==11 || a->animation==103 || (fps && a->animation==24) || a->animation==82 || a->animation==99);
    if(ground){
        int retreat=z<0 && !x && (s->aim_held || p->pistols.drawn);
        if(moving){
            int16_t target=retreat?tomb_word(heading+32768):heading;
            if(!fps && (s->aim_held || p->pistols.drawn)){
                /* Unwrap around camera-forward, routing across Lara's back
                   instead of taking a shorter path through camera-facing. */
                int from=tomb_word(a->yaw-s->yaw),to=tomb_word(target-s->yaw);
                a->yaw=tomb_word(a->yaw+clamp(to-from,-8192,8192));
            }else a->yaw=turn(a->yaw,target,8192);
        }
        else if(s->aim_held || p->pistols.drawn)a->yaw=turn(a->yaw,s->yaw,8192);
        else {int diff=tomb_word(s->yaw-a->yaw);if(abs(diff)>16016)a->yaw=turn(a->yaw,tomb_word(s->yaw+(diff>0?-16016:16016)),4096);}
        if(!fps && moving && pressed && (raw&128) && !p->pistols.drawn && !s->aim_held){
            a->yaw=moving?heading:s->yaw;anim(p,moving?157:156);
            a->flags|=8;a->fall_speed=-100;a->speed=75;
            p->animation.move_angle=p->air.move_angle=a->yaw;
            return out&~128u;
        }
        if(moving && pressed){
            int index=77,offset=0;
            if(p->pistols.drawn && (!x || !z)){
                if(x){index=x<0?79:81;offset=x<0?-16384:16384;}
                else if(z<0){index=75;offset=-32768;}
            }
            a->yaw=offset?s->yaw:heading;anim(p,index);a->flags|=8;a->fall_speed=-100;a->speed=fps?(index==77?75:50):85;
            p->animation.move_angle=p->air.move_angle=tomb_word(a->yaw+offset);
            return out&~128u;
        }
        if(moving){
            int index=(raw&128)?1:0;
            int state=retreat?((raw&128)?16:5):p->level->animations[index].state;
            /* Preserve step poses and their displacement while input is held. */
            if(a->current!=state || (retreat && a->animation!=index))anim(p,index);
            else if(!locomotion)anim(p,index);
            /* Backward collision/control with the forward gait played in reverse. */
            if(retreat)a->current=a->goal=(int16_t)state;
            out|=retreat?2:1;
        }
        else {if(locomotion)anim(p,11);if(pressed)out|=16;}
        p->animation.move_angle=tomb_word(a->yaw+(retreat?32768:0));
    }else if(a->current==10 || a->current==30 || a->current==31 || (a->current>=36 && a->current<=38)){
        /* Keep the original ledge and block commands in their local frame. */
        out|=raw&15;
    }else if(a->current==15){if(jump)out|=16;}
    if(a->flags&8)out&=~128u;
    return out;
}
void tomb_shooter_pose(TombPlaytest *p){
    if(p->shooter.mode!=TOMB_MODE_SHOOTER || p->lara.health<=0 || p->lara.water_status || p->pistols.status)return;
    int state=p->lara.actor.current;
    if(state==0 || state==1 || (p->shooter.input&15) || (p->lara.actor.flags&8)){
        p->lara.head_yaw=p->lara.torso_yaw=0;
        p->lara.head_pitch=p->lara.torso_pitch=0;
        return;
    }
    if(state!=2)return;
    int yaw=clamp(tomb_word(p->shooter.yaw-p->lara.actor.yaw)/2,-8008,8008);
    int pitch=clamp(p->shooter.pitch/2,-4004,7644);
    p->lara.head_yaw=p->lara.torso_yaw=(int16_t)yaw;
    p->lara.head_pitch=p->lara.torso_pitch=(int16_t)pitch;
}
void tomb_shooter_anchor(const TombPlaytest *p,double pos[3]){
    const TombActor *a=&p->lara.actor;
    pos[0]=a->x;pos[1]=a->y;pos[2]=a->z;
    int state=a->current;
    if(p->shooter.mode==TOMB_MODE_FPS && p->fps.mantle_ticks){
        if(p->fps.mantle_ticks==8){for(int j=0;j<3;j++)pos[j]=p->fps.mantle_from[j];}
        return;
    }
    if(p->shooter.mode==TOMB_MODE_FPS && state==19 && (a->animation==97 || a->animation==102)){
        const TombAnim *an=p->level->animations+97;
        double t=a->animation==102?1.0:(double)(a->frame-an->first_frame)/(an->last_frame-an->first_frame+1);
        if(t<0)t=0;if(t>1)t=1;
        double yaw=a->yaw*(6.283185307179586/65536);
        pos[0]+=200*t*sin(yaw);pos[2]+=200*t*cos(yaw);
        if(a->animation==97)pos[1]-=735*t;
        return;
    }
    if((p->shooter.mode!=TOMB_MODE_SHOOTER && !p->fps.block_view) || !p->visual ||
       !(state==10 || state==19 || state==30 || state==31 || (state>=36 && state<=40) || state==54 || state==55))return;
    const TombAnim *an=p->level->animations+a->animation;
    int rate=p->visual->animations[a->animation*32+4],frame=a->frame-an->first_frame;
    if(rate<=0 || frame<0)return;
    int key=frame/rate,span=an->last_frame-an->first_frame-key*rate;
    if(span>rate)span=rate;
    TombPose first,next,idle;
    if(!tomb_visual_key(p->visual,a->animation,key,&first) || !tomb_visual_key(p->visual,11,0,&idle))return;
    next=first;if(span>0)tomb_visual_key(p->visual,a->animation,key+1,&next);
    double blend=span>0?(double)(frame%rate)/span:0,root[3];
    for(int j=0;j<3;j++)root[j]=first.root[j]+blend*(next.root[j]-first.root[j])-idle.root[j];
    double yaw=a->yaw*(6.283185307179586/65536);
    pos[0]+=root[0]*cos(yaw)+root[2]*sin(yaw);pos[1]+=root[1];pos[2]+=root[2]*cos(yaw)-root[0]*sin(yaw);
}
int tomb_shooter_listener(const TombPlaytest *p,double pos[3]){
    /* The shooter view target is a distant aim point, not Lara's position.
       Follow her animated body so climbs retain local sound attenuation. */
    tomb_shooter_anchor(p,pos);pos[1]-=p->shooter.mode==TOMB_MODE_FPS?(p->lara.water_status?220:700):450;
    TombSectorRef ref;
    return tomb_find_sector(p->level,(int32_t)pos[0],(int32_t)pos[1],(int32_t)pos[2],p->lara.actor.room,&ref)?ref.room:p->lara.actor.room;
}
void tomb_shooter_view(const TombPlaytest *p,const double pos[3],double eye[3],double target[3],int *room){
    tomb_shooter_view_blend(p,pos,p->fps.block_view,eye,target,room);
}
void tomb_shooter_view_blend(const TombPlaytest *p,const double pos[3],double block_view,double eye[3],double target[3],int *room){
    double yaw=p->shooter.yaw*(6.283185307179586/65536),pitch=p->shooter.pitch*(6.283185307179586/65536);
    double direction[3]={sin(yaw)*cos(pitch),-sin(pitch),cos(yaw)*cos(pitch)};
    TombCameraPoint pivot={(int32_t)pos[0],(int32_t)pos[1]-(p->lara.water_status?220:850),(int32_t)pos[2],p->lara.actor.room};
    if(p->shooter.mode==TOMB_MODE_FPS && block_view<=0){
        eye[0]=pos[0];eye[1]=pos[1]-(p->lara.water_status?220:700);eye[2]=pos[2];
        for(int j=0;j<3;j++)target[j]=eye[j]+direction[j]*4096;
        *room=p->lara.actor.room;return;
    }
    if(p->shooter.mode==TOMB_MODE_SHOOTER){
        /* Animation root travel can lift the camera pivot through a ceiling
           before the actor changes rooms. A ray starting in solid space cannot
           retract the camera: first keep its origin inside the open column. */
        TombSectorRef ref;TombHeights h;
        if(tomb_find_sector(p->level,pivot.x,pivot.y,pivot.z,pivot.room,&ref) &&
           (p->level->items?tomb_item_heights(p->level,ref,pivot.x,pivot.y,pivot.z,0,
               p->level->items,p->level->item_count,&h):tomb_static_heights(p->level,ref,pivot.x,pivot.z,0,&h)) &&
           h.floor!=-32512 && h.ceiling!=-32512 && h.floor>h.ceiling){
            int margin=(h.floor-h.ceiling)/2;if(margin>48)margin=48;
            pivot.y=clamp(pivot.y,h.ceiling+margin,h.floor-margin);
            pivot.room=(int16_t)ref.room;
            if(tomb_find_sector(p->level,pivot.x,pivot.y,pivot.z,pivot.room,&ref))pivot.room=(int16_t)ref.room;
        }
    }
    TombCameraPoint desired={pivot.x-(int32_t)(direction[0]*1350),pivot.y-(int32_t)(direction[1]*1350),pivot.z-(int32_t)(direction[2]*1350),pivot.room};
    tomb_shooter_world_ray(p,pivot,&desired);
    /* Leave a small near-plane margin at an obstruction. */
    double dx=desired.x-pivot.x,dy=desired.y-pivot.y,dz=desired.z-pivot.z,len=sqrt(dx*dx+dy*dy+dz*dz),scale=len>64?(len-48)/len:0;
    eye[0]=pivot.x+dx*scale;eye[1]=pivot.y+dy*scale;eye[2]=pivot.z+dz*scale;
    if(p->shooter.mode==TOMB_MODE_FPS){
        double blend=block_view/8.0;
        if(blend<0)blend=0;if(blend>1)blend=1;
        blend=blend*blend*(3-2*blend);
        for(int j=0;j<3;j++){
            double first=pos[j]-(j==1?700:0);
            eye[j]=first+(eye[j]-first)*blend;
        }
    }
    for(int j=0;j<3;j++)target[j]=eye[j]+direction[j]*4096;
    /* The near-plane margin can move the eye back across a portal. Visibility
       must start in the eye's room, not the obstruction endpoint's room. */
    TombSectorRef eye_ref;
    *room=tomb_find_sector(p->level,(int32_t)eye[0],(int32_t)eye[1],(int32_t)eye[2],pivot.room,&eye_ref)?eye_ref.room:pivot.room;
}
int tomb_shooter_ray(TombPlaytest *p,TombCameraPoint origin,int16_t yaw,int16_t pitch,TombCameraPoint *hit){
    double y=yaw*(6.283185307179586/65536),t=pitch*(6.283185307179586/65536);
    double d[3]={sin(y)*cos(t),-sin(t),cos(y)*cos(t)};
    *hit=(TombCameraPoint){origin.x+(int32_t)(d[0]*16384),origin.y+(int32_t)(d[1]*16384),origin.z+(int32_t)(d[2]*16384),origin.room};
    tomb_shooter_world_ray(p,origin,hit);
    double dx=hit->x-origin.x,dy=hit->y-origin.y,dz=hit->z-origin.z,best=sqrt(dx*dx+dy*dy+dz*dz);int found=-1;
    if(!p->objects || !p->objects->enemies)return -1;
    TombEnemies *e=p->objects->enemies;int32_t o[3]={origin.x,origin.y,origin.z};
    for(int id=e->head;id>=0;id=e->next[id]){
        TombObject *item=p->objects->items+id;TombCreature *c=e->items+id;
        if(c->health<=0 || (item->actor.flags&6)==6)continue;
        TombSphere spheres[64];int n=tomb_object_spheres_joints(p->visual,item->object,&item->actor,c->pitch,c->roll,c->head,c->neck,o,yaw,pitch,p->animation.sine_quarter,spheres);
        for(int j=0;j<n;j++){
            TombSphere b=spheres[j];double r2=(double)b.radius*b.radius-(double)b.x*b.x-(double)b.y*b.y;if(r2<0)continue;
            double near=b.z-sqrt(r2);if(near<0)near=b.z+sqrt(r2);
            if(near>=0 && near<best){best=near;found=id;}
        }
    }
    if(found>=0)*hit=(TombCameraPoint){origin.x+(int32_t)(d[0]*best),origin.y+(int32_t)(d[1]*best),origin.z+(int32_t)(d[2]*best),p->objects->items[found].actor.room};
    return found;
}
void tomb_shooter_target(TombPlaytest *p){
    double pos[3],eye[3],target[3];int room;tomb_shooter_anchor(p,pos);
    tomb_shooter_view(p,pos,eye,target,&room);
    p->shooter.eye=(TombCameraPoint){(int32_t)eye[0],(int32_t)eye[1],(int32_t)eye[2],(int16_t)room};
    tomb_shooter_ray(p,p->shooter.eye,p->shooter.yaw,p->shooter.pitch,&p->shooter.target);
    TombActor *a=&p->lara.actor;int16_t angles[2];
    tomb_gun_angles(p->shooter.target.x-a->x,p->shooter.target.y-(a->y-650),p->shooter.target.z-a->z,angles);
    int yaw=tomb_word(angles[0]-a->yaw),pitch=angles[1];TombPistols *g=&p->pistols;
    /* Sentinel suppresses DOS free fire for an arm outside its aim cone. */
    g->target=-2;g->target_yaw=(int16_t)yaw;g->target_pitch=(int16_t)pitch;
    if(p->weapon_type==4)g->left.lock=g->right.lock=abs(yaw)<=14560 && abs(pitch)<=11830;
    else {g->left.lock=yaw>=-30940 && yaw<=10920 && abs(pitch)<=14560;g->right.lock=yaw>=-10920 && yaw<=30940 && abs(pitch)<=14560;}
    if(g->left.lock){g->left.yaw=(int16_t)yaw;g->left.pitch=(int16_t)pitch;}
    if(g->right.lock){g->right.yaw=(int16_t)yaw;g->right.pitch=(int16_t)pitch;}
}
