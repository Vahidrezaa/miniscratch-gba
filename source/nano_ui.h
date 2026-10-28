/*
 * nano_ui.h - NanoUI: a small retained-mode UI + graphics engine for the GBA
 *
 * ARCHITECTURE
 * ------------
 *  Video      Mode 0. BG0 is a 30x20 tile map where tile N always shows
 *             canvas tile N ("tile-linear framebuffer"). The engine draws
 *             pixel-exact into a 4bpp canvas in EWRAM and uploads only the
 *             tile rows that changed during VBlank. This gives pixel-precise
 *             text, borders and shadows while staying in tiled mode.
 *  Sprites    The mouse-style cursor is a hardware OBJ. A shadow OAM buffer
 *             is copied to real OAM during VBlank. A small sprite/tile pool
 *             is provided for future use and wiped on every scene change.
 *  Widgets    Panel, Button, List are retained: created in a scene's
 *             on_enter(), hit-tested every frame, redrawn only when dirty.
 *  Scenes     A tiny state machine. Switching is deferred to the next frame
 *             boundary and wipes widgets, canvas, sprite pool and OBJ tiles.
 *
 * FRAME LOOP
 *      ui_init();
 *      ui_scene_switch(&my_scene);
 *      for (;;) { ui_update(); ui_present(); }
 */
#ifndef NANO_UI_H
#define NANO_UI_H

#include <tonc.h>
#include <stdbool.h>
#include "font_data.h"

/* ------------------------------------------------------------------------ */
/* Display geometry                                                         */
/* ------------------------------------------------------------------------ */
#define UI_SCREEN_W          240
#define UI_SCREEN_H          160
#define UI_TILE_PX           16
#define UI_MAP_W             (UI_SCREEN_W / UI_TILE_PX)     /* 30 */
#define UI_MAP_H             (UI_SCREEN_H / UI_TILE_PX)     /* 20 */
#define UI_CANVAS_TILES      (UI_MAP_W * UI_MAP_H)          /* 600 */
#define UI_TILE_WORDS        16      /* u32 per 4bpp tile: one word per pixel row */

/* VRAM layout for BG0 (the screenblock sits in the last 2KB of BG VRAM) */
#define UI_BG_CBB            0      /* tile data base (charblock index) */
#define UI_BG_SBB            31     /* map data base (screenblock index) */
#define UI_BG_PRIO           1
#define UI_OBJ_PRIO          0      /* sprites above the background */
#define UI_SE_STRIDE         32     /* entries per row in a 32x32 screenblock */

/* ------------------------------------------------------------------------ */
/* OAM / OBJ VRAM layout                                                    */
/* ------------------------------------------------------------------------ */
#define UI_OAM_COUNT         128
#define UI_SPRITE_CURSOR     0      /* lowest slot = drawn in front of the rest */
#define UI_SPRITE_POOL_FIRST 1
#define UI_OBJ_TILES_TOTAL   512    /* 32KB of OBJ VRAM in Mode 0 (8bpp)        */
#define UI_OBJ_TILE_CURSOR   0
#define UI_CURSOR_SIZE       16     /* cursor sprite is 16x16 px                */
#define UI_CURSOR_TILES      4
#define UI_OBJ_TILE_POOL_FIRST (UI_OBJ_TILE_CURSOR + UI_CURSOR_TILES)
#define UI_INVALID_TILE      0xFFFF

/* OAM attribute bits (hardware-defined) */
#define UI_ATTR0_Y_MASK      0x00FF
#define UI_ATTR0_HIDE        0x0200
#define UI_ATTR0_SHAPE_SQ    0x0000
#define UI_ATTR0_8BPP        0x2000
#define UI_ATTR1_X_MASK      0x01FF
#define UI_ATTR1_SIZE_16     0x4000
#define UI_ATTR2_PRIO_SHIFT  10
#define UI_ATTR2_PAL_SHIFT   12

/* ------------------------------------------------------------------------ */
/* Palette: 256 colors (8bpp) - General UI colors are set here.             */
/* Indices 17-29 are reserved for the application (e.g., Scratch colors).   */
/* ------------------------------------------------------------------------ */
typedef u8 UiColor;
enum
{
    UI_COL_BLACK = 0,            /* index 0: drop shadow / backdrop       */
    UI_COL_BG_DARK,              /* dark slate: scene background          */
    UI_COL_BG_MID,               /* background grid dots                  */
    UI_COL_PANEL,                /* slate grey: panel body                */
    UI_COL_PANEL_LIGHT,          /* button face, raised surfaces          */
    UI_COL_PANEL_DARK,           /* sunken surfaces, shaded bevel edges   */
    UI_COL_BORDER_OUTER,         /* 1px outer frame                       */
    UI_COL_BORDER_INNER,         /* 1px inner highlight                   */
    UI_COL_TEXT,                 /* white                                 */
    UI_COL_TEXT_DIM,             /* secondary text                        */
    UI_COL_ACCENT_BLUE,
    UI_COL_ACCENT_BLUE_LIGHT,
    UI_COL_ACCENT_BLUE_DARK,
    UI_COL_ACCENT_ORANGE,
    UI_COL_ACCENT_ORANGE_LIGHT,
    UI_COL_ACCENT_ORANGE_DARK,
    UI_COL_WHITE,                /* index 16: pure white                  */
    
    /* Indices 17-29 are RESERVED for application colors (e.g., Scratch). */
    /* They are left uninitialized by the engine and must be filled by    */
    /* the application via ui_palette_set_rgb().                          */
    
    UI_CUR_OUTLINE = 30,         /* index 30: cursor outline color        */
    UI_CUR_FILL,                 /* index 31: cursor fill color           */
    
    UI_PAL_SIZE = 256            /* Total palette size for 8bpp           */
};
void ui_palette_set_rgb(int index, int r, int g, int b);

/* ------------------------------------------------------------------------ */
/* Widget / typography metrics                                              */
/* ------------------------------------------------------------------------ */
#define UI_MAX_TEXT_SCALE    4
#define UI_TEXT_LINE_GAP     2      /* extra px between wrapped lines (x scale) */
#define UI_TITLE_H           12     /* panel title bar height                   */
#define UI_PANEL_BORDER      2      /* outer frame + inner bevel                */
#define UI_PANEL_PAD         4      /* padding between border/title and content */
#define UI_MIN_WIDGET_SIZE   8
#define UI_BUTTON_H          18     /* recommended button height                */
#define UI_LIST_ITEM_H       12
#define UI_LIST_BORDER       2
#define UI_LIST_TEXT_PAD     6
#define UI_LIST_MARKER_W     3
#define UI_LIST_SCROLLBAR_W  5
#define UI_LIST_THUMB_MIN_H  6
#define UI_LIST_SCROLL_PERIOD 6     /* frames per row while edge-scrolling      */
#define UI_GRID_STEP_TILES   2      /* spacing of background dots (in tiles)    */
#define UI_MAX_WIDGETS       32

/* ------------------------------------------------------------------------ */
/* Cursor tuning (8.8 fixed point, pixels per frame)                        */
/* ------------------------------------------------------------------------ */
#define UI_FP_SHIFT          8
#define UI_CURSOR_SPEED_BASE 0x0180     /* 1.5 px/frame on first press      */
#define UI_CURSOR_SPEED_STEP 0x0020     /* acceleration per held frame      */
#define UI_CURSOR_SPEED_SLOW 0x0300     /* top speed, 3 px/frame            */
#define UI_CURSOR_SPEED_FAST 0x0600     /* top speed, 6 px/frame            */
#define UI_CURSOR_HOLD_MAX   255
#define UI_CURSOR_DIAG_NUM   181        /* 256/sqrt(2): diagonal correction */
#define UI_CURSOR_DIAG_SHIFT 8
#define UI_CURSOR_HOT_X      0          /* hotspot offset inside the sprite */
#define UI_CURSOR_HOT_Y      0
#define UI_CURSOR_PAL_NORMAL 0
#define UI_CURSOR_PAL_HOVER  1

typedef enum { UI_CURSOR_SLOW = 0, UI_CURSOR_FAST = 1 } UiCursorSpeed;

/* ------------------------------------------------------------------------ */
/* Input                                                                    */
/* ------------------------------------------------------------------------ */
typedef enum
{
    BTN_IDLE = 0,       /* up, and was up last frame          */
    BTN_PRESSED,        /* went down this frame               */
    BTN_HELD,           /* down this frame and last frame     */
    BTN_RELEASED        /* went up this frame                 */
} UiBtnState;

void       ui_input_poll(void);                 /* called by ui_update()       */
UiBtnState ui_btn_state(u16 key);               /* exact state of ONE key      */
bool       ui_btn_pressed(u16 mask);            /* any key in mask just pressed*/
bool       ui_btn_released(u16 mask);           /* any key in mask just released*/
bool       ui_btn_down(u16 mask);               /* any key in mask pressed/held*/

/* ------------------------------------------------------------------------ */
/* Geometry / hit testing                                                   */
/* ------------------------------------------------------------------------ */
typedef struct UiRect { s16 x, y, w, h; } UiRect;

bool ui_rect_contains(const UiRect *r, int px, int py);
bool ui_rect_overlaps(const UiRect *a, const UiRect *b);

/* ------------------------------------------------------------------------ */
/* Sprites (shadow OAM) and OBJ tile pool, wiped on every scene change      */
/* ------------------------------------------------------------------------ */
typedef struct UiSprite { u16 attr0, attr1, attr2; s16 pad; } UiSprite;

UiSprite *ui_sprite_alloc(void);                       /* NULL when pool is full */
u16       ui_obj_tiles_alloc(u16 count);               /* UI_INVALID_TILE if full */
bool      ui_obj_tiles_upload(u16 first, const u32 *data, u16 tile_count);

/* ------------------------------------------------------------------------ */
/* Immediate-mode drawing (targets the EWRAM canvas, always clipped)        */
/* ------------------------------------------------------------------------ */
void ui_clear(UiColor c);
void ui_fill_rect(int x, int y, int w, int h, UiColor c);
void ui_draw_frame(int x, int y, int w, int h, UiColor c);    /* 1px, rounded corners */
void ui_draw_bevel(int x, int y, int w, int h, UiColor light, UiColor dark);
void ui_draw_dot_grid(UiColor c);
void ui_draw_panel(int x, int y, int w, int h, const char *title);

/* Text: always rendered with a 1px (x scale) black drop shadow.
 * All return the number of lines drawn.                                    */
int  ui_draw_text(int x, int y, const char *str, UiColor c);            /* wraps at screen edge */
int  ui_draw_text_box(int x, int y, int max_w, const char *str, UiColor c); /* wraps at max_w    */
int  ui_draw_text_ex(int x, int y, int max_w, const char *str, UiColor c, int scale);
int  ui_text_width(const char *str, int scale);   /* visible px width, longest line */

/* ------------------------------------------------------------------------ */
/* Widgets                                                                  */
/* ------------------------------------------------------------------------ */
typedef enum { UI_WIDGET_NONE = 0, UI_WIDGET_PANEL, UI_WIDGET_BUTTON, UI_WIDGET_LIST } UiWidgetType;
typedef enum { UI_WS_NORMAL = 0, UI_WS_HOVER, UI_WS_PRESSED, UI_WS_DISABLED } UiWidgetState;

#define UI_WF_VISIBLE        0x01
#define UI_WF_ENABLED        0x02
#define UI_WF_INTERACTIVE    0x04
#define UI_WF_DIRTY          0x08

typedef struct UiWidget UiWidget;
typedef void (*UiClickFn)(UiWidget *self, void *user);

struct UiWidget
{
    u8              type;           /* UiWidgetType  */
    u8              flags;          /* UI_WF_*       */
    u8              state;          /* UiWidgetState */
    UiRect          rect;
    const char     *text;           /* button label / panel title */
    UiClickFn       on_click;
    void           *user;
    /* list-only data */
    const char *const *items;
    s16             item_count;
    s16             scroll;         /* index of the first visible item */
    s16             hover_item;     /* -1 when none                    */
    s16             selected;       /* -1 when none                    */
};

UiWidget *ui_panel_add(int x, int y, int w, int h, const char *title);
UiWidget *ui_button_add(int x, int y, int w, int h, const char *label, UiClickFn on_click, void *user);
UiWidget *ui_list_add(int x, int y, int w, int h, const char *const *items, int count,
                      UiClickFn on_click, void *user);

UiRect    ui_panel_content_rect(const UiWidget *panel);     /* padded inner area */
void      ui_widget_invalidate(UiWidget *w);
void      ui_widget_set_enabled(UiWidget *w, bool enabled);
void      ui_button_set_label(UiWidget *b, const char *label);
int       ui_list_selected(const UiWidget *l);
void      ui_list_scroll_by(UiWidget *l, int delta);
UiWidget *ui_hit_test(int x, int y);                        /* topmost interactive */

/* ------------------------------------------------------------------------ */
/* Cursor                                                                   */
/* ------------------------------------------------------------------------ */
void          ui_cursor_set_pos(int x, int y);
int           ui_cursor_x(void);
int           ui_cursor_y(void);
void          ui_cursor_set_speed(UiCursorSpeed s);
UiCursorSpeed ui_cursor_get_speed(void);

/* ------------------------------------------------------------------------ */
/* Scene manager                                                            */
/* ------------------------------------------------------------------------ */
typedef struct UiScene
{
    void (*on_enter)(void);     /* create widgets, set up state            */
    void (*on_update)(void);    /* per-frame logic (after widget handling) */
    void (*on_draw_bg)(void);   /* decor BELOW widgets (called on redraw)  */
    void (*on_draw_fg)(void);   /* decor ABOVE widgets (called on redraw)  */
    void (*on_exit)(void);
} UiScene;

void           ui_scene_switch(const UiScene *next);        /* deferred */
const UiScene *ui_scene_current(void);
void           ui_redraw_all(void);

/* ------------------------------------------------------------------------ */
/* Engine                                                                   */
/* ------------------------------------------------------------------------ */
void ui_init(void);
void ui_update(void);       /* input, cursor, widgets, scene logic, drawing  */
void ui_present(void);      /* wait for VBlank, upload canvas + OAM          */

#endif /* NANO_UI_H */
