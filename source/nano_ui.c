/*
 * nano_ui.c - NanoUI engine implementation. See nano_ui.h for the overview.
 *
 * Memory-safety rules used throughout:
 *   - Every canvas write goes through canvas_plot()/canvas_hspan(), which clip
 *     to the screen, so the tile index can never leave s_canvas[].
 *   - VRAM/OAM are only written by ui_init()/ui_present()/reset code, with
 *     sizes derived from the same #defines that size the source buffers.
 *   - Pools (widgets, sprites, OBJ tiles) check capacity before allocating.
 */
#include "nano_ui.h"
#include <string.h>

#ifndef EWRAM_BSS
#define EWRAM_BSS __attribute__((section(".sbss")))
#endif

/* ------------------------------------------------------------------------ */
/* Hardware map and register values                                         */
/* ------------------------------------------------------------------------ */
#define NUI_VRAM_BASE         0x06000000u
#define NUI_OBJ_VRAM_BASE     0x06010000u
#define NUI_PAL_BG_BASE       0x05000000u
#define NUI_PAL_OBJ_BASE      0x05000200u
#define NUI_OAM_BASE          0x07000000u
#define NUI_CHARBLOCK_BYTES   0x4000u
#define NUI_SCREENBLOCK_BYTES 0x0800u
#define NUI_SCREENBLOCK_ENTRIES 1024
#define NUI_TILE_BYTES        32u
#define NUI_OAM_ENTRY_WORDS   2         /* 8 bytes per OAM entry */
#define NUI_PAL_BANK_ENTRIES  16
#define NUI_DISPCNT_RUN       (DCNT_MODE0 | DCNT_BG0 | DCNT_OBJ | DCNT_OBJ_1D)

#define NUI_BG_TILES          ((u32 *)(NUI_VRAM_BASE + UI_BG_CBB * NUI_CHARBLOCK_BYTES))
#define NUI_BG_MAP            ((volatile u16 *)(NUI_VRAM_BASE + UI_BG_SBB * NUI_SCREENBLOCK_BYTES))
#define NUI_OBJ_TILES         ((u32 *)NUI_OBJ_VRAM_BASE)

#define NUI_RGB(r, g, b)      ((u16)((r) | ((g) << 5) | ((b) << 10)))
#define NUI_NIBBLE_FILL       0x11111111u   /* replicate a 4bpp colour across a word */
#define NUI_PIXELS_PER_WORD   8
#define NUI_BITS_PER_PIXEL    4
#define NUI_ALL_ROWS_MASK     ((1u << UI_MAP_H) - 1u)

/* Compile-time proof that nothing overlaps or overflows in VRAM. */
_Static_assert(UI_CANVAS_TILES <= 1024, "BG tile ids are 10 bits");
_Static_assert((UI_BG_CBB * NUI_CHARBLOCK_BYTES + UI_CANVAS_TILES * NUI_TILE_BYTES)
               <= (UI_BG_SBB * NUI_SCREENBLOCK_BYTES), "canvas tiles overlap the map");
_Static_assert(UI_MAP_H <= 32, "dirty-row mask is a u32");
_Static_assert(UI_PAL_SIZE == NUI_PAL_BANK_ENTRIES, "palette must fit one bank");

/* ------------------------------------------------------------------------ */
/* Palettes                                                                 */
/* ------------------------------------------------------------------------ */
static const u16 k_ui_palette[UI_PAL_SIZE] =
{
    NUI_RGB( 1,  1,  3),  /* BLACK            */
    NUI_RGB( 4,  6,  9),  /* BG_DARK          */
    NUI_RGB( 7,  9, 13),  /* BG_MID           */
    NUI_RGB(10, 13, 17),  /* PANEL            */
    NUI_RGB(14, 17, 22),  /* PANEL_LIGHT      */
    NUI_RGB( 6,  8, 12),  /* PANEL_DARK       */
    NUI_RGB( 2,  2,  4),  /* BORDER_OUTER     */
    NUI_RGB(19, 23, 28),  /* BORDER_INNER     */
    NUI_RGB(31, 31, 31),  /* TEXT             */
    NUI_RGB(20, 23, 27),  /* TEXT_DIM         */
    NUI_RGB( 5, 15, 28),  /* ACCENT_BLUE      */
    NUI_RGB(12, 22, 31),  /* ACCENT_BLUE_LIGHT*/
    NUI_RGB( 2,  7, 16),  /* ACCENT_BLUE_DARK */
    NUI_RGB(30, 17,  3),  /* ACCENT_ORANGE    */
    NUI_RGB(31, 25,  9),  /* ACCENT_ORANGE_LIGHT */
    NUI_RGB(22, 10,  1)   /* ACCENT_ORANGE_DARK  */
};

/* Cursor OBJ palette indices and the two palette banks (normal / hover). */
enum { CUR_TRANSPARENT = 0, CUR_OUTLINE = 1, CUR_FILL = 2, CUR_PAL_USED = 3 };
static const u16 k_cursor_pal[2][CUR_PAL_USED] =
{
    { 0, NUI_RGB(1, 1, 3), NUI_RGB(31, 31, 31) },   /* white arrow  */
    { 0, NUI_RGB(1, 1, 3), NUI_RGB(30, 17,  3) }    /* orange arrow */
};

/* 16x16 arrow: '.' transparent, 'k' outline, 'w' fill. Hotspot = top-left. */
static const char *const k_cursor_art[UI_CURSOR_SIZE] =
{
    "k...............",
    "kk..............",
    "kwk.............",
    "kwwk............",
    "kwwwk...........",
    "kwwwwk..........",
    "kwwwwwk.........",
    "kwwwwwwk........",
    "kwwwwwwwk.......",
    "kwwwwwwwwk......",
    "kwwwwwkkkkk.....",
    "kwwkwwk.........",
    "kwk.kwwk........",
    "kk..kwwk........",
    "k....kwwk.......",
    ".....kkk........"
};

/* ------------------------------------------------------------------------ */
/* Engine state                                                             */
/* ------------------------------------------------------------------------ */
static u32 s_canvas[UI_CANVAS_TILES][UI_TILE_WORDS] EWRAM_BSS __attribute__((aligned(4)));
static u32 s_dirty_rows;                         /* bit n = tile row n needs upload */
static UiSprite s_oam[UI_OAM_COUNT] __attribute__((aligned(4)));
static int  s_sprite_next;
static u16  s_obj_tile_next;
static bool s_display_on;

static UiWidget s_widgets[UI_MAX_WIDGETS];
static int      s_widget_count;
static UiWidget *s_hover;
static UiWidget *s_pressed;
static u8       s_scroll_timer;

static u16 s_keys_cur, s_keys_prev;

typedef struct { s32 fx, fy; int x, y; int hold; UiCursorSpeed speed; } UiCursorState;
static UiCursorState s_cur;

static const UiScene *s_scene_cur;
static const UiScene *s_scene_pending;

/* ------------------------------------------------------------------------ */
/* Input                                                                    */
/* ------------------------------------------------------------------------ */
void ui_input_poll(void)
{
    s_keys_prev = s_keys_cur;
    s_keys_cur  = (u16)(~REG_KEYINPUT & KEY_MASK);   /* hardware is active-low */
}

UiBtnState ui_btn_state(u16 key)
{
    bool now  = (s_keys_cur  & key) != 0;
    bool prev = (s_keys_prev & key) != 0;

    if (now)  return prev ? BTN_HELD : BTN_PRESSED;
    return prev ? BTN_RELEASED : BTN_IDLE;
}

bool ui_btn_pressed(u16 mask)  { return (s_keys_cur & (u16)~s_keys_prev & mask) != 0; }
bool ui_btn_released(u16 mask) { return (s_keys_prev & (u16)~s_keys_cur & mask) != 0; }
bool ui_btn_down(u16 mask)     { return (s_keys_cur & mask) != 0; }

/* ------------------------------------------------------------------------ */
/* Geometry                                                                 */
/* ------------------------------------------------------------------------ */
bool ui_rect_contains(const UiRect *r, int px, int py)
{
    return px >= r->x && py >= r->y && px < r->x + r->w && py < r->y + r->h;
}

bool ui_rect_overlaps(const UiRect *a, const UiRect *b)
{
    return a->x < b->x + b->w && b->x < a->x + a->w &&
           a->y < b->y + b->h && b->y < a->y + a->h;
}

/* ------------------------------------------------------------------------ */
/* Canvas primitives                                                        */
/* ------------------------------------------------------------------------ */

/* Flag the tile rows covering pixel rows [y0, y1] for upload. */
static void mark_rows(int y0, int y1)
{
    if (y0 < 0) y0 = 0;
    if (y1 > UI_SCREEN_H - 1) y1 = UI_SCREEN_H - 1;
    if (y1 < y0) return;

    u32 hi = (1u << ((y1 / UI_TILE_PX) + 1)) - 1u;
    u32 lo = (1u << (y0 / UI_TILE_PX)) - 1u;
    s_dirty_rows |= (hi & ~lo);
}

/* Single pixel, clipped. Tile layout: tile = (y/8)*30 + x/8, one word per row,
 * one nibble per pixel (leftmost pixel in the lowest nibble). */
static inline void canvas_plot(int x, int y, UiColor c)
{
    if ((unsigned)x >= UI_SCREEN_W || (unsigned)y >= UI_SCREEN_H)
        return;

    u32 *word  = &s_canvas[(y >> 3) * UI_MAP_W + (x >> 3)][y & 7];
    u32  shift = (u32)(x & 7) * NUI_BITS_PER_PIXEL;
    *word = (*word & ~(0xFu << shift)) | ((u32)c << shift);
}

/* Horizontal span [x0, x1) on pixel row y, one masked write per tile. */
static void canvas_hspan(int x0, int x1, int y, UiColor c)
{
    if ((unsigned)y >= UI_SCREEN_H) return;
    if (x0 < 0) x0 = 0;
    if (x1 > UI_SCREEN_W) x1 = UI_SCREEN_W;
    if (x0 >= x1) return;

    u32 pat = (u32)c * NUI_NIBBLE_FILL;
    int tx0 = x0 >> 3;
    int tx1 = (x1 - 1) >> 3;
    u32 *row = &s_canvas[(y >> 3) * UI_MAP_W + tx0][y & 7];

    for (int tx = tx0; tx <= tx1; tx++, row += UI_TILE_WORDS)
    {
        int a = (tx == tx0) ? (x0 & 7) : 0;
        int b = (tx == tx1) ? ((x1 - 1) & 7) : 7;
        u32 mask = (0xFFFFFFFFu << (a * NUI_BITS_PER_PIXEL)) &
                   (0xFFFFFFFFu >> ((7 - b) * NUI_BITS_PER_PIXEL));
        *row = (*row & ~mask) | (pat & mask);
    }
}

void ui_clear(UiColor c)
{
    memset32(s_canvas, (u32)c * NUI_NIBBLE_FILL, UI_CANVAS_TILES * UI_TILE_WORDS);
    s_dirty_rows = NUI_ALL_ROWS_MASK;
}

void ui_fill_rect(int x, int y, int w, int h, UiColor c)
{
    if (w <= 0 || h <= 0) return;

    int x1 = x + w, y1 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > UI_SCREEN_W) x1 = UI_SCREEN_W;
    if (y1 > UI_SCREEN_H) y1 = UI_SCREEN_H;
    if (x >= x1 || y >= y1) return;

    for (int row = y; row < y1; row++)
        canvas_hspan(x, x1, row, c);
    mark_rows(y, y1 - 1);
}

/* 1px outline; the four corner pixels are skipped for a rounded look. */
void ui_draw_frame(int x, int y, int w, int h, UiColor c)
{
    if (w < 3 || h < 3) return;
    ui_fill_rect(x + 1,     y,         w - 2, 1,     c);
    ui_fill_rect(x + 1,     y + h - 1, w - 2, 1,     c);
    ui_fill_rect(x,         y + 1,     1,     h - 2, c);
    ui_fill_rect(x + w - 1, y + 1,     1,     h - 2, c);
}

/* 1px bevel: light on top/left, dark on bottom/right. */
void ui_draw_bevel(int x, int y, int w, int h, UiColor light, UiColor dark)
{
    if (w < 2 || h < 2) return;
    ui_fill_rect(x,         y + h - 1, w, 1, dark);
    ui_fill_rect(x + w - 1, y,         1, h, dark);
    ui_fill_rect(x,         y,         w, 1, light);
    ui_fill_rect(x,         y,         1, h, light);
}

/* Blueprint-style dots: one pixel at the corner of every Nth tile. */
void ui_draw_dot_grid(UiColor c)
{
    for (int ty = 0; ty < UI_MAP_H; ty += UI_GRID_STEP_TILES)
        for (int tx = 0; tx < UI_MAP_W; tx += UI_GRID_STEP_TILES)
        {
            u32 *word = &s_canvas[ty * UI_MAP_W + tx][0];
            *word = (*word & ~0xFu) | (u32)c;
        }
    s_dirty_rows = NUI_ALL_ROWS_MASK;
}

/* ------------------------------------------------------------------------ */
/* Text engine                                                              */
/* ------------------------------------------------------------------------ */
static inline int clamp_scale(int scale)
{
    if (scale < 1) return 1;
    if (scale > UI_MAX_TEXT_SCALE) return UI_MAX_TEXT_SCALE;
    return scale;
}

static void fill_block(int x, int y, int size, UiColor c)
{
    if (size == 1) { canvas_plot(x, y, c); return; }
    for (int j = 0; j < size; j++)
        canvas_hspan(x, x + size, y + j, c);
}

/* One glyph: pass 0 = black shadow offset by (scale, scale), pass 1 = ink. */
static void draw_glyph(int x, int y, char ch, UiColor col, int scale)
{
    if (ch == ' ') return;

    const u8 *g = font_get_glyph(ch);
    for (int pass = 0; pass < 2; pass++)
    {
        UiColor c   = (pass == 0) ? (UiColor)UI_COL_BLACK : col;
        int     off = (pass == 0) ? scale : 0;

        for (int r = 0; r < FONT_GLYPH_H; r++)
        {
            u8 bits = g[r];
            if (!bits) continue;
            for (int cx = 0; cx < FONT_GLYPH_W; cx++)
                if (bits & (FONT_ROW_MSB >> cx))
                    fill_block(x + cx * scale + off, y + r * scale + off, scale, c);
        }
    }
}

static void draw_text_n(int x, int y, const char *s, int n, UiColor col, int scale)
{
    for (int i = 0; i < n; i++)
        draw_glyph(x + i * FONT_CELL_W * scale, y, s[i], col, scale);
}

/* Number of characters that fit in max_w pixels (visible width of n chars
 * is n*cell - trailing spacing, hence the + FONT_TRAIL_PX*scale). */
static int text_cols(int max_w, int scale)
{
    int cols = (max_w + FONT_TRAIL_PX * scale) / (FONT_CELL_W * scale);
    return (cols < 1) ? 1 : cols;
}

int ui_text_width(const char *str, int scale)
{
    if (!str) return 0;
    scale = clamp_scale(scale);

    int best = 0, cur = 0;
    for (const char *p = str; ; p++)
    {
        if (*p == '\n' || *p == '\0')
        {
            if (cur > best) best = cur;
            cur = 0;
            if (*p == '\0') break;
        }
        else cur++;
    }
    if (best == 0) return 0;
    return (best - 1) * FONT_CELL_W * scale + (FONT_GLYPH_W + 1) * scale;
}

int ui_draw_text_ex(int x, int y, int max_w, const char *str, UiColor color, int scale)
{
    if (!str || max_w <= 0) return 0;
    scale = clamp_scale(scale);

    int cols  = text_cols(max_w, scale);
    int pitch = (FONT_CELL_H + UI_TEXT_LINE_GAP) * scale;
    int lines = 0;
    int cy    = y;
    const char *p = str;

    while (*p && cy < UI_SCREEN_H)
    {
        int len = 0;
        int used = font_wrap_line(p, cols, &len);
        if (used <= 0) break;                       /* defensive: never spin */

        draw_text_n(x, cy, p, len, color, scale);
        mark_rows(cy, cy + FONT_CELL_H * scale - 1);

        p  += used;
        cy += pitch;
        lines++;
    }
    return lines;
}

int ui_draw_text_box(int x, int y, int max_w, const char *str, UiColor c)
{
    return ui_draw_text_ex(x, y, max_w, str, c, 1);
}

int ui_draw_text(int x, int y, const char *str, UiColor c)
{
    return ui_draw_text_ex(x, y, UI_SCREEN_W - x, str, c, 1);
}

/* ------------------------------------------------------------------------ */
/* Panels                                                                   */
/* ------------------------------------------------------------------------ */
void ui_draw_panel(int x, int y, int w, int h, const char *title)
{
    int min_h = UI_PANEL_BORDER * 2 + (title ? UI_TITLE_H : 0) + 1;
    if (w < UI_MIN_WIDGET_SIZE || h < min_h) return;

    /* body, then 1px inner bevel, then 1px outer frame (rounded corners) */
    ui_fill_rect(x + 1, y + 1, w - 2, h - 2, UI_COL_PANEL);
    ui_draw_bevel(x + 1, y + 1, w - 2, h - 2, UI_COL_BORDER_INNER, UI_COL_PANEL_DARK);
    ui_draw_frame(x, y, w, h, UI_COL_BORDER_OUTER);

    if (title)
    {
        int bx = x + UI_PANEL_BORDER;
        int by = y + UI_PANEL_BORDER;
        int bw = w - 2 * UI_PANEL_BORDER;

        ui_fill_rect(bx, by, bw, UI_TITLE_H, UI_COL_ACCENT_BLUE);
        ui_fill_rect(bx, by, bw, 1, UI_COL_ACCENT_BLUE_LIGHT);
        ui_fill_rect(bx, by + UI_TITLE_H - 1, bw, 1, UI_COL_ACCENT_BLUE_DARK);

        int cols = text_cols(bw - 2 * UI_PANEL_PAD, 1);
        int n    = (int)strlen(title);
        if (n > cols) n = cols;
        draw_text_n(bx + UI_PANEL_PAD, by + (UI_TITLE_H - FONT_CELL_H) / 2,
                    title, n, UI_COL_TEXT, 1);
    }
}

UiRect ui_panel_content_rect(const UiWidget *panel)
{
    UiRect r = {0, 0, 0, 0};
    if (!panel) return r;

    int top = UI_PANEL_BORDER + (panel->text ? UI_TITLE_H : 0) + UI_PANEL_PAD;
    int off = UI_PANEL_BORDER + UI_PANEL_PAD;

    r.x = (s16)(panel->rect.x + off);
    r.y = (s16)(panel->rect.y + top);
    r.w = (s16)(panel->rect.w - 2 * off);
    r.h = (s16)(panel->rect.h - top - off);
    return r;
}

/* ------------------------------------------------------------------------ */
/* Sprite / OBJ tile pools                                                  */
/* ------------------------------------------------------------------------ */
UiSprite *ui_sprite_alloc(void)
{
    if (s_sprite_next >= UI_OAM_COUNT) return NULL;

    UiSprite *s = &s_oam[s_sprite_next++];
    s->attr0 = UI_ATTR0_HIDE;
    s->attr1 = 0;
    s->attr2 = 0;
    return s;
}

u16 ui_obj_tiles_alloc(u16 count)
{
    if (count == 0 || (u32)s_obj_tile_next + count > UI_OBJ_TILES_TOTAL)
        return UI_INVALID_TILE;

    u16 first = s_obj_tile_next;
    s_obj_tile_next = (u16)(s_obj_tile_next + count);
    return first;
}

bool ui_obj_tiles_upload(u16 first, const u32 *data, u16 tile_count)
{
    if (!data || first < UI_OBJ_TILE_POOL_FIRST || first >= s_obj_tile_next ||
        (u32)first + tile_count > s_obj_tile_next)
        return false;                               /* only allocated tiles */

    memcpy32(&NUI_OBJ_TILES[first * UI_TILE_WORDS], data, tile_count * UI_TILE_WORDS);
    return true;
}

static void sprites_hide_pool(void)
{
    for (int i = UI_SPRITE_POOL_FIRST; i < UI_OAM_COUNT; i++)
    {
        s_oam[i].attr0 = UI_ATTR0_HIDE;
        s_oam[i].attr1 = 0;
        s_oam[i].attr2 = 0;
    }
    s_sprite_next = UI_SPRITE_POOL_FIRST;
}

static void obj_tiles_wipe_pool(void)
{
    if (s_obj_tile_next > UI_OBJ_TILE_POOL_FIRST)
        memset32(&NUI_OBJ_TILES[UI_OBJ_TILE_POOL_FIRST * UI_TILE_WORDS], 0,
                 (s_obj_tile_next - UI_OBJ_TILE_POOL_FIRST) * UI_TILE_WORDS);
    s_obj_tile_next = UI_OBJ_TILE_POOL_FIRST;
}

/* ------------------------------------------------------------------------ */
/* List helpers                                                             */
/* ------------------------------------------------------------------------ */
static int list_rows(const UiWidget *w)
{
    int r = (w->rect.h - 2 * UI_LIST_BORDER) / UI_LIST_ITEM_H;
    return (r < 1) ? 1 : r;
}

static int list_max_scroll(const UiWidget *w)
{
    int m = w->item_count - list_rows(w);
    return (m > 0) ? m : 0;
}

static int list_row_at(const UiWidget *w, int py)
{
    int rel = py - (w->rect.y + UI_LIST_BORDER);
    if (rel < 0) return -1;
    int row = rel / UI_LIST_ITEM_H;
    return (row < list_rows(w)) ? row : -1;
}

static int list_item_at(const UiWidget *w, int px, int py)
{
    int right = w->rect.x + w->rect.w - UI_LIST_BORDER -
                (list_max_scroll(w) > 0 ? UI_LIST_SCROLLBAR_W : 0);
    if (px < w->rect.x + UI_LIST_BORDER || px >= right) return -1;

    int row = list_row_at(w, py);
    if (row < 0) return -1;

    int idx = w->scroll + row;
    return (idx < w->item_count) ? idx : -1;
}

void ui_list_scroll_by(UiWidget *l, int delta)
{
    if (!l || l->type != UI_WIDGET_LIST) return;

    int s = l->scroll + delta;
    int m = list_max_scroll(l);
    if (s < 0) s = 0;
    if (s > m) s = m;
    if (s != l->scroll)
    {
        l->scroll = (s16)s;
        ui_widget_invalidate(l);
    }
}

int ui_list_selected(const UiWidget *l)
{
    return (l && l->type == UI_WIDGET_LIST) ? l->selected : -1;
}

/* ------------------------------------------------------------------------ */
/* Widget drawing                                                           */
/* ------------------------------------------------------------------------ */
static void draw_button(const UiWidget *b)
{
    const UiRect *r = &b->rect;
    UiColor face, hi, lo, txt = UI_COL_TEXT;
    int off = 0;

    switch (b->state)
    {
    case UI_WS_HOVER:
        face = UI_COL_ACCENT_ORANGE;
        hi   = UI_COL_ACCENT_ORANGE_LIGHT;
        lo   = UI_COL_ACCENT_ORANGE_DARK;
        break;
    case UI_WS_PRESSED:
        face = UI_COL_ACCENT_ORANGE_DARK;
        hi   = UI_COL_ACCENT_ORANGE_DARK;
        lo   = UI_COL_ACCENT_ORANGE;
        off  = 1;                                   /* label sinks 1px */
        break;
    case UI_WS_DISABLED:
        face = UI_COL_PANEL_DARK;
        hi   = UI_COL_PANEL;
        lo   = UI_COL_PANEL_DARK;
        txt  = UI_COL_TEXT_DIM;
        break;
    default:
        face = UI_COL_PANEL_LIGHT;
        hi   = UI_COL_BORDER_INNER;
        lo   = UI_COL_PANEL;
        break;
    }

    ui_fill_rect(r->x + 1, r->y + 1, r->w - 2, r->h - 2, face);
    ui_draw_bevel(r->x + 1, r->y + 1, r->w - 2, r->h - 2, hi, lo);
    ui_draw_frame(r->x, r->y, r->w, r->h, UI_COL_BORDER_OUTER);

    if (b->text)
    {
        int cols = text_cols(r->w - 2 * UI_PANEL_BORDER, 1);
        int n    = (int)strlen(b->text);
        if (n > cols) n = cols;

        int tw = ui_text_width(b->text, 1);
        if (tw > r->w) tw = r->w;
        draw_text_n(r->x + (r->w - tw) / 2 + off,
                    r->y + (r->h - FONT_CELL_H) / 2 + off,
                    b->text, n, txt, 1);
    }
}

static void draw_list(const UiWidget *l)
{
    const UiRect *r = &l->rect;
    int rows      = list_rows(l);
    int scrollable = list_max_scroll(l) > 0;
    int x0        = r->x + UI_LIST_BORDER;
    int y0        = r->y + UI_LIST_BORDER;
    int inner_w   = r->w - 2 * UI_LIST_BORDER;
    int items_w   = inner_w - (scrollable ? UI_LIST_SCROLLBAR_W : 0);

    /* sunken frame */
    ui_fill_rect(r->x + 1, r->y + 1, r->w - 2, r->h - 2, UI_COL_PANEL_DARK);
    ui_draw_bevel(r->x + 1, r->y + 1, r->w - 2, r->h - 2, UI_COL_BORDER_OUTER, UI_COL_PANEL);
    ui_draw_frame(r->x, r->y, r->w, r->h, UI_COL_BORDER_OUTER);

    /* rows */
    int cols = text_cols(items_w - UI_LIST_TEXT_PAD - 1, 1);
    for (int i = 0; i < rows; i++)
    {
        int idx = l->scroll + i;
        if (idx >= l->item_count) break;

        int ry = y0 + i * UI_LIST_ITEM_H;
        if (idx == l->hover_item)
            ui_fill_rect(x0, ry, items_w, UI_LIST_ITEM_H, UI_COL_ACCENT_BLUE);
        if (idx == l->selected)
            ui_fill_rect(x0, ry, UI_LIST_MARKER_W, UI_LIST_ITEM_H, UI_COL_ACCENT_ORANGE);

        const char *label = l->items[idx];
        if (label)
        {
            int n = (int)strlen(label);
            if (n > cols) n = cols;
            draw_text_n(x0 + UI_LIST_TEXT_PAD, ry + (UI_LIST_ITEM_H - FONT_CELL_H) / 2,
                        label, n, UI_COL_TEXT, 1);
        }
    }

    /* scrollbar */
    if (scrollable)
    {
        int tx = x0 + items_w;
        int ty = y0;
        int th = r->h - 2 * UI_LIST_BORDER;
        int thumb_h = th * rows / l->item_count;
        if (thumb_h < UI_LIST_THUMB_MIN_H) thumb_h = UI_LIST_THUMB_MIN_H;
        int travel  = th - thumb_h;
        int thumb_y = ty + travel * l->scroll / list_max_scroll(l);

        ui_fill_rect(tx, ty, UI_LIST_SCROLLBAR_W, th, UI_COL_PANEL);
        ui_fill_rect(tx + 1, thumb_y, UI_LIST_SCROLLBAR_W - 2, thumb_h, UI_COL_ACCENT_ORANGE);
        ui_fill_rect(tx + 1, thumb_y, UI_LIST_SCROLLBAR_W - 2, 1, UI_COL_ACCENT_ORANGE_LIGHT);
    }
}

static void widget_draw(UiWidget *w)
{
    if (w->flags & UI_WF_VISIBLE)
    {
        switch (w->type)
        {
        case UI_WIDGET_PANEL:  ui_draw_panel(w->rect.x, w->rect.y, w->rect.w, w->rect.h, w->text); break;
        case UI_WIDGET_BUTTON: draw_button(w); break;
        case UI_WIDGET_LIST:   draw_list(w);   break;
        default: break;
        }
    }
    w->flags &= (u8)~UI_WF_DIRTY;
}

/* ------------------------------------------------------------------------ */
/* Widget creation / management                                             */
/* ------------------------------------------------------------------------ */
static UiWidget *widget_alloc(UiWidgetType type, int x, int y, int w, int h, u8 extra_flags)
{
    if (s_widget_count >= UI_MAX_WIDGETS) return NULL;
    if (w < UI_MIN_WIDGET_SIZE || h < UI_MIN_WIDGET_SIZE) return NULL;

    UiWidget *wd = &s_widgets[s_widget_count++];
    memset(wd, 0, sizeof(*wd));
    wd->type       = (u8)type;
    wd->flags      = (u8)(UI_WF_VISIBLE | UI_WF_ENABLED | UI_WF_DIRTY | extra_flags);
    wd->state      = UI_WS_NORMAL;
    wd->rect.x     = (s16)x;
    wd->rect.y     = (s16)y;
    wd->rect.w     = (s16)w;
    wd->rect.h     = (s16)h;
    wd->hover_item = -1;
    wd->selected   = -1;
    return wd;
}

UiWidget *ui_panel_add(int x, int y, int w, int h, const char *title)
{
    UiWidget *p = widget_alloc(UI_WIDGET_PANEL, x, y, w, h, 0);
    if (p) p->text = title;
    return p;
}

UiWidget *ui_button_add(int x, int y, int w, int h, const char *label, UiClickFn on_click, void *user)
{
    UiWidget *b = widget_alloc(UI_WIDGET_BUTTON, x, y, w, h, UI_WF_INTERACTIVE);
    if (b) { b->text = label; b->on_click = on_click; b->user = user; }
    return b;
}

UiWidget *ui_list_add(int x, int y, int w, int h, const char *const *items, int count,
                      UiClickFn on_click, void *user)
{
    if (!items || count < 0) return NULL;

    UiWidget *l = widget_alloc(UI_WIDGET_LIST, x, y, w, h, UI_WF_INTERACTIVE);
    if (l)
    {
        l->items      = items;
        l->item_count = (s16)count;
        l->on_click   = on_click;
        l->user       = user;
    }
    return l;
}

void ui_widget_invalidate(UiWidget *w)
{
    if (w) w->flags |= UI_WF_DIRTY;
}

void ui_widget_set_enabled(UiWidget *w, bool enabled)
{
    if (!w) return;
    if (enabled) w->flags |= UI_WF_ENABLED; else w->flags &= (u8)~UI_WF_ENABLED;
    ui_widget_invalidate(w);
}

void ui_button_set_label(UiWidget *b, const char *label)
{
    if (!b || b->type != UI_WIDGET_BUTTON) return;
    b->text = label;
    ui_widget_invalidate(b);
}

UiWidget *ui_hit_test(int x, int y)
{
    const u8 need = UI_WF_VISIBLE | UI_WF_ENABLED | UI_WF_INTERACTIVE;

    for (int i = s_widget_count - 1; i >= 0; i--)       /* topmost first */
    {
        UiWidget *w = &s_widgets[i];
        if ((w->flags & need) == need && ui_rect_contains(&w->rect, x, y))
            return w;
    }
    return NULL;
}

static UiWidgetState compute_state(const UiWidget *w)
{
    if (!(w->flags & UI_WF_ENABLED)) return UI_WS_DISABLED;
    if (w == s_hover)
        return (w == s_pressed && ui_btn_down(KEY_A)) ? UI_WS_PRESSED : UI_WS_HOVER;
    return UI_WS_NORMAL;
}

/* Re-evaluate hover/pressed visuals of every interactive widget. */
static void widgets_refresh_states(void)
{
    for (int i = 0; i < s_widget_count; i++)
    {
        UiWidget *w = &s_widgets[i];
        if (!(w->flags & UI_WF_INTERACTIVE)) continue;

        UiWidgetState st = compute_state(w);
        if (st != w->state) { w->state = (u8)st; ui_widget_invalidate(w); }

        if (w->type == UI_WIDGET_LIST)
        {
            int item = (w == s_hover) ? list_item_at(w, s_cur.x, s_cur.y) : -1;
            if (item != w->hover_item) { w->hover_item = (s16)item; ui_widget_invalidate(w); }
        }
    }
}

static void widgets_handle_clicks(void)
{
    UiWidget *hov = s_hover;

    /* L / R page the list under the cursor */
    if (hov && hov->type == UI_WIDGET_LIST)
    {
        if (ui_btn_pressed(KEY_L)) ui_list_scroll_by(hov, -list_rows(hov));
        if (ui_btn_pressed(KEY_R)) ui_list_scroll_by(hov,  list_rows(hov));
    }

    if (ui_btn_state(KEY_A) == BTN_PRESSED && hov)
    {
        if (hov->type == UI_WIDGET_LIST)
        {
            int item = list_item_at(hov, s_cur.x, s_cur.y);
            if (item >= 0)
            {
                hov->selected = (s16)item;
                ui_widget_invalidate(hov);
                s_pressed = hov;
            }
        }
        else
            s_pressed = hov;
    }

    if (ui_btn_state(KEY_A) == BTN_RELEASED)
    {
        UiWidget *p = s_pressed;
        s_pressed = NULL;
        if (p && p == hov && p->on_click)               /* click = press + release on same widget */
            p->on_click(p, p->user);
    }
}

/* ------------------------------------------------------------------------ */
/* Cursor                                                                   */
/* ------------------------------------------------------------------------ */
static void cursor_sync_int(void)
{
    s_cur.x = (int)(s_cur.fx >> UI_FP_SHIFT);
    s_cur.y = (int)(s_cur.fy >> UI_FP_SHIFT);
}

void ui_cursor_set_pos(int x, int y)
{
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > UI_SCREEN_W - 1) x = UI_SCREEN_W - 1;
    if (y > UI_SCREEN_H - 1) y = UI_SCREEN_H - 1;
    s_cur.fx = (s32)x << UI_FP_SHIFT;
    s_cur.fy = (s32)y << UI_FP_SHIFT;
    cursor_sync_int();
}

int ui_cursor_x(void) { return s_cur.x; }
int ui_cursor_y(void) { return s_cur.y; }
void ui_cursor_set_speed(UiCursorSpeed s) { s_cur.speed = s; }
UiCursorSpeed ui_cursor_get_speed(void)   { return s_cur.speed; }

/* Edge scrolling: while the cursor sits on the first/last visible row of a
 * list and the D-Pad pushes further that way, the list scrolls instead and
 * the cursor stays put. Returns true when vertical motion is consumed. */
static bool list_edge_scroll(UiWidget *w, int dir)
{
    int rows = list_rows(w);
    int row  = (s_cur.y - (w->rect.y + UI_LIST_BORDER)) / UI_LIST_ITEM_H;
    if (row < 0) row = 0;
    if (row >= rows) row = rows - 1;

    bool can = (dir < 0 && row == 0 && w->scroll > 0) ||
               (dir > 0 && row == rows - 1 && w->scroll < list_max_scroll(w));
    if (!can) return false;

    if (s_scroll_timer == 0) ui_list_scroll_by(w, dir);
    if (++s_scroll_timer >= UI_LIST_SCROLL_PERIOD) s_scroll_timer = 0;
    return true;
}

static void cursor_update(void)
{
    int dx = (ui_btn_down(KEY_RIGHT) ? 1 : 0) - (ui_btn_down(KEY_LEFT) ? 1 : 0);
    int dy = (ui_btn_down(KEY_DOWN)  ? 1 : 0) - (ui_btn_down(KEY_UP)   ? 1 : 0);

    if (dx || dy) { if (s_cur.hold < UI_CURSOR_HOLD_MAX) s_cur.hold++; }
    else          { s_cur.hold = 0; }

    if (dy != 0)
    {
        UiWidget *w = ui_hit_test(s_cur.x, s_cur.y);
        if (w && w->type == UI_WIDGET_LIST && list_edge_scroll(w, dy))
            dy = 0;
    }
    else
        s_scroll_timer = 0;

    int max_speed = (s_cur.speed == UI_CURSOR_FAST) ? UI_CURSOR_SPEED_FAST : UI_CURSOR_SPEED_SLOW;
    int speed = UI_CURSOR_SPEED_BASE + s_cur.hold * UI_CURSOR_SPEED_STEP;
    if (speed > max_speed) speed = max_speed;
    if (dx && dy) speed = (speed * UI_CURSOR_DIAG_NUM) >> UI_CURSOR_DIAG_SHIFT;

    s_cur.fx += dx * speed;
    s_cur.fy += dy * speed;

    const s32 max_x = (s32)(UI_SCREEN_W - 1) << UI_FP_SHIFT;
    const s32 max_y = (s32)(UI_SCREEN_H - 1) << UI_FP_SHIFT;
    if (s_cur.fx < 0) s_cur.fx = 0;
    if (s_cur.fy < 0) s_cur.fy = 0;
    if (s_cur.fx > max_x) s_cur.fx = max_x;
    if (s_cur.fy > max_y) s_cur.fy = max_y;
    cursor_sync_int();
}

static void cursor_sprite_update(void)
{
    UiSprite *s = &s_oam[UI_SPRITE_CURSOR];
    int sx = s_cur.x - UI_CURSOR_HOT_X;
    int sy = s_cur.y - UI_CURSOR_HOT_Y;
    u16 pal = s_hover ? UI_CURSOR_PAL_HOVER : UI_CURSOR_PAL_NORMAL;

    s->attr0 = (u16)((sy & UI_ATTR0_Y_MASK) | UI_ATTR0_SHAPE_SQ);
    s->attr1 = (u16)((sx & UI_ATTR1_X_MASK) | UI_ATTR1_SIZE_16);
    s->attr2 = (u16)(UI_OBJ_TILE_CURSOR | (UI_OBJ_PRIO << UI_ATTR2_PRIO_SHIFT) |
                     (pal << UI_ATTR2_PAL_SHIFT));
}

/* Rasterise the ASCII art into 4 tiles (1D mapping: TL, TR, BL, BR). */
static void cursor_upload_tiles(void)
{
    u32 tiles[UI_CURSOR_TILES][UI_TILE_WORDS];
    memset(tiles, 0, sizeof(tiles));

    for (int y = 0; y < UI_CURSOR_SIZE; y++)
        for (int x = 0; x < UI_CURSOR_SIZE; x++)
        {
            char ch = k_cursor_art[y][x];
            if (ch == '\0') break;

            u32 idx = (ch == 'k') ? CUR_OUTLINE : (ch == 'w') ? CUR_FILL : CUR_TRANSPARENT;
            int tile = (y >> 3) * (UI_CURSOR_SIZE / UI_TILE_PX) + (x >> 3);
            tiles[tile][y & 7] |= idx << ((x & 7) * NUI_BITS_PER_PIXEL);
        }

    memcpy32(&NUI_OBJ_TILES[UI_OBJ_TILE_CURSOR * UI_TILE_WORDS], tiles,
             UI_CURSOR_TILES * UI_TILE_WORDS);
}

/* ------------------------------------------------------------------------ */
/* Scene manager                                                            */
/* ------------------------------------------------------------------------ */

/* Everything a scene may have allocated is released here. */
static void reset_scene_resources(void)
{
    memset(s_widgets, 0, sizeof(s_widgets));
    s_widget_count = 0;
    s_hover = NULL;
    s_pressed = NULL;
    s_scroll_timer = 0;

    sprites_hide_pool();
    obj_tiles_wipe_pool();
    ui_clear(UI_COL_BG_DARK);
}

void ui_redraw_all(void)
{
    ui_clear(UI_COL_BG_DARK);
    if (s_scene_cur && s_scene_cur->on_draw_bg) s_scene_cur->on_draw_bg();
    for (int i = 0; i < s_widget_count; i++) widget_draw(&s_widgets[i]);
    if (s_scene_cur && s_scene_cur->on_draw_fg) s_scene_cur->on_draw_fg();
    s_dirty_rows = NUI_ALL_ROWS_MASK;
}

void ui_scene_switch(const UiScene *next)
{
    if (next) s_scene_pending = next;
}

const UiScene *ui_scene_current(void) { return s_scene_cur; }

static void scene_apply_pending(void)
{
    if (!s_scene_pending) return;

    const UiScene *next = s_scene_pending;
    s_scene_pending = NULL;

    if (s_scene_cur && s_scene_cur->on_exit) s_scene_cur->on_exit();
    reset_scene_resources();
    s_scene_cur = next;
    if (next->on_enter) next->on_enter();
    ui_redraw_all();
}

/* ------------------------------------------------------------------------ */
/* Engine                                                                   */
/* ------------------------------------------------------------------------ */
void ui_init(void)
{
    REG_DISPCNT = DCNT_BLANK;                       /* hide garbage while setting up */

    REG_BG0CNT  = BG_CBB(UI_BG_CBB) | BG_SBB(UI_BG_SBB) | BG_4BPP |
                  BG_REG_32x32 | BG_PRIO(UI_BG_PRIO);
    REG_BG0HOFS = 0;
    REG_BG0VOFS = 0;

    /* palettes */
    volatile u16 *pal_bg  = (volatile u16 *)NUI_PAL_BG_BASE;
    volatile u16 *pal_obj = (volatile u16 *)NUI_PAL_OBJ_BASE;
    for (int i = 0; i < UI_PAL_SIZE; i++)
        pal_bg[i] = k_ui_palette[i];
    for (int bank = 0; bank < 2; bank++)
    {
        for (int i = 0; i < NUI_PAL_BANK_ENTRIES; i++)
            pal_obj[bank * NUI_PAL_BANK_ENTRIES + i] = 0;
        for (int i = 0; i < CUR_PAL_USED; i++)
            pal_obj[bank * NUI_PAL_BANK_ENTRIES + i] = k_cursor_pal[bank][i];
    }

    /* map: visible cell (tx,ty) shows canvas tile ty*30+tx, the rest is blank */
    volatile u16 *map = NUI_BG_MAP;
    for (int i = 0; i < NUI_SCREENBLOCK_ENTRIES; i++) map[i] = 0;
    for (int ty = 0; ty < UI_MAP_H; ty++)
        for (int tx = 0; tx < UI_MAP_W; tx++)
            map[ty * UI_SE_STRIDE + tx] = (u16)(ty * UI_MAP_W + tx);

    /* sprites */
    cursor_upload_tiles();
    sprites_hide_pool();
    s_oam[UI_SPRITE_CURSOR].attr0 = UI_ATTR0_HIDE;
    s_obj_tile_next = UI_OBJ_TILE_POOL_FIRST;

    /* state */
    memset(&s_cur, 0, sizeof(s_cur));
    s_cur.speed = UI_CURSOR_SLOW;
    ui_cursor_set_pos(UI_SCREEN_W / 2, UI_SCREEN_H / 2);
    s_keys_cur = s_keys_prev = 0;
    s_scene_cur = NULL;
    s_scene_pending = NULL;
    s_display_on = false;
    reset_scene_resources();
}

void ui_update(void)
{
    scene_apply_pending();
    ui_input_poll();

    cursor_update();
    s_hover = ui_hit_test(s_cur.x, s_cur.y);
    widgets_handle_clicks();
    widgets_refresh_states();

    if (s_scene_cur && s_scene_cur->on_update) s_scene_cur->on_update();

    for (int i = 0; i < s_widget_count; i++)
        if (s_widgets[i].flags & UI_WF_DIRTY)
            widget_draw(&s_widgets[i]);

    cursor_sprite_update();
}

void ui_present(void)
{
    /* wait for the start of VBlank */
    while (REG_VCOUNT >= UI_SCREEN_H) { }
    while (REG_VCOUNT <  UI_SCREEN_H) { }

    if (s_dirty_rows)
    {
        for (int ty = 0; ty < UI_MAP_H; ty++)
            if (s_dirty_rows & (1u << ty))
                memcpy32(&NUI_BG_TILES[ty * UI_MAP_W * UI_TILE_WORDS],
                         &s_canvas[ty * UI_MAP_W][0],
                         UI_MAP_W * UI_TILE_WORDS);
        s_dirty_rows = 0;
    }

    memcpy32((void *)NUI_OAM_BASE, s_oam, UI_OAM_COUNT * NUI_OAM_ENTRY_WORDS);

    if (!s_display_on)
    {
        REG_DISPCNT = NUI_DISPCNT_RUN;
        s_display_on = true;
    }
}
