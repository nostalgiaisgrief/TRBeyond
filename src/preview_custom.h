#include "trcustoms_client.h"

enum { CUSTOM_LIST=6, CUSTOM_DETAILS, CUSTOM_DESCRIPTION, CUSTOM_ROWS=4 };
static CatalogRequest custom_query;
static cJSON *custom_page_data;
static int custom_page=1,custom_pages=1,custom_row,custom_shot,custom_more,custom_read_page;
static int custom_count,custom_failed;
typedef struct {
    CatalogRequest request;
    GLuint texture;
    ULONGLONG used;
} CustomImage;
static CustomImage custom_images[10];
static char custom_title[512],custom_lines[2048][192];
static int custom_line_count;

static const cJSON *custom_entry(int index){
    return cJSON_GetArrayItem(catalog_field(custom_page_data,"results"),index);
}
static HANDLE custom_task;
static int custom_task_id,custom_status_id,custom_remove,custom_installed[CUSTOM_ROWS];
static char custom_status[160];
static int custom_id(const cJSON *entry){
    const cJSON *id=catalog_field(entry,"id");
    return cJSON_IsNumber(id)?id->valueint:0;
}
static void custom_refresh_installed(void){
    for(int i=0;i<custom_count;i++){
        char path[MAX_PATH];custom_path(path,custom_id(custom_entry(i)),"Game/level.phd");
        custom_installed[i]=GetFileAttributesA(path)!=INVALID_FILE_ATTRIBUTES;
    }
}
static void custom_poll_task(void){
    if(!custom_task || WaitForSingleObject(custom_task,0)!=WAIT_OBJECT_0)return;
    DWORD result=1;GetExitCodeProcess(custom_task,&result);
    CloseHandle(custom_task);custom_task=NULL;
    custom_status_id=custom_task_id;custom_refresh_installed();custom_remove=0;
    if(result)snprintf(custom_status,sizeof custom_status,"Failed - see Custom/TR1/%d/import.log",custom_task_id);
    else custom_status[0]=0;
}
static void custom_install_task(const cJSON *entry,int remove){
    if(custom_task)return;
    int id=custom_id(entry),file=custom_id(catalog_field(entry,"last_file"));
    char path[MAX_PATH],cmd[160],folder[MAX_PATH];
    snprintf(folder,sizeof folder,"%s/Custom/TR1",install_root);CreateDirectoryA(folder,NULL);
    custom_path(folder,id,"");CreateDirectoryA(folder,NULL);
    custom_status_id=id;
    const char *failure="No downloadable release file in catalogue";
    if(id>0 && (file>0 || remove)){
        GetModuleFileNameA(NULL,path,MAX_PATH);
        char *end=strrchr(path,'\\');
        if(end && (size_t)(end-path)+sizeof "\\TRCustomImport.exe"<=MAX_PATH){
            strcpy(end,"\\TRCustomImport.exe");
            snprintf(cmd,sizeof cmd,"TRCustomImport.exe %d %d%s",id,file>0?file:1,remove?" --uninstall":"");
            STARTUPINFOA startup={0};PROCESS_INFORMATION process={0};startup.cb=sizeof startup;
            if(CreateProcessA(path,cmd,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,install_root,&startup,&process)){
                CloseHandle(process.hThread);custom_task=process.hProcess;custom_task_id=id;custom_status[0]=0;return;
            }
        }
        failure="Could not launch TRCustomImport.exe";
    }
    custom_log_error(id,failure);
    snprintf(custom_status,sizeof custom_status,"Failed - see Custom/TR1/%d/import.log",id);
}
static void custom_fetch(int page){
    if(custom_query.thread && !catalog_finished(&custom_query))return;
    cJSON_Delete(custom_query.record);
    memset(&custom_query,0,sizeof custom_query);
    custom_query.page=page;custom_page=page;custom_failed=0;custom_row=0;custom_count=0;
    cJSON_Delete(custom_page_data);custom_page_data=NULL;
    snprintf(custom_query.url,sizeof custom_query.url,"https://trcustoms.org/api/levels/?engines=5&sort=release_date&page_size=4&page=%d",page);
    catalog_start(&custom_query);
    if(!custom_query.thread)custom_failed=1;
}
static void custom_poll(void){
    custom_poll_task();
    if(!custom_query.thread || !catalog_finished(&custom_query))return;
    cJSON *page=custom_query.record;custom_query.record=NULL;
    const cJSON *rows=catalog_field(page,"results"),*pages=catalog_field(page,"last_page");
    if(!cJSON_IsArray(rows) || !cJSON_IsNumber(pages) || cJSON_GetArraySize(rows)>CUSTOM_ROWS){
        cJSON_Delete(page);custom_failed=1;return;
    }
    custom_page_data=page;custom_count=cJSON_GetArraySize(rows);custom_pages=pages->valueint>0?pages->valueint:1;custom_refresh_installed();
}
static CustomImage *custom_image(const char *url){
    if(!url || !*url || strlen(url)>=sizeof custom_images[0].request.url)return NULL;
    CustomImage *oldest=NULL;
    for(size_t i=0;i<sizeof custom_images/sizeof *custom_images;i++){
        CustomImage *slot=custom_images+i;CatalogRequest *r=&slot->request;
        if(r->thread && catalog_finished(r) && r->pixels){
            glGenTextures(1,&slot->texture);glBindTexture(GL_TEXTURE_2D,slot->texture);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,r->w,r->h,0,GL_RGBA,GL_UNSIGNED_BYTE,r->pixels);
            free(r->pixels);r->pixels=NULL;
        }
        if(!strcmp(url,r->url)){slot->used=GetTickCount64();return slot;}
        if(!r->thread && (!oldest || slot->used<oldest->used))oldest=slot;
    }
    if(!oldest)return NULL;
    if(oldest->texture)glDeleteTextures(1,&oldest->texture);
    memset(oldest,0,sizeof *oldest);oldest->used=GetTickCount64();
    strcpy(oldest->request.url,url);catalog_start(&oldest->request);
    return oldest;
}
/* The original font is ASCII. Fold accents and common punctuation for display. */
static void custom_plain(const char *src,char *out,size_t capacity){
    size_t n=0;
    while(*src && n+1<capacity){
        unsigned char c=(unsigned char)*src++;
        if(c<128){
            if(c=='\r' || c=='`' || (c=='*' && *src=='*')){if(c=='*')src++;continue;}
            if(c=='['){
                const char *end=strchr(src,']');
                if(end && end-src<20 && (!strncmp(src,"center]",7) || !strncmp(src,"/center]",8) ||
                   !strncmp(src,"s]",2) || !strncmp(src,"/s]",3))){src=end+1;continue;}
                if(end && end[1]=='(')continue;
            }
            if(c==']' && *src=='('){const char *end=strchr(src,')');if(end){src=end+1;continue;}}
            if(c=='#' && (!n || out[n-1]=='\n')){while(*src=='#')src++;if(*src==' ')src++;continue;}
            if(c=='\n' || c>=32)out[n++]=(char)c;
            else if(c=='\t')out[n++]=' ';
        }else{
            int len=c<224?2:c<240?3:4,available=1;
            while(available<len && ((unsigned char)src[available-1]&192)==128)available++;
            wchar_t wc[2],fold[8];char ch='?';
            if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,src-1,available,wc,2)==1){
                if(wc[0]==0x2018 || wc[0]==0x2019)ch='\'';
                else if(wc[0]==0x201c || wc[0]==0x201d)ch='"';
                else if(wc[0]==0x2013 || wc[0]==0x2014)ch='-';
                else if(wc[0]==0xa0)ch=' ';
                else if(FoldStringW(MAP_COMPOSITE,wc,1,fold,8)>0 && fold[0]<128)ch=(char)fold[0];
            }
            src+=available-1;out[n++]=ch;
        }
    }
    out[n]=0;
}
static void custom_author(const cJSON *entry,char *out,size_t size){
    const cJSON *author;size_t used=0;out[0]=0;
    cJSON_ArrayForEach(author,catalog_field(entry,"authors")){
        char name[100];custom_plain(catalog_string(author,"username"),name,sizeof name);
        if(!*name)continue;
        snprintf(out+used,size-used,"%s%s",used?", ":"",name);used=strlen(out);
    }
    if(!used)snprintf(out,size,"Unknown author");
}
static int custom_width(const char *s){
    TombTextStyle style={0,0,65536,65536,1,6,0};return tomb_text_width(s,&style);
}
/* Return the next line boundary. A long unbroken word is split at a glyph. */
static const char *custom_wrap(const char *src,char *line,size_t cap,int limit){
    size_t n=0,space=0;
    while(*src==' ')src++;
    while(src[n] && src[n]!='\n' && n+1<cap){
        line[n]=src[n];line[n+1]=0;
        if(n && custom_width(line)>limit)break;
        if(src[n]==' ')space=n;
        n++;
    }
    if(src[n] && src[n]!='\n' && src[n]!=' ' && space)n=space;
    memcpy(line,src,n);line[n]=0;src+=n;
    while(*src==' ')src++;
    if(*src=='\n')src++;
    return src;
}
static void custom_details(void){
    const cJSON *entry=custom_entry(custom_row);
    char name[320],author[160];custom_plain(catalog_string(entry,"name"),name,sizeof name);custom_author(entry,author,sizeof author);
    snprintf(custom_title,sizeof custom_title,"%s by %s",name,author);
    const char *description=catalog_string(entry,"description");
    char *plain=malloc(strlen(description)+1);custom_line_count=0;
    if(plain){
        custom_plain(description,plain,strlen(description)+1);
        const char *next=plain;
        while(*next && custom_line_count<2048){
            next=custom_wrap(next,custom_lines[custom_line_count++],sizeof custom_lines[0],456);
        }
        free(plain);
    }
    custom_remove=custom_more=custom_shot=custom_read_page=0;new_game_menu=CUSTOM_DETAILS;
}
static void custom_input(unsigned input){
    custom_poll();
    if(input&TOMB_RING_BACK){
        if(new_game_menu==CUSTOM_DESCRIPTION)new_game_menu=CUSTOM_DETAILS;
        else if(new_game_menu==CUSTOM_DETAILS)new_game_menu=CUSTOM_LIST;
        else{new_game_menu=4;new_game_row=3;}
        return;
    }
    if(new_game_menu==CUSTOM_LIST){
        if(custom_failed && (input&TOMB_RING_SELECT)){custom_fetch(custom_page);return;}
        if(!custom_page_data)return;
        if((input&TOMB_RING_LEFT) && custom_page>1)custom_fetch(custom_page-1);
        else if((input&TOMB_RING_RIGHT) && custom_page<custom_pages)custom_fetch(custom_page+1);
        else if((input&16) && custom_row>0)custom_row--;
        else if((input&32) && custom_row+1<custom_count)custom_row++;
        else if((input&TOMB_RING_SELECT) && custom_count)custom_details();
    }else if(new_game_menu==CUSTOM_DETAILS){
        int shots=cJSON_GetArraySize(catalog_field(custom_entry(custom_row),"screenshots"));
        if(shots>1 && (input&(TOMB_RING_LEFT|TOMB_RING_RIGHT)))custom_shot=(custom_shot+((input&TOMB_RING_LEFT)?shots-1:1))%shots;
        if(input&16){
            if(custom_remove)custom_remove=0;
            else if(custom_line_count>3)custom_more=1;
        }
        if(input&32){
            if(custom_more)custom_more=0;
            else if(custom_installed[custom_row])custom_remove=1;
        }
        if(input&TOMB_RING_SELECT){
            const cJSON *entry=custom_entry(custom_row);
            if(custom_more){new_game_menu=CUSTOM_DESCRIPTION;custom_read_page=0;}
            else if(!custom_task && custom_installed[custom_row] && !custom_remove){
                new_game_custom_id=custom_id(entry);
                new_game_level=1;new_game_menu=1;new_game_row=new_game_mode;
            }else if(!custom_task)custom_install_task(entry,custom_remove);
        }
    }else{
        if((input&16) && custom_read_page>0)custom_read_page--;
        if((input&32) && (custom_read_page+1)*16<custom_line_count)custom_read_page++;
    }
}
/* Menu coordinates are centred in the title viewport, including forced 4:3. */
static double custom_scale(void){return fmin(ui_scale(),fmin(width/520.0,height/418.0));}
static void custom_rect(double x,double y,double w,double h,float shade){
    double s=custom_scale(),left=width*.5+x*s,top=height*.5+y*s;
    glDisable(GL_TEXTURE_2D);glColor3f(shade,shade,shade);
    glBegin(GL_QUADS);glVertex2d(left,top);glVertex2d(left+w*s,top);glVertex2d(left+w*s,top+h*s);glVertex2d(left,top+h*s);glEnd();
}
static void custom_text(double x,double y,const char *value,int centred,float brightness,double fit){
    double s=custom_scale(),glyph_scale=s;
    if(fit>0 && custom_width(value)>fit)glyph_scale*=fit/custom_width(value);
    TombTextStyle style={(int)(x*s+(centred?0:width*.5)),(int)(height*.5+y*s),
        (int32_t)(glyph_scale*65536),(int32_t)(glyph_scale*65536),1,6,(uint16_t)(centred?TOMB_TEXT_CENTRE:0)};
    glEnable(GL_TEXTURE_2D);glEnable(GL_ALPHA_TEST);glColor3f(brightness,brightness,brightness);
    tomb_text_layout(value,&style,width,height,font_glyph,NULL);
    glDisable(GL_ALPHA_TEST);glDisable(GL_TEXTURE_2D);
}
static void custom_arrow(int x,int y,int right){
    double s=custom_scale();TombTextGlyph arrow={right?109:108,(int)(width*.5+x*s),(int)(height*.5+y*s),(int32_t)(s*65536),(int32_t)(s*65536)};
    glEnable(GL_TEXTURE_2D);glEnable(GL_ALPHA_TEST);glColor3f(1,1,1);font_glyph(NULL,&arrow);
    glDisable(GL_ALPHA_TEST);glDisable(GL_TEXTURE_2D);
}
static void custom_art(const char *url,int x,int y,int w,int h,float brightness){
    CustomImage *image=custom_image(url);
    if(!image || !image->texture)return;
    double ratio=fmin((double)w/image->request.w,(double)h/image->request.h);
    double s=custom_scale(),iw=image->request.w*ratio,ih=image->request.h*ratio;
    double left=width*.5+(x+(w-iw)/2)*s,top=height*.5+(y+(h-ih)/2)*s;
    glEnable(GL_TEXTURE_2D);glColor3f(brightness,brightness,brightness);
    glBindTexture(GL_TEXTURE_2D,image->texture);glBegin(GL_QUADS);
    glTexCoord2d(0,0);glVertex2d(left,top);glTexCoord2d(1,0);glVertex2d(left+iw*s,top);
    glTexCoord2d(1,1);glVertex2d(left+iw*s,top+ih*s);glTexCoord2d(0,1);glVertex2d(left,top+ih*s);
    glEnd();glDisable(GL_TEXTURE_2D);
}
static void custom_overlay(void){
    char line[512];custom_poll();
    custom_rect(-252,-203,504,406,.09f);
    if(new_game_menu==CUSTOM_LIST){
        custom_text(0,-176,"Custom Levels from TRCustoms.org",1,1,478);
        if(!custom_count){
            const char *message=custom_failed?"Catalogue unavailable":custom_page_data?"No levels found":"Loading levels...";
            custom_text(0,0,message,1,1,460);
            if(custom_failed)custom_text(0,30,"Retry",1,1,460);
            return;
        }
        for(int i=0;i<custom_count;i++){
            const cJSON *entry=custom_entry(i);int top=-159+i*79;
            if(custom_row==i){
                custom_rect(-243,top,486,76,.46f);custom_rect(-241,top+2,482,72,.18f);
            }
            custom_rect(-231,top+6,64,64,.07f);
            custom_art(catalog_string(catalog_field(entry,"cover"),"url"),-231,top+6,64,64,custom_installed[i]?1:.75f);
            const float text_brightness=custom_installed[i]?1:.72f;
            custom_plain(catalog_string(entry,"name"),line,sizeof line);
            char title[192];const char *tail=custom_wrap(line,title,sizeof title-4,358);
            if(*tail)strcat(title,"...");
            custom_text(-156,top+16,title,0,text_brightness,382);
            char author[256];custom_author(entry,author,sizeof author);snprintf(line,sizeof line,"By %s",author);
            custom_text(-156,top+34,line,0,text_brightness,382);
            int year,month,day;static const char *months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
            const char *date=catalog_string(entry,"release_date");
            if(sscanf(date,"%d-%d-%d",&year,&month,&day)==3 && month>=1 && month<=12)
                snprintf(line,sizeof line,"%d %s %d",day,months[month-1],year);
            else snprintf(line,sizeof line,"%s",date);
            custom_text(-156,top+51,line,0,text_brightness,380);
            const char *rating=catalog_string(catalog_field(entry,"rating_class"),"name");
            custom_text(-156,top+68,*rating?rating:"Unrated",0,text_brightness,380);
        }
        snprintf(line,sizeof line,"%d / %d",custom_page,custom_pages);
        custom_text(-178,184,line,1,1,80);
        if(custom_page>1)custom_arrow(-236,184,0);
        if(custom_page<custom_pages)custom_arrow(-134,184,1);
        custom_text(0,184,custom_installed[custom_row]?"Installed":"Not Installed",1,1,220);
        return;
    }
    char first[192];const char *rest=custom_wrap(custom_title,first,sizeof first,470);
    custom_text(0,-177,first,1,1,470);
    if(*rest)custom_text(0,-157,rest,1,1,470);
    if(new_game_menu==CUSTOM_DESCRIPTION){
        int start=custom_read_page*16;
        for(int i=0;i<16 && start+i<custom_line_count;i++)custom_text(-228,-126+i*18,custom_lines[start+i],0,1,456);
        snprintf(line,sizeof line,"%d / %d",custom_read_page+1,(custom_line_count+15)/16);
        custom_text(0,182,line,1,1,100);
        return;
    }
    const cJSON *entry=custom_entry(custom_row),*shots=catalog_field(entry,"screenshots");
    int count=cJSON_GetArraySize(shots);
    const cJSON *file=count?catalog_field(cJSON_GetArrayItem(shots,custom_shot),"file"):catalog_field(entry,"cover");
    const char *url=catalog_string(file,"url");
    if(!*url)url=catalog_string(catalog_field(entry,"cover"),"url");
    if(*url)custom_art(url,-195,-139,390,172,1);
    if(count>1){custom_arrow(-224,-48,0);custom_arrow(213,-48,1);}
    int top=*url?60:-92;
    for(int i=0;i<3 && i<custom_line_count;i++)custom_text(-228,top+i*18,custom_lines[i],0,1,456);
    int action_y=top+18*(custom_line_count<3?custom_line_count:3)+15;
    if(custom_line_count>3){
        if(custom_more)custom_rect(-93,action_y-14,186,20,.30f);
        custom_text(0,action_y,"Read More",1,1,180);action_y+=27;
    }
    if(!custom_more && !custom_remove)custom_rect(-93,action_y-14,186,20,.23f);
    int id=custom_id(entry);
    int busy=custom_task && custom_task_id==id;
    const cJSON *size=catalog_field(catalog_field(entry,"last_file"),"size");
    char download_label[80]="Download";
    if(cJSON_IsNumber(size) && size->valuedouble>0 && isfinite(size->valuedouble))
        snprintf(download_label,sizeof download_label,"Download - %.1fMB",size->valuedouble/1000000.0);
    custom_text(0,action_y,busy?"Working...":custom_installed[custom_row]?"Play":download_label,1,custom_task?.65f:1,180);
    if(custom_installed[custom_row]){
        action_y+=22;
        if(custom_remove)custom_rect(-93,action_y-14,186,20,.30f);
        custom_text(0,action_y,"Uninstall",1,1,180);
    }
    if(custom_status_id==id && *custom_status)custom_text(0,192,custom_status,1,1,472);
}
