/* DOS 0x158a8, 0x15e50 and 0x15f28. */
#include "level.h"
#include "fixed.h"
static int32_t add(int32_t a,int32_t b) { return tomb_long((int64_t)a+b); }
static int32_t sub(int32_t a,int32_t b) { return tomb_long((int64_t)a-b); }
int tomb_static_contact(const TombLevel *l,const TombActor *a,TombContact *out,
    int32_t height,int32_t radius,int16_t quadrant,TombStaticContact *result)
{
    if(!l || !a || !out || !result || a->room<0 || (size_t)a->room>=l->room_count) return 0;
    TombContact c=*out; TombStaticContact t={0}; t.rooms[0]=a->room; t.room_count=1;
    int32_t r=add(radius,50),h=add(height,50);
    for(int i=0;i<8;++i) {
        TombSectorRef ref;
        int32_t x=(i&1)?sub(a->x,r):add(a->x,r);
        int32_t z=(i&2)?sub(a->z,r):add(a->z,r);
        int32_t y=i<4?a->y:sub(a->y,h);
        if(!tomb_find_sector(l,x,y,z,a->room,&ref)) return 0;
        unsigned j=0;
        while(j<t.room_count && t.rooms[j]!=ref.room) ++j;
        if(j==t.room_count) t.rooms[t.room_count++]=(int16_t)ref.room;
    }
    int32_t xmin=sub(a->x,radius),xmax=add(a->x,radius);
    int32_t zmin=sub(a->z,radius),zmax=add(a->z,radius),top=sub(a->y,height);
    for(unsigned i=0;i<t.room_count;++i) {
        const TombRoom *room=l->rooms+t.rooms[i];
        for(size_t j=0;j<room->static_count;++j) {
            const TombStaticPlacement *p=room->statics+j; const TombStaticDef *d=NULL;
            for(size_t k=0;k<l->static_def_count;++k) if(l->static_defs[k].id==p->id) { d=l->static_defs+k; break; }
            if(!d) return 0;
            if(d->flags&1) continue;
            int32_t bx0=d->bounds[0],bx1=d->bounds[1],bz0=d->bounds[4],bz1=d->bounds[5];
            if(p->rotation==16384) { bx0=d->bounds[4]; bx1=d->bounds[5]; bz0=-(int32_t)d->bounds[1]; bz1=-(int32_t)d->bounds[0]; }
            else if(p->rotation==32768) { bx0=-(int32_t)d->bounds[1]; bx1=-(int32_t)d->bounds[0]; bz0=-(int32_t)d->bounds[5]; bz1=-(int32_t)d->bounds[4]; }
            else if(p->rotation==49152) { bx0=-(int32_t)d->bounds[5]; bx1=-(int32_t)d->bounds[4]; bz0=d->bounds[0]; bz1=d->bounds[1]; }
            bx0=add(bx0,p->x); bx1=add(bx1,p->x); bz0=add(bz0,p->z); bz1=add(bz1,p->z);
            if(bx0>=xmax || bx1<=xmin || a->y<=add(p->y,d->bounds[2]) ||
                top>=add(p->y,d->bounds[3]) || bz0>=zmax || bz1<=zmin) continue;
            int32_t sx=sub(bx1,xmin),sx2=sub(xmax,bx0);
            int32_t sz=sub(bz1,zmin),sz2=sub(zmax,bz0);
            if(sx2<sx) sx=sub(0,sx2);
            if(sz2<sz) sz=sub(0,sz2);
            if(quadrant>=0 && quadrant<=3) {
                int odd=quadrant&1; int32_t lateral=odd?sz:sx;
                if(lateral>radius || lateral<sub(0,radius)) {
                    c.type=1;
                    c.shift_x=odd?sx:sub(c.old_x,a->x);
                    c.shift_z=odd?sub(c.old_z,a->z):sz;
                } else if(lateral) {
                    c.shift_x=odd?0:sx; c.shift_z=odd?sz:0;
                    int positive_left=quadrant==0 || quadrant==3;
                    c.type=(int16_t)(((lateral>0)==positive_left)?2:4);
                }
            }
            t.hit=1; *out=c; *result=t; return 1;
        }
    }
    *out=c; *result=t; return 1;
}
