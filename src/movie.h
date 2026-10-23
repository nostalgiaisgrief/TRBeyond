#ifndef TOMB_MOVIE_H
#define TOMB_MOVIE_H
#include <stdio.h>
#include <stdint.h>
typedef struct TombMovie { FILE *file; uint32_t width,height,numerator,denominator,frames,rate,channels,audio_bytes; unsigned char *pixels,*audio; } TombMovie;
int tomb_movie_open(TombMovie *,const char *);
int tomb_movie_frame(TombMovie *,unsigned);
void tomb_movie_close(TombMovie *);
#endif
