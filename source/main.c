/*
 * main.c - MiniScratch for GBA (Powered by NanoUI)
 */
#include "nano_ui.h"

/* ---- layout constants (no magic numbers) -------------------------------- */
#define TITLE_SCALE        2
#define TITLE_Y            8
#define TITLE_RULE_Y       27
#define TITLE_RULE_H       2
#define TITLE_RULE_W       160
#define SUBTITLE_Y         33
#define HINT_Y             148

#define MENU_PANEL_W       140
#define MENU_PANEL_H       110
#define MENU_PANEL_Y       40
#define MENU_BTN_W         116
#define MENU_BTN_GAP       6
#define MENU_BTN_COUNT     4

#define WS_MARGIN          4
#define WS_PANEL_H         152
#define WS_LEFT_W          64
#define WS_CENTER_W        80
#define WS_RIGHT_W         84
#define WS_CENTER_X        (WS_MARGIN + WS_LEFT_W + 2)
#define WS_RIGHT_X         (WS_CENTER_X + WS_CENTER_W + 2)

#define CENTER_X(w)        ((UI_SCREEN_W - (w)) / 2)

/* ---- scenes ------------------------------------------------------------- */
extern const UiScene scene_menu;
extern const UiScene scene_workspace;
extern const UiScene scene_about;

/* ======================================================================== */
/* Shared                                                                   */
/* ======================================================================== */
static void on_back(UiWidget *self, void *user)
{
    (void)self; (void)user;
    ui_scene_switch(&scene_menu);
}

static void draw_hint_centered(const char *text)
{
    ui_draw_text(CENTER_X(ui_text_width(text, 1)), HINT_Y, text, UI_COL_TEXT_DIM);
}

/* ======================================================================== */
/* Main menu                                                                */
/* ======================================================================== */
static void on_start(UiWidget *self, void *user)
{
    (void)self; (void)user;
    ui_scene_switch(&scene_workspace);
}

static void on_about(UiWidget *self, void *user)
{
    (void)self; (void)user;
    ui_scene_switch(&scene_about);
}

static void menu_enter(void)
{
    UiWidget *panel = ui_panel_add(CENTER_X(MENU_PANEL_W), MENU_PANEL_Y,
                                   MENU_PANEL_W, MENU_PANEL_H, "MINISCRATCH");
    UiRect c = ui_panel_content_rect(panel);

    int total = MENU_BTN_COUNT * UI_BUTTON_H + (MENU_BTN_COUNT - 1) * MENU_BTN_GAP;
    int bx = c.x + (c.w - MENU_BTN_W) / 2;
    int by = c.y + (c.h - total) / 2;
    int step = UI_BUTTON_H + MENU_BTN_GAP;

    ui_button_add(bx, by,            MENU_BTN_W, UI_BUTTON_H, "New Project", on_start, NULL);
    ui_button_add(bx, by + step,     MENU_BTN_W, UI_BUTTON_H, "Continue",    on_start, NULL);
    ui_button_add(bx, by + 2 * step, MENU_BTN_W, UI_BUTTON_H, "About",       on_about, NULL);
    ui_button_add(bx, by + 3 * step, MENU_BTN_W, UI_BUTTON_H, "Settings",    on_about, NULL);

    /* Start with cursor on the first button */
    ui_cursor_set_pos(bx + MENU_BTN_W / 2, by + UI_BUTTON_H / 2);
}

static void menu_draw_bg(void)
{
    static const char title[]    = "MINISCRATCH";
    static const char subtitle[] = "Visual Programming for GBA";

    ui_draw_dot_grid(UI_COL_BG_MID);

    ui_draw_text_ex(CENTER_X(ui_text_width(title, TITLE_SCALE)), TITLE_Y,
                    UI_SCREEN_W, title, UI_COL_ACCENT_ORANGE_LIGHT, TITLE_SCALE);

    ui_fill_rect(CENTER_X(TITLE_RULE_W), TITLE_RULE_Y, TITLE_RULE_W, TITLE_RULE_H,
                 UI_COL_ACCENT_BLUE);
    ui_fill_rect(CENTER_X(TITLE_RULE_W), TITLE_RULE_Y, TITLE_RULE_W, 1,
                 UI_COL_ACCENT_BLUE_LIGHT);

    ui_draw_text(CENTER_X(ui_text_width(subtitle, 1)), SUBTITLE_Y, subtitle, UI_COL_TEXT_DIM);
}

static void menu_draw_fg(void)
{
    draw_hint_centered("D-Pad: Move   A: Select");
}

const UiScene scene_menu =
{
    .on_enter   = menu_enter,
    .on_update  = NULL,
    .on_draw_bg = menu_draw_bg,
    .on_draw_fg = menu_draw_fg,
    .on_exit    = NULL
};

/* ======================================================================== */
/* Workspace                                                                */
/* ======================================================================== */
static const char *const k_blocks[] =
{
    "Move 10", "Turn 15", "Say Hello", "Wait 1s", 
    "Repeat 10", "Forever", "If Var=0", "Set Var", 
    "Change Var", "Next Costume", "Stop"
};
#define BLOCK_COUNT ((int)(sizeof(k_blocks) / sizeof(k_blocks[0])))

static const char *const k_script[] =
{
    "Move 10", "Wait 1s", "Say Hello"
};
#define SCRIPT_COUNT ((int)(sizeof(k_script) / sizeof(k_script[0])))

static UiWidget *s_list_blocks;
static UiWidget *s_list_script;

static void on_block_click(UiWidget *self, void *user)
{
    (void)user;
    int sel = ui_list_selected(self);
    if (sel >= 0) {
        /* In a real app, we would append to the script here */
    }
}

static void ws_enter(void)
{
    /* Left Panel: Blocks */
    UiWidget *left = ui_panel_add(WS_MARGIN, WS_MARGIN, WS_LEFT_W, WS_PANEL_H, "BLOCKS");
    UiRect lc = ui_panel_content_rect(left);
    s_list_blocks = ui_list_add(lc.x, lc.y, lc.w, lc.h, k_blocks, BLOCK_COUNT, on_block_click, NULL);

    /* Center Panel: Script */
    UiWidget *center = ui_panel_add(WS_CENTER_X, WS_MARGIN, WS_CENTER_W, WS_PANEL_H, "SCRIPT");
    UiRect cc = ui_panel_content_rect(center);
    s_list_script = ui_list_add(cc.x, cc.y, cc.w, cc.h, k_script, SCRIPT_COUNT, NULL, NULL);

    /* Right Panel: Stage */
    UiWidget *right = ui_panel_add(WS_RIGHT_X, WS_MARGIN, WS_RIGHT_W, WS_PANEL_H, "STAGE");
    UiRect rc = ui_panel_content_rect(right);
    
    /* Add a Back button in the stage panel */
    ui_button_add(rc.x + 4, rc.y + rc.h - UI_BUTTON_H - 4, rc.w - 8, UI_BUTTON_H, "Back", on_back, NULL);
    
    /* Set cursor to the blocks list */
    ui_cursor_set_pos(lc.x + lc.w / 2, lc.y + 10);
}

static void ws_draw_bg(void) 
{ 
    ui_draw_dot_grid(UI_COL_BG_MID); 
}

static void ws_draw_fg(void)
{
    UiRect rc = ui_panel_content_rect(ui_hit_test(WS_RIGHT_X, WS_MARGIN)); // Hack to get right panel rect
    /* Draw variables in the stage panel */
    ui_draw_text(WS_RIGHT_X + 6, WS_MARGIN + 20, "Var 0: 0", UI_COL_TEXT);
    ui_draw_text(WS_RIGHT_X + 6, WS_MARGIN + 32, "Var 1: 0", UI_COL_TEXT);
    ui_draw_text(WS_RIGHT_X + 6, WS_MARGIN + 44, "Var 2: 0", UI_COL_TEXT);
}

const UiScene scene_workspace =
{
    .on_enter   = ws_enter,
    .on_update  = NULL,
    .on_draw_bg = ws_draw_bg,
    .on_draw_fg = ws_draw_fg,
    .on_exit    = NULL
};

/* ======================================================================== */
/* About                                                                    */
/* ======================================================================== */
static UiRect s_about;

static void about_enter(void)
{
    UiWidget *p = ui_panel_add(24, 16, 192, 112, "ABOUT");
    s_about = ui_panel_content_rect(p);
    ui_button_add(CENTER_X(100), 134, 100, UI_BUTTON_H, "Back", on_back, NULL);
    ui_cursor_set_pos(CENTER_X(100) + 50, 134 + UI_BUTTON_H / 2);
}

static void about_draw_bg(void) { ui_draw_dot_grid(UI_COL_BG_MID); }

static void about_draw_fg(void)
{
    ui_draw_text_box(s_about.x, s_about.y, s_about.w,
        "MiniScratch for GBA.\n\nA tiny visual programming tool. Built with the NanoUI Engine.\n\nPress B to return.",
        UI_COL_TEXT);
}

const UiScene scene_about =
{
    .on_enter   = about_enter,
    .on_update  = NULL,
    .on_draw_bg = about_draw_bg,
    .on_draw_fg = about_draw_fg,
    .on_exit    = NULL
};

/* ======================================================================== */
int main(void)
{
    ui_init();
    ui_scene_switch(&scene_menu);

    for (;;)
    {
        ui_update();     /* input, cursor, widgets, scene logic, canvas drawing */
        ui_present();    /* VBlank: upload dirty tile rows + OAM                */
    }
    return 0;
}
