/* TR2 Home integration. Behaviour authority: PC/Tomb2.exe fd9c91d1... .
 * The abandoned viewer supplies renderer research, not application/menu policy. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "tr2_home.h"
#include "tr2_enemy.h"
#include "tr2_assault.h"
#include "tr2_water_visual.h"
#include "tr2_audio.h"
#include "tr2_presentation.h"
#include "tr2_world.h"
#include "render_interpolation.h"
#include "tr2_render_bounds.h"
#include "tr2_resolution.h"
#include "render_distance.h"
#include "tr2_texture.h"
static Tr2Level *level;
static int texture_filtering=1;
static int draw_distance_setting;
static double level_max_distance=20480;
void tr2_home_draw_distance(int setting){draw_distance_setting=setting>=0 && setting<DRAW_DISTANCE_COUNT?setting:DRAW_ORIGINAL;}
static double render_far_distance(void){return draw_distance_setting?level_max_distance:20480;}
static void init_draw_distance(void){
    double lo[3]={INFINITY,INFINITY,INFINITY},hi[3]={-INFINITY,-INFINITY,-INFINITY};
    for(uint32_t i=0;i<level->room_count;i++){
        const Tr2Room *r=level->rooms+i;
        double a[3]={r->x,r->top,r->z},b[3]={(double)r->x+r->nx*1024,r->bottom,(double)r->z+r->nz*1024};
        for(int k=0;k<3;k++){lo[k]=fmin(lo[k],a[k]);hi[k]=fmax(hi[k],b[k]);}
    }
    level_max_distance=tomb_render_max_distance(lo,hi);
}
static int resolution_setting;
static double render_pixel_scale=1;
static TombRenderSurface resolution_surface;
static TombRenderSurface aspect_surface;
static int force43;
static int water_style,water_surface_pass;
static uint8_t *water_faces[1024];
static int water_faces_flip;
void tr2_home_water_style(int style){water_style=style==1;}
void tr2_home_force43(int enabled){force43=!!enabled;}
void tr2_home_resolution(int setting){resolution_setting=setting>=0 && setting<=3?setting:0;}
#define PI 3.14159265358979323846
static Tr2World world;
static Tr2Actor actor;
static int campaign_home_exit,session_level,quick_draw;
int tr2_home_campaign_exit(void){return campaign_home_exit;}
static int cutscene,cut1_laptop,scene_yaw,cut_pistol_hands,cut_lara_swapped;
static unsigned *cut_meshes;
void tr2_level_cut_angle(int angle){scene_yaw=angle;}
static Tr2CampaignTotals prior_totals;
static Tr2Inventory carry_inventory;
static Tr2Combat carry_combat;
static Tr2Customisation carry_custom;
static int autosave_due,progression_counts[33];
static void autosave_inventory(int initial){
    if(!(level->custom.options&TR2_AUTOSAVE))return;
    for(int id=174;id<=206;id++){
        if(!((id>=174 && id<=177) || (id>=193 && id<=196) || id>=205))continue;
        int count=world.inventory->count[id];
        if(!initial && count!=progression_counts[id-174])autosave_due=1;
        progression_counts[id-174]=count;
    }
}
int tr2_level_take_autosave(void){
    int due=autosave_due;
    autosave_due=0;
    return due && actor.control.health>0 && !world.finished && !cutscene;
}
const Tr2World *tr2_level_world(void){return &world;}
void tr2_level_skip(void){
    for(unsigned i=0;i<world.count;i++){
        if(world.objects[i].id!=262)continue;
        tr2_start_shower(level,&world,&world.objects[i],&actor);
        return;
    }
    world.finished=1;
}
int tr2_level_finished(void){return world.finished;}
int tr2_level_keys_requested(void){int request=world.inventory_request;world.inventory_request=0;return request;}
void tr2_level_give(int id){tr2_give_weapon(&world,id);}
int tr2_level_use(int item,int request){
    int result=tr2_inventory_use(&world,&actor,item);
    if(result && request>=2 && (unsigned)(request-2)<world.count){
        tr2_detonator_use(level,&world,&world.objects[request-2],&actor,TR2_ACTION);
        world.inventory->selected=-1;
    }
    if(result==116)tr2_home_menu_sound(result);
    return result;
}
void tr2_quick_weapon(int item){
    if(!level || world.cinematic_active || actor.extra_animation || actor.control.health<=0)return;
    if(tr2_level_use(item,0))
        quick_draw=!actor.gun_status && world.combat->weapon==world.combat->requested;
}
void tr2_level_sunset(int enabled){level->sunset=enabled;}
void tr2_level_rewards(const int rewards[27]){memcpy(world.rewards,rewards,sizeof world.rewards);}
void tr2_level_remember(void){carry_inventory=*world.inventory;carry_combat=*world.combat;carry_custom=level->custom;
    if((carry_custom.options&TR2_CONSISTENT_WEAPONS) && carry_combat.weapon>=4 && carry_combat.weapon<=7 && (carry_combat.meshes&2))carry_combat.back_weapon=carry_combat.weapon;
}
void tr2_level_completed_stats(Tr2Inventory *inventory,Tr2Combat *combat){*inventory=carry_inventory;*combat=carry_combat;}
Tr2CampaignTotals tr2_campaign_totals(void){
    Tr2CampaignTotals t=prior_totals;int secrets=carry_inventory.secrets;
    t.ticks+=carry_inventory.ticks;t.distance+=carry_inventory.distance;t.kills+=carry_inventory.kills;
    t.shots+=carry_combat.shots;t.hits+=carry_combat.hits;t.medipacks+=carry_inventory.medipacks;
    t.secrets+=(secrets&1)+((secrets>>1)&1)+((secrets>>2)&1);return t;
}

static uint32_t previous_input;
int tr2_home_flare_count(void){return world.inventory?world.inventory->count[151]:0;}
int tr2_home_use_flare(void){return world.inventory?tr2_inventory_use(&world,&actor,151):0;}
static char score_path[MAX_PATH];
static Tr2AssaultScores saved_scores;
const Tr2AssaultScores *tr2_home_scores(void){return &world.assault_scores;}
unsigned tr2_home_stopwatch_ticks(void){return world.inventory && session_level>0?(unsigned)world.inventory->ticks:(unsigned)world.assault_ticks;}
void tr2_home_score_path(const char *path){
 snprintf(score_path,sizeof score_path,"%s",path);memset(&saved_scores,0,sizeof saved_scores);
 if(!score_path[0])return;
 saved_scores.total=GetPrivateProfileIntA("Assault","Finishes",0,score_path);
 for(int i=0;i<10;i++){char key[32];snprintf(key,sizeof key,"Time%d",i);saved_scores.times[i]=GetPrivateProfileIntA("Assault",key,0,score_path);
  snprintf(key,sizeof key,"Finish%d",i);saved_scores.finishes[i]=GetPrivateProfileIntA("Assault",key,0,score_path);}
}
static void scores_save(void){
 saved_scores=world.assault_scores;if(!score_path[0])return;
 char value[32],key[32];snprintf(value,sizeof value,"%u",saved_scores.total);WritePrivateProfileStringA("Assault","Finishes",value,score_path);
 for(int i=0;i<10;i++){snprintf(key,sizeof key,"Time%d",i);snprintf(value,sizeof value,"%u",saved_scores.times[i]);WritePrivateProfileStringA("Assault",key,value,score_path);
  snprintf(key,sizeof key,"Finish%d",i);snprintf(value,sizeof value,"%u",saved_scores.finishes[i]);WritePrivateProfileStringA("Assault",key,value,score_path);}
}
int tr2_home_score_test(void){
 /* Use an isolated, newly created file; automated runs never alter player scores. */
 char directory[MAX_PATH],temporary[MAX_PATH],previous[MAX_PATH];
 if(!GetFullPathNameA("build",MAX_PATH,directory,NULL) || !GetTempFileNameA(directory,"tr2",0,temporary))return 0;
 snprintf(previous,sizeof previous,"%s",score_path);
 Tr2AssaultScores saved=saved_scores;snprintf(score_path,sizeof score_path,"%s",temporary);
 scores_save();tr2_home_score_path(temporary);
 int ok=!memcmp(&saved_scores,&world.assault_scores,sizeof saved_scores);
 DeleteFileA(temporary);snprintf(score_path,sizeof score_path,"%s",previous);saved_scores=saved;
 fprintf(stderr,"TR2 stopwatch score persistence round trip: %s\n",ok?"PASS":"FAIL");return ok;
}
static GLuint textures[256],mesh_lists;
static Tr2Level *weapon_assets;
static GLuint weapon_textures[256];
static double *mesh_radius;
Tr2Level *tr2_ring_assets(int model,unsigned *pages){
    Tr2Level *source=level;
    if(model>=157 && model<=170 && weapon_assets && !tr2_model(level,model))source=weapon_assets;
    if(pages)memcpy(pages,source==level?textures:weapon_textures,sizeof textures);
    return source;
}
Tr2Level *tr2_home_menu_assets(unsigned pages[256]){memcpy(pages,textures,sizeof textures);return level;}
static int width,height,menu_mesh_shade,object_shade;
static int health_previous,health_timer,death_ticks,death_return;
int tr2_home_dead(void){return death_ticks>0;}
int tr2_home_return_title(void){return death_return;}
void tr2_home_death_input(void){if(tr2_home_death_ready(death_ticks,1))death_return=1;}
static double eye[3],heading,pitch,accumulator;
static int world_fog,outside_visible;
static int sky_mesh;
static int object_fog_depth,object_water;
static Tr2Light object_base_light;
static double room_rectangles[1024][4];
static double fog_forward[3];
static int fog_depth(int x,int y,int z){
    if(!world_fog || draw_distance_setting==DRAW_NO_FOG)return 0;
    double dx=x-eye[0],dy=y-eye[1],dz=z-eye[2];
    if(draw_distance_setting==DRAW_SMOOTH_FOG)return tomb_camera_distance(dx,dy,dz);
    return (int)floor(dx*fog_forward[0]+dy*fog_forward[1]+dz*fog_forward[2]);
}
static int fog_shade(int shade,int depth){
    if(draw_distance_setting>=DRAW_MAX_FOG)return tomb_render_fog_shade(shade,depth);
    if(!draw_distance_setting && depth>12288)shade+=depth-12288;
    return shade<0?0:shade>8191?8191:shade;
}
static void draw_mesh(uint32_t index);
static int sprite_index(int id);

static Tr2Water water;
static Tr2Camera camera;
static Tr2Look look;
static unsigned simulation_ticks;
static struct {int ticks,sprite;} pickup_icons[12];
static void update_pickup_icons(const Tr2Actor *source){
    for(int e=0;e<source->event_count;e++){
        if(source->event_kind[e]!=8)continue;
        for(int i=0;i<12;i++)if(!pickup_icons[i].ticks){
            pickup_icons[i].sprite=sprite_index(source->event_value[e]);
            pickup_icons[i].ticks=75;
            break;
        }
    }
    for(int i=0;i<12;i++)if(pickup_icons[i].ticks)pickup_icons[i].ticks--;
}
static int texture_phase,light_phase,camera_water;
static const Tr2Creature *model_creature;
static int water_offsets[32];
static Tr2Hair hair_state;
static int hair_wind;
static uint32_t hair_random;
typedef struct Tr2RenderEntry {
    TombRenderTrack pose,root;
    int bounds[2][6];
} Tr2RenderEntry;
static Tr2RenderEntry *render_entries,*render_entry;
static size_t render_count;
static uint64_t render_tick;
static int render_recording,render_camera_room;
static double render_alpha=1,render_view[16];
static Tr2Camera render_camera_previous,render_camera_current;
static int render_camera_key,render_camera_target,render_flip,render_cine_frame;
static int render_shot_change;
static int previous_fov,current_fov,previous_roll,current_roll;
static double view_fov=80,view_roll;
static Tr2Level render_light_level;
static Tr2World render_light_world;
static Tr2Hazards render_light_hazards;
static Tr2DynamicLight render_lights[2][10];
static int render_light_count[2];
static void render_capture(void);
static void update_eye(void);
static size_t render_darts(void){return 1+world.count;}
static size_t render_fragments(void){return render_darts()+TR2_MAX_DARTS;}
static size_t render_effects(void){return render_fragments()+TR2_MAX_FRAGMENTS;}
static size_t render_hair(void){return render_effects()+TR2_MAX_EFFECTS;}
static void render_matrix(TombRenderTrack *track,int node,int parent,int identity){
    float matrix[16];glGetFloatv(GL_MODELVIEW_MATRIX,matrix);
    /* TR2's original keyframe matrix blend can contain scale. Snapshot a
       rigid basis so the shared hierarchical interpolation preserves limbs. */
    double length=0;for(int i=0;i<3;i++)length+=matrix[i]*matrix[i];length=sqrt(length);
    if(length>0)for(int i=0;i<3;i++)matrix[i]/=(float)length;
    double dot=0;for(int i=0;i<3;i++)dot+=matrix[i]*matrix[4+i];
    length=0;for(int i=0;i<3;i++){matrix[4+i]-=(float)(dot*matrix[i]);length+=matrix[4+i]*matrix[4+i];}length=sqrt(length);
    if(length>0)for(int i=0;i<3;i++)matrix[4+i]/=(float)length;
    for(int i=0;i<3;i++)matrix[8+i]=matrix[(i+1)%3]*matrix[4+(i+2)%3]-matrix[(i+2)%3]*matrix[4+(i+1)%3];
    tomb_render_record(track,render_tick,node,parent,identity,matrix);
}
static int render_sample(TombRenderTrack *track,int node,int identity){
    float matrix[16];if(!tomb_render_sample(track,render_tick,node,identity,render_alpha,matrix))return 0;
    glLoadMatrixd(render_view);glMultMatrixf(matrix);return 1;
}
static void braid_update(void){
    const uint8_t *model=tr2_model(level,2);if(!model)return;
    Tr2Sphere body[5];int anchor[3],bones[6][3];
    if(!tr2_hair_samples(level,&actor,&look,anchor,body))return;
    unsigned tree=tr2_u32(model+8);
    for(int i=0;i<6;i++)for(int j=0;j<3;j++)bones[i][j]=tr2_i32(level->trees.data+(tree+i*4+1+j)*4);
    memcpy(hair_state.point[0],anchor,sizeof anchor);
    if(!hair_state.ready){
        memset(hair_state.velocity,0,sizeof hair_state.velocity);
        for(int i=0;i<6;i++){
            hair_state.pitch[i]=-16384;hair_state.yaw[i]=0;int offset[3];tr2_rotate_offset(0,-16384,bones[i],offset);
            for(int j=0;j<3;j++)hair_state.point[i+1][j]=hair_state.point[i][j]+offset[j];
        }
        hair_state.ready=1;hair_wind=0;return;
    }
    int bounds[6];tr2_best_bounds(level,&actor,bounds);
    int water_height=tr2_water_height(level,actor.x+(bounds[0]+bounds[1])/2,actor.z+(bounds[4]+bounds[5])/2,actor.room,NULL);
    Tr2Height h;Tr2Sector ref;int floor=actor.y;
    if(tr2_query(level,anchor[0],anchor[1],anchor[2],actor.room,&h,&ref)){
        floor=h.floor<anchor[1]?actor.y:h.floor;
        if(level->rooms[ref.room].flags&32){
            hair_random=hair_random*0x41c64e6d+12345;int change=(hair_random>>16)&7;
            if(change){hair_wind+=change-4;if(hair_wind<0)hair_wind=0;else if(hair_wind>=8)hair_wind--;}
        }else hair_wind=0;
    }
    tr2_hair_step(&hair_state,(const int (*)[3])bones,body,actor.control.water_status,water_height,floor,hair_wind);
}
static void braid(void){
    const uint8_t *model=tr2_model(level,2);if(!model || !hair_state.ready)return;
    for(int i=0;i<6;i++){
        glPushMatrix();glTranslated(hair_state.point[i][0],hair_state.point[i][1],hair_state.point[i][2]);
        glRotated(hair_state.yaw[i]*(360.0/65536),0,1,0);glRotated(hair_state.pitch[i]*(360.0/65536),1,0,0);
        if(render_entries)render_sample(&render_entries[render_hair()].pose,i,tr2_u16(model+6)+i);
        draw_mesh(tr2_u16(model+6)+i);glPopMatrix();
    }
}
static int texture_index(const Tr2Level *assets,int index){
    index&=0x7fff;const Tr2Span *span=&assets->animated_textures;if(!span->count)return index;
    unsigned at=1;int groups=tr2_u16(span->data);
    for(int g=0;g<groups && at<span->count;g++){
        unsigned count=tr2_u16(span->data+at++*2)+1;if(count>span->count-at)break;
        for(unsigned j=0;j<count;j++)if(tr2_u16(span->data+(at+j)*2)==index)return tr2_u16(span->data+(at+(j+(unsigned)texture_phase)%count)*2);
        at+=count;
    }return index;
}
static int placement_shade(const Tr2Room *room,const uint8_t *shades){
    int first=tr2_i16(shades);
    if(!room->light_mode)return first;
    return first+(tr2_i16(shades+2)-first)*tr2_room_light_phase(level,room,light_phase)/31;
}
static void object_light(int room,int x,int y,int z,int shade){
    for(int k=0;k<8;k++)glDisable(GL_LIGHT0+k);
    const Tr2Room *r=room>=0 && (uint32_t)room<level->room_count?level->rooms+room:NULL;
    Tr2Light light={0,0,{0,0,0}};
    int depth=fog_depth(x,y,z);
    if(draw_distance_setting==DRAW_SMOOTH_FOG && world_fog){
        depth=0; /* Fog follows each transformed mesh vertex. */
        object_fog_depth=0;
    }else if(draw_distance_setting==DRAW_MAX_FOG && world_fog){
        object_fog_depth=room>=0?depth:0;
        if(room<0){GLdouble matrix[16];glGetDoublev(GL_MODELVIEW_MATRIX,matrix);object_fog_depth=(int)floor(-matrix[14]);}
        depth=0; /* Apply capped fog after the vertex's own lighting. */
    }else object_fog_depth=0;
    if(world_fog && !draw_distance_setting && room<0){GLdouble matrix[16];glGetDoublev(GL_MODELVIEW_MATRIX,matrix);depth=(int)floor(-matrix[14]);}
    if(shade>=0)light.shade=fog_shade(tr2_dynamic_shade(&render_light_world,x,y,z,shade),depth);else if(r)tr2_room_light(&render_light_level,room,x,y,z,light_phase,depth,&light);
    object_shade=shade>=0?tr2_static_light(&render_light_world,x,y,z,shade,shade,0,0,depth):light.shade;
    object_base_light=light;
    float base=(8191-light.shade)/4096.f;
    int wet=camera_water || (r && (r->flags&1));object_water=wet;
    GLfloat ambient[4]={base*(wet?.5f:1.f),base*(wet?.875f:1.f),base,1};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT,ambient);glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_FALSE);
    if(light.divider){
        /* Two opposed signed diffuse terms preserve the original signed dot
           product; GL's single-light Lambert term would clip its dark side. */
        float amount=(float)(0x4000000/light.divider)/4096.f;
        for(int k=0;k<2;k++){
            float sign=k?1.f:-1.f;
            GLfloat diffuse[4]={sign*amount*(wet?.5f:1.f),sign*amount*(wet?.875f:1.f),sign*amount,1};
            GLfloat direction[4]={light.direction[0]*-sign,light.direction[1]*-sign,light.direction[2]*-sign,0};
            glEnable(GL_LIGHT0+k);glLightfv(GL_LIGHT0+k,GL_DIFFUSE,diffuse);glLightfv(GL_LIGHT0+k,GL_POSITION,direction);
        }
    }
}
static void set_texture(const Tr2Level *assets,const GLuint *pages,uint16_t index){
    const uint8_t *p=assets->textures.data+texture_index(assets,index)*20;
    glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,pages[tr2_u16(p+2)&0x7fff]);
    if(tr2_u16(p)){glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,0.5f);}else glDisable(GL_ALPHA_TEST);
}
static void texcoord(const Tr2Level *assets,uint16_t index,int vertex){
    const uint8_t *p=assets->textures.data+texture_index(assets,index)*20+4+vertex*4;
    glTexCoord2f(tr2_texture_coord(p),tr2_texture_coord(p+2));
}
/* Masked horizontal faces at the current wet/dry boundary are the water overlay. */
static int water_surface(const Tr2Room *room,const uint8_t *face,int n){
    unsigned texture=tr2_u16(face+n*2)&0x7fff;
    if(!tr2_u16(level->textures.data+texture*20))return 0;
    int x=0,z=0,y=tr2_i16(room->vertices.data+tr2_u16(face)*12+2);
    for(int j=0;j<n;j++){
        const uint8_t *v=room->vertices.data+tr2_u16(face+j*2)*12;
        if(tr2_i16(v+2)!=y)return 0;
        x+=tr2_i16(v);z+=tr2_i16(v+4);
    }
    return tr2_water_height(level,room->x+x/n,room->z+z/n,(int)(room-level->rooms),NULL)==y;
}
static void classify_water_surfaces(void){
    for(uint32_t i=0;i<level->room_count;i++){
        const Tr2Room *r=level->rooms+i;
        for(uint32_t j=0;j<r->quads.count;j++)water_faces[i][j]=(uint8_t)water_surface(r,r->quads.data+j*10,4);
        for(uint32_t j=0;j<r->triangles.count;j++)water_faces[i][r->quads.count+j]=(uint8_t)water_surface(r,r->triangles.data+j*8,3);
    }
    water_faces_flip=world.flip_status;
}
static int water_surfaces_init(void){
    size_t capacity[1024];
    for(uint32_t i=0;i<level->room_count;i++)capacity[i]=level->rooms[i].quads.count+level->rooms[i].triangles.count;
    for(uint32_t i=0;i<level->room_count;i++){
        int other=level->rooms[i].alternate;
        if(other<0 || (unsigned)other>=level->room_count)continue;
        size_t larger=capacity[i]>capacity[other]?capacity[i]:capacity[other];
        capacity[i]=capacity[other]=larger;
    }
    for(uint32_t i=0;i<level->room_count;i++)if(capacity[i]){
        water_faces[i]=malloc(capacity[i]);if(!water_faces[i])return 0;
    }
    classify_water_surfaces();
    return 1;
}
static void room_faces(const Tr2Room *room,Tr2Span faces,int n){
    int phase=tr2_room_light_phase(level,room,light_phase);
    int wobble=!water_style && (((room->flags&1)!=0)!=camera_water);
    GLdouble view[16],projection[16];
    if(wobble){glGetDoublev(GL_MODELVIEW_MATRIX,view);glGetDoublev(GL_PROJECTION_MATRIX,projection);}
    /* Room polygons are clockwise from their visible side. Drawing their backs
       exposes adjoining rooms' coplanar surfaces and causes depth fighting. */
    glPushAttrib(GL_ENABLE_BIT|GL_POLYGON_BIT);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);glFrontFace(GL_CW);
    for(uint32_t i=0;i<faces.count;i++){
        const uint8_t *p=faces.data+i*(n+1)*2;
        if(water_style){
            const uint8_t *mask=water_faces[room-level->rooms];
            int surface=mask && mask[(n==4?0:room->quads.count)+i];
            if(surface!=water_surface_pass)continue;
        }
        /* Root 419603..419636 passes the outside flag to 401ae0.
           Far room vertices keep their projected silhouette (4021bf),
           rather than being cut against a world-space far plane. Indoor
           polygons whose vertices all carry the far flag are rejected. */
        if(!draw_distance_setting && (!outside_visible || (room->flags&64))){
            int far_count=0;
            for(int j=0;j<n;j++){
                const uint8_t *v=room->vertices.data+tr2_u16(p+j*2)*12;
                if(fog_depth(room->x+tr2_i16(v),tr2_i16(v+2),room->z+tr2_i16(v+4))>=20480)far_count++;
            }
            if(far_count==n)continue;
        }
        uint16_t texture=tr2_u16(p+n*2);set_texture(level,textures,texture);
        if(water_surface_pass)glAlphaFunc(GL_GREATER,0.25f);
        glBegin(n==4?GL_QUADS:GL_TRIANGLES);
        for(int j=0;j<n;j++){
            const uint8_t *v=room->vertices.data+tr2_u16(p+j*2)*12;
            int x=room->x+tr2_i16(v),y=tr2_i16(v+2),z=room->z+tr2_i16(v+4);
            int shade=tr2_i16(v+6);
            /* Root 4516ee uses the vertex effect index, not lighting2.
               Table built at 44d8be: phase * (index-16) * 512 / 31. */
            if(room->light_mode){int effect=tr2_u16(v+8)&31;if(effect<31)shade+=phase*(effect-16)*512/31;}
            if(room->flags&1){unsigned index=room->vertices.count-tr2_u16(p+j*2);int phase=(water_offsets[index&31]+(int)simulation_ticks)&31;shade+=tr2_asr(tr2_sin(phase*2048)*768,14);}
            shade=tr2_dynamic_shade(&render_light_world,x,y,z,shade);
            int depth=fog_depth(x,y,z);shade=!draw_distance_setting && depth>=20480?8191:fog_shade(shade,depth);
            uint32_t rgb=tr2_shade_rgb(shade,camera_water || (room->flags&1));
            glColor4ub((GLubyte)(rgb>>16),(GLubyte)(rgb>>8),(GLubyte)rgb,water_surface_pass?128:255);
            texcoord(level,texture,j);
            /* Keep real depth. Collapsing far vertices onto one eye-ray
               distance causes z fighting and distorts crossing triangles.
               Original far shading/rejection above still determines visibility. */
            if(wobble && !(tr2_u16(v+8)&0x8000)){
                double vx=view[0]*x+view[4]*y+view[8]*z+view[12];
                double vy=view[1]*x+view[5]*y+view[9]*z+view[13];
                double vz=view[2]*x+view[6]*y+view[10]*z+view[14];
                if(vz<=-16){
                    float sx=(float)((1-vx*projection[0]/vz)*width*.5),sy=(float)((1+vy*projection[5]/vz)*height*.5),wx=sx,wy=sy;
                    tr2_water_wobble(&wx,&wy,(int)simulation_ticks,(room->flags&1)!=0,camera_water,tr2_u16(v+8));
                    wx=sx+(wx-sx)*(float)render_pixel_scale;
                    double dx=(wx-sx)*(-vz)*2/(width*projection[0]),dy=-(wy-sy)*(-vz)*2/(height*projection[5]);
                    glVertex3d(x+view[0]*dx+view[1]*dy,y+view[4]*dx+view[5]*dy,z+view[8]*dx+view[9]*dy);
                }else glVertex3i(x,y,z);
            }else glVertex3i(x,y,z);
        }glEnd();
    }glPopAttrib();
}
static int object_extended_fog(void){return world_fog && draw_distance_setting==2 && object_fog_depth>12288;}
static void mesh_faces(const Tr2Level *assets,const GLuint *pages,const uint8_t **cursor,const uint8_t *vertices,int count_vertices,int n,int coloured,const uint8_t *lights,const uint8_t *normals,int depth,const double *smooth_matrix){
    int capped=smooth_matrix || object_extended_fog();
    GLdouble matrix[16];GLfloat direction[4]={0};
    if(capped && normals){if(smooth_matrix)memcpy(matrix,smooth_matrix,sizeof matrix);else glGetDoublev(GL_MODELVIEW_MATRIX,matrix);glGetLightfv(GL_LIGHT0,GL_POSITION,direction);}
    int count=tr2_u16(*cursor);*cursor+=2;
    for(int i=0;i<count;i++){
        const uint8_t *p=*cursor;*cursor+=(n+1)*2;uint16_t texture=tr2_u16(p+n*2);
        if(coloured){glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);}else set_texture(assets,pages,texture);
        glBegin(n==4?GL_QUADS:GL_TRIANGLES);
        for(int j=0;j<n;j++){
            int index=tr2_u16(p+j*2);if(index>=count_vertices)continue;
            const uint8_t *v=vertices+index*6;
            uint32_t rgb=sky_mesh?tr2_shade_rgb(4095,0):lights?tr2_shade_rgb(tr2_prelit_shade(tr2_i16(lights+index*2),world_fog?object_shade:fog_shade(menu_mesh_shade,depth)),world_fog && camera_water):0xffffff;
            if(capped && !sky_mesh){
                int shade;
                if(lights)shade=tr2_prelit_shade(tr2_i16(lights+index*2),object_shade);
                else{
                    double dot=0,norm[3]={0},length=0,dl=0;
                    if(normals && object_base_light.divider){
                        for(int k=0;k<3;k++){for(int j=0;j<3;j++)norm[k]+=matrix[j*4+k]*tr2_i16(normals+index*6+j*2);length+=norm[k]*norm[k];dl+=direction[k]*direction[k];dot+=norm[k]*direction[k];}
                        if(length>0 && dl>0)dot/=sqrt(length*dl);else dot=0;
                    }
                    shade=object_base_light.shade+(object_base_light.divider?(int)lround((0x4000000/object_base_light.divider)*dot):0);
                }
                int vertex_depth=smooth_matrix?tomb_mesh_camera_distance(smooth_matrix,tr2_i16(v),tr2_i16(v+2),tr2_i16(v+4)):object_fog_depth;
                rgb=tr2_shade_rgb(fog_shade(shade,vertex_depth),lights?camera_water:object_water);
            }
            float red=((rgb>>16)&255)/255.f,green=((rgb>>8)&255)/255.f,blue=(rgb&255)/255.f;
            if(normals)glNormal3f(tr2_i16(normals+index*6)/16384.f,tr2_i16(normals+index*6+2)/16384.f,tr2_i16(normals+index*6+4)/16384.f);
            if(coloured){const uint8_t *c=assets->palette16+(texture>>8)*4;glColor3f(c[0]/255.f*red,c[1]/255.f*green,c[2]/255.f*blue);}
            else{glColor3f(red,green,blue);texcoord(assets,texture,j);}
            glVertex3s(tr2_i16(v),tr2_i16(v+2),tr2_i16(v+4));
        }glEnd();
    }
}
static void mesh_geometry(const Tr2Level *assets,const GLuint *pages,uint32_t index,int depth,const double *smooth_matrix){
    const uint8_t *p=assets->mesh_data.data+tr2_u32(assets->mesh_offsets.data+index*4)+10;
    int vertices_count=tr2_u16(p);p+=2;const uint8_t *vertices=p;p+=vertices_count*6;
    int normals=tr2_i16(p);p+=2;const uint8_t *lights=normals<0?p:NULL,*normal_data=normals>0?p:NULL;p+=normals<0?-normals*2:normals*6;
    glPushAttrib(GL_ENABLE_BIT);if(normal_data && !sky_mesh && !smooth_matrix && !object_extended_fog()){glEnable(GL_LIGHTING);glEnable(GL_NORMALIZE);glEnable(GL_COLOR_MATERIAL);glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);}else glDisable(GL_LIGHTING);
    mesh_faces(assets,pages,&p,vertices,vertices_count,4,0,lights,normal_data,depth,smooth_matrix);mesh_faces(assets,pages,&p,vertices,vertices_count,3,0,lights,normal_data,depth,smooth_matrix);
    mesh_faces(assets,pages,&p,vertices,vertices_count,4,1,lights,normal_data,depth,smooth_matrix);mesh_faces(assets,pages,&p,vertices,vertices_count,3,1,lights,normal_data,depth,smooth_matrix);
    glPopAttrib();
}
static void compile_mesh(uint32_t index){
    glNewList(mesh_lists+index,GL_COMPILE);mesh_geometry(level,textures,index,0,NULL);glEndList();
}
static void draw_mesh_unculled(uint32_t index){
    if(world_fog && draw_distance_setting==DRAW_SMOOTH_FOG && !menu_mesh_shade){
        double matrix[16];glGetDoublev(GL_MODELVIEW_MATRIX,matrix);
        /* Rigid mesh transforms preserve the radius. Keep nearby meshes on
           their usual path, including the normal-lit mesh display lists. */
        double safe=12288-mesh_radius[index];
        if(safe<=0 || matrix[12]*matrix[12]+matrix[13]*matrix[13]+matrix[14]*matrix[14]>safe*safe){
            mesh_geometry(level,textures,index,0,matrix);return;
        }
    }
    if(menu_mesh_shade || object_extended_fog()){mesh_geometry(level,textures,index,0,NULL);return;}
    /* Prelit meshes bypass GL lighting. Their placement/room light, flares
       and fog must be added before packing vertex colours on every draw. */
    if(world_fog){
        const uint8_t *p=level->mesh_data.data+tr2_u32(level->mesh_offsets.data+index*4)+10;
        if(tr2_i16(p+2+tr2_u16(p)*6)<0){
            mesh_geometry(level,textures,index,0,NULL);return;
        }
    }
    glCallList(mesh_lists+index);
}
static void draw_mesh(uint32_t index){
    /* TR2's object polygon paths reject backfaces (408ea0/405840).
       Scope this to world meshes; sprites, shadows and menu state are separate. */
    glPushAttrib(GL_ENABLE_BIT|GL_POLYGON_BIT);
    if(world_fog){glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glFrontFace(GL_CW);}
    if(index<level->mesh_offsets.count)draw_mesh_unculled(index);
    else if(weapon_assets)mesh_geometry(weapon_assets,weapon_textures,index-level->mesh_offsets.count,0,NULL);
    glPopAttrib();
}
static void rotate_frame(const uint8_t **cursor,const uint8_t *end){
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
    glRotatef(y*(360.f/1024),0,1,0);glRotatef(x*(360.f/1024),1,0,0);glRotatef(z*(360.f/1024),0,0,1);
}
/* Donor indices follow local indices so interpolation tracks each mesh identity. */
static const Tr2Level *weapon_source(int model_id){
    return tr2_model(level,model_id)?level:weapon_assets;
}
static unsigned weapon_mesh(int model_id,unsigned joint,unsigned fallback){
    const Tr2Level *source=weapon_source(model_id);
    const uint8_t *m=source?tr2_model(source,model_id):NULL;
    if(!m || joint>=tr2_u16(m+4))return fallback;
    return tr2_u16(m+6)+joint+(source==level?0:level->mesh_offsets.count);
}
static unsigned model_mesh(uint32_t id,unsigned start,unsigned i,int track){
    unsigned mesh=start+i;
    if((id==13 || id==51) && i==1 && track){const uint8_t *t=tr2_model(level,91);if(t)mesh=tr2_u16(t+6)+(track==1?1:7);}
    if(id==0 && actor.extra_animation && !cutscene){
        const int s=actor.control.current,frame=actor.control.frame-tr2_i16(level->animations.data+actor.control.animation*32+16);
        int source=-1;
        if(s==7 && i==10)source=frame<180?12:0;
        if(s==9 && (i==0 || i==10))source=frame<401?12:0;
        if(s==10){if((level->custom.options&TR2_CONSISTENT_WEAPONS) && (i==1 || i==4))return mesh;if(i==0)source=12;else if(i==10)source=frame>=316?3:0;else if(i==14)source=0;}
        if(s==10 && i==10 && frame>=316 && (level->custom.options&TR2_CONSISTENT_WEAPONS) && level->custom.last_drawn)
            return weapon_mesh(tr2_weapon_model(level->custom.last_drawn),i,mesh);
        if(source>=0){const uint8_t *m=tr2_model(level,source);if(m && i<tr2_u16(m+4))return tr2_u16(m+6)+i;}
    }
    if(cutscene && id==0){
        /* 43123f starts cinematic Lara with pistol-model thighs. Effects
           14/15 exchange each hand with the corresponding pistol hand. */
        if((!cut_lara_swapped && (i==1 || i==4)) || (i==10 && (cut_pistol_hands&2)) || (i==13 && (cut_pistol_hands&1))){
            if((i==1 && (cut_pistol_hands&1)) || (i==4 && (cut_pistol_hands&2)))return mesh;
            if(level->custom.options&TR2_CONSISTENT_WEAPONS){
                if(!level->custom.sidearm)return mesh;
                return weapon_mesh(tr2_weapon_model(level->custom.sidearm),i,mesh);
            }
            const uint8_t *gun=tr2_model(level,1);if(gun)return tr2_u16(gun+6)+i;
        }
        return cut_meshes?cut_meshes[mesh]:mesh;
    }
    if(cut_meshes)return cut_meshes[mesh];
    if(id==0 && world.combat && (i==1 || i==4)){
        int drawn=world.combat->weapon>=1 && world.combat->weapon<=3 && (world.combat->meshes&(i==1?1:2));
        return level->custom.sidearm && !drawn?weapon_mesh(tr2_weapon_model(level->custom.sidearm),i,mesh):mesh;
    }
    if(id==0 && world.combat && world.combat->weapon>=1 && world.combat->weapon<=3){
        if((i==13 && (world.combat->meshes&1)) || (i==10 && (world.combat->meshes&2)))
            mesh=weapon_mesh(tr2_weapon_model(world.combat->weapon),i,mesh);
    }
    if(id==0 && world.combat && world.combat->weapon>=4 && world.combat->weapon<=7 && i==10 && (world.combat->meshes&2))mesh=weapon_mesh(tr2_weapon_model(world.combat->weapon),i,mesh);
    if(id==0 && world.combat->weapon==8 && i==13 && (world.combat->meshes&1)){const uint8_t *flare=tr2_model(level,9);if(flare)mesh=tr2_u16(flare+6)+i;}
    return mesh;
}
static void model(uint32_t id,int animation,double ticks,double x,double y,double z,double yaw,uint64_t mesh_mask,int item_pitch,int item_roll,int track){
    const uint8_t *m=tr2_model(level,id);if(!m)return;
    unsigned meshes=tr2_u16(m+4),start=tr2_u16(m+6);if(!meshes || meshes>64)return;
    int ring_frame=animation==-2;
    const uint8_t *end=level->frames.data+level->frames.count*2;
    float sampled_pose[TOMB_RENDER_NODES][16];
    int sampled=!render_recording && !ring_frame && render_entry;
    if(sampled){
        uint64_t valid=tomb_render_sample_all(&render_entry->pose,render_tick,render_alpha,sampled_pose);
        for(unsigned i=0;i<meshes;i++){
            if(!(valid&(UINT64_C(1)<<i)) || render_entry->pose.nodes[i].identity!=(int)model_mesh(id,start,i,track)){sampled=0;break;}
        }
        /* Preserve the legacy attachment fallback if its expected torso mesh
           does not match the snapshot (normally Lara's torso is unchanged). */
        if(id==0 && world.combat->back_weapon && render_entry->pose.nodes[7].identity!=(int)start+7)sampled=0;
    }
    double poses[2][64][16],fraction=0;
    int parents[64];
    /* Snapshot capture and unsnapshotted/menu models retain TR2's original
       frame decoding. Normal world drawing already has these poses. */
    if(!sampled){
        if(animation<0)animation=tr2_u16(m+16);
        if((uint32_t)animation>=level->animations.count)return;
        const uint8_t *a=level->animations.data+animation*32;
        unsigned duration=tr2_u16(a+18)-tr2_u16(a+16)+1;
        unsigned rate=a[4]?a[4]:1;double tick=fmod(ticks,duration);unsigned index=(unsigned)(tick/rate);
        unsigned offset=tr2_u32(a)+index*a[5]*2;
        if(ring_frame)offset=tr2_u32(m+12)+(unsigned)ticks*a[5]*2;
        if(offset+18>level->frames.count*2)return;
        fraction=ring_frame?0:(tick-index*rate)/rate;
        const uint8_t *hit=id==0?tr2_hit_frame(level,&world):NULL;
        if(hit){offset=(unsigned)(hit-level->frames.data);fraction=0;}
        if(offset+a[5]*2+18>level->frames.count*2)fraction=0;
        int parent_stack[64],parent_top=0;parents[0]=-1;
        for(unsigned i=1;i<meshes;i++){unsigned flags=tr2_u32(level->trees.data+(tr2_u32(m+8)+(i-1)*4)*4);int parent=(int)i-1;
            if((flags&1) && parent_top)parent=parent_stack[--parent_top];
            if(flags&2)parent_stack[parent_top++]=parent;parents[i]=parent;}
        for(int pass=0;pass<(fraction>0?2:1);pass++){
            const uint8_t *frame=level->frames.data+offset+pass*a[5]*2;
            glPushMatrix();glLoadIdentity();glTranslatef(tr2_i16(frame+12),tr2_i16(frame+14),tr2_i16(frame+16));frame+=18;
            int stack=0;rotate_frame(&frame,end);glGetDoublev(GL_MODELVIEW_MATRIX,poses[pass][0]);uint32_t tree=tr2_u32(m+8);
            for(unsigned i=1;i<meshes;i++){
                const uint8_t *bone=level->trees.data+(tree+(i-1)*4)*4;uint32_t flags=tr2_u32(bone);
                if((flags&1) && stack){glPopMatrix();stack--;}
                if(flags&2){glPushMatrix();stack++;}
                glTranslatef((float)tr2_i32(bone+4),(float)tr2_i32(bone+8),(float)tr2_i32(bone+12));
                /* The six CUT1 picture planes sit 20 units ahead of the lid.
                   Leave one unit of clearance so its backing cannot z-fight. */
                if(cut1_laptop && id==128)glTranslatef(-19,0,0);
                if(frame+4>end)break;const uint8_t *rotation_end=end,*rotation=frame;unsigned packed_size=(tr2_u16(frame)>>14)?2:4;frame+=packed_size;
                Tr2Combat *g=world.combat;
                if(id==0 && !actor.extra_animation && g && g->weapon>=1 && g->weapon<=3 && actor.gun_status>=2 && actor.gun_status<=4 && i>=8 && i<=13){
                    int arm=i>=11?0:1;const Tr2Level *assets=tr2_weapon_assets(level,1);const uint8_t *pistol=tr2_model(assets,1);
                    if(pistol){int f=g->arms[arm].frame;unsigned an=tr2_u16(pistol+16)+(f<5?0:f<13?1:f<24?2:3);
                        if(an<assets->animations.count){const uint8_t *aa=assets->animations.data+an*32;unsigned off=tr2_u32(aa)+(unsigned)(f-tr2_i16(aa+16))*aa[5]*2;
                            if(off+18<assets->frames.count*2){rotation_end=assets->frames.data+assets->frames.count*2;rotation=assets->frames.data+off+18;for(unsigned j=0;j<i && rotation+4<=rotation_end;j++)rotation+=(tr2_u16(rotation)>>14)?2:4;}}
                    }
                    /* 41a58f / 41b25e restore Lara's basis at each shoulder
                       before applying the independent arm rotation. */
                    if(i==8 || i==11){
                        double shoulder[16];glGetDoublev(GL_MODELVIEW_MATRIX,shoulder);
                        glLoadIdentity();glTranslated(shoulder[12],shoulder[13],shoulder[14]);
                        const Tr2ArmAim *aim=&g->arms[arm].aim;
                        glRotated(aim->yaw*(360.0/65536),0,1,0);
                        glRotated(aim->pitch*(360.0/65536),1,0,0);
                        glRotated(aim->roll*(360.0/65536),0,0,1);
                    }
                }
                /* DrawLara 41a174 and DrawLara_I 41adbc take the torso and head
                   from the M16 frame in standing aim, fire and unaim states. */
                int rifle_state=g?g->rifle.control.current:0;
                int m16_upper_body=g && g->weapon==5 && (i==7 || i==14) &&
                    (rifle_state==0 || rifle_state==2 || rifle_state==4);
                if(id==0 && !actor.extra_animation && g && ((g->rifle_active && ((i>=8 && i<=13) || m16_upper_body)) || (g->weapon==8 && g->flare_control && i>=11 && i<=13))){
                    const Tr2Level *assets=g->rifle_active?tr2_weapon_assets(level,tr2_weapon_model(g->weapon)):level;
                    const uint8_t *aa=assets->animations.data+g->rifle.control.animation*32;unsigned off=tr2_u32(aa)+(unsigned)(g->rifle.control.frame-tr2_i16(aa+16))*aa[5]*2;
                    if(off+18<assets->frames.count*2){rotation_end=assets->frames.data+assets->frames.count*2;rotation=assets->frames.data+off+18;for(unsigned j=0;j<i && rotation+4<=rotation_end;j++)rotation+=(tr2_u16(rotation)>>14)?2:4;}
                }
                rotate_frame(&rotation,rotation_end);
                if(ring_frame && id==121 && i>=2 && i<=4){
                    int seconds=world.inventory->ticks/30;
                    int angle=i==2?-(seconds%60)*1092:i==3?-(seconds%3600)*182/10:-(seconds/60)*182/10;
                    glRotatef(angle*(360.f/65536),0,0,1);
                }
                glRotatef(tr2_creature_joint_rotation((int)id,(int)i,model_creature)*(360.f/65536),0,(id==29 && i==15) || id==22?0:1,(id==29 && i==15) || id==22?1:0);
                if(id==0 && (i==7 || i==14)){Tr2Look pose;tr2_combat_look_pose(&actor,world.combat,&look,&pose);glRotatef((i==7?pose.torso_yaw:pose.yaw)*(360.f/65536),0,1,0);glRotatef((i==7?pose.torso_pitch:pose.pitch)*(360.f/65536),1,0,0);}
                glGetDoublev(GL_MODELVIEW_MATRIX,poses[pass][i]);
            }
            while(stack-- >0)glPopMatrix();glPopMatrix();
        }
    }
    glPushMatrix();glTranslated(x,y,z);glRotated(yaw,0,1,0);
    glRotated(item_pitch*(360.0/65536),1,0,0);glRotated(item_roll*(360.0/65536),0,0,1);
    for(unsigned i=0;i<meshes;i++){
        int stone=(id==41 || id==43) && !(mesh_mask&(UINT64_C(1)<<i));
        if(!render_recording && !stone && !(mesh_mask&(UINT64_C(1)<<i)))continue;
        unsigned mesh=model_mesh(id,start,i,track);
        if(stone){const uint8_t *statue=tr2_model(level,id+1);if(statue && i<tr2_u16(statue+4))mesh=tr2_u16(statue+6)+i;}
        int saved_shade=object_shade;glPushMatrix();
        if(sampled){glLoadMatrixd(render_view);glMultMatrixf(sampled_pose[i]);}
        else{
            double matrix[16];for(int j=0;j<16;j++)matrix[j]=poses[0][i][j]+(fraction>0?(poses[1][i][j]-poses[0][i][j])*fraction:0);
            glMultMatrixd(matrix);
        }
        if(render_recording){render_matrix(&render_entry->pose,(int)i,parents[i],(int)mesh);glPopMatrix();continue;}
        if(!sampled && !ring_frame && render_entry)render_sample(&render_entry->pose,(int)i,(int)mesh);
        draw_mesh(mesh);
        if(id==0 && i==13 && world.combat->weapon==8 && (world.combat->meshes&1) && world.combat->arms[0].flash){
            const uint8_t *flame=tr2_model(level,235);if(flame){glPushAttrib(GL_LIGHTING_BIT);object_light(-1,0,0,0,2048);glTranslatef(11,32,80);glRotatef(-16380*(360.f/65536),1,0,0);glRotatef((world.hazards->random_draw&65535)*(360.f/65536),0,0,1);draw_mesh(tr2_u16(flame+6));glPopAttrib();}
        }
        /* Root DrawGunFlash: pistol, automatic and Uzi hand flashes. */
        if(id==0 && i==10 && world.combat->weapon==5 && world.combat->arms[1].flash){
            const uint8_t *flash=tr2_model(level,241);
            if(flash){glPushAttrib(GL_LIGHTING_BIT);object_light(-1,0,0,0,2560);glTranslatef(0,400,99);glRotatef(-15470*(360.f/65536),1,0,0);glRotatef((8192+(world.hazards->random_draw&16384))*(360.f/65536),0,0,1);draw_mesh(tr2_u16(flash+6));glPopAttrib();}
        }
        if(id==0 && (i==10 || i==13) && world.combat->weapon>=1 && world.combat->weapon<=3 && world.combat->arms[i==13?0:1].flash){
            const uint8_t *flash=tr2_model(level,240);int weapon=world.combat->weapon;
            if(flash){glPushAttrib(GL_LIGHTING_BIT);object_light(-1,0,0,0,weapon==1?5120:weapon==2?4096:2560);glTranslatef(0,weapon==1?185:weapon==2?215:200,weapon==1?40:weapon==2?65:50);glRotatef(-16380*(360.f/65536),1,0,0);glRotatef((world.hazards->random_draw&65535)*(360.f/65536),0,0,1);draw_mesh(tr2_u16(flash+6));glPopAttrib();}
        }
        glPopMatrix();object_shade=saved_shade;
    }
    /* DrawLara back weapon, 0x41a2a8: attach mesh 14 to the torso with
       the weapon's final bone offset and initial packed rotation. */
    if(!render_recording && id==0 && world.combat->back_weapon){
        int model_id=tr2_weapon_model(world.combat->back_weapon);
        const Tr2Level *source=weapon_source(model_id);
        const uint8_t *gun=source?tr2_model(source,model_id):NULL;
        if(gun){uint32_t tree=tr2_u32(gun+8)+13*4,off=tr2_u32(gun+12);
            if(tree+4<=source->trees.count && off+18<source->frames.count*2){const uint8_t *b=source->trees.data+tree*4,*rotation=source->frames.data+off+18;
                const uint8_t *rotation_end=source->frames.data+source->frames.count*2;
                for(int j=0;j<14 && rotation+4<=rotation_end;j++)rotation+=(tr2_u16(rotation)>>14)?2:4;
                glPushMatrix();
                if(sampled){glLoadMatrixd(render_view);glMultMatrixf(sampled_pose[7]);}
                else{
                    double matrix[16];for(int j=0;j<16;j++)matrix[j]=poses[0][7][j]+(fraction>0?(poses[1][7][j]-poses[0][7][j])*fraction:0);
                    glMultMatrixd(matrix);if(render_entry)render_sample(&render_entry->pose,7,(int)start+7);
                }
                glTranslatef(tr2_i32(b+4),tr2_i32(b+8),tr2_i32(b+12));rotate_frame(&rotation,rotation_end);draw_mesh(weapon_mesh(model_id,14,0));glPopMatrix();
            }
        }
    }glPopMatrix();
}
/* Shadow sizes written by the PC object setup routine at 43a530. */
static int shadow_size(int id){
    if(id==0)return 160;
    if(id==214 || id==260)return 64;
    if((id>=15 && id<=21) || id==25 || (id>=28 && id<=34) ||
       (id>=36 && id<=39) || id==41 || id==43 || id==45 ||
       (id>=47 && id<=51) || id==53 || id==54)return 128;
    return 0;
}
#include "tr2_home_interpolation.h"
static void draw_room(uint32_t i){
    const Tr2Room *r=level->rooms+i;
    room_faces(r,r->quads,4);room_faces(r,r->triangles,3);
}
static int static_visible(unsigned room,const uint8_t *placement,const uint8_t *definition){
    /* Static definitions contain visual bounds at +6, collision bounds at
       +18 and flags at +30. TR2 tests flag 2 before doing any mesh work. */
    if(!(tr2_u16(definition+30)&2))return 0;
    int16_t bounds[6];for(int i=0;i<6;i++)bounds[i]=tr2_i16(definition+6+i*2);
    double angle=tr2_u16(placement+12)*(2*PI/65536),c=cos(angle),s=sin(angle);
    int32_t matrix[12];
    for(int row=0;row<3;row++){
        /* OpenGL's view has upward Y and backward Z; TR2 uses the opposite. */
        double sign=row? -1:1;
        matrix[row*4]=(int32_t)llround(sign*(render_view[row]*c-render_view[8+row]*s)*16384);
        matrix[row*4+1]=(int32_t)llround(sign*render_view[4+row]*16384);
        matrix[row*4+2]=(int32_t)llround(sign*(render_view[row]*s+render_view[8+row]*c)*16384);
        double translation=render_view[12+row];
        for(int j=0;j<3;j++)translation+=render_view[j*4+row]*tr2_i32(placement+j*4);
        /* Keep extreme transforms conservatively rather than overflow while
           adapting the interpolated camera to the original fixed-point test. */
        if(fabs(translation)>=131072)return 1;
        matrix[row*4+3]=(int32_t)llround(sign*translation*16384);
    }
    double fov=world.cinematic_active?view_fov:80;
    int perspective=(int)(height/(2*tan(fov*PI/360)*.75));
    const double *r=room_rectangles[room];
    int view_width=(int)lround(width/render_pixel_scale);
    int clip[4]={(int)floor((r[0]+1)*view_width*.5),(int)ceil((r[2]+1)*view_width*.5),
        (int)floor((1-r[3])*height*.5),(int)ceil((1-r[1])*height*.5)};
    return tr2_render_bounds(bounds,matrix,perspective,20*16384,(int64_t)(render_far_distance()*16384),view_width/2,height/2,view_width-1,height-1,clip)!=0;
}
static void room_statics(uint32_t i){
        const Tr2Room *r=level->rooms+i;
        for(uint32_t j=0;j<r->statics.count;j++){
            const uint8_t *p=r->statics.data+j*20;uint16_t id=tr2_u16(p+18);
            for(uint32_t k=0;k<level->statics.count;k++){
                const uint8_t *s=level->statics.data+k*32;if(tr2_u32(s)!=id)continue;
                if(!static_visible(i,p,s))break;
                uint16_t mesh=tr2_u16(s+4);if(mesh>=level->mesh_offsets.count)continue;
                int shade=placement_shade(r,p+14);
                object_light((int)i,tr2_i32(p),tr2_i32(p+4),tr2_i32(p+8),shade);
                glPushMatrix();glTranslatef((float)tr2_i32(p),(float)tr2_i32(p+4),(float)tr2_i32(p+8));
                glRotatef(tr2_u16(p+12)*(360.f/65536),0,1,0);draw_mesh(mesh);glPopMatrix();
            }
        }
}
static void upload_pages(const Tr2Level *assets,GLuint *pages){
    glGenTextures((GLsizei)assets->tile_count,pages);uint8_t pixels[256*256*4];
    for(uint32_t tile=0;tile<assets->tile_count;tile++){
        for(int i=0;i<65536;i++){
            uint16_t c=tr2_u16(assets->tiles16+tile*131072+i*2);
            pixels[i*4]=(uint8_t)(((c>>10)&31)*255/31);pixels[i*4+1]=(uint8_t)(((c>>5)&31)*255/31);
            pixels[i*4+2]=(uint8_t)((c&31)*255/31);pixels[i*4+3]=(c&0x8000)?255:0;
        }
        tr2_texture_filter(pages[tile],texture_filtering);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812f /* GL_CLAMP_TO_EDGE */);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,256,256,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    }
}
static void init_geometry(void){
    upload_pages(level,textures);
    mesh_lists=glGenLists((GLsizei)level->mesh_offsets.count);
    for(uint32_t i=0;i<level->mesh_offsets.count;i++){
        const uint8_t *mesh=level->mesh_data.data+tr2_u32(level->mesh_offsets.data+4*i);
        double radius2=0;
        for(int j=0;j<tr2_u16(mesh+10);j++){
            const uint8_t *v=mesh+12+j*6;
            double x=tr2_i16(v),y=tr2_i16(v+2),z=tr2_i16(v+4);
            radius2=fmax(radius2,x*x+y*y+z*z);
        }
        mesh_radius[i]=sqrt(radius2);compile_mesh(i);
    }
}
void tr2_home_texture_filtering(int enabled){
    texture_filtering=!!enabled;
    if(weapon_assets)for(unsigned i=0;i<weapon_assets->tile_count;i++)tr2_texture_filter(weapon_textures[i],texture_filtering);
    if(level)for(uint32_t tile=0;tile<level->tile_count;tile++)tr2_texture_filter(textures[tile],texture_filtering);
}
static void camera_matrix(void){
    double f[3]={sin(heading)*cos(pitch),sin(pitch),cos(heading)*cos(pitch)};
    double r[3]={cos(heading),0,-sin(heading)},u[3]={r[1]*f[2]-r[2]*f[1],r[2]*f[0]-r[0]*f[2],r[0]*f[1]-r[1]*f[0]},m[16]={0};
    for(int i=0;i<3;i++){m[4*i]=r[i];m[4*i+1]=u[i];m[4*i+2]=-f[i];m[12]-=r[i]*eye[i];m[13]-=u[i]*eye[i];m[14]+=f[i]*eye[i];}
    m[15]=1;glLoadIdentity();if(world.cinematic_active)glRotated(view_roll,0,0,1);glMultMatrixd(m);
}
static void sprite(int index,int x,int y,int z,int shade){
    if((uint32_t)index>=level->sprites.count)return;
    const uint8_t *p=level->sprites.data+index*16;unsigned tile=tr2_u16(p);if(tile>=level->tile_count)return;
    /* Root 40c030/40b6a0 projects a screen rectangle at one shared depth.
       Construct it in view space: independently transformed world-space
       corners can round to different depths and fight with stacked sprites. */
    GLdouble view[16];glGetDoublev(GL_MODELVIEW_MATRIX,view);
    double cx=view[0]*x+view[4]*y+view[8]*z+view[12];
    double cy=view[1]*x+view[5]*y+view[9]*z+view[13];
    double cz=view[2]*x+view[6]*y+view[10]*z+view[14];
    int xs[4]={tr2_i16(p+8),tr2_i16(p+12),tr2_i16(p+12),tr2_i16(p+8)},ys[4]={tr2_i16(p+10),tr2_i16(p+10),tr2_i16(p+14),tr2_i16(p+14)};
    float u0=(p[2]+.5f)/256.f,v0=(p[3]+.5f)/256.f,u1=(p[2]+tr2_u16(p+4)/256.f-.5f)/256.f,v1=(p[3]+tr2_u16(p+6)/256.f-.5f)/256.f;
    glPushAttrib(GL_ENABLE_BIT|GL_CURRENT_BIT);glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);glEnable(GL_TEXTURE_2D);glEnable(GL_ALPHA_TEST);glBindTexture(GL_TEXTURE_2D,textures[tile]);
    uint32_t rgb=tr2_shade_rgb(fog_shade(shade,fog_depth(x,y,z)),camera_water);glColor3ub((GLubyte)(rgb>>16),(GLubyte)(rgb>>8),(GLubyte)rgb);
    glPushMatrix();glLoadIdentity();
    glBegin(GL_QUADS);for(int i=0;i<4;i++){glTexCoord2f(i==0 || i==3?u0:u1,i<2?v0:v1);glVertex3d(cx+xs[i],cy-ys[i],cz);}glEnd();glPopMatrix();glPopAttrib();
}
static int sprite_index(int id){
    for(uint32_t i=0;i<level->sequences.count;i++){const uint8_t *s=level->sequences.data+i*8;if(tr2_i32(s)==id)return tr2_u16(s+6);}return -1;
}
static void visible_rooms(uint8_t visible[1024]){
    double (*rectangles)[4]=room_rectangles;
    for(int i=0;i<1024;i++){rectangles[i][0]=rectangles[i][1]=-1;rectangles[i][2]=rectangles[i][3]=1;}
    if(render_camera_room<0 || (uint32_t)render_camera_room>=level->room_count){memset(visible,1,1024);return;}
    unsigned queue[8192],head=0,tail=0;
    int room=render_camera_room;visible[room]=1;rectangles[room][0]=rectangles[room][1]=-1;rectangles[room][2]=rectangles[room][3]=1;queue[tail++]=(unsigned)room;
    GLdouble projection[16];glGetDoublev(GL_PROJECTION_MATRIX,projection);
    while(head<tail && tail<8192){room=(int)queue[head++];const Tr2Room *rm=level->rooms+room;
        for(uint32_t i=0;i<rm->portals.count;i++){
            const uint8_t *p=rm->portals.data+i*32;unsigned next=tr2_u16(p);if(next>=level->room_count || next>=1024)continue;
            /* Root 418d86..418dd7 only follows portals facing the eye.
               A screen overlap alone can admit rooms behind a closed side. */
            double facing=(rm->x+tr2_i16(p+8)-eye[0])*tr2_i16(p+2)+
                (tr2_i16(p+10)-eye[1])*tr2_i16(p+4)+
                (rm->z+tr2_i16(p+12)-eye[2])*tr2_i16(p+6);
            if(facing>=0)continue;
            double bounds[4]={1,1,-1,-1},view[4][3];int behind=0,beyond=0;
            for(int v=0;v<4;v++){
                double xw=rm->x+tr2_i16(p+8+v*6),yw=tr2_i16(p+10+v*6),zw=rm->z+tr2_i16(p+12+v*6);
                for(int axis=0;axis<3;axis++)
                    view[v][axis]=render_view[axis]*xw+render_view[4+axis]*yw+render_view[8+axis]*zw+render_view[12+axis];
                view[v][2]=-view[v][2];
                double z=view[v][2];
                if(z<=0){behind++;continue;}if(z>render_far_distance())beyond++;
                double x=view[v][0]*projection[0]/z,y=view[v][1]*projection[5]/z;
                if(x-2.0/width<bounds[0])bounds[0]=x-2.0/width;if(y-2.0/height<bounds[1])bounds[1]=y-2.0/height;
                if(x+2.0/width>bounds[2])bounds[2]=x+2.0/width;if(y+2.0/height>bounds[3])bounds[3]=y+2.0/height;
            }
            /* 418f27 / 418fe5: stop traversal when ALL portal vertices are
               beyond the far plane, even though outdoor room silhouettes
               already reached by the traversal may survive that plane. */
            if(behind==4 || beyond==4)continue;
            /* 418ff7..41908c: a crossing of the eye plane expands only
               the affected screen edges, not the whole portal rectangle. */
            if(behind)for(int v=0;v<4;v++){int prev=(v+3)%4;if((view[v][2]<0)==(view[prev][2]<0))continue;
                for(int axis=0;axis<2;axis++){
                    if(view[v][axis]<0 && view[prev][axis]<0)bounds[axis]=-1;
                    else if(view[v][axis]>0 && view[prev][axis]>0)bounds[axis+2]=1;
                    else{bounds[axis]=-1;bounds[axis+2]=1;}
                }
            }
            for(int k=0;k<2;k++){if(bounds[k]<rectangles[room][k])bounds[k]=rectangles[room][k];if(bounds[k+2]>rectangles[room][k+2])bounds[k+2]=rectangles[room][k+2];}
            if(bounds[0]>=bounds[2] || bounds[1]>=bounds[3])continue;
            int changed=!visible[next];
            if(!visible[next])memcpy(rectangles[next],bounds,sizeof bounds);
            else for(int k=0;k<4;k++)if(k<2?bounds[k]<rectangles[next][k]-0.0001:bounds[k]>rectangles[next][k]+0.0001){rectangles[next][k]=bounds[k];changed=1;}
            if(changed && tail<8192){visible[next]=1;queue[tail++]=next;}
        }
    }
}
static void room_scissor(unsigned room){
    /* 4195d4..4195f9 / 4196a2..4196c6: each room and its statics
       inherit the portal-accumulated screen bounds, not the entire viewport. */
    const double *b=room_rectangles[room];
    int x=(int)floor((b[0]+1)*width*.5),y=(int)floor((b[1]+1)*height*.5);
    int right=(int)ceil((b[2]+1)*width*.5),top=(int)ceil((b[3]+1)*height*.5);
    glScissor(x,y,right-x,top-y);
}
static void draw_sky(void){
    /* 418aec..418b47: zero camera translation, rotate the first animation
       frame at +18, draw only the first mesh. Frame position is NOT applied.
       401bd0 supplies uniform 4095 vertex shade, regardless of mesh lighting. */
    const uint8_t *m=tr2_model(level,254);if(!m)return;
    unsigned animation=tr2_u16(m+16);if(animation>=level->animations.count)return;
    unsigned offset=tr2_u32(level->animations.data+animation*32);
    if(offset+20>level->frames.count*2)return;
    const uint8_t *rotation=level->frames.data+offset+18;
    glPushMatrix();glTranslated(eye[0],eye[1],eye[2]);
    rotate_frame(&rotation,level->frames.data+level->frames.count*2);
    sky_mesh=1;mesh_geometry(level,textures,tr2_u16(m+6),0,NULL);sky_mesh=0;glPopMatrix();
}
/* Tomb2.exe 4191a0 intersects the room's interior box with the far plane
   and fills its projected bounding rectangle within the room portal bounds. */

static void draw_room_far_fill(const Tr2Room *room,double far_depth){
    double corners[8][3];
    unsigned beyond=0;
    for(unsigned i=0;i<8;i++){
        double x=room->x+1024*((i&1)?room->nx-1:1);
        double y=(i&2)?room->bottom:room->top;
        double z=room->z+1024*((i&4)?room->nz-1:1);
        for(int axis=0;axis<3;axis++)
            corners[i][axis]=render_view[axis]*x+render_view[4+axis]*y+render_view[8+axis]*z+render_view[12+axis];
        if(corners[i][2]<-far_depth)beyond|=1u<<i;
    }
    if(!beyond || beyond==255)return;
    double left=HUGE_VAL,right=-HUGE_VAL,bottom=HUGE_VAL,top=-HUGE_VAL;
    for(unsigned i=0;i<8;i++)for(unsigned bit=1;bit<=4;bit<<=1){
        if(i&bit)continue;
        unsigned j=i|bit;
        if(((beyond>>i)^(beyond>>j))&1u){
            const double *a=corners[i],*b=corners[j];
            double t=(-far_depth-a[2])/(b[2]-a[2]);
            double x=a[0]+t*(b[0]-a[0]),y=a[1]+t*(b[1]-a[1]);
            left=fmin(left,x);right=fmax(right,x);bottom=fmin(bottom,y);top=fmax(top,y);
        }
    }
    glBegin(GL_QUADS);
    glVertex3d(left,bottom,-far_depth);glVertex3d(right,bottom,-far_depth);
    glVertex3d(right,top,-far_depth);glVertex3d(left,top,-far_depth);
    glEnd();
}
static void ground_shadow(const Tr2Actor *a,int id){
    int size=shadow_size(id);if(!size)return;
    int bounds[6],points[8][2];Tr2Height h;
    if(!tr2_best_bounds(level,a,bounds) || !tr2_query(level,a->x,a->y,a->z,a->room,&h,NULL) || h.floor==TR2_NO_HEIGHT)return;
    if(render_entry && render_entry->root.tick==render_tick)for(int i=0;i<6;i++)bounds[i]=(int)lround(render_entry->bounds[0][i]+render_alpha*(render_entry->bounds[1][i]-render_entry->bounds[0][i]));
    tr2_shadow_points(bounds,size,points);
    glPushAttrib(GL_ENABLE_BIT|GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_CURRENT_BIT|GL_POLYGON_BIT);
    glDisable(GL_TEXTURE_2D);glDisable(GL_LIGHTING);glDisable(GL_ALPHA_TEST);glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);
    /* Hardware polygon type 13 carries alpha 0x80 (40b1d0). */
    glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(-1,-1);glColor4f(0,0,0,128.f/255.f);
    glPushMatrix();glTranslated(a->x,h.floor,a->z);glRotated(a->yaw*(360.0/65536),0,1,0);
    if(render_entry)render_sample(&render_entry->root,1,id);
    glBegin(GL_POLYGON);for(int i=0;i<8;i++)glVertex3i(points[i][0],0,points[i][1]);glEnd();
    glPopMatrix();glPopAttrib();
}
static void render(void){
    if(water_style && water_faces_flip!=world.flip_status)classify_water_surfaces();
    render_light_level=*level;render_light_world=world;render_light_hazards=*world.hazards;
    render_light_level.world=&render_light_world;render_light_world.hazards=&render_light_hazards;
    if(render_light_count[0]==render_light_count[1])for(int i=0;i<render_light_count[1];i++){
        Tr2DynamicLight *p=render_light_hazards.lights+i;const Tr2DynamicLight *a=render_lights[0]+i,*b=render_lights[1]+i;
        if(abs(a->x-b->x)<=1024 && abs(a->y-b->y)<=1024 && abs(a->z-b->z)<=1024){
            p->x=(int)lround(a->x+render_alpha*(b->x-a->x));p->y=(int)lround(a->y+render_alpha*(b->y-a->y));p->z=(int)lround(a->z+render_alpha*(b->z-a->z));
        }
    }
    world_fog=0;
    if(width<1 || height<1)return;
    glViewport(0,0,width,height);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDisable(GL_CULL_FACE);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();double fov=world.cinematic_active?view_fov:80;double top=16*tan(fov*PI/360)*0.75,right=top*width/(height*render_pixel_scale);
    glFrustum(-right,right,-top,top,16,render_far_distance());glMatrixMode(GL_MODELVIEW);camera_matrix();glGetDoublev(GL_MODELVIEW_MATRIX,render_view);
    uint8_t alternate[1024]={0},visible[1024]={0};if(cutscene){/* InitCinematicRooms 0x412060 includes every non-alternate room. */
        for(unsigned i=0;i<level->room_count;i++){visible[i]=1;room_rectangles[i][0]=room_rectangles[i][1]=-1;room_rectangles[i][2]=room_rectangles[i][3]=1;}
    }else visible_rooms(visible);
    outside_visible=0;
    if(tr2_model(level,254))for(uint32_t i=0;i<level->room_count;i++)if(visible[i] && (level->rooms[i].flags&8)){outside_visible=1;break;}
    for(uint32_t i=0;i<level->room_count;i++)if(level->rooms[i].alternate>=0)alternate[level->rooms[i].alternate]=1;
    Tr2Sector view_sector;camera_water=tr2_find_sector(level,(int)eye[0],(int)eye[1],(int)eye[2],camera.position.room,&view_sector) && (level->rooms[view_sector.room].flags&1);
    /* Original 0x418aec removes camera translation before drawing object 254. */
    glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);
    object_light(-1,0,0,0,0);
    if(outside_visible){
        /* 418d10..418d55 unions non-inside room bounds for the sky pass. */
        double bounds[4]={1,1,-1,-1};
        for(uint32_t i=0;i<level->room_count;i++)if(visible[i] && !(level->rooms[i].flags&64))for(int k=0;k<4;k++){
            double v=room_rectangles[i][k];if(k<2?v<bounds[k]:v>bounds[k])bounds[k]=v;
        }
        int x=(int)floor((bounds[0]+1)*width*.5),y=(int)floor((bounds[1]+1)*height*.5);
        int right_edge=(int)ceil((bounds[2]+1)*width*.5),top_edge=(int)ceil((bounds[3]+1)*height*.5);
        if(right_edge>x && top_edge>y){glEnable(GL_SCISSOR_TEST);glScissor(x,y,right_edge-x,top_edge-y);draw_sky();glDisable(GL_SCISSOR_TEST);}
    }
    glDepthMask(GL_TRUE);glEnable(GL_DEPTH_TEST);
    /* Room silhouettes can survive the original 20480 far flag (4020a0).
       Give them depth-buffer space without extending object draw distance.
       Projection x/y and the original fog thresholds are unchanged. */
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glFrustum(-right,right,-top,top,16,draw_distance_setting?level_max_distance:1048576);glMatrixMode(GL_MODELVIEW);
    world_fog=1;fog_forward[0]=sin(heading)*cos(pitch);fog_forward[1]=sin(pitch);fog_forward[2]=cos(heading)*cos(pitch);
    texture_phase=(int)(simulation_ticks?(simulation_ticks-1)/5:0);
    /* Distance shade changes with the camera, even between simulation ticks.
       Submit visible rooms directly: recording and immediately replaying a
       display list each frame adds work without reusing the calculated data. */
    glEnable(GL_SCISSOR_TEST);
    if(!draw_distance_setting && outside_visible){
        glPushAttrib(GL_ENABLE_BIT|GL_CURRENT_BIT|GL_DEPTH_BUFFER_BIT);
        glDisable(GL_TEXTURE_2D);glDisable(GL_LIGHTING);glDisable(GL_BLEND);glDisable(GL_ALPHA_TEST);
        glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);glColor3ub(0,0,0);
        glPushMatrix();glLoadIdentity();
        for(uint32_t i=0;i<level->room_count;i++)if(!alternate[i] && visible[i] && (level->rooms[i].flags&64)){
            room_scissor(i);draw_room_far_fill(level->rooms+i,render_far_distance());
        }
        glPopMatrix();glPopAttrib();
    }
    for(uint32_t i=0;i<level->room_count;i++)if(!alternate[i] && visible[i]){room_scissor(i);draw_room(i);}
    /* Preserve the previous far clipping for statics, sprites and actors;
       all passes use the same projection/depth mapping. */
    glPushMatrix();glLoadIdentity();const GLdouble object_far[4]={0,0,1,render_far_distance()};glClipPlane(GL_CLIP_PLANE0,object_far);glPopMatrix();glEnable(GL_CLIP_PLANE0);
    /* 41972f tests static bounds against the room portal rectangle, but
       4019e0 resets polygon clipping to the full viewport. A static mesh can
       cross its owning room's portal bounds (e.g. Home's stair railings). */
    glDisable(GL_SCISSOR_TEST);
    for(uint32_t i=0;i<level->room_count;i++)if(!alternate[i] && visible[i])room_statics(i);
    glEnable(GL_SCISSOR_TEST);
    for(uint32_t i=0;i<level->room_count;i++)if(!alternate[i] && visible[i]){
        room_scissor(i);
        const Tr2Room *r=level->rooms+i;
        for(uint32_t j=0;j<r->sprites.count;j++){const uint8_t *s=r->sprites.data+j*4;const uint8_t *v=r->vertices.data+tr2_u16(s)*12;sprite(tr2_u16(s+2),r->x+tr2_i16(v),tr2_i16(v+2),r->z+tr2_i16(v+4),tr2_i16(v+6));}
    }
    glDisable(GL_SCISSOR_TEST);
    for(uint32_t i=0;i<world.count;i++){
        if((int)i==level->lara)continue;
        if(i<level->items.count && alternate[tr2_u16(level->items.data+i*24+2)])continue;
        const Tr2Object *o=world.objects+i;const Tr2Actor *a=&o->pose;
        if(o->listed_room<0 || (o->flags&0x8000))continue;
        if(tr2_pickup_id(o->id) && (o->status==2 || o->status==3))continue;
        /* Original setup assigns DrawDummyItem (0x42a120, a bare RET)
           to these helpers at 0x43bc87 and 0x43c752. Their placeholder
           mesh offsets alias Lara's hips; they are not visible props. */
        if(o->id==243 || o->id==253 || o->id==257)continue;
        if(!render_visible_entry(1+i,a->room,visible))continue;
        if(o->status==3)continue;
        int spr=sprite_index(o->id);if(spr>=0){
            /* DrawSpriteItem 4199c0 uses the placed item's two shades via
               451580, not room ambient/directional light. Its -4096 bias is
               undone before S_DrawSprite, which applies distance shading. */
            int shade=4096;
            if(i<level->items.count){const uint8_t *item=level->items.data+i*24;shade=tr2_i16(item+18);
                if(level->rooms[a->room].light_mode)shade+=(tr2_i16(item+20)-shade)*tr2_room_light_phase(level,level->rooms+a->room,light_phase)/31;}
            shade=tr2_dynamic_shade(&render_light_world,a->x,a->y,a->z,shade);
            sprite(spr,a->x,a->y,a->z,shade);continue;
        }
        const uint8_t *animation=level->animations.data+a->control.animation*32;
        render_entry=render_entries?render_entries+1+i:NULL;
        ground_shadow(a,o->id);
        int light_x=a->x,light_y=a->y,light_z=a->z;render_particle_position(1+i,o->id,&light_x,&light_y,&light_z);
        /* CalculateObjectLighting (41be89/41bf34) uses the item's authored
           shades and origin when its first shade is nonnegative. */
        int shade=-1;
        if(i<level->items.count && (unsigned)a->room<level->room_count){
            const uint8_t *shades=level->items.data+i*24+18;
            if(tr2_i16(shades)>=0)shade=placement_shade(level->rooms+a->room,shades);
        }
        object_light(a->room,light_x,shade>=0?light_y:light_y-512,light_z,shade);
        model_creature=world.creatures+i;
        model(o->id==13 && o->snow.drive.armed?51u:(uint32_t)o->id,a->control.animation,a->control.frame-tr2_i16(animation+16),a->x,a->y,a->z,a->yaw*(360.0/65536),tr2_object_mesh_mask(o),a->pitch,a->control.roll,(o->id==13 || o->id==51)?o->effect_counter:0);model_creature=NULL;
        if(o->id==22 && o->effect_counter< -100){
            render_entry=NULL;
            for(int id=218;id<=219;id++){
                const uint8_t *bones=tr2_model(level,id);
                if(bones)model(id,tr2_u16(bones+16),0,a->x,o->lift_origin,a->z,a->yaw*(360.0/65536),
                               id==218?UINT64_C(0xff3fffff):UINT64_MAX,0,0,0);
            }
        }
        if(o->id==22 || o->id==218){
            const uint8_t *front=tr2_model(level,o->id),*back=tr2_model(level,o->id+1);
            if(front && back){int anim=tr2_u16(back+16)+a->control.animation-tr2_u16(front+16);
                render_entry=render_entries?render_entries+render_hair()+1:NULL;
                model(o->id+1,anim,a->control.frame-tr2_i16(animation+16),a->x,a->y,a->z,a->yaw*(360.0/65536),0x1fffff,a->pitch,a->control.roll,0);}
        }
    }
    render_entry=NULL;
    for(int i=0;i<TR2_MAX_FRAGMENTS;i++){
        const Tr2Fragment *f=world.fragments+i;if(!f->active || !render_visible_entry(render_fragments()+i,f->room,visible))continue;
        int shade=f->shade-4096;if(shade<0)shade=0;
        object_light(f->room,f->x,f->y,f->z,shade);
        glPushMatrix();glTranslated(f->x,f->y,f->z);glRotated(f->yaw*(360.0/65536),0,1,0);glRotated(f->pitch*(360.0/65536),1,0,0);glRotated(f->roll*(360.0/65536),0,0,1);
        if(render_entries)render_sample(&render_entries[render_fragments()+i].root,0,f->object);
        draw_mesh(f->mesh);glPopMatrix();
    }
    if(world.hazards){
        for(int i=0;i<TR2_MAX_DARTS;i++){const Tr2Object *d=world.hazards->darts+i;const Tr2Actor *a=&d->pose;if(!d->active || !render_visible_entry(render_darts()+i,a->room,visible))continue;
            const Tr2Level *assets=d->id==248 || d->id==249?tr2_weapon_assets(level,d->id):level;
            const uint8_t *m=tr2_model(assets,(uint32_t)d->id);if(!m)continue;object_light(a->room,a->x,a->y,a->z,-1);
            if(d->id>=209 && d->id<=211){
                glPushMatrix();glTranslated(a->x,a->y,a->z);glRotated(a->yaw*(360.0/65536),0,1,0);
                float scale=d->timer/256.f;glScalef(scale,scale,scale);draw_mesh(tr2_u16(m+6));glPopMatrix();continue;
            }
            if(d->id==225){
                model(d->id,a->control.animation,a->control.frame-tr2_i16(level->animations.data+a->control.animation*32+16),a->x,a->y,a->z,a->yaw*(360.0/65536),UINT64_MAX,0,0,0);
                continue;
            }
            glPushMatrix();glTranslated(a->x,a->y,a->z);glRotated(a->yaw*(360.0/65536),0,1,0);glRotated(a->pitch*(360.0/65536),1,0,0);glRotated(a->control.roll*(360.0/65536),0,0,1);if(render_entries)render_sample(&render_entries[render_darts()+i].root,0,d->id);draw_mesh(tr2_u16(m+6)+(assets==level?0:level->mesh_offsets.count));
            if(d->id==152 && ((uint16_t)d->timer&32768)){const uint8_t *flame=tr2_model(level,235);if(flame){object_light(-1,0,0,0,2048);glTranslatef(-6,6,80);glRotatef(-16380*(360.f/65536),1,0,0);glRotatef((world.hazards->random_draw&65535)*(360.f/65536),0,0,1);draw_mesh(tr2_u16(flame+6));}}glPopMatrix();}
        for(int i=0;i<TR2_MAX_EFFECTS;i++){const Tr2Fragment *f=world.hazards->effects+i;if(!f->active || !render_visible_entry(render_effects()+i,f->room,visible))continue;if(f->object==240 || f->object==245 || f->object==247){const uint8_t *flash=tr2_model(level,f->object);if(flash){object_light(-1,0,0,0,f->shade);glPushMatrix();glTranslatef(f->x,f->y,f->z);glRotatef(f->yaw*(360.f/65536),0,1,0);glRotatef(f->pitch*(360.f/65536),1,0,0);glRotatef(f->roll*(360.f/65536),0,0,1);if(render_entries)render_sample(&render_entries[render_effects()+i].root,0,f->object);draw_mesh(tr2_u16(flash+6));glPopMatrix();}continue;}int base=sprite_index(f->object);if(base>=0){int x=f->x,y=f->y,z=f->z;render_particle_position(render_effects()+i,f->object,&x,&y,&z);sprite(base-f->mesh,x,y,z,f->object==90?f->shade:0);}}
    }
    const uint8_t *anim=level->animations.data+actor.control.animation*32;
    int light_x=actor.x,light_y=actor.y,light_z=actor.z;render_particle_position(0,0,&light_x,&light_y,&light_z);
    object_light(actor.room,light_x,light_y-512,light_z,-1);
    if(!(world.objects[level->lara].flags&256)){
        render_entry=render_entries;
        ground_shadow(&actor,0);
        model(0,actor.control.animation,actor.control.frame-tr2_i16(anim+16),actor.x,actor.y,actor.z,actor.yaw*(360.0/65536),UINT64_MAX,actor.pitch,actor.control.roll,0);
        braid();
    }
    render_entry=NULL;
    glDisable(GL_CLIP_PLANE0);
    if(water_style){
        /* Blend after opaque room geometry and actors. Keep depth testing so
           objects above the surface stay in front; retain the PC texture mask. */
        glPushAttrib(GL_ENABLE_BIT|GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_CURRENT_BIT);
        glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);glEnable(GL_SCISSOR_TEST);water_surface_pass=1;
        for(uint32_t i=0;i<level->room_count;i++)if(!alternate[i] && visible[i]){room_scissor(i);draw_room(i);}
        water_surface_pass=0;glPopAttrib();
    }
    world_fog=0;
}

static void update_eye(void){
    const Tr2Camera *previous=render_tick && !memcmp(&camera,&render_camera_current,sizeof camera)?&render_camera_previous:&camera;
    double t=render_alpha,target[3];
    view_fov=(previous_fov+t*(current_fov-previous_fov))*(360.0/65536);
    view_roll=(previous_roll+t*tr2_word(current_roll-previous_roll))*(360.0/65536);
    eye[0]=previous->position.x+t*(camera.position.x-previous->position.x);
    eye[1]=previous->position.y+previous->shift+t*((double)camera.position.y+camera.shift-previous->position.y-previous->shift);
    eye[2]=previous->position.z+t*(camera.position.z-previous->position.z);
    target[0]=previous->target.x+t*(camera.target.x-previous->target.x);target[1]=previous->target.y+t*(camera.target.y-previous->target.y);target[2]=previous->target.z+t*(camera.target.z-previous->target.z);
    render_camera_room=camera.position.room;Tr2Sector sector;
    if(tr2_find_sector(level,(int)eye[0],(int)eye[1],(int)eye[2],previous->position.room,&sector))render_camera_room=sector.room;
    double dx=target[0]-eye[0],dy=target[1]-eye[1],dz=target[2]-eye[2];
    heading=atan2(dx,dz);pitch=atan2(dy,sqrt(dx*dx+dz*dz));
}
void tr2_home_close(void){
    quick_draw=0;
    tomb_render_surface_free(&resolution_surface);
    tomb_render_surface_free(&aspect_surface);
    tr2_audio_close();
    free(render_entries);render_entries=render_entry=NULL;render_count=0;render_tick=0;render_alpha=1;
    if(level){glDeleteTextures((GLsizei)level->tile_count,textures);if(mesh_lists)glDeleteLists(mesh_lists,(GLsizei)level->mesh_offsets.count);tr2_world_free(&world);tr2_free(level);}
    for(int i=0;i<1024;i++){free(water_faces[i]);water_faces[i]=NULL;}
    if(weapon_assets){glDeleteTextures((GLsizei)weapon_assets->tile_count,weapon_textures);tr2_free(weapon_assets);weapon_assets=NULL;}
    free(cut_meshes);cut_meshes=NULL;
    free(mesh_radius);mesh_radius=NULL;
    level=NULL;mesh_lists=0;memset(&world,0,sizeof world);memset(textures,0,sizeof textures);
}
static int cut_room(int x,int y,int z,int fallback){
    /* Tomb2.exe 4123b0 tests the interior sectors of every room. */
    for(unsigned i=0;i<level->room_count;i++){
        const Tr2Room *r=level->rooms+i;
        if(x>=r->x+1024 && x<r->x+(r->nx-1)*1024 && z>=r->z+1024 && z<r->z+(r->nz-1)*1024 && y>=r->top && y<=r->bottom)return (int)i;
    }
    return fallback;
}
static void cut_actor(Tr2Actor *a,int id,Tr2Object *object){
    a->x=world.cinematic_origin.x;a->y=world.cinematic_origin.y;a->z=world.cinematic_origin.z;a->yaw=(int16_t)scene_yaw;
    int joint[3]={0};if(tr2_joint(level,a,id,0,joint))a->room=(int16_t)cut_room(joint[0],joint[1],joint[2],a->room);
    if(object){
        if((a->control.flags&128) && object->status!=3)
            tr2_dynamic_light(&world,joint[0],joint[1],joint[2],12,11);
        tr2_object_animate(level,object);
    }else tr2_animate(level,a);
    for(int i=0;i<a->event_count;i++)if(a->event_kind[i]==6){
        int effect=a->event_value[i];
        if(effect==14 && !object)cut_pistol_hands^=2;
        if(effect==15 && !object)cut_pistol_hands^=1;
        if(effect>=18 && effect<=20){
            const uint8_t *m=tr2_model(level,id),*replacement=tr2_model(level,97+effect-18);
            if(!m || !replacement)continue;
            unsigned count=tr2_u16(m+4),first=tr2_u16(m+6),other=tr2_u16(replacement+6);
            if(count>tr2_u16(replacement+4) || first+count>level->mesh_offsets.count || other+count>level->mesh_offsets.count)continue;
            for(unsigned n=0;n<count;n++){
                unsigned original=cut_meshes[first+n];cut_meshes[first+n]=cut_meshes[other+n];cut_meshes[other+n]=original;
            }
            if(!object && effect==20){cut_lara_swapped=1;cut_pistol_hands=0;}
        }
        if(effect==21 && object)object->status=3;
        if(effect==22 && object)object->status=1;
        if(effect==23)a->control.flags|=128;
        if(effect==24)a->control.flags&=(uint16_t)~128u;
        if(effect==26)hair_state.ready=0;
    }
}
static int open_session(const char *root,const char *filename,int level_index,const char *slot,int flags,int item,int extra,int carry,unsigned options,const unsigned char choices[7]){
    char path[MAX_PATH],error[256];
    snprintf(path,sizeof path,"%s/sine.bin",root);if(!tr2_math_load(path))return 0;
    snprintf(path,sizeof path,"%s/%s",root,filename);Tr2Level *candidate=tr2_load(path,error,sizeof error);
    if(!candidate){fprintf(stderr,"TR2: %s\n",error);return 0;}
    Tr2World fresh={0};Tr2Actor start={0};Tr2Water supply={1800,0};
    int cinema=level_index<0;
    if(candidate->tile_count>256 || candidate->lara<0 || (!cinema && !tr2_actor_init(candidate,&start)) || !tr2_world_init(candidate,&fresh)){tr2_free(candidate);return 0;}
    Tr2CampaignTotals totals=cinema || carry?prior_totals:(Tr2CampaignTotals){0};
    if(cinema){start=fresh.objects[candidate->lara].pose;fresh.cinematic_active=1;fresh.cinematic_frame=-1;
        fresh.cinematic_origin=(Tr2Position){start.x,start.y,start.z,start.room,0};fresh.cinematic_yaw=scene_yaw;}
    else {
        if(carry){
            Tr2World previous={0};previous.inventory=&carry_inventory;previous.combat=&carry_combat;
            tr2_campaign_carry(&fresh,&previous);totals=tr2_campaign_totals();
        }else tr2_campaign_start(&fresh,level_index);
        if(!slot && level_index>1){
            tr2_campaign_inventory_flags(&fresh,flags);
            if(item){tr2_inventory_add(&fresh,item);tr2_campaign_entry(&fresh);}
            if(!tr2_arrival_start(candidate,&fresh,&start,extra)){tr2_world_free(&fresh);tr2_free(candidate);return 0;}
        }
    }
    if(choices && !carry && !slot && !cinema)tr2_start_weapons(&fresh,choices);
    candidate->custom=(Tr2Customisation){.options=options,.sidearm=fresh.inventory->count[135]?1:0,.last_drawn=fresh.combat->last_weapon};
    if(fresh.combat->weapon==2 || fresh.combat->weapon==3)candidate->custom.sidearm=fresh.combat->weapon;
    if((cinema || carry) && (carry_custom.options&TR2_CONSISTENT_WEAPONS) && !(flags&1)){
        candidate->custom=carry_custom;
        candidate->custom.switch_pending=candidate->custom.campaign_home=0;
        fresh.combat->back_weapon=carry_combat.back_weapon;
        if(!cinema && carry_custom.last_drawn){
            fresh.combat->weapon=fresh.combat->requested=fresh.combat->last_weapon=carry_custom.last_drawn;
        }
        *fresh.start_combat=*fresh.combat;
    }
    if(slot && !tr2_load_save(slot,candidate,&fresh,&start,&supply,&totals)){tr2_world_free(&fresh);tr2_free(candidate);return 0;}
    tr2_home_close();session_level=level_index;prior_totals=totals;campaign_home_exit=0;level=candidate;world=fresh;actor=start;water=supply;cutscene=cinema;cut_pistol_hands=0;level->world=&world;world.lara=&actor;
    cut_lara_swapped=0;campaign_home_exit=level->custom.campaign_home;autosave_due=0;autosave_inventory(1);
    const char *basename=strrchr(filename,'/');basename=basename?basename+1:filename;
    const char *backslash=strrchr(basename,'\\');
    cut1_laptop=cinema && !_stricmp(backslash?backslash+1:basename,"CUT1.TR2");
    if(cinema){
        cut_meshes=malloc(level->mesh_offsets.count*sizeof *cut_meshes);
        if(!cut_meshes){tr2_home_close();return 0;}
        for(unsigned i=0;i<level->mesh_offsets.count;i++)cut_meshes[i]=i;
    }
    init_draw_distance();
    if(!water_surfaces_init()){tr2_home_close();return 0;}

    world.assault_scores=saved_scores;
    health_previous=actor.control.health;health_timer=100;death_ticks=death_return=0;
    memset(&camera,0,sizeof camera);memset(&look,0,sizeof look);memset(&hair_state,0,sizeof hair_state);
    memset(pickup_icons,0,sizeof pickup_icons);
    previous_input=0;simulation_ticks=hair_random=0;hair_wind=light_phase=texture_phase=camera_water=world_fog=0;accumulator=0;
    for(int i=0;i<32;i++){hair_random=hair_random*0x41c64e6du+12345;water_offsets[i]=(int)((hair_random>>16)&31);}
    if(cutscene || world.cinematic_active)camera.position.room=actor.room;
    if(cutscene || world.cinematic_active)tr2_world_camera(level,&world,&actor,&camera,&look);else for(int i=0;i<60;i++)tr2_camera_tick(level,&actor,&camera);
    mesh_radius=calloc(level->mesh_offsets.count,sizeof *mesh_radius);
    if(!mesh_radius){tr2_home_close();return 0;}
    if(level_index==0 || level_index==18 || (level->custom.options&TR2_CONSISTENT_WEAPONS)){
        for(int gun=1;gun<=7;gun++)if(!tr2_model(level,tr2_weapon_model(gun))){
            snprintf(path,sizeof path,"%s/DATA/WALL.TR2",root);
            weapon_assets=tr2_load(path,error,sizeof error);
            if(weapon_assets && weapon_assets->tile_count<=256)upload_pages(weapon_assets,weapon_textures);
            else{if(weapon_assets)tr2_free(weapon_assets);weapon_assets=NULL;fprintf(stderr,"TR2B weapon meshes unavailable: %s\n",path);}
            break;
        }
    }
    level->weapon_donor=weapon_assets;
    update_eye();init_geometry();braid_update();
    render_count=render_hair()+2;render_entries=calloc(render_count,sizeof *render_entries);
    if(!render_entries){tr2_home_close();return 0;}tr2_home_interpolation_reset();
    /* Sound availability does not prevent rendering or controlling Lara. */
    if(!tr2_audio_init(level,root))fprintf(stderr,"TR2 Home audio unavailable\n");
    fprintf(stderr,"TR2B: %s, Lara item %d room %d at %d,%d,%d yaw %d\n",filename,level->lara,actor.room,actor.x,actor.y,actor.z,actor.yaw);
    return 1;
}
int tr2_level_open(const char *root,const char *filename,int index){return open_session(root,filename,index,NULL,0,0,0,0,index<0?carry_custom.options:0,NULL);}
int tr2_level_start(const char *root,const char *filename,int index,int flags,int item,int extra,int campaign_home,int carry,unsigned options,const unsigned char choices[7]){
    if(!open_session(root,filename,index,NULL,flags,item,extra,carry,options,choices))return 0;
    level->custom.campaign_home=campaign_home_exit=index==0 && campaign_home;
    return 1;
}
static int on_home_gate_tiles(const Tr2Actor *a){
    /* ASSAULT room 50: four gravel quads along the west face of the estate gate. */
    return a->room==50 && a->x>=58368 && a->x<59392 && a->z>=68608 && a->z<72704 &&
        a->y==2560 && a->control.health>0 && !(a->control.flags&8);
}
int tr2_level_save(const char *path,int index){return tr2_slot_write(path,index,level,&world,&actor,&water,&prior_totals);}
int tr2_level_restore(const char *root,const char *file,int index,const char *path){return open_session(root,file,index,path,0,0,0,0,0,NULL);}
void tr2_home_update(double seconds,uint32_t input){
    if(!level)return;
    if((input&(TR2_FORWARD|TR2_BACK))==(TR2_FORWARD|TR2_BACK))input|=TR2_ROLL;
    if((input&(TR2_LEFT|TR2_RIGHT))==(TR2_LEFT|TR2_RIGHT))input&=~(TR2_LEFT|TR2_RIGHT);
    accumulator+=seconds;
    while(accumulator>=1.0/30){
        if(death_return || world.finished || world.inventory_request){accumulator=0;break;}
        level->listener_room=camera.position.room;
        if(cutscene){
            world.hazards->light_count=0;
            cut_actor(&actor,0,NULL);
            for(uint32_t i=0;i<world.count;i++){Tr2Object *o=world.objects+i;if(o->id>=123 && o->id<=132){cut_actor(&o->pose,o->id,o);tr2_world_room(&world,(int)i,o->pose.room);}
                else if(o->active && o->id==253)tr2_emitter_tick(level,&world,o,(int)i);}
            tr2_hazards_tick(level,&world,&actor);
            tr2_world_camera(level,&world,&actor,&camera,&look);camera.position.room=(int16_t)cut_room(camera.position.x,camera.position.y,camera.position.z,camera.position.room);simulation_ticks++;braid_update();render_capture();
            if(world.cinematic_frame+1>=(int)level->cinematic.count)world.finished=1;
            accumulator-=1.0/30;continue;
        }
        unsigned finishes=world.assault_scores.total;
        uint32_t pressed=input&~previous_input;previous_input=input;
        uint32_t movement=tr2_world_look_input(&world,&actor,&look,(input&~(0x80000u|32u))|(pressed&(0x80000u|32u)));
        actor.event_count=0;
        /* ExtraLaraControl 428830 resets the death counter before AnimateLara. */
        int hold_creature_death=actor.extra_animation && (actor.control.current==2 || actor.control.current==3) &&
            actor.control.frame<tr2_i16(level->animations.data+actor.control.animation*32+18)-30;
        if(quick_draw){movement|=32;quick_draw=0;}
        tr2_player_tick(level,&actor,&water,movement);tr2_world_view_tick(level,&world,&actor,movement,&look);
        autosave_inventory(0);
        if(level->custom.switch_pending && !actor.extra_animation && actor.control.current!=40 && actor.control.current!=41){
            level->custom.switch_pending=0;autosave_due=1;
        }
        update_pickup_icons(&actor);
        if(health_timer>0)health_timer--;
        if(campaign_home_exit && on_home_gate_tiles(&actor))world.finished=1;
        int health=actor.control.health<0?0:actor.control.health>1000?1000:actor.control.health;
        if(health!=health_previous){health_previous=health;health_timer=40;}
        if(actor.control.health<=0){
            if(!death_ticks)tr2_audio_stop_music();
            death_ticks=hold_creature_death?1:death_ticks+1;
            death_return=tr2_home_death_ready(death_ticks,input);
        }
        if(world.assault_scores.total!=finishes)scores_save();
        tr2_world_camera(level,&world,&actor,&camera,&look);tr2_flip_effect_tick(&world,&actor,camera.target.y);
        tr2_audio_tick(level,&actor,&world);
        simulation_ticks++;hair_random=hair_random*0x41c64e6du+12345;light_phase=(int)((hair_random>>16)&31);braid_update();render_capture();
        accumulator-=1.0/30;
    }
    render_alpha=fmax(0,fmin(1,accumulator*30));update_eye();tr2_audio_pump();
}
static void hud_sprite(int index,int x,int y,int scale){
    if((unsigned)index>=level->sprites.count)return;
    const uint8_t *s=level->sprites.data+index*16;
    unsigned tile=tr2_u16(s);if(tile>=level->tile_count)return;
    float u=(s[2]+.5f)/256.f,v=(s[3]+.5f)/256.f;
    float ue=(s[2]+tr2_u16(s+4)/256.f-.5f)/256.f,ve=(s[3]+tr2_u16(s+6)/256.f-.5f)/256.f;
    int x0=x+tr2_asr(scale*tr2_i16(s+8),16),x1=x+tr2_asr(scale*tr2_i16(s+12),16),y0=y+tr2_asr(scale*tr2_i16(s+10),16),y1=y+tr2_asr(scale*tr2_i16(s+14),16);
    glBindTexture(GL_TEXTURE_2D,textures[tile]);glBegin(GL_QUADS);
    glTexCoord2f(u,v);glVertex2i(x0,y0);glTexCoord2f(ue,v);glVertex2i(x1,y0);
    glTexCoord2f(ue,ve);glVertex2i(x1,y1);glTexCoord2f(u,ve);glVertex2i(x0,y1);glEnd();
}

static void sprite_hud(void){
    int active=world.assault_visible;
    for(int i=0;i<12;i++)active|=pickup_icons[i].ticks;
    if(!active)return;
    glPushAttrib(GL_ENABLE_BIT|GL_CURRENT_BIT);glDisable(GL_DEPTH_TEST);glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,.5f);glColor3f(1,1,1);
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,width,height,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    if(world.assault_visible){
        Tr2TimerGlyph glyphs[32];int first=sprite_index(261);
        int count=tr2_assault_glyphs((unsigned)world.assault_ticks,width,glyphs);
        if(first>=0)for(int i=0;i<count;i++)hud_sprite(first+glyphs[i].sprite,glyphs[i].x,glyphs[i].y,65536);
    }
    int step=width/10,x=width-step,y=height-step,drawn=0;
    for(int i=0;i<12;i++,x-=step)if(pickup_icons[i].ticks){
        if(drawn==4 || drawn==8){x=width-step;y-=2*step/3;}
        hud_sprite(pickup_icons[i].sprite,x,y,12288);
        drawn++;
    }
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glPopAttrib();
}
static void bar_line(int x0,int y0,int x1,int y1,int red,int green,int blue){
    /* 423b10 chooses the closest level-palette entry for each UI colour. */
    int best=0,distance=INT_MAX;
    for(int i=0;i<256;i++){const uint8_t *p=level->palette+i*3;int r=p[0]*4-red,g=p[1]*4-green,b=p[2]*4-blue,d=r*r+g*g+b*b;if(d<distance){distance=d;best=i;}}
    const uint8_t *p=level->palette+best*3;glColor3ub((uint8_t)(p[0]*4),(uint8_t)(p[1]*4),(uint8_t)(p[2]*4));
    int left=x0<x1?x0:x1,top=y0<y1?y0:y1,right=x0>x1?x0:x1,bottom=y0>y1?y0:y1;
    glBegin(GL_QUADS);glVertex2i(left,top);glVertex2i(right+1,top);glVertex2i(right+1,bottom+1);glVertex2i(left,bottom+1);glEnd();
}
static void status_bar(int amount,int air){
    if(amount<0)return;int x=air?width-110:8;
    for(int y=7;y<=13;y++)bar_line(x-2,y,x+101,y,0,0,0);
    bar_line(x-2,14,x+102,14,64,64,64);bar_line(x+102,6,x+102,14,64,64,64);
    bar_line(x-2,6,x+102,6,255,255,255);bar_line(x-2,6,x-2,14,255,255,255);
    if(amount>0)for(int y=8;y<=12;y++)bar_line(x,y,x+amount,y,air?(y==9?255:0):255,air?(y==9?255:0):(y==9?128:0),air?255:0);
}
static void status_bars(void){
    glPushAttrib(GL_ENABLE_BIT|GL_CURRENT_BIT);glDisable(GL_DEPTH_TEST);glDisable(GL_TEXTURE_2D);glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glDisable(GL_ALPHA_TEST);
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,width,height,0,-1,1);glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    int flash=(simulation_ticks/6)%2==0;
    status_bar(tr2_health_bar(actor.control.health,health_timer,actor.gun_status,flash),0);
    status_bar(tr2_air_bar(water.air,actor.control.water_status,flash),1);
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glPopAttrib();
}
void tr2_home_render(int w,int h){
    if(w<1 || h<1)return;
    int full_w=w,full_h=h;if(force43)tomb_render_fit43(&w,&h);
    width=w;height=h;if(!level)return;
    tr2_resolution_extent(resolution_setting,w,h,&width,&height);
    render_pixel_scale=(double)width*h/(height*(double)w);
    render();
    if(width!=w || height!=h)tomb_render_enlarge(&resolution_surface,width,height,w,h);
    width=w;height=h;render_pixel_scale=1;glViewport(0,0,w,h);if(!cutscene){
        sprite_hud();status_bars();
        int weapon=world.combat->weapon;
        if(actor.gun_status==4 && weapon>=2 && weapon<=7)
            tr2_draw_ammo(level,textures,w,h,world.combat->ammo[weapon]/(weapon==4?6:1));
    }
    tomb_render_center(&aspect_surface,w,h,full_w,full_h);
}
int tr2_home_presentation_test(int death){
    if(!level)return 0;
    if(!death){
        Tr2Actor saved=actor;Tr2Water saved_water=water;unsigned ticks=simulation_ticks;
        actor.control.health=200;actor.control.water_status=1;water.air=400;simulation_ticks=0;
        tr2_home_render(width,height);actor=saved;water=saved_water;simulation_ticks=ticks;
        return glGetError()==GL_NO_ERROR;
    }
    actor.control.health=-1;
    for(int i=0;i<60;i++)tr2_home_update(1.0/30,0);
    tr2_home_death_input();if(!tr2_home_dead() || death_return)return 0;
    tr2_home_update(1.0/30,0);tr2_home_death_input();if(!death_return)return 0;
    death_return=0;
    for(int i=61;i<300;i++)tr2_home_update(1.0/30,0);
    if(death_return)return 0;tr2_home_update(1.0/30,0);
    return death_return && !tr2_audio_music_active();
}
void tr2_home_menu_sound(int sound){if(level)tr2_audio_menu(level,sound);}
int tr2_home_distance_test(void){
    int saved=draw_distance_setting,ok=level_max_distance>20480;
    unsigned ticks=simulation_ticks;Tr2Actor saved_actor=actor;
    size_t bytes=(size_t)width*height*4;
    uint8_t *original=malloc(bytes),*extended=malloc(bytes),*restored=malloc(bytes);
    if(!original || !extended || !restored){free(original);free(extended);free(restored);return 0;}
    tr2_home_draw_distance(0);tr2_home_render(width,height);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,original);
    tr2_home_draw_distance(1);tr2_home_render(width,height);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,extended);
    ok=ok && render_far_distance()==level_max_distance && fog_shade(2345,50000)==2345;
    tr2_home_draw_distance(2);
    const int shades[]={0,4096,7000,7864,8100,8191};
    for(unsigned i=0;i<sizeof shades/sizeof shades[0];i++){
        int shade=shades[i];
        ok=ok && fog_shade(shade,50000)==(shade<7864?7864:shade);
        ok=ok && fog_shade(shade,12288)==shade;
        ok=ok && fog_shade(shade,12289)==(shade<7864?shade+1:shade);
    }
    tr2_home_render(width,height);
    glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,restored);
    ok=ok && memcmp(extended,restored,bytes)!=0 && glGetError()==GL_NO_ERROR;
    tr2_home_draw_distance(DRAW_SMOOTH_FOG);
    /* Sideways and forward displacement contribute equally to smooth fog.
       This mesh straddles the fog start even though its origin is nearer. */
    const double transform[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,-12000,1};
    int off_axis=tomb_mesh_camera_distance(transform,9000,0,0);
    ok=ok && off_axis==15000 && fog_shade(4096,off_axis)==6808;
    ok=ok && fog_shade(4096,12288)==4096 && fog_shade(4096,50000)==7864 && fog_shade(8100,50000)==8100;
    tr2_home_render(width,height);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,extended);
    ok=ok && memcmp(extended,restored,bytes)!=0 && glGetError()==GL_NO_ERROR;
    tr2_home_draw_distance(0);tr2_home_render(width,height);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,restored);
    ok=ok && memcmp(original,extended,bytes)!=0 && !memcmp(original,restored,bytes);
    ok=ok && ticks==simulation_ticks && !memcmp(&actor,&saved_actor,sizeof actor) && glGetError()==GL_NO_ERROR;
    free(original);free(extended);free(restored);tr2_home_draw_distance(saved);
    fprintf(stderr,"TR2B Max Distance: range %.0f, distinct render, all-room fog cap/dark surfaces, exact Original restoration, no simulation advance: %s\n",level_max_distance,ok?"PASS":"FAIL");return ok;
}
int tr2_home_depth_check(int w,int h){
    /* Exercise real room rendering across small camera rotations. Distant
       silhouettes must retain a range of depths, not collapse onto one plane. */
    if(!level || w<1 || h<1)return 0;
    float *depths=malloc((size_t)w*h*sizeof *depths);if(!depths)return 0;
    double saved_heading=heading,nearest=1048576,farthest=0;unsigned samples=0;
    for(int view=-2;view<=2;view++){
        heading=saved_heading+view*.03;tr2_home_render(w,h);
        glReadPixels(0,0,w,h,GL_DEPTH_COMPONENT,GL_FLOAT,depths);
        for(size_t i=0;i<(size_t)w*h;i++)if(depths[i]>0 && depths[i]<1){
            double z=16.0*1048576/(1048576-(double)depths[i]*(1048576-16));
            if(z>20480 && z<262144){samples++;if(z<nearest)nearest=z;if(z>farthest)farthest=z;}
        }
    }
    heading=saved_heading;free(depths);
    int ok=samples>100 && farthest-nearest>1024 && glGetError()==GL_NO_ERROR;
    fprintf(stderr,"TR2 far-room depth: %u samples, %.0f..%.0f: %s\n",samples,nearest,farthest,ok?"PASS":"FAIL");return ok;
}
int tr2_home_check(int moved){
    if(!level || level->lara!=8 || !camera.initialized || actor.control.health<=0)return 0;
    if(!moved)return actor.room==16 && actor.x==19968 && actor.y==2560 && actor.z==95744 && actor.yaw==-32768;
    return actor.room>=0 && (unsigned)actor.room<level->room_count && actor.z!=95744 && isfinite(heading) && isfinite(pitch);
}
int tr2_rig_water_view_test(void){
    /* Look west from the starting cell, through the bars under the seaplane. */
    int cinematic=world.cinematic_active,distance=draw_distance_setting,style=water_style;
    eye[0]=48384;eye[1]=-2260;eye[2]=91500;heading=-PI/2;pitch=.06;render_camera_room=4;
    world.cinematic_active=0;draw_distance_setting=0;
    int ok=1;
    for(water_style=0;water_style<=1;water_style++){
        render();
        GLubyte pixels[4*4*4];glReadPixels(width/2,height*343/720,4,4,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
        for(int i=0;i<16;i++)if(pixels[i*4]>100 && pixels[i*4]>pixels[i*4+2]+50)ok=0;
    }
    world.cinematic_active=cinematic;draw_distance_setting=distance;water_style=style;update_eye();
    fprintf(stderr,"TR2B Offshore Rig water at Original distance, PC and PS1 styles: %s\n",ok?"PASS":"FAIL");
    return ok;
}
int tr2_home_water_test(void){
    /* PC ASSAULT has 25 pool overlay quads on each side of the boundary. */
    for(int r=13;r<=14;r++){
        unsigned count=0;for(uint32_t j=0;j<level->rooms[r].quads.count;j++)count+=water_faces[r][j];
        if(count!=25){fprintf(stderr,"TR2 pool surface classification: room %d got %u, expected 25\n",r,count);return 0;}
    }
    int surface=TR2_NO_HEIGHT,found=0;
    for(unsigned ri=0;ri<level->room_count && !found;ri++){
        const Tr2Room *room=level->rooms+ri;if(!(room->flags&1))continue;
        for(int x=1;x<room->nx-1 && !found;x++)for(int z=1;z<room->nz-1 && !found;z++){
            int px=room->x+x*1024+512,pz=room->z+z*1024+512,depth;
            int wh=tr2_water_height(level,px,pz,(int)ri,&depth);
            if(wh==TR2_NO_HEIGHT || depth<=1024)continue;
            actor.x=px;actor.y=wh+384;actor.z=pz;actor.room=(int16_t)ri;surface=wh;found=1;
        }
    }
    if(!found)return 0;
    memset(world.hazards->effects,0,sizeof world.hazards->effects);
    actor.control.water_status=0;actor.control.current=3;actor.control.flags|=8;actor.fall_speed=100;
    tr2_water_transition(level,&actor,&water);
    int count=0;for(int i=0;i<TR2_MAX_EFFECTS;i++)if(world.hazards->effects[i].active){
        const Tr2Fragment *f=world.hazards->effects+i;if(f->object!=230 || f->y!=surface)return 0;count++;
    }
    if(count!=10 || actor.control.water_status!=1)return 0;
    unsigned random=world.hazards->random_draw;tr2_water_transition(level,&actor,&water);
    if(random!=world.hazards->random_draw)return 0;
    camera.position=(Tr2Position){actor.x,surface-1024,actor.z-1024,actor.room,0};
    Tr2Sector sector;if(!tr2_find_sector(level,camera.position.x,camera.position.y,camera.position.z,actor.room,&sector))return 0;
    camera.position.room=(int16_t)sector.room;camera.target=(Tr2Position){actor.x,surface,actor.z,actor.room,0};camera.shift=0;
    update_eye();braid_update();tr2_home_interpolation_reset();tr2_home_render(width,height);
    int ok=sprite_index(230)>=0 && glGetError()==GL_NO_ERROR;
    fprintf(stderr,"TR2 water entry: ten surface splash sprites, no repeat while submerged, render: %s\n",ok?"PASS":"FAIL");return ok;
}
int tr2_home_flare_pickup_test(void){
    Tr2Object *pickup=NULL;for(unsigned i=0;i<world.count;i++)if(world.objects[i].id==151 && world.objects[i].status!=3){pickup=world.objects+i;break;}
    if(!pickup)return 0;
    actor.x=pickup->pose.x;actor.y=pickup->pose.y;actor.z=pickup->pose.z-100;actor.room=pickup->pose.room;actor.yaw=actor.pitch=actor.control.roll=0;
    actor.gun_status=0;actor.speed=actor.fall_speed=0;actor.control.water_status=0;actor.control.flags&=(uint16_t)~8u;
    actor.control.current=actor.control.goal=2;tr2_set_animation(level,&actor,103,0);
    int before=tr2_home_flare_count();
    for(int tick=0;tick<100;tick++)tr2_home_update(1.0/30,TR2_ACTION);
    int ok=tr2_home_flare_count()==before+6 && pickup->status==3;
    fprintf(stderr,"TR2 real flare-box pickup: count %d -> %d, status %d, state/frame %d/%d: %s\n",before,tr2_home_flare_count(),pickup->status,actor.control.current,actor.control.frame,ok?"PASS":"FAIL");
    return ok;
}
int tr2_home_flare_use_test(void){
    int before=tr2_home_flare_count();
    /* Inventory use has requested the first flare; let its draw complete. */
    for(int i=0;i<100;i++)tr2_home_update(1.0/30,0);
    int ok=world.combat->weapon==8 && !actor.gun_status && tr2_home_flare_count()==before-1 && world.hazards->light_count>0;
    tr2_home_update(1.0/30,0x80000);
    for(int i=0;i<100;i++)tr2_home_update(1.0/30,0x80000); /* Holding must not draw another. */
    int thrown=0;for(int i=0;i<TR2_MAX_DARTS;i++)if(world.hazards->darts[i].active && world.hazards->darts[i].id==152)thrown++;
    ok=ok && thrown==1 && world.combat->weapon!=8 && tr2_home_flare_count()==before-1;
    tr2_home_update(1.0/30,0);tr2_home_update(1.0/30,0x80000);
    for(int i=0;i<100;i++)tr2_home_update(1.0/30,0);
    ok=ok && world.combat->weapon==8 && tr2_home_flare_count()==before-2 && world.hazards->light_count>0;
    fprintf(stderr,"TR2 flare inventory draw, consumption, light, throw and keyboard draw: %s\n",ok?"PASS":"FAIL");return ok;
}
int tr2_home_assault_test(void){
    Tr2Actor start=actor,finish=actor,rest=actor;int found=0;
    /* Find the real effect-30/31 floor triggers in ASSAULT.TR2, rather
       than simulating a finish by directly setting timer state. */
    for(unsigned ri=0;ri<level->room_count;ri++){
        const Tr2Room *room=level->rooms+ri;
        for(int x=1;x<room->nx-1;x++)for(int z=1;z<room->nz-1;z++){
            Tr2Height h;Tr2Sector sector;int px=room->x+x*1024+512,pz=room->z+z*1024+512;
            if(!tr2_query(level,px,room->bottom-256,pz,(int)ri,&h,&sector) || !h.trigger || h.trigger+2>=level->floors.count)continue;
            const uint8_t *words=level->floors.data;
            if((tr2_u16(words+h.trigger*2)&255)!=4)continue;
            for(unsigned at=h.trigger+2;at<level->floors.count;at++){
                unsigned command=tr2_u16(words+at*2),kind=(command>>10)&15,id=command&1023;
                if(kind==9 && (id==30 || id==31)){
                    Tr2Actor *a=id==30?&start:&finish;*a=rest;a->x=px;a->y=h.floor;a->z=pz;a->room=(int16_t)sector.room;found|=id==30?1:2;
                }
                if(command&32768)break;if(kind==1)at++;
            }
        }
    }
    if(found!=3)return 0;
    actor=start;tr2_world_tick(level,&world,&actor,0);
    if(!world.assault_active || !world.assault_visible || world.assault_ticks)return 0;
    actor=rest;for(int i=0;i<150;i++)tr2_world_tick(level,&world,&actor,0);
    if(world.assault_ticks!=150)return 0;
    actor=finish;tr2_world_tick(level,&world,&actor,0);
    if(world.assault_active || !world.assault_visible || world.assault_ticks!=151 || world.assault_best!=151 || world.cd_play!=22)return 0;
    actor=rest;for(int i=0;i<30;i++)tr2_world_tick(level,&world,&actor,0);
    if(world.assault_ticks!=151)return 0;
    actor=start;tr2_world_tick(level,&world,&actor,0);
    if(!world.assault_active || world.assault_ticks)return 0;
    actor=rest;for(int i=0;i<37;i++)tr2_world_tick(level,&world,&actor,0);
    int held=world.assault_ticks;for(int i=0;i<3;i++)tr2_home_render(width,height);
    int ok=held==37 && world.assault_ticks==held && sprite_index(261)>=0;
    fprintf(stderr,"TR2 assault real start/finish triggers, 30 Hz ticks, frozen finish, restart and render: %s\n",ok?"PASS":"FAIL");return ok;
}
#include "../tests/tr2_interpolation_render.h"

/* Checkpoints use the placed items in WALL.TR2 and the ordinary collision code. */
int tr2_wall_key_test(int key,int phase){
    Tr2Object *item=NULL;int id=phase==0?key:key+8;
    for(unsigned i=0;i<world.count;i++)if(world.objects[i].id==id){item=world.objects+i;break;}
    if(!item)return 0;
    if(phase==2){
        for(int i=0;i<185;i++)tr2_home_update(1.0/30,0);
        return !world.inventory->count[key] && item->status==2 && world.objects[key==193?0:6].pose.control.current==1;
    }
    tr2_actor_init(level,&actor);actor.room=item->pose.room;actor.y=item->pose.y;
    actor.yaw=actor.move_angle=phase?item->pose.yaw:0;
    actor.x=item->pose.x;actor.z=item->pose.z;
    if(!phase){
        actor.z+=100;
        if(level->rooms[actor.room].flags&1){actor.control.water_status=1;actor.control.current=actor.control.goal=13;tr2_set_animation(level,&actor,108,0);}
        for(int i=0;i<90 && actor.control.current!=39;i++){
            tr2_world_tick(level,&world,&actor,TR2_ACTION);
            if(actor.control.current!=39)tr2_player_tick(level,&actor,&water,TR2_ACTION);
        }
        for(int i=0;i<110;i++)tr2_home_update(1.0/30,0);
        return world.inventory->count[key]==1 && item->status==3;
    }
    actor.x+=tr2_asr(362*tr2_sin(actor.yaw),14);actor.z+=tr2_asr(362*tr2_cos(actor.yaw),14);
    tr2_world_tick(level,&world,&actor,TR2_ACTION);
    return world.inventory_request;
}
int tr2_wall_ammo_hud_test(void){
    int saved_status=actor.gun_status,weapon=world.combat->weapon,ammo=world.combat->ammo[4];
    unsigned char pixels[3][64*40*4];int w=width,h=height;
    if(force43)tomb_render_fit43(&w,&h);
    if(w<64 || h<40)return 0;
    for(int i=0;i<3;i++){
        actor.gun_status=4;world.combat->weapon=i?4:1;world.combat->ammo[4]=i==2?6:12;
        tr2_home_render(width,height);
        glReadPixels((width+w)/2-64,(height+h)/2-40,64,40,GL_RGBA,GL_UNSIGNED_BYTE,pixels[i]);
    }
    actor.gun_status=saved_status;world.combat->weapon=weapon;world.combat->ammo[4]=ammo;
    return memcmp(pixels[0],pixels[1],sizeof pixels[0]) && memcmp(pixels[1],pixels[2],sizeof pixels[0]);
}
int tr2_wall_water_fog_test(void){
    int saved=draw_distance_setting,overflows=0,ok=1;
    for(unsigned i=0;i<level->room_count;i++){
        const Tr2Room *room=level->rooms+i;
        if(!(room->flags&1))continue;
        for(unsigned j=0;j<room->vertices.count;j++){
            const uint8_t *v=room->vertices.data+12*j;
            int phase=(water_offsets[(room->vertices.count-j)&31]+(int)simulation_ticks)&31;
            int shade=tr2_i16(v+6)+tr2_asr(tr2_sin(phase*2048)*768,14);
            if(shade<=8191)continue;
            overflows++;
            for(int mode=DRAW_ORIGINAL;mode<DRAW_DISTANCE_COUNT;mode++){
                tr2_home_draw_distance(mode);
                if(tr2_shade_rgb(fog_shade(shade,1024),1)!=0xff000000u ||
                   tr2_shade_rgb(fog_shade(shade,16000),1)!=0xff000000u)ok=0;
            }
        }
    }
    tr2_home_draw_distance(saved);
    fprintf(stderr,"Great Wall water fog: %d over-range water vertices checked at fixed phase across four modes: %s\n",overflows,ok && overflows?"PASS":"FAIL");
    return ok && overflows;
}
int tr2_wall_reward_test(void){
    for(int id=190;id<=192;id++){
        Tr2Object *item=NULL;for(unsigned i=0;i<world.count;i++)if(world.objects[i].id==id){item=world.objects+i;break;}
        if(!item)return 0;tr2_actor_init(level,&actor);actor.room=item->pose.room;actor.x=item->pose.x;actor.y=item->pose.y;actor.z=item->pose.z-100;actor.yaw=actor.move_angle=0;
        int seen=0;
        for(int i=0;i<100;i++){
            tr2_home_update(1.0/30,i?0:TR2_ACTION);
            for(int slot=0;slot<12;slot++)if(pickup_icons[slot].ticks==74 && pickup_icons[slot].sprite==sprite_index(id)){
                if(tr2_audio_music_cue()!=47 || pickup_icons[slot].sprite<0)return 0;
                seen++;
            }
        }
        if(seen!=1)return 0;
    }
    return world.inventory->secrets==7 && world.rewarded && world.inventory->count[141]==1 && world.combat->ammo[6]==6;
}

int tr2_cut_swap_test(void){
    /* CUT3 switches Lara at frame 1. CUT4 switches actor 124 at frame 630. */
    const uint8_t *swap=tr2_model(level,99);
    if(swap){
        const uint8_t *lara=tr2_model(level,0);
        return model_mesh(0,tr2_u16(lara+6),0,0)==tr2_u16(swap+6);
    }
    swap=tr2_model(level,97);
    if(!swap)return 1;
    for(unsigned i=0;i<world.count;i++)if(world.objects[i].id==124){
        Tr2Object *o=world.objects+i;
        for(int n=0;n<700 && o->pose.control.frame<630;n++)tr2_home_update(1.0/30,0);
        const uint8_t *m=tr2_model(level,124);
        return o->pose.control.frame==630 && model_mesh(124,tr2_u16(m+6),0,0)==tr2_u16(swap+6);
    }
    return 0;
}
int tr2_cut_camera_test(int frame,int cut){
    if(world.cinematic_frame!=frame || fabs(render_alpha-.5)>1e-5)return 0;
    Tr2Camera prior=camera;
    int index=frame-2,fov,roll;
    if(!tr2_cinematic_camera(level,&world.cinematic_origin,world.cinematic_yaw,&index,&prior,&fov,&roll))return 0;
    double fraction=cut?1:.5;
    return fabs(eye[0]-(prior.position.x+fraction*(camera.position.x-prior.position.x)))<.01 &&
        fabs(eye[1]-(prior.position.y+fraction*(camera.position.y-prior.position.y)))<.01 &&
        fabs(eye[2]-(prior.position.z+fraction*(camera.position.z-prior.position.z)))<.01 &&
        fabs(view_fov-(fov+fraction*(world.cinematic_fov-fov))*360/65536)<.0001;
}
int tr2_cut_actor_test(void){
    /* Check the first picture's opening motion, its display position, and
       the first instantaneous exchange in the authored laptop animation. */
    for(unsigned i=0;i<world.count;i++)if(world.objects[i].id==128){
        const TombRenderTrack *pose=&render_entries[1+i].pose;
        int frame=world.objects[i].pose.control.frame;
        if(frame==2184 && !memcmp(pose->nodes[1].previous,pose->nodes[1].current,sizeof pose->nodes[1].current))return 0;
        if(frame==2188 && fabs(pose->nodes[1].current[12]+779)>.1)return 0;
        if(frame==2204)for(int panel=1;panel<=2;panel++)
            if(memcmp(pose->nodes[panel].previous,pose->nodes[panel].current,sizeof pose->nodes[panel].current))return 0;
    }
    if(world.cinematic_frame==318 && (!render_entries[0].pose.reset || !render_entries[0].root.reset))return 0;
    for(unsigned i=0;i<world.count;i++){
        const Tr2Object *o=world.objects+i;
        if(o->id!=123)continue;
        int frame=o->pose.control.frame;
        const uint8_t *base=tr2_model(level,123),*swap=tr2_model(level,98);
        unsigned start=tr2_u16(base+6);
        if(frame>=791 && frame<999 && o->status!=3)return 0;
        if(frame>=999 && o->status==3)return 0;
        return model_mesh(123,start,0,0)==(frame>=1300 && frame<1790?tr2_u16(swap+6):start);
    }
    return 0;
}
int tr2_cut1_pistols_test(int frame){
    const uint8_t *lara=tr2_model(level,0),*pistol=tr2_model(level,1);
    if(!cutscene || !lara || !pistol || actor.control.frame!=frame)return 0;
    unsigned base=tr2_u16(lara+6),gun=tr2_u16(pistol+6),hand=frame>=319 && frame<1977?gun:base;
    return model_mesh(0,base,10,0)==hand+10 && model_mesh(0,base,13,0)==hand+13 &&
        model_mesh(0,base,1,0)==(hand==gun?base:gun)+1 && model_mesh(0,base,4,0)==(hand==gun?base:gun)+4;
}
int tr2_home_gate_test(int campaign){
    Tr2Actor saved=actor;
    actor.room=50;actor.y=2560;actor.x=57856;actor.z=69120;
    tr2_home_update(1.0/30,0);
    if(world.finished)return 0;
    for(int tile=0;tile<4;tile++){
        actor.x=58880;actor.z=69120+1024*tile;actor.y=2560;
        tr2_home_update(1.0/30,0);
        if(world.finished!=campaign)return 0;
        if(tile<3)world.finished=0;
    }
    actor=saved;tr2_home_interpolation_reset();return 1;
}
static int venice_water_matches(unsigned expected_faces){
    tr2_home_render(width,height);
    const Tr2Room *canal=level->rooms+50;
    if(canal->quads.count+canal->triangles.count!=expected_faces || water_faces_flip!=world.flip_status)return 0;
    for(unsigned i=0;i<level->room_count;i++){
        const Tr2Room *r=level->rooms+i;
        for(unsigned j=0;j<r->quads.count;j++)if(water_faces[i][j]!=water_surface(r,r->quads.data+10*j,4))return 0;
        for(unsigned j=0;j<r->triangles.count;j++)if(water_faces[i][r->quads.count+j]!=water_surface(r,r->triangles.data+8*j,3))return 0;
    }
    return glGetError()==GL_NO_ERROR;
}
int tr2_venice_water_test(void){
    int saved_style=water_style;
    water_style=1;
    int ok=venice_water_matches(19);
    const Tr2Actor *lever=&world.objects[48].pose;
    actor.room=lever->room;actor.yaw=actor.move_angle=lever->yaw;actor.y=lever->y;
    actor.x=lever->x+tr2_asr(400*tr2_sin(lever->yaw),14);
    actor.z=lever->z+tr2_asr(400*tr2_cos(lever->yaw),14);
    for(int tick=0;tick<180 && !world.flip_status;tick++)tr2_home_update(1.0/30,tick?0:TR2_ACTION);
    ok=ok && world.flip_status && venice_water_matches(25);
    if(world.flip_status)tr2_world_flip(level,&world);
    ok=ok && venice_water_matches(19);
    water_style=saved_style;
    fprintf(stderr,"TR2B Venice PS1 water before and after canal switch and flip back: %s\n",ok?"PASS":"FAIL");
    return ok;
}
int tr2_venice_exit_test(void){
    actor.room=146;actor.x=25088;actor.y=1152;actor.z=79360;
    tr2_home_update(1.0/30,0);return world.finished;
}
int tr2_creature_contact_test(const int *ids,unsigned count){
    for(unsigned kind=0;kind<count;kind++){
        unsigned i=0;while(i<world.count && world.objects[i].id!=ids[kind])i++;
        if(i==world.count)return 0;
        Tr2Level scene=*level;Tr2World isolated=world;Tr2Object enemy=world.objects[i];
        scene.world=&isolated;isolated.level=&scene;isolated.objects=&enemy;isolated.creatures=world.creatures+i;isolated.count=1;
        enemy.status=1;enemy.pose.control.flags|=32;
        int tested=0,extent=enemy.id==37?1024:192;
        if(enemy.id==37){enemy.pose.x=actor.x;enemy.pose.y=actor.y;enemy.pose.z=actor.z;enemy.pose.room=actor.room;}
        for(int z=-extent;z<=extent && !tested;z+=64)for(int x=-extent;x<=extent && !tested;x+=64){
            Tr2Actor start;tr2_actor_init(level,&start);start.room=enemy.pose.room;
            start.x=enemy.pose.x+x;start.y=enemy.pose.y;start.z=enemy.pose.z+z;
            Tr2Contact contact={0};contact.radius=100;contact.flags=24;
            contact.old[0]=start.x;contact.old[1]=start.y;contact.old[2]=start.z;
            Tr2Actor lara=start;Tr2Contact live=contact;
            tr2_player_object_contacts(&scene,&lara,0,&live);
            if(lara.x==start.x && lara.z==start.z)continue;
            lara=start;lara.control.water_status=1;
            Tr2Contact submerged=contact;submerged.radius=300;
            tr2_player_object_contacts(&scene,&lara,0,&submerged);
            if(lara.x!=start.x || lara.z!=start.z || isolated.hit_direction>=0)return 0;
            /* CreatureDie clears bit 32 when the death animation ends. */
            enemy.pose.control.flags&=(uint16_t)~32u;lara=start;
            tr2_player_object_contacts(&scene,&lara,0,&contact);
            if(lara.x!=start.x || lara.z!=start.z || isolated.hit_direction>=0)return 0;
            tested=1;
        }
        if(!tested){fprintf(stderr,"TR2B contact missing for object %d\n",ids[kind]);return 0;}
    }
    return 1;
}
static Tr2Object *test_placed(int id){
    for(unsigned i=0;i<world.count;i++)if(world.objects[i].id==id)return world.objects+i;
    return NULL;
}
/* Compare the recorded rotations to the authored weapon frame, including
   the separate left and right frame counters in the executable's pistol path. */
static int weapon_pose_test(void){
    const Tr2Combat *g=world.combat;
    int dual=g->weapon>=1 && g->weapon<=3;
    if(actor.gun_status<2 || actor.gun_status>4 || (!dual && !g->rifle_active))return 1;
    const Tr2Level *assets=tr2_weapon_assets(level,dual?1:tr2_weapon_model(g->weapon));
    const uint8_t *model_data=tr2_model(assets,dual?1:tr2_weapon_model(g->weapon));
    if(!model_data)return 0;
    Tr2Look aiming;tr2_combat_look_pose(&actor,g,&look,&aiming);
    int upper=g->weapon==5 && (g->rifle.control.current==0 || g->rifle.control.current==2 || g->rifle.control.current==4);
    for(int joint=upper?7:8;joint<=(upper?14:13);joint++){
        int arm=joint>=11?0:1;
        int frame=dual?g->arms[arm].frame:g->rifle.control.frame;
        int animation=dual?tr2_u16(model_data+16)+(frame>=5)+(frame>=13)+(frame>=24):g->rifle.control.animation;
        const uint8_t *an=assets->animations.data+32*animation;
        const uint8_t *rot=assets->frames.data+tr2_u32(an)+18+(frame-tr2_i16(an+16))*an[5]*2;
        for(int n=0;n<joint;n++)rot+=tr2_u16(rot)>>14?2:4;
        glPushMatrix();glLoadIdentity();
        const TombRenderNode *node=&render_entries[0].pose.nodes[joint];
        const float *actual=node->current;
        if(dual && (joint==8 || joint==11)){
            actual=node->world;
            glRotatef(actor.yaw*(360.f/65536),0,1,0);glRotatef(actor.pitch*(360.f/65536),1,0,0);glRotatef(actor.control.roll*(360.f/65536),0,0,1);
            glRotatef(g->arms[arm].aim.yaw*(360.f/65536),0,1,0);glRotatef(g->arms[arm].aim.pitch*(360.f/65536),1,0,0);glRotatef(g->arms[arm].aim.roll*(360.f/65536),0,0,1);
        }
        rotate_frame(&rot,assets->frames.data+assets->frames.count*2);
        if(joint==7 || joint==14){
            glRotatef((joint==7?aiming.torso_yaw:aiming.yaw)*(360.f/65536),0,1,0);
            glRotatef((joint==7?aiming.torso_pitch:aiming.pitch)*(360.f/65536),1,0,0);
        }
        GLfloat expected[16];glGetFloatv(GL_MODELVIEW_MATRIX,expected);glPopMatrix();
        for(int col=0;col<3;col++)for(int k=0;k<3;k++)if(fabs(actual[col*4+k]-expected[col*4+k])>.003){
            fprintf(stderr,"weapon pose mismatch: gun %d, status %d, frame %d, joint %d\n",g->weapon,actor.gun_status,frame,joint);return 0;
        }
    }
    return 1;
}
int tr2_home_weapon_test(void){
    for(int tick=0;tick<1500 && actor.extra_animation;tick++)tr2_home_update(1.0/30,0);
    int holstered=level->custom.sidearm;
    for(int row=0;row<7;row++){
        int id=tr2_weapon_items[row],gun=tr2_inventory_weapon(id);
        tr2_level_give(id);
        tr2_level_use(id,0);
        for(int tick=0;tick<180 && actor.gun_status!=4;tick++){tr2_home_update(1.0/30,tick?0:32);if(!weapon_pose_test())return 0;}
        if(actor.gun_status!=4 || world.combat->weapon!=gun){fprintf(stderr,"draw test gun %d status %d health %d extra %d\n",gun,actor.gun_status,actor.control.health,actor.extra_animation);return 0;}
        tr2_home_render(width,height);
        if(gun<=3)holstered=gun;
        unsigned base=tr2_u16(tr2_model(level,0)+6);
        for(int thigh=1;thigh<=4;thigh+=3){
            unsigned expected=base+thigh;
            if(gun>=4 && holstered)expected=weapon_mesh(tr2_weapon_model(holstered),thigh,expected);
            if(render_entries[0].pose.nodes[thigh].identity!=(int)expected){
                fprintf(stderr,"holster mesh mismatch: gun %d, sidearm %d, thigh %d\n",gun,holstered,thigh);return 0;
            }
        }
        int shots=world.combat->shots;
        for(int tick=0;tick<120 && world.combat->shots==shots;tick++){tr2_home_update(1.0/30,TR2_ACTION);if(!weapon_pose_test())return 0;}
        if(world.combat->shots==shots || glGetError()!=GL_NO_ERROR){fprintf(stderr,"fire test gun %d health %d\n",gun,actor.control.health);return 0;}
        tr2_home_render(width,height);
        if(gun==5 || gun==7){
            /* Exercise the alternate weapon state machine independently of
               movement/collision, holding the running or swimming input state. */
            Tr2Actor saved=actor;
            if(gun==5)actor.speed=32;else actor.control.water_status=1;
            int alternate_shot=0;
            for(int tick=0;tick<180 && !alternate_shot;tick++){
                actor.event_count=0;
                int before=world.combat->shots;
                tr2_combat_view_tick(level,&world,&actor,tick<45?0:TR2_ACTION,&look);
                render_capture();
                if(!weapon_pose_test())return 0;
                alternate_shot=world.combat->rifle.control.current==8 && world.combat->shots>before;
            }
            actor=saved;
            if(!alternate_shot)return 0;
        }
        for(int tick=0;tick<180 && actor.gun_status;tick++){tr2_home_update(1.0/30,tick?0:32);if(!weapon_pose_test())return 0;}
        if(actor.gun_status || glGetError()!=GL_NO_ERROR){fprintf(stderr,"holster test gun %d status %d health %d\n",gun,actor.gun_status,actor.control.health);return 0;}
    }
    return 1;
}
int tr2_unarmed_shower_test(void){
    if(!world.cinematic_active || !actor.extra_animation || actor.control.current!=10 ||
       !(level->custom.options&TR2_CONSISTENT_WEAPONS) || level->custom.last_drawn)return 0;
    unsigned base=tr2_u16(tr2_model(level,0)+6),shotgun=tr2_u16(tr2_model(level,3)+6);
    int checked=0;
    for(int tick=0;tick<800 && !world.finished;tick++){
        int frame=actor.control.frame-tr2_i16(level->animations.data+actor.control.animation*32+16);
        if(frame==315 || frame==316){
            if(model_mesh(0,base,10,0)!=(frame==315?base:shotgun)+10)return 0;
            tr2_home_render(width,height);checked++;
        }
        tr2_home_update(1.0/30,0);
    }
    return checked==2 && world.finished && glGetError()==GL_NO_ERROR;
}
int tr2_final_level_test(int index){
    if(index==15){
        Tr2Object *p=test_placed(87);if(!p || !test_placed(37))return 0;
        int y=p->pose.y;p->flags=0x3e00;p->active=p->status=1;
        tr2_final_object(level,&world,p,&actor);
        if(p->pose.y!=y+5 || !tr2_creature_contact_test((const int[]){36,37},2))return 0;
    }
    if(index==16){
        Tr2Object *s=test_placed(41),*k=test_placed(19);if(!s || !k || !test_placed(43))return 0;
        s->active=s->status=1;tr2_enemy_tick(level,&world,(int)(s-world.objects),&actor);
        if(!s->mesh_mask)return 0;
        k->active=k->status=1;k->pose.control.current=9;
        tr2_enemy_tick(level,&world,(int)(k-world.objects),&actor);
        int knives=0;for(int i=0;i<TR2_MAX_EFFECTS;i++)knives+=world.hazards->effects[i].active && world.hazards->effects[i].object==247;
        if(knives!=2)return 0;
        /* The executable sets two yaw joints on each of these models. */
        Tr2Creature aim={0};aim.head=5460;aim.neck=-2730;
        if(tr2_creature_joint_rotation(19,1,&aim)!=5460 || tr2_creature_joint_rotation(19,9,&aim)!=-2730 ||
           tr2_creature_joint_rotation(41,7,&aim)!=5460 || tr2_creature_joint_rotation(41,13,&aim)!=-2730 ||
           tr2_creature_joint_rotation(43,7,&aim)!=5460 || tr2_creature_joint_rotation(43,17,&aim)!=-2730)return 0;
        for(int plaque=174;plaque<=175;plaque++){
            Tr2Object *socket=test_placed(plaque+8);
            if(!socket || !tr2_wall_key_test(plaque,0) || !tr2_wall_key_test(plaque,1) ||
               !tr2_level_use(plaque,tr2_level_keys_requested()) || !tr2_puzzle_use(level,&world,socket,&actor,TR2_ACTION))return 0;
            for(int i=0;i<150 && socket->id==plaque+8;i++)tr2_home_update(1.0/30,0);
            if(socket->id!=plaque+12 || world.inventory->count[plaque])return 0;
        }
    }
    if(index==17){
        Tr2Object *d=test_placed(40);if(!d)return 0;
        d->active=d->status=1;actor.x=d->pose.x;actor.z=d->pose.z;
        for(int i=0;i<155 && d->id==40;i++)tr2_final_object(level,&world,d,&actor);
        if(d->id!=22 || d->pose.control.health!=300)return 0;
        Tr2Creature *aim=world.creatures+(d-world.objects);
        int straight[3]={35,171,1168},turned[3]={35,171,1168};
        aim->head=0;if(!tr2_joint(level,&d->pose,22,12,straight))return 0;
        aim->head=5460;if(!tr2_joint(level,&d->pose,22,12,turned) || !memcmp(straight,turned,sizeof straight))return 0;
        aim->head=0;
        Tr2Object before_collapse=*d;
        tr2_player_damage(&world,d,300);
        for(int i=0;i<440 && d->pose.control.health<=0;i++){
            tr2_enemy_tick(level,&world,(int)(d-world.objects),&actor);tr2_hazards_tick(level,&world,&actor);
        }
        if(d->pose.control.health!=150)return 0;
        /* Replay the encounter from its living pose for the alternative ending. */
        *d=before_collapse;tr2_player_damage(&world,d,300);
        int used=0,tries=0;
        for(int i=0;i<330 && !used;i++){
            tr2_enemy_tick(level,&world,(int)(d-world.objects),&actor);
            int offset=d->pose.control.animation-tr2_u16(tr2_model(level,22)+16);
            if(offset!=22 && offset!=23)continue;
            tries++;int s=tr2_sin(d->pose.yaw),c=tr2_cos(d->pose.yaw);
            for(int x=575;x<=1600 && !used;x+=50)for(int z=-300;z<=300 && !used;z+=75){
                tr2_actor_init(level,&actor);actor.room=d->pose.room;actor.y=d->pose.y;
                actor.x=d->pose.x+tr2_asr(x*c+z*s,14);actor.z=d->pose.z+tr2_asr(z*c-x*s,14);
                actor.yaw=tr2_word(d->pose.yaw+16384);
                Tr2Contact contact={0};contact.radius=100;
                tr2_dragon_contact(level,&world,d,&actor,TR2_ACTION,&contact);used=actor.extra_animation;
            }
        }
        if(!used){fprintf(stderr,"Dragon dagger contact failed after %d poses, anim %d state %d frame %d hp %d timer %d\n",tries,d->pose.control.animation-tr2_u16(tr2_model(level,22)+16),d->pose.control.current,d->pose.control.frame,d->pose.control.health,d->effect_counter);return 0;}
        int faded=0,revealed=0;
        for(int i=0;i<600 && actor.extra_animation;i++){
            int timer=d->effect_counter;
            tr2_player_tick(level,&actor,&water,0);tr2_world_tick(level,&world,&actor,0);
            if(timer==-19){
                for(int j=0;j<world.hazards->light_count;j++){
                    const Tr2DynamicLight *light=world.hazards->lights+j;
                    if(light->x==d->pose.x && light->z==d->pose.z && light->intensity>=1 && light->intensity<=4)faded=1;
                }
            }
            if(timer==-101 && d->id==22 && d->pose.y==d->lift_origin+10){
                render_capture();tr2_home_render(width,height);revealed=glGetError()==GL_NO_ERROR;
            }
        }
        if(!faded || !revealed)return 0;
        if(d->id!=218 || !world.inventory->count[175] || !test_placed(264)->active)return 0;
    }
    if(index==18){
        Tr2Object *counter=test_placed(262);if(!counter)return 0;
        int bosses=0;for(unsigned i=0;i<world.count;i++){
            Tr2Object *o=world.objects+i;
            if(o->id==15 || o->id==16 || o->id==32)world.inventory->kills+=tr2_player_damage(&world,o,o->pose.control.health);
        }
        tr2_final_object(level,&world,counter,&actor);
        for(unsigned i=0;i<world.count;i++){Tr2Object *o=world.objects+i;if(o->id==20 && o->active){
            world.inventory->kills+=tr2_player_damage(&world,o,o->pose.control.health);bosses++;}}
        if(bosses!=1)return 0;
        for(int i=0;i<149;i++)tr2_final_object(level,&world,counter,&actor);
        if(!actor.extra_animation || actor.control.current!=10)return 0;
        world.combat->back_weapon=4;
        const uint8_t *lara=tr2_model(level,0),*shotgun=tr2_model(level,3);
        if(!lara || !shotgun)return 0;
        int checked=0;
        for(int i=0;i<800 && !world.finished;i++){
            int frame=actor.control.frame-tr2_i16(level->animations.data+actor.control.animation*32+16);
            if(frame==315 || frame==316){
                unsigned hand=tr2_u16((frame==315?lara:shotgun)+6)+10;
                if(frame==316 && (level->custom.options&TR2_CONSISTENT_WEAPONS) && level->custom.last_drawn)hand=weapon_mesh(tr2_weapon_model(level->custom.last_drawn),10,UINT_MAX);
                if(model_mesh(0,tr2_u16(lara+6),10,0)!=hand)return 0;
                if(frame==316 && (level->custom.options&TR2_CONSISTENT_WEAPONS)){
                    for(int gun=1;gun<=7;gun++){
                        level->custom.last_drawn=gun;
                        unsigned index=model_mesh(0,tr2_u16(lara+6),10,0);
                        if(index==tr2_u16(lara+6)+10 || index!=weapon_mesh(tr2_weapon_model(gun),10,UINT_MAX))return 0;
                        draw_mesh(index);
                    }
                }
                checked++;
            }
            tr2_player_tick(level,&actor,&water,0);
            if(frame>=1 && world.combat->back_weapon)return 0;
        }
        if(checked!=2)return 0;
    }else{
        static const int rooms[]={33,40,21},cells[]={7,41,42};
        int room=rooms[index-15],cell=cells[index-15];const Tr2Room *r=level->rooms+room;
        tr2_actor_init(level,&actor);actor.room=room;actor.x=r->x+(cell/r->nz)*1024+512;actor.z=r->z+(cell%r->nz)*1024+512;
        Tr2Height h;if(!tr2_query(level,actor.x,r->top,actor.z,room,&h,NULL))return 0;
        actor.y=h.floor;tr2_world_tick(level,&world,&actor,0);
    }
    fprintf(stderr,"TR2B final campaign level %d encounter and exit: %s\n",index,world.finished?"PASS":"FAIL");return world.finished;
}
int tr2_tibet_level_test(int index){
    if(index==11){
        for(int key=193;key<=194;key++){
            if(key==194){
                const Tr2Room *r=level->rooms+68;int cell=33;
                actor.room=68;actor.x=r->x+(cell/r->nz)*1024+512;actor.z=r->z+(cell%r->nz)*1024+512;
                actor.y=(int8_t)r->sectors.data[cell*8+5]*256;
                tr2_world_tick(level,&world,&actor,0);if(!world.flip_status)return 0;
            }
            Tr2Object *pickup=test_placed(key);
            if(pickup && pickup->pose.room==255){
                for(unsigned i=0;i<world.count;i++)if(world.objects[i].carried==pickup-world.objects)tr2_enemy_finish(&world,i);
            }
            if(!tr2_wall_key_test(key,0) || !tr2_wall_key_test(key,1)){fprintf(stderr,"Foothills key %d pickup/use request failed\n",key);return 0;}
            if(!tr2_level_use(key,tr2_level_keys_requested()))return 0;
            Tr2Object *lock=test_placed(key+8);
            if(!tr2_key_use(level,&world,lock,&actor,TR2_ACTION))return 0;
            for(int t=0;t<180 && lock->status!=2;t++)tr2_home_update(1.0/30,0);
            if(lock->status!=2 || world.inventory->count[key])return 0;
        }
        return test_placed(52)!=NULL;
    }
    if(index==12){
        Tr2Object *monk=test_placed(53),*other=test_placed(54),*trap=test_placed(64);
        if(!monk || !other || !trap || !test_placed(48))return 0;
        Tr2Creature *c=world.creatures+(monk-world.objects);c->mood=1;
        tr2_player_damage(&world,monk,10);if(monk->linked[1])return 0;
        tr2_player_damage(&world,monk,1);if(!other->linked[1])return 0;
        trap->flags=0x3e00;tr2_tibet_trap_tick(level,&world,trap,&actor);
        return trap->pose.control.goal==1 && tr2_creature_contact_test((const int[]){48,53,54},3);
    }
    if(index==13)return tr2_creature_contact_test((const int[]){45,48},2);
    Tr2Object *gong=test_placed(226),*boss=test_placed(46),*board=test_placed(86),*bell=test_placed(88),*bowl=test_placed(71);
    if(!gong || !boss || !board || !bell || !bowl)return 0;
    actor.x=board->pose.x;actor.y=board->pose.y;actor.z=board->pose.z;actor.room=board->pose.room;
    tr2_tibet_trap_tick(level,&world,board,&actor);if(actor.fall_speed!=-240 || actor.control.current!=3)return 0;
    actor.x=gong->pose.x;actor.y=gong->pose.y;actor.z=gong->pose.z-512;actor.room=gong->pose.room;
    actor.yaw=actor.pitch=actor.control.roll=actor.gun_status=0;actor.control.current=2;actor.control.flags&=~8u;
    world.inventory->count[194]=1;world.inventory->selected=194;
    if(!tr2_detonator_use(level,&world,gong,&actor,TR2_ACTION) || world.inventory->count[194] || actor.control.current!=5)return 0;
    int cue=0,triggered=0;
    for(int i=0;i<300;i++){
        Tr2Object *d=world.hazards->darts;if(!d->active){triggered=1;break;}
        tr2_hazards_tick(level,&world,&actor);if(world.cd_play==61)cue=1;
    }
    if(!cue || !triggered || !world.objects[139].active || !world.objects[140].active)return 0;
    tr2_tibet_trap_tick(level,&world,bell,&actor);
    if(!world.objects[60].active)return 0;
    bowl->pose.control.current=1;bowl->timer=149;
    tr2_tibet_trap_tick(level,&world,bowl,&actor);
    if(!world.flip_status || world.flip_flags[0]!=0x3f00)return 0;
    Tr2Object *talion=test_placed(206);if(!talion)return 0;
    actor.x=talion->pose.x;actor.y=talion->pose.y;actor.z=talion->pose.z;actor.room=talion->pose.room;
    actor.extra_animation=actor.gun_status=actor.pitch=actor.control.roll=0;
    tr2_set_animation(level,&actor,135,42);actor.control.current=39;actor.control.water_status=0;
    if(tr2_pickup_use(level,&world,talion,&actor,0)!=2)return 0;
    Tr2Height floor;if(!tr2_query(level,actor.x,actor.y,actor.z,actor.room,&floor,NULL))return 0;
    tr2_world_trigger(level,&world,&actor,floor.trigger,0);
    if(!boss->active)return 0;
    boss->pose.control.health=0;
    for(int i=0;i<300 && !world.finished;i++)tr2_enemy_tick(level,&world,(int)(boss-world.objects),&actor);
    if(world.finished)fprintf(stderr,"TR2B Tibetan traps, hostility, enemy contacts, gong strike and Guardian exit PASS\n");
    return world.finished;
}
int tr2_fathoms_shark_death_test(void){
    const uint8_t *extra=tr2_model(level,12);if(!extra)return 0;
    tr2_set_animation(level,&actor,tr2_u16(extra+16),0);
    actor.control.goal=3;tr2_animate(level,&actor);
    if(actor.control.current!=3)return 0;
    actor.extra_animation=1;actor.gun_status=1;actor.control.health=-1;
    int release=tr2_i16(level->animations.data+actor.control.animation*32+18)-30;
    if(actor.control.frame>=release)return 0;
    while(actor.control.frame<release){
        int previous=actor.control.frame;
        tr2_home_update(1.0/30,TR2_ACTION);
        if(actor.control.frame<=previous || death_ticks!=1 || death_return)return 0;
    }
    for(int ticks=2;ticks<=301;ticks++){
        tr2_home_update(1.0/30,0);
        if(death_ticks!=ticks || death_return!=(ticks==301))return 0;
        if(ticks==60 || ticks==61){
            tr2_home_death_input();if(death_return!=(ticks==61))return 0;
            death_return=0;
        }
    }
    fprintf(stderr,"TR2B 40 Fathoms shark death animation and passport timing: PASS\n");return 1;
}
int tr2_bartoli_detonator_test(int finish){
    Tr2Object *det=NULL;
    for(unsigned i=0;i<world.count;i++)if(world.objects[i].id==227)det=&world.objects[i];
    if(!det)return 0;
    if(!finish){
        tr2_actor_init(level,&actor);
        actor.room=det->pose.room;actor.x=det->pose.x;actor.y=det->pose.y;actor.z=det->pose.z;
        tr2_home_update(1.0/30,TR2_ACTION);
        return world.inventory_request>=2 && !actor.extra_animation;
    }
    if(!actor.extra_animation || world.inventory->count[194])return 0;
    int explosions=0;
    for(int i=0;i<180;i++){
        tr2_home_update(1.0/30,0);
        for(int e=0;e<det->pose.event_count;e++)if(det->pose.event_kind[e]==7 && det->pose.event_value[e]==105)explosions++;
    }
    return explosions==1 && world.flip_status && !actor.extra_animation && !actor.gun_status;
}
int tr2_wall_effect_test(void){
    Tr2Object *rex=test_placed(214);
    if(!rex || shadow_size(rex->id)!=64 || shadow_size(39)!=128 || shadow_size(22))return 0;
    Tr2Actor step=rex->pose;
    step.control.animation=369;step.control.frame=88;
    step.control.current=step.control.goal=tr2_i16(level->animations.data+369*32+6);
    int saved_camera[3];memcpy(saved_camera,world.ai_camera,sizeof saved_camera);
    world.ai_camera[0]=step.x+8192;world.ai_camera[1]=step.y;world.ai_camera[2]=step.z;
    tr2_animate(level,&step);
    memcpy(world.ai_camera,saved_camera,sizeof saved_camera);
    if(world.camera_bounce!=75)return 0;
    world.camera_bounce=0;
    Tr2Actor swim=actor;swim.control.animation=108;swim.control.frame=1742;
    swim.control.current=swim.control.goal=13;swim.water_distance=-256;
    int listener=level->listener_room;
    for(unsigned room=0;room<level->room_count;room++)if(level->rooms[room].flags&1){level->listener_room=room;break;}
    world.hazards->random_draw=1;
    tr2_animate(level,&swim);level->listener_room=listener;
    int bubbles=0,cue=0;
    for(int i=0;i<TR2_MAX_EFFECTS;i++)if(world.hazards->effects[i].active && world.hazards->effects[i].object==231)bubbles++;
    for(int i=0;i<swim.event_count;i++)if(swim.event_kind[i]==5 && swim.event_value[i]==37)cue++;
    if(!bubbles || !cue)return 0;
    for(unsigned room=0;room<level->room_count;room++)if(!(level->rooms[room].flags&1)){level->listener_room=room;break;}
    swim.control.frame=1742;world.hazards->random_draw=1;
    tr2_animate(level,&swim);level->listener_room=listener;
    for(int i=0;i<swim.event_count;i++)if(swim.event_kind[i]==5 && swim.event_value[i]==37)return 0;
    fprintf(stderr,"TR2B enemy shadow sizes, T. rex footfall bounce and underwater breathing PASS\n");
    return 1;
}
int tr2_wall_exit_test(void){
    /* The supplied WALL.TR2 has the end command at room 65, sector 56. */
    if(level->room_count<=65)return 0;const Tr2Room *r=level->rooms+65;
    tr2_actor_init(level,&actor);actor.room=65;
    actor.x=r->x+(56/r->nz)*1024+512;actor.z=r->z+(56%r->nz)*1024+512;
    Tr2Height h;if(!tr2_query(level,actor.x,r->top,actor.z,65,&h,NULL))return 0;
    actor.y=h.floor;tr2_home_update(1.0/30,0);return world.finished;
}
