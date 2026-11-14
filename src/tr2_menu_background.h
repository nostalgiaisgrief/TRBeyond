/* Tomb2.exe 4442c0: radial corner modulation used by the 8x6 tile grid. */
#include <math.h>
static int inventory_background_shade(int x,int y,int w,int h){
 double dx=(double)(x-w/2)/w,dy=(double)(y-h/2)/h;
 int shade=256-(int)(sqrt(dx*dx+dy*dy)*300);
 return shade<0?0:shade>255?255:shade;
}
