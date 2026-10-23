/* Independent DOS Text_GetWidth 0x3985c and Text_Draw 0x399bc layout.
   Font metrics and mapping are data from 0xc3ec0 / 0xc3f2e in the supplied EXE.
   Glyph art/colour comes from PHD sprite sequence 190, not a platform font. */
#include "text.h"
#include "fixed.h"
static const uint8_t widths[110]={14,11,11,11,11,11,11,13,8,11,12,11,13,13,12,11,12,12,11,12,13,13,13,12,12,11,9,9,9,9,9,9,9,9,5,9,9,5,12,10,9,9,9,8,9,8,9,9,11,9,9,9,12,8,10,10,10,10,10,9,10,10,5,5,5,11,9,10,8,6,6,7,7,3,11,8,13,16,9,4,12,12,7,5,7,7,7,7,7,7,7,7,16,14,14,14,16,16,16,16,16,12,14,8,8,8,8,8,8,8};
static const uint8_t mapping[95]={0,64,66,78,77,74,78,79,69,70,92,72,63,71,62,68,52,53,54,55,56,57,58,59,60,61,73,73,66,74,75,65,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,80,76,81,97,98,77,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,100,101,102,67};
static int glyph(unsigned c){return c<11?(int)c+81:c<16?(int)c+91:c>=32 && c<=126?mapping[c-32]:-1;}
static int scaled(int n,int32_t scale){return tomb_asr(tomb_long((int64_t)n*scale),16);}
int tomb_text_width(const char *text,const TombTextStyle *s) {
    int width=0;
    for(const unsigned char *p=(const unsigned char *)text;*p;p++) {
        unsigned c=*p;if(c>126 || (c>10 && c<32))continue;
        if(c==32)width+=scaled(s->word_spacing,s->scale_x);
        else {int g=glyph(c);width+=scaled(widths[g]+s->letter_spacing,s->scale_x);}
    }
    return (width-s->letter_spacing)&0xfffe;
}
int tomb_text_layout(const char *text,const TombTextStyle *s,int width,int height,TombTextDraw draw,void *user) {
    int x=s->x,y=s->y,count=0,w=tomb_text_width(text,s);
    if(s->flags&TOMB_TEXT_CENTRE)x+=(width-w)/2;else if(s->flags&TOMB_TEXT_RIGHT)x+=width-w;
    if(s->flags&TOMB_TEXT_MIDDLE)y+=height/2;else if(s->flags&TOMB_TEXT_BOTTOM)y+=height;
    for(const unsigned char *p=(const unsigned char *)text;*p;p++) {
        unsigned c=*p;if(c>126 || (c>15 && c<32))continue;
        if(c==32){x+=scaled(s->word_spacing,s->scale_x);continue;}
        int g=glyph(c);TombTextGlyph item={g,x,y,s->scale_x,s->scale_y};if(draw)draw(user,&item);++count;
        /* Accent/overlay symbols intentionally do not advance the draw cursor. */
        if(c!=40 && c!=41 && c!=36 && c!=126)x+=scaled(widths[g]+s->letter_spacing,s->scale_x);
    }
    return count;
}
