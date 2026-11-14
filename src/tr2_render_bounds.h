#ifndef TR2_RENDER_BOUNDS_H
#define TR2_RENDER_BOUNDS_H
#include <stdint.h>
/* Tomb2.exe 450d80: visual bounds, not collision bounds. Matrix is the
   original row-major 14-bit fixed view/object transform. Screen bounds use
   downward Y. 0 rejects, -1 needs clipping, 1 is wholly inside the viewport. */
static int tr2_render_bounds(const int16_t bounds[6],const int32_t matrix[12],
    int perspective,int near_depth,int64_t far_depth,int cx,int cy,int xmax,int ymax,const int clip[4]){
    if(matrix[11]>=far_depth || perspective<=0)return 0;
    int left=INT32_MAX,top=INT32_MAX,right=INT32_MIN,bottom=INT32_MIN,count=0;
    for(unsigned i=0;i<8;i++){
        int x=bounds[i&1],y=bounds[2+((i>>1)&1)],z=bounds[4+((i>>2)&1)];
        int32_t v[3];
        for(int row=0;row<3;row++){
            const int32_t *m=matrix+row*4;
            v[row]=(int32_t)((uint32_t)m[3]+(uint32_t)m[0]*(uint32_t)x+
                (uint32_t)m[1]*(uint32_t)y+(uint32_t)m[2]*(uint32_t)z);
        }
        if(v[2]<=near_depth || v[2]>=far_depth)continue;
        int divisor=v[2]/perspective;
        if(!divisor)return -1; /* Degenerate projection: retain conservatively. */
        int sx=v[0]/divisor,sy=v[1]/divisor;
        if(sx<left)left=sx;if(sx>right)right=sx;
        if(sy<top)top=sy;if(sy>bottom)bottom=sy;
        count++;
    }
    if(!count)return 0;
    left+=cx;right+=cx;top+=cy;bottom+=cy;
    if(left>clip[1] || top>clip[3] || right<clip[0] || bottom<clip[2])return 0;
    return count==8 && left>=0 && top>=0 && right<=xmax && bottom<=ymax?1:-1;
}
#endif
