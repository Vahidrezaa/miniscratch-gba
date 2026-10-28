#include "nano_ui.h"
#include "block_engine.h"
#include "vm.h"
#include "script.h"

// Layout Constants
#define LEFT_TABS_W 12
#define LEFT_PALETTE_W 58
#define SANDBOX_X 70
#define SANDBOX_W 100
#define RIGHT_X 170
#define RIGHT_W 70
#define STAGE_SIZE 64

extern const UiScene scene_menu;
extern const UiScene scene_workspace;
extern const UiScene scene_about;

static void on_back(UiWidget *self, void *user) {
    (void)self; (void)user;
    ui_scene_switch(&scene_menu);
}

// --- MAIN MENU ---
static void on_start(UiWidget *self, void *user) { (void)self; (void)user; ui_scene_switch(&scene_workspace); }
static void on_about(UiWidget *self, void *user) { (void)self; (void)user; ui_scene_switch(&scene_about); }

static void menu_enter(void) {
    UiWidget *panel = ui_panel_add(50, 40, 140, 110, "MINISCRATCH");
    UiRect c = ui_panel_content_rect(panel);
    int bx = c.x + (c.w - 100) / 2;
    ui_button_add(bx, c.y + 10, 100, UI_BUTTON_H, "New Project", on_start, NULL);
    ui_button_add(bx, c.y + 10 + UI_BUTTON_H + 6, 100, UI_BUTTON_H, "About", on_about, NULL);
    ui_button_add(bx, c.y + 10 + (UI_BUTTON_H + 6) * 2, 100, UI_BUTTON_H, "Settings", on_about, NULL);
    ui_cursor_set_pos(bx + 50, c.y + 10 + UI_BUTTON_H / 2);
}

static void menu_draw_bg(void) {
    ui_draw_dot_grid(UI_COL_BG_MID);
    ui_draw_text(70, 10, "MINISCRATCH", UI_COL_ACCENT_ORANGE_LIGHT);
}

const UiScene scene_menu = { .on_enter = menu_enter, .on_draw_bg = menu_draw_bg };

// --- WORKSPACE ---
static UiSprite *s_cat_sprite;

static void ws_enter(void) {
    block_engine_init();
    ui_cursor_set_pos(80, 80); // Start cursor in sandbox

    // Add actual Back button widget
    ui_button_add(RIGHT_X + 4, 140, 62, 14, "Back", on_back, NULL);

    // Setup Cat Sprite (Placeholder for now)
    s_cat_sprite = ui_sprite_alloc();
    if (s_cat_sprite) {
        s_cat_sprite->attr0 = (30 & 0xFF) | 0x0000;
        s_cat_sprite->attr1 = (180 & 0x1FF) | 0x4000; // 16x16 size
        s_cat_sprite->attr2 = 0 | (0 << 12);
    }
}

static void ws_update(void) {
    block_engine_update(); // Handle dragging and snapping

    // Update cat sprite pos (static for now)
    if (s_cat_sprite) {
        s_cat_sprite->attr0 = (30 & 0xFF) | 0x0000;
        s_cat_sprite->attr1 = (180 & 0x1FF) | 0x4000;
    }
}

static void ws_draw_bg(void) {
    // Draw background panels
    ui_fill_rect(0, 0, LEFT_TABS_W, UI_SCREEN_H, UI_COL_PANEL_DARK); // Tabs
    ui_fill_rect(LEFT_TABS_W, 0, LEFT_PALETTE_W, UI_SCREEN_H, UI_COL_BG_DARK); // Palette
    ui_fill_rect(SANDBOX_X, 0, SANDBOX_W, UI_SCREEN_H, UI_COL_PANEL); // Sandbox
    ui_fill_rect(RIGHT_X, 0, RIGHT_W, UI_SCREEN_H, UI_COL_BG_DARK); // Right panel

    // Sandbox grid
    for(int i=SANDBOX_X; i<SANDBOX_X+SANDBOX_W; i+=8)
        for(int j=0; j<UI_SCREEN_H; j+=8)
            ui_fill_rect(i, j, 1, 1, UI_COL_BG_MID);

    // Draw Tabs (Motion, Looks, Control, Events)
    ui_fill_rect(2, 20, 8, 20, CAT_MOTION);
    ui_fill_rect(2, 45, 8, 20, CAT_LOOKS);
    ui_fill_rect(2, 70, 8, 20, CAT_CONTROL);
    ui_fill_rect(2, 95, 8, 20, CAT_EVENTS);

    // Draw Palette blocks
    block_engine_draw_palette();

    // Draw Sandbox blocks
    block_engine_draw_sandbox();

    // Draw Stage box
    ui_draw_frame(RIGHT_X + 3, 3, STAGE_SIZE, STAGE_SIZE, UI_COL_TEXT_DIM);

    // Draw Sprite List
    ui_draw_text(RIGHT_X + 4, 72, "Sprites:", UI_COL_TEXT);
    ui_fill_rect(RIGHT_X + 4, 82, 62, 14, UI_COL_ACCENT_BLUE);
    ui_draw_text(RIGHT_X + 6, 85, "Cat", UI_COL_TEXT);

    // Add Sprite button
    ui_fill_rect(RIGHT_X + 4, 100, 62, 14, UI_COL_PANEL_LIGHT);
    ui_draw_text(RIGHT_X + 20, 103, "+ Add", UI_COL_TEXT);
    
    // NOTE: The Back button is drawn automatically by NanoUI because we added it in ws_enter()!
}

const UiScene scene_workspace = { .on_enter = ws_enter, .on_update = ws_update, .on_draw_bg = ws_draw_bg };

// --- ABOUT ---
static void about_enter(void) {
    ui_button_add(70, 120, 100, UI_BUTTON_H, "Back", on_back, NULL);
    ui_cursor_set_pos(120, 120 + UI_BUTTON_H / 2);
}
static void about_draw_bg(void) {
    ui_draw_dot_grid(UI_COL_BG_MID);
    ui_draw_text(50, 40, "MiniScratch GBA", UI_COL_TEXT);
}
const UiScene scene_about = { .on_enter = about_enter, .on_draw_bg = about_draw_bg };

// --- MAIN ---
int main(void) {
    ui_init();
    ui_scene_switch(&scene_menu);
    for (;;) {
        ui_update();
        ui_present();
    }
    return 0;
}
