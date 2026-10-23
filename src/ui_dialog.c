#include "ui_dialog.h"
#include <string.h>
int tomb_dialog_layout(int object,int selected,int slots,TombDialogEntry out[32]) {
    const int centre=TOMB_TEXT_CENTRE|TOMB_TEXT_MIDDLE;
    memset(out,0,32*sizeof *out);
    if(object==71) {
        out[0]=(TombDialogEntry){0,-16,TOMB_TEXT_CENTRE|TOMB_TEXT_BOTTOM,0,0};
        if(!slots)return 1;
        /* DOS high-resolution requester: ten rows, 18-pixel spacing. */
        out[0]=(TombDialogEntry){0,-300,TOMB_TEXT_CENTRE|TOMB_TEXT_BOTTOM,272,220};
        out[1]=(TombDialogEntry){0,-298,TOMB_TEXT_CENTRE|TOMB_TEXT_BOTTOM,268,16};
        for(int row=0;row<10;row++)out[row+2]=(TombDialogEntry){0,-270+18*row,TOMB_TEXT_CENTRE|TOMB_TEXT_BOTTOM,row==selected?260:0,16};
        return 12;
    }
    if(object==95 || object==96) {
        int detail=object==95,rows=detail?3:2;
        out[0]=(TombDialogEntry){0,-32,centre,detail?160:140,detail?107:85};
        out[1]=(TombDialogEntry){0,-30,centre,detail?156:136,16};
        for(int row=0;row<rows;row++)out[row+2]=(TombDialogEntry){0,row*25,centre,row==selected?(detail?148:128):0,16};
        return rows+2;
    }
    if(object==97) {
        out[0]=(TombDialogEntry){0,-60,centre,420,150};
        out[1]=(TombDialogEntry){0,-50,centre,0,0};
        for(int row=0;row<13;row++) {
            int right=row>=7,y=-25+15*(right?row-7:row);
            if(row==12)y=65;
            out[2+row]=(TombDialogEntry){right?410:190,y,TOMB_TEXT_MIDDLE,0,0};
            out[15+row]=(TombDialogEntry){right?340:120,y,TOMB_TEXT_MIDDLE,0,0};
        }
        return 28;
    }
    return 0;
}
void tomb_dialog_bounds(const TombDialogEntry *entry,int text_width,int out[4]) {
    int x=entry->x,y=entry->y,w=entry->panel_width?entry->panel_width:text_width;
    if(entry->flags&TOMB_TEXT_CENTRE)x+=320-text_width/2;
    if(entry->flags&TOMB_TEXT_MIDDLE)y+=240;
    else if(entry->flags&TOMB_TEXT_BOTTOM)y+=480;
    /* DOS Text_Draw saves x-2,y-15 before emitting glyphs. */
    out[0]=x-2+(text_width-w)/2;out[1]=y-15;
    out[2]=w+4;out[3]=entry->panel_height?entry->panel_height:16;
}
