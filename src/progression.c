#include "progression.h"
int tomb_music_level_track(int level) {
    static const int tracks[16]={0,57,57,57,57,59,59,59,58,58,59,59,59,58,60,60};
    return level>=0 && level<16?tracks[level]:0;
}
int tomb_music_source(int track) {
    if(track==13)return -2;
    if(track>2 && track<22)return -1;
    if(track>=26 && track<=56)return -3;
    if(track==2)return 2;
    if(track>=22 && track<=25)return track-15;
    if(track>56 && track<64)return track-54;
    return 0;
}
void tomb_progress_action(TombProgress *p,int action,int value,uint16_t flags,int type) {
    if(action==7)p->complete=1;
    else if(action==10 && value>=0 && value<16) {
        unsigned bit=1u<<value;if(!(p->secrets&bit)){p->secrets|=(uint16_t)bit;++p->secret_serial;}
    } else if(action==8) {
        int old=p->music.current_track;unsigned serial=p->music.audio_serial;
        tomb_gym_audio(&p->music,value,flags,type,p->lara_state);
        if(p->music.audio_serial!=serial && p->music.current_track) {
            int source=tomb_music_source(p->music.current_track);
            if(source==-1 || source==-2){p->music.current_track=old;p->music.audio_serial=serial;if(source==-2)++p->secret_serial;}
        }
    }
}
