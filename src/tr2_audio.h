#ifndef TR2_AUDIO_H
#define TR2_AUDIO_H
#include "tr2_world.h"
int tr2_audio_init(const Tr2Level *level,const char *root);
void tr2_audio_pause(int paused);
void tr2_audio_set_secret_track(int track);
void tr2_audio_stop_music(void);
int tr2_audio_music_cue(void);
int tr2_audio_music_active(void);
void tr2_audio_inventory(int open);
int tr2_audio_inventory_test(void);
void tr2_audio_tick(const Tr2Level *level,const Tr2Actor *actor,const Tr2World *world);
void tr2_audio_pump(void);
void tr2_audio_close(void);
void tr2_audio_menu(const Tr2Level *level,int sound);
void tr2_audio_volume(int music,int effects);
void tr2_audio_cut_music(int track);
void tr2_audio_level_music(int track);
#endif
