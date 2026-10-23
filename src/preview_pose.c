#include "preview_pose.h"
#include <math.h>
typedef struct Rotation { double w,x,y,z; } Rotation;
static Rotation multiply(Rotation a,Rotation b) {
    return (Rotation){a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,
        a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
        a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w};
}
static Rotation unpack(const uint16_t a[3]) {
    const double half_turn=3.14159265358979323846/65536.0;
    double x=a[0]*half_turn,y=a[1]*half_turn,z=a[2]*half_turn;
    Rotation qx={cos(x),sin(x),0,0},qy={cos(y),0,sin(y),0},qz={cos(z),0,0,sin(z)};
    return multiply(multiply(qy,qx),qz);
}
void tomb_preview_rotation(const uint16_t a[3],const uint16_t b[3],double t,float out[16]) {
    Rotation p=unpack(a),q=unpack(b);
    double dot=p.w*q.w+p.x*q.x+p.y*q.y+p.z*q.z;
    /* Equivalent Euler representations must follow the same short rotation. */
    if(dot<0) { q=(Rotation){-q.w,-q.x,-q.y,-q.z}; dot=-dot; }
    double u=1-t,v=t;
    if(dot<0.9995) {
        double angle=acos(dot),scale=sin(angle);
        u=sin((1-t)*angle)/scale; v=sin(t*angle)/scale;
    }
    Rotation r={u*p.w+v*q.w,u*p.x+v*q.x,u*p.y+v*q.y,u*p.z+v*q.z};
    double length=sqrt(r.w*r.w+r.x*r.x+r.y*r.y+r.z*r.z);
    double w=r.w/length,x=r.x/length,y=r.y/length,z=r.z/length;
    out[0]=(float)(1-2*(y*y+z*z));out[1]=(float)(2*(x*y+w*z));out[2]=(float)(2*(x*z-w*y));out[3]=0;
    out[4]=(float)(2*(x*y-w*z));out[5]=(float)(1-2*(x*x+z*z));out[6]=(float)(2*(y*z+w*x));out[7]=0;
    out[8]=(float)(2*(x*z+w*y));out[9]=(float)(2*(y*z-w*x));out[10]=(float)(1-2*(x*x+y*y));out[11]=0;
    out[12]=out[13]=out[14]=0;out[15]=1;
}
