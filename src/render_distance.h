#ifndef TOMB_RENDER_DISTANCE_H
#define TOMB_RENDER_DISTANCE_H
#include <math.h>
enum { DRAW_ORIGINAL, DRAW_NO_FOG, DRAW_MAX_FOG, DRAW_SMOOTH_FOG, DRAW_DISTANCE_COUNT };
static const char *const draw_distance_names[DRAW_DISTANCE_COUNT]={"Original","Max Distance","Max Distance with Fog","Max Distance with Smooth Fog"};
static inline int tomb_camera_distance(double x,double y,double z){
    return (int)sqrt(x*x+y*y+z*z);
}
static inline int tomb_mesh_camera_distance(const double m[16],double x,double y,double z){
    return tomb_camera_distance(m[0]*x+m[4]*y+m[8]*z+m[12],
                                m[1]*x+m[5]*y+m[9]*z+m[13],
                                m[2]*x+m[6]*y+m[10]*z+m[14]);
}
/* Shared extended range: whole-level diagonal plus eight tiles. */
static inline double tomb_render_max_distance(const double lo[3],const double hi[3]){
    double x=hi[0]-lo[0],y=hi[1]-lo[1],z=hi[2]-lo[2];
    return fmax(20480,sqrt(x*x+y*y+z*z)+8192);
}
/* Do not brighten surfaces already darker than the extended fog ceiling. */
static inline int tomb_render_fog_shade(int shade,int depth){
    const int limit=7864;
    if(depth<=12288 || shade>=limit)return shade;
    int darkness=depth-12288;
    return darkness>=limit-shade?limit:shade+darkness;
}
#endif
