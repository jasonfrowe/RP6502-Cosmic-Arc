#include <rp6502.h>
#include <stdio.h>
#include <stdbool.h>
#include "contants.h"
#include "mothership.h"

unsigned COSMICARC_CONFIG;
unsigned STAR_MAP_CONFIG;

static void init_graphics(void)
{
    // Select a 320x240 canvas
    if (xreg_vga_canvas(1) < 0) {
        puts("xreg_vga_canvas failed");
        return;
    }

    uint16_t tile_palette[16] = {
        0x0020,  // Index 0 (Transparent)
        0x3124,
        0xFFFF,
        0x5FFF,
        0x94BF,
        0xFE32,
        0x653B,
        0x9E3D,
        0x37BF,
        0x5733,
        0x35ED,
        0x6CA6,
        0x2B69,
        0x226A,
        0x39E6,
        0x71E7,
    };

    RIA.addr0 = PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 16; i++) {
        RIA.rw0 = tile_palette[i] & 0xFF;
        RIA.rw0 = tile_palette[i] >> 8;
    }

    COSMICARC_CONFIG = SPRITE_DATA_END;
    init_mothership(COSMICARC_CONFIG);

    // Mode 4 args: MODE, OPTIONS, CONFIG, LENGTH, PLANE, BEGIN, END
    if (xreg_vga_mode(4, 0, COSMICARC_CONFIG, MOTHERSHIP_PART_COUNT, 1, 0, 0) < 0) {
        puts("xreg_vga_mode failed");
        return;
    }

    STAR_MAP_CONFIG = COSMICARC_CONFIG + (MOTHERSHIP_PART_COUNT * sizeof(vga_mode4_sprite_t));

    xram0_struct_set(STAR_MAP_CONFIG, vga_mode2_config_t, x_wrap, false);
    xram0_struct_set(STAR_MAP_CONFIG, vga_mode2_config_t, y_wrap, false);
    xram0_struct_set(STAR_MAP_CONFIG, vga_mode2_config_t, x_pos_px, 0);
    xram0_struct_set(STAR_MAP_CONFIG, vga_mode2_config_t, y_pos_px, 0);
    xram0_struct_set(STAR_MAP_CONFIG, vga_mode2_config_t, width_tiles,  STAR_MAP_WIDTH_TILES);
    xram0_struct_set(STAR_MAP_CONFIG, vga_mode2_config_t, height_tiles, STAR_MAP_HEIGHT_TILES);
    xram0_struct_set(STAR_MAP_CONFIG, vga_mode2_config_t, xram_data_ptr,    STAR_MAP_TILEMAP_DATA); // tile ID grid
    xram0_struct_set(STAR_MAP_CONFIG, vga_mode2_config_t, xram_palette_ptr, PALETTE_ADDR);
    xram0_struct_set(STAR_MAP_CONFIG, vga_mode2_config_t, xram_tile_ptr,    STAR_MAP_DATA);        // tile bitmaps

    // Mode 2 args: MODE, OPTIONS, CONFIG, PLANE, BEGIN, END
    // OPTIONS: bit3=1 (16x16 tiles), bit[2:0]=2 (4-bit color index) => 0b1010 = 10
    // Plane 0 = background fill layer (behind sprite plane 1)
    if (xreg_vga_mode(2, 10, STAR_MAP_CONFIG, 0, 0, 0) < 0) {
        puts("xreg_vga_mode failed");
        return;
    }

    printf("COSMICARC DATA: 0x%04X - 0x%04X\n", COSMICARC_DATA, COSMICARC_DATA + COSMICARC_DATA_SIZE);
    printf("STAR_MAP DATA: 0x%04X - 0x%04X\n", STAR_MAP_DATA, STAR_MAP_DATA + STAR_MAP_DATA_SIZE);
    printf("STAR_MAP TILEMAP: 0x%04X - 0x%04X\n", STAR_MAP_TILEMAP_DATA, STAR_MAP_TILEMAP_DATA + STAR_MAP_TILEMAP_SIZE);
    printf("COSMICARC_CONFIG: 0x%04X\n", COSMICARC_CONFIG);
    printf("STAR_MAP_CONFIG: 0x%04X\n", STAR_MAP_CONFIG);

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
