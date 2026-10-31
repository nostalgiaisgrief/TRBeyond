/* Tracked rendering and menu bridge; the desktop ring is drawn onto an XR quad. */
static int draw(void);
static void vr_sound_listener(double listener[3],int *room){
    if(!vr.ready)return;tomb_vr_point(&vr,vr.input.head.p,listener);TombSectorRef ref;
    if(tomb_find_sector(level,(int)listener[0],(int)listener[1],(int)listener[2],*room,&ref))*room=ref.room;
}
static int vr_sound_yaw(void){
    if(!vr.ready)return play.lara.actor.yaw;
    double direction[3];tomb_vr_direction(&vr,&vr.input.head,direction);
    return (int)(atan2(direction[0],direction[2])*65536/(2*3.141592653589793));
}
static void vr_view(const TombVrPose *pose,double eye[3],double view[16]){
    double m[16];tomb_vr_matrix(&vr,pose,m);m[13]+=vr.view_offset;memcpy(eye,m+12,3*sizeof(double));
    memset(view,0,16*sizeof *view);view[15]=1;
    for(int i=0;i<3;i++)for(int j=0;j<3;j++)view[4*i+j]=m[4*j+i];
    for(int j=0;j<3;j++)view[12+j]=-(view[j]*eye[0]+view[4+j]*eye[1]+view[8+j]*eye[2]);
}
static void vr_gun_flash(const TombMeshView *gun,int weapon){
    double rear[3],muzzle[3];if(!tomb_vr_barrel(gun,weapon,rear,muzzle))return;
    TombModel flash;if(!tomb_visual_model(visual,166,&flash) || !flash.mesh_count)return;
    const TombMeshView *effect=visual->meshes+flash.mesh_start;
    double f[3],r[3],u[3],matrix[16]={0};for(int j=0;j<3;j++)f[j]=muzzle[j]-rear[j];
    double n=sqrt(f[0]*f[0]+f[1]*f[1]+f[2]*f[2]);if(n<1)return;for(int j=0;j<3;j++)f[j]/=n;
    r[0]=f[1];r[1]=-f[0];r[2]=0;n=hypot(r[0],r[1]);if(n<.01)return;for(int j=0;j<3;j++)r[j]/=n;
    u[0]=r[1]*f[2];u[1]=-r[0]*f[2];u[2]=r[0]*f[1]-r[1]*f[0];
    /* Flash +Z is its barrel axis. Place its rear-most vertex beyond the tip,
       rather than centring a long flash mesh halfway inside the weapon. */
    int back=32767;for(size_t i=0;i<effect->vertex_count;i++){
        const unsigned char *p=effect->vertices+i*effect->vertex_stride+4;int z=(int16_t)(p[0]|p[1]<<8);if(z<back)back=z;
    }
    for(int j=0;j<3;j++){matrix[j]=r[j];matrix[4+j]=-u[j];matrix[8+j]=f[j];matrix[12+j]=muzzle[j]+f[j]*(4-back);}matrix[15]=1;
    TombLighting saved=model_light;tomb_light_static(0x1400,lighting_depth(),&model_light);
    glPushMatrix();glMultMatrixd(matrix);glRotatef((float)fmod(elapsed*13871,360),0,0,1);mesh(effect);glPopMatrix();model_light=saved;
}
static void vr_wrist_bar(void *context,int x1,int y1,int x2,int y2,int palette){
    int offset=*(int *)context;colour((unsigned)palette,1);
    glBegin(GL_QUADS);glVertex2i(x1,y1+offset);glVertex2i(x2+1,y1+offset);
    glVertex2i(x2+1,y2+1+offset);glVertex2i(x1,y2+1+offset);glEnd();
}
static void vr_wrist_draw(int h,const double hand_matrix[16]){
    if(xr.render_eye<0 || play.lara.health<=0 || !vr.input.hand[h].valid || !vr.wrist_alpha[h])return;
    int weapon=vr.hands[h].weapon;if(h && !weapon)return;
    double matrix[16];tomb_vr_wrist_model_matrix(hand_matrix,h,matrix);
    glPushAttrib(GL_ENABLE_BIT|GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_CURRENT_BIT);
    light_UseProgram(0);glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);glDepthMask(GL_FALSE);
    glPushMatrix();glMultMatrixd(matrix);glScaled(.32,.32,.32);glTranslated(-58,-24,0);
    if(!h){
        int offset=0;tomb_hud_bar(play.lara.health/10,0,118,vr_wrist_bar,&offset);
        offset=16;tomb_hud_bar(play.lara.air*100/1800,1,118,vr_wrist_bar,&offset);
    }
    if(weapon){
        char text[64];int slot=weapon==4?1:weapon;
        if(weapon==1 || (play.gameplay_options&TOMB_INFINITE_AMMO))strcpy(text,"--");
        else tomb_ui_ammo(play.inventory.ammo[slot],slot,text);
        TombTextStyle style={0,h?24:76,131072,131072,1,6,TOMB_TEXT_CENTRE};
        glEnable(GL_TEXTURE_2D);glEnable(GL_ALPHA_TEST);glColor3f(1,1,1);
        const char *amount=text;while(*amount==' ')amount++;
        tomb_text_layout(amount,&style,118,80,font_glyph,NULL);
    }
    glPopMatrix();glPopAttrib();
}
static void vr_hands_draw(void){
    if(play.shooter.mode!=TOMB_MODE_VR || !vr.ready || render_recording)return;
    double wrist_models[2][16];int wrist_valid[2]={0};
    uint16_t hands[15];if(!tomb_visual_lara_meshes(visual,!caves,hands))return;
    for(int h=0;h<2;h++){
        TombVrHand *hand=vr.hands+h;if(!vr.input.hand[h].valid || !vr.input.aim[h].valid)continue;
        int joint=h?10:13,id=hands[joint];TombModel model;
        if(hand->weapon && tomb_visual_model(visual,hand->weapon==4?2:hand->weapon==2?3:hand->weapon==3?4:1,&model))id=(int)model.mesh_start+joint;
        if(id<0 || (size_t)id>=visual->mesh_count)continue;
        double matrix[16];tomb_vr_weapon_matrix(&vr,h,visual->meshes+id,matrix);matrix[13]+=vr.view_offset;render_track=SIZE_MAX;
        glPushMatrix();glMultMatrixd(matrix);
        TombActor actor=play.lara.actor;actor.x=(int32_t)matrix[12];actor.y=(int32_t)matrix[13];actor.z=(int32_t)matrix[14];TombSectorRef ref;
        if(tomb_find_sector(level,actor.x,actor.y,actor.z,actor.room,&ref))actor.room=(int16_t)ref.room;
        TombPose lighting_pose={0};actor_lighting(&actor,&lighting_pose,-1,0,0);
        if(hand->hover>=0)model_light.adder=model_light.adder>1024?model_light.adder-1024:0;
        mesh(visual->meshes+id);
        if(hand->flash>0)vr_gun_flash(visual->meshes+id,hand->weapon);glPopMatrix();
        if(hand->key){TombModel key;if(tomb_visual_model(visual,hand->key,&key) && key.mesh_count){
            double grip[16];tomb_vr_matrix(&vr,&vr.input.hand[h],grip);grip[13]+=vr.view_offset;
            const TombMeshView *item=visual->meshes+key.mesh_start;
            int lo[3]={32767,32767,32767},hi[3]={-32768,-32768,-32768};
            for(unsigned n=0;n<item->vertex_count;n++)for(int j=0;j<3;j++){
                const unsigned char *b=item->vertices+n*item->vertex_stride+j*2;int value=(int16_t)(b[0]|b[1]<<8);
                if(value<lo[j])lo[j]=value;if(value>hi[j])hi[j]=value;
            }
            /* Measure the rendered hand in grip space: authored item origins
               vary, so align its bottom to the hand rather than a fixed offset. */
            double hand_lo[3]={1e9,1e9,1e9},hand_hi[3]={-1e9,-1e9,-1e9};
            const TombMeshView *palm=visual->meshes+id;
            for(unsigned n=0;n<palm->vertex_count;n++){
                double vertex[3],world[3];for(int j=0;j<3;j++){const unsigned char *b=palm->vertices+n*palm->vertex_stride+j*2;vertex[j]=(int16_t)(b[0]|b[1]<<8);}
                for(int j=0;j<3;j++)world[j]=matrix[12+j]-grip[12+j]+matrix[j]*vertex[0]+matrix[4+j]*vertex[1]+matrix[8+j]*vertex[2];
                for(int j=0;j<3;j++){double a=grip[j*4]*world[0]+grip[j*4+1]*world[1]+grip[j*4+2]*world[2];hand_lo[j]=fmin(hand_lo[j],a);hand_hi[j]=fmax(hand_hi[j],a);}
            }
            glPushMatrix();glMultMatrixd(grip);glTranslated((hand_lo[0]+hand_hi[0])*.5,hand_lo[1],(hand_lo[2]+hand_hi[2])*.5);glScaled(.5,.5,.5);
            glRotated(180,1,0,0);
            glTranslated(-(lo[0]+hi[0])*.5,-hi[1],-(lo[2]+hi[2])*.5);mesh(item);glPopMatrix();
        }}
        memcpy(wrist_models[h],matrix,sizeof matrix);wrist_valid[h]=1;
    }
    for(int h=0;h<2;h++)if(wrist_valid[h])vr_wrist_draw(h,wrist_models[h]);
    if(vr.block.item>=0 && vr.block.hand>=0 && (size_t)vr.block.item<objects.count){
        const TombActor *block=&objects.items[vr.block.item].actor;double hand[3];tomb_vr_point(&vr,vr.input.hand[vr.block.hand].p,hand);
        double end[3]={block->x,block->y-128,block->z};end[vr.block.axis]+=vr.block.sign*514;
        double d[3],r[3],u[3];for(int j=0;j<3;j++)d[j]=hand[j]-end[j];
        double n=sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);if(n>1){
            for(int j=0;j<3;j++)d[j]/=n;double flat=hypot(d[0],d[2]);
            if(flat<.01){r[0]=1;r[1]=r[2]=0;}else{r[0]=d[2]/flat;r[1]=0;r[2]=-d[0]/flat;}
            u[0]=d[1]*r[2]-d[2]*r[1];u[1]=d[2]*r[0]-d[0]*r[2];u[2]=d[0]*r[1]-d[1]*r[0];
            light_UseProgram(0);glDisable(GL_TEXTURE_2D);glColor3f(.30f,.16f,.06f);
            double progress=(vr.feet[vr.block.axis]-vr.block.rope_origin[vr.block.axis])*vr.block.sign/1024;
            double sag=fmin(250,n*.25)*(1-fmax(0,fmin(1,progress)));
            for(int segment=0;segment<16;segment++){
                double t0=segment/16.0,t1=(segment+1)/16.0;
                glBegin(GL_QUAD_STRIP);for(int i=0;i<=8;i++){double angle=i*6.283185307179586/8;
                    double offset[3],a[3],b[3];for(int j=0;j<3;j++){
                        offset[j]=6*(cos(angle)*r[j]+sin(angle)*u[j]);
                        a[j]=end[j]+(hand[j]-end[j])*t0+offset[j];b[j]=end[j]+(hand[j]-end[j])*t1+offset[j];
                    }
                    a[1]+=sag*4*t0*(1-t0);b[1]+=sag*4*t1*(1-t1);glVertex3dv(a);glVertex3dv(b);
                }glEnd();
            }glEnable(GL_TEXTURE_2D);
        }
    }
}
static void vr_ring_release(const TombVrInput *input){
    if(!vr.menu_open)return;
    if(!inventory_open || title_mode || death_menu || objects.progress->complete){vr.menu_open=0;return;}
    if(inventory_ring.chosen>=0)return;
    if(!input->head.valid || !input->hand[0].valid || input->grip[0]<.35){
        inventory_ring.sound=112;inventory_sound();
        inventory_open=inventory_stats=inventory_input=menu_action=binding_wait=0;
        play.quest_request=0;vr.menu_open=0;xr.panel_open=0;
    }
}
static void vr_menu_input(const TombVrInput *input,double dt){
    static int primary,secondary,direction,key_grip;static double repeat;
    int grab=input->grip[1]>.6;
    if(grab && !key_grip && inventory_open && !title_mode && inventory_ring.current>=0 && inventory_ring.current<inventory_ring.count){
        int id=inventory_ring.items[inventory_ring.current].object;
        if(tomb_quest_slot(id)>=0){vr.selected_key=id;inventory_open=inventory_stats=inventory_input=0;vr.menu_open=0;play.inventory.chosen=-1;}
    }key_grip=grab;
    int a=input->primary[0] || input->primary[1],b=input->secondary[0] || input->secondary[1];
    if(b && !secondary){if(inventory_open)inventory_input|=TOMB_RING_BACK;else if(!title_mode && input->grip[0]>.6){inventory_begin();}}
    if(a && !primary){if(inventory_open){
        if(!title_mode && !death_menu && play.shooter.mode==TOMB_MODE_VR)inventory_tick(TOMB_RING_SELECT);
        else inventory_input|=TOMB_RING_SELECT;
    }else if(objects.progress->complete)load_next_level();}
    primary=a;secondary=b;
    int next=0;float x=input->stick[0][0],y=input->stick[0][1];
    if(fabs(x)>.6 || fabs(y)>.6)next=fabs(x)>fabs(y)?(x>0?TOMB_RING_RIGHT:TOMB_RING_LEFT):(y>0?16:32);
    repeat-=dt;
    if(inventory_open && next && (next!=direction || repeat<=0)){inventory_input|=(unsigned)next;repeat=next!=direction?.35:.15;}
    direction=next;
}
static void vr_panel_place(void){
    static int previous_kind;
    int kind=media_busy?5:title_mode?1:objects.progress->complete?2:vr_hand_ring()?3:4;
    if(kind!=previous_kind){xr.panel_open=0;previous_kind=kind;}

    if(vr_hand_ring()){
        const TombVrPose *hand=&vr.input.hand[0],*head=&vr.input.head;
        xr.panel=(XrPosef){{(float)head->q[0],(float)head->q[1],(float)head->q[2],(float)head->q[3]},
            {(float)hand->p[0],(float)(hand->p[1]+.16-xr.floor_offset),(float)hand->p[2]}};
        xr.panel_open=1;return;
    }
    if(xr.panel_open)return;
    TombVrPose head=xr_locate(xr.head);if(!head.valid)return;
    double x=head.q[0],y=head.q[1],z=head.q[2],w=head.q[3];
    double yaw=atan2(2*(x*z+w*y),1-2*(x*x+y*y));
    xr.panel=(XrPosef){{0,(float)sin(yaw/2),0,(float)cos(yaw/2)},{(float)(head.p[0]-sin(yaw)*1.8),(float)(head.p[1]-xr.floor_offset),(float)(head.p[2]-cos(yaw)*1.8)}};
    xr.panel_open=1;
}
static void vr_media_present(void){
    if(!xr.begun)return;
    if(!xr.valid || !xr.frame.shouldRender){xr_end(0,0);return;}
    static GLuint texture;static int w,h;
    if(!texture)glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    if(w!=width || h!=height){w=width;h=height;glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,NULL);}
    glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,w,h);vr_panel_place();int panel=0;
    if(xr_target_begin(2)){
        XrTarget *t=xr.target+2;glViewport(0,0,t->width,t->height);light_UseProgram(0);glDisable(GL_DEPTH_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_ALPHA_TEST);
        glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,1,0,1,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture);glColor3f(1,1,1);
        glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(0,0);glTexCoord2f(1,0);glVertex2f(1,0);glTexCoord2f(1,1);glVertex2f(1,1);glTexCoord2f(0,1);glVertex2f(0,1);glEnd();panel=1;
    }
    xr_target_end(2);xr_end(0,panel);glViewport(0,0,width,height);
}
static GLuint vr_mirror_eye,vr_mirror_panel;
static void vr_mirror_copy(GLuint *texture,int w,int h){
    if(!*texture)glGenTextures(1,texture);glBindTexture(GL_TEXTURE_2D,*texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812f);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812f);
    glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,0,0,w,h,0);
}
static void vr_mirror_present(int world,int panel){
    if(!world && !panel)return;
    light_UseProgram(0);glDisable(GL_DEPTH_TEST);glDisable(GL_ALPHA_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);
    glViewport(0,0,width,height);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    double aspect=world?(double)xr.target[0].width/xr.target[0].height:4.0/3;
    int vw=width,vh=(int)(width/aspect);if(vh>height){vh=height;vw=(int)(height*aspect);}glViewport((width-vw)/2,(height-vh)/2,vw,vh);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,1,0,1,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,world?vr_mirror_eye:vr_mirror_panel);glColor3f(1,1,1);
    glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(0,0);glTexCoord2f(1,0);glVertex2f(1,0);glTexCoord2f(1,1);glVertex2f(1,1);glTexCoord2f(0,1);glVertex2f(0,1);glEnd();
    if(world && panel){
        const XrFovf *f=&xr.views[0].fov;glMatrixMode(GL_PROJECTION);glLoadIdentity();glFrustum(tan(f->angleLeft)*10,tan(f->angleRight)*10,tan(f->angleDown)*10,tan(f->angleUp)*10,10,world_far_distance);
        double eye[3],view[16],model[16];TombVrPose pose=xr_pose(xr.views[0].pose,1);vr_view(&pose,eye,view);
        pose=xr_pose(xr.panel,1);tomb_vr_matrix(&vr,&pose,model);model[13]+=vr.view_offset;
        glMatrixMode(GL_MODELVIEW);glLoadMatrixd(view);glMultMatrixd(model);
        glBindTexture(GL_TEXTURE_2D,vr_mirror_panel);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        double x=(vr_hand_ring()?.55:1.6)*TOMB_VR_SCALE/2,y=(vr_hand_ring()?.4125:1.2)*TOMB_VR_SCALE/2;
        glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex3d(-x,-y,0);glTexCoord2f(1,0);glVertex3d(x,-y,0);glTexCoord2f(1,1);glVertex3d(x,y,0);glTexCoord2f(0,1);glVertex3d(-x,y,0);glEnd();
    }
    glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);glViewport(0,0,width,height);
}
static int vr_render(void){
    if(!xr.begun)return 1;
    if(!xr.valid || !xr.frame.shouldRender || (play.shooter.mode==TOMB_MODE_VR && !vr.ready)){xr_end(0,0);return 1;}
    int full_width=width,full_height=height,world=0,panel=0,ok=1;
    if(!title_mode && play.shooter.mode==TOMB_MODE_VR){
        world=1;
        for(int i=0;i<2;i++){
            XrTarget *t=xr.target+i;
            if(!xr_target_begin(i)){xr_target_end(i);world=0;ok=0;break;}
            xr.render_eye=i;width=t->width;height=t->height;ok=draw();if(i==0 && ok)vr_mirror_copy(&vr_mirror_eye,width,height);xr_target_end(i);
            xr.projection[i]=(XrCompositionLayerProjectionView){.type=XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,.pose=xr.views[i].pose,.fov=xr.views[i].fov,.subImage={t->handle,{{0,0},{t->width,t->height}},0}};
            if(!ok){world=0;break;}
        }
    }
    xr.render_eye=-1;
    if(inventory_open || title_mode || paused || objects.progress->complete){
        vr_panel_place();
        XrTarget *t=xr.target+2;
        if(xr_target_begin(2)){width=t->width;height=t->height;glViewport(0,0,width,height);glClearColor(0,0,0,vr_hand_ring()?0:1);xr.panel_drawing=1;if(!draw_frame_composite(2))ok=0;xr.panel_drawing=0;glClearColor(0,0,0,1);panel=1;vr_mirror_copy(&vr_mirror_panel,width,height);}
        else ok=0;
        xr_target_end(2);
    }else xr.panel_open=0;
    width=full_width;height=full_height;vr_mirror_present(world,panel);xr_end(world,panel);glViewport(0,0,width,height);return ok;
}
