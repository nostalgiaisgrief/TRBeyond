#ifndef TR3_TITLE_H
#define TR3_TITLE_H
#include "launcher.h"
int tr3_title_available(const char *root);
int tr3_title_style(TombLauncherStyle *style);
void tr3_title_close(TombLauncherStyle *style);
int tr3_title_begin(int play_music);
void tr3_title_tick(unsigned buttons);
void tr3_title_draw(int width,int height);
void tr3_title_end(void);
#endif
