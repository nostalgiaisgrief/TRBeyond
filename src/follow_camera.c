#include "follow_camera.h"
#include "object_contact.h"
#include "fixed.h"
#include <math.h>
#include <string.h>
static double length(const double a[3],const double b[3]) {
    double x=a[0]-b[0],y=a[1]-b[1],z=a[2]-b[2];return sqrt(x*x+y*y+z*z);
}
static double clamp(double v,double lo,double hi) { return fmax(lo,fmin(hi,v)); }
int tomb_camera_clear(const TombLevel *l,const double p[3],int room,double radius,int *resolved) {
    if(!l || !p || room<0 || (size_t)room>=l->room_count || radius<0) return 0;
    for(int i=0;i<3;i++)if(!isfinite(p[i]) || fabs(p[i])>1000000000)return 0;
    int centre=room;
    /* Square clearance covers the near plane, including its diagonal corners. */
    for(int sample=0;sample<9;sample++) {
        double x=p[0]+(sample%3-1)*radius,z=p[2]+(sample/3-1)*radius;
        TombSectorRef ref;TombHeights h;
        if(!tomb_find_sector(l,(int32_t)floor(x),(int32_t)floor(p[1]),(int32_t)floor(z),room,&ref) ||
           !tomb_static_heights(l,ref,(int32_t)floor(x),(int32_t)floor(z),0,&h))return 0;
        const TombRoom *r=&l->rooms[ref.room];
        if(x<r->x || x>=r->x+r->nx*1024.0 || z<r->z || z>=r->z+r->nz*1024.0 ||
           h.floor==-32512 || h.ceiling==-32512 || h.object_references || p[1]-radius<h.ceiling || p[1]+radius>h.floor)return 0;
        if(sample==4)centre=ref.room;
    }
    /* Static collision boxes are oriented in original Y-down world space. */
    for(size_t r=0;r<l->room_count;r++)for(size_t i=0;i<l->rooms[r].static_count;i++) {
        const TombStaticPlacement *s=&l->rooms[r].statics[i];const TombStaticDef *d=NULL;
        for(size_t j=0;j<l->static_def_count;j++)if(l->static_defs[j].id==s->id){d=&l->static_defs[j];break;}
        if(!d || (d->flags&1))continue;
        if(p[1]+radius<s->y+d->bounds[2] || p[1]-radius>s->y+d->bounds[3])continue;
        double angle=s->rotation*(6.28318530717958647692/65536.0),sn=sin(angle),cs=cos(angle);
        double dx=p[0]-s->x,dz=p[2]-s->z;
        double v[3]={cs*dx-sn*dz,p[1]-s->y,sn*dx+cs*dz},distance2=0;
        for(int j=0;j<3;j++){double delta=v[j]-clamp(v[j],d->bounds[j*2],d->bounds[j*2+1]);distance2+=delta*delta;}
        if(distance2<radius*radius)return 0;
    }
    if(resolved)*resolved=centre;return 1;
}
/* DOS 0x13978: adjust two horizontal coordinates around a box edge.
   Bounds are ordered by the side of the target, not numerically sorted. */
void tomb_camera_box_shift(double *p,double *q,double tp,double tq,
                           double bound,double near,double opposite,double far,double r2) {
    double u=(tp-bound)*(tp-bound),v=(tq-near)*(tq-near);
    double a=u+v,b=u+(tq-far)*(tq-far),c=(tp-opposite)*(tp-opposite)+v;
    double remaining=r2-u;
    if(r2<a) {
        *p=bound;if(remaining>=0)*q=tq+(near<far?-1:1)*floor(sqrt(remaining));
    } else if(a>65536) {*p=bound;*q=near;}
    else if(r2<b) {
        *p=bound;if(remaining>=0)*q=tq+(near<far?1:-1)*floor(sqrt(remaining));
    } else if(b>65536) {*p=bound;*q=far;}
    else if(r2<c) {
        remaining=r2-v;
        if(remaining>=0){*p=tp+(tp<opposite?1:-1)*floor(sqrt(remaining));*q=near;}
    } else {*p=opposite;*q=near;}
}

TombCameraRequest tomb_camera_control(int state,int water,int16_t pitch) {
    TombCameraRequest r={1536,0,12,0,0,pitch,-1,0};
    if(state==46){r.flags=1;r.angle=30940;r.elevation=-4550;}
    else if(state==39){r.angle=-23660;r.elevation=-2730;r.distance=1024;}
    else if(state==42 || state==43){r.angle=-14560;r.elevation=-4550;r.distance=1024;}
    else if(state==40 || state==41){r.angle=14560;r.elevation=-4550;r.distance=1024;}
    else if(water==2)r.elevation=-4004;
    else if(!water) {
        if(state==10 || state==30 || state==31)r.elevation=-10920;
        else if(state==11)r.angle=15470;
        else if(state==24){r.flags=2;r.elevation=-8190;}
        else if(state==25)r.angle=24570;
        else if(state==36 || state==37){r.flags=1;r.angle=6370;r.elevation=-4550;}
        else if(state==38)r.angle=13650;
        else if(state==55)r.flags=1;
    }
    return r;
}
int tomb_camera_tick_effects(TombFollowCamera *c,const TombLevel *l,const TombVisual *v,const TombActor *a,int object,const int16_t *sine,const TombCameraRequest *r,int *bounce,uint32_t *random) {
    int16_t bounds[6];if(!tomb_visual_item_bounds(v,object,a,bounds))return 0;
    int first=!c->ready;int cut=first || c->dos.fixed!=(r->fixed_index>=0 && r->item_target);if(first)c->dos.ready=0;
    /* A speed-one fixed shot snaps the eye, not its moving look-at target.
       Resetting both every tick puts the aim ahead of interpolated actors. */
    int cut_eye=cut || (r->fixed_index>=0 && r->speed==1);
    memcpy(c->previous_eye,c->eye,sizeof c->eye);memcpy(c->previous_target,c->target,sizeof c->target);
    if(!tomb_dos_camera_tick_effects(&c->dos,l,a,bounds,sine,r,bounce,random))return 0;
    c->eye[0]=c->dos.eye.x;c->eye[1]=c->dos.eye.y+c->dos.shift;c->eye[2]=c->dos.eye.z;
    c->target[0]=c->dos.target.x;c->target[1]=c->dos.target.y;c->target[2]=c->dos.target.z;
    c->room=c->dos.eye.room;c->fixed_target=c->dos.fixed;c->ready=1;c->boom=length(c->eye,c->target);
    if(cut_eye)memcpy(c->previous_eye,c->eye,sizeof c->eye);
    if(cut)memcpy(c->previous_target,c->target,sizeof c->target);
    return 1;
}
/* Compatibility driver for tests/tools. The live preview supplies the original
   sine asset and calls tomb_camera_tick exactly once per gameplay tick. */
static int drive(TombFollowCamera *c,const TombLevel *l,const TombVisual *v,const TombActor *a,int object,TombCameraRequest r,double orbit,double distance,double dt) {
    if(!isfinite(dt) || dt<0 || dt>.1 || !isfinite(orbit) || !isfinite(distance))return 0;
    int16_t sine[1025];for(int i=0;i<=1024;i++)sine[i]=(int16_t)floor(sin(i*3.14159265358979323846/2048)*16384+.5);
    r.angle=tomb_word((int)r.angle+(int)(orbit*10430.378350470453));
    if(distance!=1536)r.distance=(int32_t)distance;
    c->pending+=dt;
    while(c->pending+1e-9>=1.0/30){if(!tomb_camera_tick(c,l,v,a,object,sine,&r))return 0;c->pending-=1.0/30;}
    return 1;
}
int tomb_follow_camera(TombFollowCamera *c,const TombLevel *l,const TombActor *a,const TombVisual *v,int16_t pitch,int water,double orbit,double distance,double dt) {
    return drive(c,l,v,a,0,tomb_camera_control(a->current,water,pitch),orbit,distance,dt);
}
int tomb_scene_camera(TombFollowCamera *c,const TombObjects *w,const TombActor *a,int16_t pitch,int water,double orbit,double distance,double dt) {
    TombCameraRequest r=tomb_camera_control(a->current,water,pitch);TombActor focus=*a;int object=0;
    if(w->camera.active){r.fixed_index=(int16_t)w->camera.index;r.speed=w->camera.speed;
        if(w->camera.target>=0){size_t id=(size_t)w->camera.target;const TombItem *p=w->level->items+id;TombModel model;
            if(!tomb_visual_model(w->visual,p->object,&model))return 0;
            object=p->object;r.item_target=1;focus=(TombActor){0};focus.x=p->x;focus.y=p->y;focus.z=p->z;focus.room=p->room;focus.yaw=p->yaw;
            focus.animation=(int16_t)model.animation;focus.frame=w->level->animations[model.animation].first_frame;
            if(object==55 || (object>=57 && object<=64))focus=w->items[id].actor;
        }
    }
    return drive(c,w->level,w->visual,&focus,object,r,orbit,distance,dt);
}

/* CalculateCamera 0x146f8..0x1487b: head/torso auto-look toward an item. */
int tomb_camera_interest(const TombActor *a,const int16_t ab[6],const TombActor *o,const int16_t ob[6],int16_t *yaw,int16_t *pitch){
 int32_t dx=tomb_long((int64_t)o->x-a->x),dz=tomb_long((int64_t)o->z-a->z);
 uint32_t squared=(uint32_t)((int64_t)dx*dx+(int64_t)dz*dz);
 int distance=(int)sqrt((double)squared);
 int head=tomb_asr(tomb_word(tomb_object_angle(dz,dx)-a->yaw),1);
 int y=a->y+ab[3]+tomb_asr(3*(ab[2]-ab[3]),2),oy=o->y+(ob[2]+ob[3])/2;
 int tilt=tomb_asr(tomb_object_angle(distance,tomb_long((int64_t)y-oy)),1);
 if(head<=-9100 || head>=9100 || tilt<=-15470 || tilt>=15470)return 0;
 int d=tomb_word(head-*yaw);*yaw=tomb_word(*yaw+(d>728?728:d< -728?-728:d));
 d=tomb_word(tilt-*pitch);*pitch=tomb_word(*pitch+(d>728?728:d< -728?-728:d));return 1;
}

int tomb_camera_tick(TombFollowCamera *c,const TombLevel *l,const TombVisual *v,const TombActor *a,int object,const int16_t *sine,const TombCameraRequest *r){return tomb_camera_tick_effects(c,l,v,a,object,sine,r,NULL,NULL);}
