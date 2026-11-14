#include "tr2_runtime.h"
#include <math.h>
#include <string.h>

/* HairControl's six-segment integration, 0x42158e..0x4218e4. Geometry and
   body sampling are inputs so integer physics can be checked independently. */
void tr2_hair_step(Tr2Hair *h,const int bones[6][3],const Tr2Sphere body[5],int water_status,int water_height,int floor,int wind){
    for(int i=1;i<7;i++){
        int old[3];memcpy(old,h->point[i],sizeof old);
        for(int j=0;j<3;j++)h->point[i][j]+=h->velocity[i][j]*3/4;
        if(water_status==0){
            h->point[i][1]+=10;
            if(water_height!=TR2_NO_HEIGHT && h->point[i][1]>water_height)h->point[i][1]=water_height;
            else if(h->point[i][1]>floor)h->point[i][1]=floor;
            else h->point[i][2]+=wind;
        }else if(water_status==1 || water_status==2 || water_status==4){
            if(h->point[i][1]<water_height)h->point[i][1]=water_height;
            else if(h->point[i][1]>floor)h->point[i][1]=floor;
        }
        for(int j=0;j<5;j++){
            int dx=h->point[i][0]-body[j].x,dy=h->point[i][1]-body[j].y,dz=h->point[i][2]-body[j].z;
            int64_t square=(int64_t)dx*dx+(int64_t)dy*dy+(int64_t)dz*dz,r=body[j].radius;
            if(square<r*r){int length=(int)sqrt((double)square);if(!length)length=1;
                h->point[i][0]=body[j].x+(int)(dx*r)/length;h->point[i][1]=body[j].y+(int)(dy*r)/length;h->point[i][2]=body[j].z+(int)(dz*r)/length;}
        }
        int dx=h->point[i][0]-h->point[i-1][0],dy=h->point[i][1]-h->point[i-1][1],dz=h->point[i][2]-h->point[i-1][2];
        h->yaw[i-1]=tr2_word(tr2_atan(dz,dx));h->pitch[i-1]=tr2_word(-tr2_atan((int)sqrt((double)((int64_t)dx*dx+(int64_t)dz*dz)),dy));
        int offset[3];tr2_rotate_offset(h->yaw[i-1],h->pitch[i-1],bones[i==6?4:i-1],offset);
        for(int j=0;j<3;j++){h->point[i][j]=h->point[i-1][j]+offset[j];h->velocity[i][j]=h->point[i][j]-old[j];}
    }
}

