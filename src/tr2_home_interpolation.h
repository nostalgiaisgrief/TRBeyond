/* TR2 adapter for the shared, read-only render snapshot tracks. */
static void render_begin_entry(size_t index,const Tr2Actor *a,int id){
    render_entry=render_entries+index;
    float origin[3]={(float)a->x,(float)a->y,(float)a->z};
    tomb_render_begin(&render_entry->pose,render_tick,origin,a->room);
    tomb_render_begin(&render_entry->root,render_tick,origin,a->room);
    if(render_shot_change)render_entry->pose.reset=render_entry->root.reset=1;
    /* Push-block quadrant alignment is a discrete gameplay snap. */
    if(id>=67 && id<=70 && render_entry->root.yaw!=a->yaw)render_entry->pose.reset=render_entry->root.reset=1;
    render_entry->root.yaw=a->yaw;
    glLoadIdentity();glTranslated(a->x,a->y,a->z);glRotated(a->yaw*(360.0/65536),0,1,0);glRotated(a->pitch*(360.0/65536),1,0,0);glRotated(a->control.roll*(360.0/65536),0,0,1);
    render_matrix(&render_entry->root,0,-1,id);
    if(shadow_size(id)){
        memcpy(render_entry->bounds[0],render_entry->bounds[1],sizeof render_entry->bounds[0]);
        tr2_best_bounds(level,a,render_entry->bounds[1]);
        if(render_entry->root.reset)memcpy(render_entry->bounds[0],render_entry->bounds[1],sizeof render_entry->bounds[0]);
        Tr2Height h;if(tr2_query(level,a->x,a->y,a->z,a->room,&h,NULL) && h.floor!=TR2_NO_HEIGHT){
            glLoadIdentity();glTranslated(a->x,h.floor,a->z);glRotated(a->yaw*(360.0/65536),0,1,0);
            render_matrix(&render_entry->root,1,-1,id);
            /* The receiving floor changes discretely at steps/ledges. */
            render_entry->root.nodes[1].previous[13]=render_entry->root.nodes[1].current[13];
        }
    }
    glLoadIdentity();
}
static void render_capture_model(const Tr2Actor *a,int id,uint64_t mask,int track){
    if(a->control.animation<0 || (unsigned)a->control.animation>=level->animations.count)return;
    const uint8_t *animation=level->animations.data+a->control.animation*32;
    model((unsigned)id,a->control.animation,a->control.frame-tr2_i16(animation+16),a->x,a->y,a->z,a->yaw*(360.0/65536),mask,a->pitch,a->control.roll,track);
}
/* Camera records have no shot marker. A step much larger than the travel
   on both sides is a cut, including a stationary camera changing its target. */
static int cinematic_step(const uint8_t *frame,int channel){
    int delta=tr2_i16(frame+channel*2)-tr2_i16(frame-16+channel*2);
    return abs(channel==7?tr2_word(delta):delta);
}
static int cinematic_cut(int frame){
    if(frame<=0 || (unsigned)frame>=level->cinematic.count)return 1;
    const uint8_t *p=level->cinematic.data+frame*16;
    for(int k=0;k<8;k++){
        int before=frame>1?cinematic_step(p-16,k):0;
        int after=(unsigned)(frame+1)<level->cinematic.count?cinematic_step(p+16,k):0;
        int step=cinematic_step(p,k);
        if(step>(k<6?32:128) && step>4*(before>after?before:after))return 1;
    }
    return 0;
}
static void render_capture(void){
    if(!render_entries)return;
    render_tick++;render_recording=1;glMatrixMode(GL_MODELVIEW);glPushMatrix();
    int frame=world.cinematic_active?world.cinematic_frame:-1;
    render_shot_change=(world.cinematic_active || render_cine_frame>=0) &&
        (frame<0 || render_cine_frame<0 || frame!=render_cine_frame+1 || cinematic_cut(frame));
    memcpy(render_lights[0],render_lights[1],sizeof render_lights[0]);render_light_count[0]=render_light_count[1];
    memcpy(render_lights[1],world.hazards->lights,sizeof render_lights[1]);render_light_count[1]=world.hazards->light_count;
    if(render_tick==1){memcpy(render_lights[0],render_lights[1],sizeof render_lights[0]);render_light_count[0]=render_light_count[1];}
    render_begin_entry(0,&actor,0);render_capture_model(&actor,0,UINT64_MAX,0);
    for(unsigned i=0;i<world.count;i++){
        const Tr2Object *o=world.objects+i;if((int)i==level->lara || o->listed_room<0 || o->status==3 || (o->flags&0x8000))continue;
        render_begin_entry(1+i,&o->pose,o->id);model_creature=world.creatures+i;
        render_capture_model(&o->pose,o->id==13 && o->snow.drive.armed?51:o->id,tr2_object_mesh_mask(o),(o->id==13 || o->id==51)?o->effect_counter:0);model_creature=NULL;
        if(cut1_laptop && o->id==128){
            /* CUT1 flips picture panels by half a turn to exchange slides.
               Keep the smaller authored turns that bring the first slide in. */
            for(int panel=1;panel<=6;panel++){
                TombRenderNode *n=&render_entry->pose.nodes[panel];
                float trace=0;
                for(int col=0;col<3;col++)for(int row=0;row<3;row++)
                    trace+=n->previous[col*4+row]*n->current[col*4+row];
                if(trace<-.99f)memcpy(n->previous,n->current,sizeof n->previous);
            }
        }
        if(o->id==22 || o->id==218){
            const uint8_t *front=tr2_model(level,o->id),*rear=tr2_model(level,o->id+1);
            if(front && rear){
                Tr2Actor back=o->pose;
                int offset=back.control.animation-tr2_u16(front+16);
                int frame=back.control.frame-tr2_i16(level->animations.data+back.control.animation*32+16);
                tr2_set_animation(level,&back,tr2_u16(rear+16)+offset,frame);
                render_begin_entry(render_hair()+1,&back,o->id+1);render_capture_model(&back,o->id+1,UINT64_MAX,0);
            }
        }
    }
    for(int i=0;i<TR2_MAX_DARTS;i++){const Tr2Object *d=world.hazards->darts+i;if(d->active)render_begin_entry(render_darts()+i,&d->pose,d->id);}
    for(int group=0;group<2;group++)for(int i=0;i<(group?TR2_MAX_EFFECTS:TR2_MAX_FRAGMENTS);i++){
        const Tr2Fragment *f=group?world.hazards->effects+i:world.fragments+i;if(!f->active)continue;
        Tr2Actor a={0};a.x=f->x;a.y=f->y;a.z=f->z;a.yaw=f->yaw;a.pitch=f->pitch;a.control.roll=f->roll;a.room=f->room;
        render_begin_entry((group?render_effects():render_fragments())+i,&a,f->object);
    }
    const uint8_t *hair=tr2_model(level,2);
    if(hair && hair_state.ready){
        render_begin_entry(render_hair(),&actor,2);
        for(int i=0;i<6;i++){
            glLoadIdentity();glTranslated(hair_state.point[i][0],hair_state.point[i][1],hair_state.point[i][2]);
            glRotated(hair_state.yaw[i]*(360.0/65536),0,1,0);glRotated(hair_state.pitch[i]*(360.0/65536),1,0,0);
            render_matrix(&render_entry->pose,i,i-1,tr2_u16(hair+6)+i);
        }
    }
    glPopMatrix();render_entry=NULL;render_recording=0;
    render_camera_previous=render_camera_current;render_camera_current=camera;
    int key=world.camera_active==1 || world.camera_active==5?world.camera_index:-1;
    int cut=render_tick==1 || render_flip!=world.flip_status;
    if(world.cinematic_active){
        cut|=render_shot_change;
    }else{
        cut|=render_cine_frame>=0 || render_entries[0].root.reset ||
            key!=render_camera_key || world.camera_target!=render_camera_target;
    }
    previous_fov=current_fov;previous_roll=current_roll;
    current_fov=world.cinematic_fov>0?world.cinematic_fov:14564;
    current_roll=world.cinematic_roll;
    if(cut){render_camera_previous=render_camera_current;previous_fov=current_fov;previous_roll=current_roll;}
    render_cine_frame=frame;
    render_camera_key=key;render_camera_target=world.camera_target;render_flip=world.flip_status;
}
/* Reset presentation history without advancing the simulation or emitting audio. */
void tr2_home_interpolation_reset(void){
    if(!level || !render_entries)return;
    memset(render_entries,0,render_count*sizeof *render_entries);render_tick=0;accumulator=0;
    render_capture();render_alpha=1;update_eye();
}
static int render_visible_entry(size_t index,int room,const uint8_t *visible){
    if(room>=0 && (unsigned)room<level->room_count && visible[room])return 1;
    if(render_entries && index<render_count){const TombRenderTrack *r=&render_entries[index].root;
        int old=r->previous_room;return r->tick==render_tick && old>=0 && (unsigned)old<level->room_count && visible[old];}
    return 0;
}
static void render_particle_position(size_t index,int id,int *x,int *y,int *z){
    float matrix[16];if(render_entries && tomb_render_sample(&render_entries[index].root,render_tick,0,id,render_alpha,matrix)){
        *x=(int)lroundf(matrix[12]);*y=(int)lroundf(matrix[13]);*z=(int)lroundf(matrix[14]);
    }
}
