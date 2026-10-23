/* Structural terrain path reconstructed from DOS 0x151c0. */
#include "level.h"
#include "fixed.h"

static int32_t add(int32_t a,int32_t b) { return tomb_long((int64_t)a+b); }
static int32_t sub(int32_t a,int32_t b) { return tomb_long((int64_t)a-b); }
static int32_t sine(const int16_t *table,int32_t angle)
{
    uint32_t a=(uint32_t)angle&65535; int negative=a>=32768;
    if(negative) a-=32768;
    if(a>16384) a=32768-a;
    return negative?-table[a>>4]:table[a>>4];
}
/* Original 0x15868 snaps across a grid boundary with a one-unit margin. */
static int32_t grid_shift(int32_t sample,int32_t reference)
{
    int32_t a=tomb_asr(sample,10),b=tomb_asr(reference,10);
    if(a==b) return 0;
    int32_t fraction=(int32_t)((uint32_t)sample&1023);
    return b>a?1025-fraction:-1-fraction;
}
static void restore(const TombActor *a,TombContact *c,int type)
{
    c->shift_x=sub(c->old_x,a->x); c->shift_y=sub(c->old_y,a->y);
    c->shift_z=sub(c->old_z,a->z); c->type=(int16_t)type;
}
static int contact(const TombLevel *l,const TombActor *a,TombContact *out,
    int32_t height,int32_t radius,int32_t lara_y,const int16_t *table,int heavy,TombTerrain *terrain,
    TombStaticContact *static_result,const TombItem *items,size_t item_count)
{
    if(!l || !a || !out || !table || !terrain) return 0;
    TombContact c=*out; TombTerrain t={0};
    c.type=0; c.shift_x=c.shift_y=c.shift_z=0; t.static_meshes_pending=1;
    t.quadrant=(int16_t)((((uint32_t)(int32_t)c.facing+8192)&65535)>>14);
    int32_t dx[4]={0},dz[4]={0};
    int32_t sn=tomb_asr(tomb_long((int64_t)sine(table,c.facing)*radius),14);
    int32_t cs=tomb_asr(tomb_long((int64_t)sine(table,(int32_t)c.facing+16384)*radius),14);
    int32_t neg=sub(0,radius);
    switch(t.quadrant) {
    case 0: dx[1]=sn; dz[1]=radius; dx[2]=neg; dz[2]=radius; dx[3]=radius; dz[3]=radius; break;
    case 1: dx[1]=radius; dz[1]=cs; dx[2]=radius; dz[2]=radius; dx[3]=radius; dz[3]=neg; break;
    case 2: dx[1]=sn; dz[1]=neg; dx[2]=radius; dz[2]=neg; dx[3]=neg; dz[3]=neg; break;
    case 3: dx[1]=neg; dz[1]=cs; dx[2]=neg; dz[2]=neg; dx[3]=neg; dz[3]=radius; break;
    }
    int32_t top=sub(a->y,height),query_y=sub(top,160),room=a->room;
    for(int i=0;i<4;++i) {
        int32_t x=add(a->x,dx[i]),z=add(a->z,dz[i]);
        TombSectorRef ref; TombHeights h;
        if(!tomb_find_sector(l,x,query_y,z,room,&ref)) return 0;
        int ok=items?tomb_item_heights(l,ref,x,query_y,z,heavy,items,item_count,&h):
            tomb_static_heights(l,ref,x,z,heavy,&h);
        if(!ok) return 0;
        room=ref.room; t.object_references+=h.object_references;
        TombTerrainSample *s=t.samples+i;
        s->floor=h.floor==-32512?-32512:sub(h.floor,a->y);
        s->ceiling=h.ceiling==-32512?-32512:sub(h.ceiling,top); s->type=h.floor_type;
        if(!i) {
            int16_t tilt;
            if(!tomb_floor_tilt(l,ref,x,lara_y,z,&tilt)) return 0;
            uint16_t bits=(uint16_t)tilt;
            t.tilt_x=(int16_t)((bits&255)<128?(bits&255):(int32_t)(bits&255)-256);
            t.tilt_z=(int16_t)tomb_asr(tilt,8); t.trigger_index=h.trigger_index;
        } else if((c.flags&1) && s->type==2 && s->floor<0) s->floor=-32767;
        else if(((c.flags&2) && s->type==2 && s->floor>0) ||
            ((c.flags&4) && s->floor>0 && h.trigger_index &&
             (l->floor_data[h.trigger_index]&255)==5)) s->floor=512;
    }
    c.floor=t.samples[0].floor;
    TombTerrainSample *m=t.samples,*f=m+1,*left=m+2,*right=m+3;
    TombStaticContact sc={0};
    if(static_result) {
        TombActor query=*a; query.room=(int16_t)room;
        if(!tomb_static_contact(l,&query,&c,height,radius,t.quadrant,&sc)) return 0;
        t.static_meshes_pending=0;
    }
    if(m->floor==-32512) restore(a,&c,1);
    else if(sub(m->floor,m->ceiling)<=0) restore(a,&c,32);
    else {
        if(m->ceiling>=0) { c.type=8; c.shift_y=m->ceiling; }
        if(f->floor>c.positive_limit || f->floor<c.negative_limit || f->ceiling>c.ceiling_limit) {
            if(!(t.quadrant&1)) { c.shift_x=sub(c.old_x,a->x); c.shift_z=grid_shift(add(a->z,dz[1]),a->z); }
            else { c.shift_x=grid_shift(add(a->x,dx[1]),a->x); c.shift_z=sub(c.old_z,a->z); }
            c.type=1;
        } else if(f->ceiling==c.ceiling_limit) restore(a,&c,16);
        else if(left->floor>c.positive_limit || left->floor<c.negative_limit) {
            if(!(t.quadrant&1)) c.shift_x=grid_shift(add(a->x,dx[2]),add(a->x,dx[1]));
            else c.shift_z=grid_shift(add(a->z,dz[2]),add(a->z,dz[1]));
            c.type=2;
        } else if(right->floor>c.positive_limit || right->floor<c.negative_limit) {
            if(!(t.quadrant&1)) c.shift_x=grid_shift(add(a->x,dx[3]),add(a->x,dx[1]));
            else c.shift_z=grid_shift(add(a->z,dz[3]),add(a->z,dz[1]));
            c.type=4;
        }
    }
    *out=c; *terrain=t;
    if(static_result) *static_result=sc;
    return 1;
}
int tomb_terrain_contact(const TombLevel *l,const TombActor *a,TombContact *out,
    int32_t height,int32_t radius,int32_t lara_y,const int16_t *table,int heavy,TombTerrain *terrain)
{ return contact(l,a,out,height,radius,lara_y,table,heavy,terrain,NULL,NULL,0); }
int tomb_world_contact(const TombLevel *l,const TombActor *a,TombContact *out,
    int32_t height,int32_t radius,int32_t lara_y,const int16_t *table,int heavy,TombTerrain *terrain,
    TombStaticContact *static_result)
{
    if(!static_result) return 0;
    return contact(l,a,out,height,radius,lara_y,table,heavy,terrain,static_result,NULL,0);
}
int tomb_active_contact(const TombLevel *l,const TombActor *a,TombContact *out,
    int32_t height,int32_t radius,int32_t lara_y,const int16_t *table,int heavy,
    const TombItem *items,size_t item_count,TombTerrain *terrain,TombStaticContact *static_result)
{
    if(!static_result || !items) return 0;
    return contact(l,a,out,height,radius,lara_y,table,heavy,terrain,static_result,items,item_count);
}
