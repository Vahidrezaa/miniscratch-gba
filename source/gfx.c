#include "gfx.h"
#include "cat.h"
#include "green_flag.h"

static u8* page0 = (u8*)0x06000000;
static u8* page1 = (u8*)0x0600A000;
u8* g_back = (u8*)0x0600A000;

OBJ_ATTR obj_buffer[128];

static const u16 pal16[] = {
    0x0000, 0x7FFF, 0x3DEF, 0x2108, 0x0000, 0x7C00, 0x03E0, 0x001F,
    0x7FE0, 0x03FF, 0x7C1F, 0x7BDE, 0x6318, 0x4210, 0x294A, 0x14A5
};

void load_assets(void) {
    // 1. Load palettes into OBJ Palette memory
    memcpy16(&pal_obj_mem[0], catPal, 16);             // Bank 0 for cat
    memcpy16(&pal_obj_mem[16], green_flagPal, 16);     // Bank 1 for green flag

    // 2. Decompress tiles into VRAM
    LZ77UnCompVram(catTiles, &tile_mem[4][0]);         // Cat tiles start at index 0
    LZ77UnCompVram(green_flagTiles, &tile_mem[4][16]); // Flag tiles start at index 16

    // 3. Initialize OAM
    oam_init(obj_buffer, 128);
}

void gfx_init(void) {
    // فعال کردن Mode 4 (Bitmap) به همراه OBJ (اسپرایت‌ها)
    REG_DISPCNT = DCNT_MODE4 | DCNT_BG2 | DCNT_OBJ | DCNT_OBJ_1D;
    
    for (int i = 0; i < 16; i++) pal_bg_mem[i] = pal16[i];
    const u16 paint[16] = {
        0x7FFF,0x0000,0x7C00,0x03E0,0x001F,0x7FE0,0x03FF,0x7C1F,
        0x6318,0x4210,0x39E0,0x7BDE,0x5294,0x294A,0x14A5,0x6B4D
    };
    for (int i = 0; i < 16; i++) pal_bg_mem[16+i] = paint[i];
    
    g_back = page1;
    load_assets(); // فراخوانی بارگذاری اسپرایت‌ها
}

void gfx_flip(void) {
    while (REG_VCOUNT >= 160);
    REG_DISPCNT ^= DCNT_PAGE;
    g_back = (REG_DISPCNT & DCNT_PAGE) ? page0 : page1;
}

void gfx_clear(u8 c) {
    u32 v = (u32)c * 0x01010101u;
    u32* p = (u32*)g_back;
    for (int i = 0; i < (SCREEN_W*SCREEN_H)/4; i++) p[i] = v;
}

void gfx_pixel(int x, int y, u8 c) {
    if ((unsigned)x < SCREEN_W && (unsigned)y < SCREEN_H)
        g_back[y*SCREEN_W + x] = c;
}

void gfx_hline(int x, int y, int w, u8 c) {
    if ((unsigned)y >= SCREEN_H) return;
    if (x < 0) { w += x; x = 0; }
    if (x + w > SCREEN_W) w = SCREEN_W - x;
    if (w <= 0) return;
    u8* p = g_back + y*SCREEN_W + x;
    for (int i = 0; i < w; i++) p[i] = c;
}

void gfx_vline(int x, int y, int h, u8 c) {
    if ((unsigned)x >= SCREEN_W) return;
    if (y < 0) { h += y; y = 0; }
    if (y + h > SCREEN_H) h = SCREEN_H - y;
    if (h <= 0) return;
    u8* p = g_back + y*SCREEN_W + x;
    for (int i = 0; i < h; i++) { *p = c; p += SCREEN_W; }
}

void gfx_rect(int x, int y, int w, int h, u8 c) {
    gfx_hline(x, y, w, c);
    gfx_hline(x, y+h-1, w, c);
    gfx_vline(x, y, h, c);
    gfx_vline(x+w-1, y, h, c);
}

void gfx_fill_rect(int x, int y, int w, int h, u8 c) {
    for (int j = 0; j < h; j++) gfx_hline(x, y+j, w, c);
}

void gfx_line(int x0, int y0, int x1, int y1, u8 c) {
    int dx = x1-x0, dy = y1-y0;
    int sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
    dx = dx < 0 ? -dx : dx; dy = dy < 0 ? -dy : dy;
    int err = (dx > dy ? dx : -dy) / 2, e2;
    for (;;) {
        gfx_pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 <  dy) { err += dx; y0 += sy; }
    }
}

void gfx_fill_circle(int cx, int cy, int r, u8 c) {
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++)
            if (x*x + y*y <= r*r) gfx_pixel(cx+x, cy+y, c);
    }
}

void gfx_circle(int cx, int cy, int r, u8 c) {
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        gfx_pixel(cx+x, cy+y, c); gfx_pixel(cx+y, cy+x, c);
        gfx_pixel(cx-y, cy+x, c); gfx_pixel(cx-x, cy+y, c);
        gfx_pixel(cx-x, cy-y, c); gfx_pixel(cx-y, cy-x, c);
        gfx_pixel(cx+y, cy-x, c); gfx_pixel(cx+x, cy-y, c);
        y++;
        if (err < 0) err += 2*y + 1;
        else { x--; err += 2*(y - x) + 1; }
    }
}
