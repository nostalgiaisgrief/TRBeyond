#ifndef TR2_RESOLUTION_H
#define TR2_RESOLUTION_H
#include "render_resolution.h"
static inline void tr2_resolution_extent(int setting,int full_w,int full_h,int *w,int *h){
    static const int widths[]={0,800,640,384},heights[]={0,600,480,240};
    if(setting<0 || setting>3)setting=0;
    tomb_render_extent(full_w,full_h,widths[setting],heights[setting],w,h);
}
#endif
