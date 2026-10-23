#ifndef TOMB_TEXT_H
#define TOMB_TEXT_H
#include "step.h"
#include <stdint.h>
/* DOS Text_Draw alignment flag values; coordinates are text baselines. */
enum { TOMB_TEXT_CENTRE=0x10,TOMB_TEXT_MIDDLE=0x20,TOMB_TEXT_RIGHT=0x80,TOMB_TEXT_BOTTOM=0x100 };
typedef struct TombTextStyle { int32_t x,y,scale_x,scale_y;int16_t letter_spacing,word_spacing;uint16_t flags; } TombTextStyle;
typedef struct TombTextGlyph { int32_t glyph,x,y,scale_x,scale_y; } TombTextGlyph;
typedef void (*TombTextDraw)(void *,const TombTextGlyph *);
TOMB_EXPORT int tomb_text_width(const char *,const TombTextStyle *);
TOMB_EXPORT int tomb_text_layout(const char *,const TombTextStyle *,int width,int height,TombTextDraw,void *);
#endif
