#include <tonc.h>
#include "gfx.h"
#include "input.h"
#include "sound.h"
#include "menu.h"

void menu_draw_run(void);

int main(void) {
    irq_init(NULL);
    irq_add(II_VBLANK, NULL);

    gfx_init();
    snd_init();
    menu_init();
    menu_enter(SCR_BOOT);

    for (;;) {
        menu_input();
        if (g_screen == SCR_RUN) menu_draw_run();
        else menu_draw();

        // Copy the OBJ buffer to hardware OAM
        oam_copy(oam_mem, obj_buffer, 128);
        gfx_flip();
    }
}
