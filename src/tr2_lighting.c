#include "tr2_world.h"
#include <math.h>
#include <stdlib.h>

int tr2_dynamic_value(const Tr2DynamicLight *p,int x,int y,int z){
    int radius=1<<p->falloff,dx=x-p->x,dy=y-p->y,dz=z-p->z;
    if(abs(dx)>radius || abs(dy)>radius || abs(dz)>radius)return 0;
    int distance=dx*dx+dy*dy+dz*dz;if(distance>radius*radius)return 0;
    return (1<<p->intensity)-tr2_asr(distance,2*p->falloff-p->intensity);
}
int tr2_dynamic_shade(const Tr2World *w,int x,int y,int z,int shade){
    if(w && w->hazards)for(int i=0;i<w->hazards->light_count;i++)shade-=tr2_dynamic_value(w->hazards->lights+i,x,y,z);
    return shade<0?0:shade;
}
/* S_CalculateStaticLight 451580 -> 451540. Mesh vertex shades are relative
   to 4096; the placement's shade (including lights and fog) is an ADDER. */
int tr2_static_light(const Tr2World *w,int x,int y,int z,int first,int second,int mode,int phase,int depth){
    int shade=first;if(mode)shade+=(second-first)*phase/31;
    shade=tr2_dynamic_shade(w,x,y,z,shade)-4096;
    if(depth>12288)shade+=depth-12288;
    return shade;
}
int tr2_prelit_shade(int vertex,int adder){int shade=vertex+adder;return shade<0?0:shade>8191?8191:shade;}

/* Hardware vertex colour packing, 0x408aef..0x408b3f. */
uint32_t tr2_shade_rgb(int shade,int water){
    if(shade<0)shade=0;
    if(shade>8191)shade=8191;
    int v=tr2_asr(8191-shade,4);if(v>255)v=255;
    return 0xff000000u|((uint32_t)(water?v/2:v)<<16)|((uint32_t)(water?v*7/8:v)<<8)|(uint32_t)v;
}

/* 451e40 advances the sunset across 72000 display ticks at 60 Hz.
   The saved level clock advances once per 30 Hz simulation tick. */
int tr2_room_light_phase(const Tr2Level *l,const Tr2Room *room,int phase){
    if(room->light_mode!=3)return phase;
    const Tr2World *w=l->world;
    if(!l->sunset || !w || !w->inventory)return 0;
    int ticks=w->inventory->ticks;
    return ticks>=36000?31:ticks*31/36000;
}

/* S_CalculateLight 0x451240: the strongest room light, with rational
   attenuation and half-intensity directional shading (not eight GL lamps). */
void tr2_room_light(const Tr2Level *l,int room,int x,int y,int z,int phase,int depth,Tr2Light *out){
    *out=(Tr2Light){0,0,{0,0,0}};if(room<0 || (uint32_t)room>=l->room_count)return;
    const Tr2Room *r=l->rooms+room;phase=tr2_room_light_phase(l,r,phase);int best=0,dx=0,dy=0,dz=0;
    for(uint32_t i=0;i<r->lights.count;i++){
        const uint8_t *p=r->lights.data+i*24;int vx=x-tr2_i32(p),vy=y-tr2_i32(p+4),vz=z-tr2_i32(p+8);
        int distance=tr2_asr(tr2_long((int64_t)vx*vx+(int64_t)vy*vy+(int64_t)vz*vz),12);
        int radius=tr2_i32(p+16),falloff=tr2_asr(tr2_long((int64_t)radius*radius),12);
        int value=distance+falloff?tr2_long((int64_t)tr2_i16(p+12)*falloff)/(distance+falloff):0;
        if(r->light_mode){radius=tr2_i32(p+20);falloff=tr2_asr(tr2_long((int64_t)radius*radius),12);
            int second=distance+falloff?tr2_long((int64_t)tr2_i16(p+14)*falloff)/(distance+falloff):0;value+=(second-value)*phase/31;}
        if(value>best){best=value;dx=vx;dy=vy;dz=vz;}
    }
    int total=best;const Tr2World *w=l->world;
    if(w && w->hazards)for(int i=0;i<w->hazards->light_count;i++){
        const Tr2DynamicLight *p=w->hazards->lights+i;int value=tr2_dynamic_value(p,x,y,z);total+=value;
        if(value>best){best=value;dx=x-p->x;dy=y-p->y;dz=z-p->z;}
    }
    int half=tr2_asr(total,1);out->shade=r->ambient-half;
    if(half){
        out->divider=0x4000000/half;int yaw=tr2_atan(dz,dx);
        while(dx!=(int16_t)dx || dy!=(int16_t)dy || dz!=(int16_t)dz){dx=tr2_asr(dx,2);dy=tr2_asr(dy,2);dz=tr2_asr(dz,2);}
        int pitch=tr2_word(tr2_atan((int)sqrt((double)((int64_t)dx*dx+(int64_t)dz*dz)),dy));
        if((dy>0 && pitch>0) || (dy<0 && pitch<0))pitch=-pitch;
        int c=tr2_cos(pitch);out->direction[0]=tr2_asr(c*tr2_sin(yaw),14);out->direction[1]=-tr2_sin(pitch);out->direction[2]=tr2_asr(c*tr2_cos(yaw),14);
    }
    if(depth>12288){out->shade+=depth-12288;if(out->shade>8191)out->shade=8191;}
}
