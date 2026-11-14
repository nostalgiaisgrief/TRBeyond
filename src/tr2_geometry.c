#include "tr2_world.h"
#include <stdlib.h>

static int sb(unsigned n){return n<128?(int)n:(int)n-256;}
static const uint8_t *sector(const Tr2Level *l,Tr2Sector r){if(r.room<0 || (uint32_t)r.room>=l->room_count)return NULL;Tr2Span s=l->rooms[r.room].sectors;return r.index>=0 && (uint32_t)r.index<s.count?s.data+r.index*8:NULL;}
static int at(const Tr2Level *l,int room,int x,int z,int clamp,Tr2Sector *out){
    if(room<0 || (uint32_t)room>=l->room_count)return 0;const Tr2Room *r=l->rooms+room;
    int ix=tr2_asr((int64_t)x-r->x,10),iz=tr2_asr((int64_t)z-r->z,10);
    if(clamp){if(iz<=0 || iz>=r->nz-1){iz=iz<=0?0:r->nz-1;if(ix<1)ix=1;else if(ix>r->nx-2)ix=r->nx-2;}
        else{if(ix<0)ix=0;else if(ix>=r->nx)ix=r->nx-1;}}
    if(ix<0 || iz<0 || ix>=r->nx || iz>=r->nz)return 0;*out=(Tr2Sector){room,ix*r->nz+iz};return 1;
}
static int word(const Tr2Level *l,uint32_t at){return at<l->floors.count?tr2_u16(l->floors.data+at*2):-1;}
void tr2_bridge_height(int id,int object_y,int yaw,int x,int y,int z,int *floor,int *ceiling){
    if(id<117 || id>119)return;
    int offset=(yaw==0?-x:yaw==-32768?x:yaw==16384?z:-z)&1023;
    int height=object_y+(id==117?0:offset>>(id==118?2:1));
    if(y<=height)*floor=height;else *ceiling=height+256;
}
/* GetLiftHeight 0x4358d0. Inside a closed car, outside query points are walls. */
static int lift_inside(const Tr2Actor *a,int x,int z){
    int dx=tr2_asr(x,10)-tr2_asr(a->x,10),dz=tr2_asr(z,10)-tr2_asr(a->z,10);
    return (dx==0 || dx==-1) && (dz==0 || dz==1);
}
void tr2_lift_height(const Tr2Actor *a,const Tr2Actor *lara,int x,int y,int z,int *floor,int *ceiling){
    int inside=lift_inside(a,x,z);*floor=32767;*ceiling=-32767;
    if(!lift_inside(a,lara->x,lara->z)){
        if(!inside)return;
        if(y<=a->y-1280)*floor=a->y-1280;
        else if(y>=a->y+256)*ceiling=a->y+256;
        else if(a->control.current==0){*floor=-32512;*ceiling=32767;}
        else{*floor=a->y;*ceiling=a->y-1024;}
    }else if(a->control.current==0 && lara->y<a->y+256 && lara->y>a->y-1024){
        if(inside){*floor=a->y;*ceiling=a->y-1024;}
        else{*floor=-32512;*ceiling=32767;}
    }else if(inside){
        if(lara->y<a->y-1024)*floor=a->y-1280;
        else if(lara->y<a->y+256){*floor=a->y;*ceiling=a->y-1024;}
        else *ceiling=a->y+256;
    }
}
int tr2_world_object_height(const Tr2Level *l,int index,int x,int y,int z,Tr2Height *h){
    if(!l->object_floors || (unsigned)index>=l->items.count)return 0;
    const uint8_t *item=l->items.data+index*24;
    const Tr2World *w=l->world;
    if(w && (unsigned)index<w->count){const Tr2Object *o=w->objects+index;const Tr2Actor *a=&o->pose;
        if(o->id==65){
            const Tr2Actor *lara=w->lara?w->lara:&w->objects[l->lara].pose;int floor,ceiling;
            tr2_lift_height(a,lara,x,y,z,&floor,&ceiling);
            if(floor<h->floor)h->floor=floor;
            if(ceiling>h->ceiling){h->ceiling=ceiling;return 2;}
            return 0;
        }
        if(o->id==114 || o->id==115){
            int dx=tr2_asr(x,10)-tr2_asr(a->x,10),dz=tr2_asr(z,10)-tr2_asr(a->z,10);
            int inside=(!dx && !dz) || (a->yaw==0 && !dx && dz==1) || (a->yaw==-32768 && !dx && dz==-1) ||
                (a->yaw==16384 && dx==1 && !dz) || (a->yaw==-16384 && dx==-1 && !dz);
            if(inside && a->control.current==0){
                if(y<=a->y && a->y<h->floor)h->floor=a->y;
                else if(y>a->y && a->y>h->ceiling){h->ceiling=a->y+256;return 1;}
            }return 0;
        }
        if(o->id>=55 && o->id<=57){int height=a->y-(o->id==57?1024:512);
            if(a->control.current==0 || a->control.current==1){if(y<=height)h->floor=height;else{h->ceiling=height+256;return 1;}}
            return 0;
        }
    }
    int ceiling=INT32_MIN;
    tr2_bridge_height(tr2_u16(item),tr2_i32(item+8),tr2_i16(item+16),x,y,z,&h->floor,&ceiling);
    if(ceiling==INT32_MIN)return 0;h->ceiling=ceiling;return 1;
}
static int door(const Tr2Level *l,const uint8_t *s){uint32_t p=tr2_u16(s);if(!p)return 255;int w=word(l,p++);if(w==2){p++;w=word(l,p++);}if(w==3){p++;w=word(l,p++);}return w>=0 && (w&255)==1?word(l,p):255;}
int tr2_find_sector(const Tr2Level *l,int x,int y,int z,int room,Tr2Sector *out){
    Tr2Sector ref;const uint8_t *s;int budget=(int)l->room_count*3+1,next;
    do{if(!budget-- || !at(l,room,x,z,1,&ref))return 0;s=sector(l,ref);if(!s)return 0;next=door(l,s);if(next!=255)room=next;}while(next!=255);
    if(y>=sb(s[5])*256){while(s[4]!=255){if(!budget-- || !at(l,s[4],x,z,0,&ref))return 0;s=sector(l,ref);if(!s)return 0;if(y<sb(s[5])*256)break;}}
    else if(y<sb(s[7])*256){while(s[6]!=255){if(!budget-- || !at(l,s[6],x,z,0,&ref))return 0;s=sector(l,ref);if(!s)return 0;if(y>=sb(s[7])*256)break;}}
    *out=ref;return 1;
}
static const uint8_t *layer(const Tr2Level *l,Tr2Sector ref,int x,int z,int up){
    for(uint32_t i=0;i<=l->room_count;i++){const uint8_t *s=sector(l,ref);if(!s)return NULL;int n=s[up?6:4];if(n==255)return s;if(!at(l,n,x,z,0,&ref))return NULL;}return NULL;
}
int tr2_heights(const Tr2Level *l,Tr2Sector ref,int x,int y,int z,int chunky,Tr2Height *h){
    const uint8_t *bottom=layer(l,ref,x,z,0),*top=layer(l,ref,x,z,1);if(!bottom || !top)return 0;
    *h=(Tr2Height){sb(bottom[5])*256,sb(top[7])*256,0,0,0,0};int fx=x&1023,fz=z&1023;
    uint32_t p=tr2_u16(bottom);int budget=(int)l->floors.count,has_object_ceiling=0;
    while(p && budget-->0){int head=word(l,p++);if(head<0)return 0;int op=head&255;
        if(op>=1 && op<=3){int w=word(l,p++);if(w<0)return 0;if(op==2){int sx=sb(w&255),sz=sb((unsigned)w>>8),steep=abs(sx)>2 || abs(sz)>2;
            h->tilt_x=sx;h->tilt_z=sz;if(!chunky || !steep){h->type=steep?2:1;h->floor+=sz<0?-tr2_asr(sz*fz,2):tr2_asr(sz*(1023-fz),2);h->floor+=sx<0?-tr2_asr(sx*fx,2):tr2_asr(sx*(1023-fx),2);}}}
        else if(op==4){if(!h->trigger)h->trigger=p-1;p++;int w;do{w=word(l,p++);if(w<0)return 0;if(!(w&0x3c00))has_object_ceiling|=tr2_world_object_height(l,w&1023,x,y,z,h);if((w&0x3c00)==0x400){w=word(l,p++);if(w<0)return 0;}}while(!(w&0x8000));}
        else if(op==5)h->trigger=p-1;
        else if(op==6){if(!h->trigger)h->trigger=p-1;}
        else return 0;
        if(head&0x8000)break;
    }
    int object_ceiling=h->ceiling;h->ceiling=sb(top[7])*256;p=tr2_u16(top);
    if(p){int head=word(l,p++);if((head&255)==2){p++;head=word(l,p++);}if((head&255)==3){int w=word(l,p);if(w<0)return 0;int sx=sb(w&255),sz=sb((unsigned)w>>8);
        if(!chunky || (abs(sx)<=2 && abs(sz)<=2)){h->ceiling+=sz<0?tr2_asr(sz*fz,2):-tr2_asr(sz*(1023-fz),2);h->ceiling+=sx<0?tr2_asr(sx*(1023-fx),2):-tr2_asr(sx*fx,2);}}}
    if(has_object_ceiling&1)h->ceiling=object_ceiling;
    else if((has_object_ceiling&2) && object_ceiling>h->ceiling)h->ceiling=object_ceiling;
    return 1;
}
int tr2_query(const Tr2Level *l,int x,int y,int z,int room,Tr2Height *out,Tr2Sector *ref){Tr2Sector r;if(!tr2_find_sector(l,x,y,z,room,&r) || !tr2_heights(l,r,x,y,z,0,out))return 0;if(ref)*ref=r;return 1;}
int tr2_water_height(const Tr2Level *l,int x,int z,int room,int *depth){
    Tr2Sector ref;const uint8_t *s=NULL;int budget=(int)l->room_count*3+1,next;
    if(depth)*depth=TR2_NO_HEIGHT;
    do{if(!budget-- || !at(l,room,x,z,1,&ref))return TR2_NO_HEIGHT;s=sector(l,ref);if(!s)return TR2_NO_HEIGHT;next=door(l,s);if(next!=255)room=next;}while(next!=255);
    Tr2Sector initial=ref;int water=(l->rooms[room].flags&1)!=0,surface=TR2_NO_HEIGHT;
    while(budget--){
        next=s[water?6:4];
        if(next==255){if(water){surface=sb(s[7])*256;if(depth)*depth=32767;}break;}
        if((uint32_t)next>=l->room_count)return TR2_NO_HEIGHT;
        if(((l->rooms[next].flags&1)!=0)!=water){surface=sb(s[water?7:5])*256;break;}
        if(!at(l,next,x,z,0,&ref))return TR2_NO_HEIGHT;s=sector(l,ref);if(!s)return TR2_NO_HEIGHT;
    }
    if(depth && *depth!=32767 && surface!=TR2_NO_HEIGHT){Tr2Height h;if(tr2_heights(l,initial,x,0,z,0,&h))*depth=h.floor-surface;}
    return surface;
}
