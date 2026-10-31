/* OpenGL compatibility shader: retain the PHD palette and 32 shade tables.
   Included by preview.c; all simulation-independent GPU state stays here. */
static GLuint shade_program,pickup_program;
static GLint pickup_material,pickup_water,pickup_plane,pickup_full_colour,pickup_filtering,pickup_bounds;
static GLint shade_material,shade_water,shade_warp,shade_screen,shade_room_far,shade_surface,shade_full_colour,shade_filtering,shade_bounds,shade_rgba;
#define GLFN(result,name,args) static result (APIENTRY *light_##name) args
GLFN(GLuint,CreateShader,(GLenum));
GLFN(void,ShaderSource,(GLuint,GLsizei,const char *const *,const GLint *));
GLFN(void,CompileShader,(GLuint));
GLFN(void,GetShaderiv,(GLuint,GLenum,GLint *));
GLFN(void,GetShaderInfoLog,(GLuint,GLsizei,GLsizei *,char *));
GLFN(GLuint,CreateProgram,(void));
GLFN(void,AttachShader,(GLuint,GLuint));
GLFN(void,LinkProgram,(GLuint));
GLFN(void,GetProgramiv,(GLuint,GLenum,GLint *));
GLFN(void,GetProgramInfoLog,(GLuint,GLsizei,GLsizei *,char *));
GLFN(void,DeleteShader,(GLuint));
GLFN(void,DeleteProgram,(GLuint));
GLFN(void,UseProgram,(GLuint));
GLFN(GLint,GetUniformLocation,(GLuint,const char *));
GLFN(void,Uniform1i,(GLint,GLint));
GLFN(void,Uniform2f,(GLint,GLfloat,GLfloat));
GLFN(void,Uniform4f,(GLint,GLfloat,GLfloat,GLfloat,GLfloat));
GLFN(void,ActiveTexture,(GLenum));
#undef GLFN
static int lighting_program(void){
    if(shade_program)return 1;
#define LOAD(name) do{PROC p=wglGetProcAddress("gl" #name);if(!p || (uintptr_t)p<=3 || (intptr_t)p==-1){fprintf(stderr,"OpenGL 2.0 lighting unavailable: gl%s\n",#name);return 0;}memcpy(&light_##name,&p,sizeof p);}while(0)
    LOAD(CreateShader);LOAD(ShaderSource);LOAD(CompileShader);LOAD(GetShaderiv);LOAD(GetShaderInfoLog);
    LOAD(CreateProgram);LOAD(AttachShader);LOAD(LinkProgram);LOAD(GetProgramiv);LOAD(GetProgramInfoLog);
    LOAD(DeleteShader);LOAD(DeleteProgram);LOAD(UseProgram);LOAD(GetUniformLocation);LOAD(Uniform1i);LOAD(Uniform2f);LOAD(Uniform4f);LOAD(ActiveTexture);
#undef LOAD
    const char *fragment_helpers=
        "uniform sampler2D indices;uniform sampler2D shades;uniform sampler2D colour_lookup;\n"
        "uniform int full_colour;uniform int rgba_source;uniform int filtering;uniform vec4 texture_bounds;uniform vec2 texture_size;\n"
        "uniform int material;uniform int water;uniform int surface;uniform vec2 screen_size;varying float shade;\n"
        "vec3 limited(vec3 rgb){vec3 q=floor(clamp(rgb,0.0,1.0)*31.0+0.5);float k=q.r+32.0*q.g+1024.0*q.b;return texture2D(colour_lookup,vec2((mod(k,256.0)+0.5)/256.0,(floor(k/256.0)+0.5)/128.0)).rgb;}\n"
        "vec2 bounded(vec2 uv){return texture_bounds.z>texture_bounds.x?clamp(uv,texture_bounds.xy,texture_bounds.zw):uv;}\n"
        "vec4 texel(){vec2 uv=gl_TexCoord[0].xy;if(filtering!=0)uv=bounded(uv);vec4 c=texture2D(indices,uv);if(filtering!=0 && c.a>0.0)c.rgb/=c.a;return c;}\n"
        "vec4 lit_rgb(vec4 c){if(water==2)c.r=floor(floor(c.r*31.0+0.5)/2.0)/31.0;c.rgb=clamp(c.rgb*max(0.0,2.0*(1.0-shade)),0.0,1.0);if(water==1)c.rg*=2.0/3.0;if(full_colour==0)c.rgb=limited(c.rgb);return c;}\n"
        "vec4 lit_index(vec2 uv){float i=floor(texture2D(indices,uv).r*255.0+0.5);float row=clamp(floor(shade*32.0+0.00001),0.0,31.0);vec3 rgb=texture2D(shades,vec2((i+0.5)/256.0,(row+0.5)/32.0)).rgb;if(water==2)rgb.r=floor(floor(rgb.r*31.0+0.5)/2.0)/31.0;else if(water==1)rgb.rg=floor(floor(floor(rgb.rg*63.0+0.5)*2.0/3.0)*255.0/63.0)/255.0;return vec4(rgb,i<0.5?0.0:1.0);}\n"
        "vec4 colour_sample(vec2 uv){uv=bounded(uv);vec4 c=material==-1?lit_index(uv):lit_rgb(texture2D(indices,uv));c.rgb*=c.a;return c;}\n"
        "vec4 filtered_palette(){vec2 q=gl_TexCoord[0].xy*texture_size-vec2(0.5),f=fract(q),d=1.0/texture_size,a=(floor(q)+vec2(0.5))*d;vec4 c=mix(mix(colour_sample(a),colour_sample(a+vec2(d.x,0.0)),f.x),mix(colour_sample(a+vec2(0.0,d.y)),colour_sample(a+d),f.x),f.y);if(c.a>0.0)c.rgb/=c.a;return c;}\n";
    const char *source[2]={
        "#version 110\nuniform int warp;uniform int room_far;varying vec3 eye_position;varying float shade;void main(){eye_position=(gl_ModelViewMatrix*gl_Vertex).xyz;gl_Position=ftransform();if(room_far!=0)gl_Position.z=min(gl_Position.z,gl_Position.w);if(warp!=0)gl_Position.xy+=(gl_Color.gb-vec2(0.5))*2.0*gl_Position.w;gl_TexCoord[0]=gl_MultiTexCoord0;shade=gl_Color.r;}",
        "#version 110\n\n#ifdef PICKUP_DEPTH\nuniform vec4 depth_plane;varying vec3 eye_position;\n#endif\n"
        "void main(){\n#ifdef PICKUP_DEPTH\nfloat plane_dot=dot(depth_plane.xyz,eye_position);float t=(depth_plane.w>0.0 && plane_dot>depth_plane.w)?depth_plane.w/plane_dot:1.0;vec4 proxy=gl_ProjectionMatrix*vec4(eye_position*t,1.0);gl_FragDepth=clamp(0.5*(proxy.z/proxy.w)+0.5,0.0,1.0);\n#endif\nif(material==-6){gl_FragColor=vec4(64.0/255.0,64.0/255.0,1.0,1.0);return;}if(material==-3){vec3 c=floor(texture2D(indices,gl_FragCoord.xy/screen_size).rgb*63.0+0.5);float key=c.r+64.0*c.g+4096.0*c.b;gl_FragColor=texture2D(shades,vec2((mod(key,1024.0)+0.5)/1024.0,(floor(key/1024.0)+0.5)/256.0));return;}if(material==-2){vec4 c=texture2D(indices,gl_TexCoord[0].xy);if(water==2)c.r=floor(floor(c.r*31.0+0.5)/2.0)/31.0;else if(water==1)c.rg=floor(floor(floor(c.rg*63.0+0.5)*2.0/3.0)*255.0/63.0)/255.0;gl_FragColor=c;return;}float i=material<0?floor(texture2D(indices,gl_TexCoord[0].xy).r*255.0+0.5):float(material);"
        /* Perspective interpolation can put constant shades just below an exact
           palette boundary. Bias by 0.00256 shade units (one unit is 1/256 row),
           well below the original integer precision, before truncating. */
        "if(material==-4 || (material==-1 && filtering!=0)){vec4 c=filtering!=0 && full_colour==0?filtered_palette():lit_rgb(texel());if(surface!=0){if(c.a<0.5)discard;c.a=0.5;}gl_FragColor=c;return;}if(material==-5){vec3 c=texture2D(indices,gl_FragCoord.xy/screen_size).rgb*0.5;gl_FragColor=vec4(full_colour==0?limited(c):c,1.0);return;}"
        "bool continuous=material>=0 && (full_colour!=0 || rgba_source!=0);float row=continuous?16.0:clamp(floor(shade*32.0+0.00001),0.0,31.0);vec3 rgb=texture2D(shades,vec2((i+0.5)/256.0,(row+0.5)/32.0)).rgb;"
        "if(continuous)rgb=clamp(rgb*max(0.0,2.0*(1.0-shade)),0.0,1.0);if(water==2)rgb.r=floor(floor(rgb.r*31.0+0.5)/2.0)/31.0;else if(water==1)rgb.rg=floor(floor(floor(rgb.rg*63.0+0.5)*2.0/3.0)*255.0/63.0)/255.0;if(continuous && full_colour==0)rgb=limited(rgb);if(surface!=0 && material<0 && i<0.5)discard;gl_FragColor=vec4(rgb,(material<0 && i<0.5)?0.0:surface!=0?0.5:1.0);}"};
    for(int pickup=0;pickup<2;pickup++){
    GLuint shaders[2]={0,0},program=light_CreateProgram();int ok=1;char log[2048];
    for(int i=0;i<2;i++){
        shaders[i]=light_CreateShader(i?0x8b30:0x8b31);const char *parts[]={"#version 110\n",pickup?"#define PICKUP_DEPTH 1\n":"",i?fragment_helpers:"",source[i]+strlen("#version 110\n")};light_ShaderSource(shaders[i],4,parts,NULL);light_CompileShader(shaders[i]);
        GLint compiled=0;light_GetShaderiv(shaders[i],0x8b81,&compiled);
        if(!compiled){light_GetShaderInfoLog(shaders[i],sizeof log,NULL,log);fprintf(stderr,"Lighting shader: %s\n",log);ok=0;}
        light_AttachShader(program,shaders[i]);
    }
    light_LinkProgram(program);GLint linked=0;light_GetProgramiv(program,0x8b82,&linked);
    if(!linked){light_GetProgramInfoLog(program,sizeof log,NULL,log);fprintf(stderr,"Lighting link: %s\n",log);ok=0;}
    for(int i=0;i<2;i++)light_DeleteShader(shaders[i]);
    if(!ok){light_DeleteProgram(program);return 0;}
    light_UseProgram(program);
    light_Uniform1i(light_GetUniformLocation(program,"indices"),0);light_Uniform1i(light_GetUniformLocation(program,"shades"),1);light_Uniform1i(light_GetUniformLocation(program,"colour_lookup"),2);light_Uniform2f(light_GetUniformLocation(program,"texture_size"),256,256);
    if(pickup){pickup_full_colour=light_GetUniformLocation(program,"full_colour");pickup_filtering=light_GetUniformLocation(program,"filtering");pickup_bounds=light_GetUniformLocation(program,"texture_bounds");pickup_program=program;pickup_material=light_GetUniformLocation(program,"material");pickup_water=light_GetUniformLocation(program,"water");pickup_plane=light_GetUniformLocation(program,"depth_plane");}
    else{shade_full_colour=light_GetUniformLocation(program,"full_colour");shade_filtering=light_GetUniformLocation(program,"filtering");shade_bounds=light_GetUniformLocation(program,"texture_bounds");shade_rgba=light_GetUniformLocation(program,"rgba_source");shade_program=program;
    shade_surface=light_GetUniformLocation(program,"surface");shade_room_far=light_GetUniformLocation(program,"room_far");shade_screen=light_GetUniformLocation(program,"screen_size");shade_water=light_GetUniformLocation(program,"water");shade_warp=light_GetUniformLocation(program,"warp");shade_material=light_GetUniformLocation(program,"material");}
    light_UseProgram(0);}
    return 1;
}
#include "preview_colour.h"
static int lighting_rgba(void){return visual->rgba_tiles || settings_full_colour;}
static void lighting_options(int pickup){
    light_Uniform1i(pickup?pickup_full_colour:shade_full_colour,settings_full_colour);
    light_Uniform1i(pickup?pickup_filtering:shade_filtering,settings_texture_filtering);
    if(!pickup)light_Uniform1i(shade_rgba,visual->rgba_tiles!=NULL);
    light_ActiveTexture(0x84c2);glBindTexture(GL_TEXTURE_2D,tiles[visual->tile_count*2+1]);light_ActiveTexture(0x84c0);
}
static void lighting_filter(void){
    /* Indexed and reduced-colour paths filter decoded colours in the shader. */
    GLint filter=settings_texture_filtering && settings_full_colour?GL_LINEAR:GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,filter);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,filter);
}
static void lighting_textures(void){
    colour_lookup_texture();
    for(size_t i=0;i<visual->tile_count;i++){
        glBindTexture(GL_TEXTURE_2D,tiles[visual->tile_count+i]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        if(visual->rgba_tiles)glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,256,256,0,GL_RGBA,GL_UNSIGNED_BYTE,visual->rgba_tiles+i*65536*4);
        else glTexImage2D(GL_TEXTURE_2D,0,GL_LUMINANCE8,256,256,0,GL_LUMINANCE,GL_UNSIGNED_BYTE,visual->tiles+i*65536);
    }
    unsigned char rgb[8192*3];const unsigned char *map=visual->level->file_data+visual->level->item_parsed_bytes;
    for(int i=0;i<8192;i++)for(int c=0;c<3;c++)rgb[3*i+c]=(unsigned char)(visual->palette[(i%256?map[i]:0)*3+c]*255/63);
    glBindTexture(GL_TEXTURE_2D,tiles[visual->tile_count*2]);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,256,32,0,GL_RGB,GL_UNSIGNED_BYTE,rgb);
}
static TombLighting model_light;
static GLdouble lighting_view[16];
static void light_in_view(void){
    int32_t world[3];memcpy(world,model_light.direction,sizeof world);
    for(int i=0;i<3;i++)model_light.direction[i]=(int32_t)(lighting_view[i]*world[0]+lighting_view[4+i]*world[1]+lighting_view[8+i]*world[2]);
}
static int lighting_depth(void){GLdouble m[16];glGetDoublev(GL_MODELVIEW_MATRIX,m);return settings_draw_distance==2?0:(int)floor(-m[14]);}
static void actor_lighting(const TombActor *actor,const TombPose *pose,int intensity,int16_t pitch,int16_t roll){
    water_object=(visual->level->rooms[actor->room].flags&1)!=0;
    int depth=lighting_depth();
    if(intensity>=0){tomb_light_static(tomb_word(intensity),depth,&model_light);return;}
    int32_t centre[3],world[3];for(int i=0;i<3;i++)centre[i]=(pose->bounds[2*i]+pose->bounds[2*i+1])/2;
    tomb_rotate_vector(actor->yaw,pitch,roll,centre,0,sine_table,world);
    world[0]+=actor->x;world[1]+=actor->y;world[2]+=actor->z;
    tomb_light_room(visual->lighting+actor->room,world,depth,sine_table,&model_light);light_in_view();
}
