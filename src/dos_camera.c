/* Independently reconstructed DOS camera: 0x13560..0x141f4,
   0x14534..0x14ab2 and sector-boundary LOS 0x183c0..0x18b3a. */
#include "dos_camera.h"
#include "fixed.h"
#include "pistols.h"
#include <math.h>
#include <stdlib.h>
static int sn(const int16_t *s,int angle){unsigned a=(unsigned)angle&65535;int sign=a>=32768?-1:1;a&=32767;if(a>16384)a=32768-a;return sign*s[a>>4];}
static int32_t mul(int32_t a,int32_t b){return tomb_long((int64_t)a*b);}
static int heights(const TombLevel *l,TombCameraPoint *p,int heavy,TombHeights *h) {
    TombSectorRef ref;
    if(!tomb_find_sector(l,p->x,p->y,p->z,p->room,&ref) || !tomb_static_heights(l,ref,p->x,p->z,heavy,h))return 0;
    for(size_t i=0;i<l->item_count;i++)if(l->items[i].state_known) {
        if(!tomb_item_heights(l,ref,p->x,p->y,p->z,heavy,l->items,l->item_count,h))return 0;break;
    }
    p->room=(int16_t)ref.room;return 1;
}
static int bad(const TombLevel *l,TombCameraPoint p,int heavy) {
    TombHeights h;return !heights(l,&p,heavy,&h) || p.y>=h.floor || p.y<=h.ceiling;
}
/* X/Z LOS share the same boundary walk, but both execute in the original
   minor-axis-first order and may shorten the endpoint. */
static int axis_los(const TombLevel *l,const TombCameraPoint *s,TombCameraPoint *t,int axis,int heavy) {
    int32_t start=axis?s->z:s->x,end=axis?t->z:t->x,delta=end-start;
    if(!delta)return 1;
    int sign=delta<0?-1:1;
    int32_t side=axis?s->x:s->z,step=mul((axis?t->x:t->z)-side,1024)/delta;
    int32_t ystep=mul(t->y-s->y,1024)/delta;
    int32_t edge=delta<0?(start&~1023):(start|1023);
    int32_t other=side+tomb_asr(mul(edge-start,step),10),y=s->y+tomb_asr(mul(edge-start,ystep),10);
    int16_t room=s->room;
    while(sign<0?edge>end:edge<end) {
        TombCameraPoint p={axis?other:edge,y,axis?edge:other,room};TombHeights h;
        if(!heights(l,&p,heavy,&h))return 0;
        room=p.room;
        if(y>h.floor || y<h.ceiling){*t=p;return -1;}
        int16_t before=room;
        if(axis)p.z+=sign;else p.x+=sign;
        if(!heights(l,&p,heavy,&h))return 0;
        room=p.room;
        if(y>h.floor || y<h.ceiling){*t=(TombCameraPoint){axis?other:edge,y,axis?edge:other,before};return 0;}
        edge+=sign*1024;other+=sign*step;y+=sign*ystep;
    }
    t->room=room;return 1;
}
int tomb_dos_camera_los(const TombLevel *l,const TombCameraPoint *s,TombCameraPoint *t,int heavy) {
    int first_axis=llabs((long long)t->z-s->z)>llabs((long long)t->x-s->x)?0:1;
    int first=axis_los(l,s,t,first_axis,heavy),second=axis_los(l,s,t,!first_axis,heavy);
    if(!second)return 0;
    TombHeights h;if(!heights(l,t,heavy,&h))return 0;
    int32_t y=t->y,plane;
    if(h.floor<y && h.floor>s->y)plane=h.floor;
    else if(h.ceiling>y && h.ceiling<s->y)plane=h.ceiling;
    else return first==1 && second==1;
    t->x=s->x+mul(t->x-s->x,plane-s->y)/(y-s->y);
    t->z=s->z+mul(t->z-s->z,plane-s->y)/(y-s->y);t->y=plane;return 0;
}
static const TombCameraBox *box(const TombLevel *l,const TombCameraPoint *p) {
    if(p->room<0 || (size_t)p->room>=l->room_count)return NULL;
    const TombRoom *r=l->rooms+p->room;int x=tomb_asr(p->x-r->x,10),z=tomb_asr(p->z-r->z,10);
    if(x<0 || z<0 || x>=r->nx || z>=r->nz)return NULL;
    unsigned id=r->sectors[x*r->nz+z].box;return id<l->box_count?l->boxes+id:NULL;
}
static int32_t square(int32_t x){return mul(x,x);}
static void shift(int32_t *p,int32_t *q,int32_t tp,int32_t tq,int32_t bound,int32_t near,int32_t opposite,int32_t far,int32_t r2) {
    int32_t u=square(tp-bound),v=square(tq-near),a=u+v,b=u+square(tq-far),c=square(tp-opposite)+v;
    if(r2<a){*p=bound;if(r2>=u)*q=tq+(near<far?-1:1)*(int32_t)sqrt((double)(r2-u));}
    else if(a>65536){*p=bound;*q=near;}
    else if(r2<b){*p=bound;if(r2>=u)*q=tq+(near<far?1:-1)*(int32_t)sqrt((double)(r2-u));}
    else if(b>65536){*p=bound;*q=far;}
    else if(r2<c){if(r2>=v){*p=tp+(bound<opposite?1:-1)*(int32_t)sqrt((double)(r2-v));*q=near;}}
    else {*p=opposite;*q=near;}
}
/* ClipCamera 0x138d4: retain the view ray when constrained by a box edge. */
static void clip(int32_t *p,int32_t *q,int32_t tp,int32_t tq,int32_t bound,int32_t near,int32_t opposite,int32_t far,int32_t unused) {
    (void)unused;
    if((opposite>bound)!=(tp<bound)) {
        if(*p!=tp)*q=tq+mul(bound-tp,*q-tq)/(*p-tp);
        *p=bound;
    }
    if((near<far && tq>near && near>*q) || (near>far && tq<near && near<*q)) {
        if(*q!=tq)*p=tp+mul(near-tq,*p-tp)/(*q-tq);
        *q=near;
    }
}
static int adjust(const TombLevel *l,const TombCameraPoint *target,TombCameraPoint *p,int32_t r2,int heavy,int looking) {
    void (*edge)(int32_t *,int32_t *,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t,int32_t)=looking?clip:shift;
    tomb_dos_camera_los(l,target,p,heavy);
    const TombCameraBox *b=box(l,target),*ib=box(l,p);
    if(!b)return 0;
    if(ib && (p->z<b->zmin || p->z>b->zmax || p->x<b->xmin || p->x>b->xmax))b=ib;
    int32_t edges[4]={b->zmin,b->zmax,b->xmin,b->xmax};int blocked[4];
    for(int side=0;side<4;side++) {
        TombCameraPoint n=*p;int32_t *coord=side<2?&n.z:&n.x;
        *coord=side%2?(*coord+1024)&~1023:(*coord-1024)|1023;
        blocked[side]=bad(l,n,heavy);
        if(!blocked[side]) {
            const TombCameraBox *adj=box(l,&n);
            if(adj){int32_t edge=side==0?adj->zmin:side==1?adj->zmax:side==2?adj->xmin:adj->xmax;
                if(side%2?edge>edges[side]:edge<edges[side])edges[side]=edge;}
        }
    }
    int32_t zmin=edges[0]+256,zmax=edges[1]-256,xmin=edges[2]+256,xmax=edges[3]-256;int changed=1;
    if(p->z<zmin && blocked[0])edge(&p->z,&p->x,target->z,target->x,zmin,p->x<target->x?xmin:xmax,zmax,p->x<target->x?xmax:xmin,r2);
    else if(p->z>zmax && blocked[1])edge(&p->z,&p->x,target->z,target->x,zmax,p->x<target->x?xmin:xmax,zmin,p->x<target->x?xmax:xmin,r2);
    else if(p->x<xmin && blocked[2])edge(&p->x,&p->z,target->x,target->z,xmin,p->z<target->z?zmin:zmax,xmax,p->z<target->z?zmax:zmin,r2);
    else if(p->x>xmax && blocked[3])edge(&p->x,&p->z,target->x,target->z,xmax,p->z<target->z?zmin:zmax,xmin,p->z<target->z?zmax:zmin,r2);
    else changed=0;
    if(changed){TombHeights h;return heights(l,p,heavy,&h);}return 1;
}
int tomb_dos_camera_adjust(const TombLevel *l,const TombCameraPoint *target,TombCameraPoint *p,int32_t r2,int heavy) {
    return adjust(l,target,p,r2,heavy,0);
}
/* ShiftClamp 0x14060 returns the Y correction separately; FixedCamera ignores
   that return value, as does the original executable. */
static int shift_clamp(const TombLevel *l,TombCameraPoint *p,int heavy,int radius) {
    TombHeights h;if(!heights(l,p,heavy,&h))return 0;
    const TombCameraBox *b=box(l,p);if(!b)return 0;
    TombCameraPoint original=*p,n=*p;
    if(p->z<b->zmin+radius){n.z-=radius;if(bad(l,n,heavy))p->z=b->zmin+radius;}
    else if(p->z>b->zmax-radius){n.z+=radius;if(bad(l,n,heavy))p->z=b->zmax-radius;}
    n=original;
    if(p->x<b->xmin+radius){n.x-=radius;if(bad(l,n,heavy))p->x=b->xmin+radius;}
    else if(p->x>b->xmax-radius){n.x+=radius;if(bad(l,n,heavy))p->x=b->xmax-radius;}
    int floor=h.floor-radius,ceiling=h.ceiling+radius;
    if(floor<ceiling)floor=ceiling=tomb_asr(floor+ceiling,1);
    return p->y>floor?floor-p->y:p->y<ceiling?ceiling-p->y:0;
}
int tomb_dos_camera_move_effects(TombDosCamera *c,const TombLevel *l,TombCameraPoint ideal,int speed,int *bounce,uint32_t *random) {
    if(speed<1)return 0;
    c->eye.x+=(ideal.x-c->eye.x)/speed;c->eye.y+=(ideal.y-c->eye.y)/speed;c->eye.z+=(ideal.z-c->eye.z)/speed;c->eye.room=ideal.room;
    TombHeights h;if(!heights(l,&c->eye,0,&h))return 0;
    int floor=h.floor-256;
    if(floor<=c->eye.y && floor<=ideal.y) {
        tomb_dos_camera_los(l,&c->target,&c->eye,0);
        if(!heights(l,&c->eye,0,&h))return 0;floor=h.floor-256;
    }
    int ceiling=h.ceiling+256;
    if(floor<ceiling)floor=ceiling=tomb_asr(floor+ceiling,1);
    if(bounce && *bounce>0){c->eye.y+=*bounce;c->target.y+=*bounce;*bounce=0;}
    else if(bounce && *bounce<0 && random){
        int x=((int)tomb_control_random(random)-16384)*(*bounce)/32767;c->eye.x+=x;c->target.y+=x;
        int y=((int)tomb_control_random(random)-16384)*(*bounce)/32767;c->eye.y+=y;c->target.y+=y;
        int z=((int)tomb_control_random(random)-16384)*(*bounce)/32767;c->eye.z+=z;c->target.z+=z;*bounce+=5;
    }
    c->shift=floor<c->eye.y?floor-c->eye.y:ceiling>c->eye.y?ceiling-c->eye.y:0;
    TombCameraPoint displayed=c->eye;displayed.y+=c->shift;
    if(!heights(l,&displayed,0,&h))return 0;c->eye.room=displayed.room;return 1;
}
int tomb_dos_camera_tick_effects(TombDosCamera *c,const TombLevel *l,const TombActor *a,const int16_t b[6],const int16_t *s,const TombCameraRequest *r,int *bounce,uint32_t *random) {
    if(!c->ready){c->target=(TombCameraPoint){a->x,a->y-1024,a->z,a->room};c->eye=c->target;c->eye.z-=100;c->shift=0;c->fixed=0;c->distance_squared=1536*1536;c->ready=1;}
    if(r->flags==TOMB_CAMERA_LOOK) {
        int speed=c->fixed?1:4;int32_t old_x=c->target.x,old_z=c->target.z;
        int32_t y=a->y+b[3]+tomb_asr(3*(b[2]-b[3]),2)-256;
        c->target.y=c->fixed?y:c->target.y+tomb_asr(y-c->target.y,2);
        c->target.x=a->x;c->target.z=a->z;c->target.room=a->room;
        int angle=tomb_word(a->yaw+r->angle),elevation=tomb_word(r->pitch+r->elevation);
        int horizontal=tomb_asr(mul(1536,sn(s,elevation+16384)),14);
        int offset=tomb_asr(-mul(512,sn(s,elevation)),14);
        c->target.x+=tomb_asr(mul(offset,sn(s,a->yaw)),14);
        c->target.z+=tomb_asr(mul(offset,sn(s,a->yaw+16384)),14);
        if(bad(l,c->target,1)){c->target.x=a->x;c->target.z=a->z;}
        c->target.y+=shift_clamp(l,&c->target,1,306);
        TombCameraPoint ideal={c->target.x-tomb_asr(mul(horizontal,sn(s,angle)),14),c->target.y+tomb_asr(mul(1536,sn(s,elevation)),14),c->target.z-tomb_asr(mul(horizontal,sn(s,angle+16384)),14),c->eye.room};
        if(!adjust(l,&c->target,&ideal,c->distance_squared,1,1))return 0;
        c->target.x=old_x+(c->target.x-old_x)/speed;c->target.z=old_z+(c->target.z-old_z)/speed;
        if(!tomb_dos_camera_move_effects(c,l,ideal,speed,bounce,random))return 0;c->fixed=0;return 1;
    }
    if(r->flags==TOMB_CAMERA_COMBAT) {
        /* CalculateCamera mode 3: higher target, arithmetic (not truncating)
           vertical smoothing and speed 8. CombatCamera leaves radius2 alone. */
        int32_t y=a->y+b[3]+tomb_asr(3*(b[2]-b[3]),2)-256;
        c->target.x=a->x;c->target.z=a->z;c->target.room=a->room;
        c->target.y=c->fixed?y:c->target.y+tomb_asr(y-c->target.y,2);
        int elevation=tomb_word((int)r->elevation+r->pitch),angle=tomb_word((int)a->yaw+r->angle);
        int32_t horizontal=tomb_asr(mul(2560,sn(s,elevation+16384)),14);
        TombCameraPoint ideal={a->x-tomb_asr(mul(horizontal,sn(s,angle)),14),c->target.y+tomb_asr(mul(2560,sn(s,elevation)),14),a->z-tomb_asr(mul(horizontal,sn(s,angle+16384)),14),c->eye.room};
        if(!tomb_dos_camera_adjust(l,&c->target,&ideal,c->distance_squared,1) || !tomb_dos_camera_move_effects(c,l,ideal,c->fixed?1:8,bounce,random))return 0;
        c->fixed=0;return 1;
    }
    int fixed=r->fixed_index>=0 && (size_t)r->fixed_index<l->camera_count;
    int item=fixed && r->item_target;int32_t y=a->y+(item?(b[2]+b[3])/2:b[3]+tomb_asr(3*(b[2]-b[3]),2));
    c->target.x=a->x;c->target.z=a->z;c->target.room=a->room;
    if(r->flags==1){int centre=(b[4]+b[5])/2;c->target.x+=tomb_asr(mul(centre,sn(s,a->yaw)),14);c->target.z+=tomb_asr(mul(centre,sn(s,a->yaw+16384)),14);}
    int transition=c->fixed!=item,speed=transition?1:r->speed;
    c->target.y=transition?y:c->target.y+(y-c->target.y)/4;
    TombHeights h;int heavy=r->flags==2?0:1;
    if(!heights(l,&c->target,heavy,&h))return 0;if(h.floor<c->target.y)heavy=0;
    TombCameraPoint ideal;
    if(fixed && r->flags!=3) {
        const TombFixedCamera *p=l->cameras+r->fixed_index;ideal=(TombCameraPoint){p->x,p->y,p->z,p->room};
        if(!tomb_dos_camera_los(l,&c->target,&ideal,heavy))shift_clamp(l,&ideal,heavy,256);
    } else {
        int elevation=tomb_word((int)r->elevation+r->pitch);if(elevation>15470)elevation=15470;if(elevation< -15470)elevation=-15470;
        int32_t horizontal=tomb_asr(mul(r->distance,sn(s,elevation+16384)),14);int angle=tomb_word((int)a->yaw+r->angle);
        ideal=(TombCameraPoint){c->target.x-tomb_asr(mul(horizontal,sn(s,angle)),14),c->target.y+tomb_asr(mul(r->distance,sn(s,elevation)),14),c->target.z-tomb_asr(mul(horizontal,sn(s,angle+16384)),14),c->eye.room};
        c->distance_squared=square(horizontal);
        if(!tomb_dos_camera_adjust(l,&c->target,&ideal,c->distance_squared,heavy))return 0;
        if(!transition)speed=12;
    }
    if(!tomb_dos_camera_move_effects(c,l,ideal,speed,bounce,random))return 0;c->fixed=item;return 1;
}

int tomb_dos_camera_move(TombDosCamera *c,const TombLevel *l,TombCameraPoint ideal,int speed){return tomb_dos_camera_move_effects(c,l,ideal,speed,NULL,NULL);}
int tomb_dos_camera_tick(TombDosCamera *c,const TombLevel *l,const TombActor *a,const int16_t b[6],const int16_t *s,const TombCameraRequest *r){return tomb_dos_camera_tick_effects(c,l,a,b,s,r,NULL,NULL);}
int tomb_dos_camera_stomp(const TombCameraPoint *eye,const TombActor *a,int previous){
    int64_t x=(int64_t)a->x-eye->x,y=(int64_t)a->y-eye->y,z=(int64_t)a->z-eye->z;
    if(llabs(x)>=16384 || llabs(y)>=16384 || llabs(z)>=16384)return previous;
    return (int)((1048576-(x*x+y*y+z*z)/256)*100/1048576);
}
