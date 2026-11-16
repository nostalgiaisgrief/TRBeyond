/* TR2 chase camera. Boundary/box algorithm adapted from our independently
   reconstructed Beyond camera, then compared with this TR2 executable. */
#include "tr2_combat.h"
#include "tr2_world.h"
#include <math.h>
#include <stdlib.h>
typedef struct {int zmin,zmax,xmin,xmax;} CameraBox;
static int32_t mul(int32_t a,int32_t b){return tr2_long((int64_t)a*b);}
/* CalculateCamera 411c27..411cea: scripted target head/torso tracking. */
int tr2_scripted_look_angles(int yaw,int pitch,Tr2Look *look){
    yaw=tr2_asr(tr2_word(yaw),1);pitch=tr2_asr(tr2_word(pitch),1);
    if(yaw<=-9100 || yaw>=9100 || pitch<=-15470 || pitch>=15470)return 0;
    int dy=tr2_word(yaw-look->yaw),dp=tr2_word(pitch-look->pitch);
    look->yaw=tr2_word(look->yaw+(dy>728?728:dy< -728?-728:dy));
    look->pitch=tr2_word(look->pitch+(dp>728?728:dp< -728?-728:dp));
    look->torso_yaw=look->yaw;look->torso_pitch=look->pitch;return 1;
}
int tr2_scripted_look(const Tr2Level *l,const Tr2Actor *a,const Tr2Actor *target,Tr2Look *look){
    int b[6],y=a->y-768,ty=target->y;
    if(tr2_actor_bounds(l,a,b))y=a->y+b[3]+tr2_asr(3*(b[2]-b[3]),2);
    if(tr2_actor_bounds(l,target,b))ty+=(b[2]+b[3])/2;
    int dx=target->x-a->x,dz=target->z-a->z;
    int distance=(int)sqrt((double)((int64_t)dx*dx+(int64_t)dz*dz));
    return tr2_scripted_look_angles(tr2_word(tr2_atan(dz,dx)-a->yaw),tr2_atan(distance,y-ty),look);
}
static int heights(const Tr2Level *l,Tr2Position *p,int heavy,Tr2Height *h) {
    Tr2Sector ref;
    if(!tr2_find_sector(l,p->x,p->y,p->z,p->room,&ref) || !tr2_heights(l,ref,p->x,p->y,p->z,heavy,h))return 0;
    p->room=(int16_t)ref.room;return 1;
}
/* X/Z LOS share the same boundary walk, but both execute in the original
   minor-axis-first order and may shorten the endpoint. */
static int axis_los(const Tr2Level *l,const Tr2Position *s,Tr2Position *t,int axis,int heavy,int *rooms,int *count) {
    int32_t start=axis?s->z:s->x,end=axis?t->z:t->x,delta=end-start;
    if(!delta)return 1;
    if(count){*count=1;rooms[0]=s->room;}
    int sign=delta<0?-1:1;
    int32_t side=axis?s->x:s->z,step=mul((axis?t->x:t->z)-side,1024)/delta;
    int32_t ystep=mul(t->y-s->y,1024)/delta;
    int32_t edge=delta<0?(start&~1023):(start|1023);
    int32_t other=side+tr2_asr(mul(edge-start,step),10),y=s->y+tr2_asr(mul(edge-start,ystep),10);
    int16_t room=s->room;
    while(sign<0?edge>end:edge<end) {
        Tr2Position p={axis?other:edge,y,axis?edge:other,room,0};Tr2Height h;
        if(!heights(l,&p,heavy,&h))return 0;
        room=p.room;
        if(y>h.floor || y<h.ceiling){*t=p;return -1;}
        if(count && rooms[*count-1]!=room && *count<1024)rooms[(*count)++]=room;
        int16_t before=room;
        if(axis)p.z+=sign;else p.x+=sign;
        if(!heights(l,&p,heavy,&h))return 0;
        room=p.room;
        if(y>h.floor || y<h.ceiling){*t=(Tr2Position){axis?other:edge,y,axis?edge:other,before,0};return 0;}
        edge+=sign*1024;other+=sign*step;y+=sign*ystep;
    }
    t->room=room;return 1;
}
int tr2_shot_los(const Tr2Level *l,const Tr2Position *s,Tr2Position *t,int heavy,int *rooms,int *count) {
    int first_axis=llabs((long long)t->z-s->z)>llabs((long long)t->x-s->x)?0:1;
    int first=axis_los(l,s,t,first_axis,heavy,rooms,count),second=axis_los(l,s,t,!first_axis,heavy,rooms,count);
    if(!second)return 0;
    Tr2Height h;if(!heights(l,t,heavy,&h))return 0;
    int32_t y=t->y,plane;
    if(h.floor<y && h.floor>s->y)plane=h.floor;
    else if(h.ceiling>y && h.ceiling<s->y)plane=h.ceiling;
    else return first==1 && second==1;
    t->x=s->x+mul(t->x-s->x,plane-s->y)/(y-s->y);
    t->z=s->z+mul(t->z-s->z,plane-s->y)/(y-s->y);t->y=plane;return 0;
}
int tr2_camera_los(const Tr2Level *l,const Tr2Position *s,Tr2Position *t,int heavy){return tr2_shot_los(l,s,t,heavy,NULL,NULL);}
static const CameraBox *box(const Tr2Level *l,const Tr2Position *p) {
    static CameraBox result[8];static unsigned cursor;CameraBox *b=&result[cursor++&7];
    if(p->room<0 || (uint32_t)p->room>=l->room_count)return NULL;
    const Tr2Room *r=l->rooms+p->room;int x=tr2_asr(p->x-r->x,10),z=tr2_asr(p->z-r->z,10);
    if(x<0 || z<0 || x>=r->nx || z>=r->nz)return NULL;
    unsigned id=tr2_u16(r->sectors.data+(x*r->nz+z)*8+2);if(id>=l->boxes.count)return NULL;
    const uint8_t *raw=l->boxes.data+id*8;
    *b=(CameraBox){raw[0]*1024,raw[1]*1024-1,raw[2]*1024,raw[3]*1024-1};return b;
}
static int32_t square(int32_t x){return mul(x,x);}
static int good(const Tr2Level *l,Tr2Position p){Tr2Height h;return heights(l,&p,0,&h) && p.y<=h.floor && p.y>=h.ceiling;}
int tr2_camera_clamp(const Tr2Level *l,Tr2Position *p,int margin){
    Tr2Position old=*p;Tr2Height h;if(!heights(l,p,0,&h))return 0;
    const CameraBox *b=box(l,p);if(b){CameraBox bounds=*b;Tr2Position check=old;check.room=p->room;
        check.z=old.z-margin;if(old.z<bounds.zmin+margin && !good(l,check))p->z=bounds.zmin+margin;
        else{check.z=old.z+margin;if(old.z>bounds.zmax-margin && !good(l,check))p->z=bounds.zmax-margin;}
        check.z=old.z;check.x=old.x-margin;if(old.x<bounds.xmin+margin && !good(l,check))p->x=bounds.xmin+margin;
        else{check.x=old.x+margin;if(old.x>bounds.xmax-margin && !good(l,check))p->x=bounds.xmax-margin;}
    }
    int floor=h.floor-margin,ceiling=h.ceiling+margin;if(floor<ceiling)floor=ceiling=tr2_asr(floor+ceiling,1);
    return old.y>floor?floor-old.y:old.y<ceiling?ceiling-old.y:0;
}
uint32_t tr2_look_input(Tr2Look *v,const Tr2Actor *a,uint32_t input){
    int state=a->control.current;
    int held=(input&TR2_LOOK)!=0;
    /* 4275bb, 431768, 432057: horizontal Look reads the permission left
       by the previous state handler. Vertical Look is called by the current handler. */
    int horizontal=held && !v->blocked_next_tick &&
        (!a->extra_animation || a->control.water_status==1 || a->control.water_status==2);
    int rolling=(input&TR2_ROLL)!=0;
    int vertical=held && !a->extra_animation && (state==10 || state==56 ||
        (a->control.health>0 && ((state==2 && (!rolling || a->control.water_status==4)) ||
                                 (state==13 && !rolling))));
    v->blocked_next_tick=0;
    if(!a->extra_animation)switch(state){
        case 8: case 12: case 21: case 22: case 26: case 27: case 36: case 37:
        case 39: case 40: case 41: case 42: case 43: case 44: case 67:
            v->blocked_next_tick=1;break;
    }
    v->active=horizontal || vertical;
    if(horizontal){
        if(input&TR2_LEFT){input&=~TR2_LEFT;if(v->yaw>-8008)v->yaw-=364;}
        else if(input&TR2_RIGHT){input&=~TR2_RIGHT;if(v->yaw<8008)v->yaw+=364;}
        if(a->gun_status!=1)v->torso_yaw=v->yaw;
    }else{
        v->yaw=(int16_t)(abs(v->yaw)<364?0:v->yaw-v->yaw/8);v->pitch=(int16_t)(abs(v->pitch)<364?0:v->pitch-v->pitch/8);
        v->torso_yaw=v->yaw;v->torso_pitch=v->pitch;
    }
    if(vertical){
        if(input&TR2_FORWARD){input&=~TR2_FORWARD;if(v->pitch>-7644)v->pitch-=364;}
        else if(input&TR2_BACK){input&=~TR2_BACK;if(v->pitch<4004)v->pitch+=364;}
        if(a->gun_status!=1)v->torso_pitch=v->pitch;
    }
    return input;
}
static void shift(int32_t *p,int32_t *q,int32_t tp,int32_t tq,int32_t bound,int32_t near,int32_t opposite,int32_t far,int32_t r2) {
    int32_t u=square(tp-bound),v=square(tq-near),a=u+v,b=u+square(tq-far),c=square(tp-opposite)+v;
    if(r2<a){*p=bound;if(r2>=u)*q=tq+(near<far?-1:1)*(int32_t)sqrt((double)(r2-u));}
    else if(a>116281){*p=bound;*q=near;}
    else if(r2<b){*p=bound;if(r2>=u)*q=tq+(near<far?1:-1)*(int32_t)sqrt((double)(r2-u));}
    else if(2*r2<c){if(2*r2>=v){*p=tp+(bound<opposite?1:-1)*(int32_t)sqrt((double)(2*r2-v));*q=near;}}
    else if(b>c){*p=bound;*q=far;}
    else {*p=opposite;*q=near;}
}
/* Clip the view ray for look mode. */
static void clip(int32_t *p,int32_t *q,int32_t tp,int32_t tq,int32_t bound,int32_t near,int32_t opposite,int32_t far,int32_t *y,int32_t ty) {
    if((opposite>bound)!=(tp<bound)) {
        if(*p!=tp){*q=tq+mul(bound-tp,*q-tq)/(*p-tp);*y=ty+mul(bound-tp,*y-ty)/(*p-tp);}
        *p=bound;
    }
    if((near<far && tq>near && near>*q) || (near>far && tq<near && near<*q)) {
        if(*q!=tq){*p=tp+mul(near-tq,*p-tp)/(*q-tq);*y=ty+mul(near-tq,*y-ty)/(*q-tq);}
        *q=near;
    }
}
static int adjust(const Tr2Level *l,const Tr2Position *target,const Tr2Position *eye,Tr2Position *p,int32_t r2,int heavy,int looking) {
    tr2_camera_los(l,target,p,heavy);
    const CameraBox *b=box(l,target),*ib=box(l,p);
    if(!b)return 0;
    if(ib && (p->z<b->zmin || p->z>b->zmax || p->x<b->xmin || p->x>b->xmax))b=ib;
    int32_t edges[4]={b->zmin,b->zmax,b->xmin,b->xmax};int blocked[4];
    for(int side=0;side<4;side++) {
        Tr2Position n=*p;int32_t *coord=side<2?&n.z:&n.x;
        *coord=side%2?(*coord+1024)&~1023:(*coord-1024)|1023;
        Tr2Height h;blocked[side]=!heights(l,&n,heavy,&h) || n.y>h.floor || n.y<h.ceiling;
        if(blocked[side])edges[side]=*coord;
        if(!blocked[side]) {
            const CameraBox *adj=box(l,&n);
            if(adj){int32_t edge=side==0?adj->zmin:side==1?adj->zmax:side==2?adj->xmin:adj->xmax;
                if(side%2?edge>edges[side]:edge<edges[side])edges[side]=edge;}
        }
    }
    int32_t zmin=edges[0]+256,zmax=edges[1]-256,xmin=edges[2]+256,xmax=edges[3]-256;
    int zside=p->z<zmin && blocked[0]?0:p->z>zmax && blocked[1]?1:-1;
    int xside=p->x<xmin && blocked[2]?0:p->x>xmax && blocked[3]?1:-1;
    int major_z=abs(p->z-target->z)>abs(p->x-target->x);
    int axis=major_z?(zside>=0?0:xside>=0?1:-1):(xside>=0?1:zside>=0?0:-1);
    if(axis<0)return 1;
    Tr2Position first=*p,second=*p;int preferred;
    if(!axis){
        preferred=major_z?eye->x<target->x:p->x<target->x;
        int bound=zside?zmax:zmin,opposite=zside?zmin:zmax;
        if(looking)clip(&first.z,&first.x,target->z,target->x,bound,xmin,opposite,xmax,&first.y,target->y);else shift(&first.z,&first.x,target->z,target->x,bound,xmin,opposite,xmax,r2);
        if(looking)clip(&second.z,&second.x,target->z,target->x,bound,xmax,opposite,xmin,&second.y,target->y);else shift(&second.z,&second.x,target->z,target->x,bound,xmax,opposite,xmin,r2);
    }else{
        preferred=!major_z?eye->z<target->z:p->z<target->z;
        int bound=xside?xmax:xmin,opposite=xside?xmin:xmax;
        if(looking)clip(&first.x,&first.z,target->x,target->z,bound,zmin,opposite,zmax,&first.y,target->y);else shift(&first.x,&first.z,target->x,target->z,bound,zmin,opposite,zmax,r2);
        if(looking)clip(&second.x,&second.z,target->x,target->z,bound,zmax,opposite,zmin,&second.y,target->y);else shift(&second.x,&second.z,target->x,target->z,bound,zmax,opposite,zmin,r2);
    }
    if(preferred){if(!tr2_camera_los(l,target,&first,heavy))preferred=0;}
    else if(!tr2_camera_los(l,target,&second,heavy))preferred=1;
    /* Original copies coordinates, retaining the prior room until GetFloor. */
    Tr2Position chosen=preferred?first:second;p->x=chosen.x;p->y=chosen.y;p->z=chosen.z;
    Tr2Height h;return heights(l,p,heavy,&h);
}
int tr2_camera_adjust(const Tr2Level *l,const Tr2Position *target,Tr2Position *p,int32_t r2,int heavy) {
    return adjust(l,target,target,p,r2,heavy,0);
}

int tr2_camera_move(const Tr2Level *l,Tr2Camera *c,Tr2Position ideal,int speed){
    if(speed<1)return 0;
    c->position.x+=(ideal.x-c->position.x)/speed;c->position.y+=(ideal.y-c->position.y)/speed;c->position.z+=(ideal.z-c->position.z)/speed;c->position.room=ideal.room;
    Tr2Height h;if(!heights(l,&c->position,0,&h))return 0;
    int floor=h.floor-256;
    if(floor<=c->position.y && floor<=ideal.y){tr2_camera_los(l,&c->target,&c->position,0);if(!heights(l,&c->position,0,&h))return 0;floor=h.floor-256;}
    int ceiling=h.ceiling+256;if(floor<ceiling)floor=ceiling=tr2_asr(floor+ceiling,1);
    Tr2World *w=l->world;
    if(w && w->camera_bounce){
        if(w->camera_bounce>0){c->position.y+=w->camera_bounce;c->target.y+=w->camera_bounce;w->camera_bounce=0;}
        else{
            int delta[3];for(int i=0;i<3;i++){w->random_state=w->random_state*0x41c64e6du+12345;delta[i]=tr2_long(((int)((w->random_state>>10)&32767)-16384)*(int64_t)w->camera_bounce)/32767;}
            /* Root MoveCamera adds the X jitter to target Y as well. */
            c->position.x+=delta[0];c->target.y+=delta[0];c->position.y+=delta[1];c->target.y+=delta[1];c->position.z+=delta[2];c->target.z+=delta[2];w->camera_bounce+=5;
        }
    }
    c->shift=floor<c->position.y?floor-c->position.y:ceiling>c->position.y?ceiling-c->position.y:0;
    Tr2Position displayed=c->position;displayed.y+=c->shift;
    if(!heights(l,&displayed,0,&h))return 0;c->position.room=displayed.room;return 1;
}
int tr2_camera_chase(const Tr2Level *l,const Tr2Actor *a,Tr2Camera *c,int chunky){
    int elevation=tr2_word(c->elevation+a->pitch);if(elevation>15470)elevation=15470;if(elevation< -15470)elevation=-15470;
    c->elevation=(int16_t)elevation;
    int horizontal=tr2_asr(mul(c->distance,tr2_cos(elevation)),14),angle=tr2_word(a->yaw+c->angle);
    Tr2Position ideal={c->target.x-tr2_asr(mul(horizontal,tr2_sin(angle)),14),c->target.y+tr2_asr(mul(c->distance,tr2_sin(elevation)),14),c->target.z-tr2_asr(mul(horizontal,tr2_cos(angle)),14),c->position.room,0};
    c->chase_radius_squared=square(horizontal);
    if(!adjust(l,&c->target,&c->position,&ideal,c->chase_radius_squared,chunky,0))return 0;
    return tr2_camera_move(l,c,ideal,c->speed);
}
int tr2_camera_look(const Tr2Level *l,const Tr2Actor *a,Tr2Camera *c,const Tr2Look *v){
    int oldx=c->target.x,oldz=c->target.z;
    int angle=tr2_word(a->yaw+v->yaw+v->torso_yaw),elevation=tr2_word(a->pitch+v->pitch+v->torso_pitch);
    c->angle=(int16_t)angle;c->elevation=(int16_t)elevation;c->distance=1536;
    int horizontal=tr2_asr(mul(1536,tr2_cos(elevation)),14),offset=tr2_asr(-512*tr2_sin(elevation),14);
    c->target.x=a->x+tr2_asr(mul(offset,tr2_sin(a->yaw)),14);c->target.z=a->z+tr2_asr(mul(offset,tr2_cos(a->yaw)),14);
    if(!good(l,c->target)){c->target.x=a->x;c->target.z=a->z;}
    c->target.y+=tr2_camera_clamp(l,&c->target,306);
    Tr2Position ideal={c->target.x-tr2_asr(mul(horizontal,tr2_sin(angle)),14),c->target.y+tr2_asr(mul(1536,tr2_sin(elevation)),14),c->target.z-tr2_asr(mul(horizontal,tr2_cos(angle)),14),c->position.room,0};
    if(!adjust(l,&c->target,&c->position,&ideal,square(horizontal),0,1))return 0;
    if(c->speed<1)c->speed=4;c->target.x=oldx+(c->target.x-oldx)/c->speed;c->target.z=oldz+(c->target.z-oldz)/c->speed;
    return tr2_camera_move(l,c,ideal,c->speed);
}
/* CombatCamera 0x411660 in the root executable. */
int tr2_camera_combat(const Tr2Level *l,const Tr2Actor *a,Tr2Camera *c,const Tr2Combat *g,const Tr2Look *look){
    int yaw=g->target>=0?g->yaw:(look?look->yaw:0)+g->torso_yaw;
    int pitch=g->target>=0?g->pitch:(look?look->pitch:0)+g->torso_pitch;
    c->angle=tr2_word(a->yaw+yaw);c->elevation=tr2_word(a->pitch+pitch);c->distance=2560;
    c->target.x=a->x;c->target.z=a->z;
    int horizontal=tr2_asr(mul(c->distance,tr2_cos(c->elevation)),14);
    Tr2Position ideal={a->x-tr2_asr(mul(horizontal,tr2_sin(c->angle)),14),c->target.y+tr2_asr(mul(c->distance,tr2_sin(c->elevation)),14),a->z-tr2_asr(mul(horizontal,tr2_cos(c->angle)),14),c->position.room,0};
    int surface=a->y+a->water_distance;
    if(a->control.water_status==1 && c->target.y>surface && surface>ideal.y){
        ideal.x=c->target.x+mul(ideal.x-c->target.x,surface-c->target.y)/(ideal.y-c->target.y);
        ideal.z=c->target.z+mul(ideal.z-c->target.z,surface-c->target.y)/(ideal.y-c->target.y);
        ideal.y=surface;
    }
    if(!adjust(l,&c->target,&c->position,&ideal,c->chase_radius_squared,1,0))return 0;
    return tr2_camera_move(l,c,ideal,c->speed);
}
/* In-game cinematic camera 0x412680: advance first, clamp at final frame. */
int tr2_cinematic_camera(const Tr2Level *l,const Tr2Position *origin,int yaw,int *frame,Tr2Camera *c,int *fov,int *roll){
    if(!l->cinematic.count)return 0;
    int next=tr2_word(*frame+1);if(next>=(int)l->cinematic.count)next=(int)l->cinematic.count-1;if(next<0)return 0;*frame=next;
    const uint8_t *p=l->cinematic.data+next*16;int sn=tr2_sin(yaw),cs=tr2_cos(yaw);
    int tx=tr2_i16(p),ty=tr2_i16(p+2),tz=tr2_i16(p+4),x=tr2_i16(p+6),y=tr2_i16(p+8),z=tr2_i16(p+10);
    c->target.x=tr2_long((int64_t)origin->x+tr2_asr(tr2_long((int64_t)tz*sn+(int64_t)tx*cs),14));c->target.y=tr2_long((int64_t)origin->y+ty);
    c->target.z=tr2_long((int64_t)origin->z+tr2_asr(tr2_long((int64_t)tz*cs-(int64_t)tx*sn),14));
    c->position.x=tr2_long((int64_t)origin->x+tr2_asr(tr2_long((int64_t)x*cs+(int64_t)z*sn),14));c->position.y=tr2_long((int64_t)origin->y+y);
    c->position.z=tr2_long((int64_t)origin->z+tr2_asr(tr2_long((int64_t)z*cs-(int64_t)x*sn),14));
    Tr2Sector ref;if(tr2_find_sector(l,c->position.x,c->position.y,c->position.z,c->position.room,&ref))c->position.room=(int16_t)ref.room;
    *fov=tr2_i16(p+12);*roll=tr2_i16(p+14);c->initialized=1;c->shift=0;return 1;
}
void tr2_camera_weapon_tick(const Tr2Level *l,const Tr2Actor *a,Tr2Camera *c,const Tr2Look *look,const Tr2Combat *g){
    Tr2Look shared;tr2_combat_look_pose(a,g,look,&shared);look=&shared;
    if(!c->initialized){c->target=(Tr2Position){a->x,a->y-1024,a->z,a->room,0};c->position=c->target;c->position.z-=100;c->initialized=1;c->shift=0;}
    int y=a->y-768;
    if(a->control.animation>=0 && (uint32_t)a->control.animation<l->animations.count){
        const uint8_t *anim=l->animations.data+a->control.animation*32;int index=(a->control.frame-tr2_i16(anim+16))/(anim[4]?anim[4]:1);
        uint32_t offset=tr2_u32(anim)+(index>0?(uint32_t)index:0)*anim[5]*2;
        if(offset+12<=l->frames.count*2){
            const uint8_t *b=l->frames.data+offset;int low=tr2_i16(b+4),high=tr2_i16(b+6),rate=anim[4]?anim[4]:1;
            int fraction=(a->control.frame-tr2_i16(anim+16))%rate,next=(index+1)*rate;
            if(next>tr2_i16(anim+18))rate+=tr2_i16(anim+18)-next;
            if(fraction && rate>0 && offset+anim[5]*2+12<=l->frames.count*2){const uint8_t *b2=b+anim[5]*2;low+=(tr2_i16(b2+4)-low)*fraction/rate;high+=(tr2_i16(b2+6)-high)*fraction/rate;}
            y=a->y+high+tr2_asr(3*(low-high),2);
        }
    }
    if(!look || !look->active){c->target.x=a->x;c->target.z=a->z;}
    int armed=g && g->weapon>=1 && g->weapon<=7 && (a->gun_status==2 || a->gun_status==4);
    if((a->control.current==55 || a->control.current==36 || a->control.current==37 || a->control.current==46) && !(look && look->active) && !armed){
        int bounds[6];if(tr2_actor_bounds(l,a,bounds)){int offset=(bounds[4]+bounds[5])/2;
            c->target.x+=tr2_asr(mul(offset,tr2_sin(a->yaw)),14);c->target.z+=tr2_asr(mul(offset,tr2_cos(a->yaw)),14);}
    }
    c->target.room=a->room;
    /* CalculateCamera 411d87/411e4b: leaving an item-targeted fixed shot
       resets target height and uses MoveCamera divisor one for this tick. */
    const Tr2World *world=l->world;
    int cut=world && world->camera_fixed_target;
    if(cut)c->target.y=y-(armed || (look && look->active)?256:0);
    else if(armed || (look && look->active))c->target.y+=tr2_asr(y-256-c->target.y,2);
    else c->target.y+=(y-c->target.y)/4;
    if(look && look->active){c->speed=cut?1:4;tr2_camera_look(l,a,c,look);return;}
    if(armed){
        /* PistolHandler writes head and torso together. After losing its
           target, CombatCamera still includes both decaying rotations. */
        Tr2Look head=look?*look:(Tr2Look){0};
        if(a->gun_status==4 && g->weapon<=3){head.yaw=g->torso_yaw;head.pitch=g->torso_pitch;}
        c->speed=cut?1:8;tr2_camera_combat(l,a,c,g,&head);return;
    }
    c->distance=1536;c->speed=12;c->angle=0;c->elevation=a->control.current==24?-8190:0;
    int state=a->control.current;
    /* ExtraLaraControl4 / 0x4288d0 uses chase defaults with these angles. */
    if(a->extra_animation && state==4){c->angle=14560;c->elevation=-4550;}
    if(a->extra_animation && state==3){c->angle=29120;c->distance=3072;}
    if(a->control.water_status==2 || a->control.water_status==4)c->elevation=-4004;
    if(state==10 || state==30 || state==31)c->elevation=-10920;
    else if(state==11)c->angle=15470;
    else if(state==36 || state==37){c->angle=6370;c->elevation=-4550;}
    else if(state==38)c->angle=13650;
    else if(state==40 || state==41){c->angle=14560;c->elevation=-4550;c->distance=1024;c->speed=6;}
    /* Key and puzzle use share 428520. Both retain the normal chase
       movement divisor (10, restored by CalculateCamera at 411ee2). */
    else if(state==42 || state==43){c->angle=-14560;c->elevation=-4550;c->distance=1024;c->speed=10;}
    else if(state==46){c->angle=30940;c->elevation=-4550;} /* 0x428550 */
    else if(state==56)c->elevation=-3640;
    else if(state==57)c->elevation=5460;
    else if(state==58 || state==60){c->angle=state==58?-5460:5460;c->elevation=-2730;}
    else if(state==59)c->angle=-8190;
    else if(state==61)c->elevation=-8190;
    else if(state==70)c->angle=12740;
    if(cut)c->speed=1;
    if(world && (uint32_t)world->vehicle<world->count && world->objects[world->vehicle].id==14){c->elevation=-3640;c->distance=2048;c->angle=0;}
    int chunky=a->control.current==24?0:1;Tr2Height h;if(!heights(l,&c->target,chunky,&h))return;if(h.floor<c->target.y)chunky=0;
    tr2_camera_chase(l,a,c,chunky);
}
void tr2_camera_view_tick(const Tr2Level *l,const Tr2Actor *a,Tr2Camera *c,const Tr2Look *look){tr2_camera_weapon_tick(l,a,c,look,NULL);}
void tr2_camera_tick(const Tr2Level *l,const Tr2Actor *a,Tr2Camera *c){tr2_camera_view_tick(l,a,c,NULL);}
