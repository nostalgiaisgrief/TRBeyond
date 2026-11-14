/* Root EXE item motion 4223c3, 4234e0, 4254b0 and 425510. */
#include <stdint.h>
#include <stdlib.h>
typedef struct {int yaw,lift,frame,delay,lowering,closed;} RingItem;
static int approach_zero(int value,int step){
    if(abs(value)<=step)return 0;
    return value+(value<0?step:-step);
}
static void move_item(RingItem *item,const MenuDescriptor *d,int focused,int inspect,int frame,int turning){
    if(inspect){
        item->lowering=0;item->closed=0;
        if(item->lift<16)item->lift++;
        if(item->yaw!=d->yaw){
            int delta=d->yaw-item->yaw;
            item->yaw=(int16_t)((item->yaw+(delta>0 && delta<32768?1024:-1024))&~1023);
        }
    }else if(item->frame==frame && item->lift>0){item->lowering=1;item->lift--;}
    if(!inspect && !item->lift){
        if(focused && !turning)item->yaw=(int16_t)(item->yaw+256);
        else item->yaw=approach_zero(item->yaw,focused?512:256);
    }
    if(item->frame!=frame && (!inspect || (item->lift==16 && item->yaw==d->yaw))){
        if(item->delay)item->delay--;
        else{item->frame+=item->frame<frame?1:-1;item->delay=1;}
    }
}
static int item_pose(const RingItem *item,int start,int end){
    if((end-start)/16==0)return start;
    if(item->lift==16)return end;
    if(!item->lift)return start;
    return item->lowering?end+(start-end)/16*(16-item->lift):start+(end-start)/16*item->lift;
}
static int ring_motion_value(int start,int end,int tick,int duration){
    int step=(end-start)/duration;
    return tick>=duration && step?end:start+step*tick;
}
