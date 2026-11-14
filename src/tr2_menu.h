#ifndef TR2_MENU_H
#define TR2_MENU_H
#include "launcher.h"
#include <windows.h>
/* The host owns the native window, GL context and controller devices. */
typedef struct Tr2MenuHost {
    HWND window;
    HDC dc;
    unsigned (*buttons)(void);
    void (*fullscreen)(HWND);
} Tr2MenuHost;
void tr2_menu_root(const char *root);
int tr2_menu_available(void);
int tr2_menu_style(TombLauncherStyle *style);
void tr2_menu_style_free(TombLauncherStyle *style);
int tr2_menu_run(const Tr2MenuHost *host,int argc,char **argv);
#endif
