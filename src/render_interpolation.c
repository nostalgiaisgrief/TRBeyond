#include "render_interpolation.h"
#include <math.h>
#include <string.h>
typedef struct Quat { double v[4]; } Quat;
static Quat quaternion(const float *m){
    Quat q={{0,0,0,0}};double trace=m[0]+m[5]+m[10];
    if(trace>0){double s=sqrt(trace+1)*2;q.v[3]=s/4;q.v[0]=(m[6]-m[9])/s;q.v[1]=(m[8]-m[2])/s;q.v[2]=(m[1]-m[4])/s;}
    else{
        int i=m[0]>m[5]?(m[0]>m[10]?0:2):(m[5]>m[10]?1:2),j=(i+1)%3,k=(i+2)%3;
        double s=sqrt(1+m[i*5]-m[j*5]-m[k*5])*2;
        q.v[i]=s/4;q.v[j]=(m[j*4+i]+m[i*4+j])/s;q.v[k]=(m[k*4+i]+m[i*4+k])/s;q.v[3]=(m[j*4+k]-m[k*4+j])/s;
    }
    return q;
}
static void blend(const float *a,const float *b,double t,float *out){
    if(!memcmp(a,b,16*sizeof *out)){memcpy(out,a,16*sizeof *out);return;}
    if(t<=0){memcpy(out,a,16*sizeof *out);return;}if(t>=1){memcpy(out,b,16*sizeof *out);return;}
    Quat p=quaternion(a),q=quaternion(b);double dot=0;
    for(int i=0;i<4;i++)dot+=p.v[i]*q.v[i];
    if(dot<0){for(int i=0;i<4;i++)q.v[i]=-q.v[i];dot=-dot;}
    double u=1-t,v=t;if(dot<.9995){double angle=acos(dot),s=sin(angle);u=sin((1-t)*angle)/s;v=sin(t*angle)/s;}
    double length=0;for(int i=0;i<4;i++){q.v[i]=u*p.v[i]+v*q.v[i];length+=q.v[i]*q.v[i];}
    length=sqrt(length);for(int i=0;i<4;i++)q.v[i]/=length;
    double x=q.v[0],y=q.v[1],z=q.v[2],w=q.v[3];
    out[0]=(float)(1-2*(y*y+z*z));out[1]=(float)(2*(x*y+w*z));out[2]=(float)(2*(x*z-w*y));out[3]=0;
    out[4]=(float)(2*(x*y-w*z));out[5]=(float)(1-2*(x*x+z*z));out[6]=(float)(2*(y*z+w*x));out[7]=0;
    out[8]=(float)(2*(x*z+w*y));out[9]=(float)(2*(y*z-w*x));out[10]=(float)(1-2*(x*x+y*y));out[11]=0;
    for(int i=12;i<15;i++)out[i]=(float)(a[i]+t*(b[i]-a[i]));out[15]=1;
}
static void multiply(const float *a,const float *b,float *out){
    for(int col=0;col<4;col++)for(int row=0;row<4;row++){
        double sum=0;for(int k=0;k<4;k++)sum+=(double)a[k*4+row]*b[col*4+k];out[col*4+row]=(float)sum;
    }
}
static void relative(const float *parent,const float *world,float *out){
    float inverse[16]={0};inverse[15]=1;
    for(int i=0;i<3;i++)for(int j=0;j<3;j++)inverse[j*4+i]=parent[i*4+j];
    for(int i=0;i<3;i++)for(int j=0;j<3;j++)inverse[12+i]-=inverse[j*4+i]*parent[12+j];
    multiply(inverse,world,out);
}
void tomb_render_begin(TombRenderTrack *r,uint64_t tick,const float origin[3],int room){
    r->reset=!r->tick || r->tick+1!=tick;
    /* Ordinary room crossings stay continuous. Large relocations snap. */
    for(int i=0;i<3;i++)if(fabsf(origin[i]-r->origin[i])>1024)r->reset=1;
    r->previous_room=r->reset?room:r->room;r->room=room;r->tick=tick;memcpy(r->origin,origin,sizeof r->origin);
}
void tomb_render_record(TombRenderTrack *r,uint64_t tick,int node,int parent,int identity,const float world[16]){
    if(node<0 || node>=TOMB_RENDER_NODES || parent>=node || parent < -1)return;
    TombRenderNode *n=r->nodes+node;
    int reset=r->reset || n->tick+1!=tick || n->identity!=identity || n->parent!=parent;
    memcpy(n->previous,n->current,sizeof n->current);memcpy(n->world,world,sizeof n->world);
    if(parent>=0)relative(r->nodes[parent].world,world,n->current);else memcpy(n->current,world,sizeof n->current);
    if(reset)memcpy(n->previous,n->current,sizeof n->current);
    n->parent=parent;n->identity=identity;n->tick=tick;
}
int tomb_render_sample(const TombRenderTrack *r,uint64_t tick,int node,int identity,double alpha,float world[16]){
    if(node<0 || node>=TOMB_RENDER_NODES || !tick || r->tick!=tick)return 0;
    const TombRenderNode *n=r->nodes+node;if(n->tick!=tick || n->identity!=identity)return 0;
    if(alpha>=1){memcpy(world,n->world,sizeof n->world);return 1;}
    float local[16];blend(n->previous,n->current,alpha,local);
    if(n->parent<0)memcpy(world,local,sizeof local);
    else{
        float parent[16];if(!tomb_render_sample(r,tick,n->parent,r->nodes[n->parent].identity,alpha,parent))return 0;
        multiply(parent,local,world);
    }
    return 1;
}
