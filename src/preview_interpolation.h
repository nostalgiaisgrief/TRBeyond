/* Read-only renderer snapshots, captured once per simulation tick in world
   space. Keeping the joint hierarchy preserves bone lengths between ticks. */
static int lara(void);
static int draw_object(const TombObject *);
static TombRenderTrack *render_tracks;
static size_t render_count,render_track=SIZE_MAX;
static uint64_t render_tick;
static size_t render_secondary=SIZE_MAX;
static int render_recording,render_node,render_parent;
static void shadow_floor_reset(void);
static void render_reset(void){shadow_floor_reset();free(render_tracks);render_tracks=NULL;render_count=0;render_tick=0;render_track=SIZE_MAX;tomb_shooter_anchor(&play,shooter_current);memcpy(shooter_previous,shooter_current,sizeof shooter_current);}
static void render_select(size_t track,const TombActor *actor,int snap_yaw){
    render_track=cinema_mode?SIZE_MAX:track;render_node=0;render_parent=-1;
    if(render_recording && track<render_count){
        float origin[3]={(float)actor->x,(float)actor->y,(float)actor->z};
        TombRenderTrack *r=render_tracks+track;
        tomb_render_begin(r,render_tick,origin,actor->room);
        /* Push blocks snap to the approach quadrant before grab bounds are
           checked in DOS. Blending that alignment spins the cube through walls. */
        if(snap_yaw && r->yaw!=actor->yaw)r->reset=1;
        r->yaw=actor->yaw;
    }
}
static size_t render_object_key(const TombObject *o){
    if(o->object==29 || o->object==45)return render_secondary;
    for(size_t i=0;i<objects.count;i++)if(o==objects.items+i)return 1+i;
    if(objects.hazards)for(size_t i=0;i<TOMB_DART_CAPACITY;i++)if(o==&objects.hazards->darts[i].item)return 1+objects.count+i;
    return SIZE_MAX;
}
static int render_visible(size_t key,int room){
    TombScreenRect clip=room_rects[room];if(clip.left<clip.right && clip.bottom<clip.top)return 1;
    if(key<render_count && render_tracks[key].tick==render_tick){
        int previous=render_tracks[key].previous_room;
        if(previous>=0 && (size_t)previous<level->room_count){clip=room_rects[previous];return clip.left<clip.right && clip.bottom<clip.top;}
    }
    return 0;
}
/* A hinged door can extend out of its owning room. Keep it eligible from
   either side of its doorway; depth testing still hides it behind walls. */
static int render_object_visible(size_t index){
    const TombObject *o=objects.items+index;
    if(render_visible(1+index,o->actor.room))return 1;
    if(o->object>=57 && o->object<=64){
        const TombDoor *d=objects.doors+index;
        for(unsigned j=0;j<d->count;j++){
            int room=d->parts[j].ref.room;
            if(room>=0 && (size_t)room<level->room_count){
                TombScreenRect r=room_rects[room];
                if(r.left<r.right && r.bottom<r.top)return 1;
            }
        }
    }
    if(o->object==65 || o->object==66){
        /* Trapdoor items retain the upper room while their animated panels
           swing below its floor. Query rooms occupied by the panel bounds,
           not just the hinge's room. This only widens draw eligibility;
           normal depth testing still occludes the panel behind solid walls. */
        int16_t bounds[6];
        if(tomb_visual_item_bounds(visual,o->object,&o->actor,bounds)){
            for(int x=0;x<3;x++)for(int y=0;y<3;y++)for(int z=0;z<3;z++){
                int32_t local[3]={bounds[0]+(bounds[1]-bounds[0])*x/2,
                    bounds[2]+(bounds[3]-bounds[2])*y/2,
                    bounds[4]+(bounds[5]-bounds[4])*z/2},world[3];
                tomb_rotate_vector(o->actor.yaw,0,0,local,0,sine_table,world);
                TombSectorRef ref;
                if(tomb_find_sector(level,o->actor.x+world[0],o->actor.y+world[1],
                    o->actor.z+world[2],o->actor.room,&ref)){
                    TombScreenRect r=room_rects[ref.room];
                    if(r.left<r.right && r.bottom<r.top)return 1;
                }
            }
        }
    }
    return 0;
}
static int render_capture(void){
    size_t count=1+2*objects.count+TOMB_DART_CAPACITY+256;
    if(render_count!=count){render_reset();render_tracks=calloc(count,sizeof *render_tracks);if(!render_tracks)return 0;render_count=count;}
    render_tick++;render_recording=1;
    TombLighting saved_light=model_light;GLdouble saved_view[16];memcpy(saved_view,lighting_view,sizeof saved_view);
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();glGetDoublev(GL_MODELVIEW_MATRIX,lighting_view);
    int ok=lara();
    if(caves)for(size_t i=0;i<objects.count;i++){
        const TombObject *o=objects.items+i;
        if(tomb_object_supported(o->object) && !tomb_pickup_object(o->object) && (o->actor.flags&6)!=6)ok=draw_object(o) && ok;
    }
    if(caves && objects.hazards){
        for(size_t i=0;i<TOMB_DART_CAPACITY;i++){
            const TombObject *o=&objects.hazards->darts[i].item;if(o->active)ok=draw_object(o) && ok;
        }
        for(size_t i=0;i<256;i++){
            const TombHazardEffect *e=objects.hazards->effects+i;if(!e->active)continue;
            float m[16]={1,0,0,0,0,1,0,0,0,0,1,0,(float)e->x,(float)e->y,(float)e->z,1};
            TombRenderTrack *r=render_tracks+1+objects.count+TOMB_DART_CAPACITY+i;
            tomb_render_begin(r,render_tick,m+12,e->room);tomb_render_record(r,render_tick,0,-1,e->id,m);
        }
    }
    glPopMatrix();model_light=saved_light;memcpy(lighting_view,saved_view,sizeof saved_view);
    render_track=SIZE_MAX;render_recording=0;return ok;
}
