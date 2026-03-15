#include <rp6502.h>
#include <stdio.h>
#include <stdbool.h>
#include "constants.h"
#include "starfield.h"
#include "palette.h"

unsigned MAIN_MAP_CONFIG;
unsigned MOTHERSHIP_CONFIG;

static void init_graphics(void)
{
    // Select a 320x240 canvas
    if (xreg_vga_canvas(1) < 0) {
        puts("xreg_vga_canvas failed");
        return;
    }


    RIA.addr0 = PALETTE_ADDR;
    RIA.step0 = 1;
    for (int i = 0; i < 256; i++) {
        RIA.rw0 = tile_palette[i] & 0xFF;
        RIA.rw0 = tile_palette[i] >> 8;
    }

    // Start with one lit star color and rotate it over time.
    starfield_init();

    MAIN_MAP_CONFIG = SPRITE_DATA_END;

    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, x_wrap, false);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, y_wrap, false);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, x_pos_px, 0);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, y_pos_px, 0);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, width_tiles,  MAIN_MAP_WIDTH_TILES);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, height_tiles, MAIN_MAP_HEIGHT_TILES);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, xram_data_ptr,    MAIN_MAP_TILEMAP_DATA); // tile ID grid
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, xram_palette_ptr, PALETTE_ADDR);
    xram0_struct_set(MAIN_MAP_CONFIG, vga_mode2_config_t, xram_tile_ptr,    MAIN_MAP_DATA);        // tile bitmaps

    // Mode 2 args: MODE, OPTIONS, CONFIG, PLANE, BEGIN, END
    // OPTIONS: bit3=0 (8x8 tiles), bit[2:0]=3 (8-bit color index) => 0b0011 = 3
    // Plane 0 = background fill layer (behind sprite plane 1)
    if (xreg_vga_mode(2, 0x03, MAIN_MAP_CONFIG, 0, 0, 0) < 0) {
        puts("xreg_vga_mode failed");
        return;
    }

    MOTHERSHIP_CONFIG = MAIN_MAP_CONFIG + sizeof(vga_mode2_config_t);

    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, x_wrap, false);
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, y_wrap, false);
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, x_pos_px, 0);
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, y_pos_px, 0);
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, width_tiles,  MAIN_MAP_WIDTH_TILES);
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, height_tiles, MAIN_MAP_HEIGHT_TILES);
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, xram_data_ptr,    MOTHERSHIP_MAP_TILEMAP_DATA); // tile ID grid
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, xram_palette_ptr, PALETTE_ADDR);
    xram0_struct_set(MOTHERSHIP_CONFIG, vga_mode2_config_t, xram_tile_ptr,    MAIN_MAP_DATA);

    if (xreg_vga_mode(2, 0x03, MOTHERSHIP_CONFIG, 2, 0, 0) < 0) {
        puts("xreg_vga_mode failed");
        return;
    }


    printf("MAIN_MAP DATA: 0x%04X - 0x%04X\n", MAIN_MAP_DATA, MAIN_MAP_DATA + MAIN_MAP_DATA_SIZE);
    printf("MAIN_MAP TILEMAP: 0x%04X - 0x%04X\n", MAIN_MAP_TILEMAP_DATA, MAIN_MAP_TILEMAP_DATA + MAIN_MAP_TILEMAP_SIZE);
    printf("MAIN_MAP_CONFIG: 0x%04X\n", MAIN_MAP_CONFIG);
    printf("MOTHERSHIP_CONFIG: 0x%04X\n", MOTHERSHIP_CONFIG);

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

        starfield_update();

        }
    return 0;
}