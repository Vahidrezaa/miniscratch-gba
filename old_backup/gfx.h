#ifndef GFX_H
#define GFX_H
#include <tonc.h>

#define SCREEN_W 240
#define SCREEN_H 160

extern OBJ_ATTR obj_buffer[128];
extern u8* g_back;

void gfx_init(void);
void gfx_flip(void);
void gfx_clear(u8 col);
void gfx_pixel(int x, int y, u8 col);
void gfx_rect(int x, int y, int w, int h, u8 col);
void gfx_fill_rect(int x, int y, int w, int h, u8 col);
void gfx_hline(int x, int y, int w, u8 col);
void gfx_vline(int x, int y, int h, u8 col);
void gfx_line(int x0, int y0, int x1, int y1, u8 col);
void gfx_circle(int cx, int cy, int r, u8 col);
void gfx_fill_circle(int cx, int cy, int r, u8 col);

void load_assets(void); // تابع جدید برای بارگذاری اسپرایت‌ها

#endif
