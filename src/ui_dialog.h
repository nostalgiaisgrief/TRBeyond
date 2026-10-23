#ifndef TOMB_UI_DIALOG_H
#define TOMB_UI_DIALOG_H
#include "text.h"
/* Logical 640x480 baselines and panel dimensions recovered from DOS options.
   Panel width excludes the four pixels added by Text_Draw. */
typedef struct TombDialogEntry { int x,y,flags,panel_width,panel_height; } TombDialogEntry;
TOMB_EXPORT int tomb_dialog_layout(int object,int selected,int slots,TombDialogEntry out[32]);
TOMB_EXPORT void tomb_dialog_bounds(const TombDialogEntry *,int text_width,int out[4]);
#endif
