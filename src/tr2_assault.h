#ifndef TR2_ASSAULT_H
#define TR2_ASSAULT_H
#include <stdio.h>
typedef struct Tr2TimerGlyph {int x,y,sprite;} Tr2TimerGlyph;
/* DrawAssaultTimer 4219b0: object 261, minutes:seconds.tenths.
   Positions are screen pixels; punctuation advances 8, digits 20. */
static int tr2_assault_glyphs(unsigned ticks,int width,Tr2TimerGlyph glyphs[32]){
 char text[32];snprintf(text,sizeof text,"%u:%02u.%u",ticks/1800,(ticks/30)%60,(ticks%30)/3);
 int x=(width-1)/2-50,count=0;
 for(const char *p=text;*p && count<32;p++){
  int punctuation=*p==':' || *p=='.';
  glyphs[count++]=(Tr2TimerGlyph){x-(punctuation?6:0),36,*p==':'?10:*p=='.'?11:*p-'0'};
  x+=punctuation?8:20;
 }return count;
}
#endif
