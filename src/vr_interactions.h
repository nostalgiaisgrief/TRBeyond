/* Physical VR interactions. Included after the shared tracking/collision helpers. */
static void swim_stroke(TombVr *v,int h,double dt){
    /* OpenXR grip +X points through the left palm, -X through the right.
       Only the broad, palm-first stroke generates thrust. Recovery is edge-on. */
    if(v->input.grip[h]>.35)return;
    double axis[3]={h?-1:1,0,0},normal[3],world[3];
    rotate(v->input.hand[h].q,axis,normal);world_vector(v,normal,world);
    const double *velocity=v->hand_velocity[h];double speed=length(velocity);
    if(speed<.25 || speed>5)return;
    double palm=(world[0]*velocity[0]+world[1]*velocity[1]+world[2]*velocity[2])/speed;
    if(palm<=.25)return;
    double gain=(palm-.25)/.75*(speed-.25)/speed;
    for(int j=0;j<3;j++)v->swim_velocity[j]-=velocity[j]*gain*TOMB_VR_SCALE*3*dt;
}
static int shotgun_support(TombVr *v,int h,int pressed){
    TombVrHand *hand=&v->hands[h],*gun=&v->hands[1-h];
    if(gun->weapon!=4 || !v->input.hand[1-h].valid || v->input.grip[1-h]<.35){hand->support=0;return 0;}
    double point[3],base[3],axis[3],d[3];
    tomb_vr_point(v,v->input.hand[h].p,point);tomb_vr_point(v,v->input.hand[1-h].p,base);
    tomb_vr_direction(v,&v->input.aim[1-h],axis);
    for(int j=0;j<3;j++)d[j]=point[j]-base[j];
    double along=d[0]*axis[0]+d[1]*axis[1]+d[2]*axis[2];
    double side=sqrt(fmax(0,length(d)*length(d)-along*along));
    if(pressed && !hand->weapon && hand->pickup<0 && !hand->anchored && along>60 && along<350 && side<80){hand->support=1;hand->pump_origin=along;hand->pump_ready=1;}
    if(!hand->support)return 0;
    if(length(d)>450 || along<20){hand->support=0;return 0;}
    if(along>=hand->pump_origin-5)hand->pump_ready=1;
    if(gun->pump && hand->pump_ready && along<hand->pump_origin-40){gun->pump=0;hand->pump_ready=0;vr_sound(v,9);}
    return 1;
}
static int block_hand(TombVr *v,TombPlaytest *p,int h,int *axis,int *sign){
    if(!p->objects || v->hands[h].weapon || v->hands[h].pickup>=0 || v->hands[h].anchored || v->hands[h].support)return -1;
    double point[3],head[3];tomb_vr_point(v,v->input.hand[h].p,point);tomb_vr_point(v,v->input.head.p,head);
    for(size_t i=0;i<p->objects->count;i++){
        TombObject *o=&p->objects->items[i];if(o->object<48 || o->object>51 || o->active || (o->actor.flags&6))continue;
        double dx=point[0]-o->actor.x,dz=point[2]-o->actor.z;
        if(point[1]<o->actor.y-1024 || point[1]>o->actor.y-40)continue;
        int a=fabs(dx)>fabs(dz)?0:2;double face=a?dz:dx,side=a?dx:dz;
        if(fabs(fabs(face)-512)>85 || fabs(side)>480)continue;
        int s=face<0?-1:1;double approach=(a?head[2]-o->actor.z:head[0]-o->actor.x)*s;
        if(approach<512)continue;
        /* Ray ends just outside the solid block face. */
        double contact[3]={point[0],point[1],point[2]};contact[a]=(a?o->actor.z:o->actor.x)+s*600;
        if(!reachable(p,head,contact))continue;
        *axis=a;*sign=s;return (int)i;
    }
    return -1;
}
static int block_start(TombVr *v,TombPlaytest *p,int id,int axis,int sign){
    TombObjects *w=p->objects;TombObject *o=&w->items[id];
    int quadrant=axis==0?(sign>0?1:3):(sign>0?0:2);
    if(!tomb_block_can_move(w,&p->lara.actor,(size_t)id,quadrant,0))return 0;
    double dx=v->feet[0]-(o->actor.x+(axis==0?sign*1024:0));
    double dz=v->feet[2]-(o->actor.z+(axis==2?sign*1024:0));
    if(fabs(dx)<630 && fabs(dz)<630)return 0;
    if(!tomb_block_floor(w,(size_t)id,1024))return 0;
    v->block.item=id;v->block.axis=axis;v->block.sign=sign;v->block.moving=1;v->block.elapsed=0;
    v->block.start[0]=o->actor.x;v->block.start[1]=o->actor.y;v->block.start[2]=o->actor.z;
    vr_sound(v,63);return 1;
}
static void block_gesture(TombVr *v,TombPlaytest *p){
    if(v->block.moving){if(v->block.hand>=0 && (!v->input.hand[v->block.hand].valid || v->input.grip[v->block.hand]<.35))v->block.hand=-1;return;}
    if(v->block.item>=0){
        int h=v->block.hand;int x=(int)floor(v->feet[0]/1024),z=(int)floor(v->feet[2]/1024);
        int ax=v->block.axis,s=v->block.sign;
        const TombActor *block=&p->objects->items[v->block.item].actor;
        int cross=ax==0?z:x,block_cross=(int)floor((ax==0?block->z:block->x)/1024.0);
        if(!v->input.hand[h].valid || v->input.grip[h]<.35 ||
           cross!=block_cross || (v->feet[ax]-(ax==0?block->x:block->z))*s<512){v->block.item=-1;return;}
        if((v->feet[ax]-v->block.rope_origin[ax])*s>1024)block_start(v,p,v->block.item,ax,s);
        return;
    }
    int axis[2],sign[2],id[2];for(int h=0;h<2;h++)id[h]=v->input.hand[h].valid && !v->hands[h].weapon && !v->hands[h].key && v->hands[h].lever<0 && !(h==1 && v->selected_key>=0)?block_hand(v,p,h,&axis[h],&sign[h]):-1;
    if(id[0]<0 || id[1]<0)v->block.push_latch=0;
    if(id[0]>=0 && id[0]==id[1] && axis[0]==axis[1] && sign[0]==sign[1] && !v->block.push_latch && !v->hands[0].gripped && !v->hands[1].gripped){
        int a=axis[0],s=sign[0];
        if(v->hand_velocity[0][a]*s<-.45 && v->hand_velocity[1][a]*s<-.45){
            v->block.push_latch=1;if(block_start(v,p,id[0],a,-s))v->block.hand=-1;
        }
    }
    for(int h=0;h<2 && v->block.item<0;h++)if(id[h]>=0 && v->input.grip[h]>.6 && !v->hands[h].gripped){
        v->block.item=id[h];v->block.hand=h;v->block.axis=axis[h];v->block.sign=sign[h];
        v->block.start_x=(int)floor(v->feet[0]/1024);v->block.start_z=(int)floor(v->feet[2]/1024);
        const TombActor *block=&p->objects->items[id[h]].actor;
        v->block.rope_origin[0]=block->x;v->block.rope_origin[1]=block->y;v->block.rope_origin[2]=block->z;
        v->block.rope_origin[axis[h]]+=sign[h]*640;
    }
}
static int block_tick(TombVr *v,TombPlaytest *p){
    if(!v->block.moving || !p->objects)return 1;
    TombObjects *w=p->objects;size_t id=(size_t)v->block.item;TombActor *a=&w->items[id].actor;
    double t=fmin(1,(v->block.elapsed+1.0/30)/.65),ease=1-(1-t)*(1-t);
    int axis=v->block.axis;
    int32_t x=(int32_t)llround(v->block.start[0]+(axis==0?v->block.sign*1024*ease:0));
    int32_t z=(int32_t)llround(v->block.start[2]+(axis==2?v->block.sign*1024*ease:0));
    /* Wait if the player steps into its path; never sweep the block through them. */
    if(fabs(v->feet[0]-x)<610 && fabs(v->feet[2]-z)<610 && v->feet[1]>a->y-1024)return 1;
    v->block.rope_origin[0]+=x-a->x;v->block.rope_origin[2]+=z-a->z;
    v->block.elapsed+=1.0/30;a->x=x;a->z=z;
    TombSectorRef r;if(tomb_find_sector(p->level,a->x,a->y-1,a->z,a->room,&r))a->room=(int16_t)r.room;
    TombItem *item=w->level->items+id;item->x=a->x;item->y=a->y;item->z=a->z;item->room=a->room;
    tomb_enemies_room_sync(w);
    if(t<1)return 1;
    if(!tomb_block_floor(w,id,-1024))return 0;
    TombHeights surface;
    v->block.moving=0;
    if(v->block.hand<0)v->block.item=-1;
    vr_sound(v,70);
    if(tomb_find_sector(p->level,a->x,a->y,a->z,a->room,&r) && tomb_item_heights(p->level,r,a->x,a->y,a->z,0,p->level->items,p->level->item_count,&surface))return tomb_objects_trigger_heavy(w,surface.trigger_index);
    return 0;
}

/* Physical handles keep gameplay activation separate from intermediate poses. */
static void vr_item_trigger(TombPlaytest *p,int item,int type){
    size_t seen[128];int count=0;
    for(size_t r=0;r<p->level->room_count;r++){
        TombRoom *room=p->level->rooms+r;
        for(int x=0;x<room->nx;x++)for(int z=0;z<room->nz;z++){
            TombSectorRef ref;TombHeights h;int wx=room->x+x*1024+512,wz=room->z+z*1024+512;
            if(!tomb_find_sector(p->level,wx,p->objects->items[item].actor.y,wz,(int)r,&ref) || !tomb_item_heights(p->level,ref,wx,p->objects->items[item].actor.y,wz,0,p->level->items,p->level->item_count,&h))continue;
            size_t at=h.trigger_index;if(!at || at+2>=p->level->floor_count)continue;
            if((p->level->floor_data[at]&255)==5)++at;
            if(at+2>=p->level->floor_count || (p->level->floor_data[at]&255)!=4 || ((p->level->floor_data[at]>>8)&63)!=(unsigned)type || (p->level->floor_data[at+2]&1023)!=item)continue;
            int duplicate=0;for(int i=0;i<count;i++)if(seen[i]==at)duplicate=1;if(duplicate)continue;
            if(count<128)seen[count++]=at;
            if(count==1)tomb_objects_trigger_keys(p->objects,at,1,0);else tomb_objects_trigger_activated(p->objects,at);
        }
    }
}
static void vr_lever_event(void *user,int kind,int16_t value,TombActor *actor){
    (void)actor;if(kind==5)((TombVrHand *)user)->lever_sound=value&0x3fff;
}
static int vr_lever_begin(TombVrHand *hand,TombPlaytest *p,int id,double y){
    TombObject copy=p->objects->items[id];if(copy.actor.flags&6)return 0;
    hand->lever_frames[0]=copy.actor;hand->lever_count=1;copy.actor.goal=copy.actor.current?0:1;
    copy.active=1;copy.actor.flags=(copy.actor.flags&~6u)|3;
    hand->lever_sound=-1;TombAnimContext silent=p->animation;silent.event=vr_lever_event;silent.user=hand;
    for(int i=1;i<128;i++){
        if(!tomb_object_animate(&copy,&silent))return 0;
        hand->lever_frames[i]=copy.actor;hand->lever_count=i+1;
        if((copy.actor.flags&6)==4)break;
    }
    if((copy.actor.flags&6)!=4)return 0;
    /* Follow the moving handle rather than animation time. The original clip
       includes held poses at both ends and its travel is not a fixed 40 cm. */
    TombModel model;if(!tomb_visual_model(p->visual,copy.object,&model))return 0;
    double best=0;int joint=-1,travel_end=hand->lever_count-1;int32_t handle[3]={0};
    for(unsigned m=0;m<model.mesh_count;m++){
        const TombMeshView *mesh=p->visual->meshes+model.mesh_start+m;
        for(unsigned n=0;n<mesh->vertex_count;n++){
            int32_t local[3],first[3],last[3];
            for(int j=0;j<3;j++){const unsigned char *b=mesh->vertices+n*mesh->vertex_stride+j*2;local[j]=(int16_t)(b[0]|b[1]<<8);}
            memcpy(first,local,sizeof first);
            if(!tomb_object_joint(p->visual,copy.object,hand->lever_frames,0,0,0,m,first,p->animation.sine_quarter))continue;
            for(int k=1;k<hand->lever_count;k++){memcpy(last,local,sizeof last);
            if(!tomb_object_joint(p->visual,copy.object,hand->lever_frames+k,0,0,0,m,last,p->animation.sine_quarter))continue;
            double travel=abs(last[1]-first[1]);if(travel>best){best=travel;joint=m;travel_end=k;memcpy(handle,local,sizeof handle);}}
        }
    }
    if(joint<0 || best<10)return 0;
    for(int i=0;i<hand->lever_count;i++){
        int32_t point[3];memcpy(point,handle,sizeof point);
        if(!tomb_object_joint(p->visual,copy.object,hand->lever_frames+i,0,0,0,joint,point,p->animation.sine_quarter))return 0;
        hand->lever_y[i]=point[1];
    }
    hand->lever_end=travel_end;hand->lever_travel=hand->lever_y[travel_end]-hand->lever_y[0];
    hand->lever=id;hand->lever_start_y=y;hand->lever_progress=0;p->objects->items[id].active=0;return 1;
}
static void vr_lever_move(TombVr *v,TombPlaytest *p,int h,int held,double y,double dt){
    TombVrHand *hand=v->hands+h;if(hand->lever<0 || !p->objects)return;
    TombObject *o=p->objects->items+hand->lever;
    if(held)hand->lever_progress=fmax(0,fmin(1,(y-hand->lever_start_y)/hand->lever_travel));
    else hand->lever_progress=fmax(0,hand->lever_progress-dt*4);
    double target=hand->lever_y[0]+hand->lever_progress*hand->lever_travel,best=1e30;int frame=0;
    for(int i=0;i<=hand->lever_end;i++){double d=fabs(hand->lever_y[i]-target);if(d<best){best=d;frame=i;}}
    if(hand->lever_progress>=1)frame=hand->lever_count-1;
    o->actor=hand->lever_frames[frame];o->active=0;
    /* Intermediate animation flags must never activate a trigger. */
    if(hand->lever_progress<1)o->actor.flags&=(uint8_t)~6u;
    if(hand->lever_progress>=1){int id=hand->lever;o->actor.flags=(o->actor.flags&~6u)|4;o->active=1;hand->lever=-1;vr_item_trigger(p,id,2);if(hand->lever_sound>=0)vr_sound(v,hand->lever_sound);}
    else if(!held && hand->lever_progress<=0){o->actor=hand->lever_frames[0];hand->lever=-1;}
}
static void vr_key_release(TombVr *v,TombPlaytest *p,int h,const double point[3]){
    int key=v->hands[h].key,slot=tomb_quest_slot(key);v->hands[h].key=0;
    if(!p->objects || slot<0 || !p->inventory.quest[slot])return;
    double head[3];tomb_vr_point(v,v->input.head.p,head);
    for(size_t i=0;i<p->objects->count;i++){
        TombObject *o=p->objects->items+i;if(o->object!=key+4 || (o->actor.flags&6))continue;
        int16_t bounds[6];if(!tomb_visual_item_bounds(p->visual,o->object,&o->actor,bounds))continue;
        double angle=o->actor.yaw*(2*PI/65536),s=sin(angle),c=cos(angle),dx=point[0]-o->actor.x,dz=point[2]-o->actor.z;
        double local[3]={c*dx-s*dz,point[1]-o->actor.y,s*dx+c*dz};
        for(int j=0;j<3;j++)local[j]=fmax(bounds[j*2],fmin(bounds[j*2+1],local[j]));
        double target[3]={o->actor.x+c*local[0]+s*local[2],o->actor.y+local[1],o->actor.z-s*local[0]+c*local[2]};
        if(distance(point,target)>100 || !reachable(p,head,point))continue;
        --p->inventory.quest[slot];o->actor.flags=(o->actor.flags&~6u)|2;
        if(o->object>=118 && o->object<=121)o->object+=4;
        vr_item_trigger(p,(int)i,3);vr_sound(v,113);break;
    }
}
