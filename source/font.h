#ifndef FONT_H
#define FONT_H

#include "gfx.h"

// Function prototypes
void font_draw_char(int x, int y, char c, u8 col);
void font_draw(int x, int y, const char* s, u8 col);
void font_draw_bg(int x, int y, const char* s, u8 fg, u8 bg);
int  font_width(const char* s);

#endif // FONT_H
