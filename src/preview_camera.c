#include "preview_camera.h"
#include <math.h>
int tomb_preview_view(const double eye[3],const double target[3],double out[16])
{
    double f[3],r[3],u[3];
    for(int i=0;i<3;i++) f[i]=target[i]-eye[i];
    double n=sqrt(f[0]*f[0]+f[1]*f[1]+f[2]*f[2]);
    double h=sqrt(f[0]*f[0]+f[2]*f[2]);
    if(!isfinite(n) || !isfinite(h) || n<1e-9) return 0;
    for(int i=0;i<3;i++) f[i]/=n;
    /* For a camera looking along original +Z, +X must appear on the right.
       Downward +Y and forward +Z BOTH change sign in GL eye coordinates.
       Flipping Y alone would introduce a reflection (determinant -1). */
    /* At a vertical view the horizontal heading is undefined; keep a valid basis. */
    r[0]=h<1e-9?1:f[2]*n/h; r[1]=0; r[2]=h<1e-9?0:-f[0]*n/h;
    u[0]=r[1]*f[2]-r[2]*f[1];
    u[1]=r[2]*f[0]-r[0]*f[2];
    u[2]=r[0]*f[1]-r[1]*f[0];
    for(int i=0;i<3;i++) { out[4*i]=r[i]; out[4*i+1]=u[i]; out[4*i+2]=-f[i]; out[4*i+3]=0; }
    out[12]=out[13]=out[14]=0; out[15]=1;
    for(int i=0;i<3;i++) { out[12]-=r[i]*eye[i]; out[13]-=u[i]*eye[i]; out[14]+=f[i]*eye[i]; }
    return 1;
}

/* InitWindow 0x3ef3c: half width * cos(80*91) / sin(80*91). */
int tomb_preview_focal(int width) { return width>0?(int)((long long)(width/2)*12553/10529):0; }

double tomb_preview_aspect_focal(int height) {
    return height>0?height*(tomb_preview_focal(640)/480.0):0;
}
