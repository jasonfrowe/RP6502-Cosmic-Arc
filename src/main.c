#include <rp6502.h>
#include <stdio.h>
#include <stdbool.h>
#include "contants.h"
#include "mothership.h"

unsigned COSMICARC_CONFIG;

static void init_graphics(void)
{
    // Select a 320x240 canvas
    if (xreg_vga_canvas(1) < 0) {
        puts("xreg_vga_canvas failed");
        return;
    }

    COSMICARC_CONFIG = SPRITE_DATA_END;
    init_mothership(COSMICARC_CONFIG);

    // Mode 4 args: MODE, OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    if (xreg_vga_mode(4, 0, COSMICARC_CONFIG, MOTHERSHIP_PART_COUNT, 0, 0, 0) < 0) {
        puts("xreg_vga_mode failed");
        return;
    }

}


uint8_t vsync_last = 0;

int main(void)
{
    puts("Hello from Cosmic Arc!");

    init_graphics();

    while (true) {
        // Main game loop
        // 1. SYNC
        if (RIA.vsync == vsync_last) continue;
        vsync_last = RIA.vsync;

        update_mothership();

    }
    return 0;
}
