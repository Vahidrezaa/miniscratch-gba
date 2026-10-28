#ifndef PAINT_H
#define PAINT_H
#include <tonc.h>

void paint_open(int costume_slot);
int  paint_loop(void);    // returns 1 while active
extern u8 g_costumes[4][32*32];
#endif
