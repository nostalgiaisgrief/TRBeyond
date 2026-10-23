/* CUT1 setup/control: DOS 0x14b1a, 0x14d68 and 0x14eb8. */
#include "cinematic.h"
#include "fixed.h"
#include "hazards.h"
#include "peru.h"
#include <string.h>
static int16_t word(const unsigned char *p){return tomb_word(p[0]|p[1]<<8);}
int tomb_cinematic_init(TombCinematic *c,TombObjects *w){
    return tomb_cinematic_init_sequence(c,w,1);
}
int tomb_cinematic_init_sequence(TombCinematic *c,TombObjects *w,int sequence){
    if(sequence<1 || sequence>4)return 0;
    memset(c,0,sizeof *c);const TombLevel *l=w->level;size_t at=l->item_parsed_bytes+8192+768;
    if(at>l->file_size || l->file_size-at<2)return 0;
    unsigned n=(unsigned)(uint16_t)word(l->file_data+at);at+=2;
    if(!n || n>(l->file_size-at)/16 || !w->count)return 0;
    c->frames=l->file_data+at;c->count=n;
    size_t anchor=0;while(anchor<w->source_count && l->items[anchor].object!=77)++anchor;if(anchor==w->source_count)return 0;
    c->origin=(TombActor){0};c->origin.x=l->items[anchor].x;c->origin.y=l->items[anchor].y;c->origin.z=l->items[anchor].z;c->origin.yaw=16384;c->origin.room=l->items[anchor].room;
    if(sequence==1){c->origin.x=0x8f3c;c->origin.z=0xf6cc;c->origin.yaw=tomb_word(0xa4f0);}
    if(sequence==2){c->origin.x=0xcafa;c->origin.z=0xd200;c->origin.yaw=0x3ffc;}
    if(sequence==3 && !tomb_flip_map(w,1))return 0;
    if(sequence==2 || sequence==4){
        TombModel lara,guns;if(!tomb_visual_model(w->visual,77,&lara) || !tomb_visual_model(w->visual,1,&guns))return 0;
        for(int j=1;j<=4;j+=3){TombMeshView mesh=w->visual->meshes[lara.mesh_start+j];w->visual->meshes[lara.mesh_start+j]=w->visual->meshes[guns.mesh_start+j];w->visual->meshes[guns.mesh_start+j]=mesh;}
    }
    for(size_t i=0;i<w->count;i++){
        TombObject *o=w->items+i;TombModel m;
        if(o->object<77 || o->object>80)continue;
        if(!tomb_visual_model(w->visual,o->object,&m) || m.animation>=l->animation_count)return 0;
        const TombAnim *a=l->animations+m.animation;
        if(o->object!=80)o->actor=c->origin;else o->actor.yaw=0;
        o->actor.animation=(int16_t)m.animation;o->actor.frame=a->first_frame;o->actor.current=o->actor.goal=a->state;o->actor.flags=32;
    }return 1;
}
int tomb_cinematic_camera(const TombCinematic *c,const int16_t *sine,TombDosCamera *shot,int *fov,int *roll){
    if(c->frame>=c->count)return 0;const unsigned char *p=c->frames+c->frame*16;int v[8];for(int i=0;i<8;i++)v[i]=word(p+2*i);
    int si=tomb_hazard_sine(sine,c->origin.yaw),co=tomb_hazard_sine(sine,c->origin.yaw+16384);
    shot->target=(TombCameraPoint){c->origin.x+tomb_asr(v[0]*co+v[2]*si,14),c->origin.y+v[1],c->origin.z+tomb_asr(v[2]*co-v[0]*si,14),c->origin.room};
    shot->eye=(TombCameraPoint){c->origin.x+tomb_asr(v[3]*co+v[5]*si,14),c->origin.y+v[4],c->origin.z+tomb_asr(v[5]*co-v[3]*si,14),c->origin.room};
    shot->ready=1;*fov=v[6];*roll=v[7];return 1;
}
int tomb_cinematic_tick(TombCinematic *c,TombObjects *w,TombAnimContext *ctx){
    if(c->frame>=c->count)return 0;
    for(size_t i=0;i<w->count;i++){
        TombObject *o=w->items+i;if(o->object<77 || o->object>80)continue;
        TombActor *a=&o->actor;if(o->object!=80){a->x=c->origin.x;a->y=c->origin.y;a->z=c->origin.z;a->yaw=c->origin.yaw;}
        if(!tomb_object_animate(w->items+i,ctx))return 0;
    }
    ++c->frame;return 1;
}
