#include "tr2_runtime.h"
#include <stdlib.h>

static int grid_shift(int sample,int reference){int a=tr2_asr(sample,10),b=tr2_asr(reference,10);return a==b?0:b>a?1025-(sample&1023):-1-(sample&1023);}
static void restore(const Tr2Actor *a,Tr2Contact *c,int type){c->shift[0]=c->old[0]-a->x;c->shift[1]=c->old[1]-a->y;c->shift[2]=c->old[2]-a->z;c->type=(int16_t)type;}
int tr2_static_contact(const Tr2Level *l,const Tr2Actor *a,Tr2Contact *c,int height,int room){
    int rooms[9]={room},count=1,rr=c->radius+50;
    for(int i=0;i<8;i++){Tr2Sector ref;if(!tr2_find_sector(l,a->x+((i&1)?-rr:rr),i<4?a->y:a->y-height-50,a->z+((i&2)?-rr:rr),room,&ref))continue;
        int j=0;while(j<count && rooms[j]!=ref.room)j++;if(j==count)rooms[count++]=ref.room;}
    int xmin=a->x-c->radius,xmax=a->x+c->radius,zmin=a->z-c->radius,zmax=a->z+c->radius;
    for(int i=0;i<count;i++){
        Tr2Span placements=l->rooms[rooms[i]].statics;
        for(uint32_t j=0;j<placements.count;j++){
            const uint8_t *p=placements.data+j*20,*def=NULL;unsigned id=tr2_u16(p+18);
            for(uint32_t k=0;k<l->statics.count;k++)if(tr2_u32(l->statics.data+k*32)==id){def=l->statics.data+k*32;break;}
            if(!def || (tr2_u16(def+30)&1))continue;
            int b[6];for(int k=0;k<6;k++)b[k]=tr2_i16(def+18+k*2);
            int bx0=b[0],bx1=b[1],bz0=b[4],bz1=b[5],rot=tr2_u16(p+12);
            if(rot==16384){bx0=b[4];bx1=b[5];bz0=-b[1];bz1=-b[0];}
            else if(rot==32768){bx0=-b[1];bx1=-b[0];bz0=-b[5];bz1=-b[4];}
            else if(rot==49152){bx0=-b[5];bx1=-b[4];bz0=b[0];bz1=b[1];}
            bx0+=tr2_i32(p);bx1+=tr2_i32(p);bz0+=tr2_i32(p+8);bz1+=tr2_i32(p+8);
            int y=tr2_i32(p+4);
            if(bx0>=xmax || bx1<=xmin || bz0>=zmax || bz1<=zmin || a->y<=y+b[2] || a->y-height>=y+b[3])continue;
            int sx=bx1-xmin,sx2=xmax-bx0,sz=bz1-zmin,sz2=zmax-bz0;if(sx2<sx)sx=-sx2;if(sz2<sz)sz=-sz2;
            int odd=c->quadrant&1,lateral=odd?sz:sx;
            if(abs(lateral)>c->radius){c->type=1;c->shift[0]=odd?sx:c->old[0]-a->x;c->shift[2]=odd?c->old[2]-a->z:sz;}
            else if(lateral){c->shift[0]=odd?0:sx;c->shift[2]=odd?sz:0;int positive_left=c->quadrant==0 || c->quadrant==3;c->type=(int16_t)(((lateral>0)==positive_left)?2:4);}
            c->hit_static=1;return 1;
        }
    }return 0;
}
int tr2_contact_body(const Tr2Level *l,const Tr2Actor *a,Tr2Contact *c,int height,int statics,int body_y){
    c->type=0;c->hit_static=0;c->shift[0]=c->shift[1]=c->shift[2]=0;c->quadrant=(int16_t)(((unsigned)(c->facing+8192)&65535)>>14);
    int dx[4]={0},dz[4]={0},r=c->radius,sn=tr2_asr(tr2_sin(c->facing)*r,14),cs=tr2_asr(tr2_cos(c->facing)*r,14);
    switch(c->quadrant){
    case 0:dx[1]=sn;dz[1]=r;dx[2]=-r;dz[2]=r;dx[3]=r;dz[3]=r;break;
    case 1:dx[1]=r;dz[1]=cs;dx[2]=r;dz[2]=r;dx[3]=r;dz[3]=-r;break;
    case 2:dx[1]=sn;dz[1]=-r;dx[2]=r;dz[2]=-r;dx[3]=-r;dz[3]=-r;break;
    case 3:dx[1]=-r;dz[1]=cs;dx[2]=-r;dz[2]=-r;dx[3]=-r;dz[3]=r;break;
    }
    int top=a->y-height,query_y=top-160,room=a->room;
    for(int i=0;i<4;i++){
        Tr2Sector ref;Tr2Height h;int x=a->x+dx[i],z=a->z+dz[i];
        if(!tr2_query(l,x,query_y,z,room,&h,&ref))return 0;room=ref.room;
        int *s=c->sample[i];s[0]=h.floor==TR2_NO_HEIGHT?TR2_NO_HEIGHT:h.floor-a->y;s[1]=h.ceiling==TR2_NO_HEIGHT?TR2_NO_HEIGHT:h.ceiling-top;s[2]=h.type;
        if(!i){c->trigger=h.trigger;c->tilt_x=(int8_t)h.tilt_x;c->tilt_z=(int8_t)h.tilt_z;
            /* GetTiltType suppresses tilt while sufficiently above the surface. */
            int tx=h.tilt_x,tz=h.tilt_z,base=h.floor;
            base-=tz<0?-tr2_asr(tz*(z&1023),2):tr2_asr(tz*(1023-(z&1023)),2);
            base-=tx<0?-tr2_asr(tx*(x&1023),2):tr2_asr(tx*(1023-(x&1023)),2);
            if(body_y+512<base){c->tilt_x=0;c->tilt_z=0;}
        }else if((c->flags&1) && s[2]==2 && s[0]<0)s[0]=-32767;
        else if(((c->flags&2) && s[2]==2 && s[0]>0) || ((c->flags&4) && s[0]>0 && h.trigger && (tr2_u16(l->floors.data+h.trigger*2)&255)==5))s[0]=512;
    }
    if(statics && tr2_static_contact(l,a,c,height,room)){
        Tr2Height h;if(!tr2_query(l,a->x+c->shift[0],a->y,a->z+c->shift[2],room,&h,NULL) || h.floor<a->y-512 || h.ceiling>top){c->shift[0]=-c->shift[0];c->shift[2]=-c->shift[2];}
    }
    int *m=c->sample[0],*f=c->sample[1],*left=c->sample[2],*right=c->sample[3];
    if(m[0]==TR2_NO_HEIGHT)restore(a,c,1);
    else if(m[0]-m[1]<=0)restore(a,c,32);
    else{
        if(m[1]>=0){c->type=8;c->shift[1]=m[1];}
        if(f[0]>c->positive || f[0]<c->negative || f[1]>c->ceiling_limit){
            if(!(c->quadrant&1)){c->shift[0]=c->old[0]-a->x;c->shift[2]=grid_shift(a->z+dz[1],a->z);}
            else{c->shift[0]=grid_shift(a->x+dx[1],a->x);c->shift[2]=c->old[2]-a->z;}c->type=1;
        }else if(f[1]==c->ceiling_limit)restore(a,c,16);
        else if(left[0]>c->positive || left[0]<c->negative){if(!(c->quadrant&1))c->shift[0]=grid_shift(a->x+dx[2],a->x+dx[1]);else c->shift[2]=grid_shift(a->z+dz[2],a->z+dz[1]);c->type=2;}
        else if(right[0]>c->positive || right[0]<c->negative){if(!(c->quadrant&1))c->shift[0]=grid_shift(a->x+dx[3],a->x+dx[1]);else c->shift[2]=grid_shift(a->z+dz[3],a->z+dz[1]);c->type=4;}
    }return 1;
}
int tr2_contact(const Tr2Level *l,const Tr2Actor *a,Tr2Contact *c,int height,int statics){return tr2_contact_body(l,a,c,height,statics,a->y);}

