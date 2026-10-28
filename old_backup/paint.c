#include "paint.h"
#include "gfx.h"
#include "font.h"
#include "input.h"
#include "sound.h"

u8 g_costumes[4][32*32];

#define CANVAS_W 32
#define CANVAS_H 32
#define ZOOM 3
#define CANVAS_OX 40
#define CANVAS_OY 20
#define TOOLBAR_Y 140

static u8 canvas[32*32];
static int slot;
static int cx, cy;
static u8 cur_color = 16;  // paint palette index base
static u8 cur_tool = 0;    // 0 pencil, 1 eraser, 2 fill, 3 rect, 4 circle, 5 line
static int tool_start_x, tool_start_y;
static u8 active = 0;

static const char* tool_names[] = {"PENCIL","ERASE","FILL","RECT","CIRC","LINE"};

void paint_open(int costume_slot) {
    slot = costume_slot;
    for (int i = 0; i < 32*32; i++) canvas[i] = g_costumes[slot][i];
    cx = 16; cy = 16; active = 1;
    cur_tool = 0; cur_color = 16 + 2;
}

static void put(int x, int y, u8 c) {
    if ((unsigned)x < 32 && (unsigned)y < 32) canvas[y*32 + x] = c;
}

static void flood_fill(int x, int y, u8 newc) {
    u8 old = canvas[y*32 + x];
    if (old == newc) return;
    // iterative stack
    int stack[256][2]; int sp = 0;
    stack[sp][0] = x; stack[sp][1] = y; sp++;
    while (sp > 0) {
        sp--;
        int px = stack[sp][0], py = stack[sp][1];
        if ((unsigned)px >= 32 || (unsigned)py >= 32) continue;
        if (canvas[py*32+px] != old) continue;
        canvas[py*32+px] = newc;
        if (sp + 4 < 256) {
            stack[sp][0]=px+1; stack[sp][1]=py; sp++;
            stack[sp][0]=px-1; stack[sp][1]=py; sp++;
            stack[sp][0]=px; stack[sp][1]=py+1; sp++;
            stack[sp][0]=px; stack[sp][1]=py-1; sp++;
        }
    }
}

static void stamp_rect(int x0, int y0, int x1, int y1, u8 c) {
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) put(x, y, c);
}

static void stamp_circle(int cx0, int cy0, int r, u8 c) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x*x + y*y <= r*r) put(cx0+x, cy0+y, c);
}

static void stamp_line(int x0, int y0, int x1, int y1, u8 c) {
    int dx = x1-x0, dy = y1-y0;
    int sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
    dx = dx < 0 ? -dx : dx; dy = dy < 0 ? -dy : dy;
    int err = (dx > dy ? dx : -dy) / 2, e2;
    for (;;) {
        put(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        e2 = err;
        if (e2 > -dx) { err -= dy; x0 += sx; }
        if (e2 <  dy) { err += dx; y0 += sy; }
    }
}

static void draw_canvas(void) {
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++) {
            u8 c = canvas[y*32 + x];
            if (!c) c = 2;  // show transparent as black bg (UI)
            gfx_fill_rect(CANVAS_OX + x*ZOOM, CANVAS_OY + y*ZOOM, ZOOM, ZOOM, c);
        }
    // grid
    for (int x = 0; x <= 32; x += 8)
        gfx_vline(CANVAS_OX + x*ZOOM, CANVAS_OY, 32*ZOOM, 3);
    for (int y = 0; y <= 32; y += 8)
        gfx_hline(CANVAS_OX, CANVAS_OY + y*ZOOM, 32*ZOOM, 3);
    // cursor
    gfx_rect(CANVAS_OX + cx*ZOOM - 1, CANVAS_OY + cy*ZOOM - 1, ZOOM+2, ZOOM+2, 1);
}

static void draw_toolbar(void) {
    gfx_fill_rect(0, TOOLBAR_Y, SCREEN_W, SCREEN_H - TOOLBAR_Y, 3);
    // colors 16..31
    for (int i = 0; i < 16; i++) {
        int x = 4 + i*14;
        gfx_fill_rect(x, TOOLBAR_Y + 2, 12, 12, 16 + i);
        if (16 + i == cur_color) gfx_rect(x-1, TOOLBAR_Y + 1, 14, 14, 1);
    }
    // tools
    for (int i = 0; i < 6; i++) {
        int x = 4 + i*38;
        gfx_fill_rect(x, TOOLBAR_Y + 18, 36, 12, i == cur_tool ? 1 : 3);
        font_draw_text(x + 2, TOOLBAR_Y + 20, tool_names[i], i == cur_tool ? 2 : 1);
    }
    font_draw_text(4, 2, "PAINT  A=OK B=BACK START=SAVE", 1);
}

int paint_loop(void) {
    if (!active) return 0;
    input_update();
    u16 p = input_pressed();
    u16 r = input_repeat(BTN_DIRS);

    if (r & BTN_UP)    { cy = (cy + 31) % 32; snd_move(); }
    if (r & BTN_DOWN)  { cy = (cy + 1) % 32; snd_move(); }
    if (r & BTN_LEFT)  { cx = (cx + 31) % 32; snd_move(); }
    if (r & BTN_RIGHT) { cx = (cx + 1) % 32; snd_move(); }

    if (p & BTN_L) { cur_color = cur_color == 16 ? 31 : cur_color - 1; snd_click(); }
    if (p & BTN_R) { cur_color = cur_color == 31 ? 16 : cur_color + 1; snd_click(); }

    if (p & BTN_SELECT) { cur_tool = (cur_tool + 1) % 6; snd_click(); }

    if (p & BTN_A) {
        if (cur_tool == 0) put(cx, cy, cur_color);
        else if (cur_tool == 1) put(cx, cy, 0);
        else if (cur_tool == 2) flood_fill(cx, cy, cur_color);
        else { tool_start_x = cx; tool_start_y = cy; }
    }
    if ((p & BTN_B)) {
        if (cur_tool >= 3) {
            int x0 = tool_start_x, y0 = tool_start_y;
            if (cur_tool == 3) stamp_rect(x0, y0, cx, cy, cur_color);
            else if (cur_tool == 4) {
                int dx = cx - x0, dy = cy - y0;
                int rr = (dx*dx + dy*dy);
                int rad = 0; while ((rad+1)*(rad+1) <= rr) rad++;
                stamp_circle(x0, y0, rad, cur_color);
            } else stamp_line(x0, y0, cx, cy, cur_color);
            snd_ok();
        } else {
            for (int i = 0; i < 32*32; i++) g_costumes[slot][i] = canvas[i];
            active = 0;
            snd_ok();
        }
    }
    if (p & BTN_START) {
        for (int i = 0; i < 32*32; i++) g_costumes[slot][i] = canvas[i];
        snd_ok();
    }

    gfx_clear(3);
    draw_canvas();
    draw_toolbar();
    return active;
}
