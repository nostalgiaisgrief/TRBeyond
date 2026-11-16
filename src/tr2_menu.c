/* TR2 frontend renderer. Behaviour authority: Tomb2.exe fd9c91d1... .
   The host and Home module own platform and gameplay lifetimes respectively. */
#define WIN32_LEAN_AND_MEAN
#include "tr2_menu.h"
#include "tr2_level.h"
#include "tr2_home.h"
#include "tr2_input.h"
#include "tr2_audio.h"
#include "tr2_menu_data.h"
#include "tr2_menu_motion.h"
#include "tr2_menu_background.h"
#include "detail_options.h"
#include "movie.h"
#include <GL/gl.h>
#include <mmsystem.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "frame_limit.h"
#include "presentation_sync.h"
#include "tr2_resolution.h"
#include "render_distance.h"
#include "tr2_texture.h"
#define PI 3.14159265358979323846
static char asset_root[MAX_PATH],config_path[MAX_PATH];
static const Tr2MenuHost *host;
static Tr2Level *level,*title_level;
static GLuint textures[256],title_textures[256],title_background;
static int width=960,height=720,running,menu_mesh_shade;
static int title_selection,title_page,title_choice,return_item,return_frame,turning,turn_ticks,ring_exit;
static int title_music=10,title_effects=10,ring_held_key,title_framerate;
static int title_resolution;
static int title_filtering=1;
static int title_force43;
static int title_water_style;
static int title_draw_distance;
static TombRenderSurface title_aspect_surface;
static TombRenderSurface title_surface;
static const char *resolution_labels[]={"Full","800 x 600","640 x 480","384 x 240"};
static int home_session,home_playing,home_available,home_keys[256],pending_transition;
static void menu_reset(void);
static double title_angle,ring_open,ring_fraction;
static RingItem ring_items[265],previous_items[265];
static double previous_radius,previous_spin,previous_open,previous_angle,previous_tilt;
static int previous_ring,ring_history;
/* Home exposes the stopwatch; options share the title item renderer. */
static int game_ring,ring_switch,switch_pitch,ring_memory[3],flare_pick;
static int pending_inventory_use=-1;
static double ring_lerp(double a,double b){
    return ring_history && previous_ring==game_ring?a+ring_fraction*(b-a):b;
}
enum { TITLE_ITEMS=5 };
/* Detail Levels is a user-requested addition to the original four-item ring. */
static const int title_ids[TITLE_ITEMS]={133,155,153,154,122};
/* 41fbc3 assigns game-string 56 to the passport descriptor. */
static const char *title_names[TITLE_ITEMS]={"Game","Controls","Detail Levels","Sound","Lara's Home"};
static int title_disabled(int id){return id==122 && !home_session && !home_available;}
enum { MAIN_RING, OPTION_RING, KEY_RING };
static int current_level,ring_target,lock_ring,death_passport,passport_shortcut;
static int ring_items_owned[2][32],owned_count[2];
static const char *ring_name(int index);
static int pickup_model(int id){
 if(id>=135 && id<=151)return id+22;if(id>=174 && id<=196)return id+4;if(id>=205)return id+2;return id;
}
static void inventory_refresh(void){
 static const int order[]={121,135,136,137,138,140,141,139,151,150,149,193,194,195,196,177,176,175,174,206,205};
 const Tr2World *w=tr2_level_world();memset(owned_count,0,sizeof owned_count);if(!w->inventory)return;
 for(unsigned i=0;i<sizeof order/sizeof *order;i++){
  int id=order[i],keys=id>=174;
  if((id==121 || w->inventory->count[id]) && tr2_model(tr2_ring_assets(pickup_model(id),NULL),pickup_model(id)))ring_items_owned[keys][owned_count[keys]++]=id;
  if(id>=136 && id<=141 && w->inventory->count[id+7] && tr2_model(tr2_ring_assets(pickup_model(id+7),NULL),pickup_model(id+7)))ring_items_owned[0][owned_count[0]++]=id+7;
 }
}
static int ring_count(void){if(death_passport)return 3;return home_session?(game_ring==OPTION_RING?TITLE_ITEMS:owned_count[game_ring==KEY_RING]):TITLE_ITEMS;}
static int ring_pickup(int index){return ring_items_owned[game_ring==KEY_RING][index];}
static int ring_id(int index){if(death_passport)return title_ids[index==2?3:index];return home_session && game_ring!=OPTION_RING?pickup_model(ring_pickup(index)):title_ids[index];}
/* Tomb2.exe 42285d dispatches the selection cue by inventory model. */
static int ring_selection_sound(int id){
    if(id==121)return 113;
    if(id>=157 && id<=163)return 114;
    return 111;
}
static int ring_closing(void){return ring_switch || ring_exit==1 || (ring_exit>=4 && !return_item);}
static void asset_path(char path[MAX_PATH],const char *relative){snprintf(path,MAX_PATH,"%s/%s",asset_root,relative);}
static FILE *asset_open(const char *relative){char path[MAX_PATH];asset_path(path,relative);return fopen(path,"rb");}
static void menu_sound(int id);
static void settings_save(void);
static int clamp(int n,int low,int high){return n<low?low:n>high?high:n;}
static void set_texture(uint16_t index){
    const uint8_t *p=level->textures.data+(index&0x7fff)*20;
    glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,textures[tr2_u16(p+2)&0x7fff]);
    if(tr2_u16(p)){glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,0.5f);}else glDisable(GL_ALPHA_TEST);
}
static void texcoord(uint16_t index,int vertex){
    const uint8_t *p=level->textures.data+(index&0x7fff)*20+4+vertex*4;
    glTexCoord2f(tr2_texture_coord(p),tr2_texture_coord(p+2));
}
static uint32_t menu_shade(int shade){unsigned c=(unsigned)clamp((8191-shade)*255/8192,0,255);return c*0x010101u;}
static void mesh_faces(const uint8_t **cursor,const uint8_t *vertices,int count_vertices,int n,int coloured,const uint8_t *lights,const uint8_t *normals,int depth){
    (void)depth;int count=tr2_u16(*cursor);*cursor+=2;
    for(int i=0;i<count;i++){
        const uint8_t *p=*cursor;*cursor+=(n+1)*2;uint16_t texture=tr2_u16(p+n*2);
        if(coloured){glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);}else set_texture(texture);
        glBegin(n==4?GL_QUADS:GL_TRIANGLES);
        for(int j=0;j<n;j++){
            int index=tr2_u16(p+j*2);if(index>=count_vertices)continue;
            const uint8_t *v=vertices+index*6;
            uint32_t rgb=lights?menu_shade(tr2_i16(lights+index*2)+menu_mesh_shade):0xffffff;
            float red=((rgb>>16)&255)/255.f,green=((rgb>>8)&255)/255.f,blue=(rgb&255)/255.f;
            if(normals)glNormal3f(tr2_i16(normals+index*6)/16384.f,tr2_i16(normals+index*6+2)/16384.f,tr2_i16(normals+index*6+4)/16384.f);
            if(coloured){const uint8_t *c=level->palette16+(texture>>8)*4;glColor3f(c[0]/255.f*red,c[1]/255.f*green,c[2]/255.f*blue);}
            else{glColor3f(red,green,blue);texcoord(texture,j);}
            glVertex3s(tr2_i16(v),tr2_i16(v+2),tr2_i16(v+4));
        }glEnd();
    }
}
static void mesh_geometry(uint32_t index,int depth){
    const uint8_t *p=level->mesh_data.data+tr2_u32(level->mesh_offsets.data+index*4)+10;
    int vertices_count=tr2_u16(p);p+=2;const uint8_t *vertices=p;p+=vertices_count*6;
    int normals=tr2_i16(p);p+=2;const uint8_t *lights=normals<0?p:NULL,*normal_data=normals>0?p:NULL;p+=normals<0?-normals*2:normals*6;
    glPushAttrib(GL_ENABLE_BIT);if(normal_data){glEnable(GL_LIGHTING);glEnable(GL_NORMALIZE);glEnable(GL_COLOR_MATERIAL);glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);}else glDisable(GL_LIGHTING);
    mesh_faces(&p,vertices,vertices_count,4,0,lights,normal_data,depth);mesh_faces(&p,vertices,vertices_count,3,0,lights,normal_data,depth);
    mesh_faces(&p,vertices,vertices_count,4,1,lights,normal_data,depth);mesh_faces(&p,vertices,vertices_count,3,1,lights,normal_data,depth);
    glPopAttrib();
}
static void frame_angles(const uint8_t **cursor,const uint8_t *end,int angles[3]){
    if(*cursor+2>end)return;
    uint16_t a=tr2_u16(*cursor);*cursor+=2;unsigned x=0,y=0,z=0;
    switch(a>>14){
    case 1:x=a&1023;break;
    case 2:y=a&1023;break;
    case 3:z=a&1023;break;
    default:
        if(*cursor+2>end)return;
        {uint16_t b=tr2_u16(*cursor);*cursor+=2;x=(a>>4)&1023;y=((a&15)<<6)|(b>>10);z=b&1023;}break;
    }
    angles[0]=x;angles[1]=y;angles[2]=z;
}
static void ring_joint(const uint8_t **frame,const uint8_t **next,const uint8_t *end,double t){
    int a[3]={0},b[3]={0};frame_angles(frame,end,a);frame_angles(next,end,b);
    double angle[3];
    for(int i=0;i<3;i++)angle[i]=(a[i]+t*(((b[i]-a[i]+1536)%1024)-512))*360/1024;
    glRotated(angle[1],0,1,0);glRotated(angle[0],1,0,0);glRotated(angle[2],0,0,1);
}
static void model(uint32_t id,double frame_index,double y,double z,double yaw,uint64_t mask,double pitch){
 const uint8_t *m=tr2_model(level,id);if(!m)return;
 unsigned count=tr2_u16(m+4),start=tr2_u16(m+6),anim=tr2_u16(m+16);
 if(!count || count>64 || anim>=level->animations.count)return;
 const uint8_t *a=level->animations.data+anim*32;
 size_t offset=tr2_u32(m+12)+(size_t)frame_index*a[5]*2;
 if(offset+18>level->frames.count*2)return;
 const uint8_t *frame=level->frames.data+offset,*end=level->frames.data+level->frames.count*2;
 glPushMatrix();glTranslated(0,y,z);glRotated(yaw,0,1,0);glRotated(pitch*(360.0/65536),1,0,0);
 double t=frame_index-floor(frame_index);
 const uint8_t *next=frame;
 if(t>0 && (size_t)(end-frame)>=a[5]*2u+18)next+=a[5]*2;
 double origin[3];
 for(int i=0;i<3;i++){int p=tr2_i16(frame+12+2*i);origin[i]=p+t*(tr2_i16(next+12+2*i)-p);}
 glTranslated(origin[0],origin[1],origin[2]);frame+=18;next+=18;
 int stack=0;ring_joint(&frame,&next,end,t);if(mask&1)mesh_geometry(start,0);
 unsigned tree=tr2_u32(m+8);
 for(unsigned i=1;i<count;i++){
  const uint8_t *bone=level->trees.data+(tree+(i-1)*4)*4;unsigned flags=tr2_u32(bone);
  if((flags&1) && stack){glPopMatrix();stack--;}if(flags&2){glPushMatrix();stack++;}
  glTranslatef((float)tr2_i32(bone+4),(float)tr2_i32(bone+8),(float)tr2_i32(bone+12));ring_joint(&frame,&next,end,t);
  if(id==121 && count-i>=2 && count-i<=4)glRotatef(tr2_stopwatch_hand(tr2_home_stopwatch_ticks(),(int)(count-i))*(360.f/65536),0,0,1);
  if(mask&(1ull<<i))mesh_geometry(start+i,0);
 }
 while(stack-->0)glPopMatrix();glPopMatrix();
}
static void init_geometry(void){
    glGenTextures((GLsizei)level->tile_count,textures);uint8_t pixels[256*256*4];
    for(uint32_t tile=0;tile<level->tile_count;tile++){
        for(int i=0;i<65536;i++){
            uint16_t c=tr2_u16(level->tiles16+tile*131072+i*2);
            pixels[i*4]=(uint8_t)(((c>>10)&31)*255/31);pixels[i*4+1]=(uint8_t)(((c>>5)&31)*255/31);
            pixels[i*4+2]=(uint8_t)((c&31)*255/31);pixels[i*4+3]=(c&0x8000)?255:0;
        }
        tr2_texture_filter(textures[tile],title_filtering);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,256,256,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    }
}
static int picture_load(const char *relative,GLuint *texture){
    FILE *f=asset_open(relative);if(!f)return 0;fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);
    if(size<897 || size>16000000){fclose(f);return 0;}uint8_t *data=malloc((size_t)size);if(!data){fclose(f);return 0;}
    int ok=fread(data,1,(size_t)size,f)==(size_t)size;fclose(f);
    int w=tr2_u16(data+8)-tr2_u16(data+4)+1,h=tr2_u16(data+10)-tr2_u16(data+6)+1,stride=tr2_u16(data+66);
    if(!ok || data[0]!=10 || data[2]!=1 || data[3]!=8 || data[65]!=1 || data[size-769]!=12 || w<1 || h<1 || w>4096 || h>4096 || stride<w || stride>8192){free(data);return 0;}
    uint8_t *rgb=malloc((size_t)w*h*3);if(!rgb){free(data);return 0;}size_t pos=128;int run=0,value=0;
    for(int y=0;y<h && ok;y++)for(int x=0;x<stride;x++){
        if(!run){if(pos>=(size_t)size-769){ok=0;break;}value=data[pos++];run=1;if((value&192)==192){run=value&63;if(!run || pos>=(size_t)size-769){ok=0;break;}value=data[pos++];}}
        run--;if(x<w)memcpy(rgb+((size_t)y*w+x)*3,data+size-768+value*3,3);
    }
    if(ok){glGenTextures(1,texture);glBindTexture(GL_TEXTURE_2D,*texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812f);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812f);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,rgb);}
    free(rgb);free(data);return ok;
}
static int title_picture(void){return picture_load("DATA/TITLE.PCX",&title_background);}
static void menu_frame(const Tr2Level *font,const GLuint *pages,float x,float y,float w,float h,int fill){
    if(fill){glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glColor4f(0,0,0,.5f);glRectf(x,y,x+w,y+h);glDisable(GL_BLEND);}
    int first=-1;for(uint32_t i=0;i<font->sequences.count;i++){const uint8_t *s=font->sequences.data+i*8;if(tr2_i32(s)==100){first=tr2_u16(s+6);break;}}
    if(first<0)return;
    float left=x+4,top=y+4,right=left+w-8,bottom=top+h-8;
    float px[8]={left,right,right,left,left,right,left,left},py[8]={top,top,bottom,bottom,top,top,bottom,top};
    glEnable(GL_TEXTURE_2D);glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.1f);glColor3f(1,1,1);
    for(int i=0;i<8;i++){
        const uint8_t *s=font->sprites.data+(first+i)*16;float sx=(i==4 || i==6)?(w-8)/8:1,sy=(i==5 || i==7)?(h-8)/8:1;
        float x0=px[i]+tr2_i16(s+8)*sx,x1=px[i]+tr2_i16(s+12)*sx,y0=py[i]+tr2_i16(s+10)*sy,y1=py[i]+tr2_i16(s+14)*sy;
        float u=(s[2]+.5f)/256,v=(s[3]+.5f)/256,du=(tr2_u16(s+4)/256.f-1)/256,dv=(tr2_u16(s+6)/256.f-1)/256;
        glBindTexture(GL_TEXTURE_2D,pages[tr2_u16(s)]);glBegin(GL_QUADS);glTexCoord2f(u,v);glVertex2f(x0,y0);glTexCoord2f(u+du,v);glVertex2f(x1,y0);glTexCoord2f(u+du,v+dv);glVertex2f(x1,y1);glTexCoord2f(u,v+dv);glVertex2f(x0,y1);glEnd();
    }
}
static void font_text_at(const Tr2Level *font,const GLuint *pages,float origin,float y,const char *text,int selected,int centered){
    if(!font)return;int first=-1;for(uint32_t i=0;i<font->sequences.count;i++){const uint8_t *p=font->sequences.data+i*8;if(tr2_u32(p)==255){first=tr2_u16(p+6);break;}}
    if(first<0)return;float scale=1.f,total=(float)menu_text_width(text);
    float x=origin-(centered<0?total:centered?total/2:0);
    glPushAttrib(GL_ENABLE_BIT|GL_CURRENT_BIT);glDisable(GL_LIGHTING);glEnable(GL_TEXTURE_2D);glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.1f);
    glColor3f(selected<0?.4f:1,selected<0?.4f:1,selected<0?.4f:1);
    if(selected>0)menu_frame(font,pages,x-2,y-14,total+4,16,1);
    for(const unsigned char *c=(const unsigned char *)text;*c;c++){
        if(*c==' '){x+=6*scale;continue;}
        int secret=*c>=127 && *c<=129,index=first+menu_glyph(*c);
        if(secret){
            index=-1;
            for(unsigned n=0;n<font->sequences.count;n++){const uint8_t *seq=font->sequences.data+n*8;
                if(tr2_i32(seq)==190+*c-127){index=tr2_u16(seq+6);break;}}
        }
        if((uint32_t)index>=font->sprites.count)continue;const uint8_t *p=font->sprites.data+index*16;
        int tile=tr2_u16(p);float w=tr2_u16(p+4)/256.f,h=tr2_u16(p+6)/256.f;
        glBindTexture(GL_TEXTURE_2D,pages[tile]);float u=(p[2]+.5f)/256.f,v=(p[3]+.5f)/256.f,du=(w-1)/256,dv=(h-1)/256;
        float sprite_scale=secret?0x1be8/65536.f:scale;
        float origin_x=x+(secret?10:*c>='0' && *c<='9'?(12-menu_widths[menu_glyph(*c)])/2:0);
        float x0=origin_x+tr2_i16(p+8)*sprite_scale,x1=origin_x+tr2_i16(p+12)*sprite_scale;
        float y0=y+tr2_i16(p+10)*sprite_scale,y1=y+tr2_i16(p+14)*sprite_scale;
        glBegin(GL_QUADS);glTexCoord2f(u,v);glVertex2f(x0,y0);glTexCoord2f(u+du,v);glVertex2f(x1,y0);
        glTexCoord2f(u+du,v+dv);glVertex2f(x1,y1);glTexCoord2f(u,v+dv);glVertex2f(x0,y1);glEnd();x+=menu_text_advance(*c)*scale;
    }glPopAttrib();
}
void tr2_draw_ammo(const Tr2Level *font,const unsigned *pages,int w,int h,int count){
    char text[16];snprintf(text,sizeof text,"%5d",count);
    for(char *p=text;*p;p++)if(*p>='0' && *p<='9')*p=(char)(1+*p-'0');
    glPushAttrib(GL_ENABLE_BIT|GL_CURRENT_BIT);glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,w, h,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    font_text_at(font,pages,(float)w-10,35,text,0,-1);
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glPopAttrib();
}
static void menu_text_at(float x,float y,const char *text,int selected,int centered){
    font_text_at(title_level,title_textures,x,y,text,selected,centered);
}
static void title_text(float y,const char *text,int selected){menu_text_at(320,y,text,selected,1);}
static int ring_motion_tick(void){return (int)lround((ring_closing()?1-ring_open:ring_open)*(switch_pitch?24:32));}
static int ring_radius(void){return ring_motion_value(ring_closing()?688:0,ring_closing()?0:688,ring_motion_tick(),switch_pitch?24:32);}
static double ring_spin(void){return ring_motion_value(ring_closing()?0:32768,ring_closing()?-32768:0,ring_motion_tick(),switch_pitch?24:32)*(360.0/65536);}
static void remember_ring(void){
    previous_radius=ring_radius();previous_spin=ring_spin();previous_open=ring_open;
    previous_angle=title_angle;previous_ring=game_ring;previous_tilt=switch_pitch*(1-ring_open);
    memcpy(previous_items,ring_items,sizeof ring_items);ring_history=1;
}
static void menu_projection(void){
    glEnable(GL_DEPTH_TEST);glClear(GL_DEPTH_BUFFER_BIT);glDisable(GL_CULL_FACE);glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.5f);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();/* EXE 4220dd/424fe0/4250f0: FOV 80, radius 688, camera (0,-256,1286). */
    double edge=128*tan((14560.0/65536)*PI);glFrustum(-edge,edge,-edge*.75,edge*.75,128,8192);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    double camera_y=switch_pitch?-256:-1536+1280*ring_lerp(previous_open,ring_open);
    double pitch=-atan2(-96-camera_y,598)+(home_session?0:1024.0*2*PI/65536)+ring_lerp(previous_tilt,switch_pitch*(1-ring_open))*PI/4,cp=cos(pitch),sp=sin(pitch);
    GLfloat view[16]={-1,0,0,0, 0,(GLfloat)-cp,(GLfloat)sp,0, 0,(GLfloat)sp,(GLfloat)cp,0, 0,0,0,1};
    glMultMatrixf(view);glTranslated(0,-camera_y,-598-ring_lerp(previous_radius,ring_radius()));
    for(int i=0;i<8;i++)glDisable(GL_LIGHT0+i);
    /* Root ring light (-1536,256,1024), divider24576 (425150). */
    for(int i=0;i<2;i++){float sign=i?1.f:-1.f;GLfloat color[4]={sign*2/3,sign*2/3,sign*2/3,1},direction[4]={1536*sign,-256*sign,-1024*sign,0};glLightfv(GL_LIGHT0+i,GL_DIFFUSE,color);glLightfv(GL_LIGHT0+i,GL_POSITION,direction);glEnable(GL_LIGHT0+i);}
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_FALSE);
}
static void menu_flat(void){glDisable(GL_DEPTH_TEST);glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);glDisable(GL_LIGHTING);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,640,480,0,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();}
static void inventory_background(void){
 /* Hardware path 443e40: first textured quad of object 223, repeated 8x6.
    Borrow the current level's pages; never keep them across a level close. */
 unsigned pages[256];Tr2Level *assets=tr2_home_menu_assets(pages);
 const uint8_t *m=assets?tr2_model(assets,223):NULL;if(!m)return;
 unsigned mesh=tr2_u16(m+6);if(mesh>=assets->mesh_offsets.count)return;
 const uint8_t *p=assets->mesh_data.data+tr2_u32(assets->mesh_offsets.data+mesh*4)+10;
 int vertices=tr2_u16(p);p+=2+vertices*6;int normals=tr2_i16(p);p+=2+(normals<0?-normals*2:normals*6);
 if(!tr2_u16(p))return;unsigned texture=tr2_u16(p+10)&0x7fff;if(texture>=assets->textures.count)return;
 const uint8_t *t=assets->textures.data+texture*20;unsigned tile=tr2_u16(t+2)&0x7fff;if(tile>=assets->tile_count)return;
 float u[4]={(t[5]+.5f)/256,(t[13]+.5f)/256,(t[13]+.5f)/256,(t[5]+.5f)/256};
 float v[4]={(t[7]+.5f)/256,(t[7]+.5f)/256,(t[15]+.5f)/256,(t[15]+.5f)/256};
 menu_flat();glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,pages[tile]);glShadeModel(GL_SMOOTH);
 for(int row=0;row<6;row++)for(int col=0;col<8;col++){
  int x[4]={col*width/8,(col+1)*width/8,(col+1)*width/8,col*width/8};
  int y[4]={row*height/6,row*height/6,(row+1)*height/6,(row+1)*height/6};
  glBegin(GL_QUADS);for(int i=0;i<4;i++){float shade=inventory_background_shade(x[i],y[i],width,height)/255.f;glColor3f(shade,shade,shade);glTexCoord2f(u[i],v[i]);glVertex2f(x[i]*640.f/width,y[i]*480.f/height);}glEnd();
 }
}
static double display_item_pose(const RingItem *prior,const RingItem *now,int start,int end){
    return ring_lerp(item_pose(prior,start,end),item_pose(now,start,end));
}
static void draw_ring_item(int id,double angle){
    const MenuDescriptor *d=menu_descriptor(id);if(!d)return;
    const RingItem *item=ring_items+id;
    const RingItem *prior=previous_items+id;
    int passport=id==133 && item->lift && !item->closed;
    int selected=fabs(remainder(angle,2*PI))<1e-6 && !turning;
    menu_mesh_shade=title_disabled(id)?6656:selected?4096:5120;float base=(8191-menu_mesh_shade)/4096.f;GLfloat ambient[4]={base,base,base,1};glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambient);
    glPushMatrix();
    angle+=ring_lerp(remainder(title_angle-previous_angle,2*PI),0);
    glRotated(-90+angle*180/PI+ring_lerp(previous_spin,ring_spin()),0,1,0);
    glTranslated(ring_lerp(previous_radius,ring_radius()),0,0);glRotatef(90,0,1,0);
    glRotated(display_item_pose(prior,item,0,d->point_pitch)*(360.0/65536),1,0,0);
    double frame=ring_lerp(prior->display_frame,item->display_frame);
    if(id==133 && !passport)frame=0;
    model(passport?120u:(unsigned)id,frame,display_item_pose(prior,item,0,d->y),display_item_pose(prior,item,0,d->z),
          (item->yaw+ring_lerp((int16_t)(prior->yaw-item->yaw),0))*(360.0/65536),passport?passport_mask((int)floor(frame+.5)):d->mask,
          display_item_pose(prior,item,d->pitch,d->selected_pitch));
    glPopMatrix();menu_mesh_shade=0;
}
static const int control_defaults[14]={VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,VK_DELETE,VK_NEXT,VK_SHIFT,VK_MENU,VK_CONTROL,VK_SPACE,VK_OEM_2,VK_NUMPAD0,VK_END,VK_ESCAPE};
static const int control_alternate[14]={VK_NUMPAD8,VK_NUMPAD2,VK_NUMPAD4,VK_NUMPAD6,VK_NUMPAD7,VK_NUMPAD9,VK_NUMPAD1,VK_ADD,VK_SEPARATOR,VK_NUMPAD3,VK_SUBTRACT,VK_NUMPAD0,VK_NUMPAD5,VK_DECIMAL};
static const char *control_names[14]={"Run","Back","Left","Right","Step Left","Step Right","Walk","Jump","Action","Draw Weapon","Flare","Look","Roll","Inventory"};
static int control_keys[14],control_capture=-1,control_layout;
static int control_valid(int key){return key>0 && key<256 && key!=VK_F5 && key!=VK_F6 && key!=VK_F2 && key!=VK_F3 && key!=VK_F4 && key!=VK_LWIN && key!=VK_RWIN;}
static int control_matches(int action,int key){
    if(control_keys[action]==key)return 1;
    /* Root Key action 0x44d9b0: user first, then default unless a user
       action claims that default key. Modifier aliases normalize in window_proc. */
    for(int i=0;i<14;i++)if(control_keys[i]==control_defaults[action])return 0;
    return control_defaults[action]==key;
}
static int controls_bind(int row,int key){
    if(!control_valid(key))return 0;for(int i=0;i<14;i++)if(i!=row && control_keys[i]==key)control_keys[i]=control_keys[row];
    control_keys[row]=key;settings_save();return 1;
}
static void control_key_name(int key,char text[48]){
    const char *name=NULL;
    switch(key){case VK_SHIFT:name="Shift";break;case VK_CONTROL:name="Ctrl";break;case VK_MENU:name="Alt";break;case VK_ADD:name="Num +";break;case VK_SUBTRACT:name="Num -";break;case VK_DECIMAL:name="Num Del";break;case VK_SEPARATOR:name="Num Ent";break;case VK_NUMPAD0:name="Num 0";break;case VK_OEM_2:name="/";break;}
    if(name){snprintf(text,48,"%s",name);return;}
    UINT scan=MapVirtualKeyA((UINT)key,MAPVK_VK_TO_VSC);LONG flags=(LONG)(scan<<16);
    if(key==VK_LEFT || key==VK_RIGHT || key==VK_UP || key==VK_DOWN || key==VK_DELETE || key==VK_NEXT || key==VK_HOME || key==VK_END || key==VK_INSERT || key==VK_PRIOR)flags|=1L<<24;
    if(!GetKeyNameTextA(flags,text,48))text[0]=0;
}
#include "tr2_menu_runtime.h"
#include "tr2_menu_movie.h"
