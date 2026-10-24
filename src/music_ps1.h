#ifndef TOMB_MUSIC_PS1_H
#define TOMB_MUSIC_PS1_H
/* USA SLUS_001.52: 0x8005911c request deduplication; 0x8005923c fade
   state machine. CD seek latency is not simulated for extracted audio. */
typedef struct TombPS1Music {
    int active,pending,gain,fading_out,stopped;
    double clock;
} TombPS1Music;
static inline void tomb_ps1_music_request(TombPS1Music *m,int track) {
    if(track && (track==m->active || track==m->pending))return;
    m->pending=track;m->fading_out=1;
}
static inline void tomb_ps1_music_finished(TombPS1Music *m) {
    m->active=0;m->gain=0;
}
/* One original vertical-blank update; return nonzero when the stream changes. */
static inline int tomb_ps1_music_tick(TombPS1Music *m) {
    if(m->fading_out){
        m->gain=m->gain>32?m->gain-32:0;
        if(!m->gain){
            m->fading_out=0;
            /* 0x800596bc pauses the CD without forgetting its track identity. */
            if(!m->pending){m->stopped=1;return 0;}
            m->active=m->pending;m->pending=0;m->stopped=0;return 1;
        }
    }else if(m->active && !m->stopped && m->gain<129){m->gain+=32;if(m->gain>129)m->gain=129;}
    return 0;
}
#endif
