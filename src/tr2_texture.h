#ifndef TR2_TEXTURE_H
#define TR2_TEXTURE_H
/* 44a4c8 normalizes the low-byte edge flag to 0/255; 449f00 then
   adds/subtracts the inset. Hardware defaults (452954..452985): adjust
   mode 2, nearest adjustment 16, linear adjustment 128. Mode 2 uses
   128 for either filtering setting. Coordinates are 8.8 on a 256 page. */
static inline float tr2_texture_coord(const uint8_t *p){
    int upper=(p[0]&128)!=0;
    return ((int)p[1]*256+(upper?255-128:128))/65536.f;
}
static inline void tr2_texture_filter(GLuint texture,int enabled){
    glBindTexture(GL_TEXTURE_2D,texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,enabled?GL_LINEAR:GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,enabled?GL_LINEAR:GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812f);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812f);
}
#endif
