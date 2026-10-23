#ifndef TOMB_RENDER_INTERPOLATION_H
#define TOMB_RENDER_INTERPOLATION_H
#include <stddef.h>
#include <stdint.h>
#define TOMB_RENDER_NODES 64
typedef struct TombRenderNode {
    float previous[16],current[16],world[16];
    uint64_t tick;
    int identity,parent;
} TombRenderNode;
typedef struct TombRenderTrack {
    TombRenderNode nodes[TOMB_RENDER_NODES];
    float origin[3];
    uint64_t tick;
    int room,previous_room,reset,yaw;
} TombRenderTrack;
/* Presentation snapshots only. No pointers to mutable gameplay state. */
void tomb_render_begin(TombRenderTrack *,uint64_t tick,const float origin[3],int room);
void tomb_render_record(TombRenderTrack *,uint64_t tick,int node,int parent,int identity,const float world[16]);
int tomb_render_sample(const TombRenderTrack *,uint64_t tick,int node,int identity,double alpha,float world[16]);
#endif
