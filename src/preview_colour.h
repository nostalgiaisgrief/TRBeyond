/* A level-local 256-colour lookup for RGBA assets. DOS keeps its authored
   palette; PS1 palettes are reduced together, without needing DOS assets. */
typedef struct ColourBox { int first,count; } ColourBox;
static int colour_axis;
static int colour_compare(const void *a,const void *b){
    unsigned x=*(const uint16_t *)a,y=*(const uint16_t *)b;
    int d=(int)((x>>(colour_axis*5))&31)-(int)((y>>(colour_axis*5))&31);
    return d?d:(int)x-(int)y;
}
static void colour_palette(unsigned char palette[768]){
    if(!visual->rgba_tiles){for(int i=0;i<768;i++)palette[i]=(unsigned char)(visual->palette[i]*255/63);return;}
    uint32_t histogram[32768]={0};uint16_t keys[32768];int count=0;
    /* Sample texture colours and their lit range so dark rooms keep detail. */
    for(size_t i=0;i<visual->tile_count*65536;i+=4){
        const unsigned char *p=visual->rgba_tiles+4*i;if(p[3]<128)continue;
        for(unsigned shade=1;shade<=4;shade++){
            unsigned key=0;for(int c=0;c<3;c++)key|=((p[c]*shade/4)>>3)<<(5*c);
            histogram[key]++;
        }
    }
    for(int i=0;i<256;i++){unsigned key=0;for(int c=0;c<3;c++)key|=(visual->palette[3*i+c]>>1)<<(5*c);histogram[key]+=16;}
    histogram[0]++;for(int i=0;i<32768;i++)if(histogram[i])keys[count++]=(uint16_t)i;
    /* Keep exact black, then median-cut the remaining occupied colours. */
    ColourBox boxes[255]={{1,count-1}};int box_count=count>1?1:0;
    while(box_count<255){
        int best=-1,axis=0;uint64_t score=0;
        for(int b=0;b<box_count;b++){
            ColourBox box=boxes[b];if(box.count<2)continue;
            int lo[3]={31,31,31},hi[3]={0,0,0};uint64_t weight=0;
            for(int i=box.first;i<box.first+box.count;i++){unsigned key=keys[i];weight+=histogram[key];for(int c=0;c<3;c++){int v=(key>>(5*c))&31;if(v<lo[c])lo[c]=v;if(v>hi[c])hi[c]=v;}}
            for(int c=0;c<3;c++){uint64_t value=weight*(unsigned)(hi[c]-lo[c]);if(value>score){score=value;best=b;axis=c;}}
        }
        if(best<0)break;
        ColourBox *box=boxes+best;colour_axis=axis;qsort(keys+box->first,(size_t)box->count,sizeof *keys,colour_compare);
        uint64_t total=0,part=0;for(int i=0;i<box->count;i++)total+=histogram[keys[box->first+i]];
        int split=0;do{part+=histogram[keys[box->first+split++]];}while(split<box->count-1 && part*2<total);
        boxes[box_count++]=(ColourBox){box->first+split,box->count-split};box->count=split;
    }
    memset(palette,0,768);
    for(int b=0;b<box_count;b++){
        uint64_t total=0,sum[3]={0,0,0};ColourBox box=boxes[b];
        for(int i=box.first;i<box.first+box.count;i++){unsigned key=keys[i],w=histogram[key];total+=w;for(int c=0;c<3;c++)sum[c]+=(uint64_t)((key>>(5*c))&31)*w;}
        for(int c=0;c<3;c++)palette[3*(b+1)+c]=(unsigned char)((sum[c]*255+total*15)/(total*31));
    }
}
static void colour_lookup_texture(void){
    unsigned char palette[768],rgb[32768*3];colour_palette(palette);
    for(int key=0;key<32768;key++){
        int best=0,distance=INT_MAX;
        for(int i=0;i<256;i++){
            int d=0;for(int c=0;c<3;c++){int delta=((key>>(5*c))&31)*255/31-palette[3*i+c];d+=delta*delta;}
            if(d<distance){distance=d;best=i;}
        }
        memcpy(rgb+3*key,palette+3*best,3);
    }
    glBindTexture(GL_TEXTURE_2D,tiles[visual->tile_count*2+1]);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,256,128,0,GL_RGB,GL_UNSIGNED_BYTE,rgb);
}
