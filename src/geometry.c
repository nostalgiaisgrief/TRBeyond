#include "level.h"
#include "fixed.h"

static const TombSector *sector(const TombLevel *l,TombSectorRef ref)
{
    if(ref.room<0 || (size_t)ref.room>=l->room_count) return NULL;
    const TombRoom *r=l->rooms+ref.room;
    return ref.index>=0 && (size_t)ref.index<(size_t)r->nx*r->nz?r->sectors+ref.index:NULL;
}
static int at(const TombLevel *l,int room,int32_t x,int32_t z,int clamp,TombSectorRef *ref)
{
    if(room<0 || (size_t)room>=l->room_count) return 0;
    const TombRoom *r=l->rooms+room;
    int32_t ix=tomb_asr(tomb_long((int64_t)x-r->x),10);
    int32_t iz=tomb_asr(tomb_long((int64_t)z-r->z),10);
    if(clamp) {
        if(iz<=0 || iz>=r->nz-1) {
            iz=iz<=0?0:r->nz-1;
            if(ix<1) ix=1; else if(ix>r->nx-2) ix=r->nx-2;
        } else { if(ix<0) ix=0; else if(ix>=r->nx) ix=r->nx-1; }
    }
    if(ix<0 || ix>=r->nx || iz<0 || iz>=r->nz) return 0;
    *ref=(TombSectorRef){room,ix*r->nz+iz}; return 1;
}
static int word(const TombLevel *l,size_t n,int *out)
{
    if(n>=l->floor_count) return 0;
    *out=l->floor_data[n]; return 1;
}
static int door(const TombLevel *l,const TombSector *s,int *out)
{
    *out=255; if(!s->floor_index) return 1;
    size_t pos=s->floor_index; int w;
    if(!word(l,pos++,&w)) return 0;
    /* The original tests full words for tilt entries before masking door type. */
    if(w==2) { ++pos; if(!word(l,pos++,&w)) return 0; }
    if(w==3) { ++pos; if(!word(l,pos++,&w)) return 0; }
    return (w&255)!=1?1:word(l,pos,out);
}
int tomb_find_sector(const TombLevel *l,int32_t x,int32_t y,int32_t z,int32_t room,TombSectorRef *out)
{
    if(!l || !out) return 0;
    TombSectorRef ref; const TombSector *s; int next=255;
    size_t budget=l->room_count*3+1;
    do {
        if(!budget-- || !at(l,room,x,z,1,&ref)) return 0;
        s=sector(l,ref); if(!s || !door(l,s,&next)) return 0;
        if(next!=255) room=next;
    } while(next!=255);
    if(y>=(int32_t)s->floor*256) {
        while(s->below!=255) {
            if(!budget-- || !at(l,s->below,x,z,0,&ref)) return 0;
            s=sector(l,ref); if(!s) return 0;
            if(y<(int32_t)s->floor*256) break;
        }
    } else if(y<(int32_t)s->ceiling*256) {
        while(s->above!=255) {
            if(!budget-- || !at(l,s->above,x,z,0,&ref)) return 0;
            s=sector(l,ref); if(!s) return 0;
            if(y>=(int32_t)s->ceiling*256) break;
        }
    }
    *out=ref; return 1;
}
static int layer(const TombLevel *l,TombSectorRef *r,int32_t x,int32_t z,int up)
{
    for(size_t n=0;n<=l->room_count;++n) {
        const TombSector *s=sector(l,*r); if(!s) return 0;
        int next=up?s->above:s->below;
        if(next==255) return 1;
        if(!at(l,next,x,z,0,r)) return 0;
    }
    return 0;
}
static int byte_signed(int n) { return n<128?n:n-256; }
int tomb_floor_tilt(const TombLevel *l,TombSectorRef ref,int32_t x,int32_t y,int32_t z,int16_t *out)
{
    if(!l || !out || !layer(l,&ref,x,z,0)) return 0;
    const TombSector *s=sector(l,ref); if(!s) return 0;
    *out=0;
    if(tomb_long((int64_t)y+512)<s->floor*256 || !s->floor_index) return 1;
    int header,w;
    if(!word(l,s->floor_index,&header)) return 0;
    if((header&255)!=2) return 1;
    if(!word(l,(size_t)s->floor_index+1,&w)) return 0;
    *out=tomb_word(w); return 1;
}
static int16_t tilt(int16_t height,int w,int32_t x,int32_t z,int ceiling)
{
    int sx=byte_signed(w&255), sz=byte_signed((w>>8)&255);
    int fx=(int)((uint32_t)x&1023),fz=(int)((uint32_t)z&1023);
    if(!ceiling) {
        height=tomb_word((int32_t)height+(sz<0?-tomb_asr(sz*fz,2):tomb_asr(sz*(1023-fz),2)));
        height=tomb_word((int32_t)height+(sx<0?-tomb_asr(sx*fx,2):tomb_asr(sx*(1023-fx),2)));
    } else {
        height=tomb_word((int32_t)height+(sz<0?tomb_asr(sz*fz,2):-tomb_asr(sz*(1023-fz),2)));
        height=tomb_word((int32_t)height+(sx<0?tomb_asr(sx*(1023-fx),2):-tomb_asr(sx*fx,2)));
    }
    return height;
}
/* Decode a floor list, collect deferred object references and trigger address.
   No callback or object geometry is silently emulated here. */
static int floor_list(const TombLevel *l,uint16_t index,int32_t x,int32_t y,int32_t z,int heavy,
    const TombItem *items,size_t count,int ceiling,TombHeights *h)
{
    if(!index) return 1;
    size_t p=index;
    while(p<l->floor_count) {
        int header=l->floor_data[p++],op=header&255,w;
        if(op==1 || op==2 || op==3) {
            if(!word(l,p++,&w)) return 0;
            if(op==2 && !ceiling) {
                int sx=byte_signed(w&255),sz=byte_signed((w>>8)&255);
                int steep=sx < -2 || sx>2 || sz < -2 || sz>2;
                if(!heavy || !steep) { h->floor_type=steep?2:1; h->floor=tilt(h->floor,w,x,z,0); }
            }
        } else if(op==4) {
            if(!ceiling && !h->trigger_index) h->trigger_index=(uint32_t)(p-1);
            if(!word(l,p++,&w)) return 0; /* trigger setup */
            do {
                if(!word(l,p++,&w)) return 0;
                int action=(w&0x3fff)>>10;
                if(action==0) {
                    if(items && (size_t)(w&1023)>=count) return 0;
                    int resolved=items?tomb_item_height(items+(w&1023),x,y,z,ceiling,
                        ceiling?&h->ceiling:&h->floor):0;
                    /* DOS setup leaves switch/door height callbacks null. Their
                       collision is handled by switch bounds and door sectors,
                       not by treating every trigger referring to them as solid. */
                    if(!items && (size_t)(w&1023)<l->item_count) {
                        int id=l->items[w&1023].object;
                        if(id==55 || (id>=57 && id<=64))resolved=1;
                    }
                    if(!ceiling && !resolved) ++h->object_references;
                }
                else if(action==1 && !word(l,p++,&w)) return 0; /* camera extra word carries stop bit */
            } while(!(w&0x8000));
        } else if(op==5) { if(!ceiling) h->trigger_index=(uint32_t)(p-1); }
        else return 0;
        if(header&0x8000) return 1;
    }
    return 0;
}
static int heights(const TombLevel *l,TombSectorRef ref,int32_t x,int32_t y,int32_t z,int heavy,
    const TombItem *items,size_t count,TombHeights *out)
{
    if(!l || !out) return 0;
    TombSectorRef bottom=ref,top=ref;
    if(!layer(l,&bottom,x,z,0) || !layer(l,&top,x,z,1)) return 0;
    const TombSector *b=sector(l,bottom),*t=sector(l,top);
    if(!b || !t) return 0;
    TombHeights h={(int16_t)(b->floor*256),(int16_t)(t->ceiling*256),0,0,0};
    if(!floor_list(l,b->floor_index,x,y,z,heavy,items,count,0,&h)) return 0;
    if(t->floor_index) {
        size_t p=t->floor_index; int w;
        if(!word(l,p++,&w)) return 0;
        if((w&255)==2) { ++p; if(!word(l,p++,&w)) return 0; }
        if((w&255)==3) {
            if(!word(l,p,&w)) return 0;
            int sx=byte_signed(w&255),sz=byte_signed((w>>8)&255);
            if(!heavy || (sx>=-2 && sx<=2 && sz>=-2 && sz<=2)) h.ceiling=tilt(h.ceiling,w,x,z,1);
        }
    }
    if(items && !floor_list(l,b->floor_index,x,y,z,heavy,items,count,1,&h)) return 0;
    *out=h; return 1;
}
int tomb_static_heights(const TombLevel *l,TombSectorRef ref,int32_t x,int32_t z,int heavy,TombHeights *out)
{ return heights(l,ref,x,0,z,heavy,NULL,0,out); }
int tomb_item_heights(const TombLevel *l,TombSectorRef ref,int32_t x,int32_t y,int32_t z,int heavy,
    const TombItem *items,size_t count,TombHeights *out)
{
    if(!items) return 0;
    return heights(l,ref,x,y,z,heavy,items,count,out);
}
