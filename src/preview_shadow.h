/* DOS octagonal floor shadow with palette-row-24 destination darkening. */
static GLuint shadow_textures[2];
static const unsigned char *shadow_palette;
static int shadow_underwater=-1,shadow_width,shadow_height;
static void shadow_lookup(void){
    if(!shadow_textures[0])glGenTextures(2,shadow_textures);
    if(shadow_palette==visual->palette && shadow_underwater==water_eye)return;
    unsigned char *rgb=calloc(1024*256,3);if(!rgb)return;
    const unsigned char *map=level->file_data+level->item_parsed_bytes+24*256;
    for(int i=0;i<256;i++){
        unsigned key=0;int result[3];
        for(int c=0;c<3;c++){
            int source=visual->palette[i*3+c],dest=visual->palette[map[i]*3+c];
            if(water_eye && c<2){source=source*2/3;dest=dest*2/3;}
            key|=(unsigned)source<<(c*6);result[c]=dest*255/63;
        }
        for(int c=0;c<3;c++)rgb[key*3+c]=(unsigned char)result[c];
    }
    glBindTexture(GL_TEXTURE_2D,shadow_textures[1]);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,1024,256,0,GL_RGB,GL_UNSIGNED_BYTE,rgb);free(rgb);
    shadow_palette=visual->palette;shadow_underwater=water_eye;
}
static const TombLevel *shadow_floor_level;
static int32_t lara_shadow_floor;
static int lara_shadow_floor_valid;
static void shadow_floor_reset(void){lara_shadow_floor_valid=0;shadow_floor_level=NULL;}
/* DOS 42abe..42b0a rejects the underside by projected winding.
   The world has downward-positive Y; these vertices face upward with GL_CW. */
static void shadow_polygon(const int16_t points[8][3]){
    glEnable(GL_CULL_FACE);glCullFace(GL_BACK);glFrontFace(GL_CW);
    glBegin(GL_POLYGON);for(int i=0;i<8;i++)glVertex3s(points[i][0],0,points[i][2]);glEnd();
    glDisable(GL_CULL_FACE);
}
static void draw_shadow(const TombActor *actor,int object,int size,size_t track){
    int16_t bounds[6],points[8][3];TombSectorRef ref;TombHeights h;
    if(!tomb_visual_item_bounds(visual,object,actor,bounds))return;
    TombActor anchor=*actor;
    if(track<render_count){float sample[16];const TombRenderNode *root=render_tracks[track].nodes;
        if(tomb_render_sample(render_tracks+track,render_tick,0,root->identity,camera_alpha,sample)){
            anchor.x+=(int32_t)(sample[12]-root->world[12]);anchor.z+=(int32_t)(sample[14]-root->world[14]);
        }
    }
    if(!tomb_find_sector(level,anchor.x,anchor.y,anchor.z,anchor.room,&ref) ||
       !tomb_item_heights(level,ref,anchor.x,anchor.y,anchor.z,0,level->items,level->item_count,&h) || h.floor==-32512)return;
    if(track==0){
        if(shadow_floor_level!=level){shadow_floor_level=level;lara_shadow_floor_valid=0;}
        /* DOS DrawShadow uses the item's cached floor (10465), not a fresh
           ledge-floor query while the water-out animation hangs below it. */
        if(actor->current==55){
            if(lara_shadow_floor_valid)h.floor=lara_shadow_floor;
            if(h.floor<anchor.y+bounds[3])return;
        }else {lara_shadow_floor=h.floor;lara_shadow_floor_valid=1;}
    }
    tomb_shadow_shape(bounds,size,points);shadow_lookup();
    int pw=1,ph=1;while(pw<width)pw*=2;while(ph<height)ph*=2;
    glBindTexture(GL_TEXTURE_2D,shadow_textures[0]);
    if(pw!=shadow_width || ph!=shadow_height){glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,pw,ph,0,GL_RGB,GL_UNSIGNED_BYTE,NULL);shadow_width=pw;shadow_height=ph;}
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,width,height);
    light_UseProgram(shade_program);light_Uniform1i(shade_material,visual->rgba_tiles?-5:-3);light_Uniform1i(shade_warp,0);light_Uniform2f(shade_screen,(float)pw,(float)ph);
    light_ActiveTexture(0x84c1);glBindTexture(GL_TEXTURE_2D,shadow_textures[1]);light_ActiveTexture(0x84c0);
    glDisable(GL_ALPHA_TEST);glDisable(GL_CULL_FACE);glDepthMask(GL_FALSE);glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(-1,-1);
    glPushMatrix();glTranslatef((float)anchor.x,(float)h.floor,(float)anchor.z);glRotatef(actor->yaw*(360.f/65536.f),0,1,0);
    shadow_polygon(points);glPopMatrix();
    glDisable(GL_POLYGON_OFFSET_FILL);glDepthMask(GL_TRUE);light_UseProgram(0);
}
static void draw_shadows(void){
    if(!pose_mode && play.shooter.mode!=TOMB_MODE_VR)draw_shadow(&play.lara.actor,0,160,0);
    if(caves)for(size_t i=0;i<objects.count;i++){
        const TombObject *o=objects.items+i;
        if((o->actor.flags&6)==6 || (!tomb_enemy_object(o->object) && o->object!=43))continue;
        TombScreenRect r=room_rects[o->actor.room];if(r.left>=r.right || r.bottom>=r.top)continue;
        draw_shadow(&o->actor,o->object,o->object==43?256:o->object==10 || o->object==11?85:o->object==18?64:128,1+i);
    }
}
