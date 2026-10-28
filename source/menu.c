#include "menu.h"
#include "gfx.h"
#include "font.h"
#include "input.h"
#include "sound.h"
#include "vm.h"
#include "script.h"
#include "blocks.h"
#include "paint.h"
#include "save.h"

Screen g_screen = SCR_BOOT;
int g_paint_slot = 0;

static int menu_sel = 0;
static const char* menu_items[] = {
    "Run Script", "New Project", "Continue", "Paint Costume", "Settings", "About"
};
#define MENU_COUNT 6

static int pal_sel = 0;
static int pal_page = 0;
#define PAL_VISIBLE 8

static u16 boot_timer = 0;

void menu_init(void) {
    vm_reset();
    script_clear();
}

void menu_enter(Screen s) {
    g_screen = s;
    if (s == SCR_BOOT) boot_timer = 0;
    if (s == SCR_MENU) menu_sel = 0;
}

static void draw_boot(void) {
    gfx_clear(3);
    font_draw(SCREEN_W/2 - 40, 100, "MiniScratch", 1);
    font_draw(SCREEN_W/2 - 40, 116, "v1.0  GBA", 2);
    obj_set_pos(&obj_buffer[0], 0, 160);
    obj_set_pos(&obj_buffer[1], 0, 160);
}

static void draw_menu(void) {
    gfx_clear(3);
    font_draw_bg(0, 0, " MINISCRATCH - MENU ", 2, 1);
    
    for (int i = 0; i < MENU_COUNT; i++) {
        u8 fg = i == menu_sel ? 2 : 1;
        u8 bg = i == menu_sel ? 1 : 3;
        gfx_fill_rect(20, 30 + i*20, 200, 16, bg);
        
        if (i > 0) {
            font_draw(24, 34 + i*20, menu_items[i], fg);
        } else {
            font_draw(48, 34 + i*20, menu_items[i], fg);
        }
    }
    font_draw(20, 145, "A=Select  B=Back", 2);

    OBJ_ATTR* flag = &obj_buffer[0];
    obj_set_attr(flag,
        ATTR0_SQUARE | ATTR0_4BPP,
        ATTR1_SIZE_16,
        ATTR2_PALBANK(1) | 16);
    
    obj_set_pos(flag, 24, 32); 
    obj_set_pos(&obj_buffer[1], 0, 160);
}

static void draw_workspace(void) {
    gfx_clear(3);
    font_draw_bg(0, 0, " WORKSPACE ", 2, 1);
    
    for (int i = 0; i < PAL_VISIBLE; i++) {
        int pi = pal_page*PAL_VISIBLE + i;
        if (pi >= PALETTE_COUNT) break;
        u8 col = block_color(g_palette[pi].opcode);
        gfx_fill_rect(2, 12 + i*16, 76, 14, pi == pal_sel ? 1 : col);
        font_draw(4, 14 + i*16, g_palette[pi].name, pi == pal_sel ? 2 : 1);
    }
    
    int start = (g_cursor / 8) * 8;
    for (int i = 0; i < 8 && start + i < g_script_count; i++) {
        int idx = start + i;
        int y = 12 + i*16;
        u8 col = block_color(g_script[idx].opcode);
        gfx_fill_rect(84, y, 100, 14, idx == g_cursor ? 1 : col);
        char buf[24]; int k = 0;
        const char* lbl = block_label(g_script[idx].opcode);
        while (*lbl && k < 12) buf[k++] = *lbl++;
        if (g_script[idx].opcode != OP_FOREVER && g_script[idx].opcode != OP_STOP && g_script[idx].opcode != OP_NEXT_COSTUME) {
            buf[k++] = ' ';
            int v = g_script[idx].arg1;
            char nb[6]; int n = 0;
            if (v < 0) { buf[k++] = '-'; v = -v; }
            if (!v) nb[n++] = '0';
            else { while (v) { nb[n++] = '0' + v%10; v /= 10; } }
            while (n) buf[k++] = nb[--n];
        }
        buf[k] = 0;
        font_draw(86, y+3, buf, idx == g_cursor ? 2 : 1);
    }
    
    for (int i = 0; i < 4; i++) {
        char b[16]; int n = 0;
        b[n++] = 'V'; b[n++] = '0' + i; b[n++] = ':';
        int v = g_vars[i];
        if (v < 0) { b[n++] = '-'; v = -v; }
        char nb[6]; int m = 0;
        if (!v) nb[m++] = '0';
        else while (v) { nb[m++] = '0' + v%10; v /= 10; }
        while (m) b[n++] = nb[--m];
        b[n] = 0;
        font_draw(188, 82 + i*10, b, 1);
    }
    font_draw(0, 148, "A=Add B=Del L/R Pg S=Run SEL=Save", 2);

    vm_draw_cat();
    obj_set_pos(&obj_buffer[0], 0, 160);
}

static void draw_settings(void) {
    gfx_clear(3);
    font_draw_bg(0, 0, " SETTINGS ", 2, 1);
    font_draw(8, 24, "Emulator: mGBA or NanoBoy", 1);
    font_draw(8, 36, "Save Type: SRAM 32KB", 1);
    font_draw(8, 80, "Controls:", 1);
    font_draw(8, 92, "  D-Pad  Navigate", 1);
    font_draw(8, 104,"  A      Confirm", 1);
    font_draw(8, 116,"  B      Cancel", 1);
    font_draw(8, 128,"  L/R    Page/Tab", 1);
    font_draw(8, 140,"  Start  Run / Save", 1);
    obj_set_pos(&obj_buffer[0], 0, 160);
    obj_set_pos(&obj_buffer[1], 0, 160);
}

static void draw_about(void) {
    gfx_clear(3);
    font_draw_bg(0, 0, " ABOUT ", 2, 1);
    font_draw(8, 30, "MiniScratch for GBA", 1);
    font_draw(8, 44, "A tiny visual programming", 2);
    font_draw(8, 56, "tool for the GBA console.", 2);
    font_draw(8, 76, "Blocks, paint, run, save.", 1);
    font_draw(8, 120, "Press B to return", 2);
    obj_set_pos(&obj_buffer[0], 0, 160);
    obj_set_pos(&obj_buffer[1], 0, 160);
}

void menu_draw_run(void) {
    gfx_clear(3);
    for (int i = 0; i < 5; i++) gfx_hline(0, 30 + i*24, SCREEN_W, 2);
    font_draw(0, 0, "RUN  B=Back", 1);
    
    vm_draw_cat();
    obj_set_pos(&obj_buffer[0], 0, 160);
}

void menu_draw(void) {
    switch (g_screen) {
    case SCR_BOOT:      draw_boot(); break;
    case SCR_MENU:      draw_menu(); break;
    case SCR_WORKSPACE: draw_workspace(); break;
    case SCR_SETTINGS:  draw_settings(); break;
    case SCR_ABOUT:     draw_about(); break;
    default: break;
    }
}

static void menu_input_menu(void) {
    u16 p = input_pressed();
    u16 r = input_repeat(BTN_DIRS);
    if (r & BTN_UP)   { menu_sel = (menu_sel + MENU_COUNT-1) % MENU_COUNT; snd_move(); }
    if (r & BTN_DOWN) { menu_sel = (menu_sel + 1) % MENU_COUNT; snd_move(); }
    if (p & BTN_A) {
        snd_ok();
        if (menu_sel == 0) { vm_run(g_script, g_script_count); menu_enter(SCR_RUN); }
        else if (menu_sel == 1) { script_clear(); vm_reset(); menu_enter(SCR_WORKSPACE); }
        else if (menu_sel == 2) menu_enter(SCR_WORKSPACE);
        else if (menu_sel == 3) { g_paint_slot = 0; paint_open(0); menu_enter(SCR_PAINT); }
        else if (menu_sel == 4) menu_enter(SCR_SETTINGS);
        else if (menu_sel == 5) menu_enter(SCR_ABOUT);
    }
    if (p & BTN_B) { snd_click(); menu_enter(SCR_BOOT); }
}

static void menu_input_workspace(void) {
    u16 p = input_pressed();
    u16 r = input_repeat(BTN_DIRS);

    if (p & BTN_START) {
        snd_ok();
        vm_run(g_script, g_script_count);
        menu_enter(SCR_RUN);
        return;
    }
    if (p & BTN_SELECT) {
        SaveData sd = {0};
        sd.magic = SRAM_MAGIC;
        sd.block_count = g_script_count;
        for (int i = 0; i < g_script_count; i++) {
            sd.blocks[i*5+0] = g_script[i].opcode;
            sd.blocks[i*5+1] = g_script[i].arg1 & 0xFF;
            sd.blocks[i*5+2] = (g_script[i].arg1 >> 8) & 0xFF;
            sd.blocks[i*5+3] = g_script[i].arg2 & 0xFF;
            sd.blocks[i*5+4] = (g_script[i].arg2 >> 8) & 0xFF;
        }
        for (int i = 0; i < 8; i++) sd.variables[i] = g_vars[i];
        save_write(&sd);
        snd_ok();
        return;
    }
    if (p & BTN_L) { if (pal_page > 0) pal_page--; pal_sel = pal_page*PAL_VISIBLE; snd_click(); }
    if (p & BTN_R) { if (pal_page*PAL_VISIBLE + PAL_VISIBLE < PALETTE_COUNT) pal_page++; pal_sel = pal_page*PAL_VISIBLE; snd_click(); }
    if (r & BTN_UP)   { if (g_cursor > 0) { g_cursor--; snd_move(); } else if (pal_sel > 0) { pal_sel--; snd_move(); } }
    if (r & BTN_DOWN) { if (g_cursor < g_script_count) { g_cursor++; snd_move(); } else if (pal_sel + 1 < PALETTE_COUNT) { pal_sel++; snd_move(); } }
    
    if (p & BTN_A) {
        if (pal_sel < PALETTE_COUNT) {
            BlockDef d = g_palette[pal_sel];
            s16 a1 = d.arg_min ? d.arg_min : 1;
            if (d.opcode == OP_GOTO) a1 = 120;
            if (d.opcode == OP_POINT) a1 = 90;
            script_add(d.opcode, a1, d.opcode == OP_GOTO ? 80 : 0);
        }
    }
    if (p & BTN_B) {
        if (g_script_count > 0 && g_cursor < g_script_count) { script_remove(g_cursor); snd_click(); }
        else if (g_script_count == 0) menu_enter(SCR_MENU);
    }
}

void menu_input(void) {
    input_update();
    switch (g_screen) {
    case SCR_BOOT:
        if (boot_timer < 120) boot_timer++;
        else if (input_pressed() || boot_timer > 240) { snd_ok(); menu_enter(SCR_MENU); }
        break;
    case SCR_MENU:      menu_input_menu(); break;
    case SCR_WORKSPACE: menu_input_workspace(); break;
    case SCR_PAINT:
        if (!paint_loop()) menu_enter(SCR_MENU);
        break;
    case SCR_RUN:
        vm_step(g_script, g_script_count);
        if ((input_pressed() & BTN_B) || !vm_is_running()) { snd_click(); menu_enter(SCR_WORKSPACE); }
        break;
    case SCR_SETTINGS:
    case SCR_ABOUT:
        if (input_pressed() & BTN_B) { snd_click(); menu_enter(SCR_MENU); }
        break;
    }
}
