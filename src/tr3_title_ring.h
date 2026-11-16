#ifndef TR3_TITLE_MENU_H
#define TR3_TITLE_MENU_H
#include <stdint.h>
/* Installed EXE option list 0x4c6c18; item records at 0x4c6910 etc.
   Values describe original assets, not replacement artwork. */
typedef struct {int object,pt_select,x_select,x_idle,y_select,ytrans,ztrans;} Tr3TitleItem;
static const Tr3TitleItem tr3_title_items[5]={
    {158,4640,-4320,0,0,0,384},
    {183,5504,-2560,5632,13312,46,508},
    {182,4832,-5408,0,-3072,-18,350},
    {181,4224,-7232,0,0,-16,444},
    {147,4640,-4320,0,0,16,448}
};
typedef struct {int radius,yaw,camera_y,opening,current,target,remaining,adder;} Tr3TitleRing;
static inline void tr3_title_ring_init(Tr3TitleRing *r,int current){
    *r=(Tr3TitleRing){0,(int16_t)(16384-current*13107),-1536,32,current,0,0,0};
}
static inline void tr3_title_ring_rotate(Tr3TitleRing *r,int direction){
    if(r->opening||r->remaining)return;
    r->target=(r->current+direction+5)%5;r->remaining=24;r->adder=direction<0?546:-546;
}
static inline void tr3_title_ring_tick(Tr3TitleRing *r){
    if(r->opening){r->radius+=21;r->camera_y+=40;r->yaw=(int16_t)(r->yaw-1024);
        if(!--r->opening){r->radius=688;r->camera_y=-256;r->yaw=(int16_t)(-16384-r->current*13107);}}
    if(r->remaining){r->yaw=(int16_t)(r->yaw+r->adder);
        if(!--r->remaining){r->current=r->target;r->yaw=(int16_t)(-16384-r->current*13107);}}
}
#endif
