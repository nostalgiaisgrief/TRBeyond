/* DOS 0x104a8 / 0x1063c / 0x3ecac / 0x3e6a4. */
#include "lighting.h"
#include "combat.h"
#include "fixed.h"
#include <string.h>
static int16_t word(const unsigned char *p){return tomb_word(p[0]|(unsigned)p[1]<<8);}
static int32_t lng(const unsigned char *p){return tomb_long((uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24);}
static int sn(const int16_t *s,int angle){unsigned a=(unsigned)angle&65535;int sign=a>=32768?-1:1;a&=32767;if(a>16384)a=32768-a;return sign*s[a>>4];}
static int clamp(int x){return x<0?0:x>8191?8191:x;}
int tomb_light_room_depth(int16_t shade,int depth){
    /* DOS room transform 3e930..3e95f, including its strict far comparison. */
    return depth>20480?8191:shade+(depth>12288?depth-12288:0);
}
void tomb_light_direction(const int32_t d[3],const int16_t *s,int32_t out[3]){
    int16_t angles[2];tomb_gun_angles(d[0],d[1],d[2],angles);
    int cp=sn(s,angles[1]+16384);
    out[0]=tomb_asr(cp*sn(s,angles[0]),14);out[1]=-sn(s,angles[1]);out[2]=tomb_asr(cp*sn(s,angles[0]+16384),14);
}
void tomb_light_room(const TombRoomLights *r,const int32_t pos[3],int depth,const int16_t *s,TombLighting *l){
    memset(l,0,sizeof *l);l->adder=r->ambient;
    if(r->count){
        int base=8191-r->ambient,best=0;int32_t direction[3]={0,0,0};
        for(size_t i=0;i<r->count;i++){
            const unsigned char *p=r->data+18*i;int32_t d[3];uint32_t distance=0;
            for(int j=0;j<3;j++){d[j]=tomb_long((int64_t)pos[j]-lng(p+4*j));distance+=(uint32_t)((int64_t)d[j]*d[j]);}
            int32_t radius=lng(p+14),fall=tomb_asr(tomb_long((int64_t)radius*radius),12);
            int32_t denominator=tomb_long((int64_t)tomb_asr(tomb_long(distance),12)+fall);
            if(!denominator)continue; /* Degenerate custom light; original divides by zero. */
            int32_t numerator=tomb_long((int64_t)word(p+12)*fall);
            int candidate=tomb_long((int64_t)base+(int64_t)numerator/denominator);
            if(candidate>best){best=candidate;memcpy(direction,d,sizeof d);}
        }
        int mid=(base+best)/2;l->adder=8191-mid;
        if(best!=base && best!=mid){l->divider=0x4000000/(best-mid);tomb_light_direction(direction,s,l->direction);}
    }
    if(depth>12288){l->adder+=depth-12288;if(l->adder>8191)l->adder=8191;}
}
void tomb_light_static(int16_t intensity,int depth,TombLighting *l){
    /* DOS retains the previous direction/divider; baked-shade meshes ignore it. */
    l->adder=intensity-4096;if(depth>12288)l->adder+=depth-12288;
}
void tomb_light_vector(const TombLighting *l,const int32_t rotation[9],int32_t local[3]){
    for(int j=0;j<3;j++){
        uint32_t sum=0;for(int i=0;i<3;i++)sum+=(uint32_t)((int64_t)rotation[i*3+j]*l->direction[i]);
        local[j]=l->divider?(int32_t)((int64_t)tomb_long(sum)/l->divider):0;
    }
}
int tomb_light_vertex(const TombMeshView *m,size_t vertex,const TombLighting *l,const int32_t local[3]){
    int shade=l->adder;
    if(vertex<m->normal_count && l->divider){
        const unsigned char *p=m->normals+vertex*6;uint32_t sum=0;
        for(int j=0;j<3;j++)sum+=(uint32_t)((int64_t)word(p+j*2)*local[j]);
        shade=tomb_long((int64_t)shade+tomb_asr(tomb_long(sum),16));
    }else if(vertex<m->light_count)shade+=word(m->lights+vertex*2);
    return clamp(shade);
}
