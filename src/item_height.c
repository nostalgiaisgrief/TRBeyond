/* Original object height callbacks. IDs are from this DOS release's setup code. */
#include "level.h"
#include "fixed.h"
static int covers(const TombItem *a,int32_t x,int32_t z,int drawbridge)
{
    int32_t ix=tomb_asr(a->x,10),iz=tomb_asr(a->z,10);
    x=tomb_asr(x,10); z=tomb_asr(z,10);
    if(drawbridge) {
        if(a->yaw==0) return x==ix && (z==iz-1 || z==iz-2);
        if(a->yaw==-32768) return x==ix && (z==iz+1 || z==iz+2);
        if(a->yaw==16384) return z==iz && (x==ix-1 || x==ix-2);
        if(a->yaw==-16384) return z==iz && (x==ix+1 || x==ix+2);
    } else {
        if(a->yaw==0) return x==ix && (z==iz || z==iz+1);
        if(a->yaw==-32768) return x==ix && (z==iz || z==iz-1);
        if(a->yaw==16384) return z==iz && (x==ix || x==ix+1);
        if(a->yaw==-16384) return z==iz && (x==ix || x==ix-1);
    }
    return 0;
}
int tomb_item_height(const TombItem *a,int32_t x,int32_t y,int32_t z,int ceiling,int16_t *height)
{
    if(!a || !height || (ceiling!=0 && ceiling!=1)) return 0;
    int32_t surface=a->y;
    switch(a->object) {
    case 6: case 14: case 20: case 21: case 22: case 23: case 29: case 30: case 31: case 32: case 33: case 34:
    case 49: case 50: case 51: case 145: case 146: case 147: case 161: case 162: case 163: case 170: case 177: case 180: case 181: case 182: case 183:
    case 10: case 11: case 12: case 13: case 15: case 28:
    case 43: case 44: case 45: case 46: case 47:
    case 130: case 131: case 132: case 138: case 139: case 140:
    case 18: case 19: case 24: case 27: case 37: case 38: case 52: case 53: case 74: case 75: case 76: case 143:
    case 36: case 110: case 129: case 118: case 122: case 137: case 48: case 56: case 7: case 8: case 9: case 39: case 40: case 55: case 57: case 58: case 59: case 60: case 61: case 62: case 63: case 64:
        return 1; /* Verified null height callbacks; doors mutate sectors. */
    case 35:
        if(!a->state_known) return 0;
        if(a->state!=0 && a->state!=1) return 1;
        surface=tomb_long((int64_t)a->y-512); break;
    case 41:
        if(!a->state_known) return 0;
        if(a->state!=1 || !covers(a,x,z,1)) return 1;
        break;
    case 65: case 66:
        if(!a->state_known) return 0;
        if(a->state!=0 || !covers(a,x,z,0)) return 1;
        if(ceiling?*height>=surface:*height<=surface) return 1;
        break;
    case 68: break;
    case 69: case 70: {
        uint32_t offset;
        if(a->yaw==0) offset=(1024u-(uint32_t)x)&1023;
        else if(a->yaw==-32768) offset=(uint32_t)x&1023;
        else if(a->yaw==16384) offset=(uint32_t)z&1023;
        else offset=(1024u-(uint32_t)z)&1023;
        surface=tomb_long((int64_t)surface+(offset>>(a->object==69?2:1))); break;
    }
    default: return 0; /* Unclassified types are not assumed to lack callbacks. */
    }
    if(ceiling?y>surface:y<=surface)
        *height=tomb_word(tomb_long((int64_t)surface+(ceiling?256:0)));
    return 1;
}
