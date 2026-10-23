#include "room_visibility.h"
#include "preview_camera.h"
#include <math.h>
#include <string.h>
static int word(const unsigned char *p){unsigned v=p[0]|(unsigned)p[1]<<8;return v<32768?(int)v:(int)v-65536;}
static int empty(TombScreenRect r){return r.left>=r.right || r.bottom>=r.top;}
static TombScreenRect intersect(TombScreenRect a,TombScreenRect b){
    return (TombScreenRect){a.left>b.left?a.left:b.left,a.bottom>b.bottom?a.bottom:b.bottom,a.right<b.right?a.right:b.right,a.top<b.top?a.top:b.top};
}
int tomb_room_visibility_projection(const TombVisual *v,int room,const double eye[3],const double view[16],int width,int height,double fx,double fy,double cx,double cy,TombScreenRect *out) {
    if(!v || !v->portals || !out || room<0 || (size_t)room>=v->room_count || width<=0 || height<=0)return 0;
    memset(out,0,v->room_count*sizeof *out);out[room]=(TombScreenRect){0,0,width,height};
    /* Rectangles only grow, so cycles converge. Expansion through another
       doorway revisits the room; a one-visit flood fill would lose openings. */
    int changed=1;
    while(changed) {
        changed=0;
        for(size_t r=0;r<v->room_count;r++) {
            if(empty(out[r]))continue;
            const TombRoom *world=&v->level->rooms[r];
            for(size_t j=0;j<v->portals[r].count;j++) {
                const unsigned char *p=v->portals[r].data+32*j;int next=word(p);
                double first[3]={world->x+word(p+8),word(p+10),world->z+word(p+12)},facing=0;
                for(int k=0;k<3;k++)facing+=(eye[k]-first[k])*word(p+2+2*k);
                if(facing<-.01)continue;
                double polygon[8][3],clipped[8][3];int count=0;
                for(int k=0;k<4;k++) {
                    double a[3]={world->x+word(p+8+6*k),word(p+10+6*k),world->z+word(p+12+6*k)};
                    for(int axis=0;axis<3;axis++)polygon[k][axis]=view[axis]*a[0]+view[4+axis]*a[1]+view[8+axis]*a[2]+view[12+axis];
                }
                /* Clip at the eye plane, not the drawing near plane. A doorway
                   nearer than the near plane must still reveal its next room. */
                for(int k=0;k<4;k++) {
                    const double *a=polygon[k],*b=polygon[(k+1)%4];int ain=a[2]<=-.01,bin=b[2]<=-.01;
                    if(ain)memcpy(clipped[count++],a,3*sizeof(double));
                    if(ain!=bin) {double t=(-.01-a[2])/(b[2]-a[2]);for(int axis=0;axis<3;axis++)clipped[count][axis]=a[axis]+t*(b[axis]-a[axis]);count++;}
                }
                if(!count)continue;
                double left=width,bottom=height,right=0,top=0;
                for(int k=0;k<count;k++) {
                    double x=cx+clipped[k][0]*fx/(-clipped[k][2]);
                    double y=cy+clipped[k][1]*fy/(-clipped[k][2]);
                    left=fmin(left,x);right=fmax(right,x);bottom=fmin(bottom,y);top=fmax(top,y);
                }
                /* DOS 1a7b5..1a850 expands an eye-straddling portal to viewport
                   edges using endpoint signs, rather than its exact intersection.
                   The conservative rectangle keeps oversized room items visible. */
                for(int k=0;k<4;k++){
                    const double *a=polygon[k],*b=polygon[(k+1)%4];
                    if((a[2]>0)==(b[2]>0))continue;
                    if(a[0]<0 && b[0]<0)left=0;
                    else if(a[0]>0 && b[0]>0)right=width;
                    else {left=0;right=width;}
                    if(a[1]<0 && b[1]<0)bottom=0;
                    else if(a[1]>0 && b[1]>0)top=height;
                    else {bottom=0;top=height;}
                }
                TombScreenRect aperture={(int)floor(fmin(width,fmax(0,left))),(int)floor(fmin(height,fmax(0,bottom))),(int)ceil(fmax(0,fmin(width,right))),(int)ceil(fmax(0,fmin(height,top)))};
                aperture=intersect(aperture,out[r]);if(empty(aperture))continue;
                TombScreenRect old=out[next],merged=aperture;
                if(!empty(old))merged=(TombScreenRect){old.left<aperture.left?old.left:aperture.left,old.bottom<aperture.bottom?old.bottom:aperture.bottom,old.right>aperture.right?old.right:aperture.right,old.top>aperture.top?old.top:aperture.top};
                if(memcmp(&old,&merged,sizeof old)){out[next]=merged;changed=1;}
            }
        }
    }
    return 1;
}

int tomb_static_visible_range(const TombStaticDef *def,const double view[16],int width,int height,double fx,double fy,double cx,double cy,TombScreenRect room,double far_distance) {
    if(!def || !(def->flags&2) || empty(room) || width<=0 || height<=0)return 0;
    /* DOS 0x10090 projects the original drawing bounds, tests overlap with the
       room rectangle, and leaves object polygon clipping at the full viewport. */
    double left=INFINITY,bottom=INFINITY,right=-INFINITY,top=-INFINITY;
    int projected=0;
    if(-view[14]>=far_distance)return 0;
    for(int k=0;k<8;k++) {
        double a[3]={def->draw_bounds[k&1],def->draw_bounds[2+((k>>1)&1)],def->draw_bounds[4+((k>>2)&1)]},p[3];
        for(int axis=0;axis<3;axis++)p[axis]=view[axis]*a[0]+view[4+axis]*a[1]+view[8+axis]*a[2]+view[12+axis];
        if(p[2]>=-10 || p[2]<=-far_distance)continue;
        double x=cx+p[0]*fx/(-p[2]),y=cy+p[1]*fy/(-p[2]);
        left=fmin(left,x);right=fmax(right,x);bottom=fmin(bottom,y);top=fmax(top,y);projected++;
    }
    return projected && left<=room.right && right>=room.left && bottom<=room.top && top>=room.bottom;
}

int tomb_static_visible_projection(const TombStaticDef *def,const double view[16],int width,int height,double fx,double fy,double cx,double cy,TombScreenRect room){return tomb_static_visible_range(def,view,width,height,fx,fy,cx,cy,room,20480);}
int tomb_static_visible(const TombStaticDef *def,const double view[16],int width,int height,TombScreenRect room){double focal=tomb_preview_aspect_focal(height);return tomb_static_visible_projection(def,view,width,height,focal,focal,width*.5,height*.5,room);}
int tomb_room_visibility_focal(const TombVisual *v,int room,const double eye[3],const double view[16],int width,int height,double focal,TombScreenRect *out){return tomb_room_visibility_projection(v,room,eye,view,width,height,focal,focal,width*.5,height*.5,out);}
int tomb_room_visibility(const TombVisual *v,int room,const double eye[3],const double view[16],int width,int height,TombScreenRect *out){return tomb_room_visibility_focal(v,room,eye,view,width,height,tomb_preview_aspect_focal(height),out);}
