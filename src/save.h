#ifndef TOMB_SAVE_H
#define TOMB_SAVE_H
#include "playtest.h"
#include "follow_camera.h"
/* Versioned reconstruction saves, not DOS SAVEGAME.* files. */
int tomb_save_write(const char *,int,const TombPlaytest *,const TombObjects *,const TombFollowCamera *,const TombInventory *);
int tomb_save_level(const char *);
int tomb_save_read(const char *,int,TombPlaytest *,TombObjects *,TombFollowCamera *,TombInventory *);
#endif
