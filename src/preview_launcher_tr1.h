/* TR1 styling adapter: TITLE supplies the font and bevel palette only.
   No player, level simulation, settings write or sound output is created. */
typedef struct Tr1LauncherStyle {
    TombLevel *data;
    TombVisual *view;
    GLuint *font;
} Tr1LauncherStyle;
static int tr1_launcher_available(void){
    source_available=0;source_legacy=0;
    for(int i=0;i<3;i++)if(source_valid(i))source_available|=1u<<i;
    if(!(source_available&2)){source_legacy=1;if(source_valid(1))source_available|=2;else source_legacy=0;}
    return source_available!=0;
}
static void tr1_launcher_glyph(void *context,const TombTextGlyph *g){
    Tr1LauncherStyle *s=context;int sprite=tomb_hazard_sprite(s->view,190,g->glyph,NULL);if(sprite<0)return;
    const unsigned char *p=s->view->sprite_textures+16*sprite;unsigned tile=u16(p);
    if(tile>=s->view->tile_count)return;
    float x0=(float)(g->x+((int64_t)s16(p+8)*g->scale_x>>16)),y0=(float)(g->y+((int64_t)s16(p+10)*g->scale_y>>16));
    float x1=(float)(g->x+((int64_t)s16(p+12)*g->scale_x>>16)),y1=(float)(g->y+((int64_t)s16(p+14)*g->scale_y>>16));
    float u=p[2]/256.f,v=p[3]/256.f,du=(u16(p+4)+1)/65536.f,dv=(u16(p+6)+1)/65536.f;
    glBindTexture(GL_TEXTURE_2D,s->font[tile]);glBegin(GL_QUADS);
    glTexCoord2f(u,v);glVertex2f(x0,y0);glTexCoord2f(u+du,v);glVertex2f(x1,y0);
    glTexCoord2f(u+du,v+dv);glVertex2f(x1,y1);glTexCoord2f(u,v+dv);glVertex2f(x0,y1);glEnd();
}
static void tr1_launcher_style_free(TombLauncherStyle *style){
    Tr1LauncherStyle *s=style->context;if(!s)return;
    if(s->font){glDeleteTextures((GLsizei)s->view->tile_count,s->font);free(s->font);}
    tomb_visual_free(s->view);tomb_level_free(s->data);free(s);memset(style,0,sizeof *style);
}
static int tr1_launcher_style_open(TombLauncherStyle *style){
    Tr1LauncherStyle *s=calloc(1,sizeof *s);if(!s)return 0;style->context=s;
    char path[MAX_PATH],relative[64],error[256],settings[MAX_PATH];
    snprintf(settings,sizeof settings,"%s/Config/TR1/settings.ini",install_root);
    int deadzone=(int)GetPrivateProfileIntA("VR","deadzone",18,settings);
    if(deadzone<0 || deadzone>50)deadzone=18;
    style->vr_menu_threshold=.6+.4*deadzone/100.0;
    int source=source_choose(source_available,source_preference(settings,"level_source",""));
    snprintf(relative,sizeof relative,"DATA/TITLE.%s",source_legacy && source==1?"PHD":"TBL");
    source_path_for(path,relative,source);
    s->data=tomb_level_load(path,error,sizeof error);if(!s->data)goto fail;
    s->view=tomb_visual_load(s->data,error,sizeof error);if(!s->view)goto fail;
    s->font=calloc(s->view->tile_count,sizeof *s->font);if(!s->font)goto fail;
    unsigned char *rgba=malloc(65536*4);if(!rgba)goto fail;
    const unsigned char *light=s->data->file_data+s->data->item_parsed_bytes+4096;
    for(int glyph=0;glyph<110;glyph++){
        int sprite=tomb_hazard_sprite(s->view,190,glyph,NULL);if(sprite<0)continue;
        unsigned tile=u16(s->view->sprite_textures+16*sprite);
        if(tile>=s->view->tile_count || s->font[tile])continue;
        for(size_t j=0;j<65536;j++){
            if(s->view->rgba_tiles){memcpy(rgba+j*4,s->view->rgba_tiles+((size_t)tile*65536+j)*4,4);continue;}
            unsigned index=s->view->tiles[tile*65536+j],mapped=light[index];
            for(int c=0;c<3;c++)rgba[j*4+c]=(unsigned char)(s->view->palette[mapped*3+c]*255/63);
            rgba[j*4+3]=index?255:0;
        }
        glGenTextures(1,s->font+tile);glBindTexture(GL_TEXTURE_2D,s->font[tile]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,256,256,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba);
    }
    free(rgba);style->glyph=tr1_launcher_glyph;style->palette=s->view->palette;return 1;
fail:tr1_launcher_style_free(style);return 0;
}
