#ifndef TR2_STOPWATCH_H
#define TR2_STOPWATCH_H
#include <stdint.h>
typedef struct {uint32_t times[10],finishes[10],total;} Tr2AssaultScores;
/* Tomb2.exe 426290: equal times follow existing entries; only a qualifying
   finish increments the displayed finish number. Zero marks an empty slot. */
static inline int tr2_assault_record(Tr2AssaultScores *s,uint32_t ticks){
 int i=0;while(i<10 && s->times[i] && ticks>=s->times[i])i++;
 if(i==10)return 0;
 for(int j=9;j>i;j--){s->times[j]=s->times[j-1];s->finishes[j]=s->finishes[j-1];}
 s->times[i]=ticks;s->finishes[i]=++s->total;return 1;
}
static inline int tr2_stopwatch_hand(unsigned ticks,int mesh){
 int seconds=(int)(ticks/30);
 return mesh==2?-(seconds%60)*1092:mesh==3?-(seconds%3600)*182/10:-(seconds/12)*182/10;
}
#endif
