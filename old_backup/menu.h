#ifndef MENU_H
#define MENU_H
#include <tonc.h>

typedef enum {
    SCR_BOOT = 0,
    SCR_MENU,
    SCR_WORKSPACE,
    SCR_PAINT,
    SCR_RUN,
    SCR_SETTINGS,
    SCR_ABOUT
} Screen;

extern Screen g_screen;
extern int g_paint_slot;

void menu_init(void);
void menu_enter(Screen s);
void menu_draw(void);
void menu_input(void);
#endif
