#ifndef TOMB_SAVE_SLOTS_H
#define TOMB_SAVE_SLOTS_H
#include <stdio.h>
enum { TOMB_MANUAL_SLOTS=16 };
static inline void tomb_manual_slot_path(char *path,size_t capacity,const char *folder,int slot,const char *extension){
 snprintf(path,capacity,"%s/slot%02d.%s",folder,slot+1,extension);
}
#endif
