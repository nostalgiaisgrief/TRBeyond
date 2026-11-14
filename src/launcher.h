#ifndef TOMB_LAUNCHER_H
#define TOMB_LAUNCHER_H
#include "text.h"
#include <stddef.h>
#include <string.h>

/* Launcher services own no gameplay state. A game supplies detection, menu
   styling and its entry point; only the selected entry point is invoked. */
typedef struct TombLauncherStyle {
    void *context;
    TombTextDraw glyph;
    const unsigned char *palette;
    double vr_menu_threshold;
    void (*draw_selector)(int,unsigned,int,int);
} TombLauncherStyle;
typedef struct TombLauncherGame {
    const char *id,*name;
    int (*available)(void);
    int (*open_style)(TombLauncherStyle *);
    void (*close_style)(TombLauncherStyle *);
    int (*start)(int,char **);
} TombLauncherGame;
typedef struct TombLauncherSelection {
    unsigned available;
    int current,style,confirmed,cancelled;
} TombLauncherSelection;
static inline TombLauncherSelection tomb_launcher_select(const TombLauncherGame *games,int count,const char *last){
    TombLauncherSelection s={0,-1,-1,0,0};
    for(int i=0;i<count;i++)if(games[i].start && games[i].available && games[i].available()){
        s.available|=1u<<i;if(s.current<0)s.current=i;
    }
    for(int i=0;i<count;i++)if((s.available&(1u<<i)) && !strcmp(last,games[i].id)){s.current=i;break;}
    s.style=s.current;return s;
}
static inline void tomb_launcher_move(TombLauncherSelection *s,int count,int direction){
    if(s->current<0)return;
    for(int i=1;i<=count;i++){
        int next=(s->current+direction*i+count)%count;
        if(s->available&(1u<<next)){s->current=next;return;}
    }
}
#endif
