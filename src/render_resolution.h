#ifndef TOMB_RENDER_RESOLUTION_H
#define TOMB_RENDER_RESOLUTION_H
static inline void tomb_render_fit43(int *w,int *h){
    if((int64_t)*w*3>(int64_t)*h*4)*w=*h*4/3;else *h=*w*3/4;
    if(*w<1)*w=1;if(*h<1)*h=1;
}
/* Preset dimensions describe a 4:3 picture, including non-square-pixel
   presets. Preserve vertical sampling and extend horizontally with aspect. */
static inline void tomb_render_extent(int full_w,int full_h,int base_w,int base_h,int *w,int *h){
    *w=full_w;*h=full_h;if(!base_w || !base_h)return;
    *w=(int)(((int64_t)full_w*base_w*3+full_h*2)/((int64_t)full_h*4));if(*w<1)*w=1;
    *h=base_h;
    if(*w>full_w || *h>full_h){double fit=fmin((double)full_w/ *w,(double)full_h/ *h);*w=(int)(*w*fit);*h=(int)(*h*fit);if(*w<1)*w=1;if(*h<1)*h=1;}
}
typedef struct TombRenderSurface {GLuint texture;int width,height;} TombRenderSurface;
static inline void tomb_render_blit(TombRenderSurface *surface,int w,int h,int full_w,int full_h,int x,int y,int clear){
    int pw=1,ph=1;while(pw<w)pw*=2;while(ph<h)ph*=2;
    glPushAttrib(GL_ENABLE_BIT|GL_CURRENT_BIT|GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_POLYGON_BIT|GL_TEXTURE_BIT);
    if(!surface->texture)glGenTextures(1,&surface->texture);
    glBindTexture(GL_TEXTURE_2D,surface->texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812f);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812f);
    if(surface->width!=pw || surface->height!=ph){glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,pw,ph,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);surface->width=pw;surface->height=ph;}
    glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,w,h);
    glViewport(x,y,full_w,full_h);glDisable(GL_SCISSOR_TEST);glDisable(GL_DEPTH_TEST);glDisable(GL_ALPHA_TEST);glDisable(GL_BLEND);glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);glDisable(GL_CLIP_PLANE0);
    if(clear){glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);}
    glEnable(GL_TEXTURE_2D);glColor4f(1,1,1,1);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,full_w,0,full_h,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2i(0,0);glTexCoord2f((float)w/pw,0);glVertex2i(full_w,0);glTexCoord2f((float)w/pw,(float)h/ph);glVertex2i(full_w,full_h);glTexCoord2f(0,(float)h/ph);glVertex2i(0,full_h);glEnd();
    glPopAttrib();
}
static inline void tomb_render_enlarge(TombRenderSurface *surface,int w,int h,int full_w,int full_h){
    tomb_render_blit(surface,w,h,full_w,full_h,0,0,0);
}
static inline void tomb_render_center(TombRenderSurface *surface,int w,int h,int full_w,int full_h){
    if(w!=full_w || h!=full_h)tomb_render_blit(surface,w,h,w,h,(full_w-w)/2,(full_h-h)/2,1);
    glViewport(0,0,full_w,full_h);
}
static inline void tomb_render_surface_free(TombRenderSurface *surface){
    if(surface->texture)glDeleteTextures(1,&surface->texture);memset(surface,0,sizeof *surface);
}
#endif
