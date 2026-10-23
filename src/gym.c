#include "gym.h"
/* Independently reconstructed DOS 0x18ce8/0x18e28. */
void tomb_gym_audio(TombGym *g,int track,uint16_t flags,int type,int state) {
    if(track<=1 || track>=64)return;
    if(track==28 && (g->tracks[28]&0x100) && state==28)track=29;
    else if((track==37 || track==41) && state!=10)return;
    else if(track==42 && (g->tracks[42]&0x100) && state==10)track=43;
    else if(track==49 && state!=33)return;
    else if(track==50) {
        if(g->tracks[50]&0x100) {
            g->completion_ticks++;
            if(g->completion_ticks==120){g->complete=1;g->completion_ticks=0;}
        } else if(state!=55)return;
    }
    if(g->tracks[track]&0x100)return;
    unsigned mask=flags&0x3e00;
    if(type==2)g->tracks[track]^=mask;
    else if(type==6)g->tracks[track]&=(uint16_t)~mask;
    else if(mask)g->tracks[track]|=mask;
    if((g->tracks[track]&0x3e00)==0x3e00) {
        if(flags&0x100)g->tracks[track]|=0x100;
        if(g->current_track!=track){g->current_track=track;g->audio_serial++;}
    } else {g->current_track=0;g->audio_serial++;}
}
void tomb_gym_begin_tick(TombGym *g) {
    if(g->camera_timer>0 && --g->camera_timer==0)g->camera=0;
}
int tomb_gym_trigger(TombGym *g,const TombLevel *l,size_t index,int state) {
    if(!index){g->last_camera=0;return 1;}
    if(index>=l->floor_count || l->floor_count-index<3 || l->floor_data[index]!=0x8004)return 0;
    /* Validate before changing latches, completion counters or playback. */
    size_t at=index+2;int ended=0,has_camera=0;
    while(at<l->floor_count) {
        uint16_t word=l->floor_data[at++];unsigned type=(word&0x3fff)>>10,value=word&1023;
        if(type==8){if(value<2 || value>=64)return 0;}
        else if(type==1){if(value>=l->camera_count || value>=64 || at>=l->floor_count)return 0;word=l->floor_data[at++];has_camera=1;}
        else if(type!=7)return 0;
        if(word&0x8000){ended=1;break;}
    }
    if(!ended)return 0;
    at=index+2;
    while(at<l->floor_count) {
        uint16_t word=l->floor_data[at++];unsigned type=(word&0x3fff)>>10,value=word&1023;
        if(type==8)tomb_gym_audio(g,(int)value,l->floor_data[index+1],0,state);
        else if(type==7)g->complete=1;
        else {
            word=l->floor_data[at++];int id=(int)value+1;
            if(g->last_camera!=id && !(g->camera_once&(UINT64_C(1)<<value))) {
                g->camera=id;g->camera_timer=(word&255)==1?1:(word&255)*30;
                g->camera_speed=((word&0x3e00)>>6)+1;
                if(word&0x100)g->camera_once|=UINT64_C(1)<<value;
            }
            g->last_camera=id;
        }
        if(word&0x8000)break;
    }
    if(!has_camera){g->last_camera=0;if(!g->camera_timer)g->camera=0;}
    return 1;
}
