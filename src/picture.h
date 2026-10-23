#ifndef TOMB_PICTURE_H
#define TOMB_PICTURE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "asset.h"
/* Presentation cache padded to the existing 1024x512 menu texture. */
static unsigned char *tomb_picture_load(const char *path){
 FILE *f=fopen(path,"rb");if(!f)return NULL;
 unsigned char header[128],palette[768],*rgb=NULL,*pixels=NULL;
 if(fread(header,1,16,f)!=16)goto done;
 rgb=calloc(1024*512,3);if(!rgb)goto done;
 if(!memcmp(header,"TRBIMG1\0",8)){
  unsigned w=tomb_asset_u32(header+8),h=tomb_asset_u32(header+12);
  if(!w || !h || w>2048 || h>2048)goto fail;
  size_t size=(size_t)w*h*3;pixels=malloc(size);if(!pixels || fread(pixels,1,size,f)!=size || fgetc(f)!=EOF)goto fail;
  for(unsigned y=0;y<480;y++)for(unsigned x=0;x<640;x++)memcpy(rgb+((size_t)y*1024+x)*3,pixels+((size_t)(y*h/480)*w+x*w/640)*3,3);
 }else{
  if(fseek(f,0,SEEK_SET) || fread(header,1,128,f)!=128 || header[0]!=10 || header[3]!=8 || header[65]!=1)goto fail;
  unsigned x0=header[4]|header[5]<<8,y0=header[6]|header[7]<<8,x1=header[8]|header[9]<<8,y1=header[10]|header[11]<<8,stride=header[66]|header[67]<<8;
  if(x1-x0!=639 || y1-y0!=479 || stride<640 || stride>2048 || fseek(f,-768,SEEK_END) || fread(palette,1,768,f)!=768 || fseek(f,128,SEEK_SET))goto fail;
  int remaining=0,value=0;
  for(unsigned y=0;y<480;y++)for(unsigned x=0;x<stride;x++){
   if(!remaining){value=fgetc(f);if(value<0)goto fail;if((value&192)==192){remaining=value&63;value=fgetc(f);if(!remaining || value<0)goto fail;}else remaining=1;}
   remaining--;if(x<640)memcpy(rgb+((size_t)y*1024+x)*3,palette+value*3,3);
  }
 }
 goto done;
fail:free(rgb);rgb=NULL;
done:free(pixels);fclose(f);return rgb;
}
#endif
