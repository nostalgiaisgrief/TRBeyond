/* Presentation-only water sprites, driven once per successful simulation tick.
   Original splash/bubble spawn and controller rules: 0x1dc08 / 0x1d9f0. */
static TombWaterParticle water_particles[256],water_previous[256];
static uint32_t water_random=UINT32_C(0xd371f947);
static unsigned water_rand(void){water_random=water_random*UINT32_C(1103515245)+12345;return (water_random>>10)&32767;}
static void water_particles_reset(void){memset(water_particles,0,sizeof water_particles);memset(water_previous,0,sizeof water_previous);effect_ticks=0;}
static void water_particles_tick(void){
    ++effect_ticks;
    memcpy(water_previous,water_particles,sizeof water_particles);
    for(int i=0;i<256;i++){
        TombWaterParticle *p=water_particles+i;int frames;
        if(!p->active)continue;
        if(tomb_hazard_sprite(visual,p->id,p->frame,&frames)<0){p->active=0;continue;}
        tomb_water_particle_step(p,frames,sine_table);
        if(p->active && p->id==155){
            TombSectorRef ref;TombHeights h;
            if(!tomb_find_sector(level,p->x,p->y,p->z,p->room,&ref) || !(level->rooms[ref.room].flags&1) ||
               !tomb_static_heights(level,ref,p->x,p->z,0,&h) || h.ceiling==-32512 || p->y<=h.ceiling)p->active=0;
            else p->room=(int16_t)ref.room;
        }
    }
    for(unsigned event=0;event<play.sound_count;event++){
        const TombSoundEvent *s=play.sounds+event;
        int splash=s->kind==5 && s->id==33,bubble=s->kind==6 && s->id==3;
        if(!splash && !bubble)continue;
        int count=splash?10:(int)(water_rand()*3/32768);
        int32_t point[3]={s->x,s->y,s->z};int room=play.lara.actor.room;
        if(splash){int y=tomb_water_height(level,s->x,s->z,room);if(y==-32512)continue;point[1]=y;}
        else {point[0]=point[1]=0;point[2]=50;if(!tomb_object_joint(visual,0,&play.lara.actor,play.lara.pitch,play.lara.lean,play.lara.head_yaw,14,point,sine_table))continue;}
        for(int j=0;j<count;j++)for(int i=0;i<256;i++)if(!water_particles[i].active){
            TombWaterParticle p={0};p.x=point[0];p.y=point[1];p.z=point[2];p.room=(int16_t)room;p.id=splash?153:155;p.active=1;
            if(splash){p.yaw=tomb_word(((int)water_rand()-16384)*2);p.speed=(int16_t)(water_rand()/256);}
            else {p.speed=(int16_t)(10+water_rand()*6/32768);p.frame=(int16_t)(water_rand()*3/32768);}
            water_particles[i]=water_previous[i]=p;break;
        }
    }
}
static void water_particles_draw(void){
    for(int i=0;i<256;i++){
        const TombWaterParticle *p=water_particles+i,*old=water_previous+i;
        if(!p->active || p->room<0 || (size_t)p->room>=level->room_count)continue;
        TombScreenRect clip=room_rects[p->room];if(clip.left>=clip.right || clip.bottom>=clip.top)continue;
        TombHazardEffect e={0};e.id=p->id;e.frame=p->frame;e.room=p->room;
        e.x=(int32_t)(old->x+camera_alpha*(p->x-old->x));e.y=(int32_t)(old->y+camera_alpha*(p->y-old->y));e.z=(int32_t)(old->z+camera_alpha*(p->z-old->z));
        draw_hazard_effect(&e,SIZE_MAX,4096);
    }
}
